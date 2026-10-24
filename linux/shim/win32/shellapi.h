/* shellapi.h -- the shell32 file information call of the 7-Zip GUI code.
   windows.h includes it for C++, as <windows.h> does on Windows.

   linux/shim implements SHGetFileInfo() with theme icons (ShimShell.cpp):
   the icon of a file comes from its MIME type (by name with
   SHGFI_USEFILEATTRIBUTES, else by name and content), and the "system image
   list" is one image list per icon size that grows as icons are asked for. */

#ifndef Z7_LINUX_SHIM_SHELLAPI_H
#define Z7_LINUX_SHIM_SHELLAPI_H

#include "winuser.h"

typedef struct _SHFILEINFOA
{
  HICON hIcon;
  int iIcon;
  DWORD dwAttributes;
  CHAR szDisplayName[MAX_PATH];
  CHAR szTypeName[80];
} SHFILEINFOA;

typedef struct _SHFILEINFOW
{
  HICON hIcon;
  int iIcon;
  DWORD dwAttributes;
  WCHAR szDisplayName[MAX_PATH];
  WCHAR szTypeName[80];
} SHFILEINFOW;

typedef SHFILEINFOA SHFILEINFO;

#define SHGFI_LARGEICON         0x000000000
#define SHGFI_SMALLICON         0x000000001
#define SHGFI_OPENICON          0x000000002
#define SHGFI_SHELLICONSIZE     0x000000004
#define SHGFI_PIDL              0x000000008
#define SHGFI_USEFILEATTRIBUTES 0x000000010
#define SHGFI_ICON              0x000000100
#define SHGFI_DISPLAYNAME       0x000000200
#define SHGFI_TYPENAME          0x000000400
#define SHGFI_ATTRIBUTES        0x000000800
#define SHGFI_SYSICONINDEX      0x000004000

/* With SHGFI_SYSICONINDEX it returns the system image list (HIMAGELIST),
   otherwise nonzero on success. A path given with SHGFI_PIDL is an
   ITEMIDLIST from SHGetSpecialFolderLocation() (ShlObj.h). */
DWORD_PTR SHGetFileInfoA(LPCSTR path, DWORD attrib, SHFILEINFOA *info, UINT infoSize, UINT flags);
DWORD_PTR SHGetFileInfoW(LPCWSTR path, DWORD attrib, SHFILEINFOW *info, UINT infoSize, UINT flags);
#define SHGetFileInfo SHGetFileInfoA

#endif
