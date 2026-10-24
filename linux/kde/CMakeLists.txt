# The 7-Zip context menu of KDE file managers: a KFileItemActions plugin
# (sevenzipfileitemaction) with the menu that upstream Explorer/ContextMenu.cpp
# builds for the Windows shell extension. Without KF6 KIO the service menus
# of linux/packaging give a fixed menu instead.

find_package(KF6KIO QUIET)
if(NOT KF6KIO_FOUND)
  message(STATUS "KDE context menu plugin disabled (need KF6 KIO); the service menus are installed")
  set(Z7_KDE_PLUGIN OFF CACHE INTERNAL "")
  return()
endif()
set(Z7_KDE_PLUGIN ON CACHE INTERNAL "")

include(GNUInstallDirs)

# The upstream menu, compiled like the 7zFM sources, and what the shell
# extension DLL defines for it.
add_library(z7_kde_menu STATIC "${Z7_ROOT}/CPP/7zip/UI/Explorer/ContextMenu.cpp" ShellExtension.cpp)
target_compile_options(z7_kde_menu PRIVATE ${Z7_UPSTREAM_WARNINGS}
  "SHELL:-include ${Z7_SHIM_INCLUDE}/windows.h"
  "SHELL:-include ${Z7_ROOT}/linux/platform/UpstreamPosix.h")
target_include_directories(z7_kde_menu PRIVATE "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_kde_menu PUBLIC z7_gui)

# The commands that the menu runs. It includes Qt before windows.h, which
# defines _WIN64 (a Windows build for Qt), so windows.h is not force-included.
add_library(z7_kde_calls STATIC CompressCallPosix.cpp)
z7_port_target(z7_kde_calls)
target_compile_options(z7_kde_calls PRIVATE -Wno-non-virtual-dtor)
target_link_libraries(z7_kde_calls PUBLIC z7_7zg_linux z7_gui)

add_library(sevenzipfileitemaction MODULE FileItemAction.cpp)
z7_port_target(sevenzipfileitemaction)
target_compile_options(sevenzipfileitemaction PRIVATE -Wno-non-virtual-dtor)
target_compile_definitions(sevenzipfileitemaction PRIVATE
  Z7_INSTALL_PROGRAM_DIR="${CMAKE_INSTALL_FULL_BINDIR}")
set_target_properties(sevenzipfileitemaction PROPERTIES
  AUTOMOC ON
  PREFIX ""
  # the plugin runs in the file manager: only the Qt plugin entry points are
  # visible, not the Win32 functions of the shim
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/kf6/kfileitemaction")
target_link_options(sevenzipfileitemaction PRIVATE "LINKER:--exclude-libs,ALL" "LINKER:--no-undefined")
# The 7-Zip core goes in as a static library: only what the menu uses, not
# the archive handlers of the programs.
target_link_libraries(sevenzipfileitemaction PRIVATE KF6::KIOWidgets
  "$<LINK_GROUP:RESCAN,z7_kde_menu,z7_kde_calls,z7_7zg_linux,z7_gui,z7_shim,z7_platform,z7_ui,z7_rc_fm,z7_core_lib>")

install(TARGETS sevenzipfileitemaction
  LIBRARY DESTINATION "${QT6_INSTALL_PLUGINS}/kf6/kfileitemaction")
