// AppId.h -- the application ID of the Linux port: the base name of its
// .desktop files and the Wayland app ID. It must be a valid D-Bus name
// (segments do not start with a digit, so org.7-zip.* is not one).

#ifndef ZIP7_INC_LINUX_APP_ID_H
#define ZIP7_INC_LINUX_APP_ID_H

// 7zFM, the program that opens archives
#define Z7_APP_ID "pl.mikigal.SevenZip"
#define Z7_FM_DESKTOP_FILE Z7_APP_ID ".desktop"

// 7zG, the dialogs that file manager context menus run
#define Z7_GUI_APP_ID Z7_APP_ID ".Gui"

#endif
