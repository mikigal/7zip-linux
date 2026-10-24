/* shellapi.h -- the shell32 calls of the 7-Zip GUI code: file
   information, file operations, starting programs and icon extraction.
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


/* File operations: FO_DELETE with FOF_ALLOWUNDO moves the files to the
   freedesktop trash, and asks before deleting files that cannot go there;
   the other operations are not supported. */
#define FO_MOVE   0x0001
#define FO_COPY   0x0002
#define FO_DELETE 0x0003
#define FO_RENAME 0x0004
#define FOF_MULTIDESTFILES     0x0001
#define FOF_CONFIRMMOUSE       0x0002
#define FOF_SILENT             0x0004
#define FOF_RENAMEONCOLLISION  0x0008
#define FOF_NOCONFIRMATION     0x0010
#define FOF_WANTMAPPINGHANDLE  0x0020
#define FOF_ALLOWUNDO          0x0040
#define FOF_FILESONLY          0x0080
#define FOF_SIMPLEPROGRESS     0x0100
#define FOF_NOCONFIRMMKDIR     0x0200
#define FOF_NOERRORUI          0x0400
#define FOF_WANTNUKEWARNING    0x4000
typedef WORD FILEOP_FLAGS;

typedef struct _SHFILEOPSTRUCTA
{
  HWND hwnd;
  UINT wFunc;
  LPCSTR pFrom;
  LPCSTR pTo;
  FILEOP_FLAGS fFlags;
  BOOL fAnyOperationsAborted;
  LPVOID hNameMappings;
  LPCSTR lpszProgressTitle;
} SHFILEOPSTRUCTA, *LPSHFILEOPSTRUCTA;

typedef struct _SHFILEOPSTRUCTW
{
  HWND hwnd;
  UINT wFunc;
  LPCWSTR pFrom;
  LPCWSTR pTo;
  FILEOP_FLAGS fFlags;
  BOOL fAnyOperationsAborted;
  LPVOID hNameMappings;
  LPCWSTR lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;

typedef SHFILEOPSTRUCTA SHFILEOPSTRUCT;
typedef LPSHFILEOPSTRUCTA LPSHFILEOPSTRUCT;

int SHFileOperationA(LPSHFILEOPSTRUCTA op);
int SHFileOperationW(LPSHFILEOPSTRUCTW op);
#define SHFileOperation SHFileOperationA

/* Starting programs: a file is opened with the desktop's default
   application; there is no process handle to wait for. */
#define SEE_MASK_DEFAULT        0x00000000
#define SEE_MASK_CLASSNAME      0x00000001
#define SEE_MASK_CLASSKEY       0x00000003
#define SEE_MASK_IDLIST         0x00000004
#define SEE_MASK_INVOKEIDLIST   0x0000000C
#define SEE_MASK_NOCLOSEPROCESS 0x00000040
#define SEE_MASK_NOASYNC        0x00000100
#define SEE_MASK_FLAG_DDEWAIT   SEE_MASK_NOASYNC
#define SEE_MASK_DOENVSUBST     0x00000200
#define SEE_MASK_FLAG_NO_UI     0x00000400
#define SEE_MASK_UNICODE        0x00004000
#define SEE_MASK_NO_CONSOLE     0x00008000

#define SE_ERR_FNF             2
#define SE_ERR_PNF             3
#define SE_ERR_ACCESSDENIED    5
#define SE_ERR_OOM             8
#define SE_ERR_SHARE           26
#define SE_ERR_ASSOCINCOMPLETE 27
#define SE_ERR_DDETIMEOUT      28
#define SE_ERR_DDEFAIL         29
#define SE_ERR_DDEBUSY         30
#define SE_ERR_NOASSOC         31

typedef struct _SHELLEXECUTEINFOA
{
  DWORD cbSize;
  ULONG fMask;
  HWND hwnd;
  LPCSTR lpVerb;
  LPCSTR lpFile;
  LPCSTR lpParameters;
  LPCSTR lpDirectory;
  int nShow;
  HINSTANCE hInstApp;
  void *lpIDList;
  LPCSTR lpClass;
  HKEY hkeyClass;
  DWORD dwHotKey;
  union
  {
    HANDLE hIcon;
    HANDLE hMonitor;
  };
  HANDLE hProcess;
} SHELLEXECUTEINFOA, *LPSHELLEXECUTEINFOA;

typedef struct _SHELLEXECUTEINFOW
{
  DWORD cbSize;
  ULONG fMask;
  HWND hwnd;
  LPCWSTR lpVerb;
  LPCWSTR lpFile;
  LPCWSTR lpParameters;
  LPCWSTR lpDirectory;
  int nShow;
  HINSTANCE hInstApp;
  void *lpIDList;
  LPCWSTR lpClass;
  HKEY hkeyClass;
  DWORD dwHotKey;
  union
  {
    HANDLE hIcon;
    HANDLE hMonitor;
  };
  HANDLE hProcess;
} SHELLEXECUTEINFOW, *LPSHELLEXECUTEINFOW;

typedef SHELLEXECUTEINFOA SHELLEXECUTEINFO;
typedef LPSHELLEXECUTEINFOA LPSHELLEXECUTEINFO;

BOOL ShellExecuteExA(LPSHELLEXECUTEINFOA info);
BOOL ShellExecuteExW(LPSHELLEXECUTEINFOW info);
#define ShellExecuteEx ShellExecuteExA

/* Icons of executables and DLLs: there are none on Linux (returns 0). */
UINT ExtractIconExA(LPCSTR file, int index, HICON *large, HICON *small, UINT numIcons);
UINT ExtractIconExW(LPCWSTR file, int index, HICON *large, HICON *small, UINT numIcons);
#define ExtractIconEx ExtractIconExA

/* Change notifications of the shell: nothing listens on Linux. */
#define SHCNE_ASSOCCHANGED 0x08000000L
#define SHCNE_UPDATEDIR    0x00001000L
#define SHCNF_IDLIST       0x0000
#define SHCNF_PATHA        0x0001
#define SHCNF_PATHW        0x0005
#define SHCNF_FLUSH        0x1000
void SHChangeNotify(LONG eventId, UINT flags, LPCVOID item1, LPCVOID item2);

/* Files dropped with WM_DROPFILES (not used: drops come through Qt). */
struct HDROP__;
typedef HDROP__ *HDROP;
UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT size);
void DragFinish(HDROP drop);
void DragAcceptFiles(HWND wnd, BOOL accept);

#endif
