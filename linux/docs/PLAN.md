# 7-Zip for Linux: GUI port plan

This document is the working plan for porting the 7-Zip GUI (the `7zFM` file
manager and the `7zG` dialogs) to Linux. Tick the checkboxes as work lands, and
update a section when a decision changes.

## 1. Goals

- Provide the same features as the Windows GUI, except for Windows-only
  integrations that make no sense on Linux (see Appendix A).
- Keep the original 7-Zip design (layouts, toolbar, panels, dialogs, menus),
  rendered in a **dark theme**.
- Run as a native Wayland client (no XWayland) and integrate with the
  compositor and the desktop: drag and drop to and from file managers,
  clipboard, activation, taskbar progress, MIME associations and file manager
  context menus.
- Stay mergeable with upstream: this repo is a fork of `ip7z/7zip`, and new
  upstream releases must be easy to bring in.

Non-goals: reimplementing the Win32 API in general, Wine, X11-only features
(for example XDND Direct Save), or keeping this fork buildable for Windows: it
is built and used on Linux only.

## 2. Findings from the code analysis

| Layer | State on Linux |
|---|---|
| `C/`, `CPP/7zip/{Archive,Compress,Crypto,Common}`, `CPP/7zip/UI/Common` | Already portable; upstream builds `7zz` with `makefile.gcc`. `LoadCodecs.cpp` can load `7z.so`. |
| `CPP/7zip/UI/Agent` (an archive exposed as a folder through `IFolder.h`) | Compiles with about 15 trivial errors (`INVALID_FILE_ATTRIBUTES`, `HMODULE`, `UINT64`, `MyLoadString`). `IFolder.h` is a clean model interface and the main reusable asset. |
| `FSFolder`, `FSDrives`, `RootFolder`, `AltStreamsFolder`, `NetFolder` | Windows file-system model; needs a POSIX implementation. |
| Settings (`ZipRegistry.cpp`, `ViewSettings.cpp`, `RegistryUtils.cpp`) | Stored in the registry through `NWindows::NRegistry::CKey`. |
| Dialogs (65 templates in `.rc`, about 20 distinct dialogs) | Logic goes through the `NWindows::NControl` wrappers, not raw messages (for example `CompressDialog.cpp`: 3821 lines, 2 `SendMsg` calls). |
| `CPanel` / `CApp` | Tightly bound to Win32 (`CPanel : CWindow2`, `WM_NOTIFY`, OLE drag and drop in `PanelDrag.cpp`, shell `IContextMenu`). The panel logic uses only 45 `CListView` methods. |
| Threading | Worker threads open modal dialogs (for example `CExtractCallbackImp::AskOverwrite`). Win32 allows that; Qt does not. |

`.rc` templates expand to plain arithmetic when run through the C
preprocessor with stub `windows.h` and `CommCtrl.h` headers. The control
vocabulary is small: `LTEXT`, `RTEXT`, `CTEXT`, `EDITTEXT`, `COMBOBOX`,
`PUSHBUTTON`, `DEFPUSHBUTTON`, `GROUPBOX`, `ICON` and `CONTROL` with the
classes `Button` (check box and radio button), `SysListView32`,
`msctls_progress32`, `msctls_updown32` and `Static`. The resources also contain
one `MENUEX`, one `ACCELERATORS` table and several `STRINGTABLE`s.

## 3. Architecture

```
  7zFM (upstream FM.cpp, App, Panel*)    7zG (upstream GUI.cpp, dialogs)
        |                                     |
  linux/fm: entry point, Linux stand-ins for  |
  shell-only parts (drag and drop, ...)       |
        |                                     |
  linux/shim: Win32 user32 subset over Qt <---+   (dialog logic and NWindows wrappers
  + dialog engine (layouts from rcgen tables)      from upstream, compiled as-is)
        |
  linux/platform: registry (INI), resource strings, POSIX FSFolder/RootFolder,
  process and thread helpers
        |
  upstream core: C/, CPP/Common, CPP/Windows (POSIX parts), CPP/7zip/**, UI/Common, UI/Agent
```

Key decisions:

1. **Toolkit: Qt 6 Widgets** with the `Fusion` style and a custom dark palette.
   Qt provides native Wayland (xdg-shell, fractional scaling, xdg-activation,
   xdg-decoration), pull-based drag and drop through `QMimeData::retrieveData`,
   and `QtDBus`.
2. **Dialogs keep upstream logic.** Upstream dialog sources (`CompressDialog.cpp`,
   `ExtractDialog.cpp` and the rest) and the `NWindows` wrappers they use
   (`Window.h`, `Control/Dialog.h`, `ComboBox.h`, `ListView.h`, `PropertyPage.h`
   and their `.cpp` files) compile unchanged against a Qt implementation of
   the Win32 user32 and comctl32 subset that the wrappers call (`linux/shim`).
   An `HWND` is a shim window object that owns a Qt widget; `SendMessage`
   runs the built-in procedure of the control class (`CB_*`, `EM_*`, `LVM_*`,
   ...), and the widgets' signals come back as `WM_COMMAND` and `WM_NOTIFY`
   with the Win32 notification codes, so `OnButtonClicked`, `OnCommand` and
   `OnNotify` work as on Windows. `PostMessage` is a queued Qt call and
   timers are `QTimer`s. Implementing the API under the wrappers, instead of
   replacing the wrapper headers, needs no upstream edits at all.
3. **Original layouts come from the `.rc` files.** The `linux/tools/rcgen.py`
   generator runs `cpp` over the upstream `.rc` files and generates C++ tables
   for dialogs, menus, accelerators and strings. At runtime the dialog engine
   creates the widgets and places them by converting dialog units (DLUs) to
   pixels with the font metrics, the way Windows does. Upstream layout changes
   then come in for free, and a per-dialog override hook covers cases where the
   generated layout does not fit.
4. **Worker-thread UI is marshalled in one place.** When `CModalDialog::Create`,
   `MessageBox` or a similar call runs off the GUI thread, the shim moves it to
   the GUI thread with a blocking queued call. The GUI thread never blocks on a
   worker.
5. **The main window and the panels are upstream code too.** (Changed in
   Phase 4; the first plan was new Qt views with the panel logic reused.)
   `FM.cpp`, `App.cpp`, all `Panel*.cpp` files, `MyLoadMenu.cpp` and the
   option pages compile against the same shim as the dialogs: it also has
   menus (`HMENU` over `QMenuBar`/`QMenu`), the toolbar, rebar and status
   bar, `ComboBoxEx`, and a list view over a `QAbstractTableModel` with
   `LVN_GETDISPINFO` callbacks, custom draw and label editing. Window
   procedures and subclassing work as on Windows: Qt input events become
   `WM_KEYDOWN`, `WM_CHAR` and mouse messages first, and Qt handles an event
   only when its message reaches `DefWindowProc()`. This reuses all the
   keyboard, selection, sorting and navigation behaviour of the original
   instead of rewriting it, with a handful of upstream edits; the parts that
   are shell-only on Windows (drag and drop, context menu registration,
   links, version control) have Linux stand-ins in `linux/fm`.
6. **One static core library.** Both binaries link the full codec and format
   set (like `Bundles/Fm`), and compress and extract run in-process from the
   file manager (`CompressCall2.cpp`). `7zG` stays a separate binary for file
   manager context menus. The upstream `7zz` is built with the upstream
   makefile and also serves as the test oracle.
7. **Settings** live in `$XDG_CONFIG_HOME/7-Zip/7-Zip.ini` behind a non-Windows
   `NRegistry::CKey` implementation, so `ZipRegistry.cpp` and `ViewSettings.cpp`
   stay unchanged. Writes are atomic (temporary file plus rename) and guarded
   with `flock`, because `7zFM` and `7zG` may write at the same time.

### Repository layout

```
CLAUDE.md                 working conventions for this repo
linux/
  CMakeLists.txt          build for everything Linux-specific (upstream makefiles untouched)
  docs/                   this plan, upstream-patch inventory, testing notes
  platform/               registry backend, resource strings, POSIX folders
  shim/                   NWindows::NControl over Qt, dialog engine, win32 subset headers
  fm/                     7zFM entry point and Linux stand-ins for shell-only parts
  gui/                    7zG entry point
  theme/                  dark palette, QSS, SVG icons
  tools/                  rcgen.py, Wayland test harness, input injection helper, check.sh
  tests/                  unit, render (golden image) and Wayland scenario tests
  packaging/              .desktop, MIME, service menus, PKGBUILD, AppStream
```

### Upstream patch policy

The Windows build of this fork does not matter, but merging new upstream
releases does: every changed upstream line can conflict.

- Edit upstream files (`C/`, `CPP/`) only when it cannot be avoided; prefer a
  Linux-side solution (force-included `Win32Compat.h`, shim headers, Linux
  implementations of upstream classes). Keep edits small. The file's own
  `#ifdef _WIN32` / `#else` style is usually the smallest diff, and in a merge
  conflict it shows the Windows and POSIX variants side by side; replacing
  Windows code outright is fine too.
- Edits must keep the upstream console `7zz` building and working: it is the
  test oracle, built by `check.sh` with the upstream makefile.
- Quoted relative includes (`#include "../../../Windows/Control/Dialog.h"`)
  cannot be shadowed through include paths. Headers whose Win32 content must be
  replaced therefore get a **redirect guard**: a few lines at the top that
  include the `linux/shim` (or `linux/fm`) replacement under `Z7_QT_GUI`, with
  the original body in the `#else` branch. Nothing else in the file changes.
- Angle-bracket system headers (`<CommCtrl.h>`, `<ShlObj.h>`) are shadowed by
  stub headers in `linux/shim/win32/` on the include path.
- Upstream edits are committed separately from changes under `linux/`, with
  the reason in the commit message. Git is the inventory (Appendix B).

## 4. Design: the original 7-Zip look, in dark

- **Layouts:** taken from the `.rc` files (DLU-based), so dialogs and the
  option pages look like the originals.
- **Main window:** menu bar, archive toolbar (Add, Extract, Test) and standard
  toolbar (Copy, Move, Delete, Info) with "Large buttons" and "Show buttons
  text" options, address combo with an up-folder button, details list with the
  original default columns, a multi-part status bar, and one or two panels
  (F9).
- **Style:** `Fusion` (compact metrics close to Win32) with a dark `QPalette`
  (window about `#202020`, base about `#191919`, text about `#e6e6e6`, accent
  highlight about `#0078d4`) and a small QSS for toolbar buttons, the list
  header, group boxes and status bar separators. Dark is the default; a
  light palette may be added later as an option because it is cheap.
- **Toolbar icons:** the original `.bmp` glyphs (magenta key `#ff00ff`) lose
  contrast on dark backgrounds, especially the blue Extract and navy Move
  icons. They are redrawn as SVGs with the same shapes and hues, adjusted for
  contrast, in the 24 px and 48×36 px variants.
- **File icons:** the 7-Zip archive icons from `CPP/7zip/Archive/Icons/*.ico`
  for archive types, and theme icons through `QMimeDatabase` and
  `QIcon::fromTheme` for everything else.
- **Window chrome:** server-side decorations on KWin (`xdg-decoration`) and Qt
  client-side decorations elsewhere.
- **Fidelity check:** when a mingw-w64 toolchain is available, the upstream
  `7zFM.exe` and `7zG.exe` are cross-built from the same commit, run under Wine
  in the test compositor, and captured as reference screenshots for
  side-by-side review.

## 5. Desktop integration (Wayland)

- **Identity:** the app ID is set through `QGuiApplication::setDesktopFileName`
  and must be a valid D-Bus name (open question, see Appendix C). It provides
  matching `.desktop` files and hicolor icons.
- **Windows:** no global positioning on Wayland, so only size and maximized
  state are restored. Dialogs get a transient parent. `XDG_ACTIVATION_TOKEN`
  from the launching file manager is honoured, which Qt does automatically.
- **Drag and drop in:** `text/uri-list` to local paths, then the existing
  logic: dropping on an archive panel adds files to the archive, dropping on a
  file-system panel copies or moves, and dropping on an archive file adds to
  that archive. Ctrl/Shift choose copy or move, and a drop menu (with Alt, or
  always if configured) replaces right-button dragging.
- **Drag and drop out:**
  1. KIO/Dolphin: the Ark protocol. Advertise
     `application/x-kde-ark-dndextract-service` and `-path`, and export
     `org.kde.ark.DndExtract.extractSelectedFilesTo(QString)` over D-Bus. The
     target directory then comes back at drop time and extraction goes there
     directly, with no temporary files. This matches Windows
     `7-Zip::SetTargetFolder`, and `libKF6KIOWidgets` 6.31 supports it.
  2. Other targets: `text/uri-list` with lazy extraction in `retrieveData`
     into a 7-Zip temporary directory, which stays alive until the panel leaves
     the archive or the app exits. Wayland transfers are pull-based, so
     extraction only happens on an actual drop.
  3. Between 7-Zip panels and windows: an internal MIME type (the equivalent of
     `7-Zip::SetTransfer` and `GetTransfer`) for direct archive-to-archive
     operations.
- **Clipboard:** copy and cut files as `text/uri-list`,
  `x-special/gnome-copied-files` and `application/x-kde-cutselection`. Pasting
  into an archive panel adds the files.
- **Open outside / Edit (F4):** watch the temporary file
  (`QFileSystemWatcher`) instead of waiting on the process, and offer "update
  archive" when it changes. "Open with…" uses the portal `OpenURI` with
  `ask=true`.
- **Taskbar progress:** `com.canonical.Unity.LauncherEntry` (supported by the
  Plasma task manager and Dash to Dock). A completion notification through
  `org.freedesktop.Notifications` is optional.
- **File dialogs:** `QFileDialog`, so the portal or KDE dialogs are used.
  Email uses `xdg-email --attach`.
- **Associations:** `.desktop` `MimeType=` entries. The System options page
  sets defaults through `mimeapps.list` (`xdg-mime default`).
- **File manager context menus:** a Dolphin service menu first, then a
  `KAbstractFileItemActionPlugin` for dynamic labels, then a Nautilus Python
  extension, Thunar custom actions and Nemo actions.

## 6. Testing strategy (autonomous)

Every change is verified locally before it is committed. A single entry point
`linux/tools/check.sh` runs the levels below; flags select the heavier ones.

| Level | What | How |
|---|---|---|
| L0 Build | Linux targets build warning-free (`-Werror` for `linux/`), Debug and ASan/UBSan presets | CMake + Ninja. Whenever upstream files are touched, also rebuild upstream `7zz` with `Bundles/Alone2/makefile.gcc`. |
| L1 Unit | Registry backend, rcgen parser (every template parses, every control type is known), POSIX folders (symlinks, permissions, Unicode and invalid UTF-8 names), Agent operations, `FolderModel` (`QAbstractItemModelTester`), shim dispatch and thread marshalling, lazy MIME extraction, Ark D-Bus adaptor | QtTest + ctest with `QT_QPA_PLATFORM=offscreen`. Archive results are checked against the `7zz` oracle (`7zz l -slt`, `7zz t`). |
| L2 Render | Every dialog and the main window render with fixed data | A gallery tool renders PNGs offscreen. Golden images are compared with ImageMagick `compare` within a tolerance. When images change on purpose, they are reviewed visually and then regenerated. |
| L2b GUI | Program commands through their dialogs (`7zG a/x/t/h/b`, later 7zFM operations), with passwords, overwrite questions, errors and cancel | `z7-gui-driver` runs the program offscreen with a JSON script that answers its dialogs, message boxes and file dialogs and logs what they show (label `gui`); archives are checked with the `7zz` oracle. |
| L3 Wayland | Real Wayland code paths: windows, activation, parenting, drag and drop with Dolphin and a probe app, clipboard, scaling | Nested headless KWin (`kwin_wayland --virtual`) under `dbus-run-session`, Spectacle screenshots, and input injection through libei (`org.kde.KWin.EIS.RemoteDesktop.connectToEIS`; fallback: `org_kde_kwin_fake_input`). Verified working: nested KWin 6.8.1 running Dolphin with a full-screen capture. |
| L4 Manual | Sign-off on the real Plasma session at milestones | A short checklist per phase, run by the user. |

Isolation rules for every GUI test:

- private D-Bus session;
- a dedicated Wayland socket name;
- `HOME` and `XDG_{CONFIG,DATA,CACHE,STATE}_HOME` pointing to a temporary
  directory;
- no `DISPLAY`;
- the whole process group is killed at the end.

Tests never touch the user's real session, configuration or files.

A commit is done when it builds, `check.sh` passes at the relevant level, and
any golden-image change has been reviewed.

## 7. Phases

### Phase 0: Foundations and test harness (done)
- [x] Add an `upstream` remote (`ip7z/7zip`). The repo is Linux-only, so all
      work is committed directly to `main`.
- [x] `CLAUDE.md` and this plan.
- [x] CMake: the `z7_core` OBJECT library from the upstream source lists.
      `tools/upstream_sources.py` evaluates the upstream GNU makefiles and
      writes them to `cmake/UpstreamSources.cmake`; ctest checks that the file
      is current. The 7zz object set matches the upstream build 1:1 (322
      objects).
- [x] Build of the `7zz` oracle with the upstream makefile, out of tree
      (`oracle_7zz` target), plus a CMake-built `7zz`. The `core_roundtrip`
      test checks both directions for 7z, 7z with encrypted headers, zip,
      zip AES, tar and wim, and that both binaries report identical formats
      and codecs.
- [x] `linux/tools/check.sh` as the single verification entry point.
- [x] `linux/tools/wlsession.py`: nested KWin, isolation, Spectacle
      screenshots, window placement through KWin scripting, cleanup through a
      marker environment variable. Smoke test with `z7-probe`.
- [x] `linux/tools/ei-input`: a libei helper (move, press, release, click,
      drag, key, type, wait). Smoke tests: a synthetic click and keys reach the
      probe, and a drag between two Wayland clients delivers `text/uri-list`.

Findings:
- GCC 16 reports `-Warray-bounds` false positives in upstream code, so
  upstream sources are built without `-Werror`; port code keeps `-Werror`.
- Qt 6.12 routes file drag and drop through the portal FileTransfer interface
  (`application/vnd.portal.filetransfer`). When the portal is missing, the
  receiver gets **empty** `text/uri-list` data (it does not fall back). The
  real document portal cannot mount FUSE in the sandbox, so the test session
  D-Bus-activates `tools/fake_filetransfer_portal.py` instead. Phase 6 must
  handle both paths, and 7-Zip's own drag sources must keep working when the
  portal is unavailable.
- Qt logs to journald unless `QT_FORCE_STDERR_LOGGING=1`; the harness sets it
  so logs land in the test artifacts.

Exit: `check.sh` passes, and an automatic screenshot plus a synthetic click
work in nested KWin.

### Phase 1: Platform layer (done)
- [x] `UI/Agent` builds on Linux (small upstream patches).
- [x] INI-backed `NRegistry::CKey`, with tests.
- [x] `rcgen.py`: dialogs, `MENUEX`, `ACCELERATORS` and `STRINGTABLE`s into
      generated C++ tables, with tests.
- [x] `MyLoadString` and `CLang` (`Lang/*.txt`) wired up. `LangUtils.cpp`
      itself builds in Phase 2 (see the findings).
- [x] POSIX `FSFolder`, `RootFolder` (`/`, home, mount points with
      size and free-space columns) and POSIX columns (permissions, user,
      group, symlink target, dotfiles as hidden), with tests through the
      `IFolderFolder` API.

Exit: a headless test browses file systems and nested archives through
`IFolderFolder`, and settings round-trip. Both are in `unit_folders` and
`unit_registry`; `check.sh` and `check.sh --asan` pass.

What is where (`linux/platform`):
- `Win32Compat.h` is force-included (`-include`) into the upstream sources of
  the `z7_ui` library (UI/Common, UI/Agent, ZipRegistry, RegistryUtils,
  ViewSettings, TextPairs). It adds the few Win32 names that POSIX
  `MyWindows.h` lacks (`HMODULE`, `HINSTANCE`, `UINT64`, `RECT`, an opaque
  `HWND`, `LANGID`, registry types) instead of patching upstream headers.
  The Phase 2 shim must define `struct HWND__`.
- `Registry.cpp` implements the unchanged `Windows/Registry.h` on top of
  `$XDG_CONFIG_HOME/7-Zip/7-Zip.ini` in a `.reg`-like typed format
  (`dword:`, `hex:`, `multi:`, escaped UTF-8 strings that round-trip any
  bytes). Only `HKEY_CURRENT_USER` is stored. Changes are applied in memory
  and written when the last open key closes: under `flock` on
  `7-Zip.ini.lock`, the file is reloaded if another process replaced it, the
  pending changes are replayed on top, and the result replaces the file
  through a temporary file, `fsync` and `rename`. Readers reload when the
  file's inode, size or time changes. `RegistryIni.h` has a path override and
  an explicit flush for tests.
- `tools/rcgen.py` generates `RcFm.cpp` (Bundles/Fm/resource.rc) and
  `RcGui.cpp` (UI/GUI/resource.rc) at build time with a depfile; each program
  links one of them (`z7_link_ui(<target> FM|GUI)`), and `RcTables.h`
  describes the tables. `linux/shim/win32/windows.h` and `CommCtrl.h` hold the
  Win32 constant values for both rcgen and the coming shim.
- `ResourceString.cpp` (`MyLoadString`), `Locale.cpp` (POSIX locale to
  `LANGID`), `Program.cpp` (program directory for `Lang/`,
  `MyGetModuleFileName`), `Globals.cpp` (`g_hInstance`, `g_IsNT`).
- `FSFolderPosix.cpp`, `RootFolderPosix.cpp` and `DirWatcher.cpp` (one shared
  inotify instance, so folders do not use up the per-user instance limit).
  The classes keep the upstream names (`NFsFolder::CFSFolder`,
  `CRootFolder`), so Phase 4 can redirect `FSFolder.h`, `RootFolder.h` and the
  `FSDrives` uses in the panel code to them.

Findings:
- Only three upstream files needed patches (`Agent.cpp`, `AgentOut.cpp`,
  `ArchiveFolderOut.cpp`, 31 lines in all); everything else is handled on the
  Linux side.
- The tree has 40 dialog templates, not 65: 25 in 7zFM, 13 in 7zG (all of
  them also in 7zFM), plus SFX, installer and Windows CE variants. Two Windows
  CE variants do not build upstream either (`IDD_MENU_2`: string 3441 is
  defined twice; `IDD_BENCH_2`: undefined `IDT_BENCH_SIZE_VAL`). The controls
  use seven classes only: `Button`, `Static`, `Edit`, `ComboBox`,
  `SysListView32`, `msctls_progress32` and `msctls_updown32` (no list box or
  trackbar).
- The resource compiler adds `CBS_SIMPLE` to a `COMBOBOX` only when its style
  names no combo box type; OR-ing it unconditionally turns `CBS_DROPDOWN`
  into `CBS_DROPDOWNLIST`. rcgen follows the resource compiler.
- A `COMBOBOX` height in the templates includes the drop-down list (up to 198
  DLUs), so those controls extend past the dialog; the Phase 2 layout engine
  must compute the closed height from the font. `IDD_BROWSE` also has a label
  36 DLUs wider than the dialog.
- `FileManager/LangUtils.cpp` (`LangString`, `ReloadLang`, the dialog text
  helpers) includes `Windows/Window.h`, so it builds with the Phase 2 shim.
  Phase 1 provides what it needs from the platform: `MyLoadString`,
  `GetUserDefaultLangID` (`$LANGUAGE`, `$LC_ALL`, `$LC_MESSAGES`, `$LANG`
  mapped with a checked copy of its `kLangs` list) and the `Lang/` directory
  next to the executable (`NPlatform::InitModuleDirPrefix`). `CLang` itself is
  upstream code and is tested with a language file.
- `CompareFileNames_ForFolderList()` lives in `PanelSort.cpp`, but UI/Agent
  needs it; `FolderCompare.cpp` is a copy (a test checks that it matches)
  until `PanelSort.cpp` builds in Phase 4.
- `NDLL::MyGetModuleFileName()` and `RemoveDirWithSubItems()` exist only for
  Windows upstream; the platform layer provides POSIX versions.
- The zip handler has no `IInArchiveGetStream`, so the panel opens archives
  inside a zip by extracting them first (`IFolderOperations::CopyTo`), as on
  Windows; tar, iso and other handlers give seekable item streams.
- POSIX `FSFolder` reports `kpidCTime` only where `statx` gives a birth time,
  and the status change time as `kpidChangeTime`. A symbolic link to a
  directory is a directory (the flat view does not follow it); its own mode
  is shown as permissions. `IFolderOperations::CopyTo`, `CopyFrom` and
  `CopyFromFile` return `E_NOTIMPL` until Phase 5, and icons
  (`IFolderGetSystemIconIndex`) are left to the Qt side.
- `RootFolder` items are the home directory, `/` and mounts outside system
  directories (`/proc`, `/run` except `/run/media`, `/var`, `/boot`, ...) that
  are not pseudo file systems. Sizes come from `statvfs` only when the column
  is shown, but a hung network mount can still block that call.
- Upstream `CPropVariant::Get_Ns100()` returns 0 for
  `k_PropVar_TimePrec_1ns` (its condition reads `prec == 0 &&`), so the
  nanosecond remainder of file times is lost by callers that use it. Not
  patched; noted for an upstream report.
- Under UBSan, upstream code has known findings (left shifts of negative
  values in `C/LzFindOpt.c`, unaligned loads by design), so only port code is
  built with `-fno-sanitize-recover`.

### Phase 2: Qt shim, dialog engine, dark theme
- [x] Shim classes: `CWindow`, `CDialog`, modal and modeless dialogs, child
      controls, `CComboBox`, `CEdit`, `CStatic`, buttons, `CProgressBar`,
      up-down, trackbar, `CListView` (dialog subset), and property sheets as
      `QTabWidget`. `CComboBoxEx` follows with the panel in Phase 4.
- [x] Shim behaviour: `MessageBox`, timers, `PostMsg`, and thread marshalling.
- [x] DLU layout engine and font calibration.
- [x] Dark theme: palette and style, SVG toolbar icons, icon resources of the
      `.rc` files. The archive type icons (`Archive/Icons/*.ico`) come with
      the file list in Phase 4.
- [x] Dialog gallery and golden-image tests.
- [x] Wine reference pictures, from the official 7-Zip 26.05 release for
      Windows (`linux/tools/winref`): every template rendered by the Windows
      dialog manager of Wine, next to the gallery; and a test that compares
      the compiled templates of the release with the rcgen tables.

Exit: every dialog renders in the dark theme with the original layout, and
the golden tests are in ctest. Done: `render_gallery` covers the 25 templates
of 7zFM (7zG's are a subset), the options property sheet and the toolbar
icons; `unit_shim` drives real templates through the upstream classes.

What is where:
- `linux/shim/win32/`: `windows.h` (constants, also read by rcgen),
  `winuser.h` (types and functions), `CommCtrl.h`, `prsht.h`. Force-included
  `windows.h` gives upstream GUI sources the API; `_WIN64` is defined for
  them.
- `linux/shim/ShimWindow.h`: `HWND__`, the window object behind an `HWND`
  (Qt widget, control ID, styles, user data, window procedure, children in
  Z order), window locking, thread marshalling (`OnGuiThread`).
- `ShimCore.cpp`: handles, `SendMessage`/`PostMessage`, the message loop
  (`GetMessage` runs the Qt loop and returns `WM_NULL`), timers, geometry,
  focus, text, window data, screens. `SendMessageA` converts the narrow
  messages to the wide ones that the built-in procedures handle.
- `ShimDialog.cpp`: dialogs from the templates, base units, `DefDlgProc`,
  modal loops, and the `IsDialogMessage` keyboard rules (an application
  event filter): Enter, Esc, F1, arrows within groups, label mnemonics.
- `ShimControls.cpp` (Button, Static, Edit, ComboBox, progress, up-down,
  trackbar, registered classes), `ShimListView.cpp`, `ShimMessageBox.cpp`,
  `ShimPropSheet.cpp`, `ShimResources.cpp` (icons, image lists, fonts,
  accelerator tables).
- `ShimDialog.h`: the engine for tools and tests, plus test hooks for modal
  dialogs and message boxes.
- `linux/theme/`: `NTheme::Apply()` and `ToolbarIcon()`; the SVG icons.
- `linux/tools/gallery/`, `linux/tests/render/`: the gallery and its golden
  pictures.
- `linux/tools/winref/`: `winref.c` (a Winelib program, target `winref`) and
  `wine_reference.py` (reference pictures, side-by-side comparisons, and the
  template check behind the optional `rcgen_windows_release` test).

Findings:
- The upstream wrappers (`Window.h`, `Control/*.h`) are thin inline calls of
  user32 functions and messages, so implementing those under them needs no
  upstream edits at all. 12 of the 25 dialog and page sources already
  compile against the shim (Combo, Copy, Edit, ListView, Mem, Messages,
  Password, Split, Extract, and the Editor, Folders and 7-Zip pages); the
  others need shell, file-system and system pieces (`SHGetFileInfo`,
  `ShellExecute`, `ShObjIdl.h`, `CProcessAffinity`, ...) that come with
  Phases 3 and 5.
- `Window.h` casts pointers to `LONG` in `SetLongPtr()` unless `_WIN64` is
  defined; without it `DialogProcedure` loses the dialog object.
- Qt and Win32 controls differ in ways the dialog code notices, and the shim
  follows Win32: `QDialog` hides itself on Esc (`reject()`), so those paths
  are disabled and Esc, Enter and closing go through `WM_COMMAND` and
  `WM_CLOSE`; a check box or radio button reacts to its whole rectangle, not
  only to the box and the text (the templates make them wide); adding an
  item to an empty `QComboBox` selects it, a Win32 combo box keeps no
  selection; automatic radio buttons are exclusive within their `WS_GROUP`
  group, not among Qt siblings, and the checked one gets `WS_TABSTOP`;
  `WM_SETTEXT` to a single-line edit sends `EN_CHANGE`.
- Dialog units use the Windows formula (average width of A-Z and a-z, and
  the font height). The layouts keep their proportions with Linux UI fonts;
  fonts with a tall line height (Noto Sans) make dialogs about 15% taller
  than ones like Inter, as they would on Windows.
- Fusion draws frames by darkening the window color, which is invisible on a
  dark palette; a proxy style draws lighter frames. Style sheets are avoided:
  a matching rule takes the drawing of the whole widget away from the style.
- Keys reach the dialog logic only through the focus widget of the active
  window; offscreen tests must wait for the window to be active.
- Inter turns the `<--` text of the Browse dialog button into an arrow
  (a ligature).
- Rendering offscreen with a private `HOME` (no user fontconfig) and a fixed
  font is deterministic: the golden test allows 16 differing pixels, so a
  change of a single line is caught.
- The release's 7zFM.exe is built from `UI/FileManager` and runs 7zG.exe to
  add and extract, so it lacks the Compress, Extract and Benchmark dialogs
  that the Linux 7zFM (`Bundles/Fm`) has; its 7zG.exe has them.
- All 33 templates of 7zFM.exe and 7zG.exe 26.05 match the rcgen tables,
  after one rcgen change: rc.exe ignores `- (b)` after a closing parenthesis
  (`(a) - (b)` is `a`, while `n - (b)` subtracts). It shows in the labels of
  `IDD_BENCH`, the only place that uses it.
- Next to Wine (Tahoma 8 at 96 dpi: base units 6 x 13) the layouts match
  control for control; the port is larger by the ratio of the fonts. The one
  visible difference was the group box: Windows draws the title on the top
  line of the frame, Fusion above it; the theme now draws it as Windows.

### Phase 3: 7zG (done)
- [x] Compile `GUI.cpp`: `7zG` is the upstream `GUI.cpp` and dialogs on top
      of the shim, with a Linux `main()`.
- [x] Extract, Compress (with options), Hash (results list), Benchmark,
      Progress (with its messages list), Overwrite, Password and
      Memory-limit dialogs; Help buttons and the "..." file dialogs.
- [x] A test hook that answers dialogs from a script (`z7-gui-driver`), so
      `7zG a/x/t/h/b` is tested end to end against the `7zz` oracle.
- [x] Wayland checks: activation, modality and dialog parenting through
      KWin scripting, keyboard input through libei, screenshots.

Exit: `7zG` matches Windows `7zG` (minus Appendix A items) and is covered by
tests. Done: `gui_7zg` (19 scenarios, offscreen) and `wayland_7zg` (nested
KWin) pass, also under ASan; the dialogs that 7zG.exe of the release fills
under Wine (`wine_reference.py --live`) show the same values and layout.

What is where:
- `linux/gui/`: `Main.cpp` (command line, program directory, Qt with the
  7-Zip arguments kept away from it, theme, then `WinMain()` of
  `GUI/GUI.cpp`). The dialog sources shared with 7zFM are in `z7_gui`
  (`linux/shim/CMakeLists.txt`).
- `linux/shim/win32/`: `winbase.h`, `objbase.h`, `commdlg.h`, `shellapi.h`,
  `ShlObj.h`, `ShObjIdl.h`, `HtmlHelp.h`, `Shlwapi.h`; `winuser.h` sets
  `Z7_WIN32_WINNT_MIN` to Vista.
- `linux/shim/ShimShell.cpp`: `SHGetFileInfo()` (theme icons by MIME type,
  one system image list per icon size, index 0 for unknown types),
  special folders, `HtmlHelp()` (a local `help/` copy of the pages next to
  the program, else the online help at 7-zip.opensource.jp/chm/), the
  folder browser of `Windows/Shell.h` and the file dialogs of
  `Windows/CommonDialog.h` as `QFileDialog`s, and the text clipboard of
  `Windows/Clipboard.h`. `ShimSystem.cpp`: kernel32 and ole32 calls.
- `linux/shim/ShimDialog.cpp`: the table of Windows-only options
  (NTFS security and streams, -ssw, SFX), which stay disabled or hidden.
- `linux/platform/CommandLine.cpp`: `GetCommandLineW()` and a POSIX
  `SplitCommandLine()` that give back every argument unchanged.
  `ZoneId.cpp`: the Zone.Identifier helpers of `ArchiveExtractCallback`.
  `Instance.cpp`: `g_hInstance` apart from `Globals.cpp`, because `GUI.cpp`
  defines it.
- `linux/tests/gui/`: `gui_driver.cpp` (7zG with a JSON script for dialogs,
  message boxes and file dialogs, and a JSON log of what they show) and
  `test_7zg.py`. `linux/tests/wayland/test_7zg_wayland.py`;
  `wlsession.Session.kwin_query()` returns values from KWin scripts.
- `linux/core/oracle.mak`: the oracle build with header dependencies.

Findings:
- Five upstream files needed POSIX branches (`Windows/System.h`,
  `ProgressDialog2.cpp`, `OverwriteDialog.cpp`, `BrowseDialog.cpp`,
  `ExtractCallback.cpp`): `CFileInfo` has `timespec` times and no
  `Attrib` on POSIX, threads cannot be waited on with a timeout, and the
  custom browse dialog for very long paths is Windows-only.
- `ProgressDialog2` `WaitMode` (only 7zFM sets it: show the dialog only if
  the operation takes long) waits on a thread handle. On POSIX it polls the
  flag that the finished thread sets (Phase 4); a password asked during
  that time shows the dialog after the 500 ms.
- Win32 behaviour the dialog code relies on: typing in a combo box clears
  the selection of its list (`ExtractDialog` reads the typed path only
  when `CB_GETCURSEL` is `CB_ERR`); `MoveWindow()` sends `WM_SIZE` at once,
  also for a hidden window during `WM_INITDIALOG` (`CListViewDialog`).
- QApplication takes its own options (`-style`, `-platform`, ...) out of
  argv, so it gets only the program name; Qt settings come from the
  environment.
- "Background" in the progress dialog runs the threads with `SCHED_IDLE`.
  Going back needs a `RLIMIT_NICE` that allows nice 0 (the default limit
  is 0), so "Foreground" usually keeps the idle policy.
- The memory-limit dialog is asked for by the RAR5 and WIM handlers only;
  the test crafts a small RAR5 archive whose header asks for a 2 GB
  dictionary.
- 7zG creates no SFX archives: the GUI looks for the Windows module
  `7z.sfx` (see Appendix C).
- On Wayland, KWin decides the title bar buttons: dialogs without
  `WS_MINIMIZEBOX` still get a minimize button (no protocol removes it);
  fixed-size dialogs are not maximizable, and modal ones are modal for KWin.
- The upstream makefile has no header dependencies, so the oracle was not
  rebuilt after an upstream header edit; `oracle.mak` adds them.
- Under ASan, fontconfig leaks (`linux/tests/lsan.supp`).
- Taskbar progress comes for free once `CoCreateInstance(CLSID_TaskbarList)`
  returns an object (Phase 6): `ProgressDialog2` already drives
  `ITaskbarList3`.

### Phase 4: 7zFM core (done)
- [x] The upstream panel on the shim (decision 5): the list view with
      columns, sorting (keys and header clicks), icons and view modes, the
      address combo box with its history, the status bar.
- [x] Main window: menus and accelerators from rcgen, both toolbars with their
      options, one or two panels (F9), panel focus and Tab.
- [x] Navigation: folders, nested archives, open inside, the parent folder,
      root view, flat view, bookmarks and folder history.
- [x] Keyboard: selection keys (Ins, +, -, *, Shift+arrows), type-ahead, and
      inline rename (F2).

Exit: dual-panel browsing of file systems and archives, with screenshots
matching the original layout. Done: `gui_7zfm` (19 scenarios, offscreen) and
`wayland_7zfm` (keyboard through libei in nested KWin) pass, also under ASan.

What is where:
- `linux/fm/`: `Main.cpp` (as for 7zG, then `WinMain()` of `FM.cpp`);
  `CMakeLists.txt` lists the upstream sources of `z7_fm`, compiled with the
  shim `windows.h` and `platform/UpstreamPosix.h` force-included, and the
  ones left out (`PanelDrag`, `EnumFormatEtc`, `VerCtrl`, `LinkDialog`,
  `RegistryContextMenu`, the Windows folders). Their stand-ins:
  `PanelDragLinux.cpp` (no drag and drop until Phase 6),
  `ContextMenuRegistration.cpp` (Phase 6), `PendingCommands.cpp` (Link and
  version control say "not supported" until Phase 5),
  `RootFolderNames.cpp`, `ClassDefsShell.cpp` (shell GUIDs).
- `linux/platform/UpstreamPosix.h`, `.cpp`: POSIX meanings of the
  Windows-only helpers that the 7zFM sources call (`CTempDir`, device paths,
  privileges); `FSDrivesPosix.h`. Redirect guards in `FSFolder.h`,
  `RootFolder.h` and `FSDrives.h` select the POSIX folders of Phase 1.
- `linux/shim/`: `ShimMenu.cpp` (menus, `WM_INITMENUPOPUP`,
  `TrackPopupMenu`), `ShimBars.cpp` (toolbar, rebar, status bar),
  `ShimInput.cpp` (input events as messages, accelerators, mouse capture),
  `ShimListView.cpp` (the list view), `ShimControls.cpp` (`ComboBoxEx`; a
  top-level window of a registered class is a frame with the menu bar and a
  client widget).
- `linux/tests/gui/`: `DriverCore.cpp` (the script engine of both drivers),
  `fm_driver.cpp` (keys, clicks, header clicks, menu and toolbar commands,
  expectations on the panels), `test_7zfm.py`;
  `linux/tests/wayland/test_7zfm_wayland.py`.

Findings:
- Upstream edits: POSIX forms in `Windows/DLL.h`, `FileFind`, `FileIO`;
  `GetWinAttrib()` instead of `Attrib` and no Network item in
  `PanelFolderChange.cpp` and `BrowseDialog2.cpp`; no child process to
  wait for in `PanelItemOpen.cpp`; `g_LargePagesMode` is defined by
  `ArchiveCommandLine.cpp` on POSIX; the parent of "/" focuses the "/" item
  of the root; the `WaitMode` poll in `ProgressDialog2.cpp`.
- Win32 behaviour that 7zFM relies on: Enter opens the selected items only
  (at start item 0 has the focus but no selection); the list notifies
  `LVN_KEYDOWN` for `WM_SYSKEYDOWN` too (Alt+digit bookmarks, Alt+arrows,
  Alt+Enter); subitems that were never set are callbacks; a disabled window
  keeps the focus of its child; `EndDialog()` activates the owner, whose
  `WM_ACTIVATE` gives the focus back; a `CW_USEDEFAULT` window has its size
  at `WM_CREATE` (the splitter is placed there); `DefWindowProc(WM_NCCREATE)`
  returns TRUE.
- Keys: the digit keys are VK '0'...'9' whatever character they give (Shift+1
  is `Qt::Key_Exclam`), so the shim reads the XKB key code on Wayland and
  X11; right Ctrl (bookmarks) is told apart the same way.
- Without a window manager (offscreen tests) nothing activates a window
  after a modal one closes; the shim activates the owner as Windows does.
- The root folder is the home directory, "/" and the user's mount points
  (Phase 1); "/" or "\\" in the list opens it, as "Computer" on Windows.
- KWin 6.8 locks a new session at once: `wlsession` starts it with
  `--no-lockscreen`, and gives it the system `XDG_CONFIG_DIRS` and
  `XDG_DATA_DIRS` only (the user's `kdedefaults` leaked their theme in).
- Left for Phase 5: copying between file systems (`CopyFileSystemItems` of
  the POSIX folder), Link and version control, delete through
  `SHFileOperation`, and the calls that upstream looks up with
  `GetProcAddress(GetModuleHandle(...))`, which find nothing.
- Left for Phase 6: drag and drop, and the app ID: on Wayland the windows
  have the generic icon until a `.desktop` file names them.

### Phase 5: 7zFM operations
- [x] Copy and Move (F5, F6) across file systems and archives.
- [x] Delete to Trash, and Shift+Delete for permanent deletion.
- [ ] New folder and new file, rename, properties, comments.
- [ ] Hashes, split and combine, links.
- [ ] Add, Extract and Test from the toolbar, run in-process.
- [ ] Open inside and outside, Edit (F4) with the change watcher.
- [ ] All option pages (with a Linux System page), and Help opening the
      online docs.

Exit: the parity checklist in Appendix A is complete.

What is where:
- `linux/platform/FSFolderCopyPosix.cpp`: `CFSFolder::CopyTo()` and
  `CopyFileSystemItems()`, the POSIX `FSFolderCopy.cpp`.
- `linux/shim/ShimShell.cpp`: `SHFileOperation(FO_DELETE)` with the trash;
  `ShimSystem.cpp`: the modules that `GetModuleHandle()` finds and the
  functions that `GetProcAddress()` gives for them.

Findings:
- Copy and move follow the Windows code (the callback's `AskWrite()` deletes
  or renames the old file, folders are merged, the first error stops the
  operation), with POSIX rules: symbolic links are copied as links, FIFOs as
  FIFOs; permissions and times are kept (set-user-ID and set-group-ID only
  when the owner stays the same); data goes through `FICLONE`, then
  `copy_file_range()`, then `read()`/`write()`; partial copies are removed.
  A move is `renameat2(RENAME_NOREPLACE)`, and a copy plus delete across
  file systems. "Onto itself" is also found through symbolic links (real
  paths) and hard links (device and inode), where deleting the old file
  would delete the source.
- Upstream swaps "copy" and "move" in the "Cannot ... folder onto itself"
  message; the POSIX code does not.
- The GUI code is built without `_UNICODE`, so it looks up the wide shell
  functions by name (`Z7_GET_PROC_ADDRESS(GetModuleHandleW(L"shell32.dll"),
  ...)`): Delete did nothing while `GetModuleHandle()` returned NULL. Now
  `shell32` and `kernel32` are found, `GetProcAddress()` (a macro for the
  shim version, so the `dlsym()` one of `Windows/DLL.cpp` stays for real
  libraries) gives `SHFileOperationW`, `SHGetFileInfoW` and
  `ShellExecuteExW`, and NULL for the rest, which callers handle as a
  missing function.
- Delete moves to the freedesktop trash (`QFile::moveToTrash`, the home
  trash or `.Trash-$UID` at the top of another file system); Shift+Delete
  asks and deletes. What cannot go to the trash is listed in a question
  that offers permanent deletion, as Windows asks for drives without a
  Recycle Bin. Qt 6.12 fails with EBADF for files outside `$HOME` on the
  home file system while `$XDG_DATA_HOME/Trash` does not exist, so the shim
  creates it first.

### Phase 6: Desktop integration
- [ ] App ID, `.desktop`, icons, MIME associations.
- [ ] Drag and drop in and out (Ark protocol, lazy `uri-list`, internal MIME),
      covered by Wayland scenario tests with Dolphin and a probe app.
- [ ] Clipboard, taskbar progress, notifications, portal "Open with" and file
      dialogs, `xdg-email`.
- [ ] Context menus: Dolphin service menu, then the KDE plugin and the
      Nautilus, Thunar and Nemo integrations.

Exit: the drag-and-drop and clipboard scenarios pass in nested KWin, and the
user signs off on the real session.

### Phase 7: Packaging and upstream sync
- [ ] Install layout, `PKGBUILD` (Arch/CachyOS), AppStream metadata, `Lang`
      files; Flatpak is optional (portal FileTransfer for drag and drop).
- [ ] Upstream sync script and documentation: merge the new upstream release
      from `upstream` into `main`, rerun `upstream_sources.py`, rcgen and the
      consistency checks, check the templates against the new Windows release
      (`rcgen_windows_release`) and look at the Wine reference pictures,
      review the upstream edits (Appendix B), then the full test suite.

## Appendix A: Windows features and their Linux counterparts

| Windows | Linux |
|---|---|
| Explorer context menu (`UI/Explorer`) | File manager service menus and plugins (Phase 6) |
| Registry file associations, System page | `.desktop` `MimeType=` and `mimeapps.list` |
| Shell icons (`SHGetFileInfo`) | Theme icons through `QMimeDatabase`, plus the 7-Zip archive icons |
| OLE drag and drop, `SetTargetFolder` | Wayland drag and drop, the Ark/KIO protocol, lazy `uri-list` |
| Recycle Bin | freedesktop Trash (`QFile::moveToTrash`) |
| "Computer" with drives (`FSDrives`) | `/`, home and mount points |
| Network neighbourhood (`NetFolder`) | Dropped; gvfs and KIO mounts appear as paths |
| NTFS alternate streams (`AltStreamsFolder`) | Dropped (extended attributes may be considered later) |
| NTFS security restore | Dropped; POSIX permissions and ownership are supported |
| Large pages and the lock-memory privilege | Dropped |
| Waiting on child processes for Edit or Open | Watching the temporary file |
| Taskbar progress (`ITaskbarList3`) | `com.canonical.Unity.LauncherEntry` |
| CHM help | Online documentation |
| MAPI email | `xdg-email --attach` |
| Window position restore | Size and maximized state only (Wayland) |
| `\\.\PhysicalDrive` device access | Optional: `/dev/*` images |

## Appendix B: Upstream patch inventory

Git keeps it; each commit message says why the edit is needed. After
`git fetch upstream`, these list the edits since the last merged upstream
release (today that release is commit `b88c6bc`, 26.05):

```sh
git diff --stat $(git merge-base HEAD upstream/main) HEAD -- C CPP Asm DOC
git log --oneline $(git merge-base HEAD upstream/main)..HEAD -- C CPP Asm DOC
```

After merging a release, check that each edit is still needed.

## Appendix C: Open questions

- **App ID:** it must be a valid D-Bus name, and segments cannot start with a
  digit, so `org.7-zip.*` is invalid. Candidate: `io.github.mikigal.SevenZip`.
- **`Lang/*.txt` translations** are not in the source repo. They ship in the
  binary distribution and could be extracted with our own `7zz` from the
  official package once the user approves the download.
- **SFX archives:** 7zG looks for `7z.sfx`, the Windows GUI module, so
  "Create SFX archive" is disabled on Linux. The upstream console SFX
  (`Bundles/SFXCon/makefile.gcc`) builds on Linux; shipping it as the module
  would make self-extracting ELF archives possible.
- **Help pages:** Help opens the online copy of the help
  (7-zip.opensource.jp/chm/). The release's `7-zip.chm` could be unpacked
  with `7zz` into a `help/` directory next to the program for offline help,
  which `HtmlHelp()` already prefers; this goes with the Lang decision.
- **Windows reference screenshots** (section 4, "Fidelity check"): resolved
  without mingw-w64. The user approved downloading the official release for
  Windows (26.05, `7z2605-x64.exe`, SHA-256 checked against the GitHub
  release); a Winelib helper renders its templates (see Phase 2). The
  release also contains the `Lang/*.txt` files; shipping them is still open
  (see the item above).
