/* prsht.h -- the property sheet subset used by the 7-Zip GUI code on Linux
   (Windows/Control/PropertyPage.cpp). linux/shim implements PropertySheet()
   as a dialog with a QTabWidget. */

#ifndef Z7_LINUX_SHIM_PRSHT_H
#define Z7_LINUX_SHIM_PRSHT_H

#include <windows.h>

#define PSP_DEFAULT      0x00000000
#define PSP_DLGINDIRECT  0x00000001
#define PSP_USEHICON     0x00000002
#define PSP_USEICONID    0x00000004
#define PSP_USETITLE     0x00000008
#define PSP_RTLREADING   0x00000010
#define PSP_HASHELP      0x00000020
#define PSP_USEREFPARENT 0x00000040
#define PSP_USECALLBACK  0x00000080

#define PSH_DEFAULT         0x00000000
#define PSH_PROPTITLE       0x00000001
#define PSH_USEHICON        0x00000002
#define PSH_USEICONID       0x00000004
#define PSH_PROPSHEETPAGE   0x00000008
#define PSH_WIZARD          0x00000020
#define PSH_USEPSTARTPAGE   0x00000040
#define PSH_NOAPPLYNOW      0x00000080
#define PSH_USECALLBACK     0x00000100
#define PSH_HASHELP         0x00000200
#define PSH_MODELESS        0x00000400

#define PSN_FIRST       (0U - 200U)
#define PSN_SETACTIVE   (PSN_FIRST - 0)
#define PSN_KILLACTIVE  (PSN_FIRST - 1)
#define PSN_APPLY       (PSN_FIRST - 2)
#define PSN_RESET       (PSN_FIRST - 3)
#define PSN_HELP        (PSN_FIRST - 5)
#define PSN_QUERYCANCEL (PSN_FIRST - 9)

#define PSNRET_NOERROR              0
#define PSNRET_INVALID              1
#define PSNRET_INVALID_NOCHANGEPAGE 2
#define PSNRET_MESSAGEHANDLED       3

#define PSM_SETCURSEL           (WM_USER + 101)
#define PSM_REMOVEPAGE          (WM_USER + 102)
#define PSM_ADDPAGE             (WM_USER + 103)
#define PSM_CHANGED             (WM_USER + 104)
#define PSM_RESTARTWINDOWS      (WM_USER + 105)
#define PSM_REBOOTSYSTEM        (WM_USER + 106)
#define PSM_CANCELTOCLOSE       (WM_USER + 107)
#define PSM_QUERYSIBLINGS       (WM_USER + 108)
#define PSM_UNCHANGED           (WM_USER + 109)
#define PSM_APPLY               (WM_USER + 110)
#define PSM_PRESSBUTTON         (WM_USER + 113)
#define PSM_GETTABCONTROL       (WM_USER + 116)
#define PSM_GETCURRENTPAGEHWND  (WM_USER + 118)

#define PSBTN_OK        1
#define PSBTN_CANCEL    5
#define PSBTN_APPLYNOW  6
#define PSBTN_HELP      8

/* Command IDs of the property sheet buttons */
#define ID_APPLY_NOW 0x3021

#define ID_PSRESTARTWINDOWS 0x2
#define ID_PSREBOOTSYSTEM   (ID_PSRESTARTWINDOWS | 0x1)

#if !defined(RC_INVOKED) && defined(__cplusplus)

struct _PSP;
typedef struct _PSP *HPROPSHEETPAGE;

struct _PROPSHEETPAGEA;
struct _PROPSHEETPAGEW;
typedef UINT (CALLBACK *LPFNPSPCALLBACKA)(HWND, UINT, struct _PROPSHEETPAGEA *);
typedef UINT (CALLBACK *LPFNPSPCALLBACKW)(HWND, UINT, struct _PROPSHEETPAGEW *);
typedef int (CALLBACK *PFNPROPSHEETCALLBACK)(HWND, UINT, LPARAM);

typedef struct _PROPSHEETPAGEA
{
  DWORD dwSize;
  DWORD dwFlags;
  HINSTANCE hInstance;
  LPCSTR pszTemplate;
  union
  {
    HICON hIcon;
    LPCSTR pszIcon;
  };
  LPCSTR pszTitle;
  DLGPROC pfnDlgProc;
  LPARAM lParam;
  LPFNPSPCALLBACKA pfnCallback;
  UINT *pcRefParent;
} PROPSHEETPAGEA, *LPPROPSHEETPAGEA;
typedef const PROPSHEETPAGEA *LPCPROPSHEETPAGEA;

typedef struct _PROPSHEETPAGEW
{
  DWORD dwSize;
  DWORD dwFlags;
  HINSTANCE hInstance;
  LPCWSTR pszTemplate;
  union
  {
    HICON hIcon;
    LPCWSTR pszIcon;
  };
  LPCWSTR pszTitle;
  DLGPROC pfnDlgProc;
  LPARAM lParam;
  LPFNPSPCALLBACKW pfnCallback;
  UINT *pcRefParent;
} PROPSHEETPAGEW, *LPPROPSHEETPAGEW;
typedef const PROPSHEETPAGEW *LPCPROPSHEETPAGEW;

typedef PROPSHEETPAGEA PROPSHEETPAGE;

typedef struct _PROPSHEETHEADERA
{
  DWORD dwSize;
  DWORD dwFlags;
  HWND hwndParent;
  HINSTANCE hInstance;
  union
  {
    HICON hIcon;
    LPCSTR pszIcon;
  };
  LPCSTR pszCaption;
  UINT nPages;
  union
  {
    UINT nStartPage;
    LPCSTR pStartPage;
  };
  union
  {
    LPCPROPSHEETPAGEA ppsp;
    HPROPSHEETPAGE *phpage;
  };
  PFNPROPSHEETCALLBACK pfnCallback;
} PROPSHEETHEADERA;

typedef struct _PROPSHEETHEADERW
{
  DWORD dwSize;
  DWORD dwFlags;
  HWND hwndParent;
  HINSTANCE hInstance;
  union
  {
    HICON hIcon;
    LPCWSTR pszIcon;
  };
  LPCWSTR pszCaption;
  UINT nPages;
  union
  {
    UINT nStartPage;
    LPCWSTR pStartPage;
  };
  union
  {
    LPCPROPSHEETPAGEW ppsp;
    HPROPSHEETPAGE *phpage;
  };
  PFNPROPSHEETCALLBACK pfnCallback;
} PROPSHEETHEADERW;

typedef PROPSHEETHEADERA PROPSHEETHEADER;

typedef struct _PSHNOTIFY
{
  NMHDR hdr;
  LPARAM lParam;
} PSHNOTIFY, *LPPSHNOTIFY;

INT_PTR PropertySheetA(const PROPSHEETHEADERA *header);
INT_PTR PropertySheetW(const PROPSHEETHEADERW *header);
#define PropertySheet PropertySheetA

#define PropSheet_Changed(sheet, page) \
  SendMessage((sheet), PSM_CHANGED, (WPARAM)(page), 0)
#define PropSheet_UnChanged(sheet, page) \
  SendMessage((sheet), PSM_UNCHANGED, (WPARAM)(page), 0)
#define PropSheet_SetCurSel(sheet, page, index) \
  SendMessage((sheet), PSM_SETCURSEL, (WPARAM)(index), (LPARAM)(page))
#define PropSheet_GetCurrentPageHwnd(sheet) \
  (HWND)SendMessage((sheet), PSM_GETCURRENTPAGEHWND, 0, 0)

#endif /* !RC_INVOKED && __cplusplus */

#endif
