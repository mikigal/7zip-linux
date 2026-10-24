/* HtmlHelp.h -- the help viewer call of FileManager/HelpUtils.cpp.

   HelpUtils passes "<program dir>/7-zip.chm::/<topic>". linux/shim
   (ShimShell.cpp) opens the topic with the desktop's URL handler: from a
   local copy of the help pages when there is one, otherwise from the online
   documentation. */

#ifndef Z7_LINUX_SHIM_HTMLHELP_H
#define Z7_LINUX_SHIM_HTMLHELP_H

#include <windows.h>

#define HH_DISPLAY_TOPIC 0x0000

HWND HtmlHelpA(HWND caller, LPCSTR file, UINT command, DWORD_PTR data);
HWND HtmlHelpW(HWND caller, LPCWSTR file, UINT command, DWORD_PTR data);
#define HtmlHelp HtmlHelpA

#endif
