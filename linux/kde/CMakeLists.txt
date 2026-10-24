# The 7-Zip context menu of KDE file managers: a KFileItemActions plugin
# (sevenzipfileitemaction) with the menu that upstream Explorer/ContextMenu.cpp
# builds for the Windows shell extension (linux/shell). Without KF6 KIO the
# service menus of linux/packaging give a fixed menu instead.

find_package(KF6KIO QUIET)
if(NOT KF6KIO_FOUND)
  message(STATUS "KDE context menu plugin disabled (need KF6 KIO); the service menus are installed")
  set(Z7_KDE_PLUGIN OFF CACHE INTERNAL "")
  return()
endif()
set(Z7_KDE_PLUGIN ON CACHE INTERNAL "")

add_library(sevenzipfileitemaction MODULE FileItemAction.cpp)
z7_port_target(sevenzipfileitemaction)
target_compile_options(sevenzipfileitemaction PRIVATE -Wno-non-virtual-dtor)
set_target_properties(sevenzipfileitemaction PROPERTIES
  AUTOMOC ON
  # moc embeds the metadata file of K_PLUGIN_CLASS_WITH_JSON: a change to it
  # runs moc again (ECM sets the same filter)
  AUTOMOC_DEPEND_FILTERS "K_PLUGIN_CLASS_WITH_JSON;[\n][ \t]*K_PLUGIN_CLASS_WITH_JSON[ \t\n]*\\([^,]*,[ \t\n]*\"([^\"]+)\""
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
  "$<LINK_GROUP:RESCAN,z7_shell,z7_shell_menu,z7_7zg_linux,z7_gui,z7_shim,z7_platform,z7_ui,z7_rc_fm,z7_core_lib>")

install(TARGETS sevenzipfileitemaction
  LIBRARY DESTINATION "${QT6_INSTALL_PLUGINS}/kf6/kfileitemaction")
