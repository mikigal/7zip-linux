#!/usr/bin/env python3
"""Drag and drop between 7zFM and other Wayland clients in the nested
headless KWin, with a real pointer from libei:

- files dragged from another program (z7-probe) onto a folder panel go to
  a new archive (the Add to Archive dialog, as from Explorer on Windows);
- items of a folder panel dragged to another program arrive as
  text/uri-list;
- archive items dragged to Dolphin are extracted by 7zFM into Dolphin's
  folder: Dolphin names it through the Ark protocol (org.kde.ark.DndExtract);
- archive items dragged to another program (z7-probe, which reads the data
  at the drop; Thunar, which reads it while the pointer moves over it and
  copies the files after the drag; Firefox and Brave, with a page that
  reads the dropped file) arrive as files that 7zFM extracts into its
  temporary folder at their first request, and that stay until it exits;
- files copied with Ctrl+C go to the Wayland clipboard, where wl-paste
  reads them, and files that wl-copy puts there are pasted into an archive;
- Open With of the System menu asks the desktop portal (a stand-in that
  records the call) which application opens the file.

7zFM runs as z7-fm-driver (tests/gui/fm_driver.cpp); its script logs where
the items are and waits while the test drags.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))
import wlsession  # noqa: E402

DRIVER = None
PROBE = None
EI_INPUT = None
ORACLE = None
ARTIFACTS = None
WIDTH, HEIGHT = 1280, 800

CLIENT = """workspace.windowList().filter(w => w.caption === %s).map(w => ({
  x: w.clientGeometry.x, y: w.clientGeometry.y,
  width: w.clientGeometry.width, height: w.clientGeometry.height }))"""


def events(log):
  try:
    with open(log) as f:
      return [json.loads(line) for line in f if line.strip()]
  except (FileNotFoundError, json.JSONDecodeError):
    return []


def read_lines(path):
  try:
    with open(path) as f:
      return [line.rstrip("\n") for line in f]
  except FileNotFoundError:
    return []


def js_string(s):
  return json.dumps(s)


class WaylandDnD(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-dnd-wl-")
    self.data = os.path.join(self.tmp, "data")
    os.makedirs(os.path.join(self.data, "sub"))
    with open(os.path.join(self.data, "b.txt"), "w") as f:
      f.write("b\n")
    self.artifacts = os.path.join(ARTIFACTS, self.id().split(".")[-1])
    shutil.rmtree(self.artifacts, ignore_errors=True)
    os.makedirs(self.artifacts)
    self.log = os.path.join(self.artifacts, "7zfm-events.jsonl")
    # the temporary folder of 7zFM
    self.tmpdir = os.path.join(self.tmp, "tmp")
    os.makedirs(self.tmpdir)
    self.go = os.path.join(self.tmp, "go")
    self.placed = os.path.join(self.tmp, "placed")

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def start_7zfm(self, session, script, *args):
    path = os.path.join(self.tmp, "script.json")
    with open(path, "w") as f:
      json.dump(script, f)
    shutil.copy(path, os.path.join(self.artifacts, "script.json"))
    return session.spawn([DRIVER, "--script", path, "--log", self.log, "--timeout", "90",
                          "--", *args], log="7zfm", cwd=self.tmp,
                         env={"TMPDIR": self.tmpdir, "QT_LOGGING_RULES": "z7.dnd.debug=true"})

  def wait_event(self, kind, timeout=20):
    found = []

    def ready():
      found[:] = [e for e in events(self.log) if e["event"] == kind]
      return bool(found)
    wlsession.wait_until(ready, timeout, "event %s of 7zFM" % kind, interval=0.2)
    return found[-1]

  def client(self, session, caption):
    windows = []

    def ready():
      windows[:] = session.kwin_query(CLIENT % js_string(caption))
      return bool(windows)
    wlsession.wait_until(ready, 20, "the window " + caption, interval=0.2)
    return windows[0]

  def screen_point(self, session, caption):
    """Where the point of the "point" event is on the screen; the script
    logs it once the test has placed the window (the "placed" file)."""
    with open(self.placed, "w"):
      pass
    point = self.wait_event("point")
    c = self.client(session, caption)
    return int(c["x"] + point["x"]), int(c["y"] + point["y"])

  def drag(self, session, start, end):
    session.run([EI_INPUT, "drag", str(start[0]), str(start[1]), str(end[0]), str(end[1]), "30"],
                timeout=30)

  def finish(self, process, session=None, caption=None):
    # the drop may activate the target: the driver works in the active window
    if session and caption:
      session.kwin_script("for (const w of workspace.windowList()) if (w.caption === %s) "
                          "workspace.activeWindow = w;" % js_string(caption))
    with open(self.go, "w"):
      pass
    code = process.wait(60)
    errors = [e["message"] for e in events(self.log) if e["event"] == "error"]
    self.assertEqual(errors, [])
    self.assertEqual(code, 0)

  def place_7zfm(self, session, caption):
    session.place_window(caption, 0, 0, WIDTH // 2, HEIGHT)
    time.sleep(0.5)

  def test_drop_from_other_program_makes_archive(self):
    outside = os.path.join(self.tmp, "outside.txt")
    with open(outside, "w") as f:
      f.write("outside\n")
    data = self.data + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(data) + "$"}}]},
          {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
              {"expect": [100, r"^outside\.7z$"]},
              {"click": 1}]},
          {"on": "main", "actions": [
              {"wait_file": os.path.join(self.data, "outside.7z")},
              {"wait_file": self.go}]},
      ], self.data)
      probe_log = os.path.join(s.artifacts, "probe.log")
      s.spawn([PROBE, "--title", "z7-src", "--size", "600x700", "--color", "#aa3333",
               "--drag-file", outside], log="probe")
      wlsession.wait_until(lambda: "ready" in read_lines(probe_log), 20, "the probe window")
      self.client(s, data)
      self.place_7zfm(s, data)
      s.place_window("z7-src", WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      time.sleep(0.5)
      # from the probe to the free space of the panel
      self.drag(s, (WIDTH * 3 // 4, HEIGHT // 2), (WIDTH // 4, HEIGHT * 3 // 4))
      wlsession.wait_until(lambda: os.path.exists(os.path.join(self.data, "outside.7z")), 30,
                           "the new archive")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      self.finish(p)
    listing = subprocess.run([ORACLE, "l", "-slt", os.path.join(self.data, "outside.7z")],
                             capture_output=True, text=True).stdout
    self.assertIn("Path = outside.txt", listing)

  def test_drag_files_to_other_program(self):
    data = self.data + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(data) + "$"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "b.txt"}},
              {"wait_file": self.go}]},
      ], self.data)
      probe_log = os.path.join(s.artifacts, "probe.log")
      s.spawn([PROBE, "--title", "z7-dst", "--size", "600x700", "--color", "#33aa33"], log="probe")
      wlsession.wait_until(lambda: "ready" in read_lines(probe_log), 20, "the probe window")
      self.client(s, data)
      # 7zFM last: the driver works in the active window
      s.place_window("z7-dst", WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      self.place_7zfm(s, data)
      start = self.screen_point(s, data)
      self.drag(s, start, (WIDTH * 3 // 4, HEIGHT // 2))
      url = "drop-url file://" + os.path.join(self.data, "b.txt")
      wlsession.wait_until(lambda: url in read_lines(probe_log), 15, "the drop in the probe")
      drops = [l for l in read_lines(probe_log) if l.startswith("drop ")]
      self.assertTrue(drops and "text/uri-list" in drops[0], read_lines(probe_log))
      self.finish(p, s, data)

  @unittest.skipUnless(shutil.which("dolphin"), "needs Dolphin")
  def test_drag_archive_items_to_dolphin(self):
    src = os.path.join(self.tmp, "src")
    os.makedirs(os.path.join(src, "inner"))
    with open(os.path.join(src, "inner", "x.txt"), "w") as f:
      f.write("x\n")
    with open(os.path.join(src, "top.txt"), "w") as f:
      f.write("top\n")
    archive = os.path.join(self.data, "arc.7z")
    subprocess.run([ORACLE, "a", "-bso0", archive, "inner", "top.txt"], cwd=src, check=True)
    target = os.path.join(self.tmp, "target")
    os.makedirs(target)
    arc_title = archive + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(arc_title) + "$"}},
              {"click": {"panel": 0, "item": "top.txt"}},
              {"click": {"panel": 0, "item": "inner", "mods": "ctrl"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.go}]},
      ], archive)
      s.spawn(["dolphin", "--new-window", target], log="dolphin")
      dolphin = []

      def dolphin_window():
        dolphin[:] = s.kwin_query("""workspace.windowList().filter(w => w.normalWindow &&
            w.resourceClass.indexOf('dolphin') >= 0).map(w => w.caption)""")
        return bool(dolphin)
      wlsession.wait_until(dolphin_window, 30, "the Dolphin window", interval=0.5)
      self.client(s, arc_title)
      s.place_window(dolphin[0], WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      time.sleep(1.0)
      self.place_7zfm(s, arc_title)
      s.screenshot(os.path.join(self.artifacts, "before.png"))
      start = self.screen_point(s, arc_title)
      self.drag(s, start, (WIDTH * 3 // 4, HEIGHT // 2))
      wlsession.wait_until(lambda: os.path.exists(os.path.join(target, "inner", "x.txt")), 30,
                           "the items extracted into Dolphin's folder")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      # the Ark protocol extracts straight into the folder: no copies in the
      # temporary folder are left for a target
      self.assertEqual(os.listdir(self.tmpdir), [])
      self.finish(p, s, arc_title)
    with open(os.path.join(target, "top.txt")) as f:
      self.assertEqual(f.read(), "top\n")

  def test_drag_archive_items_to_other_program(self):
    archive = os.path.join(self.data, "arc.7z")
    with open(os.path.join(self.tmp, "top.txt"), "w") as f:
      f.write("top\n")
    subprocess.run([ORACLE, "a", "-bso0", archive, "top.txt"], cwd=self.tmp, check=True)
    arc_title = archive + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(arc_title) + "$"}},
              {"click": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.go}]},
      ], archive)
      probe_log = os.path.join(s.artifacts, "probe.log")
      s.spawn([PROBE, "--title", "z7-dst", "--size", "600x700", "--color", "#33aa33"], log="probe")
      wlsession.wait_until(lambda: "ready" in read_lines(probe_log), 20, "the probe window")
      self.client(s, arc_title)
      s.place_window("z7-dst", WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      self.place_7zfm(s, arc_title)
      start = self.screen_point(s, arc_title)
      self.drag(s, start, (WIDTH * 3 // 4, HEIGHT // 2))
      urls = []

      def dropped():
        urls[:] = [l[len("drop-url "):] for l in read_lines(probe_log) if l.startswith("drop-url ")]
        return bool(urls)
      wlsession.wait_until(dropped, 30, "the archive item in the probe")
      self.assertEqual(len(urls), 1, read_lines(probe_log))
      self.assertTrue(urls[0].startswith("file://" + self.tmpdir + "/"), urls)
      extracted = urls[0][len("file://"):]
      self.assertEqual(os.path.basename(extracted), "top.txt")
      # the drag has ended; the target copies the file later
      time.sleep(1.0)
      with open(extracted) as f:
        self.assertEqual(f.read(), "top\n")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      self.finish(p, s, arc_title)
    self.assertFalse(os.path.exists(extracted))

  def test_drag_archive_items_twice(self):
    # The drag passes over a Plasma shell (a probe with its name, which reads
    # every type of the drag as the drop areas of the shell do, the types of
    # the Ark protocol among them) to a program that takes the files. 7zFM
    # waits for an Ark call after such a drop; the next drag, right after,
    # ends the wait and starts.
    archive = os.path.join(self.data, "arc.7z")
    for name in ("one.txt", "two.txt"):
      with open(os.path.join(self.tmp, name), "w") as f:
        f.write(name + "\n")
    subprocess.run([ORACLE, "a", "-bso0", archive, "one.txt", "two.txt"], cwd=self.tmp, check=True)
    shell = os.path.join(self.tmp, "plasmashell")
    shutil.copy(PROBE, shell)
    arc_title = archive + "/"
    placed2 = os.path.join(self.tmp, "placed2")
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(arc_title) + "$"}},
              {"click": {"panel": 0, "item": "one.txt"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "one.txt"}},
              {"wait_file": placed2},
              {"log": {"panel": 0, "item": "two.txt"}},
              {"wait_file": self.go}]},
      ], archive)
      shell_log = os.path.join(s.artifacts, "shell.log")
      probe_log = os.path.join(s.artifacts, "probe.log")
      s.spawn([shell, "--title", "z7-shell", "--size", "300x700", "--color", "#aa33aa", "--read-on-enter"],
              log="shell")
      s.spawn([PROBE, "--title", "z7-dst", "--size", "340x700", "--color", "#33aa33"], log="probe")
      wlsession.wait_until(lambda: "ready" in read_lines(probe_log) and "ready" in read_lines(shell_log), 20,
                           "the probe windows")
      self.client(s, arc_title)
      s.place_window("z7-shell", 640, 0, 300, HEIGHT)
      s.place_window("z7-dst", 940, 0, 340, HEIGHT)
      self.place_7zfm(s, arc_title)
      start = self.screen_point(s, arc_title)
      self.drag(s, start, (1110, HEIGHT // 2))

      def urls():
        return [l[len("drop-url "):] for l in read_lines(probe_log) if l.startswith("drop-url ")]
      wlsession.wait_until(lambda: len(urls()) == 1, 30, "the first item in the probe")
      self.assertIn("enter-read application/x-kde-ark-dndextract-service", read_lines(shell_log))
      # the second item, right away
      s.kwin_script("for (const w of workspace.windowList()) if (w.caption === %s) "
                    "workspace.activeWindow = w;" % js_string(arc_title))
      with open(placed2, "w"):
        pass
      wlsession.wait_until(lambda: len([e for e in events(self.log) if e["event"] == "point"]) == 2, 20,
                           "the point of the second item")
      second = [e for e in events(self.log) if e["event"] == "point"][-1]
      c = self.client(s, arc_title)
      self.drag(s, (int(c["x"] + second["x"]), int(c["y"] + second["y"])), (1110, HEIGHT // 2))
      wlsession.wait_until(lambda: len(urls()) == 2, 30, "the second item in the probe")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      self.finish(p, s, arc_title)
    self.assertEqual([os.path.basename(u) for u in urls()], ["one.txt", "two.txt"])

  def drag_archive_item_to_browser(self, argv, env=None):
    """Drags top.txt of an archive to a page that shows the name and the
    contents of the dropped file in its title."""
    archive = os.path.join(self.data, "arc.7z")
    with open(os.path.join(self.tmp, "top.txt"), "w") as f:
      f.write("top\n")
    subprocess.run([ORACLE, "a", "-bso0", archive, "top.txt"], cwd=self.tmp, check=True)
    page = os.path.join(self.tmp, "drop.html")
    with open(page, "w") as f:
      f.write("<!doctype html><title>drop here</title>"
              "<body style='margin:0;height:100vh' ondragover='event.preventDefault()' "
              "ondrop='event.preventDefault(); const f = event.dataTransfer.files[0]; "
              "f.text().then(t => document.title = \"dropped \" + f.name + \" \" + t.trim())'>")
    profile = os.path.join(self.tmp, "profile")
    os.makedirs(profile)
    arc_title = archive + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(arc_title) + "$"}},
              {"click": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.go}]},
      ], archive)
      s.spawn([a.replace("PROFILE", profile).replace("PAGE", "file://" + page) for a in argv],
              log="browser", env=env)
      browser = []

      def page_window(title):
        browser[:] = s.kwin_query("""workspace.windowList().filter(w => w.normalWindow &&
            w.caption.indexOf(%s) >= 0).map(w => w.caption)""" % js_string(title))
        return bool(browser)
      wlsession.wait_until(lambda: page_window("drop here"), 60, "the page in the browser", interval=0.5)
      self.client(s, arc_title)
      s.place_window(browser[0], WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      time.sleep(1.0)
      self.place_7zfm(s, arc_title)
      start = self.screen_point(s, arc_title)
      self.drag(s, start, (WIDTH * 3 // 4, HEIGHT // 2))
      wlsession.wait_until(lambda: page_window("dropped top.txt top"), 30, "the dropped file in the page",
                           interval=0.5)
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      self.finish(p, s, arc_title)

  @unittest.skipUnless(shutil.which("firefox"), "needs Firefox")
  def test_drag_archive_items_to_firefox(self):
    self.drag_archive_item_to_browser(["firefox", "--no-remote", "--new-instance", "--profile", "PROFILE", "PAGE"],
                                      env={"MOZ_ENABLE_WAYLAND": "1"})

  @unittest.skipUnless(shutil.which("brave"), "needs Brave")
  def test_drag_archive_items_to_brave(self):
    self.drag_archive_item_to_browser(["brave", "--user-data-dir=PROFILE", "--ozone-platform=wayland",
                                       "--no-first-run", "--no-default-browser-check",
                                       "--password-store=basic", "PAGE"])

  @unittest.skipUnless(shutil.which("thunar"), "needs Thunar")
  def test_drag_archive_items_to_thunar(self):
    # a GTK file manager reads the file list while the pointer moves over
    # it, and copies the files of the drop after the drag
    archive = os.path.join(self.data, "arc.7z")
    with open(os.path.join(self.tmp, "top.txt"), "w") as f:
      f.write("top\n")
    subprocess.run([ORACLE, "a", "-bso0", archive, "top.txt"], cwd=self.tmp, check=True)
    target = os.path.join(self.tmp, "target")
    os.makedirs(target)
    arc_title = archive + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(arc_title) + "$"}},
              {"click": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "top.txt"}},
              {"wait_file": self.go}]},
      ], archive)
      s.spawn(["thunar", target], log="thunar")
      thunar = []

      def thunar_window():
        thunar[:] = s.kwin_query("""workspace.windowList().filter(w => w.normalWindow &&
            w.caption.indexOf('Thunar') >= 0).map(w => w.caption)""")
        return bool(thunar)
      wlsession.wait_until(thunar_window, 30, "the Thunar window", interval=0.5)
      self.client(s, arc_title)
      s.place_window(thunar[0], WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      time.sleep(1.0)
      self.place_7zfm(s, arc_title)
      start = self.screen_point(s, arc_title)
      self.drag(s, start, (WIDTH * 3 // 4, HEIGHT // 2))
      time.sleep(1.0)
      s.screenshot(os.path.join(self.artifacts, "dropped.png"))
      wlsession.wait_until(lambda: os.path.exists(os.path.join(target, "top.txt")), 30,
                           "the archive item in Thunar's folder")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      self.finish(p, s, arc_title)
    with open(os.path.join(target, "top.txt")) as f:
      self.assertEqual(f.read(), "top\n")


  # ---------- the clipboard ----------

  @unittest.skipUnless(shutil.which("wl-copy") and shutil.which("wl-paste"), "needs wl-clipboard")
  def test_clipboard_with_other_programs(self):
    archive = os.path.join(self.data, "arc.7z")
    subprocess.run([ORACLE, "a", "-bso0", archive, "b.txt"], cwd=self.data, check=True)
    outside = os.path.join(self.tmp, "outside.txt")
    with open(outside, "w") as f:
      f.write("outside\n")
    data = self.data + "/"
    arc_title = archive + "/"
    copied = os.path.join(self.tmp, "copied")
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(data) + "$"}},
              {"click": {"panel": 0, "item": "b.txt"}},
              {"wait_file": self.placed},
              {"log": {"panel": 0, "item": "b.txt"}},
              {"wait_file": copied},
              {"click": {"panel": 0, "item": "arc.7z"}},
              {"key": "Return"},
              {"wait": {"path": "^" + re.escape(arc_title) + "$"}},
              {"log": "clipboard"},
              {"wait_file": self.go}]},
      ], self.data)
      self.client(s, data)
      self.place_7zfm(s, data)
      # b.txt is selected and the window placed and active
      with open(self.placed, "w"):
        pass
      self.wait_event("point")
      # Ctrl+C on the keyboard: the files go to the Wayland clipboard,
      # where another program (wl-paste) reads them
      s.run([EI_INPUT, "key", "ctrl+c"])
      url = "file://" + os.path.join(self.data, "b.txt")

      def pasted(kind, expected):
        res = s.run(["wl-paste", "--no-newline", "--type", kind], check=False, timeout=10)
        return res.returncode == 0 and res.stdout.strip() == expected
      wlsession.wait_until(lambda: pasted("text/uri-list", url), 15, "the copied file in wl-paste")
      self.assertTrue(pasted("x-special/gnome-copied-files", "copy\n" + url))
      # files that another program copies (wl-copy) are pasted into the archive
      s.spawn(["wl-copy", "--type", "text/uri-list", "file://" + outside], log="wl-copy")
      wlsession.wait_until(lambda: pasted("text/uri-list", "file://" + outside), 15, "the files of wl-copy")
      with open(copied, "w"):
        pass
      self.wait_event("clipboard")
      s.run([EI_INPUT, "key", "ctrl+v"])

      def added():
        res = subprocess.run([ORACLE, "l", "-slt", archive], capture_output=True, text=True)
        return "Path = outside.txt" in res.stdout
      wlsession.wait_until(added, 30, "the pasted file in the archive")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
      self.finish(p, s, arc_title)
    self.assertTrue(os.path.exists(outside))

  # ---------- the shell commands that go to the desktop ----------

  def test_open_with_asks_through_portal(self):
    record = os.path.join(self.tmp, "portal.jsonl")
    data = self.data + "/"
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      portal_log = os.path.join(s.artifacts, "portal.log")
      s.spawn([sys.executable, "-I", os.path.join(HERE, "fake_openuri_portal.py"), record],
              log="portal")
      wlsession.wait_until(lambda: "ready" in read_lines(portal_log), 20, "the portal stand-in")
      config = os.path.join(s.env["XDG_CONFIG_HOME"], "7-Zip")
      os.makedirs(config, exist_ok=True)
      with open(os.path.join(config, "7-Zip.ini"), "w") as f:
        f.write('[Software/7-Zip/FM]\n"ShowSystemMenu"=dword:00000001\n')
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(data) + "$"}},
              {"click": {"panel": 0, "item": "b.txt"}},
              {"choose": [0, "System", "Open With..."]},
              {"wait_file": record}]},
      ], self.data)
      self.assertEqual(p.wait(60), 0)
    errors = [e["message"] for e in events(self.log) if e["event"] == "error"]
    self.assertEqual(errors, [])
    with open(record) as f:
      calls = [json.loads(line) for line in f]
    self.assertEqual(calls, [{"method": "OpenFile", "path": os.path.join(self.data, "b.txt"),
                              "options": {"ask": True}}])


def main():
  global DRIVER, PROBE, EI_INPUT, ORACLE, ARTIFACTS
  ap = argparse.ArgumentParser()
  ap.add_argument("--driver", required=True)
  ap.add_argument("--probe", required=True)
  ap.add_argument("--ei-input", required=True)
  ap.add_argument("--oracle", required=True)
  ap.add_argument("--artifacts", required=True)
  args, rest = ap.parse_known_args()
  DRIVER = os.path.abspath(args.driver)
  PROBE = os.path.abspath(args.probe)
  EI_INPUT = os.path.abspath(args.ei_input)
  ORACLE = os.path.abspath(args.oracle)
  ARTIFACTS = os.path.abspath(args.artifacts)
  os.makedirs(ARTIFACTS, exist_ok=True)
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
