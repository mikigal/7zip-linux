// fm_driver.cpp -- 7zFM with a test script: the program is the same
// (FileManager/FM.cpp on top of the shim), but a JSON script works its main
// window and answers its dialogs, message boxes and file dialogs.
//
// Usage: z7-fm-driver --script FILE --log FILE [--shots DIR] [--timeout SEC]
//            -- <7zFM arguments>
//
// The script has the steps of gui_driver.cpp ("dialog", "messagebox",
// "filedialog"), and steps for the main window:
//   {"on": "main", "actions": [...]}
// They run once the main window is shown, when no modal dialog is open; a
// dialog that an action opens takes the next step of the script. Actions:
//   {"key": "ctrl+A"}           a key press and release at the focus widget
//                               (Qt key names: "Return", "F5", "Down",
//                               "shift+Insert", "alt+F12", "keypad++", "keypad+*")
//   {"type": "text"}            types the text at the focus widget
//   {"command": id}             a menu or toolbar command (WM_COMMAND)
//   {"menu": index}             opens a menu of the menu bar, logs its items
//                               with their submenus (event "menu") and
//                               closes it
//   {"choose": [index, "Item", "Subitem"]}
//                               opens a menu of the menu bar and chooses an
//                               item by its text (without '&' and shortcut)
//   {"toolbar": id}             clicks the button of the main toolbar
//   {"click": {"panel": 0, "item": "name", "double": false, "mods": "ctrl"}}
//                               clicks an item of the list of a panel
//   {"header": {"panel": 0, "column": "Size"}}
//                               clicks a column header of the list
//   {"focus": panel}            gives the focus to the list of a panel
//   {"address": "path"}         types a path into the address bar of the
//                               focused panel ({"address": [panel, path]})
//                               and presses Enter
//   {"expect": {...}}           fails unless the panel state matches;
//   {"wait": {...}}             waits until it matches. Conditions: "panel"
//                               (default: the focused one), "path" (regex of
//                               the address bar), "focused" (item name),
//                               "selected" (names), "items" (names in list
//                               order), "count", "title" (regex),
//                               "status": [part, regex], "panels" (number
//                               shown), "focus_in_list" (true/false),
//                               "buttons" (number on the toolbar)
//   {"wait_file": "path"}       waits until the file exists
//   {"log": "panels"}           logs the state of both panels and the focus
//   {"screenshot": "name.png"}  saves a picture of the main window in --shots
//   {"close": true}             closes the main window (WM_CLOSE)
//   {"sleep": ms}               lets the program run for a while
// When all steps are done, the driver closes the main window.

#include <QApplication>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

// the Qt headers come first: windows.h defines _WIN64, which Qt reads as
// a Windows build
#include <windows.h>
#include <CommCtrl.h>

#include "UpstreamPosix.h"

#include "../../../CPP/7zip/UI/FileManager/App.h"

#include "DriverCore.h"
#include "ShimWindow.h"
#include "Theme.h"

using namespace NDriver;

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow);

namespace {

QString ItemText(HWND list, int index, int column = 0)
{
  wchar_t buf[2048];
  buf[0] = 0;
  LVITEMW item;
  memset(&item, 0, sizeof(item));
  item.iSubItem = column;
  item.pszText = buf;
  item.cchTextMax = (int)Z7_ARRAY_SIZE(buf);
  SendMessageW(list, LVM_GETITEMTEXTW, (WPARAM)index, (LPARAM)&item);
  return QString::fromWCharArray(buf);
}

QJsonObject PanelState(unsigned index)
{
  CPanel &p = g_App.Panels[index];
  QJsonObject o;
  o["panel"] = (int)index;
  o["created"] = p.PanelCreated;
  if (!p.PanelCreated)
    return o;
  HWND list = p._listView;
  o["visible"] = IsWindowVisible(p) != FALSE;
  o["path"] = WindowText(p._headerComboBox);
  o["prefix"] = QString::fromWCharArray(p._currentFolderPrefix.Ptr(), (int)p._currentFolderPrefix.Len());
  const int count = (int)SendMessageW(list, LVM_GETITEMCOUNT, 0, 0);
  QJsonArray items, selected;
  for (int i = 0; i < count; i++)
  {
    const QString name = ItemText(list, i);
    items.append(name);
    if (SendMessageW(list, LVM_GETITEMSTATE, (WPARAM)i, LVIS_SELECTED) & LVIS_SELECTED)
      selected.append(name);
  }
  o["items"] = items;
  o["selected"] = selected;
  const int focused = (int)SendMessageW(list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_FOCUSED);
  o["focused"] = focused >= 0 ? ItemText(list, focused) : QString();
  QJsonArray columns;
  for (int c = 0;; c++)
  {
    wchar_t buf[256];
    buf[0] = 0;
    LVCOLUMNW col;
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT;
    col.pszText = buf;
    col.cchTextMax = (int)Z7_ARRAY_SIZE(buf);
    if (!SendMessageW(list, LVM_GETCOLUMNW, (WPARAM)c, (LPARAM)&col))
      break;
    columns.append(QString::fromWCharArray(buf));
  }
  o["columns"] = columns;
  QJsonArray status;
  for (unsigned part = 0; part < 4; part++)
  {
    wchar_t buf[512];
    buf[0] = 0;
    SendMessageW(p._statusBar, SB_GETTEXTW, part, (LPARAM)buf);
    status.append(QString::fromWCharArray(buf));
  }
  o["status"] = status;
  o["focus_in_list"] = (GetFocus() == (HWND)list);
  return o;
}

unsigned PanelOf(const QJsonObject &c)
{
  if (c.contains("panel"))
    return (unsigned)c["panel"].toInt();
  return g_App.LastFocusedPanel;
}

// Empty if the panel state matches the conditions, else what differs.
QString Mismatch(const QJsonObject &c)
{
  if (c.contains("panels") && (int)g_App.NumPanels != c["panels"].toInt())
    return QStringLiteral("%1 panels").arg(g_App.NumPanels);
  if (c.contains("buttons"))
  {
    HWND tb = g_App._toolBar;
    const int n = (tb && NShim::FromHandle(tb)) ? (int)SendMessageW(tb, TB_BUTTONCOUNT, 0, 0) : 0;
    if (n != c["buttons"].toInt())
      return QStringLiteral("%1 toolbar buttons").arg(n);
  }
  if (c.contains("title"))
  {
    const QString title = WindowText(g_HWND);
    if (!QRegularExpression(c["title"].toString()).match(title).hasMatch())
      return QStringLiteral("title \"%1\"").arg(title);
  }
  const unsigned index = PanelOf(c);
  if (index >= kNumPanelsMax)
    return QStringLiteral("no panel %1").arg(index);
  const QJsonObject s = PanelState(index);
  if (!s["created"].toBool())
    return QStringLiteral("panel %1 is not created").arg(index);
  if (c.contains("path") && !QRegularExpression(c["path"].toString()).match(s["path"].toString()).hasMatch())
    return QStringLiteral("path \"%1\"").arg(s["path"].toString());
  if (c.contains("focused") && s["focused"].toString() != c["focused"].toString())
    return QStringLiteral("focused \"%1\"").arg(s["focused"].toString());
  if (c.contains("selected") && s["selected"].toArray() != c["selected"].toArray())
    return QStringLiteral("selected %1").arg(QString::fromUtf8(QJsonDocument(s["selected"].toArray()).toJson(QJsonDocument::Compact)));
  if (c.contains("items") && s["items"].toArray() != c["items"].toArray())
    return QStringLiteral("items %1").arg(QString::fromUtf8(QJsonDocument(s["items"].toArray()).toJson(QJsonDocument::Compact)));
  if (c.contains("count") && s["items"].toArray().size() != c["count"].toInt())
    return QStringLiteral("%1 items").arg(s["items"].toArray().size());
  if (c.contains("columns") && s["columns"].toArray() != c["columns"].toArray())
    return QStringLiteral("columns %1").arg(QString::fromUtf8(QJsonDocument(s["columns"].toArray()).toJson(QJsonDocument::Compact)));
  if (c.contains("status"))
  {
    const QJsonArray p = c["status"].toArray();
    const QString text = s["status"].toArray()[p[0].toInt()].toString();
    if (!QRegularExpression(p[1].toString()).match(text).hasMatch())
      return QStringLiteral("status %1 \"%2\"").arg(p[0].toInt()).arg(text);
  }
  if (c.contains("focus_in_list") && s["focus_in_list"].toBool() != c["focus_in_list"].toBool())
    return QStringLiteral("focus_in_list %1").arg(s["focus_in_list"].toBool());
  return QString();
}

bool ParseKey(const QString &spec, int &key, Qt::KeyboardModifiers &mods)
{
  mods = Qt::NoModifier;
  QStringList parts = spec.split(QLatin1Char('+'));
  // "keypad++", "ctrl++"
  QString name = parts.takeLast();
  if (name.isEmpty() && !parts.isEmpty())
  {
    parts.removeLast();
    name = QStringLiteral("+");
  }
  for (const QString &m : parts)
  {
    const QString l = m.toLower();
    if (l == QLatin1String("ctrl"))
      mods |= Qt::ControlModifier;
    else if (l == QLatin1String("shift"))
      mods |= Qt::ShiftModifier;
    else if (l == QLatin1String("alt"))
      mods |= Qt::AltModifier;
    else if (l == QLatin1String("keypad"))
      mods |= Qt::KeypadModifier;
    else
      return false;
  }
  if (name.size() == 1)
  {
    key = name[0].toUpper().unicode();
    return true;
  }
  const QKeySequence seq = QKeySequence::fromString(name);
  if (seq.isEmpty())
    return false;
  key = seq[0].key();
  return key != Qt::Key_unknown;
}

QWidget *ListWidget(unsigned panel)
{
  HWND list = g_App.Panels[panel]._listView;
  return NShim::WidgetOf(list);
}

class CMainRunner: public QObject
{
  QJsonArray _actions;
  int _next = 0;
  bool _busy = false;
  bool _closing = false;
  int _sleepUntil = 0;
  QElapsedTimer _clock;
  QString _waitMismatch;
  qint64 _waitLogged = 0;
  QTimer _timer;

public:
  CMainRunner()
  {
    _clock.start();
    connect(&_timer, &QTimer::timeout, this, [this]() { Tick(); });
    _timer.start(20);
  }

  bool Ready() const
  {
    if (!g_HWND || !NShim::FromHandle(g_HWND))
      return false;
    QWidget *w = NShim::WidgetOf(g_HWND);
    // the window is active and has the focus: keys go where the user's would
    return w && w->isVisible() && w->isActiveWindow() && QApplication::focusWidget()
        && !QApplication::activeModalWidget() && !QApplication::activePopupWidget();
  }

  void Close()
  {
    if (_closing)
      return;
    _closing = true;
    PostMessageW(g_HWND, WM_CLOSE, 0, 0);
  }

  void Tick()
  {
    if (_busy || _closing || !Ready() || _clock.elapsed() < _sleepUntil)
      return;
    _busy = true;
    for (;;)
    {
      if (_next >= _actions.size())
      {
        const QJsonObject step = g_Driver.Peek("main");
        if (step.isEmpty())
        {
          // the last steps are done, or the next one waits for a dialog
          if (g_Driver.Next >= g_Driver.Steps.size())
            Close();
          break;
        }
        g_Driver.Next++;
        _actions = step.value("actions").toArray();
        _next = 0;
        continue;
      }
      const QJsonObject a = _actions[_next].toObject();
      if (!Apply(a))
        break;
      _next++;
      if (_closing || !Ready() || _clock.elapsed() < _sleepUntil)
        break;
      // the program handles what an action posted before the next step
      if (!(a.contains("expect") || a.contains("wait") || a.contains("wait_file") || a.contains("log")
          || a.contains("screenshot")))
        break;
    }
    _busy = false;
  }

  bool Apply(const QJsonObject &a)
  {
    if (a.contains("key"))
    {
      int key = 0;
      Qt::KeyboardModifiers mods;
      if (!ParseKey(a["key"].toString(), key, mods))
      {
        g_Driver.Fail(QStringLiteral("bad key %1").arg(a["key"].toString()));
        return true;
      }
      QWidget *w = QApplication::focusWidget();
      if (!w)
        w = NShim::WidgetOf(g_HWND);
      QTest::keyClick(w, (Qt::Key)key, mods);
    }
    else if (a.contains("type"))
    {
      QWidget *w = QApplication::focusWidget();
      if (w)
        QTest::keyClicks(w, a["type"].toString());
    }
    else if (a.contains("command"))
      SendMessageW(g_HWND, WM_COMMAND, MAKEWPARAM(a["command"].toInt(), 0), 0);
    else if (a.contains("menu"))
      LogMenu(a["menu"].toInt());
    else if (a.contains("choose"))
      ChooseMenuItem(a["choose"].toArray());
    else if (a.contains("toolbar"))
    {
      HWND tb = g_App._toolBar;
      const int index = (int)SendMessageW(tb, TB_COMMANDTOINDEX, (WPARAM)a["toolbar"].toInt(), 0);
      QWidget *w = NShim::WidgetOf(tb);
      QList<QToolButton *> buttons = w ? w->findChildren<QToolButton *>(QString(), Qt::FindDirectChildrenOnly) : QList<QToolButton *>();
      if (index < 0 || index >= buttons.size())
        g_Driver.Fail(QStringLiteral("no toolbar button %1").arg(a["toolbar"].toInt()));
      else
        buttons[index]->click();
    }
    else if (a.contains("click"))
      Click(a["click"].toObject());
    else if (a.contains("header"))
      ClickHeader(a["header"].toObject());
    else if (a.contains("focus"))
      SetFocus(g_App.Panels[a["focus"].toInt()]._listView);
    else if (a.contains("address"))
    {
      const QJsonValue v = a["address"];
      const unsigned panel = v.isArray() ? (unsigned)v.toArray()[0].toInt() : g_App.LastFocusedPanel;
      const QString path = v.isArray() ? v.toArray()[1].toString() : v.toString();
      HWND edit = g_App.Panels[panel]._comboBoxEdit;
      SetFocus(edit);
      QWidget *w = NShim::WidgetOf(edit);
      if (!w)
        g_Driver.Fail(QStringLiteral("no address bar"));
      else
      {
        QTest::keyClick(w, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(w, path);
        QTest::keyClick(w, Qt::Key_Return);
      }
    }
    else if (a.contains("expect") || a.contains("wait"))
    {
      const bool wait = a.contains("wait");
      const QString m = Mismatch(a[wait ? "wait" : "expect"].toObject());
      if (!m.isEmpty())
      {
        if (wait)
        {
          // a wait that takes long is logged, to see what it waits for
          if (m != _waitMismatch || _clock.elapsed() - _waitLogged > 2000)
          {
            if (_clock.elapsed() - _waitLogged > 2000)
              g_Driver.Write(QJsonObject{ { "event", "waiting" }, { "mismatch", m } });
            _waitMismatch = m;
            _waitLogged = _clock.elapsed();
          }
          return false;
        }
        g_Driver.Fail(QStringLiteral("main: %1, expected %2").arg(m,
            QString::fromUtf8(QJsonDocument(a["expect"].toObject()).toJson(QJsonDocument::Compact))));
      }
    }
    else if (a.contains("wait_file"))
      return QFile::exists(a["wait_file"].toString());
    else if (a.contains("log"))
    {
      for (unsigned i = 0; i < kNumPanelsMax; i++)
      {
        QJsonObject o = PanelState(i);
        o["event"] = QStringLiteral("panel");
        g_Driver.Write(o);
      }
      QWidget *f = QApplication::focusWidget();
      g_Driver.Write(QJsonObject{ { "event", "focus" },
          { "widget", f ? QString::fromLatin1(f->metaObject()->className()) : QString() },
          { "window", WindowText(GetFocus()) } });
    }
    else if (a.contains("screenshot"))
    {
      QWidget *w = NShim::WidgetOf(g_HWND);
      const QString path = g_Driver.ShotsDir + QLatin1Char('/') + a["screenshot"].toString();
      if (w && !g_Driver.ShotsDir.isEmpty() && !w->grab().save(path))
        g_Driver.Fail(QStringLiteral("cannot save %1").arg(path));
    }
    else if (a.contains("close"))
      Close();
    else if (a.contains("sleep"))
      _sleepUntil = (int)_clock.elapsed() + a["sleep"].toInt();
    else
      g_Driver.Fail(QStringLiteral("unknown main action %1")
          .arg(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact))));
    return true;
  }

  void Click(const QJsonObject &c)
  {
    const unsigned panel = (unsigned)c.value("panel").toInt((int)g_App.LastFocusedPanel);
    HWND list = g_App.Panels[panel]._listView;
    const QString name = c["item"].toString();
    const int count = (int)SendMessageW(list, LVM_GETITEMCOUNT, 0, 0);
    int index = -1;
    for (int i = 0; i < count && index < 0; i++)
      if (ItemText(list, i) == name)
        index = i;
    if (index < 0)
    {
      g_Driver.Fail(QStringLiteral("panel %1 has no item \"%2\"").arg(panel).arg(name));
      return;
    }
    RECT r;
    r.left = LVIR_LABEL;
    SendMessageW(list, LVM_ENSUREVISIBLE, (WPARAM)index, FALSE);
    SendMessageW(list, LVM_GETITEMRECT, (WPARAM)index, (LPARAM)&r);
    // the view that shows the list now (the list widget or a child of it)
    QWidget *lw = ListWidget(panel);
    QAbstractItemView *view = qobject_cast<QAbstractItemView *>(lw);
    if (!view)
      for (QAbstractItemView *v : lw->findChildren<QAbstractItemView *>())
        if (v->isVisible())
        {
          view = v;
          break;
        }
    // the rectangle is in client coordinates of the list
    QPoint pos((r.left + r.right) / 2, (r.top + r.bottom) / 2);
    QWidget *target = lw;
    if (view)
    {
      pos = view->viewport()->mapFrom(lw, pos);
      target = view->viewport();
    }
    Qt::KeyboardModifiers mods;
    const QString m = c["mods"].toString().toLower();
    if (m.contains(QLatin1String("ctrl")))
      mods |= Qt::ControlModifier;
    if (m.contains(QLatin1String("shift")))
      mods |= Qt::ShiftModifier;
    const Qt::MouseButton button = c["button"].toString() == QLatin1String("right") ? Qt::RightButton : Qt::LeftButton;
    if (c["double"].toBool())
    {
      QTest::mouseClick(target, button, mods, pos);
      QTest::mouseDClick(target, button, mods, pos);
    }
    else
      QTest::mouseClick(target, button, mods, pos);
  }

  void ClickHeader(const QJsonObject &c)
  {
    const unsigned panel = (unsigned)c.value("panel").toInt((int)g_App.LastFocusedPanel);
    const QString name = c["column"].toString();
    QWidget *lw = ListWidget(panel);
    QHeaderView *header = NULL;
    for (QTreeView *v : lw->findChildren<QTreeView *>())
      if (v->isVisible())
        header = v->header();
    if (!header)
    {
      g_Driver.Fail(QStringLiteral("panel %1 shows no header").arg(panel));
      return;
    }
    for (int i = 0; i < header->count(); i++)
    {
      if (header->model()->headerData(i, Qt::Horizontal).toString() != name)
        continue;
      const QPoint pos(header->sectionViewportPosition(i) + header->sectionSize(i) / 2, header->height() / 2);
      QTest::mouseClick(header->viewport(), c["button"].toString() == QLatin1String("right") ?
          Qt::RightButton : Qt::LeftButton, Qt::NoModifier, pos);
      return;
    }
    g_Driver.Fail(QStringLiteral("panel %1 has no column \"%2\"").arg(panel).arg(name));
  }

  QMenu *BarMenu(int index)
  {
    QWidget *frame = NShim::WidgetOf(g_HWND);
    QMenuBar *bar = frame ? frame->findChild<QMenuBar *>() : NULL;
    if (!bar || index < 0 || index >= bar->actions().size() || !bar->actions()[index]->menu())
    {
      g_Driver.Fail(QStringLiteral("no menu %1").arg(index));
      return NULL;
    }
    return bar->actions()[index]->menu();
  }

  // The items of a menu; submenus are opened and logged too.
  static QJsonArray MenuItems(QMenu *menu)
  {
    // as when it opens: the window updates the items first
    emit menu->aboutToShow();
    QJsonArray items;
    for (QAction *act : menu->actions())
    {
      QJsonObject it;
      it["text"] = act->isSeparator() ? QStringLiteral("-") : act->text();
      it["enabled"] = act->isEnabled();
      if (act->isCheckable())
        it["checked"] = act->isChecked();
      if (act->menu())
      {
        it["submenu"] = true;
        it["items"] = MenuItems(act->menu());
      }
      items.append(it);
    }
    emit menu->aboutToHide();
    return items;
  }

  void LogMenu(int index)
  {
    QMenu *menu = BarMenu(index);
    if (!menu)
      return;
    QWidget *frame = NShim::WidgetOf(g_HWND);
    QMenuBar *bar = frame->findChild<QMenuBar *>();
    g_Driver.Write(QJsonObject{ { "event", "menu" }, { "index", index },
        { "title", bar->actions()[index]->text() }, { "items", MenuItems(menu) } });
  }

  // The text of an item without '&' and the shortcut.
  static QString PlainText(const QAction *act)
  {
    QString s = act->text().section(QLatin1Char('\t'), 0, 0);
    s.remove(QLatin1Char('&'));
    return s;
  }

  // Opens a menu of the menu bar and its submenus by the texts of the items,
  // and chooses the last item.
  void ChooseMenuItem(const QJsonArray &p)
  {
    QMenu *menu = BarMenu(p[0].toInt());
    QList<QMenu *> opened;
    for (int i = 1; menu && i < p.size(); i++)
    {
      emit menu->aboutToShow();
      opened.append(menu);
      QAction *found = NULL;
      for (QAction *act : menu->actions())
        if (!act->isSeparator() && PlainText(act) == p[i].toString())
          found = act;
      if (!found)
      {
        g_Driver.Fail(QStringLiteral("no menu item \"%1\"").arg(p[i].toString()));
        break;
      }
      if (i + 1 < p.size())
        menu = found->menu();
      else if (!found->isEnabled())
        g_Driver.Fail(QStringLiteral("menu item \"%1\" is disabled").arg(p[i].toString()));
      else
        found->trigger();
    }
    for (int i = (int)opened.size() - 1; i >= 0; i--)
      emit opened[i]->aboutToHide();
  }
};

}

int main(int argc, char *argv[])
{
  CArgs args;
  if (!ParseArgs(argc, argv, args))
  {
    fprintf(stderr, "usage: z7-fm-driver --script FILE --log FILE [--shots DIR] [--timeout SEC] -- ARGS\n");
    return 2;
  }
  SetProgramCommandLine(argc, argv, args);

  int qtArgc = 1;
  char *qtArgv[] = { argv[0], NULL };
  QApplication app(qtArgc, qtArgv);
  NTheme::Apply(app);
  if (!Start(args, "z7-fm-driver"))
    return 2;
  CMainRunner runner;

  char emptyCmdLine[] = "";
  return Finish(WinMain(NULL, NULL, emptyCmdLine, SW_SHOWNORMAL));
}
