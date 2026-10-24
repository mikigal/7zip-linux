#!/usr/bin/env python3
"""The 7-Zip extensions of GTK file managers (linux/gtk) in the real Nemo
and Thunar, in the nested headless KWin: with the keyboard from libei the
test selects an archive, opens the context menu, goes to 7-Zip > Extract to
"name/" and checks that 7zG extracted it there.

Both file managers load extensions from a fixed folder, so each runs in a
user and mount namespace where the folder holds only the 7-Zip module
(bind mount), and then as the user again (a nested user namespace)."""

import argparse
import os
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

EI_INPUT = None
TOOL = None
PROGRAMS = None   # the folder of 7zG and 7zFM
ORACLE = None
ARTIFACTS = None
MODULES = {}
WIDTH, HEIGHT = 1280, 800


def namespaces_work():
  res = subprocess.run(["unshare", "-rm", "true"], capture_output=True)
  return res.returncode == 0


class GtkFileManagers(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-gtkfm-wl-")
    self.files = os.path.join(self.tmp, "files")
    os.makedirs(self.files)
    with open(os.path.join(self.files, "c.txt"), "w") as f:
      f.write("text\n")
    subprocess.run([ORACLE, "a", "-bso0", "my_arc.7z", "c.txt"], cwd=self.files, check=True)
    self.artifacts = os.path.join(ARTIFACTS, self.id().split(".")[-1])
    shutil.rmtree(self.artifacts, ignore_errors=True)
    os.makedirs(self.artifacts)

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def run_file_manager(self, name, folder, module_dir, extra_files=()):
    if name not in MODULES or not shutil.which(name):
      self.skipTest("needs %s and its 7-Zip extension" % name)
    if not namespaces_work():
      self.skipTest("needs unprivileged user namespaces")
    # the extension folder of the file manager, with the 7-Zip module only
    ext = os.path.join(self.tmp, "ext")
    os.makedirs(os.path.join(ext, module_dir), exist_ok=True)
    for path in extra_files:
      shutil.copy(path, ext)
    shutil.copy(MODULES[name], os.path.join(ext, module_dir))
    inner = 'mount --bind "$1" "$2" && shift 2 && exec unshare --user --map-user=%d --map-group=%d "$@"' % (
        os.getuid(), os.getgid())
    env = {"Z7_PROGRAM_DIR": PROGRAMS, "Z7_CONTEXT_MENU_TOOL": TOOL}
    with wlsession.Session(WIDTH, HEIGHT, artifacts=self.artifacts) as s:
      s.spawn(["unshare", "-rm", "sh", "-c", inner, "sh", ext, folder, name, self.files],
              log=name, env=env)
      windows = []

      def window():
        windows[:] = s.kwin_query("workspace.windowList().filter(w => w.normalWindow).map(w => w.caption)")
        return bool(windows)
      wlsession.wait_until(window, 30, "the window of " + name, interval=0.5)
      s.place_window(windows[0], 0, 0, WIDTH, HEIGHT)
      time.sleep(1.5)
      # select my_arc.7z by typing; the Menu key opens the context menu,
      # where 7-Zip comes before Properties, the last item
      for keys in ("m", "y", "esc", "menu", "up", "up"):
        s.run([EI_INPUT, "key", keys])
        time.sleep(0.3)
      s.run([EI_INPUT, "key", "right"])
      time.sleep(0.8)
      s.screenshot(os.path.join(self.artifacts, "menu.png"))
      # Open archive, Open archive >, Extract files..., Extract Here, Extract to
      for keys in ("down", "down", "down", "down", "return"):
        s.run([EI_INPUT, "key", keys])
        time.sleep(0.3)
      target = os.path.join(self.files, "my_arc", "c.txt")
      wlsession.wait_until(lambda: os.path.exists(target), 30, "the archive extracted by 7zG")
      s.screenshot(os.path.join(self.artifacts, "after.png"))
    with open(target) as f:
      self.assertEqual(f.read(), "text\n")

  def test_nemo(self):
    # /usr/lib/nemo/extensions-3.0 does not exist without extensions
    self.run_file_manager("nemo", "/usr/lib/nemo", "extensions-3.0",
                          ["/usr/lib/nemo/nemo-extensions-list"])

  def test_thunar(self):
    self.run_file_manager("thunar", "/usr/lib/thunarx-3", ".")


def main():
  global EI_INPUT, TOOL, PROGRAMS, ORACLE, ARTIFACTS
  ap = argparse.ArgumentParser()
  ap.add_argument("--ei-input", required=True)
  ap.add_argument("--tool", required=True)
  ap.add_argument("--7zg", dest="gui", required=True)
  ap.add_argument("--7zfm", dest="fm", required=True)
  ap.add_argument("--oracle", required=True)
  ap.add_argument("--artifacts", required=True)
  ap.add_argument("--nemo")
  ap.add_argument("--thunar")
  args, rest = ap.parse_known_args()
  EI_INPUT = os.path.abspath(args.ei_input)
  TOOL = os.path.abspath(args.tool)
  ORACLE = os.path.abspath(args.oracle)
  ARTIFACTS = os.path.abspath(args.artifacts)
  for name in ("nemo", "thunar"):
    if getattr(args, name):
      MODULES[name] = os.path.abspath(getattr(args, name))
  # 7zG and 7zFM in one folder, as installed
  programs = tempfile.mkdtemp(prefix="z7-gtkfm-programs-")
  os.symlink(os.path.abspath(args.gui), os.path.join(programs, "7zG"))
  os.symlink(os.path.abspath(args.fm), os.path.join(programs, "7zFM"))
  PROGRAMS = programs
  try:
    result = unittest.main(argv=[sys.argv[0]] + rest, verbosity=2, exit=False).result
  finally:
    shutil.rmtree(programs, ignore_errors=True)
  sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
  main()
