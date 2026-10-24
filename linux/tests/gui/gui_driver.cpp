// gui_driver.cpp -- 7zG with a test script: the program is the same
// (GUI/GUI.cpp on top of the shim), but the dialogs, message boxes and file
// dialogs that it opens are answered by a JSON script instead of a user.
//
// Usage: z7-gui-driver --script FILE --log FILE [--shots DIR] [--timeout SEC]
//            -- <7zG arguments>
//
// The script is an array of steps, taken in order as the program asks:
//   {"on": "dialog", "name": "IDD_EXTRACT", "actions": [...]}
//   {"on": "messagebox", "contains": "text", "answer": 6}
//   {"on": "filedialog", "answer": "/path"}        (null answers Cancel)
// Dialog actions, applied in order once the dialog runs its modal loop:
//   {"text": [id, "value"]}       types the value into an edit or combo box
//   {"select": [id, "item"]}      selects a combo box item, as the user does
//   {"check": [id, true]}         clicks a check box or radio button if needed
//   {"click": id}                 clicks a button
//   {"expect": [id, "regex"]}     fails the script unless the text matches
//   {"wait": [id, "regex"]}       waits until the text matches
//   {"screenshot": "name.png"}    saves a picture of the dialog in --shots
// Dialogs named in "passive" ({"passive": ["IDD_PROGRESS"]} as the first
// element; IDD_PROGRESS by default) run on their own unless a step names
// them. Anything else that the script does not expect fails it.
//
// The log has one JSON object per line: each dialog with the state of its
// controls when it is shown and when it is closed, each message box and file
// dialog, script errors, and the exit code. The exit code is that of 7zG,
// or 100 when the script failed.

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QTreeWidget>

#include <stdio.h>
#include <unistd.h>

#include <windows.h>
#include <CommCtrl.h>

#include "Platform.h"
#include "RcTables.h"
#include "ShimDialog.h"
#include "ShimWindow.h"
#include "Theme.h"

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow);

namespace {

struct CDriver
{
  QJsonArray Steps;
  int Next = 0;
  QSet<QString> Passive;
  QString ShotsDir;
  QFile Log;
  bool Failed = false;

  void Write(QJsonObject o)
  {
    Log.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    Log.write("\n");
    Log.flush();
  }

  void Fail(const QString &message)
  {
    Failed = true;
    fprintf(stderr, "z7-gui-driver: %s\n", qPrintable(message));
    Write(QJsonObject{ { "event", "error" }, { "message", message } });
  }

  // The next step if it is of the given kind, else an empty object.
  QJsonObject Peek(const char *kind) const
  {
    if (Next >= Steps.size())
      return QJsonObject();
    const QJsonObject s = Steps[Next].toObject();
    return s.value("on").toString() == QLatin1String(kind) ? s : QJsonObject();
  }
};

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
  else if (QTreeWidget *t = qobject_cast<QTreeWidget *>(w))
  {
    QJsonArray rows;
    for (int i = 0; i < t->topLevelItemCount(); i++)
    {
      QJsonArray cells;
      const QTreeWidgetItem *item = t->topLevelItem(i);
      for (int k = 0; k < t->columnCount(); k++)
        cells.append(item->text(k));
      rows.append(cells);
    }
    o["rows"] = rows;
  }
  return o;
}

QJsonObject DialogState(const char *event, HWND dlg)
{
  QJsonObject o;
  o["event"] = QString::fromLatin1(event);
  o["name"] = DialogName(dlg);
  o["caption"] = WindowText(dlg);
  QJsonArray controls;
  for (HWND c : dlg->Children)
    if (NShim::FromHandle(c))
      controls.append(ControlState(c));
  o["controls"] = controls;
  return o;
}

HWND Control(HWND dlg, const QJsonValue &id)
{
  HWND c = GetDlgItem(dlg, id.toInt());
  if (!c)
    g_Driver.Fail(QStringLiteral("%1: no control %2").arg(DialogName(dlg)).arg(id.toInt()));
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
    HWND c = Control(dlg, p[0]);
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
    HWND c = Control(dlg, p[0]);
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
    HWND c = Control(dlg, p[0]);
    QAbstractButton *b = c ? qobject_cast<QAbstractButton *>(c->Widget.data()) : NULL;
    if (b && b->isChecked() != p[1].toBool())
      b->click();
  }
  else if (a.contains("click"))
  {
    HWND c = Control(dlg, a["click"]);
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
  bool _closed = false;
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
      if (!Apply(_dlg, _actions[_next].toObject()))
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
    fprintf(stderr, "z7-gui-driver: %s: %s\n", qPrintable(path), qPrintable(error.errorString()));
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

}

int main(int argc, char *argv[])
{
  QString script, log;
  int timeout = 60;
  int first = argc;
  for (int i = 1; i < argc; i++)
  {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a == QLatin1String("--"))
    {
      first = i + 1;
      break;
    }
    if (i + 1 >= argc)
      break;
    if (a == QLatin1String("--script"))
      script = QString::fromLocal8Bit(argv[++i]);
    else if (a == QLatin1String("--log"))
      log = QString::fromLocal8Bit(argv[++i]);
    else if (a == QLatin1String("--shots"))
      g_Driver.ShotsDir = QString::fromLocal8Bit(argv[++i]);
    else if (a == QLatin1String("--timeout"))
      timeout = atoi(argv[++i]);
  }
  if (script.isEmpty() || log.isEmpty())
  {
    fprintf(stderr, "usage: z7-gui-driver --script FILE --log FILE [--shots DIR] [--timeout SEC] -- ARGS\n");
    return 2;
  }

  // the command line of 7zG: the program name, then the arguments after --
  std::vector<const char *> args;
  args.push_back(argv[0]);
  for (int i = first; i < argc; i++)
    args.push_back(argv[i]);
  NPlatform::SetCommandLine((int)args.size(), args.data());
  NPlatform::InitModuleDirPrefix(argv[0]);

  int qtArgc = 1;
  char *qtArgv[] = { argv[0], NULL };
  QApplication app(qtArgc, qtArgv);
  NTheme::Apply(app);

  g_Driver.Log.setFileName(log);
  if (!g_Driver.Log.open(QIODevice::WriteOnly | QIODevice::Truncate) || !ReadScript(script))
  {
    fprintf(stderr, "z7-gui-driver: cannot read %s or write %s\n", qPrintable(script), qPrintable(log));
    return 2;
  }

  NShim::SetModalHook(OnModal);
  NShim::SetMessageBoxHook(OnMessageBox);
  NShim::SetFileDialogHook(OnFileDialog);

  // a watchdog for scripts that leave a dialog open
  QTimer watchdog;
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, []()
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
  watchdog.start(timeout * 1000);

  char emptyCmdLine[] = "";
  int code = WinMain(NULL, NULL, emptyCmdLine, SW_SHOWNORMAL);
  if (g_Driver.Next < g_Driver.Steps.size())
    g_Driver.Fail(QStringLiteral("the program ended before step %1").arg(g_Driver.Next));
  g_Driver.Write(QJsonObject{ { "event", "exit" }, { "code", code } });
  if (g_Driver.Failed)
    code = 100;
  return code;
}
