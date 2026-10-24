// RcTables.h -- resources from the upstream .rc files.
//
// linux/tools/rcgen.py generates the tables of each program (RcFm.cpp for
// 7zFM from Bundles/Fm/resource.rc, RcGui.cpp for 7zG from UI/GUI/resource.rc),
// and each program links exactly one of them. Coordinates are in dialog units;
// styles include the defaults that the Windows resource compiler adds.

#ifndef ZIP7_INC_LINUX_RC_TABLES_H
#define ZIP7_INC_LINUX_RC_TABLES_H

#include "../../CPP/Common/MyTypes.h"

namespace NRc {

struct CControl
{
  const char *Class;   // "Button", "Static", "Edit", "ComboBox", "SysListView32", ...
  const wchar_t *Text; // "#<id>" for an icon given by resource ID
  UInt32 Id;
  Int16 X;
  Int16 Y;
  Int16 W;
  Int16 H;             // for a combo box: the height with the drop-down list
  UInt32 Style;
  UInt32 ExStyle;
};

struct CDialog
{
  UInt32 Id;
  const char *Name;    // the IDD_ macro, if one was found
  const char *Source;  // .rc file, relative to the repository
  bool IsEx;           // DIALOGEX
  Int16 X;
  Int16 Y;
  Int16 W;
  Int16 H;
  UInt32 Style;
  UInt32 ExStyle;
  const wchar_t *Caption;
  const wchar_t *FontName;
  UInt16 FontSize;
  UInt16 FontWeight;
  Byte FontItalic;
  const CControl *Controls;
  unsigned NumControls;
};

// Menu items in source order. A popup item is followed by its items, which
// have Level + 1.
struct CMenuItem
{
  const wchar_t *Text;
  UInt32 Id;
  UInt32 Type;  // MFT_* (MENUEX) or MF_* (MENU) flags
  UInt32 State; // MFS_* (MENUEX) or MF_* (MENU) flags
  Byte Level;
  bool IsPopup;
};

struct CMenu
{
  UInt32 Id;
  bool IsEx;
  const CMenuItem *Items;
  unsigned NumItems;
};

struct CAccel
{
  Byte Flags;   // FVIRTKEY, FSHIFT, FCONTROL, FALT, FNOINVERT
  UInt16 Key;   // virtual key, or a character without FVIRTKEY
  UInt32 Cmd;
};

struct CAccelTable
{
  UInt32 Id;
  const CAccel *Items;
  unsigned NumItems;
};

struct CString
{
  UInt32 Id;
  const wchar_t *Text;
};

// An icon resource: the contents of the .ico file.
struct CIcon
{
  UInt32 Id;
  const Byte *Data;
  UInt32 Size;
};

// All arrays are sorted by Id.
struct CResources
{
  const CDialog *Dialogs;
  unsigned NumDialogs;
  const CMenu *Menus;
  unsigned NumMenus;
  const CAccelTable *AccelTables;
  unsigned NumAccelTables;
  const CString *Strings;
  unsigned NumStrings;
  const CIcon *Icons;
  unsigned NumIcons;
};

extern const CResources g_Resources;

const CDialog *FindDialog(UInt32 id);
const CMenu *FindMenu(UInt32 id);
const CAccelTable *FindAccelTable(UInt32 id);
const wchar_t *FindString(UInt32 id);
const CIcon *FindIcon(UInt32 id);

}

#endif
