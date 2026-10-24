// AppId.h -- the application ID of the Linux port: the base name of its
// .desktop files and the Wayland app ID. It must be a valid D-Bus name
// (segments do not start with a digit). Provisional: linux/docs/PLAN.md,
// Appendix C.

#ifndef ZIP7_INC_LINUX_APP_ID_H
#define ZIP7_INC_LINUX_APP_ID_H

#define Z7_APP_ID "io.github.mikigal.SevenZip"

// the .desktop file of 7zFM, which opens archives
#define Z7_FM_DESKTOP_FILE Z7_APP_ID ".desktop"

#endif
