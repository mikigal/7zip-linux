#!/usr/bin/env python3
"""The 7-Zip service menus of linux/packaging as KIO gives them to Dolphin:
z7-servicemenu-probe lists the actions for files with the menus installed
in a data directory of their own."""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

PROBE = None
PACKAGING = None


class ServiceMenus(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-servicemenus-")
    menus = os.path.join(self.tmp, "share", "kio", "servicemenus")
    os.makedirs(menus)
    for name in os.listdir(PACKAGING):
      if ".servicemenu-" in name:
        shutil.copy(os.path.join(PACKAGING, name), menus)
    self.files = os.path.join(self.tmp, "files")
    os.makedirs(os.path.join(self.files, "folder"))
    for name in ("a.7z", "b.zip", "c.txt"):
      with open(os.path.join(self.files, name), "wb") as f:
        f.write(b"7z\xbc\xaf\x27\x1c" if name.endswith(".7z") else b"PK\x03\x04" if name.endswith(".zip") else b"text\n")

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def actions(self, *names):
    home = os.path.join(self.tmp, "home")
    os.makedirs(home, exist_ok=True)
    env = {
        "PATH": "/usr/bin:/bin",
        "HOME": home,
        "XDG_CONFIG_HOME": os.path.join(home, "config"),
        "XDG_DATA_HOME": os.path.join(home, "data"),
        "XDG_CACHE_HOME": os.path.join(home, "cache"),
        "XDG_RUNTIME_DIR": home,
        "XDG_DATA_DIRS": os.path.join(self.tmp, "share") + ":/usr/share",
        "QT_QPA_PLATFORM": "offscreen",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=" + os.path.join(home, "no-bus"),
    }
    res = subprocess.run([PROBE] + [os.path.join(self.files, n) for n in names],
                         env=env, capture_output=True, text=True, timeout=30)
    self.assertEqual(res.returncode, 0, res.stderr)
    return [line.split("\t")[0] for line in res.stdout.splitlines() if line.startswith("7-Zip/")]

  def test_archive(self):
    # one 7-Zip submenu for both files; KIO sorts the actions by their text
    self.assertEqual(sorted(self.actions("a.7z")), sorted([
        "7-Zip/Open archive", "7-Zip/Extract files...", "7-Zip/Extract Here",
        "7-Zip/Extract to folder", "7-Zip/Test archive",
        "7-Zip/Add to archive...", "7-Zip/Add to 7z archive", "7-Zip/Add to zip archive",
        "7-Zip/Compress and email...", "7-Zip/Compress to 7z and email",
        "7-Zip/Compress to zip and email", "7-Zip/SHA-256 checksum", "7-Zip/CRC-32 checksum",
        "7-Zip/All checksums"]))

  def test_other_files(self):
    # no extraction for files that are not archives, or folders
    for names in (("c.txt",), ("folder",), ("a.7z", "c.txt")):
      actions = self.actions(*names)
      self.assertIn("7-Zip/Add to archive...", actions, names)
      self.assertNotIn("7-Zip/Extract Here", actions, names)


def main():
  global PROBE, PACKAGING
  ap = argparse.ArgumentParser()
  ap.add_argument("--probe", required=True)
  ap.add_argument("--packaging", required=True)
  args, rest = ap.parse_known_args()
  PROBE = os.path.abspath(args.probe)
  PACKAGING = os.path.abspath(args.packaging)
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
