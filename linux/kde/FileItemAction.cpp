// FileItemAction.cpp -- the 7-Zip context menu of KDE file managers
// (Dolphin, Konqueror, Krusader): a KFileItemActions plugin with the menu of
// the Windows shell extension (linux/shell/ShellMenu.h) as Qt actions.

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>

#include <QAction>
#include <QMenu>
#include <QPointer>

#include <vector>

#include "ShellMenu.h"

// the switch of the user questions, which the shell extension DLL defines
// (Explorer/DllExportsExplorer.cpp)
bool g_DisableUserQuestions;

// The 7-Zip menu of one request and the actions made from it.
class CMenu7z: public QObject
{
  NShellMenu::CMenu _menu;
  std::vector<QPointer<QMenu> > _subMenus;

  QList<QAction *> MakeActions(const QList<NShellMenu::CItem> &items, QWidget *parentWidget)
  {
    QList<QAction *> actions;
    for (const NShellMenu::CItem &item : items)
    {
      QAction *a = new QAction(this);
      actions.append(a);
      if (item.Separator)
      {
        a->setSeparator(true);
        continue;
      }
      a->setText(item.Text);
      a->setIcon(item.Icon);
      if (item.IsSubMenu())
      {
        QMenu *sub = new QMenu(parentWidget);
        _subMenus.push_back(sub);
        sub->addActions(MakeActions(item.Children, parentWidget));
        a->setMenu(sub);
        continue;
      }
      const int command = item.Command;
      connect(a, &QAction::triggered, this, [this, command]() { _menu.Invoke(command); });
    }
    return actions;
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
    QStringList paths;
    for (const KFileItem &item : items)
    {
      const QString path = item.localPath();
      if (path.isEmpty())
        return QList<QAction *>();
      paths.append(path);
    }
    return MakeActions(_menu.Create(paths), parentWidget);
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
    static bool s_ProgramDir = false;
    if (!s_ProgramDir)
    {
      NShellMenu::SetProgramDir();
      s_ProgramDir = true;
    }
    // a new request means that the menu of the previous one has closed
    delete _menu;
    _menu = new CMenu7z(this);
    return _menu->Create(fileItemInfos.items(), parentWidget);
  }
};

K_PLUGIN_CLASS_WITH_JSON(CSevenZipFileItemAction, "sevenzipfileitemaction.json")

#include "FileItemAction.moc"
