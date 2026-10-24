/* ShObjIdl.h -- the shell interfaces of the SDK that the 7-Zip GUI code
   uses:
   - ITaskbarList and ITaskbarList2, which FileManager/MyWindowsNew.h
     expects (it defines ITaskbarList3 on top of them). CoCreateInstance()
     has no taskbar class yet, so the progress dialog runs without taskbar
     progress.
   - IContextMenu, IShellExtInit and IShellFolder, for the context menus of
     the file manager: the 7-Zip submenu (Explorer/ContextMenu.cpp) is an
     IContextMenu of 7-Zip itself; the shell has no desktop folder on Linux
     (SHGetDesktopFolder() fails), so there is no system menu part. */

#ifndef Z7_LINUX_SHIM_SHOBJIDL_H
#define Z7_LINUX_SHIM_SHOBJIDL_H

#include <windows.h>
#include <CommCtrl.h>

#ifndef __ITaskbarList_INTERFACE_DEFINED__
#define __ITaskbarList_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITaskbarList, 0x56FDF342, 0xFD6D, 0x11d0, 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90);
DEFINE_GUID(CLSID_TaskbarList, 0x56FDF344, 0xFD6D, 0x11d0, 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90);
struct ITaskbarList: public IUnknown
{
  STDMETHOD(HrInit)(void) = 0;
  STDMETHOD(AddTab)(HWND hwnd) = 0;
  STDMETHOD(DeleteTab)(HWND hwnd) = 0;
  STDMETHOD(ActivateTab)(HWND hwnd) = 0;
  STDMETHOD(SetActiveAlt)(HWND hwnd) = 0;
};
#endif

#ifndef __ITaskbarList2_INTERFACE_DEFINED__
#define __ITaskbarList2_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITaskbarList2, 0x602D4995, 0xB13A, 0x429b, 0xA6, 0x6E, 0x19, 0x35, 0xE4, 0x4F, 0x43, 0x17);
struct ITaskbarList2: public ITaskbarList
{
  STDMETHOD(MarkFullscreenWindow)(HWND hwnd, BOOL fFullscreen) = 0;
};
#endif


DEFINE_GUID(GUID_NULL, 0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

struct _ITEMIDLIST;
struct IBindCtx: public IUnknown {};
struct IMalloc;

typedef ULONG SFGAOF;
typedef DWORD SHCONTF;
typedef DWORD SHGDNF;

#define SFGAO_FOLDER     0x20000000L
#define SFGAO_FILESYSTEM 0x40000000L
#define SHCONTF_FOLDERS    0x0020
#define SHCONTF_NONFOLDERS 0x0040
#define SHGDN_NORMAL       0x0000
#define SHGDN_INFOLDER     0x0001
#define SHGDN_FORPARSING   0x8000

typedef struct _STRRET
{
  UINT uType;
  union
  {
    LPWSTR pOleStr;
    UINT uOffset;
    char cStr[260];
  };
} STRRET;

struct IEnumIDList: public IUnknown
{
  STDMETHOD(Next)(ULONG celt, struct _ITEMIDLIST **rgelt, ULONG *pceltFetched) = 0;
  STDMETHOD(Skip)(ULONG celt) = 0;
  STDMETHOD(Reset)() = 0;
  STDMETHOD(Clone)(IEnumIDList **ppenum) = 0;
};

DEFINE_GUID(IID_IShellFolder, 0x000214E6, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
struct IShellFolder: public IUnknown
{
  STDMETHOD(ParseDisplayName)(HWND hwnd, IBindCtx *pbc, LPWSTR pszDisplayName, ULONG *pchEaten,
      struct _ITEMIDLIST **ppidl, ULONG *pdwAttributes) = 0;
  STDMETHOD(EnumObjects)(HWND hwnd, SHCONTF grfFlags, IEnumIDList **ppenumIDList) = 0;
  STDMETHOD(BindToObject)(const struct _ITEMIDLIST *pidl, IBindCtx *pbc, REFIID riid, void **ppv) = 0;
  STDMETHOD(BindToStorage)(const struct _ITEMIDLIST *pidl, IBindCtx *pbc, REFIID riid, void **ppv) = 0;
  STDMETHOD(CompareIDs)(LPARAM lParam, const struct _ITEMIDLIST *pidl1, const struct _ITEMIDLIST *pidl2) = 0;
  STDMETHOD(CreateViewObject)(HWND hwndOwner, REFIID riid, void **ppv) = 0;
  STDMETHOD(GetAttributesOf)(UINT cidl, const struct _ITEMIDLIST **apidl, SFGAOF *rgfInOut) = 0;
  STDMETHOD(GetUIObjectOf)(HWND hwndOwner, UINT cidl, const struct _ITEMIDLIST **apidl, REFIID riid,
      UINT *rgfReserved, void **ppv) = 0;
  STDMETHOD(GetDisplayNameOf)(const struct _ITEMIDLIST *pidl, SHGDNF uFlags, STRRET *pName) = 0;
  STDMETHOD(SetNameOf)(HWND hwnd, const struct _ITEMIDLIST *pidl, LPCWSTR pszName, SHGDNF uFlags,
      struct _ITEMIDLIST **ppidlOut) = 0;
};
typedef IShellFolder *LPSHELLFOLDER;

/* Fails with E_NOTIMPL: Linux has no shell namespace. */
HRESULT SHGetDesktopFolder(IShellFolder **folder);

/* IContextMenu */
#define CMF_NORMAL        0x00000000
#define CMF_DEFAULTONLY   0x00000001
#define CMF_VERBSONLY     0x00000002
#define CMF_EXPLORE       0x00000004
#define CMF_NOVERBS       0x00000008
#define CMF_CANRENAME     0x00000010
#define CMF_NODEFAULT     0x00000020
#define CMF_INCLUDESTATIC 0x00000040
#define CMF_EXTENDEDVERBS 0x00000100
#define CMF_RESERVED      0xffff0000

#define GCS_VERBA     0x00000000
#define GCS_HELPTEXTA 0x00000001
#define GCS_VALIDATEA 0x00000002
#define GCS_VERBW     0x00000004
#define GCS_HELPTEXTW 0x00000005
#define GCS_VALIDATEW 0x00000006
#define GCS_UNICODE   0x00000004
#define GCS_VERB      GCS_VERBA
#define GCS_HELPTEXT  GCS_HELPTEXTA
#define GCS_VALIDATE  GCS_VALIDATEA

#define CMIC_MASK_HOTKEY       0x00000020
#define CMIC_MASK_ICON         0x00000010
#define CMIC_MASK_FLAG_NO_UI   0x00000400
#define CMIC_MASK_UNICODE      0x00004000
#define CMIC_MASK_NO_CONSOLE   0x00008000
#define CMIC_MASK_ASYNCOK      0x00100000
#define CMIC_MASK_NOASYNC      0x00000100
#define CMIC_MASK_SHIFT_DOWN   0x10000000
#define CMIC_MASK_CONTROL_DOWN 0x40000000
#define CMIC_MASK_PTINVOKE     0x20000000

typedef struct _CMINVOKECOMMANDINFO
{
  DWORD cbSize;
  DWORD fMask;
  HWND hwnd;
  LPCSTR lpVerb;
  LPCSTR lpParameters;
  LPCSTR lpDirectory;
  int nShow;
  DWORD dwHotKey;
  HANDLE hIcon;
} CMINVOKECOMMANDINFO, *LPCMINVOKECOMMANDINFO;

typedef struct _CMInvokeCommandInfoEx
{
  DWORD cbSize;
  DWORD fMask;
  HWND hwnd;
  LPCSTR lpVerb;
  LPCSTR lpParameters;
  LPCSTR lpDirectory;
  int nShow;
  DWORD dwHotKey;
  HANDLE hIcon;
  LPCSTR lpTitle;
  LPCWSTR lpVerbW;
  LPCWSTR lpParametersW;
  LPCWSTR lpDirectoryW;
  LPCWSTR lpTitleW;
  POINT ptInvoke;
} CMINVOKECOMMANDINFOEX, *LPCMINVOKECOMMANDINFOEX;

DEFINE_GUID(IID_IContextMenu, 0x000214E4, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
struct IContextMenu: public IUnknown
{
  STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags) = 0;
  STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO pici) = 0;
  STDMETHOD(GetCommandString)(UINT_PTR idCmd, UINT uType, UINT *pReserved, LPSTR pszName, UINT cchMax) = 0;
};
typedef IContextMenu *LPCONTEXTMENU;

DEFINE_GUID(IID_IContextMenu2, 0x000214F4, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
struct IContextMenu2: public IContextMenu
{
  STDMETHOD(HandleMenuMsg)(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
};

DEFINE_GUID(IID_IContextMenu3, 0xBCFCE0A0, 0xEC17, 0x11D0, 0x8D, 0x10, 0x00, 0xA0, 0xC9, 0x0F, 0x27, 0x19);
struct IContextMenu3: public IContextMenu2
{
  STDMETHOD(HandleMenuMsg2)(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *plResult) = 0;
};

DEFINE_GUID(IID_IShellExtInit, 0x000214E8, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
struct IShellExtInit: public IUnknown
{
  STDMETHOD(Initialize)(const struct _ITEMIDLIST *pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID) = 0;
};

#endif
