# Installation: the programs in the program folder (Z7Common.cmake) with
# links in bin, their .desktop files (named by the application ID of
# platform/AppId.h), the application icon in the hicolor theme, the
# AppStream metadata, the MIME package with the archive types that
# shared-mime-info lacks, and the service menus of Dolphin (7zG --context
# ...) when the KDE plugin (linux/kde, which installs itself) is not built.
# The language files go with linux/lang, the context menu tool with
# linux/shell.

set(Z7_APP_ID "pl.mikigal.SevenZip")

install(TARGETS 7zFM 7zG RUNTIME DESTINATION "${Z7_PROGRAM_SUBDIR}")
# relative links, so that the installation can move (DESTDIR, packages)
file(RELATIVE_PATH Z7_BIN_TO_PROGRAMS "${CMAKE_INSTALL_FULL_BINDIR}" "${Z7_INSTALL_FULL_PROGRAM_DIR}")
foreach(program 7zFM 7zG)
  install(CODE "
    file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}\")
    file(CREATE_LINK \"${Z7_BIN_TO_PROGRAMS}/${program}\"
         \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${program}\" SYMBOLIC)
    message(STATUS \"Link: \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${program}\")")
endforeach()
install(FILES ${Z7_APP_ID}.desktop ${Z7_APP_ID}.Gui.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${Z7_ROOT}/linux/theme/icons/app/7-Zip.svg"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
  RENAME ${Z7_APP_ID}.svg)
install(FILES ${Z7_APP_ID}.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
install(FILES ${Z7_APP_ID}.metainfo.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
# the 7-Zip submenu of Dolphin (KIO service menus); the plugin has the
# whole menu, and both would show it twice
if(NOT Z7_KDE_PLUGIN)
  install(FILES ${Z7_APP_ID}.servicemenu-compress.desktop ${Z7_APP_ID}.servicemenu-extract.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/kio/servicemenus")
endif()
