// test_folders.cpp
// Unit tests for the POSIX folders (linux/platform/FSFolderPosix.cpp and
// RootFolderPosix.cpp), and browsing of nested archives through UI/Agent,
// all through the IFolderFolder API that the file manager panels use.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Win32Compat.h"

#include "../../../CPP/Common/MyCom.h"
#include "../../../CPP/Windows/PropVariant.h"
#include "../../../CPP/7zip/PropID.h"
#include "../../../CPP/7zip/Archive/IArchive.h"
#include "../../../CPP/7zip/UI/Agent/Agent.h"

#include "FSFolderPosix.h"
#include "RootFolderPosix.h"

using namespace NWindows;

static QString ToQString(const UString &s)
{
  return QString::fromWCharArray(s.Ptr(), (int)s.Len());
}

static QByteArray Bytes(const UString &s)
{
  const FString f = us2fs(s);
  return QByteArray(f.Ptr(), (int)f.Len());
}

static NCOM::CPropVariant GetProp(IFolderFolder *folder, UInt32 index, PROPID propID)
{
  NCOM::CPropVariant prop;
  folder->GetProperty(index, propID, &prop);
  return prop;
}

static UString GetName(IFolderFolder *folder, UInt32 index)
{
  const NCOM::CPropVariant prop = GetProp(folder, index, kpidName);
  return prop.vt == VT_BSTR ? UString(prop.bstrVal) : UString();
}

static UString GetFolderPath(IFolderFolder *folder)
{
  NCOM::CPropVariant prop;
  folder->GetFolderProperty(kpidPath, &prop);
  return prop.vt == VT_BSTR ? UString(prop.bstrVal) : UString();
}

static UString GetFolderType(IFolderFolder *folder)
{
  NCOM::CPropVariant prop;
  folder->GetFolderProperty(kpidType, &prop);
  return prop.vt == VT_BSTR ? UString(prop.bstrVal) : UString();
}

// item names (raw bytes) -> index
static QHash<QByteArray, UInt32> List(IFolderFolder *folder)
{
  QHash<QByteArray, UInt32> items;
  if (folder->LoadItems() != S_OK)
    return items;
  UInt32 num = 0;
  folder->GetNumberOfItems(&num);
  for (UInt32 i = 0; i < num; i++)
    items.insert(Bytes(GetName(folder, i)), i);
  return items;
}

static UInt64 GetUInt64(IFolderFolder *folder, UInt32 index, PROPID propID)
{
  const NCOM::CPropVariant prop = GetProp(folder, index, propID);
  if (prop.vt == VT_UI8) return prop.uhVal.QuadPart;
  if (prop.vt == VT_UI4) return prop.ulVal;
  return (UInt64)(Int64)-1;
}

static QString GetString(IFolderFolder *folder, UInt32 index, PROPID propID)
{
  const NCOM::CPropVariant prop = GetProp(folder, index, propID);
  return prop.vt == VT_BSTR ? ToQString(UString(prop.bstrVal)) : QString("<none>");
}

static bool GetBool(IFolderFolder *folder, UInt32 index, PROPID propID)
{
  const NCOM::CPropVariant prop = GetProp(folder, index, propID);
  return prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE;
}

// raw byte paths: some test names are not valid UTF-8
static void WriteFile(const QByteArray &path, const QByteArray &data)
{
  const int fd = open(path.constData(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  QVERIFY2(fd >= 0, path.constData());
  QCOMPARE(write(fd, data.constData(), (size_t)data.size()), (ssize_t)data.size());
  close(fd);
}

Z7_CLASS_IMP_COM_1(
  CProgressStub
  , IProgress
)
public:
  UInt64 Total = 0;
  UInt64 Completed = 0;
};

Z7_COM7F_IMF(CProgressStub::SetTotal(UInt64 total))
{
  Total = total;
  return S_OK;
}

Z7_COM7F_IMF(CProgressStub::SetCompleted(const UInt64 *completeValue))
{
  if (completeValue)
    Completed = *completeValue;
  return S_OK;
}

// What the panel passes to IFolderOperations::CopyTo of an archive folder.
class CExtractStub Z7_final:
  public IFolderArchiveExtractCallback,
  public IFolderOperationsExtractCallback,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderArchiveExtractCallback)
  Z7_COM_QI_ENTRY(IFolderOperationsExtractCallback)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback)
  Z7_IFACE_COM7_IMP(IFolderOperationsExtractCallback)
public:
  unsigned NumErrors = 0;
  unsigned NumFiles = 0;
};

Z7_COM7F_IMF(CExtractStub::SetTotal(UInt64)) { return S_OK; }
Z7_COM7F_IMF(CExtractStub::SetCompleted(const UInt64 *)) { return S_OK; }

Z7_COM7F_IMF(CExtractStub::AskOverwrite(const wchar_t *, const FILETIME *, const UInt64 *,
    const wchar_t *, const FILETIME *, const UInt64 *, Int32 *answer))
{
  *answer = NOverwriteAnswer::kYes;
  return S_OK;
}

Z7_COM7F_IMF(CExtractStub::PrepareOperation(const wchar_t *, Int32 isFolder, Int32, const UInt64 *))
{
  if (!isFolder)
    NumFiles++;
  return S_OK;
}

Z7_COM7F_IMF(CExtractStub::MessageError(const wchar_t *))
{
  NumErrors++;
  return S_OK;
}

Z7_COM7F_IMF(CExtractStub::SetOperationResult(Int32 opRes, Int32))
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
    NumErrors++;
  return S_OK;
}

Z7_COM7F_IMF(CExtractStub::AskWrite(const wchar_t *, Int32, const FILETIME *, const UInt64 *,
    const wchar_t *, BSTR *, Int32 *))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CExtractStub::ShowMessage(const wchar_t *))
{
  NumErrors++;
  return S_OK;
}

Z7_COM7F_IMF(CExtractStub::SetCurrentFilePath(const wchar_t *)) { return S_OK; }
Z7_COM7F_IMF(CExtractStub::SetNumFiles(UInt64)) { return S_OK; }

// What the panel passes to CopyTo() of a file system folder: AskWrite()
// does what CExtractCallbackImp does for the chosen answer.
class CCopyStub Z7_final:
  public IFolderOperationsExtractCallback,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1(IFolderOperationsExtractCallback)
  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IFolderOperationsExtractCallback)
public:
  enum EMode { kOverwrite, kSkip, kRename };
  EMode Mode = kOverwrite;
  UInt64 Total = 0;
  UInt64 Completed = 0;
  UInt64 AbortAt = 0;       // SetCompleted() cancels at this position
  unsigned NumExisting = 0; // AskWrite() for files that exist
  QStringList Messages;
};

Z7_COM7F_IMF(CCopyStub::SetTotal(UInt64 total))
{
  Total = total;
  return S_OK;
}

Z7_COM7F_IMF(CCopyStub::SetCompleted(const UInt64 *completeValue))
{
  if (completeValue)
    Completed = *completeValue;
  if (AbortAt != 0 && Completed >= AbortAt)
    return E_ABORT;
  return S_OK;
}

Z7_COM7F_IMF(CCopyStub::AskWrite(const wchar_t *, Int32, const FILETIME *, const UInt64 *,
    const wchar_t *destPath, BSTR *destPathResult, Int32 *writeAnswer))
{
  *destPathResult = NULL;
  *writeAnswer = BoolToInt(false);
  FString dest = us2fs(destPath);
  struct stat st;
  if (lstat(dest, &st) == 0)
  {
    NumExisting++;
    if (Mode == kSkip)
      return S_OK;
    if (Mode == kRename)
      dest += "_1";
    else if (unlink(dest) != 0)
      return E_FAIL;
  }
  *writeAnswer = BoolToInt(true);
  return StringToBstr(fs2us(dest), destPathResult);
}

Z7_COM7F_IMF(CCopyStub::ShowMessage(const wchar_t *message))
{
  Messages << ToQString(UString(message));
  return S_OK;
}

Z7_COM7F_IMF(CCopyStub::SetCurrentFilePath(const wchar_t *)) { return S_OK; }
Z7_COM7F_IMF(CCopyStub::SetNumFiles(UInt64)) { return S_OK; }

static UString FsPath(const QByteArray &path)
{
  return fs2us(FString(path.constData()));
}

static QByteArray ReadFile(const QByteArray &path)
{
  QFile f(QFile::decodeName(path));
  if (!f.open(QIODevice::ReadOnly))
    return QByteArray("<missing>");
  return f.readAll();
}

static bool Exists(const QByteArray &path)
{
  struct stat st;
  return lstat(path.constData(), &st) == 0;
}

static QByteArray LinkTarget(const QByteArray &path)
{
  char buf[1024];
  const ssize_t n = readlink(path.constData(), buf, sizeof(buf));
  return n < 0 ? QByteArray("<not a link>") : QByteArray(buf, (int)n);
}

// A directory on another file system than (path), or empty.
static QByteArray OtherFileSystem(const QByteArray &path)
{
  struct stat st;
  if (stat(path.constData(), &st) != 0)
    return QByteArray();
  const QByteArray candidates[] = { "/dev/shm", QFile::encodeName(QDir::currentPath()),
      "/var/tmp", qgetenv("XDG_RUNTIME_DIR") };
  for (const QByteArray &c : candidates)
  {
    struct stat other;
    if (!c.isEmpty() && stat(c.constData(), &other) == 0 && other.st_dev != st.st_dev
        && access(c.constData(), W_OK) == 0)
      return c;
  }
  return QByteArray();
}

static void CheckInner7z(IFolderFolder *inner)
{
  QVERIFY(inner);
  QCOMPARE(GetFolderType(inner), UString(L"7-Zip.7z"));
  QHash<QByteArray, UInt32> items = List(inner);
  QCOMPARE(QSet<QByteArray>(items.keyBegin(), items.keyEnd()), QSet<QByteArray>({ "a.txt", "sub" }));
  QCOMPARE(GetUInt64(inner, items["a.txt"], kpidSize), (UInt64)5);
  CMyComPtr<IFolderFolder> innerSub;
  QCOMPARE(inner->BindToFolder(L"sub", &innerSub), S_OK);
  items = List(innerSub);
  QVERIFY(items.contains("b.txt"));
  QCOMPARE(GetUInt64(innerSub, items["b.txt"], kpidSize), (UInt64)5);
  QVERIFY(GetProp(innerSub, items["b.txt"], kpidMTime).vt == VT_FILETIME);
  // the archive root has no parent folder; the panel goes back to the file system
  CMyComPtr<IFolderFolder> innerRoot, beyond;
  QCOMPARE(innerSub->BindToParentFolder(&innerRoot), S_OK);
  QVERIFY(innerRoot);
  QCOMPARE(innerRoot->BindToParentFolder(&beyond), S_OK);
  QVERIFY(!beyond);
}

class CFoldersTest: public QObject
{
  Q_OBJECT

  QTemporaryDir _tmp;
  QByteArray _root; // with a trailing '/'

  CMyComPtr<IFolderFolder> OpenFs(const QByteArray &path)
  {
    NFsFolder::CFSFolder *spec = new NFsFolder::CFSFolder;
    CMyComPtr<IFolderFolder> folder = spec;
    if (spec->Init(FString(path.constData())) != S_OK)
      return NULL;
    return folder;
  }

private slots:
  void initTestCase()
  {
    QVERIFY(_tmp.isValid());
    _root = QFile::encodeName(_tmp.path()) + "/fs/";
    QVERIFY(QDir().mkpath(QString::fromUtf8(_root) + "sub/deep"));
    WriteFile(_root + "file.txt", "hello\n");
    QCOMPARE(chmod(_root + "file.txt", 0640), 0);
    const struct timespec times[2] = { { 1577934245, 0 }, { 1577934245, 123456789 } };
    QCOMPARE(utimensat(AT_FDCWD, _root + "file.txt", times, 0), 0);
    WriteFile(_root + ".hidden", "");
    WriteFile(_root + "readonly.txt", "r");
    QCOMPARE(chmod(_root + "readonly.txt", 0444), 0);
    WriteFile(_root + "sub/a.txt", "aaa");
    WriteFile(_root + "sub/deep/b.txt", "bbbbb");
    QCOMPARE(symlink("file.txt", _root + "link-file"), 0);
    QCOMPARE(symlink("sub", _root + "link-dir"), 0);
    QCOMPARE(symlink("missing", _root + "broken"), 0);
    WriteFile(_root + "za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87.txt", "u");
    WriteFile(_root + "bad\xff.txt", "x");
    QCOMPARE(mkdir(_root + "dir\xfe", 0755), 0);
    WriteFile(_root + "dir\xfe/inner", "i");
    QCOMPARE(mkfifo(_root + "fifo", 0600), 0);
    WriteFile(_root + "file2", "22");
    WriteFile(_root + "file10", "1010");
  }

  void listing()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    QVERIFY(f);
    const QHash<QByteArray, UInt32> items = List(f);
    const QSet<QByteArray> expected = { "file.txt", ".hidden", "readonly.txt", "sub",
        "link-file", "link-dir", "broken", "za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87.txt",
        "bad\xff.txt", "dir\xfe", "fifo", "file2", "file10" };
    QCOMPARE(QSet<QByteArray>(items.keyBegin(), items.keyEnd()), expected);
    // the Unicode name arrives as Unicode
    QCOMPARE(ToQString(GetName(f, items["za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87.txt"])), QString::fromUtf8("zażółć.txt"));
    QCOMPARE(GetFolderType(f), UString(L"FSFolder"));
    QCOMPARE(Bytes(GetFolderPath(f)), _root);
  }

  void properties()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    const QHash<QByteArray, UInt32> items = List(f);
    const UInt32 file = items["file.txt"];
    struct stat st;
    QCOMPARE(lstat(_root + "file.txt", &st), 0);

    QVERIFY(!GetBool(f, file, kpidIsDir));
    QCOMPARE(GetUInt64(f, file, kpidSize), (UInt64)6);
    QCOMPARE(GetUInt64(f, file, kpidPosixAttrib), (UInt64)(S_IFREG | 0640));
    QCOMPARE(GetUInt64(f, file, kpidINode), (UInt64)st.st_ino);
    QCOMPARE(GetUInt64(f, file, kpidLinks), (UInt64)1);
    const UInt32 attrib = (UInt32)GetUInt64(f, file, kpidAttrib);
    QVERIFY(attrib & FILE_ATTRIBUTE_ARCHIVE);
    QVERIFY(!(attrib & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY)));
    QCOMPARE(attrib >> 16, (UInt32)(S_IFREG | 0640));

    // 2020-01-02 03:04:05.123456789 UTC in 100 ns units since 1601, plus 89 ns
    const NCOM::CPropVariant mtime = GetProp(f, file, kpidMTime);
    QCOMPARE(mtime.vt, (VARTYPE)VT_FILETIME);
    const UInt64 ticks = ((UInt64)mtime.filetime.dwHighDateTime << 32) | mtime.filetime.dwLowDateTime;
    QCOMPARE(ticks, (UInt64)(1577934245 + 11644473600ull) * 10000000 + 1234567);
    // the remaining nanoseconds (CPropVariant::Get_Ns100() ignores them for
    // k_PropVar_TimePrec_1ns, so the fields are checked directly)
    QCOMPARE((unsigned)mtime.wReserved1, (unsigned)k_PropVar_TimePrec_1ns);
    QCOMPARE((unsigned)mtime.wReserved2, 89u);
    QCOMPARE(GetProp(f, file, kpidChangeTime).vt, (VARTYPE)VT_FILETIME);

    const struct passwd *pw = getpwuid(getuid());
    const struct group *gr = getgrgid(getgid());
    QVERIFY(pw && gr);
    QCOMPARE(GetString(f, file, kpidUser), QString::fromUtf8(pw->pw_name));
    QCOMPARE(GetString(f, file, kpidGroup), QString::fromUtf8(gr->gr_name));
    QCOMPARE(GetString(f, file, kpidSymLink), QString("<none>"));

    QVERIFY(GetUInt64(f, items["readonly.txt"], kpidAttrib) & FILE_ATTRIBUTE_READONLY);
    QVERIFY(GetUInt64(f, items[".hidden"], kpidAttrib) & FILE_ATTRIBUTE_HIDDEN);
    const UInt32 sub = items["sub"];
    QVERIFY(GetBool(f, sub, kpidIsDir));
    QVERIFY(GetUInt64(f, sub, kpidAttrib) & FILE_ATTRIBUTE_DIRECTORY);
    QCOMPARE(GetProp(f, sub, kpidSize).vt, (VARTYPE)VT_EMPTY); // not calculated yet

    const UInt32 linkFile = items["link-file"];
    QVERIFY(!GetBool(f, linkFile, kpidIsDir));
    QCOMPARE(GetUInt64(f, linkFile, kpidSize), (UInt64)6); // of the target
    QCOMPARE(GetString(f, linkFile, kpidSymLink), QString("file.txt"));
    QVERIFY(S_ISLNK(GetUInt64(f, linkFile, kpidPosixAttrib)));
    QVERIFY(GetUInt64(f, linkFile, kpidAttrib) & FILE_ATTRIBUTE_REPARSE_POINT);
    const UInt32 linkDir = items["link-dir"];
    QVERIFY(GetBool(f, linkDir, kpidIsDir));
    QVERIFY(GetUInt64(f, linkDir, kpidAttrib) & FILE_ATTRIBUTE_DIRECTORY);
    const UInt32 broken = items["broken"];
    QVERIFY(!GetBool(f, broken, kpidIsDir));
    QCOMPARE(GetString(f, broken, kpidSymLink), QString("missing"));
    QVERIFY(S_ISFIFO(GetUInt64(f, items["fifo"], kpidPosixAttrib)));
  }

  void propertyList()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    UInt32 num = 0;
    QCOMPARE(f->GetNumberOfProperties(&num), S_OK);
    QSet<PROPID> ids;
    for (UInt32 i = 0; i < num; i++)
    {
      BSTR name;
      PROPID propID;
      VARTYPE vt;
      QCOMPARE(f->GetPropertyInfo(i, &name, &propID, &vt), S_OK);
      ids.insert(propID);
    }
    for (PROPID id : { kpidName, kpidSize, kpidMTime, kpidPosixAttrib, kpidUser, kpidGroup, kpidSymLink })
      QVERIFY(ids.contains(id));
    QVERIFY(!ids.contains(kpidPrefix));
    CMyComPtr<IFolderSetFlatMode> flat;
    f.QueryInterface(IID_IFolderSetFlatMode, &flat);
    QVERIFY(flat);
    flat->SetFlatMode(1);
    UInt32 numFlat = 0;
    f->GetNumberOfProperties(&numFlat);
    QCOMPARE(numFlat, num + 1);
  }

  void navigation()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    QHash<QByteArray, UInt32> items = List(f);
    CMyComPtr<IFolderFolder> sub;
    QCOMPARE(f->BindToFolder(items["sub"], &sub), S_OK);
    QCOMPARE(Bytes(GetFolderPath(sub)), _root + "sub/");
    items = List(sub);
    QVERIFY(items.contains("a.txt") && items.contains("deep"));
    CMyComPtr<IFolderFolder> deep;
    QCOMPARE(sub->BindToFolder(L"deep", &deep), S_OK);
    QVERIFY(List(deep).contains("b.txt"));

    // up to "/", then the root folder (no parent)
    CMyComPtr<IFolderFolder> cur = deep;
    QByteArray expected = _root + "sub/";
    for (int i = 0; i < 64; i++)
    {
      CMyComPtr<IFolderFolder> parent;
      QCOMPARE(cur->BindToParentFolder(&parent), S_OK);
      if (!parent)
        break;
      const QByteArray path = Bytes(GetFolderPath(parent));
      if (i == 0)
        QCOMPARE(path, expected);
      cur = parent;
    }
    QCOMPARE(Bytes(GetFolderPath(cur)), QByteArray("/"));

    // a link to a directory and a directory with a name that is not UTF-8
    items = List(f);
    CMyComPtr<IFolderFolder> viaLink;
    QCOMPARE(f->BindToFolder(items["link-dir"], &viaLink), S_OK);
    QVERIFY(List(viaLink).contains("a.txt"));
    CMyComPtr<IFolderFolder> bad;
    QCOMPARE(f->BindToFolder(items["dir\xfe"], &bad), S_OK);
    QCOMPARE(Bytes(GetFolderPath(bad)), _root + "dir\xfe/");
    QVERIFY(List(bad).contains("inner"));
    CMyComPtr<IFolderFolder> none;
    QCOMPARE(f->BindToFolder(items["file.txt"], &none), E_INVALIDARG);
    QVERIFY(f->BindToFolder(L"no-such-dir", &none) != S_OK);
    QVERIFY(!none);
  }

  void flatMode()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    CMyComPtr<IFolderSetFlatMode> flat;
    f.QueryInterface(IID_IFolderSetFlatMode, &flat);
    flat->SetFlatMode(1);
    QCOMPARE(f->LoadItems(), S_OK);
    UInt32 num = 0;
    f->GetNumberOfItems(&num);
    QMultiMap<QByteArray, QString> prefixes;
    for (UInt32 i = 0; i < num; i++)
      prefixes.insert(Bytes(GetName(f, i)), GetString(f, i, kpidPrefix));
    QCOMPARE(prefixes.value("a.txt"), QString("sub/")); // only once: link-dir is not followed
    QCOMPARE(prefixes.values("a.txt").size(), 1);
    QCOMPARE(prefixes.value("b.txt"), QString("sub/deep/"));
    QCOMPARE(prefixes.value("file.txt"), QString("<none>"));
  }

  void compare()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    const QHash<QByteArray, UInt32> items = List(f);
    CMyComPtr<IFolderCompare> cmp;
    f.QueryInterface(IID_IFolderCompare, &cmp);
    QVERIFY(cmp);
    QVERIFY(cmp->CompareItems(items["file2"], items["file10"], kpidName, 0) < 0); // natural order
    QVERIFY(cmp->CompareItems(items["file10"], items["file2"], kpidSize, 0) > 0);
    QVERIFY(cmp->CompareItems(items["sub"], items["file2"], kpidIsDir, 0) < 0);
    QCOMPARE(cmp->CompareItems(items["file.txt"], items["file.txt"], kpidMTime, 0), 0);
  }

  void compareMatchesUpstream()
  {
    // FolderCompare.cpp keeps a copy of the function in FileManager/PanelSort.cpp
    auto body = [](const QString &path) -> QString
    {
      QFile f(path);
      if (!f.open(QIODevice::ReadOnly))
        return QString();
      const QString text = QString::fromUtf8(f.readAll());
      const int start = text.indexOf("int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2)\n{");
      if (start < 0)
        return QString();
      const int end = text.indexOf("\n}\n", start);
      return text.mid(start, end - start);
    };
    const QString root(Z7_SOURCE_ROOT);
    const QString upstream = body(root + "/CPP/7zip/UI/FileManager/PanelSort.cpp");
    QVERIFY(upstream.length() > 100);
    QCOMPARE(body(root + "/linux/platform/FolderCompare.cpp"), upstream);
  }

  void wasChanged()
  {
    QVERIFY(QDir().mkpath(QString::fromUtf8(_root) + "watch"));
    CMyComPtr<IFolderFolder> f1 = OpenFs(_root + "watch/");
    CMyComPtr<IFolderFolder> f2 = OpenFs(_root + "watch/");
    CMyComPtr<IFolderWasChanged> w1, w2;
    f1.QueryInterface(IID_IFolderWasChanged, &w1);
    f2.QueryInterface(IID_IFolderWasChanged, &w2);
    QVERIFY(w1 && w2);
    QCOMPARE(f1->LoadItems(), S_OK);
    Int32 changed = 1;
    w1->WasChanged(&changed);
    QCOMPARE(changed, 0);
    WriteFile(_root + "watch/new", "n");
    w1->WasChanged(&changed);
    QCOMPARE(changed, 1);
    w1->WasChanged(&changed);
    QCOMPARE(changed, 0);
    w2->WasChanged(&changed); // the same directory: both watchers see it
    QCOMPARE(changed, 1);
    // the other folder keeps working when one is released
    f1.Release();
    w1.Release();
    QCOMPARE(unlink(_root + "watch/new"), 0);
    w2->WasChanged(&changed);
    QCOMPARE(changed, 1);
    // a subdirectory change is not a change of this directory
    QVERIFY(QDir().mkpath(QString::fromUtf8(_root) + "watch/d"));
    w2->WasChanged(&changed);
    WriteFile(_root + "watch/d/x", "x");
    w2->WasChanged(&changed);
    QCOMPARE(changed, 0);
  }

  void calcFullSize()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root);
    const QHash<QByteArray, UInt32> items = List(f);
    CMyComPtr<IFolderCalcItemFullSize> calc;
    f.QueryInterface(IID_IFolderCalcItemFullSize, &calc);
    QVERIFY(calc);
    QCOMPARE(calc->CalcItemFullSize(items["sub"], NULL), S_OK);
    QCOMPARE(GetUInt64(f, items["sub"], kpidSize), (UInt64)(3 + 5));
    QCOMPARE(GetUInt64(f, items["sub"], kpidNumSubFiles), (UInt64)2);
    QCOMPARE(GetUInt64(f, items["sub"], kpidNumSubDirs), (UInt64)1);
  }

  void operations()
  {
    QVERIFY(QDir().mkpath(QString::fromUtf8(_root) + "ops"));
    CMyComPtr<IFolderFolder> f = OpenFs(_root + "ops/");
    CMyComPtr<IFolderOperations> ops;
    f.QueryInterface(IID_IFolderOperations, &ops);
    QVERIFY(ops);
    QCOMPARE(ops->CreateFolder(L"newdir/nested", NULL), S_OK);
    QCOMPARE(ops->CreateFile(L"newfile", NULL), S_OK);
    QVERIFY(ops->CreateFile(L"newfile", NULL) != S_OK);
    WriteFile(_root + "ops/newdir/nested/x", "x");
    QCOMPARE(symlink("/", _root + "ops/newdir/to-root"), 0); // must not be followed

    QHash<QByteArray, UInt32> items = List(f);
    QCOMPARE(ops->Rename(items["newfile"], L"renamed", NULL), S_OK);
    items = List(f);
    QVERIFY(items.contains("renamed") && !items.contains("newfile"));
    QCOMPARE(ops->Rename(items["renamed"], L"newdir", NULL), HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));

    CProgressStub *progressSpec = new CProgressStub;
    CMyComPtr<IProgress> progress = progressSpec;
    const UInt32 indices[] = { items["newdir"], items["renamed"] };
    QCOMPARE(ops->Delete(indices, 2, progress), S_OK);
    QCOMPARE(progressSpec->Total, (UInt64)2);
    QCOMPARE(progressSpec->Completed, (UInt64)2);
    QVERIFY(List(f).isEmpty());
    QVERIFY(QFile::exists("/")); // not removed through the link, as a sanity check

    // comments go to descript.ion
    QCOMPARE(ops->CreateFile(L"commented", NULL), S_OK);
    items = List(f);
    NCOM::CPropVariant comment = L"a comment";
    QCOMPARE(ops->SetProperty(items["commented"], kpidComment, &comment, NULL), S_OK);
    QVERIFY(QFile::exists(QString::fromUtf8(_root) + "ops/descript.ion"));
    items = List(f);
    QCOMPARE(GetString(f, items["commented"], kpidComment), QString("a comment"));

    // no alternate streams on POSIX
    CMyComPtr<IFolderAltStreams> alt;
    f.QueryInterface(IID_IFolderAltStreams, &alt);
    QVERIFY(alt);
    Int32 supported = 1;
    QCOMPARE(alt->AreAltStreamsSupported(items["commented"], &supported), S_OK);
    QCOMPARE(supported, (Int32)0);
  }

  void copy()
  {
    const QByteArray base = QFile::encodeName(_tmp.path()) + "/copy/";
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "src/tree/empty")));
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "dest")));
    WriteFile(base + "src/one.txt", "one");
    WriteFile(base + "src/tree/two.txt", "two two");
    QCOMPARE(chmod(base + "src/tree/two.txt", 0750), 0);
    QCOMPARE(symlink("two.txt", base + "src/tree/link"), 0);
    QCOMPARE(symlink("/nonexistent", base + "src/tree/broken"), 0);
    QCOMPARE(mkfifo(base + "src/tree/fifo", 0600), 0);
    const struct timespec times[2] = { { 1577934245, 0 }, { 1577934245, 123456789 } };
    QCOMPARE(utimensat(AT_FDCWD, base + "src/tree/two.txt", times, 0), 0);
    QCOMPARE(chmod(base + "src/tree", 0750), 0);
    QCOMPARE(utimensat(AT_FDCWD, base + "src/tree", times, 0), 0);

    CMyComPtr<IFolderFolder> src = OpenFs(base + "src/");
    CMyComPtr<IFolderOperations> ops;
    src.QueryInterface(IID_IFolderOperations, &ops);
    QHash<QByteArray, UInt32> items = List(src);
    CCopyStub *stubSpec = new CCopyStub;
    CMyComPtr<IFolderOperationsExtractCallback> stub = stubSpec;
    const UInt32 one = items["one.txt"];
    const UInt32 tree = items["tree"];
    const UInt32 both[] = { one, tree };
    QCOMPARE(ops->CopyTo(0, both, 2, 0, 0, FsPath(base + "dest/"), stub), S_OK);
    QCOMPARE(stubSpec->Messages, QStringList());
    QCOMPARE(stubSpec->NumExisting, 0u);
    // links count with the length of their text, as in the folder sizes
    QCOMPARE(stubSpec->Total, (UInt64)(3 + 7 + 7 + 12));
    QCOMPARE(stubSpec->Completed, stubSpec->Total);
    QCOMPARE(ReadFile(base + "dest/one.txt"), QByteArray("one"));
    QCOMPARE(ReadFile(base + "dest/tree/two.txt"), QByteArray("two two"));
    struct stat st;
    QCOMPARE(lstat(base + "dest/tree/two.txt", &st), 0);
    QCOMPARE(st.st_mode & 07777, (mode_t)0750);
    QCOMPARE(st.st_mtim.tv_sec, times[1].tv_sec);
    QCOMPARE(st.st_mtim.tv_nsec, times[1].tv_nsec);
    QCOMPARE(LinkTarget(base + "dest/tree/link"), QByteArray("two.txt"));
    QCOMPARE(LinkTarget(base + "dest/tree/broken"), QByteArray("/nonexistent"));
    QCOMPARE(lstat(base + "dest/tree/fifo", &st), 0);
    QVERIFY(S_ISFIFO(st.st_mode));
    QCOMPARE(lstat(base + "dest/tree/empty", &st), 0);
    QVERIFY(S_ISDIR(st.st_mode));
    QCOMPARE(lstat(base + "dest/tree", &st), 0);
    QCOMPARE(st.st_mode & 07777, (mode_t)0750);
    QCOMPARE(st.st_mtim.tv_sec, times[1].tv_sec);
    QVERIFY(Exists(base + "src/tree/two.txt"));

    // again: the folder is merged, and every file is asked about
    stubSpec->Mode = CCopyStub::kSkip;
    QCOMPARE(ops->CopyTo(0, both, 2, 0, 0, FsPath(base + "dest/"), stub), S_OK);
    QCOMPARE(stubSpec->NumExisting, 5u);
    QCOMPARE(stubSpec->Messages, QStringList());
    WriteFile(base + "src/one.txt", "ONE!");
    stubSpec->Mode = CCopyStub::kOverwrite;
    QCOMPARE(ops->CopyTo(0, &one, 1, 0, 0, FsPath(base + "dest/"), stub), S_OK);
    QCOMPARE(ReadFile(base + "dest/one.txt"), QByteArray("ONE!"));
    stubSpec->Mode = CCopyStub::kRename;
    QCOMPARE(ops->CopyTo(0, &one, 1, 0, 0, FsPath(base + "dest/"), stub), S_OK);
    QCOMPARE(ReadFile(base + "dest/one.txt_1"), QByteArray("ONE!"));

    // a path without a trailing separator is the new name of the item
    QCOMPARE(ops->CopyTo(0, &one, 1, 0, 0, FsPath(base + "dest/renamed.txt"), stub), S_OK);
    QCOMPARE(ReadFile(base + "dest/renamed.txt"), QByteArray("ONE!"));
    QCOMPARE(ops->CopyTo(0, both, 2, 0, 0, FsPath(base + "dest/x"), stub), E_INVALIDARG);
    QCOMPARE(stubSpec->Messages, QStringList());

    // onto itself: directly, through a symbolic link and through a hard link
    QCOMPARE(ops->CopyTo(0, &one, 1, 0, 0, FsPath(base + "src/one.txt"), stub), E_ABORT);
    QCOMPARE(ops->CopyTo(0, &tree, 1, 0, 0, FsPath(base + "src/tree/empty/"), stub), E_ABORT);
    QCOMPARE(symlink(base + "src", base + "src-link"), 0);
    QCOMPARE(ops->CopyTo(0, &tree, 1, 0, 0, FsPath(base + "src-link/tree/empty/"), stub), E_ABORT);
    QCOMPARE(link(base + "src/one.txt", base + "hard"), 0);
    QCOMPARE(ops->CopyTo(0, &one, 1, 0, 0, FsPath(base + "hard"), stub), E_ABORT);
    QCOMPARE(stubSpec->Messages.size(), 4);
    QVERIFY(stubSpec->Messages[0].startsWith("Cannot copy file onto itself : "));
    QVERIFY(stubSpec->Messages[1].startsWith("Cannot copy folder onto itself : "));
    QVERIFY(stubSpec->Messages[2].startsWith("Cannot copy folder onto itself : "));
    QVERIFY(stubSpec->Messages[3].startsWith("Cannot copy file onto itself : "));
    QCOMPARE(ReadFile(base + "src/one.txt"), QByteArray("ONE!"));
    QVERIFY(!Exists(base + "src/tree/empty/tree"));

    // errors stop the operation and are reported with the path
    stubSpec->Messages.clear();
    QCOMPARE(ops->CopyTo(0, &one, 1, 0, 0, FsPath(base + "missing/x/"), stub), E_ABORT);
    QCOMPARE(stubSpec->Messages.size(), 1);
    QVERIFY(stubSpec->Messages[0].endsWith(" : " + QFile::decodeName(base + "missing/x/one.txt")));

    // paths from outside the panel (drag and drop, clipboard)
    stubSpec->Messages.clear();
    UStringVector paths;
    paths.Add(FsPath(base + "src/one.txt"));
    paths.Add(FsPath(base + "src/tree/"));
    paths.Add(FsPath(base + "src/missing"));
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "dest3")));
    QCOMPARE(NFsFolder::CopyFileSystemItems(paths, FString(base + "dest3/"), false, stub), S_OK);
    QCOMPARE(ReadFile(base + "dest3/one.txt"), QByteArray("ONE!"));
    QCOMPARE(LinkTarget(base + "dest3/tree/link"), QByteArray("two.txt"));
    QCOMPARE(stubSpec->Messages.size(), 1);
    QVERIFY(stubSpec->Messages[0].startsWith("Cannot find the file : "));
  }

  void copyCancel()
  {
    const QByteArray base = QFile::encodeName(_tmp.path()) + "/cancel/";
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "dest")));
    WriteFile(base + "big", QByteArray(3 << 20, 'b'));
    CMyComPtr<IFolderFolder> src = OpenFs(base);
    CMyComPtr<IFolderOperations> ops;
    src.QueryInterface(IID_IFolderOperations, &ops);
    const UInt32 big = List(src)["big"];
    CCopyStub *stubSpec = new CCopyStub;
    CMyComPtr<IFolderOperationsExtractCallback> stub = stubSpec;
    stubSpec->AbortAt = 1;
    QCOMPARE(ops->CopyTo(0, &big, 1, 0, 0, FsPath(base + "dest/"), stub), E_ABORT);
    // the partial copy is removed
    QVERIFY(!Exists(base + "dest/big"));
    QVERIFY(Exists(base + "big"));
  }

  void move()
  {
    const QByteArray base = QFile::encodeName(_tmp.path()) + "/move/";
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "src/tree/sub")));
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "dest/tree")));
    WriteFile(base + "src/one.txt", "one");
    WriteFile(base + "src/tree/sub/two.txt", "two");
    WriteFile(base + "dest/tree/old.txt", "old");
    CMyComPtr<IFolderFolder> src = OpenFs(base + "src/");
    CMyComPtr<IFolderOperations> ops;
    src.QueryInterface(IID_IFolderOperations, &ops);
    QHash<QByteArray, UInt32> items = List(src);
    CCopyStub *stubSpec = new CCopyStub;
    CMyComPtr<IFolderOperationsExtractCallback> stub = stubSpec;

    // a rename in the same file system; "tree" is merged with the existing one
    struct stat before, after;
    QCOMPARE(lstat(base + "src/one.txt", &before), 0);
    const UInt32 both[] = { items["one.txt"], items["tree"] };
    QCOMPARE(ops->CopyTo(1, both, 2, 0, 0, FsPath(base + "dest/"), stub), S_OK);
    QCOMPARE(stubSpec->Messages, QStringList());
    QCOMPARE(lstat(base + "dest/one.txt", &after), 0);
    QCOMPARE(after.st_ino, before.st_ino);
    QCOMPARE(ReadFile(base + "dest/tree/sub/two.txt"), QByteArray("two"));
    QCOMPARE(ReadFile(base + "dest/tree/old.txt"), QByteArray("old"));
    QVERIFY(!Exists(base + "src/one.txt"));
    QVERIFY(!Exists(base + "src/tree"));

    // a move onto itself leaves the file alone
    WriteFile(base + "src/self.txt", "self");
    items = List(src);
    const UInt32 self = items["self.txt"];
    QCOMPARE(ops->CopyTo(1, &self, 1, 0, 0, FsPath(base + "src/"), stub), E_ABORT);
    QCOMPARE(stubSpec->Messages.size(), 1);
    QVERIFY(stubSpec->Messages[0].startsWith("Cannot move file onto itself : "));
    QCOMPARE(ReadFile(base + "src/self.txt"), QByteArray("self"));

    // to another file system: copied with the attributes, then deleted
    const QByteArray otherFs = OtherFileSystem(base);
    if (otherFs.isEmpty())
      QSKIP("no other writable file system to move to");
    QTemporaryDir otherDir(QFile::decodeName(otherFs) + "/z7-test-move-XXXXXX");
    QVERIFY(otherDir.isValid());
    const QByteArray other = QFile::encodeName(otherDir.path()) + "/";
    QVERIFY(QDir().mkpath(QFile::decodeName(base + "src/far/inner")));
    WriteFile(base + "src/far/inner/data", QByteArray(100000, 'd'));
    QCOMPARE(chmod(base + "src/far/inner/data", 0604), 0);
    QCOMPARE(symlink("inner/data", base + "src/far/link"), 0);
    const struct timespec times[2] = { { 1577934245, 0 }, { 1577934245, 5000 } };
    QCOMPARE(utimensat(AT_FDCWD, base + "src/far/inner/data", times, 0), 0);
    items = List(src);
    const UInt32 far = items["far"];
    stubSpec->Messages.clear();
    QCOMPARE(ops->CopyTo(1, &far, 1, 0, 0, FsPath(other), stub), S_OK);
    QCOMPARE(stubSpec->Messages, QStringList());
    QVERIFY(!Exists(base + "src/far"));
    QCOMPARE(ReadFile(other + "far/inner/data"), QByteArray(100000, 'd'));
    QCOMPARE(LinkTarget(other + "far/link"), QByteArray("inner/data"));
    QCOMPARE(lstat(other + "far/inner/data", &after), 0);
    QCOMPARE(after.st_mode & 07777, (mode_t)0604);
    QCOMPARE(after.st_mtim.tv_nsec, times[1].tv_nsec);
  }

  void clone()
  {
    CMyComPtr<IFolderFolder> f = OpenFs(_root + "sub/");
    CMyComPtr<IFolderClone> cl;
    f.QueryInterface(IID_IFolderClone, &cl);
    CMyComPtr<IFolderFolder> copy;
    QCOMPARE(cl->Clone(&copy), S_OK);
    QCOMPARE(Bytes(GetFolderPath(copy)), _root + "sub/");
    QCOMPARE(List(copy).size(), 2);
  }

  void unreadableFolder()
  {
    if (geteuid() == 0)
      QSKIP("root can read every directory");
    QVERIFY(QDir().mkpath(QString::fromUtf8(_root) + "locked"));
    QCOMPARE(chmod(_root + "locked", 0), 0);
    NFsFolder::CFSFolder *spec = new NFsFolder::CFSFolder;
    CMyComPtr<IFolderFolder> f = spec;
    const HRESULT res = spec->Init(FString(_root + "locked/"));
    QCOMPARE(chmod(_root + "locked", 0755), 0);
    QVERIFY(res != S_OK);
  }

  void mountInfo()
  {
    const AString text (
        "22 1 0:21 / / rw,relatime shared:1 - btrfs /dev/nvme0n1p2 rw,subvol=/@\n"
        "23 22 0:21 /@home /home rw,relatime shared:2 - btrfs /dev/nvme0n1p2 rw\n"
        "24 22 0:22 / /proc rw,nosuid shared:3 - proc proc rw\n"
        "25 22 259:1 / /boot rw - vfat /dev/nvme0n1p1 rw\n"
        "26 22 0:23 / /tmp rw shared:4 - tmpfs tmpfs rw\n"
        "27 22 0:21 /@log /var/log rw - btrfs /dev/nvme0n1p2 rw\n"
        "28 22 8:17 / /run/media/user/USB\\040Stick rw master:5 - vfat /dev/sdb1 rw\n"
        "29 22 0:50 / /mnt/nas rw - nfs4 server:/export rw\n"
        "30 22 0:51 / /run/user/1000/gvfs rw - fuse.gvfsd-fuse gvfsd-fuse rw\n"
        "31 22 8:2 / /data rw - ext4 /dev/sda2 rw\n"
        "32 22 8:3 / /data rw - xfs /dev/sda3 rw\n"
        "33 22 0:24 / /run/media rw - tmpfs tmpfs rw\n"
        "bad line\n");
    CObjectVector<NRootFolder::CMountPoint> mounts;
    NRootFolder::ParseMountInfo(text, mounts);
    QCOMPARE((int)mounts.Size(), 11); // /data twice: the later mount wins
    QStringList user;
    FOR_VECTOR (i, mounts)
      if (NRootFolder::IsUserMountPoint(mounts[i]))
        user << QString::fromUtf8(mounts[i].Path.Ptr());
    QCOMPARE(user, QStringList() << "/" << "/home" << "/run/media/user/USB Stick" << "/mnt/nas" << "/data");
    QCOMPARE(QString(mounts.Back().FsType.Ptr()), QString("tmpfs"));

    const QByteArray oldHome = qgetenv("HOME");
    qputenv("HOME", "/home/tester/");
    CRootFolder *spec = new CRootFolder;
    CMyComPtr<IFolderFolder> root = spec;
    spec->Init(&text);
    qputenv("HOME", oldHome);
    UInt32 num = 0;
    root->GetNumberOfItems(&num);
    QStringList names;
    for (UInt32 i = 0; i < num; i++)
      names << ToQString(GetName(root, i));
    QCOMPARE(names, QStringList() << "/home/tester" << "/" << "/home" << "/run/media/user/USB Stick" << "/mnt/nas" << "/data");
    QCOMPARE(GetString(root, 0, kpidFileSystem), QString("btrfs")); // /home/tester is on /home
    QCOMPARE(GetString(root, 5, kpidFileSystem), QString("xfs"));
    QCOMPARE(GetString(root, 5, kpidVolumeName), QString("/dev/sda3"));
    QVERIFY(GetBool(root, 3, kpidIsDir));
  }

  void rootFolder()
  {
    CRootFolder *spec = new CRootFolder;
    CMyComPtr<IFolderFolder> root = spec;
    QCOMPARE(root->LoadItems(), S_OK);
    QCOMPARE(GetFolderType(root), UString(L"RootFolder"));
    UInt32 num = 0;
    root->GetNumberOfItems(&num);
    QVERIFY(num >= 2);
    QCOMPARE(Bytes(GetName(root, 0)), qgetenv("HOME"));
    int rootIndex = -1;
    for (UInt32 i = 0; i < num; i++)
      if (GetName(root, i) == L"/")
        rootIndex = (int)i;
    QVERIFY(rootIndex >= 0);
    const UInt64 total = GetUInt64(root, (UInt32)rootIndex, kpidTotalSize);
    const UInt64 free = GetUInt64(root, (UInt32)rootIndex, kpidFreeSpace);
    QVERIFY(total != (UInt64)(Int64)-1 && total > 0);
    QVERIFY(free <= total);
    QVERIFY(GetString(root, (UInt32)rootIndex, kpidFileSystem) != "<none>");

    CMyComPtr<IFolderFolder> sub;
    QCOMPARE(root->BindToFolder((UInt32)rootIndex, &sub), S_OK);
    QCOMPARE(Bytes(GetFolderPath(sub)), QByteArray("/"));
    sub.Release();
    QCOMPARE(root->BindToFolder(L"~", &sub), S_OK);
    QCOMPARE(Bytes(GetFolderPath(sub)), qgetenv("HOME") + "/");
    sub.Release();
    QCOMPARE(root->BindToFolder(fs2us(FString(_root + "sub")), &sub), S_OK);
    QCOMPARE(Bytes(GetFolderPath(sub)), _root + "sub/");
    sub.Release();
    QCOMPARE(root->BindToFolder(L"relative/path", &sub), E_INVALIDARG);
    QCOMPARE(root->BindToFolder(L"", &sub), S_OK);
    QCOMPARE(GetFolderType(sub), UString(L"RootFolder"));
    CMyComPtr<IFolderFolder> parent;
    QCOMPARE(root->BindToParentFolder(&parent), S_OK);
    QVERIFY(!parent);
  }

  void nestedArchives()
  {
    // a .7z inside a .zip and inside a .tar, made with the 7zz of this build
    const QString dir = QString::fromUtf8(_root) + "arch/";
    const QString stage = QString::fromUtf8(_root) + "stage/";
    const QString out = QString::fromUtf8(_root) + "extracted/";
    QVERIFY(QDir().mkpath(stage + "sub"));
    QVERIFY(QDir().mkpath(dir + "docs"));
    QVERIFY(QDir().mkpath(out));
    WriteFile(QFile::encodeName(stage) + "a.txt", "alpha");
    WriteFile(QFile::encodeName(stage) + "sub/b.txt", "bravo");
    WriteFile(QFile::encodeName(dir) + "docs/readme.txt", "read me");
    const QString sevenZip(Z7_TEST_7ZZ);
    QProcess p;
    p.setWorkingDirectory(stage);
    p.start(sevenZip, { "a", "-bd", "-y", dir + "inner.7z", "a.txt", "sub" });
    QVERIFY(p.waitForFinished(60000));
    QCOMPARE(p.exitCode(), 0);
    p.setWorkingDirectory(dir);
    p.start(sevenZip, { "a", "-bd", "-y", "-tzip", "outer.zip", "docs", "inner.7z" });
    QVERIFY(p.waitForFinished(60000));
    QCOMPARE(p.exitCode(), 0);
    p.start(sevenZip, { "a", "-bd", "-y", "-ttar", "outer.tar", "inner.7z" });
    QVERIFY(p.waitForFinished(60000));
    QCOMPARE(p.exitCode(), 0);

    // root folder -> file system folder
    CRootFolder *rootSpec = new CRootFolder;
    CMyComPtr<IFolderFolder> root = rootSpec;
    rootSpec->Init();
    CMyComPtr<IFolderFolder> fs;
    QCOMPARE(root->BindToFolder(fs2us(FString(QFile::encodeName(dir).constData())), &fs), S_OK);
    QHash<QByteArray, UInt32> items = List(fs);
    QVERIFY(items.contains("outer.zip") && items.contains("outer.tar"));

    CArchiveFolderManager *managerSpec = new CArchiveFolderManager;
    CMyComPtr<IFolderManager> manager = managerSpec;

    // -> outer.zip
    CMyComPtr<IFolderFolder> zip;
    const UString zipPath = fs2us(FString(QFile::encodeName(dir + "outer.zip").constData()));
    QCOMPARE(manager->OpenFolderFile(NULL, zipPath, L"", &zip, NULL), S_OK);
    QVERIFY(zip);
    QCOMPARE(GetFolderType(zip), UString(L"7-Zip.zip"));
    items = List(zip);
    QCOMPARE(QSet<QByteArray>(items.keyBegin(), items.keyEnd()), QSet<QByteArray>({ "docs", "inner.7z" }));
    QVERIFY(GetBool(zip, items["docs"], kpidIsDir));
    CMyComPtr<IFolderFolder> docs;
    QCOMPARE(zip->BindToFolder(items["docs"], &docs), S_OK);
    QHash<QByteArray, UInt32> docItems = List(docs);
    QVERIFY(docItems.contains("readme.txt"));
    QCOMPARE(GetUInt64(docs, docItems["readme.txt"], kpidSize), (UInt64)7);
    CMyComPtr<IFolderFolder> up;
    QCOMPARE(docs->BindToParentFolder(&up), S_OK);
    QVERIFY(up);
    QVERIFY(List(up).contains("inner.7z"));

    // zip items have no stream interface, so the panel extracts inner.7z
    // first (IFolderOperations::CopyTo) and opens the file
    CMyComPtr<IInArchiveGetStream> zipGetStream;
    zip.QueryInterface(IID_IInArchiveGetStream, &zipGetStream);
    CMyComPtr<ISequentialInStream> noStream;
    if (zipGetStream)
      zipGetStream->GetStream(items["inner.7z"], &noStream);
    QVERIFY(!noStream);
    CMyComPtr<IFolderOperations> zipOps;
    zip.QueryInterface(IID_IFolderOperations, &zipOps);
    QVERIFY(zipOps);
    CExtractStub *extractSpec = new CExtractStub;
    CMyComPtr<IFolderOperationsExtractCallback> extractCallback = extractSpec;
    const UInt32 innerIndex = items["inner.7z"];
    const UString outPath = fs2us(FString(QFile::encodeName(out).constData()));
    QCOMPARE(zipOps->CopyTo(0, &innerIndex, 1, 0, 0, outPath, extractCallback), S_OK);
    const UInt32 docsIndex = items["docs"];
    QCOMPARE(zipOps->CopyTo(0, &docsIndex, 1, 0, 0, outPath, extractCallback), S_OK);
    QCOMPARE(extractSpec->NumErrors, 0u);
    QCOMPARE(extractSpec->NumFiles, 2u);
    QFile readme(out + "docs/readme.txt");
    QVERIFY(readme.open(QIODevice::ReadOnly));
    QCOMPARE(readme.readAll(), QByteArray("read me"));
    CMyComPtr<IFolderFolder> innerFromZip;
    QCOMPARE(manager->OpenFolderFile(NULL, outPath + L"inner.7z", L"", &innerFromZip, NULL), S_OK);
    CheckInner7z(innerFromZip);

    // -> outer.tar -> inner.7z, opened from the stream without temporary files
    CMyComPtr<IFolderFolder> tar;
    const UString tarPath = fs2us(FString(QFile::encodeName(dir + "outer.tar").constData()));
    QCOMPARE(manager->OpenFolderFile(NULL, tarPath, L"", &tar, NULL), S_OK);
    QCOMPARE(GetFolderType(tar), UString(L"7-Zip.tar"));
    items = List(tar);
    QVERIFY(items.contains("inner.7z"));
    CMyComPtr<IInArchiveGetStream> getStream;
    tar.QueryInterface(IID_IInArchiveGetStream, &getStream);
    QVERIFY(getStream);
    CMyComPtr<ISequentialInStream> seqStream;
    QCOMPARE(getStream->GetStream(items["inner.7z"], &seqStream), S_OK);
    QVERIFY(seqStream);
    CMyComPtr<IInStream> inStream;
    seqStream.QueryInterface(IID_IInStream, &inStream);
    QVERIFY(inStream);
    CMyComPtr<IFolderFolder> innerFromTar;
    QCOMPARE(manager->OpenFolderFile(inStream, L"inner.7z", L"", &innerFromTar, NULL), S_OK);
    CheckInner7z(innerFromTar);
  }
};

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);
  CFoldersTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_folders.moc"
