// Win32Compat.h -- Win32 names used by the upstream GUI-side code (UI/Agent,
// UI/FileManager, UI/GUI) that CPP/Common/MyWindows.h does not provide on
// POSIX.
//
// The Linux port force-includes this header (-include) into the upstream
// sources it compiles for 7zFM and 7zG, so those files build unchanged. The
// console 7zz never sees it.

#ifndef ZIP7_INC_LINUX_WIN32_COMPAT_H
#define ZIP7_INC_LINUX_WIN32_COMPAT_H

#ifdef _WIN32
#error "linux/platform is for non-Windows builds only"
#endif

#ifdef __cplusplus

#include <errno.h>
#include <stdint.h>

#include "../../CPP/Common/Common.h"

typedef UInt64 UINT64;
typedef BYTE *LPBYTE;
typedef DWORD *LPDWORD;
typedef TCHAR *LPTSTR;

// Windows/DLL.h uses the same definition on POSIX.
typedef void *HMODULE;
typedef void *HINSTANCE;

#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

// FileManager code assumes the Windows (wchar_t) API on NT.
extern bool g_IsNT;

// The command line of the program (linux/platform/CommandLine.cpp): the
// arguments quoted so that NCommandLineParser::SplitCommandLine() gives
// them back unchanged.
WCHAR *GetCommandLineW();

// Window handles are opaque here; the GUI shim defines HWND__.
struct HWND__;
typedef HWND__ *HWND;

typedef struct tagRECT
{
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT, *LPRECT;

// Locale (linux/platform/Locale.cpp maps the POSIX locale to a LANGID).
typedef WORD LANGID;
#define MAKELANGID(p, s)    ((LANGID)((((WORD)(s)) << 10) | (WORD)(p)))
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define SUBLANGID(lgid)     ((WORD)(lgid) >> 10)
LANGID GetUserDefaultLangID();
LANGID GetSystemDefaultLangID();

// Error codes. As in C/7zTypes.h, errno values stand in for Win32 errors
// that have an errno equivalent, and FACILITY_WIN32 codes for the rest.
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0L
#endif
#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED EACCES
#endif
#ifndef ERROR_INVALID_DATA
#define ERROR_INVALID_DATA ((HRESULT)0x8007000DL)
#endif
#ifndef ERROR_UNSUPPORTED_TYPE
#define ERROR_UNSUPPORTED_TYPE ((HRESULT)0x8007065EL)
#endif

// Registry: Windows/Registry.h compiles unchanged against these names, and
// linux/platform/Registry.cpp implements its CKey class on top of an INI file.
struct HKEY__;
typedef HKEY__ *HKEY;
typedef DWORD REGSAM;
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;

#define HKEY_CLASSES_ROOT   ((HKEY)(uintptr_t)0x80000000u)
#define HKEY_CURRENT_USER   ((HKEY)(uintptr_t)0x80000001u)
#define HKEY_LOCAL_MACHINE  ((HKEY)(uintptr_t)0x80000002u)

#define REG_NONE       0
#define REG_SZ         1
#define REG_EXPAND_SZ  2
#define REG_BINARY     3
#define REG_DWORD      4
#define REG_MULTI_SZ   7
#define REG_QWORD     11

#define REG_OPTION_NON_VOLATILE 0
#define REG_CREATED_NEW_KEY     1
#define REG_OPENED_EXISTING_KEY 2

#define KEY_QUERY_VALUE        0x0001
#define KEY_SET_VALUE          0x0002
#define KEY_CREATE_SUB_KEY     0x0004
#define KEY_ENUMERATE_SUB_KEYS 0x0008
#define KEY_READ               0x20019
#define KEY_WRITE              0x20006
#define KEY_ALL_ACCESS         0xF003F

// Declared for the private inline CKey::QueryValueEx() in Windows/Registry.h.
// The INI backend does not use or define it.
LONG RegQueryValueEx(HKEY key, LPCTSTR valueName, LPDWORD reserved,
    LPDWORD type, LPBYTE data, LPDWORD dataSize);

#endif // __cplusplus

#endif
