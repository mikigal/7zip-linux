/* winuser.h -- types and functions of the Win32 user32 subset (plus a few
   GDI calls) that the 7-Zip GUI code uses. linux/shim implements them over
   Qt Widgets, so the upstream wrappers in CPP/Windows (Window.h,
   Control/Dialog.h, Control/ComboBox.h, ...) and the dialog sources compile
   unchanged.

   windows.h includes this header for C++ only; rcgen.py never sees it.
   Narrow-character ("A") functions take and return UTF-8. Unsuffixed names
   map to the "A" functions, as in a Windows build without UNICODE, because
   TCHAR is char on POSIX (CPP/Common/MyWindows.h). */

#ifndef Z7_LINUX_SHIM_WINUSER_H
#define Z7_LINUX_SHIM_WINUSER_H

#include "../../platform/Win32Compat.h"

/* This is a 64-bit Win32 subset: Windows/Window.h passes pointers through
   SetWindowLongPtr() unchanged only when _WIN64 is defined. Only the GUI
   sources see this header, and they test _WIN64 elsewhere only in Windows
   code (WOW64 and large-page checks). */
#if defined(__LP64__) && !defined(_WIN64)
#define _WIN64
#endif

/* The API level of the subset, as C/Precomp.h sets it on Windows: Vista
   (comctl32 6), so the GUI code takes no run-time paths for older systems. */
#ifndef Z7_WIN32_WINNT_MIN
#define Z7_WIN32_WINNT_MIN 0x0600
#endif

#ifndef CONST
#define CONST const
#endif
#define CALLBACK
#define APIENTRY
#define WINUSERAPI
#define WINGDIAPI

typedef void *HANDLE;
typedef HANDLE *PHANDLE;
typedef void *LPVOID;
typedef void *PVOID;
typedef CHAR *LPSTR;
typedef WCHAR *LPWSTR;
typedef BOOL *PBOOL;
typedef UINT_PTR ULONG_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef WORD ATOM;
typedef DWORD COLORREF;
typedef const RECT *LPCRECT;

/* Handles are opaque; linux/shim defines the structures. */
struct HMENU__;    typedef HMENU__ *HMENU;
struct HICON__;    typedef HICON__ *HICON;
typedef HICON HCURSOR;
struct HBRUSH__;   typedef HBRUSH__ *HBRUSH;
struct HFONT__;    typedef HFONT__ *HFONT;
struct HBITMAP__;  typedef HBITMAP__ *HBITMAP;
struct HDC__;      typedef HDC__ *HDC;
struct HMONITOR__; typedef HMONITOR__ *HMONITOR;
struct HACCEL__;   typedef HACCEL__ *HACCEL;
typedef void *HGDIOBJ;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define LOBYTE(w)           ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w)           ((BYTE)((DWORD_PTR)(w) >> 8))
#define MAKEWPARAM(l, h)    ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELPARAM(l, h)    ((LPARAM)(DWORD)MAKELONG(l, h))
#define MAKELRESULT(l, h)   ((LRESULT)(DWORD)MAKELONG(l, h))
#define GET_X_LPARAM(lp)    ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)    ((int)(short)HIWORD(lp))
#define RGB(r, g, b)        ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb)      (LOBYTE(rgb))
#define GetGValue(rgb)      (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb)      (LOBYTE((rgb) >> 16))

#define IS_INTRESOURCE(r)   ((((ULONG_PTR)(r)) >> 16) == 0)
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE     MAKEINTRESOURCEA

#define HWND_TOP       ((HWND)0)
#define HWND_BOTTOM    ((HWND)1)
#define HWND_TOPMOST   ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)
#define HWND_MESSAGE   ((HWND)-3)

#define IDI_APPLICATION  MAKEINTRESOURCE(32512)
#define IDI_HAND         MAKEINTRESOURCE(32513)
#define IDI_QUESTION     MAKEINTRESOURCE(32514)
#define IDI_EXCLAMATION  MAKEINTRESOURCE(32515)
#define IDI_ASTERISK     MAKEINTRESOURCE(32516)
#define IDC_ARROW        MAKEINTRESOURCE(32512)
#define IDC_WAIT         MAKEINTRESOURCE(32514)

#define CW_USEDEFAULT  ((int)0x80000000)

/* Windows/WinDefs.h defines it only for _WIN32 */
inline BOOL BoolToBOOL(bool v) { return (v ? TRUE: FALSE); }

typedef struct tagPOINT
{
  LONG x;
  LONG y;
} POINT, *LPPOINT;

typedef struct tagSIZE
{
  LONG cx;
  LONG cy;
} SIZE, *LPSIZE;

typedef struct tagMSG
{
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
} MSG, *LPMSG;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

typedef struct tagWNDCLASSA
{
  UINT style;
  WNDPROC lpfnWndProc;
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HICON hIcon;
  HCURSOR hCursor;
  HBRUSH hbrBackground;
  LPCSTR lpszMenuName;
  LPCSTR lpszClassName;
} WNDCLASSA;

typedef struct tagWNDCLASSW
{
  UINT style;
  WNDPROC lpfnWndProc;
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HICON hIcon;
  HCURSOR hCursor;
  HBRUSH hbrBackground;
  LPCWSTR lpszMenuName;
  LPCWSTR lpszClassName;
} WNDCLASSW;

typedef WNDCLASSA WNDCLASS;

typedef struct tagCREATESTRUCTA
{
  LPVOID lpCreateParams;
  HINSTANCE hInstance;
  HMENU hMenu;
  HWND hwndParent;
  int cy;
  int cx;
  int y;
  int x;
  LONG style;
  LPCSTR lpszName;
  LPCSTR lpszClass;
  DWORD dwExStyle;
} CREATESTRUCTA;

typedef struct tagCREATESTRUCTW
{
  LPVOID lpCreateParams;
  HINSTANCE hInstance;
  HMENU hMenu;
  HWND hwndParent;
  int cy;
  int cx;
  int y;
  int x;
  LONG style;
  LPCWSTR lpszName;
  LPCWSTR lpszClass;
  DWORD dwExStyle;
} CREATESTRUCTW;

typedef CREATESTRUCTA CREATESTRUCT;
typedef CREATESTRUCTA *LPCREATESTRUCTA;
typedef CREATESTRUCTW *LPCREATESTRUCTW;
typedef CREATESTRUCTA *LPCREATESTRUCT;

typedef struct tagWINDOWPLACEMENT
{
  UINT length;
  UINT flags;
  UINT showCmd;
  POINT ptMinPosition;
  POINT ptMaxPosition;
  RECT rcNormalPosition;
} WINDOWPLACEMENT;

typedef struct tagMINMAXINFO
{
  POINT ptReserved;
  POINT ptMaxSize;
  POINT ptMaxPosition;
  POINT ptMinTrackSize;
  POINT ptMaxTrackSize;
} MINMAXINFO, *LPMINMAXINFO;

typedef struct tagNMHDR
{
  HWND hwndFrom;
  UINT_PTR idFrom;
  UINT code;
} NMHDR, *LPNMHDR;

typedef struct tagMONITORINFO
{
  DWORD cbSize;
  RECT rcMonitor;
  RECT rcWork;
  DWORD dwFlags;
} MONITORINFO, *LPMONITORINFO;

/* Fonts (GDI) */
#define LF_FACESIZE         32
#define FW_DONTCARE         0
#define FW_NORMAL           400
#define FW_BOLD             700
#define DEFAULT_CHARSET     1
#define OUT_DEFAULT_PRECIS  0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY     0
#define DEFAULT_PITCH       0
#define FIXED_PITCH         1
#define VARIABLE_PITCH      2
#define FF_DONTCARE         0x00
#define FF_MODERN           0x30

typedef struct tagLOGFONTA
{
  LONG lfHeight;
  LONG lfWidth;
  LONG lfEscapement;
  LONG lfOrientation;
  LONG lfWeight;
  BYTE lfItalic;
  BYTE lfUnderline;
  BYTE lfStrikeOut;
  BYTE lfCharSet;
  BYTE lfOutPrecision;
  BYTE lfClipPrecision;
  BYTE lfQuality;
  BYTE lfPitchAndFamily;
  CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA;

typedef struct tagLOGFONTW
{
  LONG lfHeight;
  LONG lfWidth;
  LONG lfEscapement;
  LONG lfOrientation;
  LONG lfWeight;
  BYTE lfItalic;
  BYTE lfUnderline;
  BYTE lfStrikeOut;
  BYTE lfCharSet;
  BYTE lfOutPrecision;
  BYTE lfClipPrecision;
  BYTE lfQuality;
  BYTE lfPitchAndFamily;
  WCHAR lfFaceName[LF_FACESIZE];
} LOGFONTW;

typedef LOGFONTA LOGFONT;

/* Window classes and windows */
ATOM RegisterClassA(const WNDCLASSA *wndClass);
ATOM RegisterClassW(const WNDCLASSW *wndClass);
BOOL GetClassInfoA(HINSTANCE instance, LPCSTR className, WNDCLASSA *wndClass);
BOOL GetClassInfoW(HINSTANCE instance, LPCWSTR className, WNDCLASSW *wndClass);
HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style,
    int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style,
    int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
#define CreateWindowA(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExA(0, c, n, s, x, y, w, h, p, m, i, l)
#define CreateWindowW(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExW(0, c, n, s, x, y, w, h, p, m, i, l)
#define RegisterClass  RegisterClassA
#define GetClassInfo   GetClassInfoA
#define CreateWindowEx CreateWindowExA
#define CreateWindow   CreateWindowA
BOOL DestroyWindow(HWND wnd);
BOOL IsWindow(HWND wnd);
BOOL IsWindowEnabled(HWND wnd);
BOOL IsWindowVisible(HWND wnd);
BOOL EnableWindow(HWND wnd, BOOL enable);
BOOL ShowWindow(HWND wnd, int cmdShow);
BOOL UpdateWindow(HWND wnd);
BOOL InvalidateRect(HWND wnd, const RECT *rect, BOOL erase);
BOOL MoveWindow(HWND wnd, int x, int y, int width, int height, BOOL repaint);
BOOL SetWindowPos(HWND wnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags);
BOOL GetWindowRect(HWND wnd, LPRECT rect);
BOOL GetClientRect(HWND wnd, LPRECT rect);
BOOL ClientToScreen(HWND wnd, LPPOINT point);
BOOL ScreenToClient(HWND wnd, LPPOINT point);
int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count);
BOOL IsZoomed(HWND wnd);
BOOL IsIconic(HWND wnd);
BOOL GetWindowPlacement(HWND wnd, WINDOWPLACEMENT *placement);
BOOL SetWindowPlacement(HWND wnd, const WINDOWPLACEMENT *placement);
HWND GetParent(HWND wnd);
HWND GetAncestor(HWND wnd, UINT flags);
#define GA_PARENT    1
#define GA_ROOT      2
#define GA_ROOTOWNER 3
HWND GetDesktopWindow();
HMENU GetSystemMenu(HWND wnd, BOOL revert);
BOOL SetForegroundWindow(HWND wnd);
HWND GetForegroundWindow();
HWND GetActiveWindow();
HWND SetFocus(HWND wnd);
HWND GetFocus();
SHORT GetKeyState(int virtKey);
SHORT GetAsyncKeyState(int virtKey);
BOOL GetCursorPos(LPPOINT point);
BOOL MessageBeep(UINT type);

BOOL SetWindowTextA(HWND wnd, LPCSTR s);
BOOL SetWindowTextW(HWND wnd, LPCWSTR s);
int GetWindowTextA(HWND wnd, LPSTR s, int maxCount);
int GetWindowTextW(HWND wnd, LPWSTR s, int maxCount);
int GetWindowTextLengthA(HWND wnd);
int GetWindowTextLengthW(HWND wnd);
#define SetWindowText       SetWindowTextA
#define GetWindowText       GetWindowTextA
#define GetWindowTextLength GetWindowTextLengthA

LONG GetWindowLongA(HWND wnd, int index);
LONG GetWindowLongW(HWND wnd, int index);
LONG SetWindowLongA(HWND wnd, int index, LONG value);
LONG SetWindowLongW(HWND wnd, int index, LONG value);
LONG_PTR GetWindowLongPtrA(HWND wnd, int index);
LONG_PTR GetWindowLongPtrW(HWND wnd, int index);
LONG_PTR SetWindowLongPtrA(HWND wnd, int index, LONG_PTR value);
LONG_PTR SetWindowLongPtrW(HWND wnd, int index, LONG_PTR value);
#define GetWindowLong    GetWindowLongA
#define SetWindowLong    SetWindowLongA
#define GetWindowLongPtr GetWindowLongPtrA
#define SetWindowLongPtr SetWindowLongPtrA

/* Messages */
LRESULT SendMessageA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL PostMessageA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL PostMessageW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CallWindowProcA(WNDPROC proc, HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CallWindowProcW(WNDPROC proc, HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefWindowProcA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefWindowProcW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefDlgProcA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefDlgProcW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
#define SendMessage    SendMessageA
#define PostMessage    PostMessageA
#define CallWindowProc CallWindowProcA
#define DefWindowProc  DefWindowProcA
#define DefDlgProc     DefDlgProcA

BOOL GetMessageA(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax);
BOOL GetMessageW(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax);
BOOL PeekMessageA(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax, UINT removeMsg);
BOOL PeekMessageW(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax, UINT removeMsg);
BOOL TranslateMessage(const MSG *msg);
LRESULT DispatchMessageA(const MSG *msg);
LRESULT DispatchMessageW(const MSG *msg);
void PostQuitMessage(int exitCode);
#define GetMessage      GetMessageA
#define PeekMessage     PeekMessageA
#define DispatchMessage DispatchMessageA

UINT_PTR SetTimer(HWND wnd, UINT_PTR idEvent, UINT elapse, TIMERPROC timerFunc);
BOOL KillTimer(HWND wnd, UINT_PTR idEvent);

/* Dialogs */
HWND CreateDialogParamA(HINSTANCE instance, LPCSTR templateName, HWND parent, DLGPROC proc, LPARAM param);
HWND CreateDialogParamW(HINSTANCE instance, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM param);
INT_PTR DialogBoxParamA(HINSTANCE instance, LPCSTR templateName, HWND parent, DLGPROC proc, LPARAM param);
INT_PTR DialogBoxParamW(HINSTANCE instance, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM param);
#define CreateDialogParam CreateDialogParamA
#define DialogBoxParam    DialogBoxParamA
BOOL EndDialog(HWND dlg, INT_PTR result);
HWND GetDlgItem(HWND dlg, int id);
int GetDlgCtrlID(HWND wnd);
BOOL SetDlgItemTextA(HWND dlg, int id, LPCSTR s);
BOOL SetDlgItemTextW(HWND dlg, int id, LPCWSTR s);
UINT GetDlgItemTextA(HWND dlg, int id, LPSTR s, int maxCount);
UINT GetDlgItemTextW(HWND dlg, int id, LPWSTR s, int maxCount);
#define SetDlgItemText SetDlgItemTextA
#define GetDlgItemText GetDlgItemTextA
BOOL SetDlgItemInt(HWND dlg, int id, UINT value, BOOL isSigned);
UINT GetDlgItemInt(HWND dlg, int id, BOOL *translated, BOOL isSigned);
BOOL CheckDlgButton(HWND dlg, int id, UINT check);
UINT IsDlgButtonChecked(HWND dlg, int id);
BOOL CheckRadioButton(HWND dlg, int firstId, int lastId, int checkId);
LRESULT SendDlgItemMessageA(HWND dlg, int id, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT SendDlgItemMessageW(HWND dlg, int id, UINT message, WPARAM wParam, LPARAM lParam);
#define SendDlgItemMessage SendDlgItemMessageA
HWND GetNextDlgGroupItem(HWND dlg, HWND ctl, BOOL previous);
HWND GetNextDlgTabItem(HWND dlg, HWND ctl, BOOL previous);
BOOL MapDialogRect(HWND dlg, LPRECT rect);
LONG GetDialogBaseUnits();
BOOL IsDialogMessageA(HWND dlg, LPMSG msg);
BOOL IsDialogMessageW(HWND dlg, LPMSG msg);
#define IsDialogMessage IsDialogMessageA

int MessageBoxA(HWND wnd, LPCSTR text, LPCSTR caption, UINT type);
int MessageBoxW(HWND wnd, LPCWSTR text, LPCWSTR caption, UINT type);
#define MessageBox MessageBoxA

/* Accelerators */
HACCEL LoadAcceleratorsA(HINSTANCE instance, LPCSTR name);
HACCEL LoadAcceleratorsW(HINSTANCE instance, LPCWSTR name);
#define LoadAccelerators LoadAcceleratorsA
int TranslateAcceleratorA(HWND wnd, HACCEL accel, LPMSG msg);
int TranslateAcceleratorW(HWND wnd, HACCEL accel, LPMSG msg);
#define TranslateAccelerator TranslateAcceleratorA

/* Screen, monitors and system parameters */
BOOL SystemParametersInfoA(UINT action, UINT param, PVOID data, UINT winIni);
BOOL SystemParametersInfoW(UINT action, UINT param, PVOID data, UINT winIni);
#define SystemParametersInfo SystemParametersInfoA
int GetSystemMetrics(int index);
HMONITOR MonitorFromWindow(HWND wnd, DWORD flags);
BOOL GetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info);
BOOL GetMonitorInfoW(HMONITOR monitor, LPMONITORINFO info);
DWORD GetSysColor(int index);

/* Icons, cursors and images */
HICON LoadIconA(HINSTANCE instance, LPCSTR name);
HICON LoadIconW(HINSTANCE instance, LPCWSTR name);
#define LoadIcon LoadIconA
HANDLE LoadImageA(HINSTANCE instance, LPCSTR name, UINT type, int cx, int cy, UINT flags);
HANDLE LoadImageW(HINSTANCE instance, LPCWSTR name, UINT type, int cx, int cy, UINT flags);
#define LoadImage LoadImageA
BOOL DestroyIcon(HICON icon);
HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name);
HCURSOR LoadCursorW(HINSTANCE instance, LPCWSTR name);
#define LoadCursor LoadCursorA
HCURSOR SetCursor(HCURSOR cursor);

/* Fonts and GDI objects */
HFONT CreateFontIndirectA(const LOGFONTA *font);
HFONT CreateFontIndirectW(const LOGFONTW *font);
#define CreateFontIndirect CreateFontIndirectA
BOOL DeleteObject(HGDIOBJ object);

#endif
