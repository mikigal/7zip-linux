#!/usr/bin/env python3
"""Generate C++ resource tables from the upstream 7-Zip resource scripts.

rcgen runs the C preprocessor over a resource script (.rc) with the stub
windows.h and CommCtrl.h from linux/shim/win32 and RC_INVOKED defined, and
parses the result: DIALOG and DIALOGEX templates, MENU and MENUEX resources,
ACCELERATORS tables, STRINGTABLEs and ICONs (the .ico files are embedded).
Bitmaps, manifests and version information are skipped. Anything else is an
error, so a new construct in an upstream release is noticed instead of
silently dropped.

The C++ output defines NRc::g_Resources (see linux/platform/RcTables.h).
Styles get the defaults that the Windows resource compiler adds (for example
WS_CHILD | WS_VISIBLE | SS_LEFT | WS_GROUP for LTEXT), so the tables hold the
same values as a compiled .res file. An explicit style is OR-ed with these
defaults, "NOT x" removes bits, and COMBOBOX gets CBS_SIMPLE only when the
style names no combo box type.

Usage:
  rcgen.py --cpp OUT.cpp [--depfile OUT.d] SCRIPT.rc
  rcgen.py --dump SCRIPT.rc
Options -D NAME[=VALUE] define preprocessor macros, --cc selects the C
compiler used as the preprocessor (default: $CC or cc).
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SHIM_INCLUDE = os.path.join(ROOT, "linux", "shim", "win32")

# Win32 constants that rcgen itself needs; their values are read from the
# stub headers, so there is a single source of truth.
CONSTANT_NAMES = """
  WS_POPUP WS_CHILD WS_VISIBLE WS_BORDER WS_SYSMENU WS_CAPTION WS_GROUP WS_TABSTOP
  DS_SETFONT
  SS_LEFT SS_RIGHT SS_CENTER SS_ICON
  BS_PUSHBUTTON BS_DEFPUSHBUTTON BS_PUSHBOX BS_GROUPBOX BS_CHECKBOX BS_AUTOCHECKBOX
  BS_RADIOBUTTON BS_AUTORADIOBUTTON BS_3STATE BS_AUTO3STATE
  ES_LEFT CBS_SIMPLE CBS_DROPDOWN CBS_DROPDOWNLIST LBS_NOTIFY SBS_HORZ
  MFT_STRING MFT_SEPARATOR MFT_MENUBREAK MFT_MENUBARBREAK MFT_RIGHTJUSTIFY
  MFS_CHECKED MFS_GRAYED
  MF_GRAYED MF_DISABLED MF_CHECKED MF_MENUBREAK MF_MENUBARBREAK MF_HELP
  FVIRTKEY FNOINVERT FSHIFT FCONTROL FALT
""".split()

MEMORY_OPTIONS = {"PRELOAD", "LOADONCALL", "FIXED", "MOVEABLE", "DISCARDABLE",
                  "PURE", "IMPURE", "SHARED", "NONSHARED"}

# Window classes that a dialog engine has to implement.
KNOWN_CLASSES = {"Button", "Static", "Edit", "ComboBox", "ListBox", "ScrollBar",
                 "SysListView32", "msctls_progress32", "msctls_updown32",
                 "msctls_trackbar32"}

# Predefined class atoms that CONTROL statements may use instead of names.
CLASS_KEYWORDS = {"BUTTON": "Button", "EDIT": "Edit", "STATIC": "Static",
                  "LISTBOX": "ListBox", "SCROLLBAR": "ScrollBar",
                  "COMBOBOX": "ComboBox"}
CLASS_ATOMS = {0x80: "Button", 0x81: "Edit", 0x82: "Static", 0x83: "ListBox",
               0x84: "ScrollBar", 0x85: "ComboBox"}


class RcError(Exception):
  pass


class Token:
  __slots__ = ("kind", "value", "file", "line")

  def __init__(self, kind, value, file, line):
    self.kind = kind    # "str", "num", "id" or "op"
    self.value = value  # decoded text, int, name or operator
    self.file = file
    self.line = line

  def where(self):
    return "%s:%d" % (self.file, self.line)

  def __repr__(self):
    return "%s(%r)" % (self.kind, self.value)


class Control:
  def __init__(self, statement, cls, text, id, x, y, w, h, style, exstyle, source):
    self.statement = statement
    self.cls = cls
    self.text = text
    self.id = id
    self.x, self.y, self.w, self.h = x, y, w, h
    self.style = style
    self.exstyle = exstyle
    self.source = source


class Dialog:
  def __init__(self, id, extended, source):
    self.id = id
    self.name = None
    self.extended = extended
    self.source = source
    self.x = self.y = self.w = self.h = 0
    self.style = 0
    self.exstyle = 0
    self.caption = ""
    self.font_name = ""
    self.font_size = 0
    self.font_weight = 0
    self.font_italic = 0
    self.controls = []


class MenuItem:
  def __init__(self, text, id, type, state, level, popup):
    self.text = text
    self.id = id
    self.type = type
    self.state = state
    self.level = level
    self.popup = popup


class Menu:
  def __init__(self, id, extended):
    self.id = id
    self.extended = extended
    self.items = []


class Accel:
  def __init__(self, key, cmd, flags):
    self.key = key
    self.cmd = cmd
    self.flags = flags


class Resources:
  def __init__(self):
    self.dialogs = {}
    self.menus = {}
    self.accels = {}
    self.strings = {}
    self.icons = {}    # id -> path of the .ico file
    self.symbols = {}  # macro name -> int, for the names of resource IDs

  def add(self, table, id, value, kind, tok):
    if id in table:
      raise RcError("%s: duplicate %s resource %r" % (tok.where(), kind, id))
    table[id] = value

  def symbol_for(self, value, prefix):
    names = sorted(n for n, v in self.symbols.items() if v == value and n.startswith(prefix))
    return names[0] if names else None


# ---------- Preprocessing and tokens ----------

def preprocess(path, defines=(), cc=None, depfile=None, deptarget=None, text=None):
  cc = cc or os.environ.get("CC") or "cc"
  cmd = [cc, "-E", "-x", "c", "-undef", "-nostdinc", "-dD", "-DRC_INVOKED",
         "-I", SHIM_INCLUDE]
  cmd += ["-D" + d for d in defines]
  if depfile:
    cmd += ["-MD", "-MF", depfile, "-MT", deptarget or "rc"]
  cmd.append(path if text is None else "-")
  res = subprocess.run(cmd, input=None if text is None else text.encode(),
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  if res.returncode != 0:
    raise RcError("preprocessing %s failed:\n%s" % (path, res.stderr.decode(errors="replace")))
  # Resource scripts are ASCII; narrow strings would be in the ANSI code page.
  return res.stdout.decode("latin-1")


TOKEN_RE = re.compile(r"""
    (?P<ws>[ \t\r\f\v]+)
  | (?P<str>L?"(?:[^"\\\n]|\\.|"")*")
  | (?P<num>(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*)
  | (?P<id>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<op>[,|&^+\-*/%()~{}!])
""", re.X)

LINE_MARKER_RE = re.compile(r'^#\s*(?:line\s+)?(\d+)\s+"((?:[^"\\]|\\.)*)"')
DEFINE_RE = re.compile(r"^#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.*)$")


def decode_string(raw, file, line):
  wide = raw.startswith("L")
  s = raw[2:-1] if wide else raw[1:-1]
  out = []
  i = 0
  while i < len(s):
    c = s[i]
    if c == '"':  # "" inside a string is a quote
      out.append('"')
      i += 2
      continue
    if c != "\\":
      out.append(c)
      i += 1
      continue
    e = s[i + 1]
    i += 2
    simple = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f",
              "v": "\v", "\\": "\\", '"': '"', "'": "'"}
    if e in simple:
      out.append(simple[e])
    elif e in "xX":
      m = re.match(r"[0-9a-fA-F]{1,%d}" % (4 if wide else 2), s[i:])
      if not m:
        raise RcError("%s:%d: bad \\x escape in %s" % (file, line, raw))
      out.append(chr(int(m.group(0), 16)))
      i += len(m.group(0))
    elif e in "01234567":
      m = re.match(r"[0-7]{0,2}", s[i:])
      out.append(chr(int(e + m.group(0), 8)))
      i += len(m.group(0))
    else:
      out.append(e)
  text = "".join(out)
  if not wide:
    text = text.encode("latin-1").decode("cp1252", errors="replace")
  return text


def tokenize(text, symbols=None):
  tokens = []
  file, line = "<input>", 1
  for raw_line in text.split("\n"):
    stripped = raw_line.lstrip()
    if stripped.startswith("#"):
      m = LINE_MARKER_RE.match(stripped)
      if m:
        file, line = m.group(2), int(m.group(1))
        continue
      m = DEFINE_RE.match(stripped)
      if m and symbols is not None:
        value = m.group(2).strip()
        try:
          symbols[m.group(1)] = parse_number(value)
        except ValueError:
          pass
      line += 1
      continue
    pos = 0
    while pos < len(raw_line):
      m = TOKEN_RE.match(raw_line, pos)
      if not m:
        raise RcError("%s:%d: unexpected character %r" % (file, line, raw_line[pos]))
      pos = m.end()
      kind = m.lastgroup
      if kind == "ws":
        continue
      value = m.group(kind)
      if kind == "str":
        value = decode_string(value, file, line)
      elif kind == "num":
        value = parse_number(value)
      tokens.append(Token(kind, value, file, line))
    line += 1
  return tokens


def parse_number(s):
  s = s.rstrip("uUlL")
  if s[:2] in ("0x", "0X"):
    return int(s[2:], 16)
  return int(s, 10)  # resource scripts have no octal numbers


# ---------- Parser ----------

class Parser:
  def __init__(self, tokens, consts, resources):
    self.toks = tokens
    self.pos = 0
    self.c = consts
    self.res = resources

  # token helpers

  def peek(self, k=0):
    i = self.pos + k
    return self.toks[i] if i < len(self.toks) else None

  def next(self):
    tok = self.peek()
    if tok is None:
      last = self.toks[-1] if self.toks else Token("op", "", "<input>", 0)
      raise RcError("%s: unexpected end of input" % last.where())
    self.pos += 1
    return tok

  def error(self, msg, tok=None):
    tok = tok or self.peek() or (self.toks[-1] if self.toks else None)
    raise RcError("%s: %s" % (tok.where() if tok else "<input>", msg))

  def is_op(self, value, k=0):
    tok = self.peek(k)
    return tok is not None and tok.kind == "op" and tok.value == value

  def is_kw(self, *names, k=0):
    tok = self.peek(k)
    return tok is not None and tok.kind == "id" and tok.value in names

  def accept_op(self, value):
    if self.is_op(value):
      self.pos += 1
      return True
    return False

  def accept_kw(self, *names):
    if self.is_kw(*names):
      return self.next().value
    return None

  def expect_op(self, value):
    if not self.accept_op(value):
      self.error("expected %r" % value)

  def string(self):
    tok = self.next()
    if tok.kind != "str":
      self.error("expected a string, got %r" % (tok.value,), tok)
    return tok.value

  def begin(self):
    if not (self.accept_kw("BEGIN") or self.accept_op("{")):
      self.error("expected BEGIN")

  def at_end_kw(self):
    return self.is_kw("END") or self.is_op("}")

  def end(self):
    if not (self.accept_kw("END") or self.accept_op("}")):
      self.error("expected END")

  # expressions (C precedence; all operands must be numbers after cpp)

  def expr(self):
    v = self.xor_expr()
    while self.accept_op("|"):
      v |= self.xor_expr()
    return v

  def xor_expr(self):
    v = self.and_expr()
    while self.accept_op("^"):
      v ^= self.and_expr()
    return v

  def and_expr(self):
    v = self.add_expr()
    while self.accept_op("&"):
      v &= self.add_expr()
    return v

  def add_expr(self):
    v = self.mul_expr()
    while True:
      if self.accept_op("+"):
        v += self.mul_expr()
      elif self.rc_drops_subtraction():
        # "(a) - (b)" is a: rc.exe ignores "- (b)" after a closing
        # parenthesis. Seen in 7zG.exe 26.05: the "Current" and "Resulting"
        # labels of IDD_BENCH, width (xSize - g4x) = (58) - (8 + 8), are 58
        # dialog units wide, while "8 - (...)" elsewhere subtracts normally.
        self.next()
        self.unary()
      elif self.accept_op("-"):
        v -= self.mul_expr()
      else:
        return v

  def rc_drops_subtraction(self):
    prev = self.toks[self.pos - 1] if self.pos > 0 else None
    return (prev is not None and prev.kind == "op" and prev.value == ")"
            and self.is_op("-") and self.is_op("(", k=1))

  def mul_expr(self):
    v = self.unary()
    while True:
      if self.accept_op("*"):
        v *= self.unary()
      elif self.accept_op("/"):
        d = self.unary()
        if d == 0:
          self.error("division by zero")
        v = int(v / d)  # C division truncates toward zero
      elif self.accept_op("%"):
        d = self.unary()
        if d == 0:
          self.error("division by zero")
        v = v - d * int(v / d)
      else:
        return v

  def unary(self):
    if self.accept_op("-"):
      return -self.unary()
    if self.accept_op("+"):
      return self.unary()
    if self.accept_op("~"):
      return ~self.unary()
    if self.accept_op("!"):
      return int(not self.unary())
    if self.accept_op("("):
      v = self.expr()
      self.expect_op(")")
      return v
    tok = self.next()
    if tok.kind == "num":
      return tok.value
    if tok.kind == "id":
      self.error("unresolved identifier %s" % tok.value, tok)
    self.error("expected a number, got %r" % (tok.value,), tok)

  def style(self, base):
    """A style list: terms joined with '|', each optionally preceded by NOT."""
    v = base
    while True:
      neg = self.accept_kw("NOT")
      term = self.xor_expr()
      v = (v & ~term) if neg else (v | term)
      if not self.accept_op("|"):
        return v & 0xFFFFFFFF

  def comma_expr(self):
    self.expect_op(",")
    return self.expr()

  def optional_comma_expr(self):
    """Value of an optional trailing ", expr" argument, or None."""
    if not self.is_op(","):
      return None
    self.next()
    if self.is_op(","):  # empty argument
      return None
    return self.expr()

  def skip_memory_options(self):
    while self.accept_kw(*MEMORY_OPTIONS):
      pass

  def skip_block(self):
    self.begin()
    depth = 1
    while depth:
      tok = self.next()
      if (tok.kind == "id" and tok.value == "BEGIN") or (tok.kind == "op" and tok.value == "{"):
        depth += 1
      elif (tok.kind == "id" and tok.value == "END") or (tok.kind == "op" and tok.value == "}"):
        depth -= 1

  # top level

  def parse(self):
    while self.peek() is not None:
      if self.accept_kw("LANGUAGE"):
        self.expr()
        self.comma_expr()
        continue
      if self.is_kw("STRINGTABLE"):
        self.stringtable()
        continue
      name_tok = self.peek()
      if name_tok.kind == "num" or self.is_op("(") or self.is_op("-"):
        name = self.expr()
      elif name_tok.kind in ("id", "str"):
        name = self.next().value
      else:
        self.error("unexpected %r" % (name_tok.value,))
      type_tok = self.next()
      kind = type_tok.value if type_tok.kind == "id" else None
      if kind in ("DIALOG", "DIALOGEX"):
        self.dialog(name, kind == "DIALOGEX", type_tok)
      elif kind in ("MENU", "MENUEX"):
        self.menu(name, kind == "MENUEX", type_tok)
      elif kind == "ACCELERATORS":
        self.accelerators(name, type_tok)
      elif kind == "VERSIONINFO":
        while not (self.is_kw("BEGIN") or self.is_op("{")):
          self.next()
        self.skip_block()
      elif kind == "ICON":
        self.icon(name, type_tok)
      elif kind in ("BITMAP", "CURSOR", "FONT", "HTML", "MESSAGETABLE",
                    "RCDATA") or type_tok.kind == "num":
        self.skip_memory_options()
        if self.peek() is not None and self.peek().kind == "str":
          self.next()
        else:
          self.skip_block()
      else:
        self.error("unsupported resource type %r" % (type_tok.value,), type_tok)

  def icon(self, id, tok):
    self.skip_memory_options()
    file_tok = self.peek()
    if file_tok is None or file_tok.kind != "str":
      self.error("ICON without a file name", tok)
    self.next()
    if not isinstance(id, int):
      self.error("icon %r: only numeric icon IDs are supported" % (id,), tok)
    self.res.add(self.res.icons, id, resolve_file(file_tok.value, self.script_dir, file_tok.file), "icon", tok)

  def stringtable(self):
    tok = self.next()
    self.skip_memory_options()
    while True:
      if self.accept_kw("LANGUAGE"):
        self.expr()
        self.comma_expr()
      elif self.accept_kw("CHARACTERISTICS", "VERSION"):
        self.expr()
      else:
        break
    self.begin()
    while not self.at_end_kw():
      id_tok = self.peek()
      id = self.expr()
      self.accept_op(",")
      text = self.string()
      self.res.add(self.res.strings, id, text, "string", id_tok)
    self.end()
    return tok

  def dialog(self, id, extended, tok):
    c = self.c
    d = Dialog(id, extended, "%s:%d" % (tok.file, tok.line))
    self.skip_memory_options()
    d.x = self.expr()
    d.y = self.comma_expr()
    d.w = self.comma_expr()
    d.h = self.comma_expr()
    if extended:
      self.optional_comma_expr()  # help ID
    style = c["WS_POPUP"] | c["WS_BORDER"] | c["WS_SYSMENU"]
    has_caption = has_font = False
    while True:
      if self.accept_kw("STYLE"):
        style = self.style(0)
      elif self.accept_kw("EXSTYLE"):
        d.exstyle = self.style(0)
      elif self.accept_kw("CAPTION"):
        d.caption = self.string()
        has_caption = True
      elif self.accept_kw("FONT"):
        d.font_size = self.expr()
        self.expect_op(",")
        d.font_name = self.string()
        if extended:
          d.font_weight = self.optional_comma_expr() or 0
          d.font_italic = self.optional_comma_expr() or 0
          self.optional_comma_expr()  # charset
        has_font = True
      elif self.accept_kw("MENU", "CLASS"):
        self.next()
      elif self.accept_kw("LANGUAGE"):
        self.expr()
        self.comma_expr()
      elif self.accept_kw("CHARACTERISTICS", "VERSION"):
        self.expr()
      else:
        break
    if has_caption:
      style |= c["WS_CAPTION"]
    if has_font:
      style |= c["DS_SETFONT"]
    d.style = style & 0xFFFFFFFF
    self.begin()
    while not self.at_end_kw():
      d.controls.append(self.control(extended))
    self.end()
    d.name = self.res.symbol_for(id, "IDD_")
    self.res.add(self.res.dialogs, id, d, "dialog", tok)

  def control_defaults(self):
    c = self.c
    return {
        "LTEXT": ("Static", c["SS_LEFT"] | c["WS_GROUP"]),
        "RTEXT": ("Static", c["SS_RIGHT"] | c["WS_GROUP"]),
        "CTEXT": ("Static", c["SS_CENTER"] | c["WS_GROUP"]),
        "ICON": ("Static", c["SS_ICON"]),
        "PUSHBUTTON": ("Button", c["BS_PUSHBUTTON"] | c["WS_TABSTOP"]),
        "DEFPUSHBUTTON": ("Button", c["BS_DEFPUSHBUTTON"] | c["WS_TABSTOP"]),
        "PUSHBOX": ("Button", c["BS_PUSHBOX"] | c["WS_TABSTOP"]),
        "GROUPBOX": ("Button", c["BS_GROUPBOX"]),
        "CHECKBOX": ("Button", c["BS_CHECKBOX"] | c["WS_TABSTOP"]),
        "AUTOCHECKBOX": ("Button", c["BS_AUTOCHECKBOX"] | c["WS_TABSTOP"]),
        "RADIOBUTTON": ("Button", c["BS_RADIOBUTTON"] | c["WS_TABSTOP"]),
        "AUTORADIOBUTTON": ("Button", c["BS_AUTORADIOBUTTON"]),
        "STATE3": ("Button", c["BS_3STATE"] | c["WS_TABSTOP"]),
        "AUTO3STATE": ("Button", c["BS_AUTO3STATE"] | c["WS_TABSTOP"]),
        "EDITTEXT": ("Edit", c["ES_LEFT"] | c["WS_BORDER"] | c["WS_TABSTOP"]),
        # CBS_SIMPLE is added below only if the style has no combo box type
        "COMBOBOX": ("ComboBox", c["WS_TABSTOP"]),
        "LISTBOX": ("ListBox", c["LBS_NOTIFY"] | c["WS_BORDER"]),
        "SCROLLBAR": ("ScrollBar", c["SBS_HORZ"]),
    }

  def control(self, extended):
    c = self.c
    tok = self.next()
    if tok.kind != "id":
      self.error("expected a control statement, got %r" % (tok.value,), tok)
    statement = tok.value
    base = c["WS_CHILD"] | c["WS_VISIBLE"]
    exstyle = 0
    text = ""
    if statement == "CONTROL":
      text = self.control_text()
      id = self.comma_expr()
      self.expect_op(",")
      cls = self.class_name()
      self.expect_op(",")
      style = self.style(base)
      x = self.comma_expr()
      y = self.comma_expr()
      w = self.comma_expr()
      h = self.comma_expr()
    else:
      defaults = self.control_defaults()
      if statement not in defaults:
        self.error("unknown control statement %s" % statement, tok)
      cls, default_style = defaults[statement]
      if statement in ("EDITTEXT", "COMBOBOX", "LISTBOX", "SCROLLBAR"):
        id = self.expr()
      else:
        text = self.control_text()
        id = self.comma_expr()
      x = self.comma_expr()
      y = self.comma_expr()
      if statement == "ICON" and not self.is_op(","):
        w = h = 0
      else:
        w = self.comma_expr()
        h = self.comma_expr()
      style = base | default_style
      if self.is_op(","):
        self.next()
        style = self.style(base | default_style)
      type_mask = c["CBS_SIMPLE"] | c["CBS_DROPDOWN"] | c["CBS_DROPDOWNLIST"]
      if statement == "COMBOBOX" and (style & type_mask) == 0:
        style |= c["CBS_SIMPLE"]
    v = self.optional_comma_expr()
    if v is not None:
      exstyle = v
    if extended:
      self.optional_comma_expr()  # help ID
    if cls not in KNOWN_CLASSES:
      self.error("unknown window class %r" % cls, tok)
    return Control(statement, cls, text, id, x, y, w, h, style & 0xFFFFFFFF,
                   exstyle & 0xFFFFFFFF, "%s:%d" % (tok.file, tok.line))

  def control_text(self):
    tok = self.peek()
    if tok is not None and tok.kind == "str":
      return self.next().value
    return "#%d" % self.expr()  # resource ID, for example of an icon

  def class_name(self):
    tok = self.next()
    if tok.kind == "str":
      return tok.value
    if tok.kind == "id" and tok.value in CLASS_KEYWORDS:
      return CLASS_KEYWORDS[tok.value]
    if tok.kind == "num" and tok.value in CLASS_ATOMS:
      return CLASS_ATOMS[tok.value]
    self.error("bad window class %r" % (tok.value,), tok)

  def menu(self, id, extended, tok):
    m = Menu(id, extended)
    self.skip_memory_options()
    while self.accept_kw("LANGUAGE"):
      self.expr()
      self.comma_expr()
    self.menu_items(m, extended, 0)
    self.res.add(self.res.menus, id, m, "menu", tok)

  def menu_options(self):
    """MENU item options: returns (type, state)."""
    c = self.c
    type = state = 0
    while True:
      self.accept_op(",")
      opt = self.accept_kw("CHECKED", "GRAYED", "INACTIVE", "MENUBREAK", "MENUBARBREAK", "HELP")
      if opt is None:
        return type, state
      if opt == "CHECKED":
        state |= c["MF_CHECKED"]
      elif opt == "GRAYED":
        state |= c["MF_GRAYED"]
      elif opt == "INACTIVE":
        state |= c["MF_DISABLED"]
      elif opt == "MENUBREAK":
        type |= c["MF_MENUBREAK"]
      elif opt == "MENUBARBREAK":
        type |= c["MF_MENUBARBREAK"]
      else:
        type |= c["MF_HELP"]

  def menu_items(self, m, extended, level):
    self.begin()
    while not self.at_end_kw():
      tok = self.next()
      if tok.kind != "id" or tok.value not in ("MENUITEM", "POPUP"):
        self.error("expected MENUITEM or POPUP", tok)
      popup = tok.value == "POPUP"
      if not popup and not extended and self.accept_kw("SEPARATOR"):
        m.items.append(MenuItem("", 0, self.c["MFT_SEPARATOR"], 0, level, False))
        continue
      text = self.string()
      id = type = state = 0
      if extended:
        id = self.optional_comma_expr() or 0
        type = self.optional_comma_expr() or 0
        state = self.optional_comma_expr() or 0
        if popup:
          self.optional_comma_expr()  # help ID
      else:
        if not popup:
          id = self.comma_expr()
        type, state = self.menu_options()
      m.items.append(MenuItem(text, id, type, state, level, popup))
      if popup:
        self.menu_items(m, extended, level + 1)
    self.end()

  def accelerators(self, id, tok):
    c = self.c
    items = []
    self.skip_memory_options()
    while self.accept_kw("LANGUAGE"):
      self.expr()
      self.comma_expr()
    self.begin()
    while not self.at_end_kw():
      key_tok = self.peek()
      if key_tok.kind == "str":
        s = self.next().value
        if len(s) == 2 and s[0] == "^":
          key = ord(s[1].upper()) - 0x40
        elif len(s) == 1:
          key = ord(s)
        else:
          self.error("bad accelerator key %r" % s, key_tok)
      else:
        key = self.expr()
      cmd = self.comma_expr()
      flags = 0
      while self.accept_op(","):
        opt = self.accept_kw("VIRTKEY", "ASCII", "NOINVERT", "ALT", "SHIFT", "CONTROL")
        if opt is None:
          self.error("bad accelerator option")
        flags |= {"VIRTKEY": c["FVIRTKEY"], "ASCII": 0, "NOINVERT": c["FNOINVERT"],
                  "ALT": c["FALT"], "SHIFT": c["FSHIFT"], "CONTROL": c["FCONTROL"]}[opt]
      items.append(Accel(key, cmd, flags))
    self.end()
    self.res.add(self.res.accels, id, items, "accelerators", tok)


def read_constants(cc=None):
  # "rcgen_NAME NAME": the prefixed identifier is not a macro, the second one expands
  text = "#include <CommCtrl.h>\n" + "".join(
      "rcgen_%s %s\n" % (n, n) for n in CONSTANT_NAMES)
  tokens = tokenize(preprocess("<constants>", cc=cc, text=text))
  consts = {}
  starts = [i for i, t in enumerate(tokens) if t.kind == "id" and t.value.startswith("rcgen_")]
  for k, i in enumerate(starts):
    end = starts[k + 1] if k + 1 < len(starts) else len(tokens)
    p = Parser(tokens[i + 1:end], {}, None)
    name = tokens[i].value[len("rcgen_"):]
    try:
      consts[name] = p.expr()
    except RcError:
      raise RcError("%s is not a number in the stub headers" % name)
    if p.peek() is not None:
      raise RcError("%s is not a number in the stub headers" % name)
  missing = set(CONSTANT_NAMES) - set(consts)
  if missing:
    raise RcError("missing in the stub headers: %s" % " ".join(sorted(missing)))
  return consts


def repo_relative(path, cache={}):
  if path.startswith("<"):
    return path
  if path not in cache:
    full = os.path.normpath(os.path.abspath(path))
    cache[path] = os.path.relpath(full, ROOT) if full.startswith(ROOT + os.sep) else full
  return cache[path]


def resolve_file(name, script_dir, rc_file):
  """A file named in a resource script: the resource compiler looks in its
  working directory, which is the directory of the top-level script; the
  directory of the script that names the file is the fallback."""
  candidates = [os.path.join(script_dir, name)]
  if not rc_file.startswith("<"):
    base = rc_file if os.path.isabs(rc_file) else os.path.join(ROOT, rc_file)
    candidates.append(os.path.join(os.path.dirname(base), name))
  for c in candidates:
    if os.path.isfile(c):
      return os.path.normpath(c)
  return os.path.normpath(candidates[0])


def parse_file(path, defines=(), cc=None, depfile=None, deptarget=None):
  consts = read_constants(cc)
  res = Resources()
  tokens = tokenize(preprocess(path, defines, cc, depfile, deptarget), res.symbols)
  # paths in diagnostics and tables are relative to the repository
  for t in tokens:
    t.file = repo_relative(t.file)
  parser = Parser(tokens, consts, res)
  parser.script_dir = os.path.dirname(os.path.abspath(path))
  parser.parse()
  return res


# ---------- C++ output ----------

def cpp_wstring(s):
  out = ['L"']
  for ch in s:
    o = ord(ch)
    if ch == "\\":
      out.append("\\\\")
    elif ch == '"':
      out.append('\\"')
    elif ch == "\n":
      out.append("\\n")
    elif ch == "\t":
      out.append("\\t")
    elif ch == "\r":
      out.append("\\r")
    elif o < 0x20 or o == 0x7f:
      out.append("\\%03o" % o)
    elif o < 0x80:
      out.append(ch)
    elif o < 0x10000:
      out.append("\\u%04x" % o)
    else:
      out.append("\\U%08x" % o)
  out.append('"')
  return "".join(out)


def cpp_string(s):
  return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def generate_cpp(res, script):
  out = ["// Generated by linux/tools/rcgen.py from %s. Do not edit.\n" % script,
         '#include "RcTables.h"\n',
         "namespace NRc {\n"]

  for id in sorted(res.dialogs):
    d = res.dialogs[id]
    out.append("\n// %s (%s)\n" % (d.name or id, d.source))
    out.append("static const CControl kControls_%d[] =\n{\n" % id)
    for ctl in d.controls:
      out.append("  { %s, %s, %d, %d, %d, %d, %d, 0x%08x, 0x%08x },\n" % (
          cpp_string(ctl.cls), cpp_wstring(ctl.text), ctl.id & 0xFFFFFFFF,
          ctl.x, ctl.y, ctl.w, ctl.h, ctl.style, ctl.exstyle))
    if not d.controls:
      out.append("  { NULL, NULL, 0, 0, 0, 0, 0, 0, 0 }\n")
    out.append("};\n")

  out.append("\nstatic const CDialog kDialogs[] =\n{\n")
  if not res.dialogs:
    out.append("  { 0, NULL, NULL, false, 0, 0, 0, 0, 0, 0, NULL, NULL, 0, 0, 0, NULL, 0 }\n")
  for id in sorted(res.dialogs):
    d = res.dialogs[id]
    out.append("  { %d, %s, %s, %s, %d, %d, %d, %d, 0x%08x, 0x%08x, %s, %s, %d, %d, %d, kControls_%d, %d },\n" % (
        id, cpp_string(d.name or ""), cpp_string(d.source.rsplit(":", 1)[0]),
        "true" if d.extended else "false", d.x, d.y, d.w, d.h, d.style, d.exstyle,
        cpp_wstring(d.caption), cpp_wstring(d.font_name), d.font_size,
        d.font_weight, 1 if d.font_italic else 0, id, len(d.controls)))
  out.append("};\n")

  for id in sorted(res.menus):
    m = res.menus[id]
    out.append("\nstatic const CMenuItem kMenuItems_%d[] =\n{\n" % id)
    for it in m.items:
      out.append("  { %s, %d, 0x%x, 0x%x, %d, %s },\n" % (
          cpp_wstring(it.text), it.id & 0xFFFFFFFF, it.type, it.state, it.level,
          "true" if it.popup else "false"))
    out.append("};\n")
  out.append("\nstatic const CMenu kMenus[] =\n{\n")
  for id in sorted(res.menus):
    m = res.menus[id]
    out.append("  { %d, %s, kMenuItems_%d, %d },\n" % (
        id, "true" if m.extended else "false", id, len(m.items)))
  if not res.menus:
    out.append("  { 0, false, NULL, 0 }\n")
  out.append("};\n")

  for id in sorted(res.accels):
    out.append("\nstatic const CAccel kAccels_%d[] =\n{\n" % id)
    for a in res.accels[id]:
      out.append("  { 0x%02x, 0x%04x, %d },\n" % (a.flags, a.key, a.cmd))
    out.append("};\n")
  out.append("\nstatic const CAccelTable kAccelTables[] =\n{\n")
  for id in sorted(res.accels):
    out.append("  { %d, kAccels_%d, %d },\n" % (id, id, len(res.accels[id])))
  if not res.accels:
    out.append("  { 0, NULL, 0 }\n")
  out.append("};\n")

  out.append("\nstatic const CString kStrings[] =\n{\n")
  if not res.strings:
    out.append("  { 0, NULL }\n")
  for id in sorted(res.strings):
    out.append("  { %d, %s },\n" % (id, cpp_wstring(res.strings[id])))
  out.append("};\n")

  for id in sorted(res.icons):
    path = res.icons[id]
    try:
      with open(path, "rb") as f:
        data = f.read()
    except OSError as e:
      raise RcError("icon %d: cannot read %s: %s" % (id, path, e.strerror))
    out.append("\n// %s\nstatic const Byte kIcon_%d[] =\n{\n" % (repo_relative(path), id))
    for i in range(0, len(data), 16):
      out.append("  " + ",".join("0x%02x" % b for b in data[i:i + 16]) + ",\n")
    out.append("};\n")
  out.append("\nstatic const CIcon kIcons[] =\n{\n")
  if not res.icons:
    out.append("  { 0, NULL, 0 }\n")
  for id in sorted(res.icons):
    out.append("  { %d, kIcon_%d, sizeof(kIcon_%d) },\n" % (id, id, id))
  out.append("};\n")

  out.append("\nextern const CResources g_Resources =\n{\n"
             "  kDialogs, %d,\n  kMenus, %d,\n  kAccelTables, %d,\n  kStrings, %d,\n  kIcons, %d\n};\n" % (
                 len(res.dialogs), len(res.menus), len(res.accels), len(res.strings), len(res.icons)))
  out.append("\n}\n")
  return "".join(out)


def dump(res):
  for id in sorted(res.dialogs):
    d = res.dialogs[id]
    print("DIALOG%s %d %s %s %r %d,%d,%d,%d style=0x%08x font=%r,%d" % (
        "EX" if d.extended else "", id, d.name, d.source, d.caption,
        d.x, d.y, d.w, d.h, d.style, d.font_name, d.font_size))
    for c in d.controls:
      print("  %-15s %-18s %6d %4d %4d %4d %4d 0x%08x %r" % (
          c.statement, c.cls, c.id, c.x, c.y, c.w, c.h, c.style, c.text))
  for id in sorted(res.menus):
    print("MENU%s %d" % ("EX" if res.menus[id].extended else "", id))
    for it in res.menus[id].items:
      print("  %s%s %r id=%d type=0x%x state=0x%x" % (
          "  " * it.level, "POPUP" if it.popup else "ITEM", it.text, it.id, it.type, it.state))
  for id in sorted(res.accels):
    print("ACCELERATORS %d" % id)
    for a in res.accels[id]:
      print("  key=0x%x cmd=%d flags=0x%x" % (a.key, a.cmd, a.flags))
  for id in sorted(res.strings):
    print("STRING %d %r" % (id, res.strings[id]))
  for id in sorted(res.icons):
    print("ICON %d %s" % (id, repo_relative(res.icons[id])))


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("script", help="resource script (.rc)")
  ap.add_argument("--cpp", help="write the C++ tables to this file")
  ap.add_argument("--depfile", help="write a make-style dependency file")
  ap.add_argument("--dump", action="store_true", help="print the parsed resources")
  ap.add_argument("-D", dest="defines", action="append", default=[], help="preprocessor macro")
  ap.add_argument("--cc", help="C compiler used as the preprocessor")
  args = ap.parse_args()
  try:
    res = parse_file(args.script, args.defines, args.cc,
                     args.depfile, args.cpp or "rc")
  except RcError as e:
    print("rcgen: %s" % e, file=sys.stderr)
    return 1
  if args.dump:
    dump(res)
  if args.cpp:
    script = os.path.relpath(os.path.abspath(args.script), ROOT)
    try:
      text = generate_cpp(res, script)
    except RcError as e:
      print("rcgen: %s" % e, file=sys.stderr)
      return 1
    if args.depfile and res.icons:
      # the embedded icons are inputs too
      with open(args.depfile, encoding="utf-8") as f:
        dep = f.read().rstrip()
      dep += "".join(" \\\n " + p.replace(" ", "\\ ") for p in sorted(set(res.icons.values())))
      with open(args.depfile, "w", encoding="utf-8") as f:
        f.write(dep + "\n")
    tmp = tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False,
                                      dir=os.path.dirname(os.path.abspath(args.cpp)))
    with tmp:
      tmp.write(text)
    umask = os.umask(0)
    os.umask(umask)
    os.chmod(tmp.name, 0o666 & ~umask)
    os.replace(tmp.name, args.cpp)
  return 0


if __name__ == "__main__":
  sys.exit(main())
