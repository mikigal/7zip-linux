# Upstream patch inventory

Every edit to upstream files (`C/`, `CPP/`, `Asm/`, `DOC/`) made for the Linux
port is listed here. Each patch is its own commit, is kept minimal, and is
guarded with `#ifndef _WIN32` / `#ifdef _WIN32` or `#ifdef Z7_QT_GUI`, so the
Windows build is unchanged. When merging a new upstream release, check that
every patch below still applies and is still needed.

| File | Guard | Purpose |
|---|---|---|
| `CPP/7zip/UI/Agent/Agent.cpp` | `#ifdef _WIN32` / `#else` | POSIX `CFileInfo` has no `Attrib`/`IsDevice` (use `GetWinAttrib()` and `S_ISCHR`/`S_ISBLK`); skip `IsAltStreamPrefixWithColon()`, which exists only on Windows. |
