// ShellMenu.h -- the 7-Zip context menu of file managers: the menu of the
// Windows shell extension, which upstream Explorer/ContextMenu.cpp builds
// (into a shim HMENU, as for the 7-Zip submenu of 7zFM) with the options of
// the 7-Zip page of the settings (cascaded menu, menu items, icons) and the
// 7-Zip language, as a tree of items. A chosen item runs its command
// through InvokeCommand(), which starts 7zG (CompressCallPosix.cpp) or 7zFM.
//
// The KDE plugin (linux/kde) shows the tree in the file manager's process.
// GTK file managers cannot load Qt, so their extensions (linux/gtk) run
//
//   7z-context-menu [--] FILE...
//
// (ContextMenuTool.cpp), which prints the tree on stdout as records of
// NUL-terminated fields:
//
//   "7-Zip menu 1"                       first, the format
//   "menu" DEPTH TEXT ICON               a submenu: the records of DEPTH + 1
//                                        that follow are its items
//   "item" DEPTH TEXT ICON ARGC ARG...   an item and the program that it
//                                        starts with its arguments
//   "separator" DEPTH
//
// DEPTH is 0 for the items of the file manager's menu. ICON is "1" when the
// options ask for the 7-Zip logo, else "0".

#ifndef ZIP7_INC_LINUX_SHELL_SHELL_MENU_H
#define ZIP7_INC_LINUX_SHELL_SHELL_MENU_H

#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>

#include <windows.h>
#include <ShObjIdl.h>

#include "../../CPP/Common/MyCom.h"

namespace NShellMenu {

struct CItem
{
  QString Text;
  QIcon Icon;               // the 7-Zip logo, with "Icons in context menu"
  bool Separator = false;
  int Command = -1;         // for Invoke(); -1 for submenus and separators
  QList<CItem> Children;    // the items of a submenu

  bool IsSubMenu() const { return !Separator && Command < 0; }
};

class CMenu
{
  CMyComPtr<IContextMenu> _contextMenu;

public:
  // The menu for paths of the file system; empty when 7-Zip offers nothing.
  QList<CItem> Create(const QStringList &paths);
  // Runs the command of an item of the last menu.
  void Invoke(int command);
};

// Sets the program folder (7zG, 7zFM, Lang/) for code that runs outside
// it: the installed one, or $Z7_PROGRAM_DIR (tests).
void SetProgramDir();

// 7z-context-menu: prints the menu for paths, in the format above.
int PrintMenu(const QStringList &paths);

}

#endif
