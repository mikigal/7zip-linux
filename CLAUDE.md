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
- This fork is built for Linux only; its Windows build does not matter.
  Upstream trees (`C/`, `CPP/`, `Asm/`, `DOC/`) are still edited only when
  unavoidable, because every changed line can conflict when a new upstream
  release is merged:
  - prefer a Linux-side solution (`linux/platform/Win32Compat.h`, shim
    headers, Linux implementations of upstream classes);
  - keep edits small; the file's own `#ifdef _WIN32` / `#else` style is
    usually the smallest diff, but replacing Windows code is fine too;
  - edits must keep the upstream console `7zz` (the test oracle that
    `check.sh` builds) building and working;
  - to replace a Win32 header, use a redirect guard at the top of that header
    (see `PLAN.md`, "Upstream patch policy");
  - commit upstream edits separately from `linux/` changes, with the reason
    in the commit message. Git is the inventory: see `PLAN.md`, Appendix B.
- Match upstream style in code that touches 7-Zip types: 2-space indent, no
  tabs, LF endings, braces on their own line for functions, `C`-prefixed
  classes, `UString`/`AString`, `CObjectVector`, `Z7_` macros.

## Git
- This repo is Linux-only: commit directly to `main`, with no feature branches.
- New upstream releases come from the `upstream` remote (`ip7z/7zip`) and are
  merged into `main`.
- Commit small, self-contained steps as you go, each one building and passing
  tests.
- Never push without asking the user.

## Verification (do this yourself before every commit)
- Run `linux/tools/check.sh`: it configures, builds (including the upstream
  `7zz` oracle through the upstream makefile, which also catches breakage in
  upstream files) and runs every test. It takes a few seconds incrementally.
- Use `--no-wayland` only for quick iterations; run the full suite before
  committing. Use `--asan` after larger changes in C++ code.
- Logs and screenshots are in `build/linux/test-artifacts/`. Look at the
  screenshots when a change affects what is on screen.
- Merge a new upstream release with `linux/tools/upstream_sync.sh` (it runs
  `upstream_sources.py` and `check.sh`; see `linux/docs/UPSTREAM.md`).
- UI changes: render the dialog gallery, look at the PNGs, and update golden
  images only after a visual review.
- Wayland and desktop integration: run the scenario tests in a nested headless
  KWin (`kwin_wayland --virtual`).
- GUI tests must stay isolated: private D-Bus (`dbus-run-session`), a
  dedicated `WAYLAND_DISPLAY`, temporary `HOME`/`XDG_*_HOME`, no `DISPLAY`,
  and the whole process group killed at the end. Never touch the user's real
  session, configuration or files.
