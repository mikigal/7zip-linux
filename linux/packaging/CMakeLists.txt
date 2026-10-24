# Installation: the programs, their .desktop files (named by the application
# ID of platform/AppId.h), the application icon in the hicolor theme, and
# the MIME package with the archive types that shared-mime-info lacks.
# Lang files and the help pages come with the package layout (PLAN.md,
# Phase 7).

include(GNUInstallDirs)

set(Z7_APP_ID "io.github.mikigal.SevenZip")

install(TARGETS 7zFM 7zG RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${Z7_APP_ID}.desktop ${Z7_APP_ID}.Gui.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${Z7_ROOT}/linux/theme/icons/app/7-Zip.svg"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
  RENAME ${Z7_APP_ID}.svg)
install(FILES ${Z7_APP_ID}.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
