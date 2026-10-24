#!/usr/bin/env python3
"""7zFM as a Wayland client in the nested headless KWin: the main window is
decorated and active, keyboard input from libei drives the panel (the
window title follows the folder), Alt+Shift+digit and Alt+digit keep and
open bookmarks with a real keyboard layout, a dialog of the main window is
modal and transient for it, and the main window gets the focus back after
it.

7zFM runs as z7-fm-driver (see tests/gui/fm_driver.cpp); its script only
waits while the test types, and checks the panels at the end.
"""

import argparse
import json
import os
import re
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))
import wlsession  # noqa: E402

DRIVER = None
EI_INPUT = None
ARTIFACTS = None
WIDTH, HEIGHT = 1280, 800

WINDOWS = """workspace.windowList().filter(w => w.normalWindow || w.dialog).map(w => ({
  caption: w.caption, active: w.active, transient: w.transient, modal: w.modal,
  transientFor: w.transientFor ? w.transientFor.caption : null,
  decorated: !w.noBorder, desktopFileName: w.desktopFileName,
  width: w.frameGeometry.width, height: w.frameGeometry.height }))"""


def events(log):
  try:
    with open(log) as f:
      return [json.loads(line) for line in f if line.strip()]
  except (FileNotFoundError, json.JSONDecodeError):
    return []


class Wayland7zFM(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-7zfm-wl-")
    self.data = os.path.join(self.tmp, "data")
    os.makedirs(os.path.join(self.data, "sub"))
    with open(os.path.join(self.data, "b.txt"), "w") as f:
      f.write("b\n")
    self.artifacts = os.path.join(ARTIFACTS, self.id().split(".")[-1])
    os.makedirs(self.artifacts, exist_ok=True)
    self.log = os.path.join(self.artifacts, "7zfm-events.jsonl")

  def start_7zfm(self, session, script, *args):
    path = os.path.join(self.tmp, "script.json")
    with open(path, "w") as f:
      json.dump(script, f)
    return session.spawn([DRIVER, "--script", path, "--log", self.log, "--timeout", "90",
                          "--", *args], log="7zfm", cwd=self.tmp)

  def wait_windows(self, session, predicate, what):
    result = []

    def ready():
      windows = session.kwin_query(WINDOWS)
      if predicate(windows):
        result[:] = windows
        return True
      return False
    wlsession.wait_until(ready, 20, what, interval=0.2)
    return result

  def wait_active(self, session, caption):
    windows = self.wait_windows(
        session, lambda ws: any(w["caption"] == caption and w["active"] for w in ws),
        "active window " + caption)
    return [w for w in windows if w["caption"] == caption][0]

  def keys(self, session, *keys):
    argv = [EI_INPUT]
    for k in keys:
      argv += ["key", k]
    session.run(argv, timeout=30)

  @unittest.skipUnless(EI_INPUT or "--ei-input" in sys.argv, "needs ei-input")
  def test_keyboard_bookmarks_and_dialog(self):
    data = self.data + "/"
    sub = os.path.join(self.data, "sub") + "/"
    go = os.path.join(self.tmp, "go")
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      p = self.start_7zfm(s, [
          {"on": "main", "actions": [
              {"wait": {"path": "^" + re.escape(data) + "$"}},
              {"wait_file": go},
              {"expect": {"panels": 2, "panel": 0, "path": "^" + re.escape(sub) + "$"}}]},
          {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [
              {"wait_file": os.path.join(self.tmp, "never")}]},
      ], self.data)
      main = self.wait_active(s, data)
      self.assertEqual(events(self.log)[0], {"event": "start", "platform": "wayland"})
      self.assertTrue(main["decorated"], main)
      self.assertFalse(main["transient"], main)
      # the app ID names the .desktop file (linux/packaging)
      self.assertEqual(main["desktopFileName"], "pl.mikigal.SevenZip", main)
      s.screenshot(os.path.join(self.artifacts, "main.png"))

      # the first item has the focus; Space selects it, Enter opens it
      self.keys(s, "space", "enter")
      self.wait_active(s, sub)
      # Shift+1 gives "!" on the US layout: still the bookmark key 1
      self.keys(s, "alt+shift+1", "backspace")
      self.wait_active(s, data)
      self.keys(s, "alt+1")
      self.wait_active(s, sub)

      self.keys(s, "alt+f12")
      windows = self.wait_windows(
          s, lambda ws: any(w["caption"] == "Folders History" and w["active"] for w in ws),
          "the Folders History dialog")
      history = [w for w in windows if w["caption"] == "Folders History"][0]
      self.assertTrue(history["modal"], windows)
      self.assertEqual(history["transientFor"], sub, windows)
      s.screenshot(os.path.join(self.artifacts, "history.png"))
      self.keys(s, "esc")
      self.wait_active(s, sub)

      self.keys(s, "f9")
      time.sleep(0.5)
      s.screenshot(os.path.join(self.artifacts, "two-panels.png"))
      with open(go, "w"):
        pass
      code = p.wait(60)
    errors = [e["message"] for e in events(self.log) if e["event"] == "error"]
    self.assertEqual(errors, [])
    self.assertEqual(code, 0)


def main():
  global DRIVER, EI_INPUT, ARTIFACTS
  ap = argparse.ArgumentParser()
  ap.add_argument("--driver", required=True)
  ap.add_argument("--ei-input")
  ap.add_argument("--artifacts", required=True)
  args, rest = ap.parse_known_args()
  DRIVER = os.path.abspath(args.driver)
  EI_INPUT = os.path.abspath(args.ei_input) if args.ei_input else None
  ARTIFACTS = os.path.abspath(args.artifacts)
  os.makedirs(ARTIFACTS, exist_ok=True)
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
