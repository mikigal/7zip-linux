# 7-Zip Linux port

Everything specific to the native Linux/Wayland GUI port of 7-Zip lives in this
directory. The plan and progress are tracked in [docs/PLAN.md](docs/PLAN.md).

## Requirements

- CMake 3.25+, Ninja, GCC or Clang, Python 3
- Qt 6 (Widgets, Svg, DBus, Test)
- ImageMagick (`magick`) and the Noto Sans font for the golden-image test
  (`render_gallery`; skipped when the font is missing)
- Optional, for the Windows reference pictures: Wine with `winegcc` and `wrc`
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
- `gui/7zG`: the 7-Zip dialogs program (add, extract, test, hash,
  benchmark);
- `tests/z7-gui-driver`: 7zG with its dialogs answered by a script (label
  `gui` for the offscreen tests, `wayland` for the nested KWin ones);
- `fm/7zFM`: the 7-Zip file manager;
- `tests/z7-fm-driver`: 7zFM worked by a script (keys, clicks, menu
  commands) that checks the panels;
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

## 7zG

`gui/` builds `7zG` from the upstream `GUI/GUI.cpp` and dialogs on top of the
shim; it takes the upstream command line:

```sh
build/linux/gui/7zG a -ad /tmp/new.7z README.md
```

```sh
build/linux/gui/7zG x -ad /tmp/new.7z -o/tmp/out
```

The `gui_7zg` test runs commands through `tests/z7-gui-driver`: a JSON script
answers the dialogs (control IDs come from the upstream resource headers
through `rcgen.py`), and the driver logs every dialog, message box and file
dialog with the state of its controls. Screenshots and logs of each scenario
are in `build/linux/test-artifacts/gui_7zg/`. See
[docs/PLAN.md](docs/PLAN.md), Phase 3.

## 7zFM

`fm/` builds `7zFM` from the upstream file manager sources (`FM.cpp`,
`App.cpp`, `Panel*.cpp`, the option pages) on top of the shim, which also
provides the main window parts: menus, toolbars, the address combo box, the
status bar and the list view. It takes the upstream command line (a folder
or an archive to open):

```sh
build/linux/fm/7zFM ~/Downloads
```

The `gui_7zfm` test runs scenarios through `tests/z7-fm-driver`: a JSON
script of keys, clicks, menu and toolbar commands and expectations on the
panels (path, items, focus, selection, columns, status bar), with the
dialogs answered as in `z7-gui-driver`. `wayland_7zfm` types into 7zFM in the
nested KWin through libei. Screenshots and logs are in
`build/linux/test-artifacts/gui_7zfm/` and `wayland_7zfm/`. See
[docs/PLAN.md](docs/PLAN.md), Phase 4.

## Windows reference pictures (Wine)

`tools/winref/` compares the port with the official 7-Zip release for
Windows, which is not in the repository. Download the release of the merged
upstream version, check its SHA-256 against the digest of the GitHub release
asset, and extract it with 7zz:

```sh
curl -L -o /tmp/7z2605-x64.exe https://www.7-zip.org/a/7z2605-x64.exe
```

```sh
build/linux/core/7zz x -obuild/windows-release/26.05 /tmp/7z2605-x64.exe
```

With the release configured, the `rcgen_windows_release` test compares the
compiled dialog templates of its 7zFM.exe and 7zG.exe with the rcgen tables,
control by control:

```sh
cmake -B build/linux -DZ7_WINDOWS_RELEASE=$PWD/build/windows-release/26.05
```

The reference pictures need Wine with `winegcc` and the nested KWin. `winref`
creates every template with the dialog manager of Wine (classic look, Tahoma
at 96 dpi); the script names the pictures like the gallery and puts each next
to the gallery picture in `compare/`:

```sh
cmake --build build/linux --target winref
```

```sh
linux/tools/winref/wine_reference.py --release build/windows-release/26.05 --helper build/linux/tools/winref/winref.exe --out build/linux/test-artifacts/wine_reference --gallery build/linux/test-artifacts/render_gallery/rendered
```

`--live` runs 7zG.exe of the release (compress, extract, benchmark) on small
test data and takes screenshots of its dialogs as the program fills them,
to compare with the `gui_7zg` screenshots:

```sh
linux/tools/winref/wine_reference.py --release build/windows-release/26.05 --out build/linux/test-artifacts/wine_reference --live
```

## Packages and releases

`cmake --install` puts 7zFM, 7zG, `7z-context-menu` and `Lang/` in the
program folder (`lib/7zip-linux`, with links in `bin`), and the file manager
extensions where their file managers look for them (Phase 7 in
[docs/PLAN.md](docs/PLAN.md)). The AUR packages `7zip-linux` (from source)
and `7zip-linux-bin` (prebuilt) are in `packaging/aur/`; a GitHub release
builds and publishes them, see [docs/RELEASE.md](docs/RELEASE.md).

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
with `s.spawn()`, `s.screenshot()`, `s.place_window()`, `s.kwin_script()` and
`s.kwin_query()` (the value of a KWin JavaScript expression, as JSON).
Inside the session, `tools/ei-input` injects input, for example
`ei-input move 100 200 click right type abc key ctrl+a drag 10 10 500 10`.
