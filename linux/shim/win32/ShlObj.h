/* ShlObj.h -- the shell types and calls that Windows/Shell.h and the 7-Zip
   GUI code use. linux/shim implements the calls (ShimShell.cpp); the
   folder browser of Windows/Shell.h is a Qt file dialog there. */

#ifndef Z7_LINUX_SHIM_SHLOBJ_H
#define Z7_LINUX_SHIM_SHLOBJ_H

#include "shellapi.h"
#include "ShObjIdl.h"

typedef struct _SHITEMID
{
  WORD cb;
  BYTE abID[1];
} SHITEMID;

typedef struct _ITEMIDLIST
{
  SHITEMID mkid;
} ITEMIDLIST;

typedef ITEMIDLIST *LPITEMIDLIST;
typedef const ITEMIDLIST *LPCITEMIDLIST;


typedef int (CALLBACK *BFFCALLBACK)(HWND wnd, UINT message, LPARAM lParam, LPARAM data);

typedef struct _browseinfoA
{
  HWND hwndOwner;
  LPCITEMIDLIST pidlRoot;
  LPSTR pszDisplayName;
  LPCSTR lpszTitle;
  UINT ulFlags;
  BFFCALLBACK lpfn;
  LPARAM lParam;
  int iImage;
} BROWSEINFOA, *LPBROWSEINFOA;

typedef struct _browseinfoW
{
  HWND hwndOwner;
  LPCITEMIDLIST pidlRoot;
  LPWSTR pszDisplayName;
  LPCWSTR lpszTitle;
  UINT ulFlags;
  BFFCALLBACK lpfn;
  LPARAM lParam;
  int iImage;
} BROWSEINFOW, *LPBROWSEINFOW;

typedef BROWSEINFOA BROWSEINFO;
typedef LPBROWSEINFOA LPBROWSEINFO;

/* Special folders: SHGetSpecialFolderLocation() returns an ITEMIDLIST that
   names the folder; free it with CoTaskMemFree(). */
#define CSIDL_DESKTOP  0x0000
#define CSIDL_PERSONAL 0x0005
#define CSIDL_DRIVES   0x0011
#define CSIDL_NETWORK  0x0012
#define CSIDL_PROFILE  0x0028

HRESULT SHGetSpecialFolderLocation(HWND owner, int folder, LPITEMIDLIST *pidl);

/* Data of drag and drop: the files of CF_HDROP, and the names of the
   formats that drop targets and sources exchange. */
typedef struct _DROPFILES
{
  DWORD pFiles;   // offset of the names
  POINT pt;
  BOOL fNC;
  BOOL fWide;     // wchar_t names
} DROPFILES, *LPDROPFILES;

#define CFSTR_PREFERREDDROPEFFECT        TEXT("Preferred DropEffect")
#define CFSTR_PERFORMEDDROPEFFECT        TEXT("Performed DropEffect")
#define CFSTR_PASTESUCCEEDED             TEXT("Paste Succeeded")
#define CFSTR_LOGICALPERFORMEDDROPEFFECT TEXT("Logical Performed DropEffect")
#define CFSTR_TARGETCLSID                TEXT("TargetCLSID")

#endif
