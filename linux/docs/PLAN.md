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

Non-goals: reimplementing the Win32 API in general, Wine, or X11-only features
(for example XDND Direct Save).

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
  7zFM (Qt main window, panels)          7zG (Qt dialogs only)
        |                                     |
  linux/fm: CApp/CPanel views, FolderModel,   |
  drag and drop, clipboard, desktop services  |
        |                                     |
  linux/shim: NWindows::NControl over Qt  <---+   (dialog logic from upstream
  + dialog engine (layouts from rcgen tables)      GUI/ and FileManager/ compiled as-is)
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
   `ExtractDialog.cpp` and the rest) compile unchanged against a Qt
   implementation of `NWindows::NControl`. Controls are found by resource ID,
   signals map to `OnButtonClicked`, `OnCommand(CBN_SELCHANGE / EN_CHANGE)` and
   `OnMessage(WM_TIMER / custom)`, and `PostMsg` becomes a queued Qt event.
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
5. **The main window and panel views are new Qt code; the panel logic is
   reused.** The upstream `FileManager/Panel.h` is redirected to a Qt version
   that keeps the member names, and `_listView` becomes an adapter over
   `QTreeView` plus a virtual `FolderModel` built on `IFolderFolder`. Panel
   logic files (`PanelCopy`, `PanelOperations`, `PanelCrc`, `PanelSplitFile`,
   `PanelFolderChange`, `PanelSelect`, `PanelSort`, and parts of
   `PanelItemOpen` and `PanelItems`) are compiled from upstream where possible.
   The view-side files (`Panel.cpp`, `PanelListNotify`, `PanelKey`,
   `PanelDrag`, `PanelMenu`) are reimplemented in `linux/fm`.
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
  fm/                     7zFM main window, panels, models, drag and drop, clipboard
  gui/                    7zG entry point
  theme/                  dark palette, QSS, SVG icons
  tools/                  rcgen.py, Wayland test harness, input injection helper, check.sh
  tests/                  unit, render (golden image) and Wayland scenario tests
  packaging/              .desktop, MIME, service menus, PKGBUILD, AppStream
```

### Upstream patch policy

- Edit upstream files (`C/`, `CPP/`) only when it cannot be avoided, and keep
  every edit minimal and guarded (`#ifndef _WIN32` or `#ifdef Z7_QT_GUI`) so
  the Windows build is unchanged.
- Quoted relative includes (`#include "../../../Windows/Control/Dialog.h"`)
  cannot be shadowed through include paths. Headers whose Win32 content must be
  replaced therefore get a **redirect guard**: a few lines at the top that
  include the `linux/shim` (or `linux/fm`) replacement under `Z7_QT_GUI`, with
  the original body in the `#else` branch. Nothing else in the file changes.
- Angle-bracket system headers (`<CommCtrl.h>`, `<ShlObj.h>`) are shadowed by
  stub headers in `linux/shim/win32/` on the include path.
- Each upstream patch is its own small commit, and `linux/docs/UPSTREAM-PATCHES.md`
  lists them.

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

### Phase 0: Foundations and test harness
- [ ] Work branch `linux-port`; add an `upstream` remote (`ip7z/7zip`).
- [ ] `CLAUDE.md` and this plan.
- [ ] CMake: a static core library from the upstream source lists (derived
      from `Bundles/Format7zF/Arc_gcc.mak`, with a consistency check).
- [ ] Build of the `7zz` oracle with the upstream makefile, out of tree.
- [ ] `linux/tools/check.sh` as the single verification entry point.
- [ ] `linux/tools/wlsession.py`: nested KWin, isolation, screenshots, cleanup,
      and a smoke test with a trivial Qt window.
- [ ] `linux/tools/ei-input`: a libei helper for pointer move, press and
      release and for keys, with a smoke test (a synthetic click reaches the
      test window).

Exit: `check.sh` passes, and an automatic screenshot plus a synthetic click
work in nested KWin.

### Phase 1: Platform layer
- [ ] `UI/Agent` builds on Linux (small upstream patches).
- [ ] INI-backed `NRegistry::CKey`, with tests.
- [ ] `rcgen.py`: dialogs, `MENUEX`, `ACCELERATORS` and `STRINGTABLE`s into
      generated C++ tables, with tests.
- [ ] `MyLoadString` and `CLang` (`Lang/*.txt`) wired up.
- [ ] POSIX `FSFolder`, `RootFolder` (`/`, home, mount points with
      size and free-space columns) and POSIX columns (permissions, user,
      group, symlink target, dotfiles as hidden), with tests through the
      `IFolderFolder` API.

Exit: a headless test browses file systems and nested archives through
`IFolderFolder`, and settings round-trip.

### Phase 2: Qt shim, dialog engine, dark theme
- [ ] Shim classes: `CWindow`, `CDialog`, modal and modeless dialogs, child
      controls, `CComboBox(Ex)`, `CEdit`, `CStatic`, buttons, `CProgressBar`,
      up-down, trackbar, `CListView` (dialog subset), and property sheets as
      `QTabWidget`.
- [ ] Shim behaviour: `MessageBox`, timers, `PostMsg`, and thread marshalling.
- [ ] DLU layout engine and font calibration.
- [ ] Dark theme: palette and QSS, SVG toolbar icons, archive `.ico` icons.
- [ ] Dialog gallery and golden-image tests; Wine references if mingw-w64 is
      available.

Exit: every dialog renders in the dark theme with the original layout, and
the golden tests are in ctest.

### Phase 3: 7zG
- [ ] Compile `GUI.cpp`.
- [ ] Extract, Compress (with options), Hash (results list), Benchmark,
      Progress, Overwrite, Password, Messages and Memory-limit dialogs.
- [ ] A test hook that answers dialogs from a script, so `7zG a/x/t/h/b` can
      be tested end to end against the `7zz` oracle.
- [ ] Wayland checks: activation, dialog parenting, screenshots.

Exit: `7zG` matches Windows `7zG` (minus Appendix A items) and is covered by
tests.

### Phase 4: 7zFM core
- [ ] Redirect `Panel.h` to the Qt version, the list view adapter, a virtual
      `FolderModel`, columns, sorting, icons, address combo with history,
      status bar.
- [ ] Main window: menus and accelerators from rcgen, both toolbars with their
      options, one or two panels (F9), panel focus and Tab.
- [ ] Navigation: folders, nested archives, open inside, the parent folder,
      root view, flat view, bookmarks and folder history.
- [ ] Keyboard: selection keys (Ins, +, -, *, Shift+arrows), type-ahead, and
      inline rename (F2).

Exit: dual-panel browsing of file systems and archives, with screenshots
matching the original layout.

### Phase 5: 7zFM operations
- [ ] Copy and Move (F5, F6) across file systems and archives.
- [ ] Delete to Trash, and Shift+Delete for permanent deletion.
- [ ] New folder and new file, rename, properties, comments.
- [ ] Hashes, split and combine, links.
- [ ] Add, Extract and Test from the toolbar, run in-process.
- [ ] Open inside and outside, Edit (F4) with the change watcher.
- [ ] All option pages (with a Linux System page), and Help opening the
      online docs.

Exit: the parity checklist in Appendix A is complete.

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
- [ ] Upstream sync script and documentation: fast-forward `main` from
      upstream, merge into `linux-port`, rerun rcgen and the consistency
      checks, then the full test suite.

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

Maintained in `linux/docs/UPSTREAM-PATCHES.md`, which is created with the
first upstream edit.

## Appendix C: Open questions

- **App ID:** it must be a valid D-Bus name, and segments cannot start with a
  digit, so `org.7-zip.*` is invalid. Candidate: `io.github.mikigal.SevenZip`.
- **`Lang/*.txt` translations** are not in the source repo. They ship in the
  binary distribution and could be extracted with our own `7zz` from the
  official package once the user approves the download.
- **mingw-w64** (`mingw-w64-gcc`, which needs root to install) would enable
  Windows reference screenshots under Wine and a check that upstream patches do
  not break the Windows build.
