# 7-Zip Linux GUI port: working conventions

This repo is a fork of upstream 7-Zip (`ip7z/7zip`). On top of it we build a
native Linux/Wayland GUI (`7zFM`, `7zG`) that keeps the original 7-Zip design,
rendered in a dark theme. The full plan and phase checklist are in
`linux/docs/PLAN.md`; keep it up to date as work lands.

## Language
- Code, identifiers, comments, commit messages and repository docs are always
  in English.
- Chat with the user is in Polish.

## Where code goes
- Linux-specific code lives under `linux/` (`platform/`, `shim/`, `fm/`,
  `gui/`, `theme/`, `tools/`, `tests/`, `packaging/`).
- Upstream trees (`C/`, `CPP/`, `Asm/`, `DOC/`) are edited only when
  unavoidable:
  - keep each edit minimal and guarded with `#ifndef _WIN32` or
    `#ifdef Z7_QT_GUI`, so the Windows build is unchanged;
  - make each edit its own commit and record it in
    `linux/docs/UPSTREAM-PATCHES.md`;
  - to replace a Win32 header, use a redirect guard at the top of that header
    (see `PLAN.md`, "Upstream patch policy").
- Match upstream style in code that touches 7-Zip types: 2-space indent, no
  tabs, LF endings, braces on their own line for functions, `C`-prefixed
  classes, `UString`/`AString`, `CObjectVector`, `Z7_` macros.

## Git
- Work on the `linux-port` branch; `main` mirrors upstream.
- Commit small, self-contained steps as you go, each one building and passing
  tests.
- Never push without asking the user.

## Verification (do this yourself before every commit)
- Build the Linux targets and run the tests; `linux/tools/check.sh` becomes
  the single entry point once Phase 0 lands.
- If upstream files changed, also rebuild upstream `7zz`
  (`CPP/7zip/Bundles/Alone2`, `make -f makefile.gcc`).
- UI changes: render the dialog gallery, look at the PNGs, and update golden
  images only after a visual review.
- Wayland and desktop integration: run the scenario tests in a nested headless
  KWin (`kwin_wayland --virtual`).
- GUI tests must stay isolated: private D-Bus (`dbus-run-session`), a
  dedicated `WAYLAND_DISPLAY`, temporary `HOME`/`XDG_*_HOME`, no `DISPLAY`,
  and the whole process group killed at the end. Never touch the user's real
  session, configuration or files.
