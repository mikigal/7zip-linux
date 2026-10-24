// test_packaging.cpp
// Checks the desktop files, the AppStream metadata and the MIME package of
// linux/packaging against 7-Zip and shared-mime-info: every type that 7-Zip
// associates (the System options page) has a MIME type, with the MIME
// package installed, and the .desktop file of 7zFM lists it; the files are
// named by the application ID of platform/AppId.h.

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

#include "Win32Compat.h"

#include "../../../CPP/Common/MyCom.h"
#include "../../../CPP/7zip/UI/Agent/Agent.h"

#include "AppId.h"

static const QString kPackaging = QStringLiteral(Z7_SOURCE_ROOT "/linux/packaging/");

// The value of a key of the [Desktop Entry] group.
static QString DesktopValue(const QString &path, const QString &key)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  bool inEntry = false;
  while (!f.atEnd())
  {
    const QString line = QString::fromUtf8(f.readLine()).trimmed();
    if (line.startsWith(QLatin1Char('[')))
      inEntry = (line == QLatin1String("[Desktop Entry]"));
    else if (inEntry && line.section(QLatin1Char('='), 0, 0) == key)
      return line.section(QLatin1Char('='), 1);
  }
  return QString();
}

class CPackagingTest: public QObject
{
  Q_OBJECT

  QSet<QString> _desktopTypes;
  QSet<QString> _packageTypes;  // types that the MIME package defines

private slots:
  void initTestCase()
  {
    QVERIFY(QFile::exists(kPackaging + Z7_FM_DESKTOP_FILE));
    QVERIFY(QFile::exists(kPackaging + Z7_GUI_APP_ID ".desktop"));
    QVERIFY(QFile::exists(kPackaging + Z7_APP_ID ".xml"));
    for (const QString &t : DesktopValue(kPackaging + Z7_FM_DESKTOP_FILE, "MimeType").split(';', Qt::SkipEmptyParts))
      _desktopTypes.insert(t);
    QFile xml(kPackaging + Z7_APP_ID ".xml");
    QVERIFY(xml.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&xml);
    while (!r.atEnd())
    {
      if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("mime-type"))
      {
        const QString type = r.attributes().value("type").toString();
        // a type of its own has a comment; the others only add globs
        bool hasComment = false;
        while (!(r.readNext() == QXmlStreamReader::EndElement && r.name() == QLatin1String("mime-type")) && !r.atEnd())
          if (r.isStartElement() && r.name() == QLatin1String("comment"))
            hasComment = true;
        if (hasComment)
          _packageTypes.insert(type);
      }
    }
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));
  }

  void desktopFiles()
  {
    QCOMPARE(DesktopValue(kPackaging + Z7_FM_DESKTOP_FILE, "Icon"), QStringLiteral(Z7_APP_ID));
    QCOMPARE(DesktopValue(kPackaging + Z7_GUI_APP_ID ".desktop", "Icon"), QStringLiteral(Z7_APP_ID));
    QCOMPARE(DesktopValue(kPackaging + Z7_FM_DESKTOP_FILE, "Exec"), QStringLiteral("7zFM %f"));
    QCOMPARE(DesktopValue(kPackaging + Z7_GUI_APP_ID ".desktop", "NoDisplay"), QStringLiteral("true"));
    for (const QString &t : _packageTypes)
      QVERIFY2(_desktopTypes.contains(t), qPrintable(t));
  }

  void serviceMenus()
  {
    // the archive actions offer the types that 7zFM opens
    const QString extract = kPackaging + Z7_APP_ID ".servicemenu-extract.desktop";
    const QString compress = kPackaging + Z7_APP_ID ".servicemenu-compress.desktop";
    QCOMPARE(DesktopValue(extract, "MimeType"), DesktopValue(kPackaging + Z7_FM_DESKTOP_FILE, "MimeType"));
    QCOMPARE(DesktopValue(compress, "MimeType"), QStringLiteral("all/all;"));
    // every action runs a command that 7zG knows
    const QStringList known = { "extract", "extracthere", "extractto", "test", "add", "add7z",
        "addzip", "email", "email7z", "emailzip", "hash:SHA256", "hash:CRC32", "hash:*" };
    for (const QString &menu : { extract, compress })
    {
      QFile f(menu);
      QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
      int numExec = 0;
      while (!f.atEnd())
      {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.startsWith(QLatin1String("Exec=")))
          continue;
        numExec++;
        const QStringList parts = line.mid(5).split(' ');
        if (parts[0] == QLatin1String("7zFM"))
          continue;
        QCOMPARE(parts.value(0), QStringLiteral("7zG"));
        QCOMPARE(parts.value(1), QStringLiteral("--context"));
        QVERIFY2(known.contains(parts.value(2)), qPrintable(line));
        QCOMPARE(parts.value(3), QStringLiteral("%F"));
      }
      const int numActions = DesktopValue(menu, "Actions").split(';', Qt::SkipEmptyParts).size();
      QCOMPARE(numExec, numActions);
    }
  }

  void appStreamMetadata()
  {
    // named by the application ID, which it has, with the .desktop file of 7zFM
    QFile f(kPackaging + Z7_APP_ID ".metainfo.xml");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&f);
    QString id, launchable;
    while (!r.atEnd())
    {
      if (r.readNext() != QXmlStreamReader::StartElement)
        continue;
      if (r.name() == QLatin1String("id"))
        id = r.readElementText();
      else if (r.name() == QLatin1String("launchable"))
        launchable = r.readElementText();
    }
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));
    QCOMPARE(id, QStringLiteral(Z7_APP_ID));
    QCOMPARE(launchable, QStringLiteral(Z7_FM_DESKTOP_FILE));
  }

  void kdePlugin()
  {
    // the metadata of the KFileItemActions plugin (linux/kde)
    QFile f(QStringLiteral(Z7_SOURCE_ROOT "/linux/kde/sevenzipfileitemaction.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonParseError error;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll(), &error).object();
    QCOMPARE(error.error, QJsonParseError::NoError);
    const QJsonObject plugin = root.value("KPlugin").toObject();
    QCOMPARE(plugin.value("Icon").toString(), QStringLiteral(Z7_APP_ID));
    QCOMPARE(plugin.value("Name").toString(), QStringLiteral("7-Zip"));
    // files and folders: 7-Zip decides what its menu offers
    QCOMPARE(plugin.value("MimeTypes").toArray(), QJsonArray({ "application/octet-stream", "inode/directory" }));
    QCOMPARE(root.value("MimeType").toString(), QStringLiteral("application/octet-stream;inode/directory;"));
  }

  void everyAssociatedTypeIsListed()
  {
    // the MIME package as an installed one, in a database of its own
    if (QStandardPaths::findExecutable("update-mime-database").isEmpty())
      QSKIP("no update-mime-database");
    QTemporaryDir dir;
    QVERIFY(QDir().mkpath(dir.path() + "/mime/packages"));
    QVERIFY(QFile::copy(kPackaging + Z7_APP_ID ".xml", dir.path() + "/mime/packages/" Z7_APP_ID ".xml"));
    qputenv("XDG_DATA_DIRS", QFile::encodeName(dir.path()) + ":/usr/local/share:/usr/share");
    QCOMPARE(QProcess::execute("update-mime-database", { dir.path() + "/mime" }), 0);
    QMimeDatabase db;

    CMyComPtr<IFolderManager> manager = new CArchiveFolderManager;
    CMyComBSTR exts;
    QCOMPARE(manager->GetExtensions(&exts), S_OK);
    const QStringList list = QString::fromWCharArray(exts).split(' ', Qt::SkipEmptyParts);
    QVERIFY(list.size() > 30);
    for (const QString &ext : list)
    {
      const QMimeType t = db.mimeTypeForFile("a." + ext, QMimeDatabase::MatchExtension);
      QVERIFY2(!t.isDefault(), qPrintable("no MIME type for ." + ext));
      QVERIFY2(_desktopTypes.contains(t.name()), qPrintable("." + ext + ": " + t.name() + " is not in MimeType"));
    }
    for (const QString &t : _desktopTypes)
      QVERIFY2(db.mimeTypeForName(t).isValid(), qPrintable("unknown type " + t));
  }
};

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);
  CPackagingTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_packaging.moc"
