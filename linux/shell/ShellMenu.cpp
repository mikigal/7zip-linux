// ShellMenu.cpp

#include <QFileInfo>

#include <cstdio>

#include "ShellMenu.h"

#include "../../CPP/7zip/UI/Explorer/ContextMenu.h"

#include "Platform.h"

#include "ShimDialog.h"
#include "ShimResources.h"
#include "ShimWindow.h"

namespace NShellMenu {

static const UINT kFirstCommandId = 1;
static const UINT kLastCommandId = 0x7FFF;

static QList<CItem> ReadItems(HMENU menu)
{
  QList<CItem> items;
  const int num = GetMenuItemCount(menu);
  for (int i = 0; i < num; i++)
  {
    wchar_t text[1024];
    MENUITEMINFOW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING | MIIM_CHECKMARKS | MIIM_BITMAP;
    mi.dwTypeData = text;
    mi.cch = Z7_ARRAY_SIZE(text);
    if (!GetMenuItemInfoW(menu, (UINT)i, TRUE, &mi))
      continue;
    CItem item;
    if (mi.fType & MFT_SEPARATOR)
    {
      item.Separator = true;
      items.append(item);
      continue;
    }
    item.Text = QString::fromWCharArray(text);
    // the menu logo of the options (IDB_MENU_LOGO: the program icon)
    const HBITMAP bitmap = mi.hbmpItem ? mi.hbmpItem : mi.hbmpUnchecked;
    if (bitmap)
      item.Icon = bitmap->Icon;
    if (mi.hSubMenu)
      item.Children = ReadItems(mi.hSubMenu);
    else
      item.Command = (int)(mi.wID - kFirstCommandId);
    items.append(item);
  }
  return items;
}

QList<CItem> CMenu::Create(const QStringList &paths)
{
  CZipContextMenu *spec = new CZipContextMenu;
  _contextMenu = spec;
  if (paths.isEmpty())
    return QList<CItem>();
  // as the 7zFM panel fills it (CPanel::CreateSevenZipMenu)
  for (int i = 0; i < paths.size(); i++)
  {
    if (spec->_attribs.FirstDirIndex < 0 && QFileInfo(paths[i]).isDir())
      spec->_attribs.FirstDirIndex = i;
    spec->_fileNames.Add(NShim::ToU(paths[i]));
  }
  QList<CItem> items;
  HMENU menu = CreatePopupMenu();
  if (SUCCEEDED(_contextMenu->QueryContextMenu(menu, 0, kFirstCommandId, kLastCommandId, CMF_NORMAL)))
    items = ReadItems(menu);
  DestroyMenu(menu);
  // the menu of the file manager has its own separators around 7-Zip
  while (!items.isEmpty() && items.first().Separator)
    items.removeFirst();
  while (!items.isEmpty() && items.last().Separator)
    items.removeLast();
  return items;
}

void CMenu::Invoke(int command)
{
  if (!_contextMenu || command < 0)
    return;
  CMINVOKECOMMANDINFO ci;
  memset(&ci, 0, sizeof(ci));
  ci.cbSize = sizeof(ci);
  ci.lpVerb = MAKEINTRESOURCEA(command);
  ci.nShow = SW_SHOWNORMAL;
  _contextMenu->InvokeCommand(&ci);
}

static void PutField(const QString &s)
{
  const QByteArray a = s.toUtf8();
  std::fwrite(a.constData(), 1, (size_t)a.size() + 1, stdout);  // with the NUL
}

static void PrintItems(CMenu &menu, const QList<CItem> &items, int depth)
{
  for (const CItem &item : items)
  {
    if (item.Separator)
    {
      PutField(QStringLiteral("separator"));
      PutField(QString::number(depth));
      continue;
    }
    PutField(item.IsSubMenu() ? QStringLiteral("menu") : QStringLiteral("item"));
    PutField(QString::number(depth));
    PutField(item.Text);
    PutField(item.Icon.isNull() ? QStringLiteral("0") : QStringLiteral("1"));
    if (item.IsSubMenu())
    {
      PrintItems(menu, item.Children, depth + 1);
      continue;
    }
    // the command of the item: the program that InvokeCommand() would start
    QStringList command;
    NShim::SetStartHook([&command](const QString &program, const QStringList &args)
    {
      command = QStringList(program) + args;
      return true;
    });
    menu.Invoke(item.Command);
    NShim::SetStartHook(NShim::CStartHook());
    PutField(QString::number(command.size()));
    for (const QString &arg : command)
      PutField(arg);
  }
}

void SetProgramDir()
{
  QByteArray dir = qgetenv("Z7_PROGRAM_DIR");
  if (dir.isEmpty())
    dir = Z7_INSTALL_PROGRAM_DIR;
  NPlatform::SetModuleDir(FString(dir.constData()));
}

int PrintMenu(const QStringList &paths)
{
  CMenu menu;
  const QList<CItem> items = menu.Create(paths);
  PutField(QStringLiteral("7-Zip menu 1"));
  PrintItems(menu, items, 0);
  return std::fflush(stdout) == 0 ? 0 : 1;
}

}
