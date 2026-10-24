// ShimShellFolder.cpp -- the part of the shell namespace that the file
// manager uses for file system items: SHGetDesktopFolder(), item ID lists
// that hold paths, and the context menu of files (IContextMenu), which is
// the "System" submenu of 7zFM and runs its Properties command. Its verbs:
//   "open"        opens the items with the desktop's applications;
//   "properties"  shows the Properties dialog of the desktop's file manager
//                 (org.freedesktop.FileManager1.ShowItemProperties), or a
//                 message box with the basic properties where none runs.

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QPointer>
#include <QUrl>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <windows.h>
#include <ShlObj.h>

#include "../../CPP/Common/MyCom.h"

#include "ShimDialog.h"
#include "ShimWindow.h"

namespace NShim {

// ---------- Item ID lists ----------

/* An item ID list here is one SHITEMID that holds the absolute path
   (UTF-8 bytes of the file name, ending with 0), and the terminating
   empty SHITEMID. */
static LPITEMIDLIST CreatePidl(const QString &path)
{
  const QByteArray bytes = QFile::encodeName(path);
  const size_t idSize = sizeof(WORD) + (size_t)bytes.size() + 1;
  if (idSize > 0xFFFF)
    return NULL;
  Byte *p = (Byte *)CoTaskMemAlloc(idSize + sizeof(WORD));
  if (!p)
    return NULL;
  const WORD cb = (WORD)idSize;
  memcpy(p, &cb, sizeof(cb));
  memcpy(p + sizeof(WORD), bytes.constData(), (size_t)bytes.size() + 1);
  memset(p + idSize, 0, sizeof(WORD));
  return (LPITEMIDLIST)(void *)p;
}

static QString PidlPath(const ITEMIDLIST *pidl)
{
  if (!pidl)
    return QString();
  const Byte *p = (const Byte *)(const void *)pidl;
  WORD cb;
  memcpy(&cb, p, sizeof(cb));
  if (cb <= sizeof(WORD))
    return QString();
  return QFile::decodeName(QByteArray((const char *)p + sizeof(WORD)));
}

static bool Exists(const QString &path)
{
  struct stat st;
  return lstat(QFile::encodeName(path).constData(), &st) == 0;
}


// ---------- The context menu of files ----------

struct CVerb
{
  const char *Name;
  const char *Text;
  const char *HelpText;
};

static const CVerb kVerbs[] =
{
  { "open", "&Open", "Opens the selected items with their applications." },
  { "properties", "P&roperties", "Shows the properties of the selected items." }
};

static const unsigned kVerb_Open = 0;
static const unsigned kVerb_Properties = 1;

static QString SizeText(qint64 size)
{
  return QLocale::system().toString(size) + QStringLiteral(" bytes");
}

static QString PermissionsText(mode_t mode)
{
  QString s;
  s += S_ISDIR(mode) ? QLatin1Char('d') : S_ISLNK(mode) ? QLatin1Char('l') : QLatin1Char('-');
  const char *rwx = "rwxrwxrwx";
  for (int i = 0; i < 9; i++)
    s += (mode & (0400 >> i)) ? QLatin1Char(rwx[i]) : QLatin1Char('-');
  return s;
}

// The text of the message box that stands in for the file manager's dialog.
static QString PropertiesText(const QStringList &paths)
{
  const QLocale locale = QLocale::system();
  if (paths.size() != 1)
  {
    qint64 size = 0;
    int numFiles = 0, numFolders = 0;
    for (const QString &path : paths)
    {
      const QFileInfo fi(path);
      if (fi.isDir() && !fi.isSymLink())
        numFolders++;
      else
      {
        numFiles++;
        size += fi.size();
      }
    }
    return QStringLiteral("Folders: %1\nFiles: %2\nSize of the files: %3\nLocation: %4")
        .arg(numFolders).arg(numFiles).arg(SizeText(size), QFileInfo(paths[0]).absolutePath());
  }
  const QString &path = paths[0];
  const QFileInfo fi(path);
  QString s = QStringLiteral("Name: %1\nLocation: %2\n").arg(fi.fileName(), fi.absolutePath());
  struct stat st;
  memset(&st, 0, sizeof(st));
  lstat(QFile::encodeName(path).constData(), &st);
  if (fi.isSymLink())
    s += QStringLiteral("Link target: %1\n").arg(fi.symLinkTarget());
  if (fi.isDir())
    s += QStringLiteral("Type: Folder\n");
  else
  {
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(fi);
    s += QStringLiteral("Type: %1 (%2)\n").arg(mime.comment(), mime.name());
    s += QStringLiteral("Size: %1\n").arg(SizeText(fi.size()));
  }
  s += QStringLiteral("Modified: %1\n").arg(locale.toString(fi.lastModified(), QLocale::LongFormat));
  s += QStringLiteral("Permissions: %1\n").arg(PermissionsText(st.st_mode));
  s += QStringLiteral("Owner: %1\nGroup: %2").arg(fi.owner(), fi.group());
  return s;
}

static void ShowPropertiesBox(HWND owner, const QStringList &paths)
{
  MessageBoxW(owner, ToU(PropertiesText(paths)), L"Properties", MB_OK | MB_ICONINFORMATION);
}

/* Asks the desktop's file manager for its Properties dialog; the message
   box is shown when there is no session bus or the call fails. */
static void ShowProperties(HWND owner, const QStringList &paths)
{
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected())
  {
    ShowPropertiesBox(owner, paths);
    return;
  }
  QStringList uris;
  for (const QString &path : paths)
    uris.append(QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded));
  QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
      QStringLiteral("/org/freedesktop/FileManager1"), QStringLiteral("org.freedesktop.FileManager1"),
      QStringLiteral("ShowItemProperties"));
  call << uris << QString();
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), qApp);
  QPointer<QWidget> ownerWidget = WidgetOf(owner);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, [=](QDBusPendingCallWatcher *w)
  {
    w->deleteLater();
    if (!w->isError())
      return;
    // the owner may be gone by now
    HWND o = ownerWidget ? FromWidget(ownerWidget->window()) : NULL;
    ShowPropertiesBox(o, paths);
  });
}

class CFileContextMenu Z7_final:
  public IContextMenu,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1(IContextMenu)

  STDMETHOD(QueryContextMenu)(HMENU menu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT flags) Z7_override;
  STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO ci) Z7_override;
  STDMETHOD(GetCommandString)(UINT_PTR idCmd, UINT type, UINT *reserved, LPSTR name, UINT cchMax) Z7_override;

  QStringList _paths;
  HWND _owner;
public:
  CFileContextMenu(const QStringList &paths, HWND owner): _paths(paths), _owner(owner) {}
};

STDMETHODIMP CFileContextMenu::QueryContextMenu(HMENU menu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT flags)
{
  if (idCmdLast < idCmdFirst + Z7_ARRAY_SIZE(kVerbs) - 1)
    return E_INVALIDARG;
  UINT pos = indexMenu;
  InsertMenuW(menu, pos++, MF_BYPOSITION | MF_STRING, idCmdFirst + kVerb_Open, ToU(QString::fromLatin1(kVerbs[kVerb_Open].Text)));
  if (!(flags & CMF_NODEFAULT))
    SetMenuDefaultItem(menu, idCmdFirst + kVerb_Open, FALSE);
  if (!(flags & CMF_DEFAULTONLY))
  {
    InsertMenuW(menu, pos++, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(menu, pos++, MF_BYPOSITION | MF_STRING, idCmdFirst + kVerb_Properties,
        ToU(QString::fromLatin1(kVerbs[kVerb_Properties].Text)));
  }
  // MAKE_HRESULT(SEVERITY_SUCCESS, 0, the largest offset used + 1)
  return (HRESULT)Z7_ARRAY_SIZE(kVerbs);
}

STDMETHODIMP CFileContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO ci)
{
  if (!ci)
    return E_INVALIDARG;
  unsigned verb = Z7_ARRAY_SIZE(kVerbs);
  if (IS_INTRESOURCE(ci->lpVerb))
    verb = LOWORD((UINT_PTR)ci->lpVerb);
  else
  {
    for (unsigned i = 0; i < Z7_ARRAY_SIZE(kVerbs); i++)
      if (strcmp(ci->lpVerb, kVerbs[i].Name) == 0)
        verb = i;
  }
  const HWND owner = ci->hwnd ? ci->hwnd : _owner;
  switch (verb)
  {
    case kVerb_Open:
      for (const QString &path : _paths)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
      return S_OK;
    case kVerb_Properties:
      ShowProperties(owner, _paths);
      return S_OK;
    default:
      return E_INVALIDARG;
  }
}

STDMETHODIMP CFileContextMenu::GetCommandString(UINT_PTR idCmd, UINT type, UINT * /* reserved */, LPSTR name, UINT cchMax)
{
  if (idCmd >= Z7_ARRAY_SIZE(kVerbs))
    return E_INVALIDARG;
  const CVerb &v = kVerbs[idCmd];
  const char *s;
  switch (type & ~(UINT)GCS_UNICODE)
  {
    case GCS_VERBA: s = v.Name; break;
    case GCS_HELPTEXTA: s = v.HelpText; break;
    case GCS_VALIDATEA: return S_OK;
    default: return E_INVALIDARG;
  }
  if (!name || cchMax == 0)
    return E_INVALIDARG;
  if (type & GCS_UNICODE)
  {
    wchar_t *w = (wchar_t *)(void *)name;
    UINT i = 0;
    for (; s[i] != 0 && i + 1 < cchMax; i++)
      w[i] = (wchar_t)(Byte)s[i];
    w[i] = 0;
  }
  else
  {
    strncpy(name, s, cchMax - 1);
    name[cchMax - 1] = 0;
  }
  return S_OK;
}


// ---------- Folders ----------

/* The desktop (empty path: names are absolute paths) or a file system
   folder. */
class CShellFolder Z7_final:
  public IShellFolder,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1(IShellFolder)

  STDMETHOD(ParseDisplayName)(HWND hwnd, IBindCtx *pbc, LPWSTR displayName, ULONG *eaten,
      LPITEMIDLIST *pidl, ULONG *attributes) Z7_override;
  STDMETHOD(EnumObjects)(HWND hwnd, SHCONTF flags, IEnumIDList **list) Z7_override;
  STDMETHOD(BindToObject)(LPCITEMIDLIST pidl, IBindCtx *pbc, REFIID riid, void **ppv) Z7_override;
  STDMETHOD(BindToStorage)(LPCITEMIDLIST pidl, IBindCtx *pbc, REFIID riid, void **ppv) Z7_override;
  STDMETHOD(CompareIDs)(LPARAM lParam, LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2) Z7_override;
  STDMETHOD(CreateViewObject)(HWND owner, REFIID riid, void **ppv) Z7_override;
  STDMETHOD(GetAttributesOf)(UINT cidl, LPCITEMIDLIST *apidl, SFGAOF *inOut) Z7_override;
  STDMETHOD(GetUIObjectOf)(HWND owner, UINT cidl, LPCITEMIDLIST *apidl, REFIID riid,
      UINT *reserved, void **ppv) Z7_override;
  STDMETHOD(GetDisplayNameOf)(LPCITEMIDLIST pidl, SHGDNF flags, STRRET *name) Z7_override;
  STDMETHOD(SetNameOf)(HWND hwnd, LPCITEMIDLIST pidl, LPCWSTR name, SHGDNF flags,
      LPITEMIDLIST *pidlOut) Z7_override;

  QString _path;
public:
  CShellFolder(const QString &path): _path(path) {}
};

STDMETHODIMP CShellFolder::ParseDisplayName(HWND /* hwnd */, IBindCtx * /* pbc */, LPWSTR displayName,
    ULONG *eaten, LPITEMIDLIST *pidl, ULONG *attributes)
{
  if (!pidl)
    return E_INVALIDARG;
  *pidl = NULL;
  if (!displayName || !*displayName)
    return E_INVALIDARG;
  QString name = FromW(displayName);
  if (eaten)
    *eaten = (ULONG)wcslen(displayName);
  if (name == QLatin1String(".") || name == QLatin1String(".."))
    return E_INVALIDARG;
  QString path;
  if (name.startsWith(QLatin1Char('/')))
    path = name;
  else if (_path.isEmpty())
    return E_INVALIDARG;
  else
    path = _path + QLatin1Char('/') + name;
  while (path.size() > 1 && path.endsWith(QLatin1Char('/')))
    path.chop(1);
  if (!Exists(path))
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  if (attributes)
  {
    ULONG a = SFGAO_FILESYSTEM;
    if (QFileInfo(path).isDir())
      a |= SFGAO_FOLDER;
    *attributes &= a;
  }
  *pidl = CreatePidl(path);
  return *pidl ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CShellFolder::EnumObjects(HWND, SHCONTF, IEnumIDList **list)
{
  if (list)
    *list = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP CShellFolder::BindToObject(LPCITEMIDLIST pidl, IBindCtx * /* pbc */, REFIID riid, void **ppv)
{
  if (!ppv)
    return E_INVALIDARG;
  *ppv = NULL;
  if (riid != IID_IShellFolder)
    return E_NOINTERFACE;
  const QString path = PidlPath(pidl);
  if (path.isEmpty() || !QFileInfo(path).isDir())
    return E_INVALIDARG;
  CMyComPtr<IShellFolder> folder = new CShellFolder(path);
  *ppv = folder.Detach();
  return S_OK;
}

STDMETHODIMP CShellFolder::BindToStorage(LPCITEMIDLIST, IBindCtx *, REFIID, void **ppv)
{
  if (ppv)
    *ppv = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP CShellFolder::CompareIDs(LPARAM, LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
  const int c = PidlPath(pidl1).compare(PidlPath(pidl2));
  // MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)(short)result)
  return (HRESULT)(UInt16)(Int16)(c < 0 ? -1 : c > 0 ? 1 : 0);
}

STDMETHODIMP CShellFolder::CreateViewObject(HWND, REFIID, void **ppv)
{
  if (ppv)
    *ppv = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP CShellFolder::GetAttributesOf(UINT cidl, LPCITEMIDLIST *apidl, SFGAOF *inOut)
{
  if (!inOut)
    return E_INVALIDARG;
  SFGAOF a = SFGAO_FILESYSTEM | SFGAO_FOLDER;
  for (UINT i = 0; i < cidl; i++)
    if (!QFileInfo(PidlPath(apidl[i])).isDir())
      a &= ~(SFGAOF)SFGAO_FOLDER;
  *inOut &= a;
  return S_OK;
}

STDMETHODIMP CShellFolder::GetUIObjectOf(HWND owner, UINT cidl, LPCITEMIDLIST *apidl, REFIID riid,
    UINT * /* reserved */, void **ppv)
{
  if (!ppv)
    return E_INVALIDARG;
  *ppv = NULL;
  if (riid != IID_IContextMenu)
    return E_NOINTERFACE;
  if (cidl == 0 || !apidl)
    return E_INVALIDARG;
  QStringList paths;
  for (UINT i = 0; i < cidl; i++)
  {
    const QString path = PidlPath(apidl[i]);
    if (path.isEmpty())
      return E_INVALIDARG;
    paths.append(path);
  }
  CMyComPtr<IContextMenu> menu = new CFileContextMenu(paths, owner);
  *ppv = menu.Detach();
  return S_OK;
}

STDMETHODIMP CShellFolder::GetDisplayNameOf(LPCITEMIDLIST pidl, SHGDNF flags, STRRET *name)
{
  if (!name)
    return E_INVALIDARG;
  const QString path = PidlPath(pidl);
  if (path.isEmpty())
    return E_INVALIDARG;
  const QString s = (flags & SHGDN_FORPARSING) ? path : QFileInfo(path).fileName();
  const UString u = ToU(s);
  const size_t size = (u.Len() + 1) * sizeof(wchar_t);
  name->uType = 0; // STRRET_WSTR
  name->pOleStr = (LPWSTR)CoTaskMemAlloc(size);
  if (!name->pOleStr)
    return E_OUTOFMEMORY;
  memcpy(name->pOleStr, u.Ptr(), size);
  return S_OK;
}

STDMETHODIMP CShellFolder::SetNameOf(HWND, LPCITEMIDLIST, LPCWSTR, SHGDNF, LPITEMIDLIST *pidlOut)
{
  if (pidlOut)
    *pidlOut = NULL;
  return E_NOTIMPL;
}

}

using namespace NShim;

HRESULT SHGetDesktopFolder(IShellFolder **folder)
{
  if (!folder)
    return E_INVALIDARG;
  CMyComPtr<IShellFolder> desktop = new CShellFolder(QString());
  *folder = desktop.Detach();
  return S_OK;
}
