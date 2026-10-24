#!/usr/bin/env python3
"""The 7-Zip context menu of GTK file managers (linux/gtk): their
extensions run 7z-context-menu, which prints the upstream menu with the
command of each item (linux/shell/ShellMenu.h).

- Tool: the records of 7z-context-menu;
- Core: the menu as the shared code of the extensions reads it
  (z7-gtk-menu-probe), and the commands of its items, which the oracle
  runs to check them;
- Nemo, Thunar, Nautilus: the menu items that each extension gives its
  file manager (z7-<name>-probe loads the module as the file manager does)
  and what activating one starts.

7zG and 7zFM are stand-ins that write their arguments."""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

TOOL = None
PROBE = None
ORACLE = None
EXTENSIONS = {}   # name: (probe, module)
APP_ID = "io.github.mikigal.SevenZip"


def oracle(*args, cwd=None):
  res = subprocess.run([ORACLE] + list(args), cwd=cwd, capture_output=True, text=True, timeout=60)
  if res.returncode != 0:
    raise AssertionError("7zz %s: %s%s" % (" ".join(args), res.stdout, res.stderr))
  return res


class GtkMenus(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-gtk-menus-")
    self.home = os.path.join(self.tmp, "home")
    self.config = os.path.join(self.home, "config")
    os.makedirs(self.config)
    self.files = os.path.join(self.tmp, "files")
    os.makedirs(os.path.join(self.files, "folder"))
    with open(os.path.join(self.files, "c.txt"), "w") as f:
      f.write("text\n")
    oracle("a", "-bso0", "a.7z", "c.txt", cwd=self.files)
    oracle("a", "-bso0", "-tzip", "b.zip", "c.txt", cwd=self.files)
    # 7zG and 7zFM write their arguments, one per line
    self.bin = os.path.join(self.tmp, "bin")
    os.makedirs(self.bin)
    for name in ("7zG", "7zFM"):
      path = os.path.join(self.bin, name)
      with open(path, "w") as f:
        f.write('#!/bin/sh\nfor a in "$@"; do printf "%s\\n" "$a"; done > "$0.tmp" && mv "$0.tmp" "$0.log"\n')
      os.chmod(path, 0o755)
    self.extra_env = {}

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def f(self, name):
    return os.path.join(self.files, name)

  def env(self):
    env = {
        "PATH": "/usr/bin:/bin",
        "HOME": self.home,
        "XDG_CONFIG_HOME": self.config,
        "XDG_DATA_HOME": os.path.join(self.home, "data"),
        "XDG_CACHE_HOME": os.path.join(self.home, "cache"),
        "XDG_RUNTIME_DIR": self.home,
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=" + os.path.join(self.home, "no-bus"),
        "Z7_PROGRAM_DIR": self.bin,
        "Z7_CONTEXT_MENU_TOOL": TOOL,
    }
    # the sanitizer options of the ASan build (check.sh --asan)
    for name in ("ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"):
      if name in os.environ:
        env[name] = os.environ[name]
    env.update(self.extra_env)
    return env

  def write_settings(self, text):
    os.makedirs(os.path.join(self.config, "7-Zip"), exist_ok=True)
    with open(os.path.join(self.config, "7-Zip", "7-Zip.ini"), "w") as f:
      f.write(text)

  def wait_log(self, program):
    log = os.path.join(self.bin, program + ".log")
    for _ in range(100):
      if os.path.exists(log):
        break
      time.sleep(0.1)
    with open(log) as f:
      args = f.read().splitlines()
    os.remove(log)
    return args


class Tool(GtkMenus):

  def records(self, *names):
    res = subprocess.run([TOOL, "--"] + [self.f(n) for n in names], env=self.env(),
                         capture_output=True, timeout=30)
    self.assertEqual(res.returncode, 0, res.stderr)
    self.assertTrue(res.stdout.endswith(b"\0"))
    return [s.decode() for s in res.stdout[:-1].split(b"\0")]

  def test_records(self):
    fields = self.records("b.zip")
    self.assertEqual(fields[:5], ["7-Zip menu 1", "menu", "0", "7-Zip", "0"])
    # "item" DEPTH TEXT ICON ARGC ARG...
    i = fields.index("Extract Here")
    self.assertEqual(fields[i - 2:i + 3], ["item", "1", "Extract Here", "0", "5"])
    self.assertEqual(fields[i + 3:i + 8], [os.path.join(self.bin, "7zG"), "x", "-o" + self.files + "/",
                                           "-an", "-aiw-!" + self.f("b.zip")])
    self.assertIn("separator", fields)
    # nothing for files that are not there
    self.assertEqual(self.records("none"), ["7-Zip menu 1"])


class Core(GtkMenus):

  def probe(self, *args):
    res = subprocess.run([PROBE] + list(args), env=self.env(), capture_output=True, text=True,
                         timeout=30)
    self.assertEqual(res.returncode, 0, res.stderr)
    return [(line.split("\t")[0], line.endswith("\ticon")) for line in res.stdout.splitlines()]

  def menu(self, *names):
    items = [p for p, _ in self.probe(*[self.f(n) for n in names])]
    self.assertEqual(items[0], "7-Zip >")
    return [p[len("7-Zip > "):] for p in items[1:] if " > " not in p[len("7-Zip > "):]]

  def test_archive(self):
    # the menu of the Windows shell extension, in its order
    self.assertEqual(self.menu("b.zip"), [
        "Open archive", "Open archive >", "Extract files...", "Extract Here", 'Extract to "b/"',
        "Test archive", "Add to archive...", "Compress and email...",
        'Add to "b_2.7z"', 'Compress to "b_2.7z" and email',
        'Add to "b_2.zip"', 'Compress to "b_2.zip" and email', "CRC SHA >"])
    self.assertIn('Extract to "*/"', self.menu("a.7z", "b.zip"))
    self.assertNotIn("Extract Here", self.menu("c.txt"))
    self.assertEqual(self.probe("/proc/no-such-file"), [])

  def test_options(self):
    self.write_settings('[Software/7-Zip/Options]\n"CascadedMenu"=dword:00000000\n'
                        '"MenuIcons"=dword:00000001\n"ContextMenu"=dword:%08x\n' % ((1 << 1) | (1 << 9)))
    self.assertEqual(self.probe(self.f("b.zip")), [("Extract Here", True), ('Add to "b_2.7z"', True)])

  def test_commands(self):
    self.probe("--trigger", '7-Zip > Extract to "*/"', self.f("a.7z"), self.f("b.zip"))
    args = self.wait_log("7zG")
    self.assertEqual(args, ["x", "-o" + self.files + "/*/", "-spe", "-an",
                            "-aiw-!" + self.f("a.7z"), "-aiw-!" + self.f("b.zip")])
    oracle(*args)
    for name in ("a", "b"):
      with open(os.path.join(self.files, name, "c.txt")) as f:
        self.assertEqual(f.read(), "text\n")
    self.probe("--trigger", '7-Zip > Add to "c.7z"', self.f("c.txt"))
    args = self.wait_log("7zG")
    self.assertEqual(args, ["a", "-iw-!" + self.f("c.txt"), "-t7z", "-sae", "--", self.f("c.7z")])
    oracle(*args)
    self.assertIn("Path = c.txt", oracle("l", "-slt", self.f("c.7z")).stdout)
    self.probe("--trigger", "7-Zip > Open archive > zip", self.f("b.zip"))
    self.assertEqual(self.wait_log("7zFM"), [self.f("b.zip"), "-tzip"])


class Extension:
  """The menu of an extension; the file managers differ in separators
  (only Nemo has them) and icons (Nautilus 4 shows none). The labels are
  GTK mnemonic texts: "_" is doubled."""
  FM = None
  SEPARATORS = False
  ICONS = False

  def setUp(self):
    if self.FM not in EXTENSIONS:
      self.skipTest("the extension is not built")
    super().setUp()
    self.fm_probe, self.module = EXTENSIONS[self.FM]

  def env(self):
    env = super().env()
    # the system libraries have no frame pointers: the slow unwinder finds
    # their frames, which lsan.supp names
    env["ASAN_OPTIONS"] = ":".join(filter(None, [env.get("ASAN_OPTIONS"), "fast_unwind_on_malloc=0"]))
    return env

  def probe(self, *args):
    res = subprocess.run([self.fm_probe, self.module] + list(args), env=self.env(),
                         capture_output=True, text=True, timeout=30)
    self.assertEqual(res.returncode, 0, res.stderr)
    self.assertEqual(res.stderr, "")
    return [(line.split("\t")[0], line.split("\t")[2] if line.count("\t") == 2 else "")
            for line in res.stdout.splitlines()]

  def test_menu(self):
    items = [p for p, _ in self.probe(self.f("b.zip"))]
    self.assertEqual(items[0], "7-Zip >")
    direct = [p[len("7-Zip > "):] for p in items[1:] if " > " not in p[len("7-Zip > "):]]
    self.assertEqual(direct, [
        "Open archive", "Open archive >", "Extract files...", "Extract Here", 'Extract to "b/"',
        "Test archive", "Add to archive...", "Compress and email...",
        'Add to "b__2.7z"', 'Compress to "b__2.7z" and email',
        'Add to "b__2.zip"', 'Compress to "b__2.zip" and email', "CRC SHA >"])
    self.assertIn("7-Zip > CRC SHA > SHA-256", items)
    separators = [p for p in items if p == "-" or p.endswith(" > -")]
    self.assertEqual(separators, ["7-Zip > CRC SHA > -"] if self.SEPARATORS else [])

  def test_options(self):
    self.write_settings('[Software/7-Zip/Options]\n"CascadedMenu"=dword:00000000\n'
                        '"MenuIcons"=dword:00000001\n"ContextMenu"=dword:%08x\n' % ((1 << 1) | (1 << 9)))
    icon = APP_ID if self.ICONS else ""
    self.assertEqual(self.probe(self.f("b.zip")), [("Extract Here", icon), ('Add to "b__2.7z"', icon)])

  def test_commands(self):
    self.probe("--trigger", "7-Zip > Extract Here", self.f("b.zip"))
    self.assertEqual(self.wait_log("7zG"), ["x", "-o" + self.files + "/", "-an", "-aiw-!" + self.f("b.zip")])
    self.probe("--trigger", "7-Zip > Open archive > zip", self.f("b.zip"))
    self.assertEqual(self.wait_log("7zFM"), [self.f("b.zip"), "-tzip"])

  def test_other_files(self):
    menu = [p for p, _ in self.probe(self.f("folder"), self.f("c.txt"))]
    self.assertIn('7-Zip > Add to "files.7z"', menu)
    self.assertNotIn("7-Zip > Extract Here", menu)
    # 7-Zip works on paths of the file system
    self.assertEqual(self.probe("https://example.com/a.7z"), [])


class Nemo(Extension, GtkMenus):
  FM = "nemo"
  SEPARATORS = True
  ICONS = True


class Thunar(Extension, GtkMenus):
  FM = "thunar"
  ICONS = True


class Nautilus(Extension, GtkMenus):
  FM = "nautilus"


def main():
  global TOOL, PROBE, ORACLE
  ap = argparse.ArgumentParser()
  ap.add_argument("--tool", required=True)
  ap.add_argument("--probe", required=True)
  ap.add_argument("--oracle", required=True)
  for fm in ("nemo", "thunar", "nautilus"):
    ap.add_argument("--" + fm, nargs=2, metavar=("PROBE", "MODULE"))
  args, rest = ap.parse_known_args()
  TOOL = os.path.abspath(args.tool)
  PROBE = os.path.abspath(args.probe)
  ORACLE = os.path.abspath(args.oracle)
  for fm in ("nemo", "thunar", "nautilus"):
    if getattr(args, fm):
      EXTENSIONS[fm] = tuple(os.path.abspath(p) for p in getattr(args, fm))
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
