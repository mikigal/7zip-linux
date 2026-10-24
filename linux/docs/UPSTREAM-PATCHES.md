# Upstream patch inventory

Every edit to upstream files (`C/`, `CPP/`, `Asm/`, `DOC/`) made for the Linux
port is listed here. Each patch is its own commit, is kept minimal, and is
guarded with `#ifndef _WIN32` / `#ifdef _WIN32` or `#ifdef Z7_QT_GUI`, so the
Windows build is unchanged. When merging a new upstream release, check that
every patch below still applies and is still needed.

| File | Guard | Purpose |
|---|---|---|
| `CPP/7zip/UI/Agent/Agent.cpp` | `#ifdef _WIN32` / `#else` | POSIX `CFileInfo` has no `Attrib`/`IsDevice` (use `GetWinAttrib()` and `S_ISCHR`/`S_ISBLK`); skip `IsAltStreamPrefixWithColon()`, which exists only on Windows. |
| `CPP/7zip/UI/Agent/AgentOut.cpp` | `#ifdef _WIN32` / `#else` | No alternate streams on POSIX: skip `IsAltStreamPrefixWithColon()` and `CDirItem::IsAltStream`; `CreateFolder()` uses `SetAsDir()` instead of `Attrib`. The current time goes through `CFiTime`/`GetCurUtc_FiTime()` unguarded: on Windows these are `FILETIME` and the `GetCurUtcFileTime` macro, so the Windows code is token-for-token the same. |
| `CPP/7zip/UI/Agent/ArchiveFolderOut.cpp` | `#ifdef _WIN32` / `#else` | POSIX `CDirEntry` has no `IsDir()` (use `CEnumerator::DirEntry_IsDir()`), and there is no read-only attribute to clear before `RemoveDir()`. |
