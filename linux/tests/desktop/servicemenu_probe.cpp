// servicemenu_probe.cpp
// z7-servicemenu-probe: prints the actions that KIO gives a file manager for
// some files, as Dolphin builds its context menu (KFileItemActions), one
// line per action: "submenu > text<TAB>command<TAB>icon"; a submenu is
// "text >", a separator "-". The service menus come from
// $XDG_DATA_DIRS/kio/servicemenus, the plugins from
// $QT_PLUGIN_PATH/kf6/kfileitemaction.
//
// Usage: z7-servicemenu-probe [--plugins] [--trigger "SUBMENU > TEXT"] FILE...
//   --plugins  the actions of the plugins instead of the service menus
//   --trigger  then runs the action with that path

#include <KFileItem>
#include <KFileItemActions>
#include <KFileItemListProperties>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QUrl>

#include <cstdio>

static QAction *Print(const QMenu *menu, const QString &prefix, const QString &trigger)
{
  QAction *found = nullptr;
  for (QAction *a : menu->actions())
  {
    if (a->isSeparator())
    {
      std::printf("%s-\n", qPrintable(prefix));
      continue;
    }
    QString text = a->text();
    text.remove(QLatin1Char('&'));
    if (a->menu())
    {
      std::printf("%s >\t\t%s\n", qPrintable(prefix + text), a->icon().isNull() ? "" : "icon");
      QAction *f = Print(a->menu(), prefix + text + QLatin1String(" > "), trigger);
      if (f)
        found = f;
      continue;
    }
    std::printf("%s\t%s\t%s\n", qPrintable(prefix + text), qPrintable(a->data().toString()),
        a->icon().isNull() ? "" : "icon");
    if (prefix + text == trigger)
      found = a;
  }
  return found;
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  bool plugins = false;
  QString trigger;
  KFileItemList items;
  for (int i = 1; i < argc; i++)
  {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg == QLatin1String("--plugins"))
      plugins = true;
    else if (arg == QLatin1String("--trigger") && i + 1 < argc)
      trigger = QString::fromLocal8Bit(argv[++i]);
    else
      items << KFileItem(QUrl::fromLocalFile(arg));
  }
  KFileItemActions actions;
  actions.setItemListProperties(KFileItemListProperties(items));
  QMenu menu;
  actions.addActionsTo(&menu, plugins ? KFileItemActions::MenuActionSource::Plugins
      : KFileItemActions::MenuActionSource::Services);
  QAction *action = Print(&menu, QString(), trigger);
  std::fflush(stdout);
  if (!trigger.isEmpty())
  {
    if (!action)
    {
      std::fprintf(stderr, "no action %s\n", qPrintable(trigger));
      return 1;
    }
    action->trigger();
  }
  return 0;
}
