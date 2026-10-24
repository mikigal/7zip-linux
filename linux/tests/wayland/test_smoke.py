#!/usr/bin/env python3
"""Smoke test of the Wayland test infrastructure.

Starts the nested headless KWin, shows z7-probe full screen, checks that it is
a native Wayland client, that the screenshot shows its color, and that
synthetic pointer and keyboard input from ei-input reaches it.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import wlsession  # noqa: E402

PROBE = None
EI_INPUT = None
ARTIFACTS = None
WIDTH, HEIGHT = 1280, 800
PROBE_COLOR = "3366CC"


def read_lines(path):
  try:
    with open(path, encoding="utf-8") as f:
      return [l.rstrip("\n") for l in f]
  except FileNotFoundError:
    return []


def pixel(png, x, y):
  res = subprocess.run(["magick", png, "-format", "%%[hex:p{%d,%d}]" % (x, y), "info:"],
                       capture_output=True, text=True, check=True)
  return res.stdout.strip().upper()[:6]


class Smoke(unittest.TestCase):

  def test_probe_window(self):
    with wlsession.Session(WIDTH, HEIGHT, artifacts=ARTIFACTS) as s:
      log = os.path.join(s.artifacts, "probe.log")
      s.spawn([PROBE, "--color", "#" + PROBE_COLOR], log="probe")
      wlsession.wait_until(lambda: "ready" in read_lines(log), 20, "probe window to be ready")
      self.assertIn("platform wayland", read_lines(log))

      shot = s.screenshot(os.path.join(s.artifacts, "smoke.png"))
      self.assertEqual(PROBE_COLOR, pixel(shot, WIDTH // 2, HEIGHT // 2))

      if EI_INPUT:
        s.run([EI_INPUT, "move", str(WIDTH // 2), str(HEIGHT // 2), "click", "right",
               "type", "z7", "key", "f5"], timeout=30)
        def got_input():
          lines = read_lines(log)
          return ("key 7" in lines and any(l.startswith("key ") and l.split()[1].isdigit()
                                           and l != "key 7" for l in lines))
        wlsession.wait_until(got_input, 10, "keyboard input in the probe window")
        lines = read_lines(log)
        self.assertTrue(any(l.startswith("press 640 400 right") for l in lines), lines)
        self.assertTrue(any(l.startswith("release 640 400 right") for l in lines), lines)
        self.assertIn("key z", lines)

  @unittest.skipUnless(EI_INPUT or "--ei-input" in sys.argv, "needs ei-input")
  def test_drag_and_drop_between_clients(self):
    with wlsession.Session(WIDTH, HEIGHT, artifacts=ARTIFACTS) as s:
      payload = os.path.join(s.root, "dragged file.txt")
      with open(payload, "w") as f:
        f.write("payload\n")
      src_log = os.path.join(s.artifacts, "dnd-source.log")
      dst_log = os.path.join(s.artifacts, "dnd-target.log")
      s.spawn([PROBE, "--title", "z7-src", "--size", "600x700", "--color", "#aa3333",
               "--drag-file", payload], log="dnd-source")
      s.spawn([PROBE, "--title", "z7-dst", "--size", "600x700", "--color", "#33aa33"],
              log="dnd-target")
      for log in (src_log, dst_log):
        wlsession.wait_until(lambda: "ready" in read_lines(log), 20, "probe windows to be ready")
      s.place_window("z7-src", 0, 0, WIDTH // 2, HEIGHT)
      s.place_window("z7-dst", WIDTH // 2, 0, WIDTH // 2, HEIGHT)
      time.sleep(0.5)
      s.screenshot(os.path.join(s.artifacts, "dnd-before.png"))

      s.run([EI_INPUT, "drag", str(WIDTH // 4), str(HEIGHT // 2),
             str(WIDTH * 3 // 4), str(HEIGHT // 2), "30"], timeout=30)
      url = "drop-url file://" + payload  # QUrl::toString() is percent-decoded
      wlsession.wait_until(lambda: url in read_lines(dst_log), 10, "the drop in the target window")
      wlsession.wait_until(lambda: any(l.startswith("drag-end") for l in read_lines(src_log)),
                           10, "the drag to finish in the source window")
      self.assertIn("drag-end copy", read_lines(src_log))
      self.assertTrue(any(l.startswith("drop copy formats=") and "text/uri-list" in l
                          for l in read_lines(dst_log)), read_lines(dst_log))


def main():
  global PROBE, EI_INPUT, ARTIFACTS
  ap = argparse.ArgumentParser()
  ap.add_argument("--probe", required=True)
  ap.add_argument("--ei-input")
  ap.add_argument("--artifacts")
  args, rest = ap.parse_known_args()
  PROBE = os.path.abspath(args.probe)
  EI_INPUT = os.path.abspath(args.ei_input) if args.ei_input else None
  ARTIFACTS = os.path.abspath(args.artifacts) if args.artifacts else tempfile.mkdtemp(prefix="z7-smoke-")
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
