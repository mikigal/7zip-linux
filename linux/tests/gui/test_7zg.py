#!/usr/bin/env python3
"""End-to-end tests of 7zG: the commands a, x, t, h and b through their
dialogs, answered by z7-gui-driver (7zG with scripted dialogs), with the
archives checked by the 7zz oracle.

Dialog and control IDs are named as in the upstream resource headers; the
names come from rcgen.py, which reads them from UI/GUI/resource.rc.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))
import rcgen  # noqa: E402

DRIVER = None
ORACLE = None
ARTIFACTS = None
IDS = {}

# exit codes of 7-Zip (UI/Common/ExitCode.h)
K_OK = 0
K_FATAL = 2
K_USER_BREAK = 255


def ID(name):
  return IDS[name]


def make_tree(root):
  os.makedirs(os.path.join(root, "dir", "nested"))
  files = {
      "small.txt": b"hello 7-Zip\n",
      "dir/zażółć gęślą jaźń.txt": "Polish name\n".encode(),
      "dir/nested/compressible.bin": b"0123456789abcdef" * (64 * 1024),
      "dir/nested/random.bin": os.urandom(300 * 1024 + 7),
  }
  for rel, data in files.items():
    with open(os.path.join(root, rel), "wb") as f:
      f.write(data)


def rar5_vint(n):
  out = bytearray()
  while True:
    b = n & 0x7f
    n >>= 7
    out.append(b | (0x80 if n else 0))
    if not n:
      return bytes(out)


def rar5_header(body):
  data = rar5_vint(len(body)) + body
  return struct.pack("<I", zlib.crc32(data)) + data


def make_rar5_big_dictionary(path):
  """A RAR5 archive with one stored file whose header asks for a 2 GB
  dictionary: extraction with a lower memory limit asks the user."""
  content = b"hello rar\n"
  name = b"big-dict.txt"
  comp = 14 << 10   # algorithm 0, method 0 (store), dictionary 128 KB << 14
  file_header = (rar5_vint(2) + rar5_vint(0x0002) + rar5_vint(len(content))
                 + rar5_vint(0x0004) + rar5_vint(len(content)) + rar5_vint(0x20)
                 + struct.pack("<I", zlib.crc32(content))
                 + rar5_vint(comp) + rar5_vint(1) + rar5_vint(len(name)) + name)
  with open(path, "wb") as f:
    f.write(b"Rar!\x1a\x07\x01\x00")
    f.write(rar5_header(rar5_vint(1) + rar5_vint(0) + rar5_vint(0)))
    f.write(rar5_header(file_header) + content)
    f.write(rar5_header(rar5_vint(5) + rar5_vint(0) + rar5_vint(0)))


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


def oracle(*args, check=True, cwd=None):
  res = subprocess.run([ORACLE, *args], capture_output=True, text=True, cwd=cwd)
  if check and res.returncode != 0:
    raise AssertionError("7zz %s failed (%d):\n%s\n%s" % (
        " ".join(args), res.returncode, res.stdout[-3000:], res.stderr[-3000:]))
  return res


class Run:
  """One run of z7-gui-driver: exit code and the events of its log."""

  def __init__(self, code, events, stderr):
    self.code = code
    self.events = events
    self.stderr = stderr

  def of(self, kind, name=None):
    return [e for e in self.events
            if e["event"] == kind and (name is None or e.get("name") == name)]

  def errors(self):
    return [e["message"] for e in self.of("error")]


def control(dialog_event, id):
  for c in dialog_event["controls"]:
    if c["id"] == id:
      return c
  raise AssertionError("no control %d in %s" % (id, dialog_event["name"]))


class GuiTest(unittest.TestCase):

  def setUp(self):
    self.tmp = tempfile.mkdtemp(prefix="z7-7zg-")
    self.src = os.path.join(self.tmp, "src")
    make_tree(self.src)
    self.home = os.path.join(self.tmp, "home")
    os.makedirs(self.home)
    self.shots = os.path.join(ARTIFACTS, self.id().split(".")[-1])
    shutil.rmtree(self.shots, ignore_errors=True)
    os.makedirs(self.shots)

  def tearDown(self):
    shutil.rmtree(self.tmp, ignore_errors=True)

  def run_7zg(self, script, *args, cwd=None, timeout=60, bin_dir=None):
    script_path = os.path.join(self.tmp, "script.json")
    log_path = os.path.join(self.shots, "log.jsonl")
    with open(script_path, "w") as f:
      json.dump(script, f, indent=1)
    shutil.copy(script_path, os.path.join(self.shots, "script.json"))
    env = {
        # (bin_dir) holds stand-ins of desktop programs (xdg-email)
        "PATH": (bin_dir + ":" if bin_dir else "") + "/usr/bin:/bin",
        "HOME": self.home,
        "XDG_CONFIG_HOME": os.path.join(self.home, "config"),
        "XDG_DATA_HOME": os.path.join(self.home, "data"),
        "XDG_CACHE_HOME": os.path.join(self.home, "cache"),
        "XDG_STATE_HOME": os.path.join(self.home, "state"),
        "XDG_RUNTIME_DIR": self.home,
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
        cwd=cwd or self.tmp, env=env, capture_output=True, text=True, timeout=timeout + 30)
    with open(os.path.join(self.shots, "stderr.txt"), "w") as f:
      f.write(res.stderr)
    events = []
    if os.path.exists(log_path):
      with open(log_path) as f:
        events = [json.loads(line) for line in f if line.strip()]
    return Run(res.returncode, events, res.stderr)

  def assertRun(self, run, code=K_OK):
    self.assertEqual(run.errors(), [], run.stderr)
    self.assertEqual(run.code, code, run.stderr)

  def make_archive(self, name, *switches):
    path = os.path.join(self.tmp, name)
    oracle("a", "-bso0", *switches, path, ".", cwd=self.src)
    return path

  # ---------- Add ----------

  def test_add_with_dialog(self):
    archive = os.path.join(self.tmp, "out.zip")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            {"screenshot": "compress.png"},
            {"select": [ID("IDC_COMPRESS_FORMAT"), "zip"]},
            {"expect": [ID("IDC_COMPRESS_ARCHIVE"), r"\.zip$"]},
            {"text": [ID("IDC_COMPRESS_ARCHIVE"), archive]},
            {"click": ID("IDOK")}]},
    ], "a", "-ad", os.path.join(self.tmp, "out.7z"), "small.txt", "dir", cwd=self.src)
    self.assertRun(run)
    self.assertTrue(run.of("dialog", "IDD_PROGRESS"))
    listing = oracle("l", "-slt", archive).stdout
    self.assertIn("Type = zip", listing)
    oracle("t", archive)
    out = os.path.join(self.tmp, "out")
    oracle("x", "-bso0", archive, "-o" + out)
    self.assertEqual(snapshot(out), snapshot(self.src))

  def test_add_browse(self):
    chosen = os.path.join(self.tmp, "chosen name.7z")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            {"click": ID("IDB_COMPRESS_SET_ARCHIVE")},
            # the folder is shown above the name
            {"expect": [ID("IDC_COMPRESS_ARCHIVE"), r"^chosen name\.7z$"]},
            {"expect": [ID("IDT_COMPRESS_ARCHIVE_FOLDER"), "^" + re.escape(self.tmp) + "/$"]},
            {"click": ID("IDOK")}]},
        {"on": "filedialog", "answer": chosen},
    ], "a", "-ad", os.path.join(self.tmp, "default.7z"), "small.txt", cwd=self.src)
    self.assertRun(run)
    request = run.of("filedialog")[0]
    self.assertEqual(request["kind"], 2)   # save
    self.assertTrue(any(f.startswith("7z ") and "*.7z" in f for f in request["filters"]),
                    request["filters"])
    oracle("t", chosen)

  def test_add_encrypted(self):
    archive = os.path.join(self.tmp, "secret.7z")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            {"text": [ID("IDE_COMPRESS_PASSWORD1"), "pässwörd"]},
            {"text": [ID("IDE_COMPRESS_PASSWORD2"), "pässwörd"]},
            {"check": [ID("IDX_COMPRESS_ENCRYPT_FILE_NAMES"), True]},
            {"screenshot": "compress.png"},
            {"click": ID("IDOK")}]},
    ], "a", "-ad", archive, "small.txt", "dir", cwd=self.src)
    self.assertRun(run)
    # the names are encrypted: no listing without the password
    self.assertNotEqual(oracle("l", "-p", archive, check=False).returncode, 0)
    oracle("t", "-ppässwörd", archive)
    out = os.path.join(self.tmp, "out")
    oracle("x", "-bso0", "-ppässwörd", archive, "-o" + out)
    self.assertEqual(snapshot(out), snapshot(self.src))

  def test_add_options_symlinks(self):
    os.symlink("dir/nested/compressible.bin", os.path.join(self.src, "link.bin"))
    # 7z has no link options (its handler does not report kSymLinks); tar does
    archive = os.path.join(self.tmp, "links.tar")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            {"click": ID("IDB_COMPRESS_OPTIONS")},
            {"click": ID("IDOK")}]},
        {"on": "dialog", "name": "IDD_COMPRESS_OPTIONS", "actions": [
            {"screenshot": "options.png"},
            {"check": [ID("IDX_COMPRESS_NT_SYM_LINKS"), True]},
            {"click": ID("IDOK")}]},
    ], "a", "-ad", "-ttar", archive, "small.txt", "link.bin", cwd=self.src)
    self.assertRun(run)
    listing = oracle("l", "-slt", archive).stdout
    self.assertIn("Symbolic Link = dir/nested/compressible.bin", listing)

  def test_add_password_mismatch(self):
    archive = os.path.join(self.tmp, "secret.7z")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_COMPRESS", "actions": [
            {"text": [ID("IDE_COMPRESS_PASSWORD1"), "one"]},
            {"text": [ID("IDE_COMPRESS_PASSWORD2"), "two"]},
            {"click": ID("IDOK")},
            {"click": ID("IDCANCEL")}]},
        {"on": "messagebox", "contains": "do not match", "answer": ID("IDOK")},
    ], "a", "-ad", archive, "small.txt", cwd=self.src)
    self.assertRun(run, K_USER_BREAK)
    self.assertFalse(os.path.exists(archive))

  def test_add_without_dialog(self):
    archive = os.path.join(self.tmp, "plain.tar")
    run = self.run_7zg([], "a", "-ttar", archive, ".", cwd=self.src)
    self.assertRun(run)
    self.assertIn("Type = tar", oracle("l", "-slt", archive).stdout)
    out = os.path.join(self.tmp, "out")
    oracle("x", "-bso0", archive, "-o" + out)
    self.assertEqual(snapshot(out), snapshot(self.src))

  def test_add_and_email(self):
    # the mail program of the desktop gets the archive through xdg-email
    bin_dir = os.path.join(self.tmp, "bin")
    os.makedirs(bin_dir)
    record = os.path.join(self.tmp, "xdg-email.args")
    with open(os.path.join(bin_dir, "xdg-email"), "w") as f:
      f.write('#!/bin/sh\nprintf "%%s\\n" "$@" > "%s"\n' % record)
    os.chmod(os.path.join(bin_dir, "xdg-email"), 0o755)
    archive = os.path.join(self.tmp, "mail.7z")
    run = self.run_7zg([], "a", "-semlsomeone@example.com", archive, "small.txt",
                       cwd=self.src, bin_dir=bin_dir)
    self.assertRun(run)
    with open(record) as f:
      self.assertEqual(f.read().splitlines(), ["--attach", archive, "someone@example.com"])
    oracle("t", archive)

  def test_add_cancel(self):
    big = os.path.join(self.src, "big.bin")
    with open(big, "wb") as f:
      for _ in range(64):
        f.write(os.urandom(1 << 20))
    archive = os.path.join(self.tmp, "big.7z")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_PROGRESS", "actions": [
            {"click": ID("IDCANCEL")}]},
        {"on": "messagebox", "contains": "", "answer": ID("IDYES")},
    ], "a", "-mx9", "-mmt1", archive, "big.bin", cwd=self.src)
    self.assertRun(run, K_USER_BREAK)
    self.assertFalse(os.path.exists(archive))

  def test_add_background(self):
    big = os.path.join(self.src, "big.bin")
    with open(big, "wb") as f:
      for _ in range(64):
        f.write(os.urandom(1 << 20))
    archive = os.path.join(self.tmp, "big.7z")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_PROGRESS", "actions": [
            {"click": ID("IDB_PROGRESS_BACKGROUND")},
            {"expect": [ID("IDB_PROGRESS_BACKGROUND"), "Foreground"]},
            {"click": ID("IDCANCEL")}]},
        {"on": "messagebox", "contains": "", "answer": ID("IDYES")},
    ], "a", "-mx9", "-mmt1", archive, "big.bin", cwd=self.src)
    self.assertRun(run, K_USER_BREAK)
    closed = run.of("dialog_closed", "IDD_PROGRESS")[-1]
    self.assertIn("Background", closed["caption"])

  # ---------- Extract ----------

  def test_extract_with_dialog(self):
    archive = self.make_archive("a.7z")
    out = os.path.join(self.tmp, "out")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_EXTRACT", "actions": [
            {"screenshot": "extract.png"},
            # the last part of -o is in the name field, added to the path
            {"expect": [ID("IDE_EXTRACT_NAME"), "^unused/$"]},
            {"check": [ID("IDX_EXTRACT_NAME_ENABLE"), False]},
            {"text": [ID("IDC_EXTRACT_PATH"), out]},
            {"click": ID("IDOK")}]},
    ], "x", "-ad", archive, "-o" + os.path.join(self.tmp, "unused"))
    self.assertRun(run)
    self.assertEqual(snapshot(out), snapshot(self.src))

  def test_extract_browse_and_help(self):
    archive = self.make_archive("a.7z")
    out = os.path.join(self.tmp, "chosen")
    os.makedirs(out)
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_EXTRACT", "actions": [
            {"click": ID("IDHELP")},
            {"click": ID("IDB_EXTRACT_SET_PATH")},
            {"expect": [ID("IDC_EXTRACT_PATH"), "^" + re.escape(out) + "/?$"]},
            {"check": [ID("IDX_EXTRACT_NAME_ENABLE"), False]},
            {"click": ID("IDOK")}]},
        {"on": "filedialog", "answer": out},
    ], "x", "-ad", archive)
    self.assertRun(run)
    self.assertEqual(snapshot(out), snapshot(self.src))
    self.assertEqual([e["url"] for e in run.of("open_url")],
                     ["https://7-zip.opensource.jp/chm/fm/plugins/7-zip/extract.htm"])
    self.assertEqual(run.of("filedialog")[0]["kind"], 0)   # folder

  def test_extract_password(self):
    archive = self.make_archive("enc.7z", "-psecret", "-mhe=on")
    out = os.path.join(self.tmp, "out")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_PASSWORD", "actions": [
            {"screenshot": "password.png"},
            {"text": [ID("IDE_PASSWORD_PASSWORD"), "secret"]},
            {"click": ID("IDOK")}]},
    ], "x", archive, "-o" + out)
    self.assertRun(run)
    self.assertEqual(snapshot(out), snapshot(self.src))

  def test_extract_wrong_password(self):
    archive = self.make_archive("enc.7z", "-psecret")
    out = os.path.join(self.tmp, "out")
    # the progress dialog comes first; it waits while the password is asked
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_PROGRESS", "actions": [
            {"wait": [ID("IDCANCEL"), "Close"]},
            {"screenshot": "errors.png"},
            {"click": ID("IDCANCEL")}]},
        {"on": "dialog", "name": "IDD_PASSWORD", "actions": [
            {"text": [ID("IDE_PASSWORD_PASSWORD"), "wrong"]},
            {"click": ID("IDOK")}]},
    ], "x", archive, "-o" + out)
    self.assertRun(run, K_FATAL)
    closed = run.of("dialog_closed", "IDD_PROGRESS")[-1]
    messages = control(closed, ID("IDL_PROGRESS_MESSAGES"))["rows"]
    self.assertTrue(any("Wrong password" in " ".join(r) for r in messages), messages)

  def test_extract_overwrite(self):
    archive = self.make_archive("a.7z")
    out = os.path.join(self.tmp, "out")
    os.makedirs(out)
    with open(os.path.join(out, "small.txt"), "wb") as f:
      f.write(b"old content\n")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_OVERWRITE", "actions": [
            {"screenshot": "overwrite.png"},
            {"expect": [ID("IDT_OVERWRITE_OLD_FILE_SIZE_TIME"), r"12 bytes"]},
            {"click": ID("IDB_AUTO_RENAME")}]},
    ], "x", archive, "-o" + out)
    self.assertRun(run)
    with open(os.path.join(out, "small.txt"), "rb") as f:
      self.assertEqual(f.read(), b"old content\n")
    with open(os.path.join(out, "small_1.txt"), "rb") as f:
      self.assertEqual(f.read(), b"hello 7-Zip\n")

  def test_extract_memory_limit(self):
    archive = os.path.join(self.tmp, "big-dict.rar")
    make_rar5_big_dictionary(archive)
    self.assertIn("2G", oracle("l", "-slt", archive).stdout)
    out = os.path.join(self.tmp, "out")
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_MEM", "actions": [
            {"screenshot": "memory.png"},
            {"click": ID("IDCANCEL")}]},
    ], "x", "-smemx1g", archive, "-o" + out)
    self.assertRun(run, K_USER_BREAK)
    shown = run.of("dialog", "IDD_MEM")[0]
    texts = " ".join(c["text"] for c in shown["controls"])
    self.assertIn("2 GB", texts)

  # ---------- Test ----------

  def test_test(self):
    archive = self.make_archive("a.7z")
    run = self.run_7zg([
        {"on": "messagebox", "contains": "There are no errors", "answer": ID("IDOK")},
    ], "t", archive)
    self.assertRun(run)
    text = run.of("messagebox")[0]["text"]
    self.assertIn("Files: 4", text)

  def test_test_corrupt(self):
    archive = self.make_archive("a.7z", "-mx1")
    with open(archive, "r+b") as f:
      f.seek(64)
      f.write(b"\xff" * 64)
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_PROGRESS", "actions": [
            {"wait": [ID("IDCANCEL"), "Close"]},
            {"screenshot": "errors.png"},
            {"click": ID("IDCANCEL")}]},
    ], "t", archive)
    self.assertRun(run, K_FATAL)
    closed = run.of("dialog_closed", "IDD_PROGRESS")[-1]
    self.assertTrue(control(closed, ID("IDL_PROGRESS_MESSAGES"))["rows"])

  # ---------- Hash ----------

  def test_hash(self):
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_LISTVIEW", "actions": [
            {"screenshot": "hash.png"},
            {"click": ID("IDOK")}]},
    ], "h", "-scrcSHA256", "small.txt", "dir", cwd=self.src)
    self.assertRun(run)
    rows = control(run.of("dialog", "IDD_LISTVIEW")[0], ID("IDL_LISTVIEW"))["rows"]
    got = {r[0]: r[1] for r in rows}
    expected = oracle("h", "-scrcSHA256", "small.txt", "dir", cwd=self.src).stdout
    m = re.search(r"SHA256 for data:\s+(\S+)", expected)
    self.assertIsNotNone(m, expected)
    self.assertEqual(got.get("SHA256 checksum for data"), m.group(1), rows)

  # ---------- Benchmark ----------

  def test_benchmark(self):
    run = self.run_7zg([
        {"on": "dialog", "name": "IDD_BENCH", "actions": [
            {"select": [ID("IDC_BENCH_DICTIONARY"), "2 MB"]},
            {"select": [ID("IDC_BENCH_NUM_PASSES"), "1"]},
            {"wait": [ID("IDT_BENCH_TOTAL_RATING_VAL"), r"[0-9.]+ [GM]IPS"]},
            {"screenshot": "bench.png"},
            {"click": ID("IDCANCEL")}]},
    ], "b", timeout=120)
    self.assertRun(run)
    closed = run.of("dialog_closed", "IDD_BENCH")[-1]
    self.assertRegex(control(closed, ID("IDT_BENCH_VER"))["text"], r"^7-Zip ")

  # ---------- Errors ----------

  def test_no_command(self):
    run = self.run_7zg([
        {"on": "messagebox", "contains": "Specify command", "answer": ID("IDOK")},
    ])
    self.assertRun(run)


def main():
  global DRIVER, ORACLE, ARTIFACTS, IDS
  parser = argparse.ArgumentParser()
  parser.add_argument("--driver", required=True)
  parser.add_argument("--oracle", required=True)
  parser.add_argument("--artifacts", required=True)
  args, rest = parser.parse_known_args()
  DRIVER = os.path.abspath(args.driver)
  ORACLE = os.path.abspath(args.oracle)
  ARTIFACTS = os.path.abspath(args.artifacts)
  os.makedirs(ARTIFACTS, exist_ok=True)
  IDS = rcgen.parse_file(os.path.join(ROOT, "CPP/7zip/UI/GUI/resource.rc")).symbols
  unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
  main()
