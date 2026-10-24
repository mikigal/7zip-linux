# Installation: the programs, their .desktop files (named by the application
# ID of platform/AppId.h), the application icon in the hicolor theme, and
# the MIME package with the archive types that shared-mime-info lacks, and
# the service menus of Dolphin (7zG --context ...) when the KDE plugin
# (linux/kde, which installs itself) is not built.
# Lang files and the help pages come with the package layout (PLAN.md,
# Phase 7).

include(GNUInstallDirs)

set(Z7_APP_ID "pl.mikigal.SevenZip")

install(TARGETS 7zFM 7zG RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${Z7_APP_ID}.desktop ${Z7_APP_ID}.Gui.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${Z7_ROOT}/linux/theme/icons/app/7-Zip.svg"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
  RENAME ${Z7_APP_ID}.svg)
install(FILES ${Z7_APP_ID}.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
# the 7-Zip submenu of Dolphin (KIO service menus); the plugin has the
# whole menu, and both would show it twice
if(NOT Z7_KDE_PLUGIN)
  install(FILES ${Z7_APP_ID}.servicemenu-compress.desktop ${Z7_APP_ID}.servicemenu-extract.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/kio/servicemenus")
endif()
