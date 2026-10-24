#!/usr/bin/env python3
"""Tests for linux/tools/rcgen.py.

- The resource scripts of 7zFM and 7zG parse, and every control uses a known
  statement and window class.
- Every dialog template in the upstream tree parses, in the configuration
  where its script is meant to build.
- The icons of both programs are found where the resource compiler finds
  them.
- Resource script syntax (strings, styles, menus, accelerators, string
  tables, icons, skipped resources) and error reporting, on small scripts.
"""

import glob
import os
import re
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "linux", "tools"))

import rcgen  # noqa: E402

PROGRAMS = {
    "7zFM": "CPP/7zip/Bundles/Fm/resource.rc",
    "7zG": "CPP/7zip/UI/GUI/resource.rc",
}

# Windows CE variants whose scripts do not build upstream either: MenuPage2.rc
# defines string 3441 twice and BenchmarkDialog.rc uses an undefined
# IDT_BENCH_SIZE_VAL when UNDER_CE is defined.
BROKEN_UPSTREAM_UNDER_CE = {"IDD_MENU_2", "IDD_BENCH_2"}

TEMPLATE_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z_0-9]*)\s+(DIALOGEX|DIALOG|MY_PAGE)\b")


def parse_text(text, defines=()):
  with tempfile.TemporaryDirectory() as d:
    path = os.path.join(d, "test.rc")
    with open(path, "w") as f:
      f.write(text)
    return rcgen.parse_file(path, defines)


HEADER = "#include <windows.h>\n#include <CommCtrl.h>\n"


class ProgramScripts(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    cls.res = {name: rcgen.parse_file(os.path.join(ROOT, path)) for name, path in PROGRAMS.items()}

  def test_contents(self):
    fm = self.res["7zFM"]
    gui = self.res["7zG"]
    self.assertGreaterEqual(len(fm.dialogs), 20)
    self.assertGreaterEqual(len(gui.dialogs), 10)
    self.assertEqual(len(fm.menus), 1)
    self.assertEqual(len(fm.accels), 1)
    self.assertGreaterEqual(len(fm.strings), 200)
    print("\n7zFM: %d dialogs, %d controls, %d strings; 7zG: %d dialogs, %d strings" % (
        len(fm.dialogs), sum(len(d.controls) for d in fm.dialogs.values()), len(fm.strings),
        len(gui.dialogs), len(gui.strings)), file=sys.stderr)

  def test_icons(self):
    fm = self.res["7zFM"]
    gui = self.res["7zG"]
    # IDI_ICON is 1 in both programs, IDI_LOGO (100) only in 7zFM; the paths
    # are relative to the top-level script
    self.assertEqual(sorted(fm.icons), [1, 100])
    self.assertEqual(rcgen.repo_relative(fm.icons[1]), "CPP/7zip/UI/FileManager/FM.ico")
    self.assertEqual(rcgen.repo_relative(fm.icons[100]), "CPP/7zip/UI/FileManager/7zipLogo.ico")
    self.assertEqual(rcgen.repo_relative(gui.icons[1]), "CPP/7zip/UI/GUI/FM.ico")
    for path in list(fm.icons.values()) + list(gui.icons.values()):
      self.assertTrue(os.path.isfile(path), path)

  def test_bitmaps(self):
    fm = self.res["7zFM"]
    # the toolbar bitmaps (large and small) and the context menu logo
    names = {id: os.path.basename(p) for id, p in fm.bitmaps.items()}
    self.assertEqual(names[100], "Add.bmp")
    self.assertEqual(names[150], "Add2.bmp")
    self.assertEqual(names[190], "MenuLogo.bmp")
    self.assertEqual(len(fm.bitmaps), 15)
    for path in fm.bitmaps.values():
      self.assertTrue(os.path.isfile(path), path)

  def test_controls_are_known(self):
    for name, res in self.res.items():
      for d in res.dialogs.values():
        self.assertTrue(d.name and d.name.startswith("IDD_"), d.source)
        self.assertTrue(d.controls, d.name)
        for c in d.controls:
          self.assertIn(c.cls, rcgen.KNOWN_CLASSES, "%s %s" % (d.name, c.source))
          self.assertTrue(0 <= c.x < d.w and 0 <= c.y < d.h, "%s %s" % (d.name, c.source))

  def test_7zg_dialogs_match_7zfm(self):
    fm = self.res["7zFM"]
    for id, d in self.res["7zG"].dialogs.items():
      self.assertIn(id, fm.dialogs)
      f = fm.dialogs[id]
      self.assertEqual((d.source, d.w, d.h, d.style, len(d.controls)),
                       (f.source, f.w, f.h, f.style, len(f.controls)))

  def test_generated_cpp_is_stable(self):
    fm = self.res["7zFM"]
    a = rcgen.generate_cpp(fm, PROGRAMS["7zFM"])
    b = rcgen.generate_cpp(rcgen.parse_file(os.path.join(ROOT, PROGRAMS["7zFM"])), PROGRAMS["7zFM"])
    self.assertEqual(a, b)
    self.assertIn("extern const CResources g_Resources", a)


class EveryTemplate(unittest.TestCase):

  def test_every_template_parses(self):
    scripts = sorted(glob.glob(os.path.join(ROOT, "CPP", "**", "*.rc"), recursive=True)
                     + glob.glob(os.path.join(ROOT, "C", "**", "*.rc"), recursive=True))
    defined = set()
    for path in scripts:
      rel = os.path.relpath(path, ROOT)
      with open(path, encoding="latin-1") as f:
        for line in f:
          m = TEMPLATE_RE.match(line)
          if m and m.group(1) != "DIALOG_ID":  # ProgressDialog2a.rc: a macro, checked below
            defined.add((rel, m.group(1)))
    parsed = set()
    for path in scripts:
      for defines in ([], ["UNDER_CE"]):
        try:
          res = rcgen.parse_file(path, defines)
        except rcgen.RcError:
          continue  # fragments that only build when included by another script
        for d in res.dialogs.values():
          parsed.add((d.source.rsplit(":", 1)[0], d.name))
    missing = {t for t in defined - parsed if t[1] not in BROKEN_UPSTREAM_UNDER_CE}
    self.assertEqual(missing, set())
    self.assertIn(("CPP/7zip/UI/FileManager/ProgressDialog2a.rc", "IDD_PROGRESS"), parsed)
    print("\n%d dialog templates parsed; %d Windows CE variants are broken upstream" % (
        len(parsed), len(BROKEN_UPSTREAM_UNDER_CE)), file=sys.stderr)


class Syntax(unittest.TestCase):

  def test_strings(self):
    res = parse_text(HEADER + r'''
STRINGTABLE
BEGIN
  1 "plain"
  2, "with comma"
  3 "quote ""x"" tab\t nl\n bs\\ hex\x41 oct\101 dq\""
  4 L"wide"
  5 ""
END
''')
    self.assertEqual(res.strings, {1: "plain", 2: "with comma",
                                   3: 'quote "x" tab\t nl\n bs\\ hexA octA dq"',
                                   4: "wide", 5: ""})

  def test_dialog_styles(self):
    res = parse_text(HEADER + '''
#define xs (100 + 2 * 8)
10 DIALOG 0, 0, xs, 50 - 10 / 3
STYLE DS_MODALFRAME | WS_POPUP
CAPTION "Caption"
FONT 8, "MS Shell Dlg"
BEGIN
  LTEXT "a", 100, 1, 2, 3, 4
  RTEXT "b", 101, 1, 2, 3, 4, SS_NOPREFIX
  PUSHBUTTON "c", 102, 1, 2, 3, 4, NOT WS_TABSTOP | WS_DISABLED
  COMBOBOX 103, 1, 2, 3, 4
  COMBOBOX 104, 1, 2, 3, 4, CBS_DROPDOWN
  EDITTEXT 105, 1, 2, 3, 4, ES_READONLY, WS_EX_CLIENTEDGE
  ICON "", 106, 1, 2
  CONTROL "d", 107, "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 1, 2, 3, 4
  CONTROL "", 108, L"msctls_updown32", UDS_ARROWKEYS, 1, 2, 3, 4
  GROUPBOX "e", 109, -1, 0, 3, 4
END
''')
    d = res.dialogs[10]
    c = rcgen.read_constants()
    self.assertEqual((d.w, d.h), (116, 47))
    self.assertEqual(d.style, 0x80 | 0x80000000 | 0x00C00000 | 0x40)
    self.assertEqual(d.caption, "Caption")
    self.assertEqual((d.font_name, d.font_size), ("MS Shell Dlg", 8))
    child = 0x40000000 | 0x10000000
    styles = [ctl.style for ctl in d.controls]
    self.assertEqual(styles[0], child | 0x20000)                 # SS_LEFT | WS_GROUP
    self.assertEqual(styles[1], child | 0x20000 | 2 | 0x80)      # SS_RIGHT | SS_NOPREFIX
    self.assertEqual(styles[2], child | 0x08000000)              # no WS_TABSTOP
    self.assertEqual(styles[3], child | 0x10000 | c["CBS_SIMPLE"])
    self.assertEqual(styles[4], child | 0x10000 | c["CBS_DROPDOWN"])
    self.assertEqual(styles[5], child | 0x10000 | 0x800000 | 0x800)
    self.assertEqual(d.controls[5].exstyle, 0x200)
    self.assertEqual((d.controls[6].cls, styles[6], d.controls[6].w), ("Static", child | 3, 0))
    self.assertEqual(styles[7], child | 3 | 0x10000)
    self.assertEqual(d.controls[8].cls, "msctls_updown32")
    self.assertEqual(d.controls[9].x, -1)

  def test_rc_exe_subtraction(self):
    # rc.exe ignores "- (b)" after a closing parenthesis, but not after a
    # number (verified against the IDD_BENCH template of 7zG.exe 26.05)
    res = parse_text(HEADER + '''
10 DIALOG 0, 0, 100, 100
BEGIN
  LTEXT "a", 1, 0, 0, ((50 + 8) - 8) - (8 + 8), 8
  LTEXT "b", 2, 0, 0, 100 - (8 + 8), 8
  LTEXT "c", 3, 0, 0, (100) - 16, 8
END
''')
    self.assertEqual([c.w for c in res.dialogs[10].controls], [50, 84, 84])

  def test_dialogex(self):
    res = parse_text(HEADER + '''
11 DIALOGEX 0, 0, 100, 50, 77
STYLE WS_POPUP
EXSTYLE WS_EX_TOPMOST
FONT 9, "Segoe UI", 700, 1, 0x1
BEGIN
  LTEXT "a", 100, 1, 2, 3, 4, 0, 0, 55
END
''')
    d = res.dialogs[11]
    self.assertTrue(d.extended)
    self.assertEqual((d.font_name, d.font_size, d.font_weight, d.font_italic), ("Segoe UI", 9, 700, 1))
    self.assertEqual(d.exstyle, 0x8)
    self.assertEqual(len(d.controls), 1)

  def test_menus_and_accelerators(self):
    res = parse_text(HEADER + '''
20 MENU
BEGIN
  POPUP "&File"
  BEGIN
    MENUITEM "&Open", 1, CHECKED
    MENUITEM SEPARATOR
    MENUITEM "E&xit", 2, GRAYED, MENUBARBREAK
  END
END
21 MENUEX
BEGIN
  POPUP "&Edit", 30
  BEGIN
    MENUITEM "Cu&t", 31, MFT_STRING, MFS_CHECKED
    MENUITEM "", 0, MFT_SEPARATOR
    POPUP "More", 32, 0, 0, 99
    BEGIN
      MENUITEM "Deep", 33
    END
  END
END
22 ACCELERATORS
BEGIN
  VK_F1, 1, VIRTKEY, NOINVERT
  "^C", 2
  "a", 3, ASCII
  "X", 4, VIRTKEY, CONTROL, SHIFT, ALT
END
''')
    m = res.menus[20].items
    self.assertEqual([(i.text, i.level, i.popup) for i in m],
                     [("&File", 0, True), ("&Open", 1, False), ("", 1, False), ("E&xit", 1, False)])
    self.assertEqual(m[1].state, 0x8)
    self.assertEqual(m[2].type, 0x800)
    self.assertEqual((m[3].state, m[3].type), (0x1, 0x20))
    m = res.menus[21].items
    self.assertTrue(res.menus[21].extended)
    self.assertEqual([(i.text, i.id, i.level) for i in m],
                     [("&Edit", 30, 0), ("Cu&t", 31, 1), ("", 0, 1), ("More", 32, 1), ("Deep", 33, 2)])
    self.assertEqual(m[1].state, 0x8)
    a = res.accels[22]
    self.assertEqual([(x.key, x.cmd, x.flags) for x in a],
                     [(0x70, 1, 0x3), (3, 2, 0), (ord("a"), 3, 0), (ord("X"), 4, 0x1d)])

  def test_skipped_resources(self):
    res = parse_text(HEADER + '''
LANGUAGE 9, 1
1 VERSIONINFO
  FILEVERSION 1,2,3,4
  FILEFLAGS 0x0L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    VALUE "CompanyName", "x"
  END
END
100 ICON "app.ico"
101 BITMAP MOVEABLE PURE "a.bmp"
1 24 MOVEABLE PURE "app.manifest"
STRINGTABLE
BEGIN
  7 "seven"
END
''')
    self.assertEqual(res.strings, {7: "seven"})
    self.assertEqual(list(res.icons), [100])
    self.assertTrue(res.icons[100].endswith(os.sep + "app.ico"))
    self.assertEqual(list(res.bitmaps), [101])
    self.assertTrue(res.bitmaps[101].endswith(os.sep + "a.bmp"))
    res.icons.clear()  # the .ico file does not exist
    self.assertIn('{ 101, "a" }', rcgen.generate_cpp(res, "test.rc"))

  def test_cpp_strings(self):
    self.assertEqual(rcgen.cpp_wstring('a"b\\c\n\t\x01ą\U0001F600'),
                     'L"a\\"b\\\\c\\n\\t\\001\\u0105\\U0001f600"')


class Errors(unittest.TestCase):

  def check_error(self, body, message):
    with self.assertRaises(rcgen.RcError) as cm:
      parse_text(HEADER + body)
    self.assertIn(message, str(cm.exception))
    self.assertRegex(str(cm.exception), r"test\.rc:\d+")

  def test_unknown_control(self):
    self.check_error('10 DIALOG 0,0,1,1\nBEGIN\n  SLIDER "a", 1, 0, 0, 1, 1\nEND\n',
                     "unknown control statement SLIDER")

  def test_unknown_class(self):
    self.check_error('10 DIALOG 0,0,1,1\nBEGIN\n  CONTROL "a", 1, "RichEdit20W", 0, 0, 0, 1, 1\nEND\n',
                     "unknown window class 'RichEdit20W'")

  def test_unresolved_identifier(self):
    self.check_error('10 DIALOG 0,0,1,1\nBEGIN\n  LTEXT "a", IDT_MISSING, 0, 0, 1, 1\nEND\n',
                     "unresolved identifier IDT_MISSING")

  def test_duplicates(self):
    self.check_error('STRINGTABLE\nBEGIN\n  1 "a"\n  1 "b"\nEND\n', "duplicate string resource 1")

  def test_unsupported_resource(self):
    self.check_error('5 TOOLBAR 16, 15\nBEGIN\nEND\n', "unsupported resource type 'TOOLBAR'")


if __name__ == "__main__":
  unittest.main(verbosity=2)
