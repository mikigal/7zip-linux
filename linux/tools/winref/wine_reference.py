#!/usr/bin/env python3
"""Reference pictures of the 7-Zip dialogs from the Windows release, rendered
by the Windows dialog manager of Wine, next to the gallery of the Linux port.

The official release is not part of the repository: download it (for
example 7z2605-x64.exe for 26.05, the version of the merged upstream release)
and extract it with 7zz into a directory of its own. Then:

  wine_reference.py --release DIR --helper build/linux/tools/winref/winref.exe \\
      --out OUTDIR [--gallery GALLERYDIR] [--templates]

- winref (a Winelib program, see winref.c) creates every dialog template of
  7zFM.exe with the dialog manager of Wine and writes its client area, and
  the options property sheet. Pictures are named like those of z7-gallery
  (IDD_COMPRESS.png, PROPERTY_SHEET_OPTIONS.png, ...).
- --gallery puts each reference picture next to the gallery picture of the
  same name (OUTDIR/compare/).
- --templates compares the compiled dialog templates in 7zFM.exe and 7zG.exe
  with the tables of rcgen.py, control by control.

Wine runs in a nested headless KWin (wlsession.py) with its own prefix,
HOME and D-Bus, and its Wayland driver; nothing touches the user's session
or Wine prefix. Wine draws the classic Windows look with its Tahoma at
96 dpi, so the pictures show the original proportions in dialog units.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))
import rcgen  # noqa: E402
import wlsession  # noqa: E402

# The scripts of the programs in the release: its 7zFM.exe is built from
# UI/FileManager and runs 7zG.exe to add and extract; the Linux 7zFM uses
# Bundles/Fm, which has the dialogs of both.
FM_SCRIPT = "CPP/7zip/UI/FileManager/resource.rc"
GUI_SCRIPT = "CPP/7zip/UI/GUI/resource.rc"
BUNDLE_SCRIPT = "CPP/7zip/Bundles/Fm/resource.rc"
# the pages of the options sheet, as in z7-gallery
SHEET_PAGES = [2500, 2101, 2400, 2103, 2300, 2200]


def dos_path(path):
  return "Z:" + os.path.abspath(path).replace("/", "\\")


def render(release, helper, raw, artifacts):
  """Renders the templates of 7zFM.exe into raw/fm and of 7zG.exe into
  raw/gui, and the options sheet of 7zFM.exe."""
  with wlsession.Session(1600, 1000, artifacts=artifacts) as s:
    env = {
        "WINEPREFIX": os.path.join(s.root, "wineprefix"),
        "WINEARCH": "win64",
        "WINEDEBUG": "-all",
        # no Mono or Gecko installation prompts
        "WINEDLLOVERRIDES": "mscoree,mshtml=",
    }
    s.run(["wineboot", "-i"], timeout=300, env=env)
    s.run(["wine", "reg", "add", "HKCU\\Software\\Wine\\Drivers", "/v", "Graphics",
           "/d", "wayland", "/f"], env=env)
    s.run(["wineserver", "-w"], timeout=120, env=env)
    results = []
    for exe, sub, extra in (("7zFM.exe", "fm", ["--sheet", ",".join(str(i) for i in SHEET_PAGES)]),
                            ("7zG.exe", "gui", [])):
      os.makedirs(os.path.join(raw, sub))
      results.append(s.run([helper, dos_path(os.path.join(release, exe)),
                            dos_path(os.path.join(raw, sub))] + extra,
                           timeout=600, env=env, check=False))
    s.run(["wineserver", "-k"], timeout=60, env=env, check=False)
  for res in results:
    if res.returncode != 0:
      print(res.stdout + res.stderr)
      sys.exit("winref failed with exit code %d" % res.returncode)


def convert(raw, out):
  """PNG files named like the gallery ones; a template in both programs is
  taken from 7zFM.exe."""
  bundle = rcgen.parse_file(os.path.join(ROOT, BUNDLE_SCRIPT))
  names = []
  for sub in ("fm", "gui"):
    for f in sorted(os.listdir(os.path.join(raw, sub))):
      if not f.endswith(".bmp"):
        continue
      stem = f[:-4]
      if stem == "sheet":
        name = "PROPERTY_SHEET_OPTIONS"
      else:
        d = bundle.dialogs.get(int(stem))
        name = d.name if d and d.name else "DIALOG_%s" % stem
      if name in names:
        continue
      subprocess.run(["magick", os.path.join(raw, sub, f), os.path.join(out, name + ".png")], check=True)
      names.append(name)
  return sorted(names)


def compare(names, out, gallery):
  dest = os.path.join(out, "compare")
  os.makedirs(dest, exist_ok=True)
  for name in names:
    ours = os.path.join(gallery, name + ".png")
    if not os.path.exists(ours):
      continue
    subprocess.run(["magick", "montage", "-label", "Windows (Wine): " + name,
                    os.path.join(out, name + ".png"), "-label", "Linux port", ours,
                    "-tile", "2x1", "-geometry", "+8+8", "-background", "#606060",
                    "-fill", "white", "-pointsize", "14",
                    os.path.join(dest, name + ".png")], check=True)
  print("comparisons in %s" % dest)


# ---------- Compiled templates ----------

def pe_dialogs(path):
  """The RT_DIALOG resources of a PE file: {id: bytes}."""
  with open(path, "rb") as f:
    data = f.read()
  pe = struct.unpack_from("<I", data, 0x3C)[0]
  if data[pe:pe + 4] != b"PE\0\0":
    raise ValueError("%s is not a PE file" % path)
  nsec = struct.unpack_from("<H", data, pe + 6)[0]
  opt_size = struct.unpack_from("<H", data, pe + 20)[0]
  opt = pe + 24
  magic = struct.unpack_from("<H", data, opt)[0]
  dirs = opt + (112 if magic == 0x20B else 96)
  rsrc_rva = struct.unpack_from("<I", data, dirs + 2 * 8)[0]
  sections = []
  for i in range(nsec):
    s = opt + opt_size + 40 * i
    vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, s + 8)
    sections.append((va, max(vsize, rawsize), rawptr))

  def off(rva):
    for va, size, rawptr in sections:
      if va <= rva < va + size:
        return rawptr + rva - va
    raise ValueError("RVA %x outside the sections" % rva)

  base = off(rsrc_rva)

  def entries(dir_off):
    named, ids = struct.unpack_from("<HH", data, base + dir_off + 12)
    for i in range(named + ids):
      name, target = struct.unpack_from("<II", data, base + dir_off + 16 + 8 * i)
      yield name, target

  result = {}
  for type_id, type_target in entries(0):
    if type_id != 5 or not type_target & 0x80000000:   # RT_DIALOG
      continue
    for name_id, name_target in entries(type_target & 0x7FFFFFFF):
      if name_id & 0x80000000:
        continue
      for _lang, lang_target in entries(name_target & 0x7FFFFFFF):
        rva, size = struct.unpack_from("<II", data, base + lang_target)
        result[name_id] = data[off(rva):off(rva) + size]
        break
  return result


def parse_template(t):
  """Controls of a compiled DLGTEMPLATE or DLGTEMPLATEEX: a list of
  (class, text, id, x, y, w, h, style, exstyle) and the dialog
  (x, y, w, h, style, exstyle)."""
  pos = 0

  def u16():
    nonlocal pos
    v = struct.unpack_from("<H", t, pos)[0]
    pos += 2
    return v

  def sz_or_ord():
    nonlocal pos
    first = struct.unpack_from("<H", t, pos)[0]
    if first == 0xFFFF:
      pos += 4
      return struct.unpack_from("<H", t, pos - 2)[0]
    end = pos
    while struct.unpack_from("<H", t, end)[0] != 0:
      end += 2
    s = t[pos:end].decode("utf-16-le")
    pos = end + 2
    return s

  def align():
    nonlocal pos
    pos = (pos + 3) & ~3

  ex = len(t) >= 4 and struct.unpack_from("<HH", t, 0) == (1, 0xFFFF)
  if ex:
    _, _, _help, exstyle, style, n, x, y, w, h = struct.unpack_from("<HHIIIHhhhh", t, 0)
    pos = 26
  else:
    style, exstyle, n, x, y, w, h = struct.unpack_from("<IIHhhhh", t, 0)
    pos = 18
  sz_or_ord()  # menu
  sz_or_ord()  # class
  sz_or_ord()  # caption
  if style & 0x40:  # DS_SETFONT
    u16()
    if ex:
      pos += 4  # weight, italic, charset
    sz_or_ord()
  controls = []
  classes = {0x80: "Button", 0x81: "Edit", 0x82: "Static", 0x83: "ListBox",
             0x84: "ScrollBar", 0x85: "ComboBox"}
  for _ in range(n):
    align()
    if ex:
      _help, cexstyle, cstyle, cx, cy, cw, ch, cid = struct.unpack_from("<IIIhhhhI", t, pos)
      pos += 24
    else:
      cstyle, cexstyle, cx, cy, cw, ch, cid = struct.unpack_from("<IIhhhhH", t, pos)
      pos += 18
    cls = sz_or_ord()
    if isinstance(cls, int):
      cls = classes.get(cls, "#%d" % cls)
    text = sz_or_ord()
    if isinstance(text, int):
      text = "#%d" % text
    extra = u16()
    pos += extra
    controls.append((cls, text, cid, cx, cy, cw, ch, cstyle, cexstyle))
  return (x, y, w, h, style, exstyle), controls


def compare_templates(release):
  failures = 0
  checked = 0
  for exe, script in (("7zFM.exe", FM_SCRIPT), ("7zG.exe", GUI_SCRIPT)):
    res = rcgen.parse_file(os.path.join(ROOT, script))
    compiled = pe_dialogs(os.path.join(release, exe))
    ours = set(res.dialogs)
    if set(compiled) != ours:
      print("%s: dialogs only in the binary: %s, only in rcgen: %s" % (
          exe, sorted(set(compiled) - ours), sorted(ours - set(compiled))))
      failures += 1
    for id in sorted(ours & set(compiled)):
      d = res.dialogs[id]
      (x, y, w, h, style, exstyle), controls = parse_template(compiled[id])
      problems = []
      if (x, y, w, h) != (d.x, d.y, d.w, d.h):
        problems.append("dialog rect %s, rcgen %s" % ((x, y, w, h), (d.x, d.y, d.w, d.h)))
      if (style, exstyle) != (d.style, d.exstyle):
        problems.append("dialog style 0x%08x 0x%08x, rcgen 0x%08x 0x%08x" % (
            style, exstyle, d.style, d.exstyle))
      if len(controls) != len(d.controls):
        problems.append("%d controls, rcgen %d" % (len(controls), len(d.controls)))
      for c, r in zip(controls, d.controls):
        cls, text, cid, cx, cy, cw, ch, cstyle, cexstyle = c
        # class names are not case-sensitive
        ours_c = (r.cls.lower(), r.text, r.id & 0xFFFF, r.x, r.y, r.w, r.h, r.style, r.exstyle)
        theirs = (cls.lower(), text, cid & 0xFFFF, cx, cy, cw, ch, cstyle, cexstyle)
        if ours_c != theirs:
          problems.append("control %d: binary %r, rcgen %r" % (cid, theirs, ours_c))
      checked += 1
      if problems:
        failures += 1
        print("%s %s:\n  %s" % (exe, d.name or id, "\n  ".join(problems)))
  print("%d templates compared, %d differ" % (checked, failures))
  return failures


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--release", required=True, help="the extracted 7-Zip release for Windows")
  ap.add_argument("--helper", help="winref.exe (Winelib)")
  ap.add_argument("--out", help="directory for the pictures")
  ap.add_argument("--gallery", help="z7-gallery pictures to compare with")
  ap.add_argument("--templates", action="store_true", help="compare the compiled templates with rcgen")
  args = ap.parse_args()

  failures = 0
  if args.templates:
    failures += compare_templates(args.release)
  if args.helper:
    if not args.out:
      sys.exit("--out is required with --helper")
    raw = os.path.join(args.out, "bmp")
    shutil.rmtree(raw, ignore_errors=True)
    os.makedirs(raw)
    render(args.release, args.helper, raw, os.path.join(args.out, "session"))
    names = convert(raw, args.out)
    print("%d reference pictures in %s" % (len(names), args.out))
    if args.gallery:
      compare(names, args.out, args.gallery)
  return 1 if failures else 0


if __name__ == "__main__":
  sys.exit(main())
