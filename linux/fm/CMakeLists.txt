# 7zFM: the 7-Zip file manager. The upstream FileManager sources (FM.cpp,
# App.cpp, the panel and its menus, the option pages) are compiled unchanged
# against the shim, like the dialogs of 7zG; Main.cpp sets up Qt and the
# platform layer and calls WinMain() of FM.cpp.
#
# Z7_QT_GUI selects the Linux replacements of the Windows-only folder headers
# (FSFolder.h, RootFolder.h, FSDrives.h) through their redirect guards.

# From FM_OBJS in UI/FileManager/FM.mak, EXPLORER_OBJS and UI_COMMON_OBJS
# in Bundles/Fm/makefile, minus the files that are already in z7_ui and
# z7_gui, the Windows file system folders (FSFolder, FSFolderCopy,
# RootFolder, FSDrives, NetFolder, AltStreamsFolder: linux/platform has the
# POSIX ones) and the Windows registry associations
# (RegistryAssociations: see RegistryAssociationsLinux.cpp).
set(Z7_FM_SOURCES
  CPP/Windows/MemoryGlobal.cpp
  CPP/7zip/UI/Common/CompressCall2.cpp
  CPP/7zip/UI/Explorer/ContextMenu.cpp
  CPP/7zip/UI/FileManager/AboutDialog.cpp
  CPP/7zip/UI/FileManager/App.cpp
  CPP/7zip/UI/FileManager/BrowseDialog2.cpp
  CPP/7zip/UI/FileManager/CopyDialog.cpp
  CPP/7zip/UI/FileManager/EnumFormatEtc.cpp
  CPP/7zip/UI/FileManager/EditPage.cpp
  CPP/7zip/UI/FileManager/FileFolderPluginOpen.cpp
  CPP/7zip/UI/FileManager/FilePlugins.cpp
  CPP/7zip/UI/FileManager/FM.cpp
  CPP/7zip/UI/FileManager/FoldersPage.cpp
  CPP/7zip/UI/FileManager/LangPage.cpp
  CPP/7zip/UI/FileManager/LinkDialog.cpp
  CPP/7zip/UI/FileManager/MenuPage.cpp
  CPP/7zip/UI/FileManager/MessagesDialog.cpp
  CPP/7zip/UI/FileManager/MyLoadMenu.cpp
  CPP/7zip/UI/FileManager/OptionsDialog.cpp
  CPP/7zip/UI/FileManager/Panel.cpp
  CPP/7zip/UI/FileManager/PanelCopy.cpp
  CPP/7zip/UI/FileManager/PanelCrc.cpp
  CPP/7zip/UI/FileManager/PanelDrag.cpp
  CPP/7zip/UI/FileManager/PanelFolderChange.cpp
  CPP/7zip/UI/FileManager/PanelItemOpen.cpp
  CPP/7zip/UI/FileManager/PanelItems.cpp
  CPP/7zip/UI/FileManager/PanelKey.cpp
  CPP/7zip/UI/FileManager/PanelListNotify.cpp
  CPP/7zip/UI/FileManager/PanelMenu.cpp
  CPP/7zip/UI/FileManager/PanelOperations.cpp
  CPP/7zip/UI/FileManager/PanelSelect.cpp
  CPP/7zip/UI/FileManager/PanelSort.cpp
  CPP/7zip/UI/FileManager/PanelSplitFile.cpp
  CPP/7zip/UI/FileManager/SettingsPage.cpp
  CPP/7zip/UI/FileManager/SplitDialog.cpp
  CPP/7zip/UI/FileManager/SystemPage.cpp
  CPP/7zip/UI/FileManager/UpdateCallback100.cpp
)
z7_upstream_paths(Z7_FM_SOURCES ${Z7_FM_SOURCES})

add_library(z7_fm STATIC ${Z7_FM_SOURCES})
target_compile_definitions(z7_fm PUBLIC Z7_QT_GUI)
# SHELL: keeps CMake from merging the two -include options into one.
target_compile_options(z7_fm PRIVATE ${Z7_UPSTREAM_WARNINGS}
  "SHELL:-include ${Z7_SHIM_INCLUDE}/windows.h"
  "SHELL:-include ${Z7_ROOT}/linux/platform/UpstreamPosix.h")
target_include_directories(z7_fm PRIVATE "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_fm PUBLIC z7_gui)

# Linux parts of the file manager (port code), compiled like the upstream
# sources they complete.
add_library(z7_fm_linux STATIC
  ClassDefsShell.cpp
  ContextMenuRegistration.cpp
  PendingCommands.cpp
  RootFolderNames.cpp
)
z7_port_target(z7_fm_linux)
# Upstream COM interfaces have no virtual destructors by design.
target_compile_options(z7_fm_linux PRIVATE -Wno-non-virtual-dtor
  "SHELL:-include ${Z7_SHIM_INCLUDE}/windows.h"
  "SHELL:-include ${Z7_ROOT}/linux/platform/UpstreamPosix.h")
target_link_libraries(z7_fm_linux PUBLIC z7_fm z7_fm_qt)

# Linux parts of the file manager that use Qt. They include the Qt headers
# before windows.h, which defines _WIN64 (a Windows build for Qt), so
# windows.h is not force-included for them.
add_library(z7_fm_qt STATIC
  RegistryAssociationsLinux.cpp
)
z7_port_target(z7_fm_qt)
target_compile_options(z7_fm_qt PRIVATE -Wno-non-virtual-dtor)
target_compile_definitions(z7_fm_qt PRIVATE Z7_QT_GUI)
target_include_directories(z7_fm_qt PRIVATE "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_fm_qt PUBLIC z7_gui)

add_executable(7zFM Main.cpp)
z7_port_target(7zFM)
target_compile_options(7zFM PRIVATE -Wno-non-virtual-dtor)
z7_link_ui(7zFM FM)
target_link_libraries(7zFM PRIVATE "$<LINK_GROUP:RESCAN,z7_fm,z7_fm_linux>" z7_gui z7_theme)
