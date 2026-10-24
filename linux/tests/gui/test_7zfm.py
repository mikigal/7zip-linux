#!/usr/bin/env python3
"""End-to-end tests of 7zFM: browsing folders and archives in its panels
with the keyboard, the mouse, the menus and the address bar, run by
z7-fm-driver (7zFM with a script) offscreen. Archives are made by the 7zz
oracle.

Command IDs are named as in the upstream resource headers; the names come
from rcgen.py, which reads them from Bundles/Fm/resource.rc.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))
import rcgen  # noqa: E402

DRIVER = None
ORACLE = None
IDYES = 6
IDNO = 7
ARTIFACTS = None
IDS = {}


def ID(name):
  return IDS[name]


def oracle(*args, cwd=None):
  res = subprocess.run([ORACLE, *args], capture_output=True, text=True, cwd=cwd)
  if res.returncode != 0:
    raise AssertionError("7zz %s failed (%d):\n%s\n%s" % (
        " ".join(args), res.returncode, res.stdout[-3000:], res.stderr[-3000:]))
  return res


def main_step(*actions):
  return {"on": "main", "actions": list(actions)}


class Run:
  """One run of z7-fm-driver: exit code and the events of its log."""

  def __init__(self, code, events, stderr):
    self.code = code
    self.events = events
    self.stderr = stderr

  def of(self, kind, name=None):
    return [e for e in self.events
            if e["event"] == kind and (name is None or e.get("name") == name)]

  def errors(self):
    return [e["message"] for e in self.of("error")]

  def panels(self):
    return self.of("panel")


class FmTest(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-7zfm-")
    self.data = os.path.join(self.tmp, "data")
    os.makedirs(os.path.join(self.data, "docs"))
    os.makedirs(os.path.join(self.data, "sub"))
    with open(os.path.join(self.data, "docs", "a.txt"), "w") as f:
      f.write("hello\n")
    with open(os.path.join(self.data, "b.txt"), "w") as f:
      f.write("world, a longer file\n")
    with open(os.path.join(self.data, "c10.bin"), "wb") as f:
      f.write(b"\0" * 1000)
    self.home = os.path.join(self.tmp, "home")
    os.makedirs(self.home)
    self.tmpdir = os.path.join(self.tmp, "tmp")
    os.makedirs(self.tmpdir)
    self.shots = os.path.join(ARTIFACTS, self.id().split(".")[-1])
    shutil.rmtree(self.shots, ignore_errors=True)
    os.makedirs(self.shots)

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def run_fm(self, script, *args, timeout=60):
    script_path = os.path.join(self.tmp, "script.json")
    log_path = os.path.join(self.shots, "log.jsonl")
    with open(script_path, "w") as f:
      json.dump(script, f, indent=1)
    shutil.copy(script_path, os.path.join(self.shots, "script.json"))
    env = {
        "PATH": "/usr/bin:/bin",
        "HOME": self.home,
        "XDG_CONFIG_HOME": os.path.join(self.home, "config"),
        "XDG_DATA_HOME": os.path.join(self.home, "data"),
        "XDG_CACHE_HOME": os.path.join(self.home, "cache"),
        "XDG_STATE_HOME": os.path.join(self.home, "state"),
        "XDG_RUNTIME_DIR": self.home,
        # the temporary folders of 7-Zip (7zO*, 7zE*) go with the test
        "TMPDIR": self.tmpdir,
        # "all users" settings (mimeapps.list of the System options page)
        "XDG_CONFIG_DIRS": os.path.join(self.tmp, "etc-xdg"),
        "LANG": "C.UTF-8",
        "QT_QPA_PLATFORM": "offscreen",
        "QT_FORCE_STDERR_LOGGING": "1",
    }
    for name in ("ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"):
      if name in os.environ:
        env[name] = os.environ[name]
    res = subprocess.run(
        [DRIVER, "--script", script_path, "--log", log_path, "--shots", self.shots,
         "--timeout", str(timeout), "--", *args],
        cwd=self.tmp, env=env, capture_output=True, text=True, timeout=timeout + 30)
    with open(os.path.join(self.shots, "stderr.txt"), "w") as f:
      f.write(res.stderr)
    events = []
    if os.path.exists(log_path):
      with open(log_path) as f:
        events = [json.loads(line) for line in f if line.strip()]
    return Run(res.returncode, events, res.stderr)

  def assertRun(self, run):
    self.assertEqual(run.errors(), [], run.stderr)
    self.assertEqual(run.code, 0, run.stderr)

  def path_re(self, *parts):
    return "^" + re.escape(os.path.join(self.data, *parts)) + "/$"

  # ---------- Start and the panel ----------

  def test_start_lists_folder(self):
    run = self.run_fm([
        main_step(
            {"wait": {"panel": 0, "path": self.path_re()}},
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"], "focused": "docs",
                        "selected": [], "focus_in_list": True,
                        "status": [0, r"^0 / 4 "], "panels": 1,
                        "title": "^" + re.escape(self.data) + "/$"}},
            {"screenshot": "main.png"},
            {"log": "panels"}),
    ], self.data)
    self.assertRun(run)
    panel = run.panels()[0]
    self.assertEqual(panel["columns"][:3], ["Name", "Size", "Modified"])
    self.assertFalse(run.panels()[1]["created"])

  def test_navigate_folders_with_keys(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            # the first item has the focus, but no selection: Enter opens
            # only selected items, as on Windows
            {"expect": {"focused": "docs", "selected": []}},
            {"key": "Return"},
            {"expect": {"path": self.path_re()}},
            # as in the Windows control, Space selects the focused item;
            # Home does not change anything on the first item
            {"key": "Home"},
            {"expect": {"focused": "docs", "selected": []}},
            {"key": "Space"},
            {"expect": {"focused": "docs", "selected": ["docs"]}},
            {"key": "Return"},
            {"wait": {"path": self.path_re("docs"), "items": ["a.txt"]}},
            {"key": "Backspace"},
            {"wait": {"path": self.path_re(), "focused": "docs"}},
            {"key": "Down"},
            {"expect": {"focused": "sub"}},
            {"key": "ctrl+PgDown"},
            {"wait": {"path": self.path_re("sub"), "count": 0}},
            {"key": "ctrl+PgUp"},
            {"wait": {"path": self.path_re(), "focused": "sub"}}),
    ], self.data)
    self.assertRun(run)

  def test_mouse_double_click_opens(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"expect": {"focused": "b.txt", "selected": ["b.txt"], "status": [1, "^21$"]}},
            {"click": {"panel": 0, "item": "c10.bin", "mods": "ctrl"}},
            {"expect": {"selected": ["b.txt", "c10.bin"], "status": [0, r"^2 / 4 "]}},
            {"click": {"panel": 0, "item": "docs", "double": True}},
            {"wait": {"path": self.path_re("docs"), "items": ["a.txt"]}}),
    ], self.data)
    self.assertRun(run)

  def test_selection_keys(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "ctrl+A"},
            {"expect": {"selected": ["docs", "sub", "b.txt", "c10.bin"], "status": [0, r"^4 / 4 "]}},
            {"key": "keypad+*"},
            {"expect": {"selected": []}},
            {"key": "keypad++"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"text": [ID("IDC_COMBO"), "*.txt"]},
            {"click": ID("IDOK")}]},
        main_step(
            {"expect": {"selected": ["b.txt"]}},
            {"key": "Home"},
            {"key": "shift+Down"},
            {"key": "shift+Down"},
            {"expect": {"focused": "b.txt", "selected": ["docs", "sub", "b.txt"]}}),
    ], self.data)
    self.assertRun(run)

  def test_type_ahead(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"type": "c1"},
            {"expect": {"focused": "c10.bin"}},
            # a pause starts a new search
            {"sleep": 1200},
            {"type": "s"},
            {"expect": {"focused": "sub"}}),
    ], self.data)
    self.assertRun(run)

  def test_rename_inline(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "End"},
            {"expect": {"focused": "c10.bin"}},
            {"key": "F2"},
            {"type": "renamed.bin"},
            {"key": "Return"},
            {"wait": {"focused": "renamed.bin"}},
            {"expect": {"items": ["docs", "sub", "b.txt", "renamed.bin"]}}),
    ], self.data)
    self.assertRun(run)
    self.assertTrue(os.path.exists(os.path.join(self.data, "renamed.bin")))
    self.assertFalse(os.path.exists(os.path.join(self.data, "c10.bin")))

  def test_address_bar(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"address": os.path.join(self.data, "docs")},
            {"wait": {"path": self.path_re("docs"), "items": ["a.txt"], "focus_in_list": True}}),
    ], self.data)
    self.assertRun(run)

  # ---------- Archives ----------

  def make_archive(self):
    src = os.path.join(self.tmp, "src")
    os.makedirs(os.path.join(src, "inner"))
    with open(os.path.join(src, "inner", "x.txt"), "w") as f:
      f.write("x\n")
    with open(os.path.join(src, "top.txt"), "w") as f:
      f.write("top\n")
    oracle("a", "-bso0", os.path.join(src, "nested.zip"), "top.txt", cwd=src)
    archive = os.path.join(self.data, "arc.7z")
    oracle("a", "-bso0", archive, "inner", "top.txt", "nested.zip", cwd=src)
    return archive

  def test_archive_navigation(self):
    self.make_archive()
    arc = os.path.join(self.data, "arc.7z")
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"key": "Return"},
            {"wait": {"path": "^" + re.escape(arc) + "/$", "items": ["inner", "nested.zip", "top.txt"]}},
            {"screenshot": "archive.png"},
            {"expect": {"focused": "inner", "selected": []}},
            {"key": "Space"},
            {"expect": {"focused": "inner", "selected": ["inner"]}},
            {"key": "Return"},
            {"wait": {"path": "^" + re.escape(arc) + "/inner/$", "items": ["x.txt"]}},
            {"key": "Backspace"},
            {"wait": {"path": "^" + re.escape(arc) + "/$", "focused": "inner"}},
            {"key": "Down"},
            {"key": "ctrl+PgDown"},
            {"wait": {"path": "^" + re.escape(arc) + "/nested.zip/$", "items": ["top.txt"]}},
            {"key": "Backspace"},
            {"wait": {"path": "^" + re.escape(arc) + "/$", "focused": "nested.zip"}},
            {"key": "Backspace"},
            {"wait": {"path": self.path_re(), "focused": "arc.7z"}}),
    ], self.data)
    self.assertRun(run)
    # a quick open shows no progress window (WaitMode waits 500 ms first)
    captions = [d.get("caption", "") for d in run.of("dialog")]
    self.assertEqual([c for c in captions if "Opening" in c], [], captions)

  def test_open_archive_from_command_line(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$", "items": ["inner", "nested.zip", "top.txt"]}}),
    ], archive)
    self.assertRun(run)

  # ---------- Copy and move ----------

  def two_panels(self, right):
    """Steps that show two panels, the right one at (right), with the
    focus in the left one."""
    return [
        {"wait": {"path": self.path_re()}},
        {"key": "F9"},
        {"wait": {"panels": 2}},
        {"address": [1, right]},
        {"wait": {"panel": 1, "path": "^" + re.escape(right) + "/$"}},
        {"focus": 0}]

  def test_copy_and_move_between_panels(self):
    sub = os.path.join(self.data, "sub")
    run = self.run_fm([
        main_step(
            *self.two_panels(sub),
            {"click": {"panel": 0, "item": "b.txt"}},
            {"click": {"panel": 0, "item": "docs", "mods": "ctrl"}},
            {"key": "F5"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [
            {"expect": [ID("IDC_COPY"), "^" + re.escape(sub) + "/$"]},
            {"expect": [ID("IDT_COPY_INFO"), "Files: 1"]},
            {"screenshot": "copy.png"},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait": {"panel": 1, "items": ["docs", "b.txt"]}},
            {"expect": {"panel": 0, "items": ["docs", "sub", "b.txt", "c10.bin"],
                        "selected": [], "focus_in_list": True}},
            {"key": "End"},
            {"expect": {"focused": "c10.bin"}},
            {"key": "F6"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [{"click": ID("IDOK")}]},
        main_step(
            {"wait": {"panel": 1, "items": ["docs", "b.txt", "c10.bin"]}},
            {"wait": {"panel": 0, "items": ["docs", "sub", "b.txt"]}}),
    ], self.data)
    self.assertRun(run)
    with open(os.path.join(sub, "docs", "a.txt")) as f:
      self.assertEqual(f.read(), "hello\n")
    self.assertTrue(os.path.exists(os.path.join(self.data, "b.txt")))
    self.assertFalse(os.path.exists(os.path.join(self.data, "c10.bin")))
    self.assertEqual(os.path.getsize(os.path.join(sub, "c10.bin")), 1000)
    captions = [d["caption"] for d in run.of("dialog", "IDD_COPY")]
    self.assertEqual(captions, ["Copy", "Move"])

  def test_copy_asks_before_replacing(self):
    sub = os.path.join(self.data, "sub")
    for name in ("b.txt", "c10.bin"):
      with open(os.path.join(sub, name), "w") as f:
        f.write("old\n")
    run = self.run_fm([
        main_step(
            *self.two_panels(sub),
            {"click": {"panel": 0, "item": "b.txt"}},
            {"click": {"panel": 0, "item": "c10.bin", "mods": "ctrl"}},
            {"key": "F5"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [{"click": ID("IDOK")}]},
        {"on": "dialog", "name": "IDD_OVERWRITE", "actions": [
            {"screenshot": "overwrite.png"},
            {"click": ID("IDNO")}]},
        {"on": "dialog", "name": "IDD_OVERWRITE", "actions": [{"click": ID("IDYES")}]},
        main_step(
            {"wait": {"panel": 1, "items": ["b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    # the files come in the order of the folder: the first one asked about
    # is kept, the second one is replaced
    asked = []
    for d in run.of("dialog", "IDD_OVERWRITE"):
      text = [c["text"] for c in d["controls"] if c["id"] == ID("IDT_OVERWRITE_OLD_FILE_SIZE_TIME")][0]
      self.assertTrue(text.startswith(sub + "/\n"), text)
      asked.append(text.split("\n")[1])
    self.assertEqual(sorted(asked), ["b.txt", "c10.bin"])
    with open(os.path.join(sub, asked[0])) as f:
      self.assertEqual(f.read(), "old\n")
    self.assertEqual(os.path.getsize(os.path.join(sub, asked[1])),
                     os.path.getsize(os.path.join(self.data, asked[1])))

  def test_copy_onto_itself_is_reported(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "ctrl+A"},
            {"key": "F5"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [
            {"text": [ID("IDC_COPY"), os.path.join(self.data, "docs") + "/"]},
            {"click": ID("IDOK")}]},
        {"on": "dialog", "name": "IDD_PROGRESS", "actions": [
            {"wait": [ID("IDCANCEL"), "^&Close$"]},
            {"screenshot": "error.png"},
            {"click": ID("IDCANCEL")}]},
        main_step({"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    progress = run.of("dialog_closed", "IDD_PROGRESS")[-1]
    rows = [c for c in progress["controls"] if c["class"] == "SysListView32"][0]["rows"]
    self.assertTrue(any("Cannot copy folder onto itself" in r[-1] for r in rows), rows)
    self.assertFalse(os.path.exists(os.path.join(self.data, "docs", "docs")))

  def test_copy_into_and_out_of_archives(self):
    archive = self.make_archive()
    sub = os.path.join(self.data, "sub")
    run = self.run_fm([
        main_step(
            *self.two_panels(archive),
            # into the archive: the agent updates it
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "F5"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [{"click": ID("IDOK")}]},
        main_step(
            {"wait": {"panel": 1, "items": ["inner", "b.txt", "nested.zip", "top.txt"]}},
            # out of the archive into the file system: extraction
            {"focus": 1},
            {"click": {"panel": 1, "item": "inner"}},
            {"click": {"panel": 1, "item": "top.txt", "mods": "ctrl"}},
            {"address": [0, sub]},
            {"wait": {"panel": 0, "path": self.path_re("sub")}},
            {"focus": 1},
            {"key": "F5"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [
            {"expect": [ID("IDC_COPY"), "^" + re.escape(sub) + "/$"]},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait": {"panel": 0, "items": ["inner", "top.txt"]}}),
    ], self.data)
    self.assertRun(run)
    listing = oracle("l", "-slt", archive).stdout
    self.assertIn("Path = b.txt", listing)
    self.assertIn("Path = inner" + os.sep + "x.txt", listing)
    oracle("t", archive)
    with open(os.path.join(sub, "inner", "x.txt")) as f:
      self.assertEqual(f.read(), "x\n")

  def test_copy_between_archives(self):
    archive = self.make_archive()
    other = os.path.join(self.data, "other.zip")
    oracle("a", "-bso0", other, os.path.join(self.data, "c10.bin"))
    run = self.run_fm([
        main_step(
            *self.two_panels(other),
            {"address": [0, archive]},
            {"wait": {"panel": 0, "path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "inner"}},
            {"key": "F5"}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [{"click": ID("IDOK")}]},
        main_step(
            {"wait": {"panel": 1, "items": ["inner", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    listing = oracle("l", "-slt", other).stdout
    self.assertIn("Path = inner" + os.sep + "x.txt", listing)
    oracle("t", other)

  # ---------- Delete ----------

  def trash(self, *parts):
    return os.path.join(self.home, "data", "Trash", *parts)

  def test_delete_to_trash(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"click": {"panel": 0, "item": "docs", "mods": "ctrl"}},
            {"key": "Delete"},
            {"wait": {"items": ["sub", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    self.assertEqual(run.of("messagebox"), [])
    with open(self.trash("files", "b.txt")) as f:
      self.assertEqual(f.read(), "world, a longer file\n")
    self.assertTrue(os.path.exists(self.trash("files", "docs", "a.txt")))
    with open(self.trash("info", "b.txt.trashinfo")) as f:
      self.assertIn("Path=" + os.path.join(self.data, "b.txt"), f.read())

  def test_shift_delete_is_permanent(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "Space"},
            {"key": "shift+Delete"}),
        {"on": "messagebox", "contains": "docs", "answer": IDNO},
        main_step(
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}},
            {"key": "shift+Delete"}),
        {"on": "messagebox", "contains": "docs", "answer": IDYES},
        main_step(
            {"wait": {"items": ["sub", "b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    box = run.of("messagebox")[0]
    self.assertEqual(box["caption"], "Confirm Folder Delete")
    self.assertFalse(os.path.exists(os.path.join(self.data, "docs")))
    self.assertFalse(os.path.exists(self.trash("files", "docs")))

  def test_delete_without_trash_asks(self):
    # a file where the trash folder would be: nothing can go to the trash
    os.makedirs(os.path.join(self.home, "data"))
    with open(self.trash(), "w") as f:
      f.write("not a folder\n")
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "Delete"}),
        {"on": "messagebox", "contains": "Cannot move to the Trash", "answer": IDNO},
        main_step(
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}},
            {"key": "Delete"}),
        {"on": "messagebox", "contains": "Delete permanently?", "answer": IDYES},
        main_step(
            {"wait": {"items": ["docs", "sub", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    box = run.of("messagebox")[0]
    self.assertIn(os.path.join(self.data, "b.txt"), box["text"])
    self.assertEqual(box["caption"], "Delete File")
    self.assertFalse(os.path.exists(os.path.join(self.data, "b.txt")))

  def test_delete_in_archive(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            {"key": "Delete"}),
        {"on": "messagebox", "contains": "top.txt", "answer": IDYES},
        main_step(
            {"wait": {"items": ["inner", "nested.zip"]}}),
    ], archive)
    self.assertRun(run)
    listing = oracle("l", "-slt", archive).stdout
    self.assertNotIn("Path = top.txt", listing)
    self.assertIn("Path = nested.zip", listing)
    oracle("t", archive)

  # ---------- Create, rename, comments ----------

  def test_create_folder_and_file(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "F7"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"expect": [ID("IDC_COMBO"), "^New Folder$"]},
            {"text": [ID("IDC_COMBO"), "made/deeper"]},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait": {"focused": "made", "selected": ["made"]}},
            {"key": "ctrl+N"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"text": [ID("IDC_COMBO"), "new.txt"]},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait": {"focused": "new.txt"}},
            {"expect": {"items": ["docs", "made", "sub", "b.txt", "c10.bin", "new.txt"]}},
            # a name that exists
            {"key": "ctrl+N"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"text": [ID("IDC_COMBO"), "new.txt"]},
            {"click": ID("IDOK")}]},
        {"on": "messagebox", "contains": "exists"},
        main_step({"expect": {"focused": "new.txt"}}),
    ], self.data)
    self.assertRun(run)
    captions = [d["caption"] for d in run.of("dialog", "IDD_COMBO")]
    self.assertEqual(captions, ["Create Folder", "Create File", "Create File"])
    self.assertTrue(os.path.isdir(os.path.join(self.data, "made", "deeper")))
    self.assertEqual(os.path.getsize(os.path.join(self.data, "new.txt")), 0)

  def test_comment(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "ctrl+Z"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"expect": [ID("IDC_COMBO"), "^$"]},
            {"text": [ID("IDC_COMBO"), "the second file"]},
            {"click": ID("IDOK")}]},
        main_step(
            # the comments are kept in descript.ion, as on Windows
            {"wait": {"items": ["docs", "sub", "b.txt", "c10.bin", "descript.ion"]}},
            {"expect": {"focused": "b.txt"}},
            {"key": "ctrl+Z"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"expect": [ID("IDC_COMBO"), "^the second file$"]},
            {"click": ID("IDCANCEL")}]},
    ], self.data)
    self.assertRun(run)
    self.assertEqual(run.of("dialog", "IDD_COMBO")[0]["caption"], "b.txt : Comment")
    with open(os.path.join(self.data, "descript.ion")) as f:
      self.assertEqual(f.read().strip(), "b.txt the second file")

  def test_comment_in_zip(self):
    archive = os.path.join(self.data, "arc.zip")
    oracle("a", "-bso0", archive, "b.txt", cwd=self.data)
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "ctrl+Z"}),
        {"on": "dialog", "name": "IDD_COMBO", "actions": [
            {"text": [ID("IDC_COMBO"), "zip comment"]},
            {"click": ID("IDOK")}]},
        main_step({"wait": {"focused": "b.txt"}}),
    ], archive)
    self.assertRun(run)
    self.assertIn("Comment = zip comment", oracle("l", "-slt", archive).stdout)
    oracle("t", archive)

  def test_rename_in_archive(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            {"key": "F2"},
            {"type": "renamed.txt"},
            {"key": "Return"},
            {"wait": {"items": ["inner", "nested.zip", "renamed.txt"]}}),
    ], archive)
    self.assertRun(run)
    listing = oracle("l", "-slt", archive).stdout
    self.assertIn("Path = renamed.txt", listing)
    self.assertNotIn("Path = top.txt", listing)
    oracle("t", archive)

  # ---------- Properties ----------

  def write_settings(self, text):
    os.makedirs(os.path.join(self.home, "config", "7-Zip"), exist_ok=True)
    with open(os.path.join(self.home, "config", "7-Zip", "7-Zip.ini"), "w") as f:
      f.write(text)

  def test_properties_of_files(self):
    # no session bus: a message box stands in for the file manager's dialog
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "alt+Return"}),
        {"on": "messagebox", "contains": "Name: b.txt"},
        main_step(
            {"click": {"panel": 0, "item": "docs", "mods": "ctrl"}},
            {"toolbar": ID("IDM_PROPERTIES")}),
        {"on": "messagebox", "contains": "Folders: 1\nFiles: 1"},
    ], self.data)
    self.assertRun(run)
    box = run.of("messagebox")[0]
    self.assertEqual(box["caption"], "Properties")
    self.assertIn("Location: " + self.data + "\n", box["text"])
    self.assertIn("Size: 21 bytes", box["text"])
    self.assertIn("Permissions: -rw-", box["text"])

  def test_properties_in_archive(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            {"key": "alt+Return"}),
        {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [
            {"screenshot": "properties.png"},
            {"click": ID("IDOK")}]},
    ], archive)
    self.assertRun(run)
    dialog = run.of("dialog", "IDD_LISTVIEW")[0]
    rows = [c for c in dialog["controls"] if c["class"] == "SysListView32"][0]["rows"]
    # the item, then the archive after a separator
    values = {}
    for r in rows:
      values.setdefault(r[0], r[1])
    self.assertEqual(values.get("Name"), "top.txt")
    self.assertEqual(values.get("Size"), "4")
    self.assertTrue(values.get("Method", "").startswith("LZMA2"), values)
    self.assertIn(["Type", "7z"], rows)

  def test_system_menu(self):
    self.write_settings('[Software/7-Zip/FM]\n"ShowSystemMenu"=dword:00000001\n')
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"menu": 0},
            {"choose": [0, "System", "Properties"]}),
        {"on": "messagebox", "contains": "Name: b.txt"},
    ], self.data)
    self.assertRun(run)
    items = run.of("menu")[0]["items"]
    self.assertEqual(items[0]["text"], "System")
    self.assertEqual([i["text"] for i in items[0]["items"]], ["&Open", "-", "Cu&t", "&Copy", "-", "P&roperties"])

  # ---------- Hashes, split and combine, links ----------

  def hash_rows(self, run):
    dialog = run.of("dialog_closed", "IDD_LISTVIEW")[-1]
    rows = [c for c in dialog["controls"] if c["class"] == "SysListView32"][0]["rows"]
    return {r[0]: r[1] for r in rows if len(r) > 1}

  def test_hash_files(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"command": ID("IDM_SHA256")}),
        {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [
            {"screenshot": "hash.png"},
            {"click": ID("IDOK")}]},
        main_step(
            {"click": {"panel": 0, "item": "c10.bin", "mods": "ctrl"}},
            {"choose": [0, "CRC", "CRC-32"]}),
        {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [{"click": ID("IDOK")}]},
    ], self.data)
    self.assertRun(run)
    import hashlib, zlib
    with open(os.path.join(self.data, "b.txt"), "rb") as f:
      b = f.read()
    with open(os.path.join(self.data, "c10.bin"), "rb") as f:
      c = f.read()
    first = run.of("dialog_closed", "IDD_LISTVIEW")[0]
    rows = [c2 for c2 in first["controls"] if c2["class"] == "SysListView32"][0]["rows"]
    self.assertEqual(first["caption"], "Checksum information")
    self.assertEqual(rows, [["Name", "b.txt"], ["Size", "21 bytes"],
                            ["SHA256", hashlib.sha256(b).hexdigest()]])
    # several files: the sum of their checksums, with the carry
    values = self.hash_rows(run)
    total = zlib.crc32(b) + zlib.crc32(c)
    self.assertEqual(values.get("CRC32 checksum for data"),
                     "%08X-%08X" % (total & 0xFFFFFFFF, total >> 32), values)
    self.assertEqual(values.get("Files"), "2")

  def test_hash_in_archive(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            {"command": ID("IDM_CRC32")}),
        {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [{"click": ID("IDOK")}]},
    ], archive)
    self.assertRun(run)
    import zlib
    self.assertEqual(self.hash_rows(run).get("CRC32"), "%08X" % zlib.crc32(b"top\n"))

  def test_split_and_combine(self):
    out = os.path.join(self.tmp, "parts")
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "c10.bin"}},
            {"command": ID("IDM_SPLIT")}),
        {"on": "dialog", "name": "IDD_SPLIT", "actions": [
            {"text": [ID("IDC_SPLIT_PATH"), out + "/"]},
            {"text": [ID("IDC_SPLIT_VOLUME"), "300"]},
            {"screenshot": "split.png"},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait_file": os.path.join(out, "c10.bin.004")},
            {"address": out},
            {"wait": {"items": ["c10.bin.001", "c10.bin.002", "c10.bin.003", "c10.bin.004"]}},
            {"click": {"panel": 0, "item": "c10.bin.001"}},
            {"command": ID("IDM_COMBINE")}),
        {"on": "dialog", "name": "IDD_COPY", "actions": [
            {"text": [ID("IDC_COPY"), os.path.join(self.tmp, "joined") + "/"]},
            {"click": ID("IDOK")}]},
        main_step({"wait_file": os.path.join(self.tmp, "joined", "c10.bin")}),
    ], self.data)
    self.assertRun(run)
    sizes = [os.path.getsize(os.path.join(out, "c10.bin.%03d" % i)) for i in range(1, 5)]
    self.assertEqual(sizes, [300, 300, 300, 100])
    with open(os.path.join(self.tmp, "joined", "c10.bin"), "rb") as f:
      self.assertEqual(f.read(), b"\0" * 1000)
    self.assertEqual(run.of("dialog", "IDD_COPY")[0]["caption"], "Combine Files c10.bin.001")

  def test_links(self):
    b = os.path.join(self.data, "b.txt")
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"command": ID("IDM_LINK")}),
        {"on": "dialog", "name": "IDD_LINK", "actions": [
            {"expect": [ID("IDC_LINK_PATH_TO"), "^" + re.escape(b) + "$"]},
            {"screenshot": "link.png"},
            {"text": [ID("IDC_LINK_PATH_FROM"), os.path.join(self.data, "hard.txt")]},
            {"check": [ID("IDR_LINK_TYPE_HARD"), True]},
            {"click": ID("IDB_LINK_LINK")}]},
        main_step(
            {"command": ID("IDM_LINK")}),
        {"on": "dialog", "name": "IDD_LINK", "actions": [
            # relative to the folder of the link
            {"text": [ID("IDC_LINK_PATH_FROM"), "sym.txt"]},
            {"text": [ID("IDC_LINK_PATH_TO"), "b.txt"]},
            {"check": [ID("IDR_LINK_TYPE_SYM_FILE"), True]},
            {"click": ID("IDB_LINK_LINK")}]},
        main_step(
            {"wait": {"items": ["docs", "sub", "b.txt", "c10.bin", "hard.txt", "sym.txt"]}},
            {"click": {"panel": 0, "item": "sym.txt"}},
            {"command": ID("IDM_LINK")}),
        {"on": "dialog", "name": "IDD_LINK", "actions": [
            # an existing link: its target, and it can be pointed elsewhere
            {"expect": [ID("IDT_LINK_PATH_TO_CUR"), "^b.txt$"]},
            {"expect": [ID("IDC_LINK_PATH_FROM"), "^" + re.escape(os.path.join(self.data, "sym.txt")) + "$"]},
            {"text": [ID("IDC_LINK_PATH_TO"), "c10.bin"]},
            {"click": ID("IDB_LINK_LINK")}]},
        main_step({"wait": {"focused": "sym.txt"}}),
    ], self.data)
    self.assertRun(run)
    self.assertEqual(os.stat(os.path.join(self.data, "hard.txt")).st_ino, os.stat(b).st_ino)
    self.assertEqual(os.readlink(os.path.join(self.data, "sym.txt")), "c10.bin")
    dialog = run.of("dialog", "IDD_LINK")[0]
    states = {c["id"]: c for c in dialog["controls"]}
    self.assertFalse(states[ID("IDR_LINK_TYPE_JUNCTION")]["enabled"])
    self.assertFalse(states[ID("IDR_LINK_TYPE_WSL")]["enabled"])
    self.assertTrue(states[ID("IDR_LINK_TYPE_HARD")]["checked"])
    third = run.of("dialog", "IDD_LINK")[2]
    self.assertTrue({c["id"]: c for c in third["controls"]}[ID("IDR_LINK_TYPE_SYM_FILE")]["checked"])

  # ---------- Add, Extract and Test ----------

  # command IDs of the archive toolbar (kMenuCmdID_Toolbar_* in App.h)
  TOOLBAR_ADD = 1070
  TOOLBAR_EXTRACT = 1071
  TOOLBAR_TEST = 1072

  def test_add_from_toolbar(self):
    archive = os.path.join(self.data, "new.zip")
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "docs"}},
            {"click": {"panel": 0, "item": "b.txt", "mods": "ctrl"}},
            {"toolbar": self.TOOLBAR_ADD}),
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            # the name of the archive comes from the folder
            {"expect": [ID("IDC_COMPRESS_ARCHIVE"), r"^data\.7z$"]},
            {"select": [ID("IDC_COMPRESS_FORMAT"), "zip"]},
            {"text": [ID("IDC_COMPRESS_ARCHIVE"), "new.zip"]},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait_file": archive},
            {"wait": {"items": ["docs", "sub", "b.txt", "c10.bin", "new.zip"]}}),
    ], self.data)
    self.assertRun(run)
    self.assertTrue(run.of("dialog", "IDD_PROGRESS"))
    listing = oracle("l", "-slt", archive).stdout
    self.assertIn("Type = zip", listing)
    self.assertIn("Path = docs" + os.sep + "a.txt", listing)
    self.assertIn("Path = b.txt", listing)
    oracle("t", archive)

  def test_extract_from_toolbar(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"toolbar": self.TOOLBAR_EXTRACT}),
        {"on": "dialog", "name": "IDD_EXTRACT", "actions": [
            {"expect": [ID("IDC_EXTRACT_PATH"), "^" + re.escape(self.data) + "/$"]},
            {"click": ID("IDOK")}]},
        main_step(
            {"wait_file": os.path.join(self.data, "arc", "inner", "x.txt")},
            {"wait": {"items": ["arc", "docs", "sub", "arc.7z", "b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    with open(os.path.join(self.data, "arc", "top.txt")) as f:
      self.assertEqual(f.read(), "top\n")

  def test_test_from_toolbar(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"toolbar": self.TOOLBAR_TEST}),
        {"on": "messagebox", "contains": "There are no errors"},
        # inside the archive: the panel tests the selected items
        main_step(
            {"key": "Return"},
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"key": "ctrl+A"},
            {"toolbar": self.TOOLBAR_TEST}),
        {"on": "messagebox", "contains": "There are no errors"},
    ], self.data)
    self.assertRun(run)
    boxes = [b["text"] for b in run.of("messagebox")]
    self.assertIn("Files: 3", boxes[0])
    self.assertIn("Files: 3", boxes[1])

  # ---------- Open outside and Edit ----------

  def test_edit_in_archive_with_editor(self):
    archive = self.make_archive()
    # an editor that changes the file a moment after it starts
    editor = os.path.join(self.tmp, "editor.sh")
    with open(editor, "w") as f:
      f.write('#!/bin/sh\nsleep 0.3\nprintf "edited\\n" >> "$1"\n')
    os.chmod(editor, 0o755)
    self.write_settings('[Software/7-Zip/FM]\n"Editor"="%s"\n' % editor)
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            {"key": "F4"}),
        {"on": "messagebox", "contains": "'top.txt' was modified", "answer": IDYES},
        main_step({"wait": {"focused": "top.txt"}}),
    ], archive)
    self.assertRun(run)
    out = os.path.join(self.tmp, "out")
    oracle("x", "-bso0", archive, "-o" + out, "top.txt")
    with open(os.path.join(out, "top.txt")) as f:
      self.assertEqual(f.read(), "top\nedited\n")

  def test_open_outside_in_archive(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            # no editor set: the desktop's application, as Open Outside
            {"key": "F4"},
            {"append_opened": "first\n"}),
        {"on": "messagebox", "contains": "was modified", "answer": IDNO},
        # every save is offered
        main_step({"append_opened": "second\n"}),
        {"on": "messagebox", "contains": "was modified", "answer": IDYES},
        main_step(
            {"wait": {"focused": "top.txt"}},
            {"key": "shift+Return"}),
    ], archive)
    self.assertRun(run)
    urls = [e["url"] for e in run.of("open_url")]
    self.assertEqual(len(urls), 2)
    self.assertTrue(all(u.startswith("file:///") and u.endswith("/top.txt") for u in urls), urls)
    # a temporary copy outside the archive folder
    self.assertNotIn(self.data, urls[0])
    out = os.path.join(self.tmp, "out")
    oracle("x", "-bso0", archive, "-o" + out, "top.txt")
    with open(os.path.join(out, "top.txt")) as f:
      self.assertEqual(f.read(), "top\nfirst\nsecond\n")
    # the temporary copies are removed when 7zFM exits
    self.assertTrue(urls[0].startswith("file://" + self.tmpdir + "/7zO"), urls)
    self.assertEqual(os.listdir(self.tmpdir), [])

  def test_open_and_edit_files(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "F4"},
            {"key": "Return"},
            {"click": {"panel": 0, "item": "docs"}},
            {"key": "shift+Return"}),
    ], self.data)
    self.assertRun(run)
    urls = [e["url"] for e in run.of("open_url")]
    b = "file://" + os.path.join(self.data, "b.txt")
    self.assertEqual(urls, [b, b, "file://" + os.path.join(self.data, "docs") + "/"])

  # ---------- Options and Help ----------

  def test_options_pages(self):
    actions = []
    for i in range(6):
      actions += [{"tab": i}, {"screenshot": "options-%d.png" % i}]
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"command": ID("IDM_OPTIONS")}),
        {"on": "dialog", "name": "PropertySheet", "actions": actions + [
            {"click": ID("IDCANCEL")}]},
    ], self.data)
    self.assertRun(run)
    sheet = run.of("dialog_closed", "PropertySheet")[0]
    self.assertEqual(sheet["caption"], "Options")
    pages = sorted({c["page"] for c in sheet["controls"] if "page" in c})
    self.assertEqual(pages, [0, 1, 2, 3, 4, 5])
    states = {(c["page"], c["id"]): c for c in sheet["controls"] if "page" in c}
    # Windows only: Zone.Identifier streams (7-Zip page), large pages (Settings)
    self.assertFalse(states[(1, ID("IDC_SYSTEM_ZONE"))]["enabled"])
    self.assertFalse(states[(4, ID("IDX_SETTINGS_LARGE_PAGES"))]["enabled"])

  def system_rows(self, run):
    sheet = run.of("dialog_closed", "PropertySheet")[-1]
    rows = [c for c in sheet["controls"] if c["id"] == ID("IDL_SYSTEM_ASSOCIATE")][0]["rows"]
    return {r[0]: r[1:] for r in rows}

  def test_system_page_associations(self):
    config = os.path.join(self.home, "config")
    os.makedirs(config)
    mimeapps = os.path.join(config, "mimeapps.list")
    with open(mimeapps, "w") as f:
      f.write("# kept\n[Default Applications]\napplication/zip=org.kde.ark.desktop;\n"
              "text/plain=org.kde.kate.desktop;\n")
    app = "io.github.mikigal.SevenZip.desktop"
    # nothing selected: "+" works on all types; a type of another program
    # is kept while there are types without a program, as on Windows
    first = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"command": ID("IDM_OPTIONS")}),
        {"on": "dialog", "name": "PropertySheet", "actions": [
            {"click": ID("IDB_SYSTEM_CURRENT")},
            {"click": ID("IDOK")}]},
    ], self.data)
    self.assertRun(first)
    with open(mimeapps) as f:
      text = f.read()
    self.assertTrue(text.startswith("# kept\n[Default Applications]\n"), text)
    self.assertIn("application/zip=org.kde.ark.desktop;\n", text)
    self.assertIn("application/x-7z-compressed=%s;\n" % app, text)
    self.assertIn("application/x-extension-001=%s;\n" % app, text)
    self.assertIn("text/plain=org.kde.kate.desktop;\n", text)
    self.assertIn("[Added Associations]\napplication/x-7z-compressed=%s;\n" % app, text)

    # the page reads them back; "+" takes the other program's type too,
    # then clears all
    second = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"command": ID("IDM_OPTIONS")}),
        {"on": "dialog", "name": "PropertySheet", "actions": [
            {"screenshot": "system.png"},
            {"click": ID("IDB_SYSTEM_CURRENT")},
            {"click": ID("IDB_SYSTEM_CURRENT")},
            {"click": ID("IDOK")}]},
    ], self.data)
    self.assertRun(second)
    sheet = second.of("dialog", "PropertySheet")[0]
    rows = [c for c in sheet["controls"] if c["id"] == ID("IDL_SYSTEM_ASSOCIATE")][0]["rows"]
    rows = {r[0]: r[1] for r in rows}
    self.assertEqual(rows["7z"], "7-Zip")
    self.assertEqual(rows["001"], "7-Zip")
    self.assertEqual(rows["zip"], "org.kde.ark.desktop")
    with open(mimeapps) as f:
      text = f.read()
    self.assertNotIn(app, text)
    self.assertIn("application/zip=org.kde.ark.desktop;\n", text)
    self.assertNotIn("application/x-7z-compressed", text)
    self.assertIn("text/plain=org.kde.kate.desktop;\n", text)

  def test_system_page_all_users(self):
    # the system file belongs to root: the change fails with an error
    etc = os.path.join(self.tmp, "etc-xdg")
    os.makedirs(etc)
    os.chmod(etc, 0o555)
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"command": ID("IDM_OPTIONS")}),
        {"on": "dialog", "name": "PropertySheet", "actions": [
            {"click": ID("IDB_SYSTEM_ALL")},
            {"click": ID("IDOK")}]},
        {"on": "messagebox", "contains": "denied"},
    ], self.data)
    os.chmod(etc, 0o755)
    self.assertRun(run)
    self.assertEqual(os.listdir(etc), [])

  def test_help_and_about(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "F1"},
            {"command": ID("IDM_OPTIONS")}),
        {"on": "dialog", "name": "PropertySheet", "actions": [
            {"tab": 4},
            {"click": ID("IDHELP")},
            {"click": ID("IDCANCEL")}]},
        main_step({"command": ID("IDM_ABOUT")}),
        {"on": "dialog", "name": "IDD_ABOUT", "actions": [
            {"screenshot": "about.png"},
            {"click": ID("IDB_ABOUT_HOMEPAGE")},
            {"click": ID("IDOK")}]},
    ], self.data)
    self.assertRun(run)
    urls = [e["url"] for e in run.of("open_url")]
    self.assertEqual(len(urls), 3, urls)
    # the online copy of the help, as there is no CHM file
    self.assertTrue(urls[0].endswith("/chm/fm/index.htm"), urls)
    self.assertTrue(urls[1].endswith("/chm/fm/options.htm#settings"), urls)
    self.assertEqual(urls[2], "https://www.7-zip.org/")
    about = run.of("dialog", "IDD_ABOUT")[0]
    version = [c["text"] for c in about["controls"] if c["id"] == ID("IDT_ABOUT_VERSION")][0]
    self.assertIn("7-Zip", version)

  def test_seven_zip_menu(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"choose": [0, "7-Zip", 'Add to "b.7z"']},
            {"wait_file": os.path.join(self.data, "b.7z")},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"choose": [0, "7-Zip", "CRC SHA", "SHA-256 -> b.txt.sha256"]},
            {"wait_file": os.path.join(self.data, "b.txt.sha256")},
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"choose": [0, "7-Zip", 'Extract to "arc/"']},
            {"wait_file": os.path.join(self.data, "arc", "inner", "x.txt")},
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"choose": [0, "7-Zip", "Test archive"]}),
        {"on": "messagebox", "contains": "There are no errors"},
    ], self.data)
    self.assertRun(run)
    self.assertIn("Path = b.txt", oracle("l", "-slt", os.path.join(self.data, "b.7z")).stdout)
    import hashlib
    with open(os.path.join(self.data, "b.txt"), "rb") as f:
      digest = hashlib.sha256(f.read()).hexdigest()
    with open(os.path.join(self.data, "b.txt.sha256")) as f:
      self.assertIn(digest, f.read())

  def test_diff(self):
    # the diff program of the Editor page gets both files
    script = os.path.join(self.tmp, "diff.sh")
    out = os.path.join(self.tmp, "diff-args")
    with open(script, "w") as f:
      f.write('#!/bin/sh\nprintf "%%s\\n" "$@" > "%s"\n' % out)
    os.chmod(script, 0o755)
    self.write_settings('[Software/7-Zip/FM]\n"Diff"="%s"\n' % script)
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"click": {"panel": 0, "item": "c10.bin", "mods": "ctrl"}},
            {"command": ID("IDM_DIFF")},
            {"wait_file": out}),
    ], self.data)
    self.assertRun(run)
    with open(out) as f:
      self.assertEqual(sorted(f.read().split("\n")[:2]),
                       [os.path.join(self.data, "b.txt"), os.path.join(self.data, "c10.bin")])

  def test_alternate_streams_are_windows_only(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"menu": 0}),
    ], self.data)
    self.assertRun(run)
    items = {i["text"]: i for i in run.of("menu")[0]["items"]}
    self.assertFalse(items["&Alternate streams"]["enabled"])

  # ---------- Drag and drop ----------

  def outside_files(self):
    outside = os.path.join(self.tmp, "outside")
    os.makedirs(os.path.join(outside, "folder"))
    with open(os.path.join(outside, "one.txt"), "w") as f:
      f.write("one\n")
    with open(os.path.join(outside, "folder", "two.txt"), "w") as f:
      f.write("two\n")
    return [os.path.join(outside, "one.txt"), os.path.join(outside, "folder")]

  def test_drop_files_on_folder_panel(self):
    # as from Explorer on Windows: the files go to a new archive
    files = self.outside_files()
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"drop": {"files": files, "panel": 0, "item": "sub"}}),
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            {"expect": [ID("IDC_COMPRESS_ARCHIVE"), r"^outside\.7z$"]},
            {"screenshot": "compress.png"},
            {"click": ID("IDOK")}]},
        main_step({"wait_file": os.path.join(self.data, "sub", "outside.7z")}),
    ], self.data)
    self.assertRun(run)
    # the folder item under the pointer is the folder of the archive
    listing = oracle("l", "-slt", os.path.join(self.data, "sub", "outside.7z")).stdout
    self.assertIn("Path = one.txt", listing)
    self.assertIn("Path = folder" + os.sep + "two.txt", listing)

  def test_drop_files_on_archive_panel(self):
    archive = self.make_archive()
    files = self.outside_files()
    run = self.run_fm([
        main_step(
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"drop": {"files": files, "panel": 0}}),
        {"on": "messagebox", "contains": "copy files to archive", "answer": IDYES},
        main_step({"wait": {"items": ["folder", "inner", "nested.zip", "one.txt", "top.txt"]}}),
    ], archive)
    self.assertRun(run)
    listing = oracle("l", "-slt", archive).stdout
    self.assertIn("Path = folder" + os.sep + "two.txt", listing)
    oracle("t", archive)

  def test_drag_between_panels(self):
    archive = self.make_archive()
    sub = os.path.join(self.data, "sub")
    run = self.run_fm([
        main_step(
            *self.two_panels(sub),
            # Ctrl: a copy, by the target
            {"click": {"panel": 0, "item": "b.txt"}},
            {"drag": {"panel": 0, "to": {"panel": 1, "mods": "ctrl"}}},
            {"wait": {"panel": 1, "items": ["b.txt"]}},
            # no keys, the same file system: a move, as on one drive on Windows
            {"click": {"panel": 0, "item": "c10.bin"}},
            {"drag": {"panel": 0, "to": {"panel": 1}}},
            {"wait": {"panel": 1, "items": ["b.txt", "c10.bin"]}},
            {"wait": {"panel": 0, "items": ["docs", "sub", "arc.7z", "b.txt"]}},
            # out of an archive: the source extracts into the folder that
            # the target names
            {"address": [0, archive]},
            {"wait": {"panel": 0, "path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "inner"}},
            {"drag": {"panel": 0, "to": {"panel": 1}}},
            {"wait": {"panel": 1, "items": ["inner", "b.txt", "c10.bin"]}},
            # into an archive: a copy, after a question
            {"focus": 1},
            {"click": {"panel": 1, "item": "b.txt"}},
            {"drag": {"panel": 1, "to": {"panel": 0}}}),
        {"on": "messagebox", "contains": "copy files to archive", "answer": IDYES},
        main_step(
            {"wait": {"panel": 0, "items": ["inner", "b.txt", "nested.zip", "top.txt"]}}),
    ], self.data)
    self.assertRun(run)
    with open(os.path.join(sub, "inner", "x.txt")) as f:
      self.assertEqual(f.read(), "x\n")
    self.assertTrue(os.path.exists(os.path.join(self.data, "b.txt")))
    self.assertFalse(os.path.exists(os.path.join(self.data, "c10.bin")))
    self.assertEqual(os.path.getsize(os.path.join(sub, "c10.bin")), 1000)
    self.assertTrue(os.path.exists(os.path.join(sub, "b.txt")))
    self.assertIn("Path = b.txt", oracle("l", "-slt", archive).stdout)
    # copy, move, move (extraction: upstream prefers move, the archive
    # stays as it is), copy (into the archive)
    self.assertEqual([d["action"] for d in run.of("drag")], [1, 2, 2, 1])

  def test_drag_to_other_program(self):
    archive = self.make_archive()
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"click": {"panel": 0, "item": "docs", "mods": "ctrl"}},
            {"drag": {"panel": 0, "to": "outside"}},
            {"key": "Return"}),
        main_step(
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"key": "Return"},
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"click": {"panel": 0, "item": "top.txt"}},
            {"drag": {"panel": 0, "to": "outside"}}),
    ], self.data)
    self.assertRun(run)
    drags = run.of("drag")
    # files of the file system: their paths
    self.assertIn("text/uri-list", drags[0]["formats"])
    self.assertEqual(sorted(drags[0]["files"]),
                     [os.path.join(self.data, "b.txt"), os.path.join(self.data, "docs")])
    # archive items: no paths before the target names a folder (the Ark
    # protocol needs a session bus, which these tests do not have)
    self.assertNotIn("text/uri-list", drags[1]["formats"])
    self.assertEqual(drags[1]["action"], 0)

  # ---------- Clipboard ----------

  def test_clipboard_copy_cut_paste(self):
    archive = self.make_archive()
    b = os.path.join(self.data, "b.txt")
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "b.txt"}},
            {"key": "ctrl+C"},
            {"log": "clipboard"},
            {"address": os.path.join(self.data, "sub")},
            {"wait": {"path": self.path_re("sub")}},
            {"key": "ctrl+V"},
            {"wait": {"items": ["b.txt"]}},
            {"address": self.data},
            {"wait": {"path": self.path_re()}},
            {"click": {"panel": 0, "item": "c10.bin"}},
            {"key": "ctrl+X"},
            {"log": "clipboard"},
            # cut files pasted into an archive are moved into it
            {"click": {"panel": 0, "item": "arc.7z"}},
            {"key": "Return"},
            {"wait": {"path": "^" + re.escape(archive) + "/$"}},
            {"key": "ctrl+V"},
            {"wait": {"items": ["inner", "c10.bin", "nested.zip", "top.txt"]}}),
    ], self.data)
    self.assertRun(run)
    copied, cut = run.of("clipboard")
    url = "file://" + b
    self.assertEqual(copied["formats"]["text/uri-list"].strip(), url)
    self.assertEqual(copied["formats"]["text/plain"], "b.txt")
    self.assertEqual(copied["formats"]["x-special/gnome-copied-files"], "copy\n" + url)
    self.assertEqual(cut["formats"]["application/x-kde-cutselection"], "1")
    self.assertTrue(os.path.exists(b))
    with open(os.path.join(self.data, "sub", "b.txt")) as f:
      self.assertEqual(f.read(), "world, a longer file\n")
    self.assertIn("Path = c10.bin", oracle("l", "-slt", archive).stdout)
    self.assertFalse(os.path.exists(os.path.join(self.data, "c10.bin")))

  # ---------- Window ----------

  def test_two_panels(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "F9"},
            {"wait": {"panels": 2}},
            {"focus": 0},
            {"key": "Tab"},
            {"expect": {"panel": 1, "focus_in_list": True}},
            {"address": [1, os.path.join(self.data, "docs")]},
            {"wait": {"panel": 1, "path": self.path_re("docs"), "focus_in_list": True}},
            {"screenshot": "two-panels.png"},
            {"key": "Tab"},
            {"expect": {"panel": 0, "focus_in_list": True, "path": self.path_re()}},
            {"key": "F9"},
            {"wait": {"panels": 1}}),
    ], self.data)
    self.assertRun(run)

  def test_view_modes_and_menu(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "ctrl+1"},
            {"screenshot": "large-icons.png"},
            {"menu": 2},
            {"key": "ctrl+3"},
            {"screenshot": "list.png"},
            {"key": "ctrl+4"},
            {"menu": 2},
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)
    menus = run.of("menu")
    self.assertEqual(len(menus), 2)
    def checked(menu):
      return [i["text"] for i in menu["items"] if i.get("checked")]
    self.assertIn("Lar&ge Icons\tCtrl+1", checked(menus[0]))
    self.assertIn("&Details\tCtrl+4", checked(menus[1]))
    self.assertIn("Name\tCtrl+F3", checked(menus[1]))

  def test_toolbar_options(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            # both toolbars, small buttons with text (the upstream defaults)
            {"expect": {"buttons": 7}},
            {"command": ID("IDM_VIEW_STANDARD_TOOLBAR")},
            {"expect": {"buttons": 3}},
            {"command": ID("IDM_VIEW_ARCHIVE_TOOLBAR")},
            {"expect": {"buttons": 0}},
            {"command": ID("IDM_VIEW_ARCHIVE_TOOLBAR")},
            {"command": ID("IDM_VIEW_STANDARD_TOOLBAR")},
            {"command": ID("IDM_VIEW_TOOLBARS_LARGE_BUTTONS")},
            {"expect": {"buttons": 7, "items": ["docs", "sub", "b.txt", "c10.bin"]}},
            {"screenshot": "large-buttons.png"},
            {"command": ID("IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT")},
            {"screenshot": "large-no-text.png"},
            {"menu": 2}),
    ], self.data)
    self.assertRun(run)
    toolbars = [i for i in run.of("menu")[0]["items"] if i["text"].startswith("Toolbars")]
    self.assertEqual(len(toolbars), 1)
    self.assertTrue(toolbars[0].get("submenu"), toolbars)

  def test_sort_by_size(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            # by size: the largest first, folders before files
            {"key": "ctrl+F6"},
            {"expect": {"items": ["sub", "docs", "c10.bin", "b.txt"]}},
            {"key": "ctrl+F6"},
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)

  def test_sort_by_column_click(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            # a new sort column: ascending, but the largest first by size
            {"header": {"column": "Size"}},
            {"expect": {"items": ["sub", "docs", "c10.bin", "b.txt"]}},
            {"header": {"column": "Size"}},
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}},
            {"header": {"column": "Name"}},
            {"expect": {"items": ["docs", "sub", "b.txt", "c10.bin"]}},
            # descending; the folders stay first
            {"header": {"column": "Name"}},
            {"expect": {"items": ["sub", "docs", "c10.bin", "b.txt"]}}),
    ], self.data)
    self.assertRun(run)

  def test_flat_view(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"command": ID("IDM_VIEW_FLAT_VIEW")},
            {"wait": {"items": ["docs", "sub", "a.txt", "b.txt", "c10.bin"]}},
            {"screenshot": "flat.png"},
            {"command": ID("IDM_VIEW_FLAT_VIEW")},
            {"wait": {"items": ["docs", "sub", "b.txt", "c10.bin"]}}),
    ], self.data)
    self.assertRun(run)

  def test_root_folder(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"address": [0, "/"]},
            {"wait": {"path": "^/$"}},
            # the parent of "/" is the root, with "/" focused
            {"key": "Backspace"},
            {"wait": {"path": "^$", "focused": "/",
                      "columns": ["Name", "Total Size", "Free Space", "File System", "Label"]}},
            {"log": "panels"},
            {"screenshot": "root.png"},
            {"key": "Space"},
            {"key": "Return"},
            {"wait": {"path": "^/$"}},
            {"address": [0, self.data]},
            {"wait": {"path": self.path_re()}},
            # "/" or "\" in the list opens the root
            {"key": "/"},
            {"wait": {"path": "^$"}},
            {"address": [0, self.data]},
            {"wait": {"path": self.path_re()}},
            {"command": ID("IDM_OPEN_ROOT_FOLDER")},
            {"wait": {"path": "^$"}}),
    ], self.data)
    self.assertRun(run)
    root = [p for p in run.panels() if p.get("created") and p["path"] == ""][0]
    # the home directory, "/", then the mounted file systems of the user
    self.assertEqual(root["items"][:2], [self.home, "/"])

  def test_bookmarks(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "alt+shift+1"},
            {"key": "Space"},
            {"key": "Return"},
            {"wait": {"path": self.path_re("docs")}},
            {"key": "alt+1"},
            {"wait": {"path": self.path_re()}},
            {"menu": 3}),
    ], self.data)
    self.assertRun(run)
    items = [i["text"] for i in run.of("menu")[0]["items"]]
    self.assertIn(self.data + "/\tAlt+1", items)
    self.assertIn("-\tAlt+2", items)

  def test_folders_history(self):
    run = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "Space"},
            {"key": "Return"},
            {"wait": {"path": self.path_re("docs")}},
            {"key": "alt+F12"}),
        {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [
            {"screenshot": "history.png"},
            {"click": ID("IDCANCEL")}]},
        main_step({"expect": {"path": self.path_re("docs")}}),
    ], self.data)
    self.assertRun(run)
    dialog = run.of("dialog", "IDD_LISTVIEW")[0]
    rows = [c for c in dialog["controls"] if c["class"] == "SysListView32"][0]["rows"]
    self.assertEqual([r[0] for r in rows[:2]],
                     [os.path.join(self.data, "docs") + "/", self.data + "/"])

  def test_settings_are_kept(self):
    first = self.run_fm([
        main_step(
            {"wait": {"path": self.path_re()}},
            {"key": "Space"},
            {"key": "Return"},
            {"wait": {"path": self.path_re("docs")}}),
    ], self.data)
    self.assertRun(first)
    second = self.run_fm([
        main_step({"wait": {"path": self.path_re("docs")}}),
    ])
    self.assertRun(second)


def main():
  global DRIVER, ORACLE, ARTIFACTS, IDS
  parser = argparse.ArgumentParser()
  parser.add_argument("--driver", required=True)
  parser.add_argument("--oracle", required=True)
  parser.add_argument("--artifacts", required=True)
  args, rest = parser.parse_known_args()
  DRIVER = args.driver
  ORACLE = args.oracle
  ARTIFACTS = args.artifacts
  os.makedirs(ARTIFACTS, exist_ok=True)
  IDS = rcgen.parse_file(os.path.join(ROOT, "CPP/7zip/Bundles/Fm/resource.rc")).symbols
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
