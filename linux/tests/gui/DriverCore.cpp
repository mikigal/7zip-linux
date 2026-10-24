// DriverCore.cpp -- the parts of the test drivers of 7zG and 7zFM that
// answer dialogs, message boxes and file dialogs from a JSON script and log
// what the program shows (see gui_driver.cpp for the script format).

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>

#include <stdio.h>
#include <unistd.h>

#include <windows.h>
#include <CommCtrl.h>

#include "DriverCore.h"
#include "Platform.h"
#include "RcTables.h"
#include "ShimDialog.h"
#include "ShimWindow.h"
#include "Theme.h"

namespace NDriver {


CDriver g_Driver;

QString WindowText(HWND wnd)
{
  wchar_t buf[4096];
  const int n = GetWindowTextW(wnd, buf, (int)Z7_ARRAY_SIZE(buf));
  return QString::fromWCharArray(buf, n < 0 ? 0 : n);
}

QString DialogName(HWND dlg)
{
  QWidget *w = NShim::WidgetOf(dlg);
  return w ? w->objectName() : QString();
}

QJsonObject ControlState(HWND c)
{
  QJsonObject o;
  o["id"] = (int)c->Id;
  o["class"] = QString::fromLatin1(NShim::ClassName(c));
  o["text"] = WindowText(c);
  o["enabled"] = IsWindowEnabled(c) != FALSE;
  o["visible"] = IsWindowVisible(c) != FALSE;
  QWidget *w = c->Widget;
  if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
  {
    if (b->isCheckable())
      o["checked"] = b->isChecked();
  }
  else if (QComboBox *cb = qobject_cast<QComboBox *>(w))
  {
    QJsonArray items;
    for (int i = 0; i < cb->count(); i++)
      items.append(cb->itemText(i));
    o["items"] = items;
    o["current"] = cb->currentIndex();
  }
  else if (strcmp(NShim::ClassName(c), "SysListView32") == 0)
  {
    // the rows as the program sees them (texts of LVN_GETDISPINFO too)
    int numColumns = 0;
    for (;; numColumns++)
    {
      LVCOLUMNW col;
      memset(&col, 0, sizeof(col));
      col.mask = LVCF_WIDTH;
      if (!SendMessageW(c, LVM_GETCOLUMNW, (WPARAM)numColumns, (LPARAM)&col))
        break;
    }
    QJsonArray rows;
    const int count = (int)SendMessageW(c, LVM_GETITEMCOUNT, 0, 0);
    for (int i = 0; i < count; i++)
    {
      QJsonArray cells;
      for (int k = 0; k < qMax(1, numColumns); k++)
      {
        wchar_t buf[1024];
        buf[0] = 0;
        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.iSubItem = k;
        item.pszText = buf;
        item.cchTextMax = (int)Z7_ARRAY_SIZE(buf);
        SendMessageW(c, LVM_GETITEMTEXTW, (WPARAM)i, (LPARAM)&item);
        cells.append(QString::fromWCharArray(buf));
      }
      rows.append(cells);
    }
    o["rows"] = rows;
  }
  return o;
}

// The controls of a dialog, and those of its child dialogs (the pages of a
// property sheet) after them, with the index of the page.
static void AddControlStates(HWND dlg, QJsonArray &controls, int page = -1)
{
  int numPages = 0;
  for (HWND c : dlg->Children)
  {
    if (!NShim::FromHandle(c))
      continue;
    if (strcmp(NShim::ClassName(c), "#32770") == 0)
    {
      AddControlStates(c, controls, numPages++);
      continue;
    }
    QJsonObject o = ControlState(c);
    if (page >= 0)
      o["page"] = page;
    controls.append(o);
  }
}

QJsonObject DialogState(const char *event, HWND dlg)
{
  QJsonObject o;
  o["event"] = QString::fromLatin1(event);
  o["name"] = DialogName(dlg);
  o["caption"] = WindowText(dlg);
  QJsonArray controls;
  AddControlStates(dlg, controls);
  o["controls"] = controls;
  return o;
}

// The page of a property sheet that is shown, or NULL.
static HWND VisiblePage(HWND dlg)
{
  for (HWND c : dlg->Children)
    if (NShim::FromHandle(c) && !c->Children.empty() && IsWindowVisible(c)
        && strcmp(NShim::ClassName(c), "#32770") == 0)
      return c;
  return NULL;
}

HWND Control(HWND dlg, const QJsonValue &id)
{
  HWND c = GetDlgItem(dlg, id.toInt());
  // the controls of a property sheet page
  if (!c)
  {
    HWND page = VisiblePage(dlg);
    if (page)
      c = GetDlgItem(page, id.toInt());
  }
  if (!c)
    g_Driver.Fail(QStringLiteral("%1: no control %2").arg(DialogName(dlg)).arg(id.toInt()));
  return c;
}

// A control that the user can work with: visible and enabled.
HWND InputControl(HWND dlg, const QJsonValue &id)
{
  HWND c = Control(dlg, id);
  if (c && (!IsWindowVisible(c) || !IsWindowEnabled(c)))
  {
    g_Driver.Fail(QStringLiteral("%1: control %2 is %3").arg(DialogName(dlg)).arg(id.toInt())
        .arg(IsWindowVisible(c) ? QStringLiteral("disabled") : QStringLiteral("hidden")));
    return NULL;
  }
  return c;
}

// Applies one action. Returns false when the action must be tried again
// later (a "wait" whose condition does not hold yet).
bool Apply(HWND dlg, const QJsonObject &a)
{
  const QString name = DialogName(dlg);
  if (a.contains("text"))
  {
    const QJsonArray p = a["text"].toArray();
    HWND c = InputControl(dlg, p[0]);
    if (!c)
      return true;
    const QString v = p[1].toString();
    if (QComboBox *cb = qobject_cast<QComboBox *>(c->Widget.data()))
    {
      if (QLineEdit *e = cb->lineEdit())
      {
        e->setText(v);
        emit e->textEdited(v);
      }
      else
        g_Driver.Fail(QStringLiteral("%1: combo box %2 is not editable").arg(name).arg(c->Id));
    }
    else
      SetWindowTextW(c, NShim::ToU(v));
  }
  else if (a.contains("select"))
  {
    const QJsonArray p = a["select"].toArray();
    HWND c = InputControl(dlg, p[0]);
    QComboBox *cb = c ? qobject_cast<QComboBox *>(c->Widget.data()) : NULL;
    if (!cb)
      return true;
    const int i = cb->findText(p[1].toString());
    if (i < 0)
      g_Driver.Fail(QStringLiteral("%1: combo box %2 has no item \"%3\"").arg(name).arg(c->Id).arg(p[1].toString()));
    else
    {
      cb->setCurrentIndex(i);
      emit cb->activated(i);
    }
  }
  else if (a.contains("check"))
  {
    const QJsonArray p = a["check"].toArray();
    HWND c = InputControl(dlg, p[0]);
    QAbstractButton *b = c ? qobject_cast<QAbstractButton *>(c->Widget.data()) : NULL;
    if (b && b->isChecked() != p[1].toBool())
      b->click();
  }
  else if (a.contains("click"))
  {
    HWND c = InputControl(dlg, a["click"]);
    if (!c)
      return true;
    if (QAbstractButton *b = qobject_cast<QAbstractButton *>(c->Widget.data()))
      b->click();
    else
      g_Driver.Fail(QStringLiteral("%1: control %2 is not a button").arg(name).arg(c->Id));
  }
  else if (a.contains("expect") || a.contains("wait"))
  {
    const bool wait = a.contains("wait");
    const QJsonArray p = a[wait ? "wait" : "expect"].toArray();
    HWND c = Control(dlg, p[0]);
    if (!c)
      return true;
    const QString text = WindowText(c);
    const QRegularExpression re(p[1].toString());
    if (!re.match(text).hasMatch())
    {
      if (wait)
        return false;
      g_Driver.Fail(QStringLiteral("%1: control %2 is \"%3\", expected /%4/")
          .arg(name).arg(c->Id).arg(text, p[1].toString()));
    }
  }
  else if (a.contains("tab"))
  {
    QWidget *w = NShim::WidgetOf(dlg);
    QTabWidget *tabs = w ? w->findChild<QTabWidget *>(QStringLiteral("z7PropertySheetTabs")) : NULL;
    const int i = a["tab"].toInt();
    if (!tabs || i < 0 || i >= tabs->count())
      g_Driver.Fail(QStringLiteral("%1: no tab %2").arg(name).arg(i));
    else
      tabs->setCurrentIndex(i);
  }
  else if (a.contains("wait_file"))
  {
    if (!QFile::exists(a["wait_file"].toString()))
      return false;
  }
  else if (a.contains("screenshot"))
  {
    QWidget *w = NShim::WidgetOf(dlg);
    const QString path = g_Driver.ShotsDir + QLatin1Char('/') + a["screenshot"].toString();
    if (w && !g_Driver.ShotsDir.isEmpty() && !w->window()->grab().save(path))
      g_Driver.Fail(QStringLiteral("cannot save %1").arg(path));
  }
  else
    g_Driver.Fail(QStringLiteral("unknown action %1")
        .arg(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact))));
  return true;
}

// Runs the actions of a step in the dialog's modal loop, and logs the state
// of the dialog when it is hidden (EndDialog), before its controls go away.
// It is a child of the dialog widget, so it goes away with the dialog.
class CDialogRunner: public QObject
{
  HWND _dlg;
  QJsonArray _actions;
  int _next = 0;
  bool _inAction = false;
  bool _closed = false;

  static bool IsWait(const QJsonObject &a) { return a.contains("wait") || a.contains("wait_file"); }
public:
  CDialogRunner(HWND dlg, const QJsonArray &actions, QWidget *w):
      QObject(w), _dlg(dlg), _actions(actions)
  {
    w->installEventFilter(this);
  }

  bool Alive() const { return NShim::FromHandle(_dlg) && NShim::IsInModalLoop(_dlg); }

  void Run()
  {
    while (_next < _actions.size() && Alive())
    {
      _inAction = true;
      const bool done = Apply(_dlg, _actions[_next].toObject());
      _inAction = false;
      if (!done)
      {
        QTimer::singleShot(50, this, [this]() { Run(); });
        return;
      }
      _next++;
    }
  }

  bool eventFilter(QObject *watched, QEvent *e) override
  {
    if (e->type() == QEvent::Hide && watched == parent() && !_closed && NShim::FromHandle(_dlg))
    {
      _closed = true;
      g_Driver.Write(DialogState("dialog_closed", _dlg));
      // an action that closes the dialog is done; a wait may be cut short
      const int pending = _next + (_inAction ? 1 : 0);
      if (pending < _actions.size() && !IsWait(_actions[pending].toObject()))
        g_Driver.Fail(QStringLiteral("%1 closed before action %2").arg(DialogName(_dlg)).arg(pending));
    }
    return false;
  }
};

void OnModal(HWND dlg)
{
  const QString name = DialogName(dlg);
  g_Driver.Write(DialogState("dialog", dlg));

  QJsonObject step = g_Driver.Peek("dialog");
  if (!step.isEmpty() && step.value("name").toString() != name)
    step = QJsonObject();
  if (step.isEmpty() && !g_Driver.Passive.contains(name))
  {
    g_Driver.Fail(QStringLiteral("unexpected dialog %1 (step %2)").arg(name).arg(g_Driver.Next));
    EndDialog(dlg, IDCANCEL);
    return;
  }
  if (!step.isEmpty())
    g_Driver.Next++;
  CDialogRunner *runner = new CDialogRunner(dlg, step.value("actions").toArray(), NShim::WidgetOf(dlg));
  runner->Run();
}


int OnMessageBox(HWND /* owner */, const QString &text, const QString &caption, UINT type)
{
  g_Driver.Write(QJsonObject{ { "event", "messagebox" }, { "text", text },
      { "caption", caption }, { "type", (int)type } });
  const QJsonObject step = g_Driver.Peek("messagebox");
  if (step.isEmpty() || !text.contains(step.value("contains").toString()))
  {
    g_Driver.Fail(QStringLiteral("unexpected message box \"%1\" (step %2)").arg(text).arg(g_Driver.Next));
    const UINT buttons = type & MB_TYPEMASK;
    return (buttons == MB_OK) ? IDOK : (buttons == MB_YESNO ? IDNO : IDCANCEL);
  }
  g_Driver.Next++;
  return step.value("answer").toInt(IDOK);
}

bool OnFileDialog(NShim::CFileDialogRequest &r)
{
  g_Driver.Write(QJsonObject{ { "event", "filedialog" }, { "kind", (int)r.Kind },
      { "title", r.Title }, { "path", r.Path }, { "filters", QJsonArray::fromStringList(r.Filters) } });
  const QJsonObject step = g_Driver.Peek("filedialog");
  if (step.isEmpty())
  {
    g_Driver.Fail(QStringLiteral("unexpected file dialog (step %1)").arg(g_Driver.Next));
    return false;
  }
  g_Driver.Next++;
  const QJsonValue answer = step.value("answer");
  if (!answer.isString())
    return false;
  r.Path = answer.toString();
  if (step.contains("filter"))
    r.FilterIndex = step.value("filter").toInt();
  return true;
}

bool ReadScript(const QString &path)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
  if (!doc.isArray())
  {
    fprintf(stderr, "%s: %s: %s\n", qPrintable(g_Driver.Name), qPrintable(path), qPrintable(error.errorString()));
    return false;
  }
  QJsonArray steps = doc.array();
  g_Driver.Passive = { QStringLiteral("IDD_PROGRESS") };
  if (!steps.isEmpty() && steps[0].toObject().contains("passive"))
  {
    g_Driver.Passive.clear();
    for (const QJsonValue &v : steps[0].toObject().value("passive").toArray())
      g_Driver.Passive.insert(v.toString());
    steps.removeFirst();
  }
  g_Driver.Steps = steps;
  return true;
}

// Logs the URLs that the program opens (help topics) instead of opening them.
static CUrlLogger *g_UrlLogger;

bool ParseArgs(int argc, char *argv[], CArgs &args)
{
  args.First = argc;
  for (int i = 1; i < argc; i++)
  {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a == QLatin1String("--"))
    {
      args.First = i + 1;
      break;
    }
    if (i + 1 >= argc)
      break;
    if (a == QLatin1String("--script"))
      args.Script = QString::fromLocal8Bit(argv[++i]);
    else if (a == QLatin1String("--log"))
      args.Log = QString::fromLocal8Bit(argv[++i]);
    else if (a == QLatin1String("--shots"))
      g_Driver.ShotsDir = QString::fromLocal8Bit(argv[++i]);
    else if (a == QLatin1String("--timeout"))
      args.Timeout = atoi(argv[++i]);
  }
  return !args.Script.isEmpty() && !args.Log.isEmpty();
}

void SetProgramCommandLine(int argc, char *argv[], const CArgs &args)
{
  // the command line of the program: its name, then the arguments after --
  std::vector<const char *> list;
  list.push_back(argv[0]);
  for (int i = args.First; i < argc; i++)
    list.push_back(argv[i]);
  NPlatform::SetCommandLine((int)list.size(), list.data());
  NPlatform::InitModuleDirPrefix(argv[0]);
}

bool Start(const CArgs &args, const char *name)
{
  g_Driver.Name = QString::fromLatin1(name);
  g_Driver.Log.setFileName(args.Log);
  if (!g_Driver.Log.open(QIODevice::WriteOnly | QIODevice::Truncate) || !ReadScript(args.Script))
  {
    fprintf(stderr, "%s: cannot read %s or write %s\n", name, qPrintable(args.Script), qPrintable(args.Log));
    return false;
  }
  g_Driver.Write(QJsonObject{ { "event", "start" }, { "platform", QGuiApplication::platformName() } });
  g_UrlLogger = new CUrlLogger(qApp);
  for (const char *scheme : { "http", "https", "file" })
    QDesktopServices::setUrlHandler(QString::fromLatin1(scheme), g_UrlLogger, "openUrl");
  NShim::SetModalHook(OnModal);
  NShim::SetMessageBoxHook(OnMessageBox);
  NShim::SetFileDialogHook(OnFileDialog);

  // a watchdog for scripts that leave a dialog open
  QTimer *watchdog = new QTimer(qApp);
  watchdog->setSingleShot(true);
  QObject::connect(watchdog, &QTimer::timeout, []()
  {
    g_Driver.Fail(QStringLiteral("timeout"));
    for (QWidget *w : QApplication::topLevelWidgets())
    {
      HWND dlg = NShim::FromWidget(w);
      if (w->isVisible() && dlg && dlg->Widget == w)
        g_Driver.Write(DialogState("dialog_at_timeout", dlg));
    }
    g_Driver.Write(QJsonObject{ { "event", "exit" }, { "code", 124 } });
    _exit(124);
  });
  watchdog->start(args.Timeout * 1000);
  return true;
}

int Finish(int code)
{
  if (g_Driver.Next < g_Driver.Steps.size())
    g_Driver.Fail(QStringLiteral("the program ended before step %1").arg(g_Driver.Next));
  g_Driver.Write(QJsonObject{ { "event", "exit" }, { "code", code } });
  if (g_Driver.Failed)
    code = 100;
  return code;
}

}

#include "moc_DriverCore.cpp"
