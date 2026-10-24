#!/usr/bin/env python3
"""The 7-Zip context menus of KDE file managers as KIO gives them to Dolphin
(KFileItemActions): z7-servicemenu-probe lists the actions for files and
runs one.

- the service menus of linux/packaging, installed in a data directory of
  their own;
- the plugin of linux/kde (when it is built): the menu of upstream
  Explorer/ContextMenu.cpp with the 7-Zip options, and the 7zG and 7zFM
  commands of its actions, which the oracle runs to check them.

The other plugins of the system are disabled in kservicemenurc."""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

PROBE = None
PACKAGING = None
PLUGIN = None
ORACLE = None


def oracle(*args, cwd=None):
  res = subprocess.run([ORACLE] + list(args), cwd=cwd, capture_output=True, text=True, timeout=60)
  if res.returncode != 0:
    raise AssertionError("7zz %s: %s%s" % (" ".join(args), res.stdout, res.stderr))
  return res


class KdeMenus(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-kde-menus-")
    self.home = os.path.join(self.tmp, "home")
    self.config = os.path.join(self.home, "config")
    os.makedirs(self.config)
    self.share = os.path.join(self.tmp, "share")
    os.makedirs(self.share)
    self.files = os.path.join(self.tmp, "files")
    os.makedirs(os.path.join(self.files, "folder"))
    with open(os.path.join(self.files, "c.txt"), "w") as f:
      f.write("text\n")
    oracle("a", "-bso0", "a.7z", "c.txt", cwd=self.files)
    oracle("a", "-bso0", "-tzip", "b.zip", "c.txt", cwd=self.files)
    self.extra_env = {}

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def env(self):
    env = {
        "PATH": "/usr/bin:/bin",
        "HOME": self.home,
        "XDG_CONFIG_HOME": self.config,
        "XDG_DATA_HOME": os.path.join(self.home, "data"),
        "XDG_CACHE_HOME": os.path.join(self.home, "cache"),
        "XDG_RUNTIME_DIR": self.home,
        "XDG_DATA_DIRS": self.share + ":/usr/share",
        "QT_QPA_PLATFORM": "offscreen",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=" + os.path.join(self.home, "no-bus"),
    }
    env.update(self.extra_env)
    return env

  def probe(self, *args):
    """[(path, icon)] of the actions: "submenu > text", "submenu >", "-"."""
    res = subprocess.run([PROBE] + [os.path.join(self.files, a) if not a.startswith("-") and "/" not in a and " > " not in a else a
                                    for a in args],
                         env=self.env(), capture_output=True, text=True, timeout=30)
    self.assertEqual(res.returncode, 0, res.stderr)
    items = []
    for line in res.stdout.splitlines():
      parts = line.split("\t")
      items.append((parts[0], len(parts) > 2 and parts[2] == "icon"))
    return items


class ServiceMenus(KdeMenus):

  def setUp(self):
    super().setUp()
    menus = os.path.join(self.share, "kio", "servicemenus")
    os.makedirs(menus)
    for name in os.listdir(PACKAGING):
      if ".servicemenu-" in name:
        shutil.copy(os.path.join(PACKAGING, name), menus)

  def actions(self, *names):
    return [p for p, _ in self.probe(*names) if p.startswith("7-Zip > ") and not p.endswith(" >")]

  def test_archive(self):
    # one 7-Zip submenu for both files; KIO sorts the actions by their text
    self.assertEqual(sorted(self.actions("a.7z")), sorted([
        "7-Zip > Open archive", "7-Zip > Extract files...", "7-Zip > Extract Here",
        "7-Zip > Extract to folder", "7-Zip > Test archive",
        "7-Zip > Add to archive...", "7-Zip > Add to 7z archive", "7-Zip > Add to zip archive",
        "7-Zip > Compress and email...", "7-Zip > Compress to 7z and email",
        "7-Zip > Compress to zip and email", "7-Zip > SHA-256 checksum", "7-Zip > CRC-32 checksum",
        "7-Zip > All checksums"]))

  def test_other_files(self):
    # no extraction for files that are not archives, or folders
    for names in (("c.txt",), ("folder",), ("a.7z", "c.txt")):
      actions = self.actions(*names)
      self.assertIn("7-Zip > Add to archive...", actions, names)
      self.assertNotIn("7-Zip > Extract Here", actions, names)


class Plugin(KdeMenus):

  def setUp(self):
    if not PLUGIN:
      self.skipTest("the KDE plugin is not built")
    super().setUp()
    plugins = os.path.join(self.tmp, "plugins", "kf6", "kfileitemaction")
    os.makedirs(plugins)
    os.symlink(PLUGIN, os.path.join(plugins, os.path.basename(PLUGIN)))
    # only this plugin: KIO runs the enabled plugins of the system too
    with open(os.path.join(self.config, "kservicemenurc"), "w") as f:
      f.write("[Show]\n")
      for path in glob.glob("/usr/lib*/qt6/plugins/kf6/kfileitemaction/*.so"):
        name = os.path.splitext(os.path.basename(path))[0]
        if name != "sevenzipfileitemaction":
          f.write(name + "=false\n")
    # 7zG and 7zFM write their arguments, one per line
    self.bin = os.path.join(self.tmp, "bin")
    os.makedirs(self.bin)
    for name in ("7zG", "7zFM"):
      path = os.path.join(self.bin, name)
      with open(path, "w") as f:
        f.write('#!/bin/sh\nfor a in "$@"; do printf "%s\\n" "$a"; done > "$0.tmp" && mv "$0.tmp" "$0.log"\n')
      os.chmod(path, 0o755)
    self.extra_env = {
        "QT_PLUGIN_PATH": os.path.join(self.tmp, "plugins"),
        "Z7_PROGRAM_DIR": self.bin,
    }

  def write_settings(self, text):
    os.makedirs(os.path.join(self.config, "7-Zip"), exist_ok=True)
    with open(os.path.join(self.config, "7-Zip", "7-Zip.ini"), "w") as f:
      f.write(text)

  def menu(self, *names):
    """The direct items of the 7-Zip submenu."""
    items = [p[len("7-Zip > "):] for p, _ in self.probe("--plugins", *names) if p.startswith("7-Zip > ")]
    return [p for p in items if " > " not in p]

  def run_action(self, program, path, *names):
    log = os.path.join(self.bin, program + ".log")
    if os.path.exists(log):
      os.remove(log)
    self.probe("--plugins", "--trigger", path, *names)
    # the program runs detached
    for _ in range(100):
      if os.path.exists(log):
        break
      time.sleep(0.1)
    with open(log) as f:
      return f.read().splitlines()

  def test_archive(self):
    # the menu of the Windows shell extension, in its order
    self.assertEqual(self.menu("b.zip"), [
        "Open archive", "Open archive >", "Extract files...", "Extract Here", 'Extract to "b/"',
        "Test archive", "Add to archive...", "Compress and email...",
        'Add to "b_2.7z"', 'Compress to "b_2.7z" and email',
        'Add to "b_2.zip"', 'Compress to "b_2.zip" and email', "CRC SHA >"])
    items = [p for p, _ in self.probe("--plugins", self.f("b.zip"))]
    self.assertIn("7-Zip > Open archive > zip", items)
    self.assertIn("7-Zip > CRC SHA > SHA-256", items)
    self.assertIn("7-Zip > CRC SHA > SHA-256 -> b.zip.sha256", items)
    # several archives go to folders named after them
    self.assertIn('Extract to "*/"', self.menu("a.7z", "b.zip"))

  def test_other_files(self):
    for names, name in ((("c.txt",), "c"), (("folder",), "folder"), (("a.7z", "c.txt"), "files")):
      menu = self.menu(*names)
      self.assertEqual(menu[:4], ["Add to archive...", "Compress and email...",
                                  'Add to "%s.7z"' % name, 'Compress to "%s.7z" and email' % name], names)
      self.assertNotIn("Extract Here", menu, names)

  def f(self, name):
    return os.path.join(self.files, name)

  def test_options(self):
    # the 7-Zip page of the options: no submenu, icons, two items
    self.write_settings('[Software/7-Zip/Options]\n"CascadedMenu"=dword:00000000\n'
                        '"MenuIcons"=dword:00000001\n"ContextMenu"=dword:%08x\n' % ((1 << 1) | (1 << 9)))
    self.assertEqual(self.probe("--plugins", "b.zip"), [("Extract Here", True), ('Add to "b_2.7z"', True)])

  def test_commands(self):
    # the commands of CompressCall.cpp, with the names as -i switches
    args = self.run_action("7zG", '7-Zip > Extract to "*/"', "a.7z", "b.zip")
    self.assertEqual(args, ["x", "-o" + self.files + "/*/", "-spe", "-an",
                            "-aiw-!" + self.f("a.7z"), "-aiw-!" + self.f("b.zip")])
    oracle(*args)
    for name in ("a", "b"):
      with open(os.path.join(self.files, name, "c.txt")) as f:
        self.assertEqual(f.read(), "text\n")

    self.assertEqual(self.run_action("7zG", '7-Zip > Add to "files.zip"', "folder", "c.txt"),
                     ["a", "-iw-!" + self.f("folder"), "-iw-!" + self.f("c.txt"), "-tzip", "-sae", "--",
                      self.f("files.zip")])
    args = self.run_action("7zG", '7-Zip > Add to "c.7z"', "c.txt")
    self.assertEqual(args, ["a", "-iw-!" + self.f("c.txt"), "-t7z", "-sae", "--", self.f("c.7z")])
    oracle(*args)
    self.assertIn("Path = c.txt", oracle("l", "-slt", self.f("c.7z")).stdout)

    self.assertEqual(self.run_action("7zG", "7-Zip > Add to archive...", "c.txt"),
                     ["a", "-iw-!" + self.f("c.txt"), "-ad", "-saa", "--", self.f("c")])
    # 7zG makes the archive to send in a temporary folder
    self.assertEqual(self.run_action("7zG", '7-Zip > Compress to "c.zip" and email', "c.txt"),
                     ["a", "-iw-!" + self.f("c.txt"), "-tzip", "-seml.", "-sae", "--", "c.zip"])
    self.assertEqual(self.run_action("7zG", "7-Zip > Test archive", "a.7z"),
                     ["t", "-an", "-aiw-!" + self.f("a.7z")])
    self.assertEqual(self.run_action("7zG", "7-Zip > CRC SHA > SHA-256", "c.txt"),
                     ["h", "-scrcSHA256", "-iw-!" + self.f("c.txt")])
    self.assertEqual(self.run_action("7zG", "7-Zip > CRC SHA > SHA-256 -> c.txt.sha256", "c.txt"),
                     ["a", "-iw-!" + self.f("c.txt"), "-thash", "-sae", "--", self.f("c.txt.sha256")])
    # Open archive runs 7zFM (upstream names it 7zFM.exe)
    self.assertEqual(self.run_action("7zFM", "7-Zip > Open archive > zip", "b.zip"), [self.f("b.zip"), "-tzip"])

  def test_language(self):
    # the language of 7-Zip: the locale, with Lang/ in the program folder
    os.makedirs(os.path.join(self.bin, "Lang"))
    with open(os.path.join(self.bin, "Lang", "pl.txt"), "w", encoding="utf-8") as f:
      f.write(";!@Lang2@!UTF-8!\n\n0\n7-Zip\nPolish\nPolski\n"
              "2326\nWypakuj tutaj\nWypakuj do {0}\n")
    self.extra_env["LANG"] = "pl_PL.UTF-8"
    menu = self.menu("b.zip")
    self.assertIn("Wypakuj tutaj", menu)
    self.assertIn('Wypakuj do "b/"', menu)
    self.assertIn("Test archive", menu)


def main():
  global PROBE, PACKAGING, PLUGIN, ORACLE
  ap = argparse.ArgumentParser()
  ap.add_argument("--probe", required=True)
  ap.add_argument("--packaging", required=True)
  ap.add_argument("--plugin")
  ap.add_argument("--oracle", required=True)
  args, rest = ap.parse_known_args()
  PROBE = os.path.abspath(args.probe)
  PACKAGING = os.path.abspath(args.packaging)
  PLUGIN = os.path.abspath(args.plugin) if args.plugin else None
  ORACLE = os.path.abspath(args.oracle)
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
