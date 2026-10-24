/* Shlwapi.h -- GUI/GUI.cpp includes it for DllGetVersion(), which it calls
   only when Z7_WIN32_WINNT_MIN is below Windows 2000. winuser.h sets a
   Vista minimum, so nothing is needed from here. */

#ifndef Z7_LINUX_SHIM_SHLWAPI_H
#define Z7_LINUX_SHIM_SHLWAPI_H

#include <windows.h>

#endif
