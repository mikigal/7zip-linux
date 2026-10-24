# File manager integration: the 7-Zip context menu of the Windows shell
# extension. Upstream Explorer/ContextMenu.cpp builds it on the shim
# (z7_shell_menu); z7_shell has the Linux side: the commands that its items
# run (CompressCallPosix.cpp) and the menu as a tree (ShellMenu.h), which
# the KDE plugin shows and 7z-context-menu prints for the extensions of GTK
# file managers.

# The upstream menu, compiled like the 7zFM sources, and what the shell
# extension DLL defines for it.
add_library(z7_shell_menu STATIC "${Z7_ROOT}/CPP/7zip/UI/Explorer/ContextMenu.cpp" ShellGuids.cpp)
target_compile_options(z7_shell_menu PRIVATE ${Z7_UPSTREAM_WARNINGS}
  "SHELL:-include ${Z7_SHIM_INCLUDE}/windows.h"
  "SHELL:-include ${Z7_ROOT}/linux/platform/UpstreamPosix.h")
target_include_directories(z7_shell_menu PRIVATE "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_shell_menu PUBLIC z7_gui)

# They include Qt before windows.h, which defines _WIN64 (a Windows build
# for Qt), so windows.h is not force-included.
add_library(z7_shell STATIC CompressCallPosix.cpp ShellMenu.cpp)
z7_port_target(z7_shell)
target_compile_options(z7_shell PRIVATE -Wno-non-virtual-dtor)
target_include_directories(z7_shell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(z7_shell PUBLIC z7_shell_menu z7_7zg_linux z7_gui)
# the folder of 7zG and 7zFM for the plugin and 7z-context-menu
target_compile_definitions(z7_shell PRIVATE Z7_INSTALL_PROGRAM_DIR="${Z7_INSTALL_FULL_PROGRAM_DIR}")

# The menu for GTK file managers (ContextMenuTool.cpp). The core goes in as
# a static library: only what the menu uses, not the archive handlers.
add_executable(7z-context-menu ContextMenuTool.cpp)
z7_port_target(7z-context-menu)
target_compile_options(7z-context-menu PRIVATE -Wno-non-virtual-dtor)
target_link_libraries(7z-context-menu PRIVATE
  "$<LINK_GROUP:RESCAN,z7_shell,z7_shell_menu,z7_7zg_linux,z7_gui,z7_shim,z7_platform,z7_ui,z7_rc_fm,z7_core_lib>")
install(TARGETS 7z-context-menu RUNTIME DESTINATION "${Z7_PROGRAM_SUBDIR}")
