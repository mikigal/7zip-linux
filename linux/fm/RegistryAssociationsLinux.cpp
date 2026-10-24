// RegistryAssociationsLinux.cpp -- NRegistryAssoc (FileManager/
// RegistryAssociations.h) for Linux: the file associations of the System
// options page are the default applications of mimeapps.list.
//
// An extension stands for its MIME type (shared-mime-info). The "current
// user" key (HKEY_CURRENT_USER) is $XDG_CONFIG_HOME/mimeapps.list, the "all
// users" key (HKEY_LOCAL_MACHINE) is mimeapps.list in the first directory
// of $XDG_CONFIG_DIRS (/etc/xdg), which only root can change. Associating
// makes the .desktop file of 7zFM the default application of the type and
// adds it to the type's applications; clearing removes it from both.
// Extensions without a MIME type in shared-mime-info (split volumes .001,
// ...) get "application/x-extension-<ext>", as GIO names unknown types, so
// that the page treats every type alike; such an association works once a
// MIME package defines the type.

#include <QDir>
#include <QFile>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

#include <errno.h>

#include <windows.h>

#include "../../CPP/Common/MyCom.h"
#include "../../CPP/Common/StringConvert.h"

#include "../../CPP/7zip/UI/Agent/Agent.h"
#include "../../CPP/7zip/UI/FileManager/RegistryAssociations.h"

#include "AppId.h"

namespace NRegistryAssoc {

static const char * const k7zipPrefix = "7-Zip.";
static const char * const kDefaultSection = "[Default Applications]";
static const char * const kAddedSection = "[Added Associations]";

static QString MimeAppsPath(HKEY hkey)
{
  if (hkey == HKEY_CURRENT_USER)
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/mimeapps.list");
  QString dirs = qEnvironmentVariable("XDG_CONFIG_DIRS");
  QString first = dirs.section(QLatin1Char(':'), 0, 0);
  if (first.isEmpty())
    first = QStringLiteral("/etc/xdg");
  return first + QStringLiteral("/mimeapps.list");
}

// The MIME type of files with the extension.
static QString ExtMimeType(const CSysString &ext)
{
  const QString e = QString::fromUtf8(ext.Ptr()).toLower();
  const QMimeType t = QMimeDatabase().mimeTypeForFile(QStringLiteral("a.") + e, QMimeDatabase::MatchExtension);
  if (!t.isValid() || t.isDefault())
    return QStringLiteral("application/x-extension-") + e;
  return t.name();
}

// The lines of a mimeapps.list file; other lines are kept as they are.
struct CMimeApps
{
  QStringList Lines;

  bool Load(const QString &path)
  {
    Lines.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
      return !f.exists();
    QTextStream in(&f);
    while (!in.atEnd())
      Lines.append(in.readLine());
    return true;
  }

  bool Save(const QString &path) const
  {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    QTextStream out(&f);
    for (const QString &line : Lines)
      out << line << '\n';
    out.flush();
    return f.commit();
  }

  // the index of the line of the key in the section, or -1;
  // (sectionEnd) is where a new key of the section goes (-1: no section)
  int Find(const char *section, const QString &key, int &sectionEnd) const
  {
    sectionEnd = -1;
    bool in = false;
    for (int i = 0; i < Lines.size(); i++)
    {
      const QString line = Lines[i].trimmed();
      if (line.startsWith(QLatin1Char('[')))
      {
        in = (line == QLatin1String(section));
        if (in)
          sectionEnd = i + 1;
        continue;
      }
      if (!in)
        continue;
      if (!line.isEmpty())
        sectionEnd = i + 1;
      if (line.section(QLatin1Char('='), 0, 0).trimmed() == key)
        return i;
    }
    return -1;
  }

  QStringList Get(const char *section, const QString &key) const
  {
    int end;
    const int i = Find(section, key, end);
    if (i < 0)
      return QStringList();
    return Lines[i].section(QLatin1Char('='), 1).trimmed().split(QLatin1Char(';'), Qt::SkipEmptyParts);
  }

  void Set(const char *section, const QString &key, const QStringList &values)
  {
    int end;
    const int i = Find(section, key, end);
    const QString line = key + QLatin1Char('=') + values.join(QLatin1Char(';')) + QLatin1Char(';');
    if (values.isEmpty())
    {
      if (i >= 0)
        Lines.removeAt(i);
      return;
    }
    if (i >= 0)
      Lines[i] = line;
    else if (end >= 0)
      Lines.insert(end, line);
    else
    {
      if (!Lines.isEmpty() && !Lines.last().trimmed().isEmpty())
        Lines.append(QString());
      Lines.append(QString::fromLatin1(section));
      Lines.append(line);
    }
  }
};

static LONG LastError()
{
  return errno ? (LONG)errno : (LONG)EIO;
}

bool CShellExtInfo::ReadFromRegistry(HKEY hkey, const CSysString &ext)
{
  ProgramKey.Empty();
  IconPath.Empty();
  IconIndex = -1;
  const QString mime = ExtMimeType(ext);
  CMimeApps apps;
  apps.Load(MimeAppsPath(hkey));
  const QStringList defaults = apps.Get(kDefaultSection, mime);
  if (defaults.isEmpty())
    return false;
  if (defaults[0] != QLatin1String(Z7_FM_DESKTOP_FILE))
  {
    ProgramKey = defaults[0].toUtf8().constData();
    return true;
  }
  ProgramKey = k7zipPrefix;
  ProgramKey += ext;
  // the icon that the System page expects for 7-Zip (FilePlugins.cpp)
  CMyComPtr<IFolderManager> manager = new CArchiveFolderManager;
  CMyComBSTR iconPath;
  Int32 iconIndex = 0;
  if (manager->GetIconPath(GetUnicodeString(ext), &iconPath, &iconIndex) == S_OK && iconPath)
  {
    IconPath = (const wchar_t *)iconPath;
    IconIndex = (int)iconIndex;
  }
  return true;
}

bool CShellExtInfo::IsIt7Zip() const
{
  return ProgramKey.IsPrefixedBy_Ascii_NoCase(k7zipPrefix);
}

LONG DeleteShellExtensionInfo(HKEY hkey, const CSysString &ext)
{
  const QString mime = ExtMimeType(ext);
  const QString path = MimeAppsPath(hkey);
  CMimeApps apps;
  if (!apps.Load(path))
    return LastError();
  const QString id = QStringLiteral(Z7_FM_DESKTOP_FILE);
  QStringList defaults = apps.Get(kDefaultSection, mime);
  QStringList added = apps.Get(kAddedSection, mime);
  if (!defaults.contains(id) && !added.contains(id))
    return 0;
  defaults.removeAll(id);
  added.removeAll(id);
  apps.Set(kDefaultSection, mime, defaults);
  apps.Set(kAddedSection, mime, added);
  return apps.Save(path) ? 0 : LastError();
}

LONG AddShellExtensionInfo(HKEY hkey,
    const CSysString &ext,
    const UString & /* programTitle */,
    const UString & /* programOpenCommand */,
    const UString & /* iconPath */, int /* iconIndex */)
{
  const QString mime = ExtMimeType(ext);
  const QString path = MimeAppsPath(hkey);
  CMimeApps apps;
  if (!apps.Load(path))
    return LastError();
  const QString id = QStringLiteral(Z7_FM_DESKTOP_FILE);
  QStringList defaults = apps.Get(kDefaultSection, mime);
  QStringList added = apps.Get(kAddedSection, mime);
  defaults.removeAll(id);
  defaults.prepend(id);
  added.removeAll(id);
  added.prepend(id);
  apps.Set(kDefaultSection, mime, defaults);
  apps.Set(kAddedSection, mime, added);
  return apps.Save(path) ? 0 : LastError();
}

}
