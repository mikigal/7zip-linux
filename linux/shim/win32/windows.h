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

/* Accelerator flags (ACCEL.fVirt) */
#define FVIRTKEY  0x01
#define FNOINVERT 0x02
#define FSHIFT    0x04
#define FCONTROL  0x08
#define FALT      0x10

/* Virtual keys */
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

/* Languages */
#define LANG_NEUTRAL       0x00
#define LANG_ENGLISH       0x09
#define SUBLANG_NEUTRAL    0x00
#define SUBLANG_DEFAULT    0x01
#define SUBLANG_ENGLISH_US 0x01

#ifndef RC_INVOKED
/* C and C++ declarations go here when the GUI shim needs them. */
#endif

#endif
