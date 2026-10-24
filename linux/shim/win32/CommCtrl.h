/* CommCtrl.h -- the common-controls subset used by the 7-Zip resource
   scripts and GUI code on Linux. Constants have their Win32 values (see
   windows.h). */

#ifndef Z7_LINUX_SHIM_COMMCTRL_H
#define Z7_LINUX_SHIM_COMMCTRL_H

#include <windows.h>

/* Window classes */
#define WC_LISTVIEWA      "SysListView32"
#define PROGRESS_CLASSA   "msctls_progress32"
#define UPDOWN_CLASSA     "msctls_updown32"
#define TRACKBAR_CLASSA   "msctls_trackbar32"

/* List view styles */
#define LVS_ICON            0x0000
#define LVS_REPORT          0x0001
#define LVS_SMALLICON       0x0002
#define LVS_LIST            0x0003
#define LVS_TYPEMASK        0x0003
#define LVS_SINGLESEL       0x0004
#define LVS_SHOWSELALWAYS   0x0008
#define LVS_SORTASCENDING   0x0010
#define LVS_SORTDESCENDING  0x0020
#define LVS_SHAREIMAGELISTS 0x0040
#define LVS_NOLABELWRAP     0x0080
#define LVS_AUTOARRANGE     0x0100
#define LVS_EDITLABELS      0x0200
#define LVS_OWNERDRAWFIXED  0x0400
#define LVS_ALIGNTOP        0x0000
#define LVS_ALIGNLEFT       0x0800
#define LVS_OWNERDATA       0x1000
#define LVS_NOSCROLL        0x2000
#define LVS_NOCOLUMNHEADER  0x4000
#define LVS_NOSORTHEADER    0x8000

/* Up-down control styles */
#define UDS_WRAP        0x0001
#define UDS_SETBUDDYINT 0x0002
#define UDS_ALIGNRIGHT  0x0004
#define UDS_ALIGNLEFT   0x0008
#define UDS_AUTOBUDDY   0x0010
#define UDS_ARROWKEYS   0x0020
#define UDS_HORZ        0x0040
#define UDS_NOTHOUSANDS 0x0080
#define UDS_HOTTRACK    0x0100

/* Progress bar styles */
#define PBS_SMOOTH   0x01
#define PBS_VERTICAL 0x04
#define PBS_MARQUEE  0x08

/* Trackbar styles */
#define TBS_AUTOTICKS      0x0001
#define TBS_VERT           0x0002
#define TBS_HORZ           0x0000
#define TBS_TOP            0x0004
#define TBS_BOTTOM         0x0000
#define TBS_LEFT           0x0004
#define TBS_RIGHT          0x0000
#define TBS_BOTH           0x0008
#define TBS_NOTICKS        0x0010
#define TBS_ENABLESELRANGE 0x0020
#define TBS_FIXEDLENGTH    0x0040
#define TBS_NOTHUMB        0x0080
#define TBS_TOOLTIPS       0x0100

#endif
