// FileItemAction.cpp -- the 7-Zip context menu of KDE file managers
// (Dolphin, Konqueror, Krusader): a KFileItemActions plugin with the menu of
// the Windows shell extension. Upstream Explorer/ContextMenu.cpp builds the
// menu, as it does for the 7-Zip submenu of 7zFM: into a shim HMENU, with
// the options of the 7-Zip page of the settings (cascaded menu, menu items,
// icons) and the 7-Zip language. The plugin turns that menu into Qt actions;
// a chosen action goes back to InvokeCommand(), which starts 7zG
// (CompressCallPosix.cpp) or 7zFM.

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>

#include <QAction>
#include <QMenu>
#include <QPointer>

#include <vector>

#include <windows.h>

#include "../../CPP/Common/MyCom.h"

#include "../../CPP/7zip/UI/Explorer/ContextMenu.h"

#include "Platform.h"
#include "ShimResources.h"
#include "ShimWindow.h"

// The program folder (7zG, 7zFM, Lang/): the installed one, or
// $Z7_PROGRAM_DIR (tests).
static void InitProgramDir()
{
  static bool s_Done = false;
  if (s_Done)
    return;
  s_Done = true;
  QByteArray dir = qgetenv("Z7_PROGRAM_DIR");
  if (dir.isEmpty())
    dir = Z7_INSTALL_PROGRAM_DIR;
  NPlatform::SetModuleDir(FString(dir.constData()));
}

static const UINT kFirstCommandId = 1;
static const UINT kLastCommandId = 0x7FFF;

// The 7-Zip menu of one request: the upstream object whose InvokeCommand()
// runs a chosen command, and the actions made from its menu.
class CMenu7z: public QObject
{
  CMyComPtr<IContextMenu> _contextMenu;
  std::vector<QPointer<QMenu> > _subMenus;

  void Invoke(UINT offset)
  {
    CMINVOKECOMMANDINFO ci;
    memset(&ci, 0, sizeof(ci));
    ci.cbSize = sizeof(ci);
    ci.lpVerb = MAKEINTRESOURCEA(offset);
    ci.nShow = SW_SHOWNORMAL;
    _contextMenu->InvokeCommand(&ci);
  }

  void AddItems(HMENU menu, QList<QAction *> &actions, QWidget *parentWidget)
  {
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
      QAction *a = new QAction(this);
      actions.append(a);
      if (mi.fType & MFT_SEPARATOR)
      {
        a->setSeparator(true);
        continue;
      }
      a->setText(QString::fromWCharArray(text));
      // the menu logo of the options (IDB_MENU_LOGO: the program icon)
      const HBITMAP bitmap = mi.hbmpItem ? mi.hbmpItem : mi.hbmpUnchecked;
      if (bitmap)
        a->setIcon(bitmap->Icon);
      if (mi.hSubMenu)
      {
        QMenu *sub = new QMenu(parentWidget);
        _subMenus.push_back(sub);
        QList<QAction *> subActions;
        AddItems(mi.hSubMenu, subActions, parentWidget);
        sub->addActions(subActions);
        a->setMenu(sub);
        continue;
      }
      const UINT offset = mi.wID - kFirstCommandId;
      connect(a, &QAction::triggered, this, [this, offset]() { Invoke(offset); });
    }
  }

public:
  CMenu7z(QObject *parent): QObject(parent) {}

  ~CMenu7z() override
  {
    for (QPointer<QMenu> &m : _subMenus)
      delete m;
  }

  QList<QAction *> Create(const KFileItemList &items, QWidget *parentWidget)
  {
    CZipContextMenu *spec = new CZipContextMenu;
    _contextMenu = spec;
    // as the 7zFM panel fills it (CPanel::CreateSevenZipMenu)
    int index = 0;
    for (const KFileItem &item : items)
    {
      const QString path = item.localPath();
      if (path.isEmpty())
        return QList<QAction *>();
      if (item.isDir() && spec->_attribs.FirstDirIndex < 0)
        spec->_attribs.FirstDirIndex = index;
      spec->_fileNames.Add(NShim::ToU(path));
      index++;
    }
    QList<QAction *> actions;
    HMENU menu = CreatePopupMenu();
    if (SUCCEEDED(_contextMenu->QueryContextMenu(menu, 0, kFirstCommandId, kLastCommandId, CMF_NORMAL)))
      AddItems(menu, actions, parentWidget);
    DestroyMenu(menu);
    // the menu of the file manager has its own separators around plugins
    while (!actions.isEmpty() && actions.first()->isSeparator())
      delete actions.takeFirst();
    while (!actions.isEmpty() && actions.last()->isSeparator())
      delete actions.takeLast();
    return actions;
  }
};

class CSevenZipFileItemAction: public KAbstractFileItemActionPlugin
{
  Q_OBJECT

  QPointer<CMenu7z> _menu;

public:
  CSevenZipFileItemAction(QObject *parent, const QVariantList &): KAbstractFileItemActionPlugin(parent) {}

  QList<QAction *> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) override
  {
    // 7-Zip works on paths of the file system
    if (!fileItemInfos.isLocal() || fileItemInfos.items().isEmpty())
      return QList<QAction *>();
    InitProgramDir();
    // a new request means that the menu of the previous one has closed
    delete _menu;
    _menu = new CMenu7z(this);
    return _menu->Create(fileItemInfos.items(), parentWidget);
  }
};

K_PLUGIN_CLASS_WITH_JSON(CSevenZipFileItemAction, "sevenzipfileitemaction.json")

#include "FileItemAction.moc"
