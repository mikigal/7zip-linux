// probe_window.cpp
// z7-probe: a test window for the Wayland scenario tests. It fills itself
// with a known color and reports what it receives on stdout, one event per
// line:
//   platform <name>
//   ready
//   press <x> <y> <button> / release <x> <y> <button>
//   key <text or key code>
//   drag-start / drag-end <action>
//   drop <action> formats=<comma-separated MIME types>
//   drop-url <url>
//
// Options:
//   --color COLOR      fill color (default #3366cc)
//   --title TITLE      window title (default z7-probe)
//   --size WxH         normal window of that size instead of full screen
//   --drag-file PATH   act as a drag source offering PATH as text/uri-list

#include <QApplication>
#include <QCommandLineParser>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <cstdio>

namespace {

void Report(const QString &line)
{
  std::printf("%s\n", line.toUtf8().constData());
  std::fflush(stdout);
}

QString ButtonName(Qt::MouseButton b)
{
  switch (b)
  {
    case Qt::LeftButton: return QStringLiteral("left");
    case Qt::RightButton: return QStringLiteral("right");
    case Qt::MiddleButton: return QStringLiteral("middle");
    default: return QString::number((int)b);
  }
}

QString DropActionName(Qt::DropAction a)
{
  switch (a)
  {
    case Qt::CopyAction: return QStringLiteral("copy");
    case Qt::MoveAction: return QStringLiteral("move");
    case Qt::LinkAction: return QStringLiteral("link");
    default: return QStringLiteral("none");
  }
}

class CProbeWindow: public QWidget
{
public:
  CProbeWindow(const QColor &color, const QString &dragFile): _color(color), _dragFile(dragFile)
  {
    setAcceptDrops(_dragFile.isEmpty());
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter(this).fillRect(rect(), _color);
    if (!_reportedReady)
    {
      _reportedReady = true;
      // Report after the first frame has been committed to the compositor.
      QTimer::singleShot(200, [] { Report(QStringLiteral("ready")); });
    }
  }

  void mousePressEvent(QMouseEvent *e) override
  {
    _pressPos = e->position().toPoint();
    Report(QStringLiteral("press %1 %2 %3").arg(e->position().x()).arg(e->position().y())
        .arg(ButtonName(e->button())));
  }

  void mouseMoveEvent(QMouseEvent *e) override
  {
    if (_dragFile.isEmpty() || !(e->buttons() & Qt::LeftButton))
      return;
    if ((e->position().toPoint() - _pressPos).manhattanLength() < QApplication::startDragDistance())
      return;
    auto *mime = new QMimeData;
    mime->setUrls({ QUrl::fromLocalFile(_dragFile) });
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    Report(QStringLiteral("drag-start"));
    const Qt::DropAction action = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    Report(QStringLiteral("drag-end %1").arg(DropActionName(action)));
  }

  void mouseReleaseEvent(QMouseEvent *e) override
  {
    Report(QStringLiteral("release %1 %2 %3").arg(e->position().x()).arg(e->position().y())
        .arg(ButtonName(e->button())));
  }

  void keyPressEvent(QKeyEvent *e) override
  {
    Report(QStringLiteral("key %1").arg(e->text().isEmpty() ? QString::number(e->key()) : e->text()));
  }

  void dragEnterEvent(QDragEnterEvent *e) override { e->acceptProposedAction(); }

  void dropEvent(QDropEvent *e) override
  {
    Report(QStringLiteral("drop %1 formats=%2").arg(DropActionName(e->dropAction()))
        .arg(e->mimeData()->formats().join(QLatin1Char(','))));
    for (const QUrl &url : e->mimeData()->urls())
      Report(QStringLiteral("drop-url %1").arg(url.toString()));
    if (qEnvironmentVariableIsSet("Z7_PROBE_RAW"))
      for (const QString &format : e->mimeData()->formats())
        Report(QStringLiteral("drop-raw %1 %2").arg(format,
            QString::fromUtf8(e->mimeData()->data(format).toPercentEncoding("/:"))));
    e->acceptProposedAction();
  }

private:
  QColor _color;
  QString _dragFile;
  QPoint _pressPos;
  bool _reportedReady = false;
};

}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QCommandLineParser parser;
  parser.addHelpOption();
  const QCommandLineOption colorOpt(QStringLiteral("color"), QStringLiteral("Fill color."),
      QStringLiteral("color"), QStringLiteral("#3366cc"));
  const QCommandLineOption titleOpt(QStringLiteral("title"), QStringLiteral("Window title."),
      QStringLiteral("title"), QStringLiteral("z7-probe"));
  const QCommandLineOption sizeOpt(QStringLiteral("size"), QStringLiteral("Window size WxH."),
      QStringLiteral("size"));
  const QCommandLineOption dragOpt(QStringLiteral("drag-file"), QStringLiteral("File to offer as a drag source."),
      QStringLiteral("path"));
  parser.addOptions({ colorOpt, titleOpt, sizeOpt, dragOpt });
  parser.process(app);

  Report(QStringLiteral("platform %1").arg(QGuiApplication::platformName()));
  CProbeWindow w(QColor(parser.value(colorOpt)), parser.value(dragOpt));
  w.setWindowTitle(parser.value(titleOpt));
  const QStringList size = parser.value(sizeOpt).split(QLatin1Char('x'));
  if (size.size() == 2)
  {
    w.resize(size.at(0).toInt(), size.at(1).toInt());
    w.show();
  }
  else
    w.showFullScreen();
  return app.exec();
}
