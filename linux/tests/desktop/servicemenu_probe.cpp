// servicemenu_probe.cpp
// z7-servicemenu-probe: prints the actions that the KIO service menus give a
// file manager for some files, as Dolphin builds its context menu
// (KFileItemActions), one line per action: "submenu/text<TAB>command".
// The service menus come from $XDG_DATA_DIRS/kio/servicemenus.
//
// Usage: z7-servicemenu-probe FILE...

#include <KFileItem>
#include <KFileItemActions>
#include <KFileItemListProperties>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QUrl>

#include <cstdio>

static void Print(const QMenu *menu, const QString &prefix)
{
  for (const QAction *a : menu->actions())
  {
    if (a->isSeparator())
      continue;
    QString text = a->text();
    text.remove(QLatin1Char('&'));
    if (a->menu())
    {
      Print(a->menu(), prefix + text + QLatin1Char('/'));
      continue;
    }
    std::printf("%s\t%s\n", qPrintable(prefix + text), qPrintable(a->data().toString()));
  }
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  KFileItemList items;
  for (int i = 1; i < argc; i++)
    items << KFileItem(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[i])));
  KFileItemActions actions;
  actions.setItemListProperties(KFileItemListProperties(items));
  QMenu menu;
  actions.addActionsTo(&menu, KFileItemActions::MenuActionSource::Services);
  Print(&menu, QString());
  return 0;
}
