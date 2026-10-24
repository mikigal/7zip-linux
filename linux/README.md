# 7-Zip Linux port

Everything specific to the native Linux/Wayland GUI port of 7-Zip lives in this
directory. The plan and progress are tracked in [docs/PLAN.md](docs/PLAN.md).

## Requirements

- CMake 3.25+, Ninja, GCC or Clang, Python 3
- Qt 6 (Widgets, Svg, DBus, Test)
- ImageMagick (`magick`) and the Noto Sans font for the golden-image test
  (`render_gallery`; skipped when the font is missing)
- For the Wayland tests: `kwin_wayland`, `spectacle`, ImageMagick (`magick`),
  `libei` 1.x, and `dbus-python` with PyGObject

## Build and test

```sh
linux/tools/check.sh               # configure, build, run all tests
linux/tools/check.sh --no-wayland  # skip tests that need the nested KWin
linux/tools/check.sh --asan        # AddressSanitizer + UBSan build in build/linux-asan
```

The build goes to `build/linux`. It contains:

- `core/7zz`: console 7-Zip built from the CMake core library;
- `oracle/7zz`: the same program built by the upstream makefile, used as a
  test oracle;
- `tools/ei-input`: the input injection helper;
- `tests/z7-probe`: the test window;
- `tests/z7-test-*`: the unit tests (QtTest, label `unit`);
- `tools/z7-gallery`: renders every dialog with the dark theme;
- `platform/RcFm.cpp`, `platform/RcGui.cpp`: resource tables generated from
  the upstream `.rc` files by `tools/rcgen.py`.

Test logs and screenshots go to `build/linux/test-artifacts/`. Unit tests run
with `HOME` and `XDG_*_HOME` in `test-artifacts/unit-home`.

## Platform layer

`platform/` holds what the shared upstream UI code (UI/Common, UI/Agent and
the non-window parts of UI/FileManager) needs on Linux: the Win32 names it
uses (`Win32Compat.h`, force-included), the registry on top of
`$XDG_CONFIG_HOME/7-Zip/7-Zip.ini`, resource strings, the locale, and the
POSIX file system and root folders. Programs link it with
`z7_link_ui(<target> FM|GUI)`. See [docs/PLAN.md](docs/PLAN.md), Phase 1.

To look at what rcgen reads from a resource script:

```sh
linux/tools/rcgen.py --dump CPP/7zip/Bundles/Fm/resource.rc
```

## Dialogs: the Win32 shim and the dark theme

`shim/` implements over Qt Widgets the Win32 user32 and comctl32 subset that
the upstream `NWindows` wrappers call (`shim/win32/windows.h`, `winuser.h`,
`CommCtrl.h`, `prsht.h`). The wrappers, `LangUtils.cpp` and the dialog sources
compile unchanged against it (`z7_gui`, with `windows.h` force-included).
Dialogs are built from the rcgen templates in dialog units. `theme/` holds
the Fusion-based dark style and the SVG toolbar icons. See
[docs/PLAN.md](docs/PLAN.md), Phase 2.

To look at the dialogs, render them to PNG files:

```sh
QT_QPA_PLATFORM=offscreen build/linux/tools/z7-gallery --out /tmp/gallery
QT_QPA_PLATFORM=offscreen build/linux/tools/z7-gallery --out /tmp/gallery --only IDD_COMPRESS
```

The `render_gallery` test compares the gallery with
`tests/render/golden/`. When dialogs change on purpose, look at the new
pictures in `build/linux/test-artifacts/render_gallery/rendered/` and then
update the golden ones:

```sh
linux/tests/render/test_gallery.py --gallery build/linux/tools/z7-gallery --artifacts build/linux/test-artifacts/render_gallery --update
```

## Upstream source lists

`cmake/UpstreamSources.cmake` is generated from the upstream makefiles. After
merging a new upstream release, run:

```sh
linux/tools/upstream_sources.py
```

The `upstream_sources_current` test fails until the file is regenerated.

## Wayland test session

`tools/wlsession.py` runs programs in an isolated, headless, nested KWin. The
session has its own D-Bus, `XDG_RUNTIME_DIR`, `HOME` and `XDG_*_HOME`, and no
`DISPLAY`. Every process it started is killed at the end. For example:

```sh
linux/tools/wlsession.py --artifacts /tmp/wl -- build/linux/tests/z7-probe
```

Python tests use it as a context manager (`with wlsession.Session() as s:`),
with `s.spawn()`, `s.screenshot()`, `s.place_window()` and `s.kwin_script()`.
Inside the session, `tools/ei-input` injects input, for example
`ei-input move 100 200 click right type abc key ctrl+a drag 10 10 500 10`.
