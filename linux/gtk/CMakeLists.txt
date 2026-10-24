# The 7-Zip context menu of GTK file managers: an extension module for each
# of Nemo, Thunar and Nautilus, built when its extension library is found.
# They cannot load Qt into the file manager, so they run 7z-context-menu
# (linux/shell), which prints the upstream menu with the command of each
# item (MenuTool.c reads it).

find_package(PkgConfig QUIET)
if(NOT PKG_CONFIG_FOUND)
  message(STATUS "GTK file manager extensions disabled (need pkg-config)")
  return()
endif()
pkg_check_modules(Z7_GIO IMPORTED_TARGET gio-2.0)
if(NOT Z7_GIO_FOUND)
  message(STATUS "GTK file manager extensions disabled (need GIO)")
  return()
endif()

include(GNUInstallDirs)

add_library(z7_gtk_menu STATIC MenuTool.c)
set_target_properties(z7_gtk_menu PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
  C_VISIBILITY_PRESET hidden)
target_compile_options(z7_gtk_menu PRIVATE ${Z7_PORT_C_WARNINGS})
target_compile_definitions(z7_gtk_menu PRIVATE
  Z7_CONTEXT_MENU_TOOL="${CMAKE_INSTALL_FULL_LIBEXECDIR}/7z-context-menu")
target_include_directories(z7_gtk_menu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${Z7_ROOT}/linux/platform")
target_link_libraries(z7_gtk_menu PUBLIC PkgConfig::Z7_GIO)
