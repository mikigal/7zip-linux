// DriverCore.h -- the script engine of the test drivers of 7zG
// (gui_driver.cpp) and 7zFM (fm_driver.cpp): the steps that answer dialogs,
// message boxes and file dialogs, and the JSON log.

#ifndef ZIP7_INC_LINUX_TESTS_DRIVER_CORE_H
#define ZIP7_INC_LINUX_TESTS_DRIVER_CORE_H

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

#include <stdio.h>

#include <windows.h>

#include "ShimDialog.h"

namespace NDriver {

struct CDriver
{
  QString Name;
  QJsonArray Steps;
  int Next = 0;
  QSet<QString> Passive;
  QString ShotsDir;
  QString LastOpenedUrl;  // the last URL that the program opened
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
    fprintf(stderr, "%s: %s\n", qPrintable(Name), qPrintable(message));
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

extern CDriver g_Driver;

// Logs the URLs that the program opens (help topics) instead of opening them.
class CUrlLogger: public QObject
{
  Q_OBJECT
public:
  CUrlLogger(QObject *parent): QObject(parent) {}
public slots:
  void openUrl(const QUrl &url)
  {
    g_Driver.LastOpenedUrl = url.toString();
    g_Driver.Write(QJsonObject{ { "event", "open_url" }, { "url", url.toString() } });
  }
};

struct CArgs
{
  QString Script;
  QString Log;
  int Timeout = 60;
  int First = 0;      // the first program argument (after --)
};

QString WindowText(HWND wnd);
QString DialogName(HWND dlg);
QJsonObject ControlState(HWND c);
QJsonObject DialogState(const char *event, HWND dlg);

// Applies a dialog action; false when it must be tried again later.
bool Apply(HWND dlg, const QJsonObject &a);

void OnModal(HWND dlg);
int OnMessageBox(HWND owner, const QString &text, const QString &caption, UINT type);
bool OnFileDialog(NShim::CFileDialogRequest &r);
bool ReadScript(const QString &path);

// --script FILE --log FILE [--shots DIR] [--timeout SEC] -- ARGS
bool ParseArgs(int argc, char *argv[], CArgs &args);
// GetCommandLineW() and the program directory for the program under test.
void SetProgramCommandLine(int argc, char *argv[], const CArgs &args);
// After the application object exists: opens the log, reads the script,
// installs the hooks and the watchdog.
bool Start(const CArgs &args, const char *name);
// Logs the exit; returns the exit code (100 if the script failed).
int Finish(int code);

}

#endif
