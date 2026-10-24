/* windows.h -- the Win32 subset used by the 7-Zip resource scripts and GUI
   code on Linux.

   Constants have their Win32 values, so the tables that linux/tools/rcgen.py
   generates from the .rc files and the GUI code that tests style bits agree
   with Windows. rcgen.py preprocesses the .rc files with RC_INVOKED defined;
   only constants are visible then. */

#ifndef Z7_LINUX_SHIM_WINDOWS_H
#define Z7_LINUX_SHIM_WINDOWS_H

/* Window styles */
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_VSCROLL          0x00200000L
#define WS_HSCROLL          0x00100000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_GROUP            0x00020000L
#define WS_TABSTOP          0x00010000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_SIZEBOX          WS_THICKFRAME
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW      (WS_POPUP | WS_BORDER | WS_SYSMENU)

/* Extended window styles */
#define WS_EX_DLGMODALFRAME  0x00000001L
#define WS_EX_NOPARENTNOTIFY 0x00000004L
#define WS_EX_TOPMOST        0x00000008L
#define WS_EX_ACCEPTFILES    0x00000010L
#define WS_EX_TRANSPARENT    0x00000020L
#define WS_EX_TOOLWINDOW     0x00000080L
#define WS_EX_WINDOWEDGE     0x00000100L
#define WS_EX_CLIENTEDGE     0x00000200L
#define WS_EX_CONTEXTHELP    0x00000400L
#define WS_EX_RIGHT          0x00001000L
#define WS_EX_STATICEDGE     0x00020000L
#define WS_EX_APPWINDOW      0x00040000L

/* Dialog styles */
#define DS_ABSALIGN      0x01L
#define DS_SYSMODAL      0x02L
#define DS_3DLOOK        0x04L
#define DS_FIXEDSYS      0x08L
#define DS_NOFAILCREATE  0x10L
#define DS_LOCALEDIT     0x20L
#define DS_SETFONT       0x40L
#define DS_MODALFRAME    0x80L
#define DS_NOIDLEMSG     0x100L
#define DS_SETFOREGROUND 0x200L
#define DS_CONTROL       0x400L
#define DS_CENTER        0x800L
#define DS_CENTERMOUSE   0x1000L
#define DS_CONTEXTHELP   0x2000L
#define DS_SHELLFONT     (DS_SETFONT | DS_FIXEDSYS)

/* Button styles */
#define BS_PUSHBUTTON      0x00000000L
#define BS_DEFPUSHBUTTON   0x00000001L
#define BS_CHECKBOX        0x00000002L
#define BS_AUTOCHECKBOX    0x00000003L
#define BS_RADIOBUTTON     0x00000004L
#define BS_3STATE          0x00000005L
#define BS_AUTO3STATE      0x00000006L
#define BS_GROUPBOX        0x00000007L
#define BS_USERBUTTON      0x00000008L
#define BS_AUTORADIOBUTTON 0x00000009L
#define BS_PUSHBOX         0x0000000AL
#define BS_OWNERDRAW       0x0000000BL
#define BS_TYPEMASK        0x0000000FL
#define BS_LEFTTEXT        0x00000020L
#define BS_TEXT            0x00000000L
#define BS_ICON            0x00000040L
#define BS_BITMAP          0x00000080L
#define BS_LEFT            0x00000100L
#define BS_RIGHT           0x00000200L
#define BS_CENTER          0x00000300L
#define BS_TOP             0x00000400L
#define BS_BOTTOM          0x00000800L
#define BS_VCENTER         0x00000C00L
#define BS_PUSHLIKE        0x00001000L
#define BS_MULTILINE       0x00002000L
#define BS_NOTIFY          0x00004000L
#define BS_FLAT            0x00008000L
#define BS_RIGHTBUTTON     BS_LEFTTEXT

/* Edit control styles */
#define ES_LEFT        0x0000L
#define ES_CENTER      0x0001L
#define ES_RIGHT       0x0002L
#define ES_MULTILINE   0x0004L
#define ES_UPPERCASE   0x0008L
#define ES_LOWERCASE   0x0010L
#define ES_PASSWORD    0x0020L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_NOHIDESEL   0x0100L
#define ES_OEMCONVERT  0x0400L
#define ES_READONLY    0x0800L
#define ES_WANTRETURN  0x1000L
#define ES_NUMBER      0x2000L

/* Static control styles */
#define SS_LEFT           0x00000000L
#define SS_CENTER         0x00000001L
#define SS_RIGHT          0x00000002L
#define SS_ICON           0x00000003L
#define SS_BLACKRECT      0x00000004L
#define SS_GRAYRECT       0x00000005L
#define SS_WHITERECT      0x00000006L
#define SS_BLACKFRAME     0x00000007L
#define SS_GRAYFRAME      0x00000008L
#define SS_WHITEFRAME     0x00000009L
#define SS_USERITEM       0x0000000AL
#define SS_SIMPLE         0x0000000BL
#define SS_LEFTNOWORDWRAP 0x0000000CL
#define SS_OWNERDRAW      0x0000000DL
#define SS_BITMAP         0x0000000EL
#define SS_ENHMETAFILE    0x0000000FL
#define SS_ETCHEDHORZ     0x00000010L
#define SS_ETCHEDVERT     0x00000011L
#define SS_ETCHEDFRAME    0x00000012L
#define SS_TYPEMASK       0x0000001FL
#define SS_REALSIZECONTROL 0x00000040L
#define SS_NOPREFIX       0x00000080L
#define SS_NOTIFY         0x00000100L
#define SS_CENTERIMAGE    0x00000200L
#define SS_RIGHTJUST      0x00000400L
#define SS_REALSIZEIMAGE  0x00000800L
#define SS_SUNKEN         0x00001000L
#define SS_EDITCONTROL    0x00002000L
#define SS_ENDELLIPSIS    0x00004000L
#define SS_PATHELLIPSIS   0x00008000L
#define SS_WORDELLIPSIS   0x0000C000L
#define SS_ELLIPSISMASK   0x0000C000L

/* Combo box styles */
#define CBS_SIMPLE            0x0001L
#define CBS_DROPDOWN          0x0002L
#define CBS_DROPDOWNLIST      0x0003L
#define CBS_OWNERDRAWFIXED    0x0010L
#define CBS_OWNERDRAWVARIABLE 0x0020L
#define CBS_AUTOHSCROLL       0x0040L
#define CBS_OEMCONVERT        0x0080L
#define CBS_SORT              0x0100L
#define CBS_HASSTRINGS        0x0200L
#define CBS_NOINTEGRALHEIGHT  0x0400L
#define CBS_DISABLENOSCROLL   0x0800L
#define CBS_UPPERCASE         0x2000L
#define CBS_LOWERCASE         0x4000L

/* List box styles */
#define LBS_NOTIFY            0x0001L
#define LBS_SORT              0x0002L
#define LBS_NOREDRAW          0x0004L
#define LBS_MULTIPLESEL       0x0008L
#define LBS_OWNERDRAWFIXED    0x0010L
#define LBS_OWNERDRAWVARIABLE 0x0020L
#define LBS_HASSTRINGS        0x0040L
#define LBS_USETABSTOPS       0x0080L
#define LBS_NOINTEGRALHEIGHT  0x0100L
#define LBS_MULTICOLUMN       0x0200L
#define LBS_WANTKEYBOARDINPUT 0x0400L
#define LBS_EXTENDEDSEL       0x0800L
#define LBS_DISABLENOSCROLL   0x1000L
#define LBS_NODATA            0x2000L
#define LBS_NOSEL             0x4000L
#define LBS_STANDARD          (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)

/* Scroll bar styles */
#define SBS_HORZ 0x0000L
#define SBS_VERT 0x0001L

/* Dialog box command IDs */
#define IDOK       1
#define IDCANCEL   2
#define IDABORT    3
#define IDRETRY    4
#define IDIGNORE   5
#define IDYES      6
#define IDNO       7
#define IDCLOSE    8
#define IDHELP     9
#define IDTRYAGAIN 10
#define IDCONTINUE 11

/* Menu item types and states (MENUEX) */
#define MFT_STRING       0x00000000L
#define MFT_BITMAP       0x00000004L
#define MFT_MENUBARBREAK 0x00000020L
#define MFT_MENUBREAK    0x00000040L
#define MFT_OWNERDRAW    0x00000100L
#define MFT_RADIOCHECK   0x00000200L
#define MFT_SEPARATOR    0x00000800L
#define MFT_RIGHTORDER   0x00002000L
#define MFT_RIGHTJUSTIFY 0x00004000L
#define MFS_GRAYED       0x00000003L
#define MFS_DISABLED     MFS_GRAYED
#define MFS_CHECKED      0x00000008L
#define MFS_HILITE       0x00000080L
#define MFS_ENABLED      0x00000000L
#define MFS_UNCHECKED    0x00000000L
#define MFS_UNHILITE     0x00000000L
#define MFS_DEFAULT      0x00001000L

/* Menu flags (MENU) */
#define MF_GRAYED       0x00000001L
#define MF_DISABLED     0x00000002L
#define MF_CHECKED      0x00000008L
#define MF_POPUP        0x00000010L
#define MF_MENUBARBREAK 0x00000020L
#define MF_MENUBREAK    0x00000040L
#define MF_SEPARATOR    0x00000800L
#define MF_HELP         0x00004000L
#define MF_INSERT       0x00000000L
#define MF_CHANGE       0x00000080L
#define MF_APPEND       0x00000100L
#define MF_DELETE       0x00000200L
#define MF_REMOVE       0x00001000L
#define MF_BYCOMMAND    0x00000000L
#define MF_BYPOSITION   0x00000400L
#define MF_ENABLED      0x00000000L
#define MF_UNCHECKED    0x00000000L
#define MF_STRING       0x00000000L
#define MF_BITMAP       0x00000004L
#define MF_OWNERDRAW    0x00000100L
#define MF_USECHECKBITMAPS 0x00000200L
#define MF_HILITE       0x00000080L
#define MF_UNHILITE     0x00000000L
#define MF_DEFAULT      0x00001000L
#define MF_RIGHTJUSTIFY 0x00004000L

/* MENUITEMINFO masks */
#define MIIM_STATE      0x00000001
#define MIIM_ID         0x00000002
#define MIIM_SUBMENU    0x00000004
#define MIIM_CHECKMARKS 0x00000008
#define MIIM_TYPE       0x00000010
#define MIIM_DATA       0x00000020
#define MIIM_STRING     0x00000040
#define MIIM_BITMAP     0x00000080
#define MIIM_FTYPE      0x00000100

/* TrackPopupMenu() */
#define TPM_LEFTBUTTON   0x0000L
#define TPM_RIGHTBUTTON  0x0002L
#define TPM_LEFTALIGN    0x0000L
#define TPM_CENTERALIGN  0x0004L
#define TPM_RIGHTALIGN   0x0008L
#define TPM_TOPALIGN     0x0000L
#define TPM_VCENTERALIGN 0x0010L
#define TPM_BOTTOMALIGN  0x0020L
#define TPM_HORIZONTAL   0x0000L
#define TPM_VERTICAL     0x0040L
#define TPM_NONOTIFY     0x0080L
#define TPM_RETURNCMD    0x0100L
#define TPM_RECURSE      0x0001L

/* Accelerator flags (ACCEL.fVirt) */
#define FVIRTKEY  0x01
#define FNOINVERT 0x02
#define FSHIFT    0x04
#define FCONTROL  0x08
#define FALT      0x10

/* Virtual keys */
#define VK_LBUTTON  0x01
#define VK_RBUTTON  0x02
#define VK_CANCEL   0x03
#define VK_MBUTTON  0x04
#define VK_BACK     0x08
#define VK_TAB      0x09
#define VK_CLEAR    0x0C
#define VK_RETURN   0x0D
#define VK_SHIFT    0x10
#define VK_CONTROL  0x11
#define VK_MENU     0x12
#define VK_PAUSE    0x13
#define VK_CAPITAL  0x14
#define VK_ESCAPE   0x1B
#define VK_SPACE    0x20
#define VK_PRIOR    0x21
#define VK_NEXT     0x22
#define VK_END      0x23
#define VK_HOME     0x24
#define VK_LEFT     0x25
#define VK_UP       0x26
#define VK_RIGHT    0x27
#define VK_DOWN     0x28
#define VK_SELECT   0x29
#define VK_PRINT    0x2A
#define VK_EXECUTE  0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT   0x2D
#define VK_DELETE   0x2E
#define VK_HELP     0x2F
#define VK_LWIN     0x5B
#define VK_RWIN     0x5C
#define VK_APPS     0x5D
#define VK_NUMPAD0  0x60
#define VK_NUMPAD1  0x61
#define VK_NUMPAD2  0x62
#define VK_NUMPAD3  0x63
#define VK_NUMPAD4  0x64
#define VK_NUMPAD5  0x65
#define VK_NUMPAD6  0x66
#define VK_NUMPAD7  0x67
#define VK_NUMPAD8  0x68
#define VK_NUMPAD9  0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD      0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL  0x6E
#define VK_DIVIDE   0x6F
#define VK_F1       0x70
#define VK_F2       0x71
#define VK_F3       0x72
#define VK_F4       0x73
#define VK_F5       0x74
#define VK_F6       0x75
#define VK_F7       0x76
#define VK_F8       0x77
#define VK_F9       0x78
#define VK_F10      0x79
#define VK_F11      0x7A
#define VK_F12      0x7B
#define VK_NUMLOCK  0x90
#define VK_SCROLL   0x91
#define VK_LSHIFT   0xA0
#define VK_RSHIFT   0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU    0xA4
#define VK_RMENU    0xA5
#define VK_OEM_1      0xBA
#define VK_OEM_PLUS   0xBB
#define VK_OEM_COMMA  0xBC
#define VK_OEM_MINUS  0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2      0xBF
#define VK_OEM_3      0xC0
#define VK_OEM_4      0xDB
#define VK_OEM_5      0xDC
#define VK_OEM_6      0xDD
#define VK_OEM_7      0xDE

/* MapVirtualKey() */
#define MAPVK_VK_TO_VSC   0
#define MAPVK_VSC_TO_VK   1
#define MAPVK_VK_TO_CHAR  2

/* Mouse messages: key state in wParam */
#define MK_LBUTTON  0x0001
#define MK_RBUTTON  0x0002
#define MK_SHIFT    0x0004
#define MK_CONTROL  0x0008
#define MK_MBUTTON  0x0010

/* Languages */
#define LANG_NEUTRAL       0x00
#define LANG_ENGLISH       0x09
#define SUBLANG_NEUTRAL    0x00
#define SUBLANG_DEFAULT    0x01
#define SUBLANG_ENGLISH_US 0x01

/* Window messages */
#define WM_NULL            0x0000
#define WM_CREATE          0x0001
#define WM_DESTROY         0x0002
#define WM_MOVE            0x0003
#define WM_SIZE            0x0005
#define WM_ACTIVATE        0x0006
#define WM_SETFOCUS        0x0007
#define WM_KILLFOCUS       0x0008
#define WM_ENABLE          0x000A
#define WM_SETREDRAW       0x000B
#define WM_SETTEXT         0x000C
#define WM_GETTEXT         0x000D
#define WM_GETTEXTLENGTH   0x000E
#define WM_PAINT           0x000F
#define WM_CLOSE           0x0010
#define WM_QUIT            0x0012
#define WM_ERASEBKGND      0x0014
#define WM_SHOWWINDOW      0x0018
#define WM_SETTINGCHANGE   0x001A
#define WM_ACTIVATEAPP     0x001C
#define WM_CANCELMODE      0x001F
#define WM_SETCURSOR       0x0020
#define WM_GETMINMAXINFO   0x0024
#define WM_NEXTDLGCTL      0x0028
#define WM_DRAWITEM        0x002B
#define WM_SETFONT         0x0030
#define WM_GETFONT         0x0031
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_COPYDATA        0x004A
#define WM_NOTIFY          0x004E
#define WM_HELP            0x0053
#define WM_CONTEXTMENU     0x007B
#define WM_SETICON         0x0080
#define WM_NCCREATE        0x0081
#define WM_NCDESTROY       0x0082
#define WM_GETDLGCODE      0x0087
#define WM_KEYDOWN         0x0100
#define WM_KEYUP           0x0101
#define WM_CHAR            0x0102
#define WM_SYSKEYDOWN      0x0104
#define WM_SYSKEYUP        0x0105
#define WM_SYSCHAR         0x0106
#define WM_INITDIALOG      0x0110
#define WM_COMMAND         0x0111
#define WM_SYSCOMMAND      0x0112
#define WM_TIMER           0x0113
#define WM_HSCROLL         0x0114
#define WM_VSCROLL         0x0115
#define WM_INITMENU        0x0116
#define WM_INITMENUPOPUP   0x0117
#define WM_MENUSELECT      0x011F
#define WM_MENUCHAR        0x0120
#define WM_ENTERIDLE       0x0121
#define WM_UNINITMENUPOPUP 0x0125
#define WM_CHANGEUISTATE   0x0127
#define WM_UPDATEUISTATE   0x0128
#define WM_QUERYUISTATE    0x0129
#define WM_CTLCOLOREDIT    0x0133
#define WM_CTLCOLORSTATIC  0x0138
#define WM_MOUSEMOVE       0x0200
#define WM_LBUTTONDOWN     0x0201
#define WM_LBUTTONUP       0x0202
#define WM_LBUTTONDBLCLK   0x0203
#define WM_RBUTTONDOWN     0x0204
#define WM_RBUTTONUP       0x0205
#define WM_RBUTTONDBLCLK   0x0206
#define WM_MBUTTONDOWN     0x0207
#define WM_MBUTTONUP       0x0208
#define WM_MOUSEWHEEL      0x020A
#define WM_ENTERMENULOOP   0x0211
#define WM_EXITMENULOOP    0x0212
#define WM_CAPTURECHANGED  0x0215
#define WM_DEVICECHANGE    0x0219
#define WM_DROPFILES       0x0233
#define WM_MOUSELEAVE      0x02A3
#define WM_USER            0x0400
#define WM_APP             0x8000

/* WM_*UISTATE */
#define UIS_SET        1
#define UIS_CLEAR      2
#define UIS_INITIALIZE 3
#define UISF_HIDEFOCUS 0x1
#define UISF_HIDEACCEL 0x2

/* WM_ACTIVATE wParam */
#define WA_INACTIVE    0
#define WA_ACTIVE      1
#define WA_CLICKACTIVE 2

/* WM_SYSCOMMAND wParam */
#define SC_SIZE     0xF000
#define SC_MOVE     0xF010
#define SC_MINIMIZE 0xF020
#define SC_MAXIMIZE 0xF030
#define SC_CLOSE    0xF060
#define SC_KEYMENU  0xF100
#define SC_RESTORE  0xF120

/* WM_SIZE wParam */
#define SIZE_RESTORED  0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define SIZE_MAXSHOW   3
#define SIZE_MAXHIDE   4

/* ShowWindow() */
#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_NORMAL          1
#define SW_SHOWMINIMIZED   2
#define SW_SHOWMAXIMIZED   3
#define SW_MAXIMIZE        3
#define SW_SHOWNOACTIVATE  4
#define SW_SHOW            5
#define SW_MINIMIZE        6
#define SW_SHOWMINNOACTIVE 7
#define SW_SHOWNA          8
#define SW_RESTORE         9
#define SW_SHOWDEFAULT     10

/* SetWindowPos() */
#define SWP_NOSIZE       0x0001
#define SWP_NOMOVE       0x0002
#define SWP_NOZORDER     0x0004
#define SWP_NOREDRAW     0x0008
#define SWP_NOACTIVATE   0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW   0x0040
#define SWP_HIDEWINDOW   0x0080

/* GetWindowLong() indexes */
#define GWL_WNDPROC    (-4)
#define GWL_HINSTANCE  (-6)
#define GWL_HWNDPARENT (-8)
#define GWL_ID         (-12)
#define GWL_STYLE      (-16)
#define GWL_EXSTYLE    (-20)
#define GWL_USERDATA   (-21)
#define GWLP_WNDPROC    (-4)
#define GWLP_HINSTANCE  (-6)
#define GWLP_HWNDPARENT (-8)
#define GWLP_USERDATA   (-21)
#define GWLP_ID         (-12)
/* 64-bit layout: LRESULT, DLGPROC and the user pointer */
#define DWLP_MSGRESULT 0
#define DWLP_DLGPROC   8
#define DWLP_USER      16

/* Button notifications, messages and states */
#define BN_CLICKED       0
#define BN_DBLCLK        5
#define BN_DOUBLECLICKED BN_DBLCLK
#define BN_SETFOCUS      6
#define BN_KILLFOCUS     7
#define BM_GETCHECK      0x00F0
#define BM_SETCHECK      0x00F1
#define BM_GETSTATE      0x00F2
#define BM_SETSTATE      0x00F3
#define BM_SETSTYLE      0x00F4
#define BM_CLICK         0x00F5
#define BM_GETIMAGE      0x00F6
#define BM_SETIMAGE      0x00F7
#define BST_UNCHECKED     0x0000
#define BST_CHECKED       0x0001
#define BST_INDETERMINATE 0x0002
#define BST_PUSHED        0x0004
#define BST_FOCUS         0x0008

/* Combo box messages and notifications */
#define CB_OKAY               0
#define CB_ERR                (-1)
#define CB_ERRSPACE           (-2)
#define CB_GETEDITSEL         0x0140
#define CB_LIMITTEXT          0x0141
#define CB_SETEDITSEL         0x0142
#define CB_ADDSTRING          0x0143
#define CB_DELETESTRING       0x0144
#define CB_DIR                0x0145
#define CB_GETCOUNT           0x0146
#define CB_GETCURSEL          0x0147
#define CB_GETLBTEXT          0x0148
#define CB_GETLBTEXTLEN       0x0149
#define CB_INSERTSTRING       0x014A
#define CB_RESETCONTENT       0x014B
#define CB_FINDSTRING         0x014C
#define CB_SELECTSTRING       0x014D
#define CB_SETCURSEL          0x014E
#define CB_SHOWDROPDOWN       0x014F
#define CB_GETITEMDATA        0x0150
#define CB_SETITEMDATA        0x0151
#define CB_GETDROPPEDCONTROLRECT 0x0152
#define CB_SETITEMHEIGHT      0x0153
#define CB_GETITEMHEIGHT      0x0154
#define CB_SETEXTENDEDUI      0x0155
#define CB_GETEXTENDEDUI      0x0156
#define CB_GETDROPPEDSTATE    0x0157
#define CB_FINDSTRINGEXACT    0x0158
#define CB_SETDROPPEDWIDTH    0x0160
#define CBN_ERRSPACE          (-1)
#define CBN_SELCHANGE         1
#define CBN_DBLCLK            2
#define CBN_SETFOCUS          3
#define CBN_KILLFOCUS         4
#define CBN_EDITCHANGE        5
#define CBN_EDITUPDATE        6
#define CBN_DROPDOWN          7
#define CBN_CLOSEUP           8
#define CBN_SELENDOK          9
#define CBN_SELENDCANCEL      10

/* Edit control messages and notifications */
#define EM_GETSEL          0x00B0
#define EM_SETSEL          0x00B1
#define EM_SCROLLCARET     0x00B7
#define EM_GETMODIFY       0x00B8
#define EM_SETMODIFY       0x00B9
#define EM_GETLINECOUNT    0x00BA
#define EM_REPLACESEL      0x00C2
#define EM_LIMITTEXT       0x00C5
#define EM_SETLIMITTEXT    EM_LIMITTEXT
#define EM_SETPASSWORDCHAR 0x00CC
#define EM_SETREADONLY     0x00CF
#define EM_GETPASSWORDCHAR 0x00D2
#define EN_SETFOCUS        0x0100
#define EN_KILLFOCUS       0x0200
#define EN_CHANGE          0x0300
#define EN_UPDATE          0x0400
#define EN_ERRSPACE        0x0500
#define EN_MAXTEXT         0x0501

/* Static control messages and notifications */
#define STM_SETICON  0x0170
#define STM_GETICON  0x0171
#define STM_SETIMAGE 0x0172
#define STM_GETIMAGE 0x0173
#define STN_CLICKED  0
#define STN_DBLCLK   1

/* Dialog messages and WM_GETDLGCODE results */
#define DM_GETDEFID   (WM_USER + 0)
#define DM_SETDEFID   (WM_USER + 1)
#define DM_REPOSITION (WM_USER + 2)
#define DC_HASDEFID   0x534B
#define DLGC_WANTARROWS      0x0001
#define DLGC_WANTTAB         0x0002
#define DLGC_WANTALLKEYS     0x0004
#define DLGC_WANTMESSAGE     0x0004
#define DLGC_HASSETSEL       0x0008
#define DLGC_DEFPUSHBUTTON   0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON     0x0040
#define DLGC_WANTCHARS       0x0080
#define DLGC_STATIC          0x0100
#define DLGC_BUTTON          0x2000

/* Scroll bar requests (WM_HSCROLL, WM_VSCROLL) */
#define SB_HORZ          0
#define SB_VERT          1
#define SB_CTL           2
#define SB_LINEUP        0
#define SB_LINELEFT      0
#define SB_LINEDOWN      1
#define SB_LINERIGHT     1
#define SB_PAGEUP        2
#define SB_PAGEDOWN      3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK    5
#define SB_TOP           6
#define SB_BOTTOM        7
#define SB_ENDSCROLL     8

/* MessageBox() */
#define MB_OK                0x00000000L
#define MB_OKCANCEL          0x00000001L
#define MB_ABORTRETRYIGNORE  0x00000002L
#define MB_YESNOCANCEL       0x00000003L
#define MB_YESNO             0x00000004L
#define MB_RETRYCANCEL       0x00000005L
#define MB_CANCELTRYCONTINUE 0x00000006L
#define MB_ICONHAND          0x00000010L
#define MB_ICONQUESTION      0x00000020L
#define MB_ICONEXCLAMATION   0x00000030L
#define MB_ICONASTERISK      0x00000040L
#define MB_ICONERROR         MB_ICONHAND
#define MB_ICONSTOP          MB_ICONHAND
#define MB_ICONWARNING       MB_ICONEXCLAMATION
#define MB_ICONINFORMATION   MB_ICONASTERISK
#define MB_DEFBUTTON1        0x00000000L
#define MB_DEFBUTTON2        0x00000100L
#define MB_DEFBUTTON3        0x00000200L
#define MB_DEFBUTTON4        0x00000300L
#define MB_APPLMODAL         0x00000000L
#define MB_SYSTEMMODAL       0x00001000L
#define MB_TASKMODAL         0x00002000L
#define MB_HELP              0x00004000L
#define MB_SETFOREGROUND     0x00010000L
#define MB_TOPMOST           0x00040000L
#define MB_TYPEMASK          0x0000000FL
#define MB_ICONMASK          0x000000F0L
#define MB_DEFMASK           0x00000F00L

/* System parameters, metrics and monitors */
#define SPI_GETNONCLIENTMETRICS 0x0029
#define SPI_GETWORKAREA         0x0030
#define SM_CXSCREEN    0
#define SM_CYSCREEN    1
#define SM_CXVSCROLL   2
#define SM_CYHSCROLL   3
#define SM_CYCAPTION   4
#define SM_CXICON      11
#define SM_CYICON      12
#define SM_CXSMICON    49
#define SM_CYSMICON    50
#define MONITOR_DEFAULTTONULL    0x00000000
#define MONITOR_DEFAULTTOPRIMARY 0x00000001
#define MONITOR_DEFAULTTONEAREST 0x00000002

/* Icons and images */
#define ICON_SMALL    0
#define ICON_BIG      1
#define IMAGE_BITMAP  0
#define IMAGE_ICON    1
#define IMAGE_CURSOR  2
#define LR_DEFAULTCOLOR 0x00000000
#define LR_LOADFROMFILE 0x00000010
#define LR_DEFAULTSIZE  0x00000040
#define LR_SHARED       0x00008000

/* System colors */
#define COLOR_SCROLLBAR     0
#define COLOR_BACKGROUND    1
#define COLOR_ACTIVECAPTION 2
#define COLOR_MENU          4
#define COLOR_WINDOW        5
#define COLOR_WINDOWTEXT    8
#define COLOR_HIGHLIGHT     13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_BTNFACE       15
#define COLOR_3DFACE        COLOR_BTNFACE
#define COLOR_GRAYTEXT      17
#define COLOR_BTNTEXT       18

/* PeekMessage() */
#define PM_NOREMOVE 0x0000
#define PM_REMOVE   0x0001

#if !defined(RC_INVOKED) && defined(__cplusplus)
/* Types and functions: the user32 subset that linux/shim implements over Qt,
   and the kernel32, ole32, comdlg32 and shell32 calls that <windows.h>
   also declares on Windows. */
#include "winuser.h"
#include "winbase.h"
#include "objbase.h"
#include "commdlg.h"
#include "shellapi.h"
#endif

#endif
