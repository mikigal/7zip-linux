#!/usr/bin/env python3
"""Golden-image test of the dialog gallery.

Renders every dialog template, the options property sheet and the toolbar
icons with z7-gallery (offscreen, dark theme, a fixed font) and compares each
PNG with linux/tests/render/golden/ using ImageMagick. A picture fails when
its size differs or when more than MAX_DIFF_PIXELS of its pixels differ by
more than FUZZ: the rendering is deterministic, and a small local change (a
moved line, a header border) must not hide in a large picture. Differences
are written to the artifacts directory.

When the pictures change on purpose, look at the new ones in the artifacts
directory first, then copy them over the golden ones:

  test_gallery.py --gallery build/linux/tools/z7-gallery --artifacts DIR --update

The font must be installed (the gallery exits with 77 otherwise, and the test
is skipped): the golden pictures depend on it.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
GOLDEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "golden")
FONT = "Noto Sans,10"
FUZZ = "5%"
MAX_DIFF_PIXELS = 16
SKIP = 77


def render(gallery, out):
  # a private configuration: no user fonts settings, styles or 7-Zip settings
  home = tempfile.mkdtemp(prefix="z7-gallery-home-")
  env = dict(os.environ)
  env.pop("DISPLAY", None)
  env.pop("WAYLAND_DISPLAY", None)
  env.update({
      "QT_QPA_PLATFORM": "offscreen",
      "HOME": home,
      "XDG_CONFIG_HOME": os.path.join(home, "config"),
      "XDG_DATA_HOME": os.path.join(home, "data"),
      "XDG_CACHE_HOME": os.path.join(home, "cache"),
      "XDG_STATE_HOME": os.path.join(home, "state"),
      "QT_SCALE_FACTOR": "1",
      "QT_FONT_DPI": "96",
  })
  try:
    res = subprocess.run([gallery, "--out", out, "--font", FONT], env=env,
                         capture_output=True, text=True, timeout=120)
  finally:
    shutil.rmtree(home, ignore_errors=True)
  if res.returncode == SKIP:
    print(res.stderr.strip())
    sys.exit(SKIP)
  if res.returncode != 0:
    print(res.stdout + res.stderr)
    sys.exit("z7-gallery failed with exit code %d" % res.returncode)


def size(png):
  res = subprocess.run(["magick", "identify", "-format", "%w %h", png],
                       capture_output=True, text=True, check=True)
  w, h = res.stdout.split()
  return int(w), int(h)


def differing_pixels(a, b, diff):
  # "compare" prints the metric on stderr and exits with 1 when the images differ
  res = subprocess.run(["magick", "compare", "-metric", "AE", "-fuzz", FUZZ, a, b, diff],
                       capture_output=True, text=True)
  if res.returncode > 1:
    raise RuntimeError(res.stderr)
  return float(res.stderr.split()[0])


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--gallery", required=True, help="the z7-gallery program")
  ap.add_argument("--artifacts", required=True, help="directory for the pictures and differences")
  ap.add_argument("--update", action="store_true", help="replace the golden pictures")
  args = ap.parse_args()

  out = os.path.join(args.artifacts, "rendered")
  diffs = os.path.join(args.artifacts, "diff")
  for d in (out, diffs):
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
  render(args.gallery, out)
  rendered = sorted(f for f in os.listdir(out) if f.endswith(".png"))
  if not rendered:
    sys.exit("z7-gallery wrote no pictures")

  if args.update:
    os.makedirs(GOLDEN, exist_ok=True)
    for f in os.listdir(GOLDEN):
      if f.endswith(".png") and f not in rendered:
        os.remove(os.path.join(GOLDEN, f))
    for f in rendered:
      shutil.copyfile(os.path.join(out, f), os.path.join(GOLDEN, f))
    print("updated %d golden pictures in %s" % (len(rendered), os.path.relpath(GOLDEN, ROOT)))
    return 0

  golden = sorted(f for f in os.listdir(GOLDEN) if f.endswith(".png")) if os.path.isdir(GOLDEN) else []
  failures = []
  for f in sorted(set(golden) - set(rendered)):
    failures.append("%s: golden picture, but the gallery does not render it" % f)
  for f in rendered:
    a = os.path.join(GOLDEN, f)
    b = os.path.join(out, f)
    if not os.path.exists(a):
      failures.append("%s: no golden picture" % f)
      continue
    sa, sb = size(a), size(b)
    if sa != sb:
      failures.append("%s: size %dx%d, golden %dx%d" % (f, sb[0], sb[1], sa[0], sa[1]))
      continue
    n = differing_pixels(a, b, os.path.join(diffs, f))
    status = "ok" if n <= MAX_DIFF_PIXELS else "DIFFERS"
    print("%-32s %6d pixels differ %s" % (f, n, status))
    if n > MAX_DIFF_PIXELS:
      failures.append("%s: %d pixels differ, see %s" % (f, n, os.path.join(diffs, f)))
    else:
      os.remove(os.path.join(diffs, f))
  if failures:
    print("\n".join(["FAILED:"] + failures))
    print("Pictures: %s. After reviewing them, rerun with --update." % out)
    return 1
  print("%d pictures match" % len(rendered))
  return 0


if __name__ == "__main__":
  sys.exit(main())
