# 7-Zip Linux port

Everything specific to the native Linux/Wayland GUI port of 7-Zip lives in this
directory. The plan and progress are tracked in [docs/PLAN.md](docs/PLAN.md).

## Requirements

- CMake 3.25+, Ninja, GCC or Clang, Python 3
- Qt 6 (Widgets, DBus, Test)
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
- `tests/z7-probe`: the test window.

Test logs and screenshots go to `build/linux/test-artifacts/`.

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
