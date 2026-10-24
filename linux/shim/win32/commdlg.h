/* commdlg.h -- the common dialog errors that the 7-Zip GUI code reads.
   windows.h includes it for C++. The file dialogs themselves are Qt file
   dialogs behind NWindows::CCommonDialogInfo (ShimShell.cpp). */

#ifndef Z7_LINUX_SHIM_COMMDLG_H
#define Z7_LINUX_SHIM_COMMDLG_H

#include "winuser.h"

#define CDERR_GENERALCODES    0x0000
#define CDERR_DIALOGFAILURE   0xFFFF
#define FNERR_FILENAMECODES   0x3000
#define FNERR_SUBCLASSFAILURE 0x3001
#define FNERR_INVALIDFILENAME 0x3002
#define FNERR_BUFFERTOOSMALL  0x3003

/* The error of the last file dialog on this thread; 0 after a cancel. */
DWORD CommDlgExtendedError();

#endif
