// ShimShell.cpp -- the shell side of the GUI code: file icons and the
// system image lists (SHGetFileInfo), special folders, help topics, file
// operations (the trash), opening documents with their applications, the
// folder browser and file dialogs of Windows/Shell.h and
// Windows/CommonDialog.h, and the text clipboard of Windows/Clipboard.h.

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QMimeData>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>

#include <windows.h>
#include <CommCtrl.h>
#include <HtmlHelp.h>
#include <ShlObj.h>

#include "../../CPP/Common/CommandLineParser.h"

#include "../../CPP/Windows/Clipboard.h"
#include "../../CPP/Windows/CommonDialog.h"
#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/FileFind.h"
#include "../../CPP/Windows/Shell.h"

#include "ShimDialog.h"
#include "ShimResources.h"
#include "ShimWindow.h"

namespace NShim {

// ---------- File icons ----------

struct CFileType
{
  QString Key;        // identifies the icon in the system image lists
  QIcon Icon;
  QString TypeName;
};

static QIcon ThemeIcon(const QString &name, const QString &generic, QStyle::StandardPixmap fallback)
{
  QIcon icon = QIcon::fromTheme(name);
  if (icon.isNull() && !generic.isEmpty())
    icon = QIcon::fromTheme(generic);
  if (icon.isNull())
    icon = QApplication::style()->standardIcon(fallback);
  return icon;
}

// The type that index 0 of the system image lists shows, as Windows shows
// a default icon for unknown types.
static CFileType UnknownFileType()
{
  CFileType t;
  t.Key = QStringLiteral("application-octet-stream");
  t.Icon = ThemeIcon(t.Key, QStringLiteral("unknown"), QStyle::SP_FileIcon);
  t.TypeName = QMimeDatabase().mimeTypeForName(QStringLiteral("application/octet-stream")).comment();
  return t;
}

static CFileType FolderType(bool open)
{
  CFileType t;
  t.Key = open ? QStringLiteral("folder-open") : QStringLiteral("folder");
  t.Icon = ThemeIcon(t.Key, QStringLiteral("folder"), open ? QStyle::SP_DirOpenIcon : QStyle::SP_DirIcon);
  t.TypeName = QMimeDatabase().mimeTypeForName(QStringLiteral("inode/directory")).comment();
  return t;
}

// The ITEMIDLIST of a special folder: one item that holds the CSIDL value.
struct CSpecialFolderId
{
  WORD Cb;
  Int32 Csidl;
  WORD End;
};

static CFileType SpecialFolderType(int csidl)
{
  CFileType t;
  switch (csidl)
  {
    case CSIDL_DRIVES:
      t.Key = QStringLiteral("computer");
      t.Icon = ThemeIcon(t.Key, QString(), QStyle::SP_ComputerIcon);
      break;
    case CSIDL_NETWORK:
      t.Key = QStringLiteral("network-workgroup");
      t.Icon = ThemeIcon(t.Key, QStringLiteral("folder-network"), QStyle::SP_DriveNetIcon);
      break;
    case CSIDL_PERSONAL:
    case CSIDL_PROFILE:
      t.Key = QStringLiteral("user-home");
      t.Icon = ThemeIcon(t.Key, QStringLiteral("folder"), QStyle::SP_DirHomeIcon);
      break;
    case CSIDL_DESKTOP:
      t.Key = QStringLiteral("user-desktop");
      t.Icon = ThemeIcon(t.Key, QStringLiteral("folder"), QStyle::SP_DesktopIcon);
      break;
    default:
      return FolderType(false);
  }
  t.TypeName = QStringLiteral("System Folder");
  return t;
}

/* The type of a file or folder. With SHGFI_USEFILEATTRIBUTES the file is not
   read: the name gives the type (the GUI code passes names such as ".7z" or
   "__file__") and attrib tells whether it is a folder. Otherwise the type
   comes from the file itself. */
static CFileType FileType(const QString &path, DWORD attrib, UINT flags)
{
  const bool useAttrib = (flags & SHGFI_USEFILEATTRIBUTES) != 0;
  bool isDir;
  if (useAttrib)
    isDir = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
  else
    isDir = QFileInfo(path).isDir();
  if (isDir)
    return FolderType((flags & SHGFI_OPENICON) != 0);

  QMimeDatabase db;
  const QMimeType mime = useAttrib ?
      db.mimeTypeForFile(path, QMimeDatabase::MatchExtension) :
      db.mimeTypeForFile(path);
  if (!mime.isValid() || mime.isDefault())
    return UnknownFileType();
  CFileType t;
  t.Key = mime.iconName();
  t.Icon = ThemeIcon(mime.iconName(), mime.genericIconName(), QStyle::SP_FileIcon);
  t.TypeName = mime.comment();
  return t;
}

struct CSystemImageList
{
  HIMAGELIST List;
  QHash<QString, int> Indexes;   // CFileType::Key -> index
};

static CSystemImageList *g_SystemImageLists[2];

static void FreeSystemImageLists()
{
  for (CSystemImageList *&l : g_SystemImageLists)
  {
    if (!l)
      continue;
    delete l->List;
    delete l;
    l = NULL;
  }
}

static CSystemImageList &SystemList(bool smallIcons)
{
  CSystemImageList *&l = g_SystemImageLists[smallIcons ? 0 : 1];
  if (!l)
  {
    if (!g_SystemImageLists[0] && !g_SystemImageLists[1])
      qAddPostRoutine(FreeSystemImageLists);
    const int size = smallIcons ? 16 : 32;
    l = new CSystemImageList;
    l->List = ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, 0, 0);
    l->List->System = true;
    const CFileType unknown = UnknownFileType();
    l->List->Icons.append(unknown.Icon);
    l->Indexes.insert(unknown.Key, 0);
  }
  return *l;
}

HIMAGELIST SystemImageList(bool smallIcons)
{
  return SystemList(smallIcons).List;
}

static int SystemIconIndex(bool smallIcons, const CFileType &t)
{
  CSystemImageList &l = SystemList(smallIcons);
  const auto it = l.Indexes.constFind(t.Key);
  if (it != l.Indexes.constEnd())
    return *it;
  const int index = (int)l.List->Icons.size();
  l.List->Icons.append(t.Icon);
  l.Indexes.insert(t.Key, index);
  return index;
}

struct CFileInfoResult
{
  DWORD_PTR Result;
  HICON Icon;
  int IconIndex;
  QString DisplayName;
  QString TypeName;
};

static CFileInfoResult GetFileInfoImpl(const QString &path, const CSpecialFolderId *pidl,
    DWORD attrib, UINT flags)
{
  CFileInfoResult r;
  r.Result = 1;
  r.Icon = NULL;
  r.IconIndex = 0;
  const CFileType t = pidl ? SpecialFolderType(pidl->Csidl) : FileType(path, attrib, flags);
  const bool smallIcons = (flags & SHGFI_SMALLICON) != 0;
  if (flags & SHGFI_SYSICONINDEX)
  {
    r.IconIndex = SystemIconIndex(smallIcons, t);
    r.Result = (DWORD_PTR)SystemImageList(smallIcons);
  }
  if (flags & SHGFI_ICON)
    r.Icon = NewIconHandle(t.Icon);
  if (flags & SHGFI_DISPLAYNAME)
    r.DisplayName = pidl ? t.TypeName : QFileInfo(path).fileName();
  if (flags & SHGFI_TYPENAME)
    r.TypeName = t.TypeName;
  return r;
}

template <class T, class C>
static DWORD_PTR GetFileInfo(const QString &path, const CSpecialFolderId *pidl,
    DWORD attrib, T *info, UINT flags,
    int (*copy)(const QString &, C *, int))
{
  const CFileInfoResult r = OnGuiThread<CFileInfoResult>([&]()
  {
    return GetFileInfoImpl(path, pidl, attrib, flags);
  });
  if (info)
  {
    if (flags & (SHGFI_SYSICONINDEX | SHGFI_ICON))
      info->iIcon = r.IconIndex;
    if (flags & SHGFI_ICON)
      info->hIcon = r.Icon;
    if (flags & SHGFI_ATTRIBUTES)
      info->dwAttributes = 0;
    if (flags & SHGFI_DISPLAYNAME)
      copy(r.DisplayName, info->szDisplayName, (int)Z7_ARRAY_SIZE(info->szDisplayName));
    if (flags & SHGFI_TYPENAME)
      copy(r.TypeName, info->szTypeName, (int)Z7_ARRAY_SIZE(info->szTypeName));
  }
  else if (r.Icon)
    DestroyIcon(r.Icon);
  return r.Result;
}

// ---------- File dialogs ----------

static CFileDialogHook g_FileDialogHook;
static DWORD g_CommDlgError;

void SetFileDialogHook(const CFileDialogHook &hook)
{
  g_FileDialogHook = hook;
}

static bool RunFileDialog(CFileDialogRequest &r)
{
  if (g_FileDialogHook)
    return g_FileDialogHook(r);

  QWidget *parent = NULL;
  HWND owner = FromHandle(r.Owner);
  if (owner && owner->Widget)
    parent = owner->Widget->window();

  QFileDialog dialog(parent, r.Title);
  switch (r.Kind)
  {
    case CFileDialogRequest::kFolder:
      dialog.setFileMode(QFileDialog::Directory);
      dialog.setOption(QFileDialog::ShowDirsOnly);
      break;
    case CFileDialogRequest::kOpenFile:
      dialog.setFileMode(QFileDialog::ExistingFile);
      dialog.setAcceptMode(QFileDialog::AcceptOpen);
      break;
    case CFileDialogRequest::kSaveFile:
      dialog.setFileMode(QFileDialog::AnyFile);
      dialog.setAcceptMode(QFileDialog::AcceptSave);
      // as GetSaveFileName() without OFN_OVERWRITEPROMPT: the caller asks
      dialog.setOption(QFileDialog::DontConfirmOverwrite);
      break;
  }
  if (!r.Filters.isEmpty())
  {
    dialog.setNameFilters(r.Filters);
    if (r.FilterIndex >= 0 && r.FilterIndex < r.Filters.size())
      dialog.selectNameFilter(r.Filters[r.FilterIndex]);
  }
  if (!r.Path.isEmpty())
  {
    const QFileInfo fi(r.Path);
    if (r.Kind == CFileDialogRequest::kFolder || fi.isDir())
      dialog.setDirectory(r.Path);
    else
    {
      dialog.setDirectory(fi.absolutePath());
      dialog.selectFile(fi.fileName());
    }
  }
  QPointer<QWidget> ownerWindow = parent;
  const int code = dialog.exec();
  ReactivateOwner(ownerWindow);
  if (code != QDialog::Accepted)
    return false;
  const QStringList files = dialog.selectedFiles();
  if (files.isEmpty())
    return false;
  r.Path = files[0];
  r.FilterIndex = r.Filters.indexOf(dialog.selectedNameFilter());
  return true;
}

static bool FileDialog(CFileDialogRequest &r)
{
  g_CommDlgError = 0;
  return OnGuiThread<bool>([&]() { return RunFileDialog(r); });
}

// A Qt name filter from a Win32 filter: "7z" and "*.7z;*.7Z" give
// "7z (*.7z *.7Z)"; "*.*" matches every file, as on Windows.
static QString QtNameFilter(const QString &description, const QString &masks)
{
  QStringList patterns = masks.split(QLatin1Char(';'), Qt::SkipEmptyParts);
  for (QString &p : patterns)
  {
    p = p.trimmed();
    if (p == QLatin1String("*.*"))
      p = QStringLiteral("*");
  }
  return description + QStringLiteral(" (") + patterns.join(QLatin1Char(' ')) + QLatin1Char(')');
}

// ---------- Help ----------

// The online copy of the 7-Zip help (7-zip.chm).
static const char * const kOnlineHelp = "https://7-zip.opensource.jp/chm/";

static void ShowHelpTopic(const QString &file)
{
  // "<dir>/7-zip.chm::/fm/plugins/7-zip/extract.htm#anchor"
  const int sep = file.indexOf(QLatin1String("::/"));
  const QString chm = sep >= 0 ? file.left(sep) : file;
  QString topic = sep >= 0 ? file.mid(sep + 3) : QString();
  QString anchor;
  const int hash = topic.indexOf(QLatin1Char('#'));
  if (hash >= 0)
  {
    anchor = topic.mid(hash + 1);
    topic.truncate(hash);
  }
  // the help file is case-insensitive, and its pages have lowercase names
  topic = topic.toLower();
  // a local copy of the pages: the help/ directory next to the help file
  const QString local = QFileInfo(chm).absoluteDir().filePath(QStringLiteral("help/") + topic);
  QUrl url = QFileInfo(local).isFile() ?
      QUrl::fromLocalFile(local) :
      QUrl(QString::fromLatin1(kOnlineHelp) + topic);
  if (!anchor.isEmpty())
    url.setFragment(anchor);
  QDesktopServices::openUrl(url);
}

}

using namespace NShim;

// ---------- shell32 ----------

static const CSpecialFolderId *SpecialFolderOf(const void *path, UINT flags)
{
  return (flags & SHGFI_PIDL) ? (const CSpecialFolderId *)path : NULL;
}

DWORD_PTR SHGetFileInfoW(LPCWSTR path, DWORD attrib, SHFILEINFOW *info, UINT /* infoSize */, UINT flags)
{
  const CSpecialFolderId *pidl = SpecialFolderOf(path, flags);
  return GetFileInfo(pidl ? QString() : FromW(path), pidl, attrib, info, flags, CopyToW);
}

DWORD_PTR SHGetFileInfoA(LPCSTR path, DWORD attrib, SHFILEINFOA *info, UINT /* infoSize */, UINT flags)
{
  const CSpecialFolderId *pidl = SpecialFolderOf(path, flags);
  return GetFileInfo(pidl ? QString() : FromA(path), pidl, attrib, info, flags, CopyToA);
}

// The path of a special folder ITEMIDLIST (SHGetSpecialFolderLocation()).
static QString SpecialFolderPath(int csidl)
{
  switch (csidl)
  {
    case CSIDL_PERSONAL:
    case CSIDL_PROFILE:
      return QDir::homePath();
    case CSIDL_DESKTOP:
      return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    default:
      return QString();
  }
}

HRESULT SHGetDesktopFolder(IShellFolder **folder)
{
  // there is no shell namespace on Linux
  if (folder)
    *folder = NULL;
  return E_NOTIMPL;
}

// pFrom holds zero-terminated paths, ended by an empty one.
template <class TChar>
static QStringList PathList(const TChar *s, QString (*conv)(const TChar *))
{
  QStringList list;
  if (!s)
    return list;
  while (*s)
  {
    list.append(conv(s));
    while (*s)
      s++;
    s++;
  }
  return list;
}

static QString ConvW(const WCHAR *s) { return FromW(s); }
static QString ConvA(const CHAR *s) { return FromA(s); }

/* FO_DELETE: with FOF_ALLOWUNDO the files go to the freedesktop trash
   (QFile::moveToTrash), else they are deleted. Returns 0 or an errno code;
   other operations are not supported (ENOTSUP). */
static int FileOperation(UINT func, FILEOP_FLAGS flags, const QStringList &paths, BOOL &aborted)
{
  aborted = FALSE;
  if (func != FO_DELETE)
    return ENOTSUP;
  int result = 0;
  for (const QString &path : paths)
  {
    bool ok;
    if (flags & FOF_ALLOWUNDO)
      ok = QFile::moveToTrash(path);
    else
    {
      const FString fs = us2fs(ToU(path));
      NWindows::NFile::NFind::CFileInfo fi;
      if (!fi.Find(fs))
        ok = false;
      else if (fi.IsDir() && !fi.IsPosixLink())
        ok = NWindows::NFile::NDir::RemoveDirWithSubItems(fs);
      else
        ok = NWindows::NFile::NDir::DeleteFileAlways(fs);
    }
    if (!ok && result == 0)
      result = errno ? errno : EIO;
  }
  return result;
}

int SHFileOperationW(LPSHFILEOPSTRUCTW op)
{
  if (!op)
    return EINVAL;
  const QStringList paths = PathList(op->pFrom, ConvW);
  return OnGuiThread<int>([&]() { return FileOperation(op->wFunc, op->fFlags, paths, op->fAnyOperationsAborted); });
}

int SHFileOperationA(LPSHFILEOPSTRUCTA op)
{
  if (!op)
    return EINVAL;
  const QStringList paths = PathList(op->pFrom, ConvA);
  return OnGuiThread<int>([&]() { return FileOperation(op->wFunc, op->fFlags, paths, op->fAnyOperationsAborted); });
}

/* Opens a document with the desktop's application for it, or runs a
   program with parameters. Returns the hInstApp value of Windows: above 32
   on success, SE_ERR_NOASSOC or SE_ERR_FNF on failure. */
static int Execute(const QString &verb, const QString &file, const QString &params, const QString &dir)
{
  if (file.isEmpty())
    return SE_ERR_FNF;
  if (!params.isEmpty())
  {
    UStringVector parts;
    NCommandLineParser::SplitCommandLine(ToU(params), parts);
    QStringList args;
    FOR_VECTOR (i, parts)
      args.append(FromU(parts[i]));
    return QProcess::startDetached(file, args, dir) ? 33 : SE_ERR_FNF;
  }
  if (!verb.isEmpty() && verb.compare(QLatin1String("open"), Qt::CaseInsensitive) != 0)
    return SE_ERR_NOASSOC;
  const QString path = (dir.isEmpty() || QDir::isAbsolutePath(file)) ? file : QDir(dir).filePath(file);
  const QUrl url = QFileInfo::exists(path) ? QUrl::fromLocalFile(path) : QUrl(file);
  return QDesktopServices::openUrl(url) ? 33 : SE_ERR_NOASSOC;
}

BOOL ShellExecuteExW(LPSHELLEXECUTEINFOW info)
{
  if (!info)
    return FALSE;
  const int r = OnGuiThread<int>([=]()
  {
    return Execute(FromW(info->lpVerb), FromW(info->lpFile), FromW(info->lpParameters), FromW(info->lpDirectory));
  });
  info->hInstApp = (HINSTANCE)(INT_PTR)r;
  info->hProcess = NULL;
  return r > 32 ? TRUE : FALSE;
}

BOOL ShellExecuteExA(LPSHELLEXECUTEINFOA info)
{
  if (!info)
    return FALSE;
  const int r = OnGuiThread<int>([=]()
  {
    return Execute(FromA(info->lpVerb), FromA(info->lpFile), FromA(info->lpParameters), FromA(info->lpDirectory));
  });
  info->hInstApp = (HINSTANCE)(INT_PTR)r;
  info->hProcess = NULL;
  return r > 32 ? TRUE : FALSE;
}

HINSTANCE ShellExecuteW(HWND /* wnd */, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, int /* showCmd */)
{
  return (HINSTANCE)(INT_PTR)OnGuiThread<int>([=]()
  {
    return Execute(FromW(operation), FromW(file), FromW(parameters), FromW(directory));
  });
}

HINSTANCE ShellExecuteA(HWND /* wnd */, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, int /* showCmd */)
{
  return (HINSTANCE)(INT_PTR)OnGuiThread<int>([=]()
  {
    return Execute(FromA(operation), FromA(file), FromA(parameters), FromA(directory));
  });
}

UINT ExtractIconExW(LPCWSTR /* file */, int /* index */, HICON * /* large */, HICON * /* small */, UINT /* numIcons */)
{
  return 0;
}

UINT ExtractIconExA(LPCSTR /* file */, int /* index */, HICON * /* large */, HICON * /* small */, UINT /* numIcons */)
{
  return 0;
}

void SHChangeNotify(LONG /* eventId */, UINT /* flags */, LPCVOID /* item1 */, LPCVOID /* item2 */)
{
}

UINT DragQueryFileW(HDROP /* drop */, UINT /* index */, LPWSTR /* file */, UINT /* size */)
{
  return 0;
}

void DragFinish(HDROP /* drop */)
{
}

void DragAcceptFiles(HWND /* wnd */, BOOL /* accept */)
{
}

HRESULT SHGetSpecialFolderLocation(HWND /* owner */, int folder, LPITEMIDLIST *pidl)
{
  if (!pidl)
    return E_INVALIDARG;
  CSpecialFolderId *id = (CSpecialFolderId *)CoTaskMemAlloc(sizeof(CSpecialFolderId));
  if (!id)
    return E_OUTOFMEMORY;
  id->Cb = (WORD)(sizeof(id->Cb) + sizeof(id->Csidl));
  id->Csidl = folder;
  id->End = 0;
  *pidl = (LPITEMIDLIST)(void *)id;
  return S_OK;
}

HWND HtmlHelpW(HWND /* caller */, LPCWSTR file, UINT command, DWORD_PTR /* data */)
{
  if (command == HH_DISPLAY_TOPIC && file)
  {
    const QString f = FromW(file);
    RunOnGuiThread([&]() { ShowHelpTopic(f); });
  }
  return NULL;
}

HWND HtmlHelpA(HWND /* caller */, LPCSTR file, UINT command, DWORD_PTR /* data */)
{
  if (command == HH_DISPLAY_TOPIC && file)
  {
    const QString f = FromA(file);
    RunOnGuiThread([&]() { ShowHelpTopic(f); });
  }
  return NULL;
}

// ---------- comdlg32 ----------

DWORD CommDlgExtendedError()
{
  return g_CommDlgError;
}

// ---------- Windows/Shell.h, Windows/CommonDialog.h, Windows/Clipboard.h ----------

namespace NWindows {

namespace NShell {

bool GetPathFromIDList(LPCITEMIDLIST itemIDList, UString &path)
{
  path.Empty();
  if (!itemIDList)
    return false;
  const QString p = SpecialFolderPath(((const CSpecialFolderId *)(const void *)itemIDList)->Csidl);
  if (p.isEmpty())
    return false;
  path = ToU(p);
  return true;
}

// Data objects come with OLE drag and drop, which Linux does not have.
HRESULT DataObject_GetData_HDROP_or_IDLIST_Names(IDataObject * /* dataObject */, UStringVector &names)
{
  names.Clear();
  return E_NOTIMPL;
}

HRESULT DataObject_GetData_FILE_ATTRS(IDataObject * /* dataObject */, CFileAttribs & /* attribs */)
{
  return E_NOTIMPL;
}

bool BrowseForFolder(HWND owner, LPCWSTR title, LPCWSTR initialFolder, UString &resultPath)
{
  CFileDialogRequest r;
  r.Kind = CFileDialogRequest::kFolder;
  r.Owner = owner;
  r.Title = title ? FromW(title) : QString();
  r.Path = initialFolder ? FromW(initialFolder) : QString();
  r.FilterIndex = -1;
  if (!FileDialog(r))
    return false;
  resultPath = ToU(r.Path);
  return true;
}

bool BrowseForFolder(HWND owner, LPCTSTR title, LPCTSTR initialFolder, CSysString &resultPath)
{
  UString path;
  if (!BrowseForFolder(owner,
      title ? ToU(FromA(title)).Ptr() : NULL,
      initialFolder ? ToU(FromA(initialFolder)).Ptr() : NULL,
      path))
    return false;
  resultPath = FromU(path).toUtf8().constData();
  return true;
}

}

bool CCommonDialogInfo::CommonDlg_BrowseForFile(LPCWSTR lpstrInitialDir, const UStringVector &filters)
{
  CFileDialogRequest r;
  r.Kind = SaveMode ? CFileDialogRequest::kSaveFile : CFileDialogRequest::kOpenFile;
  r.Owner = hwndOwner;
  r.Title = lpstrTitle ? FromW(lpstrTitle) : QString();
  const QString dir = lpstrInitialDir ? FromW(lpstrInitialDir) : QString();
  const QString name = FromU(FilePath);
  r.Path = dir.isEmpty() ? name : (name.isEmpty() ? dir : QDir(dir).filePath(name));
  for (unsigned i = 0; i + 1 < filters.Size(); i += 2)
    r.Filters.append(QtNameFilter(FromU(filters[i]), FromU(filters[i + 1])));
  r.FilterIndex = FilterIndex;
  if (!FileDialog(r))
    return false;
  FilePath = ToU(r.Path);
  FilterIndex = r.FilterIndex;
  return true;
}

bool CClipboard::Open(HWND /* wndNewOwner */) throw()
{
  m_Open = true;
  return true;
}

bool CClipboard::Close() throw()
{
  m_Open = false;
  return true;
}

bool ClipboardIsFormatAvailableHDROP()
{
  return OnGuiThread<bool>([]()
  {
    const QMimeData *data = QGuiApplication::clipboard()->mimeData();
    return data && data->hasUrls();
  });
}

bool ClipboardSetText(HWND /* owner */, const UString &s)
{
  const QString text = FromU(s);
  RunOnGuiThread([&]() { QGuiApplication::clipboard()->setText(text); });
  return true;
}

}
