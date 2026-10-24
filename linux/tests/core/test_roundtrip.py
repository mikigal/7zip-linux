#!/usr/bin/env python3
"""Round-trip tests between the CMake-built 7zz and the upstream-built oracle.

Archives created by one binary must test and extract correctly with the other,
and both must report the same set of formats and codecs. This verifies that the
CMake core library is equivalent to the upstream build.
"""

import argparse
import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

OURS = None
ORACLE = None


def run(exe, *args, cwd=None):
  res = subprocess.run([exe, *args], cwd=cwd, capture_output=True, text=True)
  if res.returncode != 0:
    raise AssertionError("%s %s failed (%d):\n%s\n%s" % (
        os.path.basename(exe), " ".join(args), res.returncode, res.stdout[-3000:], res.stderr[-3000:]))
  return res.stdout


def make_tree(root):
  os.makedirs(os.path.join(root, "dir", "nested", "deeper"))
  os.makedirs(os.path.join(root, "empty dir"))
  files = {
      "empty.txt": b"",
      "small.txt": b"hello 7-Zip\n",
      "dir/zażółć gęślą jaźń.txt": "Polish name\n".encode(),
      "dir/日本語.txt": "Japanese name\n".encode(),
      "dir/nested/compressible.bin": b"0123456789abcdef" * (128 * 1024),
      "dir/nested/deeper/random.bin": os.urandom(1024 * 1024 + 17),
  }
  for rel, data in files.items():
    with open(os.path.join(root, rel), "wb") as f:
      f.write(data)
  script = os.path.join(root, "dir", "run.sh")
  with open(script, "w") as f:
    f.write("#!/bin/sh\necho ok\n")
  os.chmod(script, 0o755)
  os.symlink("nested/compressible.bin", os.path.join(root, "dir", "link.bin"))


def snapshot(root, links=True, modes=True):
  result = {}
  for dirpath, dirnames, filenames in os.walk(root):
    for name in dirnames + filenames:
      path = os.path.join(dirpath, name)
      rel = os.path.relpath(path, root)
      st = os.lstat(path)
      if stat.S_ISLNK(st.st_mode) and links:
        result[rel] = ("link", os.readlink(path))
        continue
      if not links:
        # Formats without symlink support store the target's content.
        st = os.stat(path)
      if stat.S_ISDIR(st.st_mode):
        result[rel] = ("dir",)
      else:
        with open(path, "rb") as f:
          digest = hashlib.sha256(f.read()).hexdigest()
        exe = bool(st.st_mode & stat.S_IXUSR) if modes else None
        result[rel] = ("file", digest, exe)
  return result


class RoundTrip(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-roundtrip-")
    self.src = os.path.join(self.tmp, "src")
    make_tree(self.src)

  def tearDown(self):
    shutil.rmtree(self.tmp)

  def roundtrip(self, creator, reader, fmt, ext, extra=(), links=True, modes=True):
    arc = os.path.join(self.tmp, "%s-%s.%s" % (os.path.basename(os.path.dirname(creator)), fmt, ext))
    run(creator, "a", "-t" + fmt, *extra, arc, ".", cwd=self.src)
    run(reader, "t", *[e for e in extra if e.startswith("-p")], arc)
    out = os.path.join(self.tmp, "out-" + os.path.basename(arc))
    run(reader, "x", *[e for e in extra if e.startswith("-p")], "-snl", "-o" + out, arc)
    expected = snapshot(self.src, links, modes)
    actual = snapshot(out, links, modes)
    self.assertEqual(expected, actual)

  def both_ways(self, fmt, ext, extra=(), links=True, modes=True):
    self.roundtrip(OURS, ORACLE, fmt, ext, extra, links, modes)
    self.roundtrip(ORACLE, OURS, fmt, ext, extra, links, modes)

  def test_7z(self):
    self.both_ways("7z", "7z", ("-snl",))

  def test_7z_encrypted_headers(self):
    self.both_ways("7z", "7z", ("-snl", "-psecret", "-mhe=on"))

  def test_zip(self):
    self.both_ways("zip", "zip", ("-snl",))

  def test_zip_aes(self):
    self.both_ways("zip", "zip", ("-snl", "-psecret", "-mem=AES256"))

  def test_tar(self):
    self.both_ways("tar", "tar", ("-snl",))

  def test_wim(self):
    self.both_ways("wim", "wim", (), links=False, modes=False)

  def test_same_formats_and_codecs(self):
    def info(exe):
      lines = run(exe, "i").splitlines()
      # Skip the banner (version, build date, CPU) and keep the tables.
      start = next(i for i, l in enumerate(lines) if l.startswith("Formats:"))
      return lines[start:]
    self.assertEqual(info(ORACLE), info(OURS))


def main():
  global OURS, ORACLE
  ap = argparse.ArgumentParser()
  ap.add_argument("--ours", required=True)
  ap.add_argument("--oracle", required=True)
  args, rest = ap.parse_known_args()
  OURS = os.path.abspath(args.ours)
  ORACLE = os.path.abspath(args.oracle)
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
