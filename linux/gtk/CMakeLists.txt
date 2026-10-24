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

# z7_gtk_extension(<target> <source> <pkg-config module> <directory variable>)
# The extension module of a file manager, when its extension library is
# found; it goes to the extension directory of the file manager, under
# CMAKE_INSTALL_PREFIX. Only the module entry points are visible.
function(z7_gtk_extension target source module dirvar)
  pkg_check_modules(${target}_PKG IMPORTED_TARGET GLOBAL ${module})
  if(NOT ${target}_PKG_FOUND)
    message(STATUS "${target} disabled (need ${module})")
    return()
  endif()
  add_library(${target} MODULE ${source})
  set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
    C_VISIBILITY_PRESET hidden PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/gtk-extensions/${target}")
  target_compile_options(${target} PRIVATE ${Z7_PORT_C_WARNINGS})
  target_link_options(${target} PRIVATE "LINKER:--no-undefined")
  target_link_libraries(${target} PRIVATE z7_gtk_menu PkgConfig::${target}_PKG)
  pkg_get_variable(dir ${module} ${dirvar})
  pkg_get_variable(prefix ${module} prefix)
  if(prefix AND dir MATCHES "^${prefix}/")
    string(LENGTH "${prefix}/" len)
    string(SUBSTRING "${dir}" ${len} -1 dir)
  endif()
  install(TARGETS ${target} LIBRARY DESTINATION "${dir}")
endfunction()

z7_gtk_extension(libnemo-sevenzip NemoExtension.c libnemo-extension extensiondir)
z7_gtk_extension(thunar-sevenzip ThunarExtension.c thunarx-3 extensionsdir)
z7_gtk_extension(libnautilus-sevenzip NautilusExtension.c libnautilus-extension-4 extensiondir)
