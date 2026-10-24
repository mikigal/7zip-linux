#!/usr/bin/env python3
"""Generate linux/cmake/UpstreamSources.cmake from the upstream GNU makefiles.

The upstream build (CPP/7zip/**/makefile.gcc + CPP/7zip/7zip_gcc.mak) is the
source of truth for which files make up the 7-Zip core. This script evaluates
the small subset of GNU make used there (variables, include, ifdef/ifndef/
ifeq/ifneq/else/endif and "$O/name.o: source" rules) for a native Linux build
without assembler, and writes the resulting source lists as CMake variables.

Usage:
  upstream_sources.py            regenerate the CMake file
  upstream_sources.py --check    exit with status 1 if the file is out of date
"""

import argparse
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTPUT = os.path.join(ROOT, "linux", "cmake", "UpstreamSources.cmake")

# Upstream programs whose object lists we import: name -> makefile.
PROGRAMS = {
    "7ZZ": "CPP/7zip/Bundles/Alone2/makefile.gcc",
}

# Object-list variables of each program that are exported as separate groups,
# so the Linux build can reuse them (for example the archive core in 7zFM).
GROUPS = ["ARC_OBJS", "SYS_OBJS", "UI_COMMON_OBJS", "CONSOLE_OBJS"]

OBJ_PREFIX = "@O@/"
# "$(NAME)" or a single-letter reference such as "$O" ("$<" and "$@" are kept).
VAR_REF = re.compile(r"\$\(([A-Za-z0-9_]+)\)|\$([A-Za-z])")
FUNC_REF = re.compile(r"\$\((shell|basename|notdir|dir|wildcard|subst)\s[^)]*\)")


class MakeError(Exception):
  pass


class MakeEval:
  def __init__(self, cwd, defines):
    self.cwd = cwd
    self.vars = dict(defines)
    self.rules = {}  # object name -> (source path relative to cwd, extra flags)

  def expand(self, text, depth=0):
    if depth > 50:
      raise MakeError("recursive variable expansion: " + text)
    text = FUNC_REF.sub("", text)
    def repl(m):
      return self.expand(self.vars.get(m.group(1) or m.group(2), ""), depth + 1)
    return VAR_REF.sub(repl, text)

  def defined(self, name):
    return self.expand(self.vars.get(name, "")).strip() != ""

  def parse_file(self, path):
    with open(path, encoding="utf-8", errors="replace") as f:
      raw = f.read().split("\n")
    lines = []
    buf = ""
    for line in raw:
      if line.endswith("\\"):
        buf += line[:-1] + " "
        continue
      lines.append(buf + line)
      buf = ""
    if buf:
      lines.append(buf)

    # Stack of (branch_active, any_branch_taken, parent_active).
    stack = []
    active = True
    rule_target = None
    for line in lines:
      if line.startswith("\t"):
        if active and rule_target is not None:
          self._add_recipe(rule_target, line.strip())
        continue
      stripped = line.split("#", 1)[0].strip()
      if not stripped:
        continue
      rule_target = None
      word = stripped.split(None, 1)[0]
      rest = stripped[len(word):].strip()

      if word in ("ifdef", "ifndef", "ifeq", "ifneq"):
        cond = self._condition(word, rest) if active else False
        stack.append((cond, cond, active))
        active = active and cond
        continue
      if word == "else":
        if not stack:
          raise MakeError("else without if in " + path)
        _, taken, parent = stack.pop()
        branch = parent and not taken
        if rest:  # "else ifdef X"
          sub = rest.split(None, 1)
          branch = branch and self._condition(sub[0], sub[1] if len(sub) > 1 else "")
        stack.append((branch, taken or branch, parent))
        active = branch
        continue
      if word == "endif":
        if not stack:
          raise MakeError("endif without if in " + path)
        _, _, parent = stack.pop()
        active = parent
        continue
      if not active:
        continue

      if word in ("include", "-include"):
        for inc in self.expand(rest).split():
          inc_path = os.path.normpath(os.path.join(self.cwd, inc))
          if os.path.exists(inc_path):
            self.parse_file(inc_path)
          elif word == "include":
            raise MakeError("missing include " + inc_path)
        continue

      m = re.match(r"^([A-Za-z0-9_]+)\s*(\+=|:=|\?=|=)\s*(.*)$", stripped)
      if m:
        name, op, value = m.groups()
        if op == "+=":
          self.vars[name] = (self.vars.get(name, "") + " " + value).strip()
        elif op == "?=":
          self.vars.setdefault(name, value)
        elif op == ":=":
          self.vars[name] = self.expand(value)
        else:
          self.vars[name] = value
        continue

      m = re.match(r"^\$O/([A-Za-z0-9_]+)\.o\s*:\s*(.*)$", stripped)
      if m:
        prereqs = m.group(2).split()
        if prereqs and m.group(1) not in self.rules:
          self.rules[m.group(1)] = [prereqs[0], None]
          rule_target = m.group(1)
        continue
      # Other rules and directives are irrelevant for the source lists.

    if stack:
      raise MakeError("unterminated conditional in " + path)

  def _condition(self, word, rest):
    if word in ("ifdef", "ifndef"):
      res = self.defined(rest.strip())
      return res if word == "ifdef" else not res
    m = re.match(r"^\((.*),(.*)\)$", rest)
    if not m:
      raise MakeError("unsupported conditional: %s %s" % (word, rest))
    a = self.expand(m.group(1)).strip()
    b = self.expand(m.group(2)).strip()
    return (a == b) if word == "ifeq" else (a != b)

  def _add_recipe(self, target, recipe):
    entry = self.rules[target]
    if entry[1] is not None:
      return
    m = re.match(r"^\$\((CC|CXX)\)\s+\$\((CFLAGS|CXXFLAGS)\)\s+(.*)\$<\s*$", recipe)
    entry[1] = self.expand(m.group(3)).split() if m else []

  def objects(self, var):
    names = []
    for tok in self.expand(self.vars.get(var, "")).split():
      if not tok.startswith(OBJ_PREFIX) or not tok.endswith(".o"):
        raise MakeError("unexpected token in %s: %s" % (var, tok))
      names.append(tok[len(OBJ_PREFIX):-2])
    return names

  def source(self, obj):
    if obj not in self.rules:
      raise MakeError("no rule for object " + obj)
    rel = self.rules[obj][0]
    return os.path.relpath(os.path.normpath(os.path.join(self.cwd, rel)), ROOT)


def evaluate(makefile):
  path = os.path.join(ROOT, makefile)
  ev = MakeEval(os.path.dirname(path), {"O": "@O@", "CC": "cc", "CXX": "g++"})
  ev.parse_file(path)
  return ev


def cmake_list(name, items):
  out = ["set(%s" % name]
  out += ["  %s" % i for i in items]
  out.append(")")
  return "\n".join(out) + "\n"


def generate():
  parts = [
      "# Generated by linux/tools/upstream_sources.py from the upstream GNU makefiles.\n"
      "# Do not edit. Regenerate after merging a new upstream release.\n"
  ]
  for prog, makefile in PROGRAMS.items():
    ev = evaluate(makefile)
    objs = ev.objects("OBJS")
    parts.append("\n# %s (%s)\n" % (prog, makefile))
    parts.append(cmake_list("Z7_%s_SOURCES" % prog, [ev.source(o) for o in objs]))
    defines = ev.expand(ev.vars.get("LOCAL_FLAGS", "")).split()
    parts.append(cmake_list("Z7_%s_FLAGS" % prog, defines))
    for group in GROUPS:
      if group in ev.vars:
        parts.append(cmake_list("Z7_%s_%s" % (prog, group.replace("_OBJS", "_SOURCES")),
                                [ev.source(o) for o in ev.objects(group)]))
    extra = []
    for o in objs:
      flags = ev.rules[o][1] or []
      if flags:
        extra.append("%s|%s" % (ev.source(o), " ".join(flags)))
    parts.append(cmake_list("Z7_%s_SOURCE_FLAGS" % prog, extra))
  return "".join(parts)


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--check", action="store_true", help="verify the generated file is current")
  args = ap.parse_args()
  text = generate()
  if args.check:
    try:
      with open(OUTPUT, encoding="utf-8") as f:
        current = f.read()
    except FileNotFoundError:
      current = ""
    if current != text:
      print("%s is out of date; run linux/tools/upstream_sources.py" % os.path.relpath(OUTPUT, ROOT))
      return 1
    return 0
  os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
  with open(OUTPUT, "w", encoding="utf-8") as f:
    f.write(text)
  return 0


if __name__ == "__main__":
  sys.exit(main())
