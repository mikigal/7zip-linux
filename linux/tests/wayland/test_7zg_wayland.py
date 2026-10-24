#!/usr/bin/env python3
"""7zG as a Wayland client in the nested headless KWin: its dialogs are
activated, dialogs opened from a dialog are transient for it, keyboard
input from libei reaches the focused control, and Spectacle screenshots
show the windows with their decorations.

7zG runs as z7-gui-driver (see tests/gui/gui_driver.cpp): its script waits
at a dialog while the test looks at KWin, and the test types into it.
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))
import rcgen  # noqa: E402
import wlsession  # noqa: E402

DRIVER = None
ORACLE = None
EI_INPUT = None
ARTIFACTS = None
IDS = {}
WIDTH, HEIGHT = 1280, 800

# the windows of the session: caption, state and the window each one is transient for
WINDOWS = """workspace.windowList().filter(w => w.normalWindow || w.dialog).map(w => ({
  caption: w.caption, active: w.active, transient: w.transient, modal: w.modal,
  transientFor: w.transientFor ? w.transientFor.caption : null,
  resourceClass: w.resourceClass, decorated: !w.noBorder, desktopFileName: w.desktopFileName,
  width: w.frameGeometry.width, height: w.frameGeometry.height }))"""


def ID(name):
  return IDS[name]


def make_tree(root):
  os.makedirs(os.path.join(root, "dir"))
  for rel, data in {"small.txt": b"hello 7-Zip\n", "dir/data.bin": os.urandom(100000)}.items():
    with open(os.path.join(root, rel), "wb") as f:
      f.write(data)


def snapshot(root):
  result = {}
  for dirpath, dirnames, filenames in os.walk(root):
    for name in dirnames + filenames:
      path = os.path.join(dirpath, name)
      rel = os.path.relpath(path, root)
      if os.path.isdir(path):
        result[rel] = "dir"
      else:
        with open(path, "rb") as f:
          result[rel] = hashlib.sha256(f.read()).hexdigest()
  return result


def events(log):
  try:
    with open(log) as f:
      return [json.loads(line) for line in f if line.strip()]
  except (FileNotFoundError, json.JSONDecodeError):
    return []


class Wayland7zG(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-7zg-wl-")
    self.src = os.path.join(self.tmp, "src")
    make_tree(self.src)
    self.artifacts = os.path.join(ARTIFACTS, self.id().split(".")[-1])
    os.makedirs(self.artifacts, exist_ok=True)

  def start_7zg(self, session, script, *args, cwd=None):
    path = os.path.join(self.tmp, "script.json")
    with open(path, "w") as f:
      json.dump(script, f)
    self.log = os.path.join(self.artifacts, "7zg-events.jsonl")
    return session.spawn([DRIVER, "--script", path, "--log", self.log, "--timeout", "60",
                          "--", *args], log="7zg", cwd=cwd or self.tmp)

  def wait_dialog(self, name):
    wlsession.wait_until(lambda: any(e["event"] == "dialog" and e.get("name") == name
                                     for e in events(self.log)), 30, "dialog " + name)

  def window(self, windows, caption):
    for w in windows:
      if w["caption"] == caption:
        return w
    raise AssertionError("no window %r in %r" % (caption, windows))

  def wait_windows(self, session, *captions):
    """The windows of KWin once it shows the given ones (Wayland maps a
    surface after the dialog is shown) and one of them is active."""
    result = []

    def ready():
      windows = session.kwin_query(WINDOWS)
      have = {w["caption"] for w in windows}
      if all(c in have for c in captions) and any(w["active"] for w in windows):
        result[:] = windows
        return True
      return False
    wlsession.wait_until(ready, 20, "windows " + ", ".join(captions), interval=0.2)
    return result

  def finish(self, proc):
    code = proc.wait(60)
    errors = [e["message"] for e in events(self.log) if e["event"] == "error"]
    self.assertEqual(errors, [])
    return code

  @unittest.skipUnless(EI_INPUT or "--ei-input" in sys.argv, "needs ei-input")
  def test_password_typed(self):
    archive = os.path.join(self.tmp, "enc.7z")
    subprocess.run([ORACLE, "a", "-bso0", "-psecret", "-mhe=on", archive, "."],
                   cwd=self.src, check=True)
    out = os.path.join(self.tmp, "out")
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      # the script only waits: the password comes from the keyboard
      p = self.start_7zg(s, [
          {"on": "dialog", "name": "IDD_PASSWORD", "actions": [
              {"wait_file": os.path.join(self.tmp, "never")}]},
      ], "x", archive, "-o" + out)
      self.wait_dialog("IDD_PASSWORD")
      self.assertEqual(events(self.log)[0], {"event": "start", "platform": "wayland"})
      windows = self.wait_windows(s, "Enter password")
      password = self.window(windows, "Enter password")
      self.assertTrue(password["active"], windows)
      self.assertTrue(password["modal"], windows)
      self.assertTrue(password["transient"], windows)
      self.assertRegex(password["transientFor"] or "", "Extracting", windows)
      self.assertTrue(password["decorated"], windows)
      self.assertEqual(password["desktopFileName"], "io.github.mikigal.SevenZip.Gui", windows)
      s.screenshot(os.path.join(self.artifacts, "password.png"))
      s.run([EI_INPUT, "type", "secret", "key", "enter"], timeout=30)
      self.assertEqual(self.finish(p), 0)
    self.assertEqual(snapshot(out), snapshot(self.src))
    closed = [e for e in events(self.log) if e["event"] == "dialog_closed" and e["name"] == "IDD_PASSWORD"]
    texts = [c["text"] for c in closed[0]["controls"] if c["id"] == ID("IDE_PASSWORD_PASSWORD")]
    self.assertEqual(texts, ["secret"])

  def test_taskbar_progress(self):
    # the progress of an operation goes to the task manager of the desktop
    with open(os.path.join(self.src, "big.bin"), "wb") as f:
      f.write(os.urandom(6 << 20))
    archive = os.path.join(self.tmp, "big.7z")
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      monitor_log = os.path.join(s.artifacts, "launcher.log")
      s.spawn(["dbus-monitor", "--session",
               "type='signal',interface='com.canonical.Unity.LauncherEntry'"], log="launcher")
      time.sleep(0.5)
      p = self.start_7zg(s, [], "a", "-mx9", "-mmt1", archive, "big.bin", cwd=self.src)
      self.assertEqual(self.finish(p), 0)
      time.sleep(0.5)
    with open(monitor_log) as f:
      text = f.read()
    self.assertIn('string "application://io.github.mikigal.SevenZip.Gui.desktop"', text)
    self.assertIn('string "progress"', text)
    self.assertRegex(text, r'string "progress-visible"\s+variant\s+boolean true')
    # hidden at the end
    self.assertRegex(text, r'(?s)boolean true.*string "progress-visible"\s+variant\s+boolean false')

  def test_options_dialog_parent(self):
    archive = os.path.join(self.tmp, "out.7z")
    go = os.path.join(self.tmp, "go")
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zg(s, [
          {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
              {"click": ID("IDB_COMPRESS_OPTIONS")},
              {"click": ID("IDOK")}]},
          {"on": "dialog", "name": "IDD_COMPRESS_OPTIONS", "actions": [
              {"wait_file": go},
              {"click": ID("IDOK")}]},
      ], "a", "-ad", archive, "small.txt", "dir", cwd=self.src)
      self.wait_dialog("IDD_COMPRESS_OPTIONS")
      windows = self.wait_windows(s, "Options", "Add to Archive")
      options = self.window(windows, "Options")
      compress = self.window(windows, "Add to Archive")
      self.assertTrue(options["active"], windows)
      self.assertTrue(options["modal"], windows)
      self.assertEqual(options["transientFor"], "Add to Archive", windows)
      self.assertFalse(compress["transient"], windows)
      self.assertTrue(compress["decorated"], windows)
      s.screenshot(os.path.join(self.artifacts, "options.png"))
      with open(go, "w"):
        pass
      self.assertEqual(self.finish(p), 0)
    subprocess.run([ORACLE, "t", archive], check=True, capture_output=True)


def main():
  global DRIVER, ORACLE, EI_INPUT, ARTIFACTS, IDS
  ap = argparse.ArgumentParser()
  ap.add_argument("--driver", required=True)
  ap.add_argument("--oracle", required=True)
  ap.add_argument("--ei-input")
  ap.add_argument("--artifacts", required=True)
  args, rest = ap.parse_known_args()
  DRIVER = os.path.abspath(args.driver)
  ORACLE = os.path.abspath(args.oracle)
  EI_INPUT = os.path.abspath(args.ei_input) if args.ei_input else None
  ARTIFACTS = os.path.abspath(args.artifacts)
  os.makedirs(ARTIFACTS, exist_ok=True)
  IDS = rcgen.parse_file(os.path.join(ROOT, "CPP/7zip/UI/GUI/resource.rc")).symbols
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
