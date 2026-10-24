// FSFolderPosix.cpp

#include "Win32Compat.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../CPP/Common/ComTry.h"
#include "../../CPP/Common/IntToString.h"
#include "../../CPP/Common/StringConvert.h"
#include "../../CPP/Common/UTFConvert.h"

#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/FileFind.h"
#include "../../CPP/Windows/FileIO.h"
#include "../../CPP/Windows/FileName.h"
#include "../../CPP/Windows/PropVariant.h"

#include "../../CPP/7zip/PropID.h"

#include "FSFolderPosix.h"

using namespace NWindows;
using namespace NFile;

namespace NFsFolder {

static const Byte kProps[] =
{
  kpidName,
  kpidSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidChangeTime,
  kpidPosixAttrib,
  kpidAttrib,
  kpidUser,
  kpidGroup,
  kpidSymLink,
  kpidPackSize,
  kpidINode,
  kpidLinks,
  kpidComment,
  kpidNumSubDirs,
  kpidNumSubFiles,
  kpidPrefix // only in flat mode
};

UInt32 CDirItem::GetWinAttrib() const
{
  // a link gets the type of its target, as on Windows
  mode_t mode = Mode;
  if (IsLink)
    mode = (mode_t)((mode & ~(mode_t)S_IFMT) | (IsDir ? S_IFDIR : S_IFREG));
  UInt32 attrib = NFind::Get_WinAttribPosix_From_PosixMode(mode);
  if (IsLink)
    attrib |= FILE_ATTRIBUTE_REPARSE_POINT;
  if (!Name.IsEmpty() && Name[0] == '.')
    attrib |= FILE_ATTRIBUTE_HIDDEN;
  return attrib;
}


// ---------- stat ----------

struct CStat
{
  mode_t Mode;
  uid_t Uid;
  gid_t Gid;
  UInt32 NumLinks;
  UInt64 INode;
  UInt64 Size;
  UInt64 Blocks;
  bool BTime_Defined;
  CFiTime MTime;
  CFiTime ATime;
  CFiTime BTime;
  CFiTime ChangeTime;
};

static void SetTime(CFiTime &t, const struct statx_timestamp &ts)
{
  t.tv_sec = (time_t)ts.tv_sec;
  t.tv_nsec = (long)ts.tv_nsec;
}

// statx() with a fallback to fstatat() where statx is blocked (old seccomp rules).
static bool StatAt(int dirFd, const char *name, bool followLink, CStat &st)
{
  struct statx x;
  const int flags = followLink ? 0 : AT_SYMLINK_NOFOLLOW;
  if (statx(dirFd, name, flags, STATX_BASIC_STATS | STATX_BTIME, &x) == 0)
  {
    st.Mode = x.stx_mode;
    st.Uid = x.stx_uid;
    st.Gid = x.stx_gid;
    st.NumLinks = x.stx_nlink;
    st.INode = x.stx_ino;
    st.Size = x.stx_size;
    st.Blocks = x.stx_blocks;
    SetTime(st.MTime, x.stx_mtime);
    SetTime(st.ATime, x.stx_atime);
    SetTime(st.ChangeTime, x.stx_ctime);
    st.BTime_Defined = (x.stx_mask & STATX_BTIME) != 0;
    if (st.BTime_Defined)
      SetTime(st.BTime, x.stx_btime);
    return true;
  }
  if (errno != ENOSYS && errno != EPERM)
    return false;
  struct stat s;
  if (fstatat(dirFd, name, &s, flags) != 0)
    return false;
  st.Mode = s.st_mode;
  st.Uid = s.st_uid;
  st.Gid = s.st_gid;
  st.NumLinks = (UInt32)s.st_nlink;
  st.INode = s.st_ino;
  st.Size = (UInt64)s.st_size;
  st.Blocks = (UInt64)s.st_blocks;
  st.MTime = s.st_mtim;
  st.ATime = s.st_atim;
  st.ChangeTime = s.st_ctim;
  st.BTime_Defined = false;
  return true;
}

static bool ReadLinkAt(int dirFd, const char *name, FString &target)
{
  target.Empty();
  for (unsigned size = 256; size <= (1u << 20); size <<= 1)
  {
    char *buf = target.GetBuf(size);
    const ssize_t n = readlinkat(dirFd, name, buf, size);
    if (n < 0)
    {
      target.ReleaseBuf_SetEnd(0);
      return false;
    }
    if ((size_t)n < size)
    {
      target.ReleaseBuf_SetEnd((unsigned)n);
      return true;
    }
    target.ReleaseBuf_SetEnd(0);
  }
  return false;
}

// Fills (di) from the directory entry (name) of the open directory (dirFd).
static bool ReadItem(int dirFd, CDirItem &di)
{
  CStat st;
  if (!StatAt(dirFd, di.Name, false, st))
    return false;
  di.Mode = st.Mode;
  di.Uid = st.Uid;
  di.Gid = st.Gid;
  di.NumLinks = st.NumLinks;
  di.INode = st.INode;
  di.ChangeTime = st.ChangeTime;
  di.IsLink = S_ISLNK(st.Mode);
  di.LinkTarget.Empty();
  if (di.IsLink)
  {
    ReadLinkAt(dirFd, di.Name, di.LinkTarget);
    CStat target;
    if (StatAt(dirFd, di.Name, true, target))
      st = target; // size, times and type of the target
  }
  di.IsDir = S_ISDIR(st.Mode);
  di.Size = di.IsDir ? 0 : st.Size;
  di.PackSize = st.Blocks * 512;
  di.MTime = st.MTime;
  di.ATime = st.ATime;
  di.CTime_Defined = st.BTime_Defined;
  if (st.BTime_Defined)
    di.CTime = st.BTime;
  di.FolderStat_Defined = false;
  di.NumFolders = 0;
  di.NumFiles = 0;
  return true;
}

static bool IsDots(const char *name)
{
  return name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0));
}


HRESULT CFsFolderStat::Enumerate()
{
  if (Progress)
  {
    RINOK(Progress->SetCompleted(NULL))
  }
  DIR *dir = opendir(Path);
  if (!dir)
    return S_OK; // unreadable directories are skipped, as on Windows
  Path.Add_PathSepar();
  const unsigned len = Path.Len();
  HRESULT res = S_OK;
  for (;;)
  {
    const struct dirent *de = readdir(dir);
    if (!de)
      break;
    if (IsDots(de->d_name))
      continue;
    struct stat st;
    if (fstatat(dirfd(dir), de->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0)
      continue;
    if (S_ISDIR(st.st_mode))
    {
      NumFolders++;
      Path.DeleteFrom(len);
      Path += de->d_name;
      res = Enumerate();
      if (res != S_OK)
        break;
    }
    else
    {
      NumFiles++;
      Size += (UInt64)st.st_size;
    }
  }
  closedir(dir);
  Path.DeleteFrom(len);
  return res;
}


HRESULT CFSFolder::Init(const FString &path)
{
  _path = path;
  DIR *dir = opendir(_path.IsEmpty() ? "." : (const char *)_path);
  if (!dir)
    return GetLastError_noZero_HRESULT();
  closedir(dir);
  _watcher.Start(_path);
  return S_OK;
}

HRESULT CFSFolder::LoadSubItems(int dirItem, const FString &relPrefix)
{
  const unsigned startIndex = Folders.Size();
  DIR *dir = opendir(_path + relPrefix);
  if (!dir)
  {
    // the top folder must be readable; subfolders of a flat view may not be
    return dirItem < 0 ? GetLastError_noZero_HRESULT() : S_OK;
  }
  CDirItem di;
  di.Parent = dirItem;
  for (;;)
  {
    errno = 0;
    const struct dirent *de = readdir(dir);
    if (!de)
      break;
    if (IsDots(de->d_name))
      continue;
    di.Name = de->d_name;
    if (!ReadItem(dirfd(dir), di))
      continue; // removed in the meantime
    // flat mode does not follow links to directories (they can make loops)
    if (_flatMode && di.IsDir && !di.IsLink)
      Folders.Add(relPrefix + di.Name + FCHAR_PATH_SEPARATOR);
    Files.Add(di);
  }
  closedir(dir);
  if (!_flatMode)
    return S_OK;
  const unsigned endIndex = Folders.Size();
  for (unsigned i = startIndex; i < endIndex; i++)
  {
    RINOK(LoadSubItems((int)i, Folders[i]))
  }
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::LoadItems())
{
  COM_TRY_BEGIN
  _watcher.WasChanged(); // the new list includes the changes so far
  Clear();
  RINOK(LoadSubItems(-1, FString()))
  _commentsAreLoaded = false;
  return S_OK;
  COM_TRY_END
}

static CFSTR const kDescriptionFileName = FTEXT("descript.ion");

bool CFSFolder::LoadComments()
{
  _comments.Clear();
  _commentsAreLoaded = true;
  NIO::CInFile file;
  if (!file.Open(_path + kDescriptionFileName))
    return false;
  UInt64 len;
  if (!file.GetLength(len) || len >= (1 << 28))
    return false;
  AString s;
  char *p = s.GetBuf((unsigned)(size_t)len);
  size_t processedSize;
  const bool ok = file.ReadFull(p, (size_t)len, processedSize);
  s.ReleaseBuf_CalcLen((unsigned)(size_t)len);
  if (!ok || processedSize != len)
    return false;
  UString u;
  if (!ConvertUTF8ToUnicode(s, u))
    return false;
  return _comments.ReadFromString(u);
}

bool CFSFolder::SaveComments()
{
  AString utf;
  {
    UString u;
    _comments.SaveToString(u);
    ConvertUnicodeToUTF8(u, utf);
  }
  if (!utf.IsAscii())
    utf.Insert(0, "\xEF\xBB\xBF" "\r\n");
  NIO::COutFile file;
  if (!file.Create_ALWAYS(_path + kDescriptionFileName))
    return false;
  const bool ok = file.WriteFull(utf, utf.Len());
  _commentsAreLoaded = false;
  return file.Close() && ok;
}

Z7_COM7F_IMF(CFSFolder::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Files.Size();
  return S_OK;
}

static void AddIdName(CRecordVector<UInt32> &ids, UStringVector &names, UInt32 id, const char *name)
{
  ids.Add(id);
  if (name)
    names.Add(fs2us(FString(name)));
  else
  {
    UString s;
    s.Add_UInt32(id);
    names.Add(s);
  }
}

const UString &CFSFolder::GetUserName(uid_t uid)
{
  FOR_VECTOR (i, _userIds)
    if (_userIds[i] == (UInt32)uid)
      return _userNames[i];
  char buf[4096];
  struct passwd pw, *res = NULL;
  getpwuid_r(uid, &pw, buf, sizeof(buf), &res);
  AddIdName(_userIds, _userNames, (UInt32)uid, res ? res->pw_name : NULL);
  return _userNames.Back();
}

const UString &CFSFolder::GetGroupName(gid_t gid)
{
  FOR_VECTOR (i, _groupIds)
    if (_groupIds[i] == (UInt32)gid)
      return _groupNames[i];
  char buf[4096];
  struct group gr, *res = NULL;
  getgrgid_r(gid, &gr, buf, sizeof(buf), &res);
  AddIdName(_groupIds, _groupNames, (UInt32)gid, res ? res->gr_name : NULL);
  return _groupNames.Back();
}

Z7_COM7F_IMF(CFSFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (index >= Files.Size())
    return E_INVALIDARG;
  const CDirItem &fi = Files[index];
  switch (propID)
  {
    case kpidIsDir: prop = fi.IsDir; break;
    case kpidIsAltStream: prop = false; break;
    case kpidName: prop = fs2us(fi.Name); break;
    case kpidSize: if (!fi.IsDir || fi.FolderStat_Defined) prop = fi.Size; break;
    case kpidPackSize: if (!fi.IsDir) prop = fi.PackSize; break;
    case kpidMTime: PropVariant_SetFrom_FiTime(prop, fi.MTime); break;
    case kpidATime: PropVariant_SetFrom_FiTime(prop, fi.ATime); break;
    case kpidCTime: if (fi.CTime_Defined) PropVariant_SetFrom_FiTime(prop, fi.CTime); break;
    case kpidChangeTime: PropVariant_SetFrom_FiTime(prop, fi.ChangeTime); break;
    case kpidPosixAttrib: prop = (UInt32)fi.Mode; break;
    case kpidAttrib: prop = fi.GetWinAttrib(); break;
    case kpidUser: prop = GetUserName(fi.Uid); break;
    case kpidGroup: prop = GetGroupName(fi.Gid); break;
    case kpidSymLink: if (fi.IsLink) prop = fs2us(fi.LinkTarget); break;
    case kpidINode: prop = fi.INode; break;
    case kpidLinks: prop = fi.NumLinks; break;
    case kpidComment:
    {
      if (!_commentsAreLoaded)
        LoadComments();
      UString comment;
      if (_comments.GetValue(fs2us(GetRelPath(fi)), comment))
      {
        const int pos = comment.Find((wchar_t)4);
        if (pos >= 0)
          comment.DeleteFrom((unsigned)pos);
        prop = comment;
      }
      break;
    }
    case kpidPrefix:
      if (fi.Parent >= 0)
        prop = fs2us(Folders[fi.Parent]);
      break;
    case kpidNumSubDirs: if (fi.IsDir && fi.FolderStat_Defined) prop = fi.NumFolders; break;
    case kpidNumSubFiles: if (fi.IsDir && fi.FolderStat_Defined) prop = fi.NumFiles; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static int CompareFiTime(const CFiTime &t1, const CFiTime &t2)
{
  if (t1.tv_sec != t2.tv_sec)
    return t1.tv_sec < t2.tv_sec ? -1 : 1;
  return MyCompare(t1.tv_nsec, t2.tv_nsec);
}

// position of the extension, including the '.'
static UString GetExtension(const FString &name)
{
  const int dotPos = name.ReverseFind_Dot();
  return fs2us(FString(name.Ptr(dotPos < 0 ? name.Len() : (unsigned)dotPos)));
}

Z7_COM7F_IMF2(Int32, CFSFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 /* propIsRaw */))
{
  const CDirItem &fi1 = Files[index1];
  const CDirItem &fi2 = Files[index2];
  switch (propID)
  {
    case kpidName:
      return CompareFileNames_ForFolderList(fs2us(fi1.Name), fs2us(fi2.Name));
    case kpidSize: return MyCompare(fi1.Size, fi2.Size);
    case kpidPackSize: return MyCompare(fi1.PackSize, fi2.PackSize);
    case kpidMTime: return CompareFiTime(fi1.MTime, fi2.MTime);
    case kpidATime: return CompareFiTime(fi1.ATime, fi2.ATime);
    case kpidChangeTime: return CompareFiTime(fi1.ChangeTime, fi2.ChangeTime);
    case kpidCTime:
      if (fi1.CTime_Defined != fi2.CTime_Defined)
        return fi1.CTime_Defined ? 1 : -1;
      return fi1.CTime_Defined ? CompareFiTime(fi1.CTime, fi2.CTime) : 0;
    case kpidAttrib: return MyCompare(fi1.GetWinAttrib(), fi2.GetWinAttrib());
    case kpidPosixAttrib: return MyCompare((UInt32)fi1.Mode, (UInt32)fi2.Mode);
    case kpidIsDir:
      if (fi1.IsDir == fi2.IsDir)
        return 0;
      return fi1.IsDir ? -1 : 1;
    case kpidINode: return MyCompare(fi1.INode, fi2.INode);
    case kpidLinks: return MyCompare(fi1.NumLinks, fi2.NumLinks);
    case kpidUser: return MyStringCompareNoCase(GetUserName(fi1.Uid), GetUserName(fi2.Uid));
    case kpidGroup: return MyStringCompareNoCase(GetGroupName(fi1.Gid), GetGroupName(fi2.Gid));
    case kpidSymLink:
      return CompareFileNames_ForFolderList(fs2us(fi1.LinkTarget), fs2us(fi2.LinkTarget));
    case kpidComment:
    {
      if (!_commentsAreLoaded)
        LoadComments();
      UString comment1, comment2;
      _comments.GetValue(fs2us(GetRelPath(fi1)), comment1);
      _comments.GetValue(fs2us(GetRelPath(fi2)), comment2);
      return MyStringCompareNoCase(comment1, comment2);
    }
    case kpidPrefix:
      if (fi1.Parent == fi2.Parent)
        return 0;
      if (fi1.Parent < 0) return -1;
      if (fi2.Parent < 0) return 1;
      return CompareFileNames_ForFolderList(
          fs2us(Folders[fi1.Parent]),
          fs2us(Folders[fi2.Parent]));
    case kpidExtension:
      return CompareFileNames_ForFolderList(GetExtension(fi1.Name), GetExtension(fi2.Name));
  }
  return 0;
}

HRESULT CFSFolder::BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  CFSFolder *folderSpec = new CFSFolder;
  CMyComPtr<IFolderFolder> subFolder = folderSpec;
  RINOK(folderSpec->Init(_path + name + FCHAR_PATH_SEPARATOR))
  *resultFolder = subFolder.Detach();
  return S_OK;
}

FString CFSFolder::GetRelPath(const CDirItem &item) const
{
  if (item.Parent < 0)
    return item.Name;
  return Folders[item.Parent] + item.Name;
}

Z7_COM7F_IMF(CFSFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  if (index >= Files.Size())
    return E_INVALIDARG;
  const CDirItem &fi = Files[index];
  if (!fi.IsDir)
    return E_INVALIDARG;
  return BindToFolderSpec(GetRelPath(fi), resultFolder);
}

Z7_COM7F_IMF(CFSFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  return BindToFolderSpec(us2fs(name), resultFolder);
}

/* The parent of "/a/b/" is "/a/". The root directory has no parent folder:
   the panel shows the root folder (mount points) then. */
Z7_COM7F_IMF(CFSFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  if (_path.IsEmpty())
    return E_INVALIDARG;
  int pos = _path.ReverseFind_PathSepar();
  if (pos != (int)_path.Len() - 1)
    return E_FAIL;
  FString parentPath = _path.Left((unsigned)pos);
  pos = parentPath.ReverseFind_PathSepar();
  if (pos < 0)
    return S_OK;
  parentPath.DeleteFrom((unsigned)(pos + 1));
  CFSFolder *parentFolderSpec = new CFSFolder;
  CMyComPtr<IFolderFolder> parentFolder = parentFolderSpec;
  if (parentFolderSpec->Init(parentPath) == S_OK)
    *resultFolder = parentFolder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::GetNumberOfProperties(UInt32 *numProperties))
{
  *numProperties = Z7_ARRAY_SIZE(kProps);
  if (!_flatMode)
    (*numProperties)--;
  return S_OK;
}

IMP_IFolderFolder_GetProp(CFSFolder::GetPropertyInfo, kProps)

Z7_COM7F_IMF(CFSFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "FSFolder"; break;
    case kpidPath: prop = fs2us(_path); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFSFolder::WasChanged(Int32 *wasChanged))
{
  *wasChanged = BoolToInt(_watcher.WasChanged());
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::Clone(IFolderFolder **resultFolder))
{
  CFSFolder *fsFolderSpec = new CFSFolder;
  CMyComPtr<IFolderFolder> folderNew = fsFolderSpec;
  fsFolderSpec->_flatMode = _flatMode;
  RINOK(fsFolderSpec->Init(_path))
  *resultFolder = folderNew.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::CalcItemFullSize(UInt32 index, IProgress *progress))
{
  if (index >= Files.Size())
    return S_OK;
  CDirItem &fi = Files[index];
  if (!fi.IsDir || fi.IsLink)
    return S_OK;
  CFsFolderStat stat(_path + GetRelPath(fi), progress);
  RINOK(stat.Enumerate())
  fi.Size = stat.Size;
  fi.NumFolders = stat.NumFolders;
  fi.NumFiles = stat.NumFiles;
  fi.FolderStat_Defined = true;
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::SetFlatMode(Int32 flatMode))
{
  _flatMode = IntToBool(flatMode);
  return S_OK;
}


// ---------- IFolderOperations ----------

void CFSFolder::GetAbsPath(const wchar_t *name, FString &absPath)
{
  absPath.Empty();
  if (!NName::IsAbsolutePath(name))
    absPath += _path;
  absPath += us2fs(name);
}

Z7_COM7F_IMF(CFSFolder::CreateFolder(const wchar_t *name, IProgress * /* progress */))
{
  FString absPath;
  GetAbsPath(name, absPath);
  if (NDir::CreateDir(absPath))
    return S_OK;
  if (errno != EEXIST && NDir::CreateComplexDir(absPath))
    return S_OK;
  return GetLastError_noZero_HRESULT();
}

Z7_COM7F_IMF(CFSFolder::CreateFile(const wchar_t *name, IProgress * /* progress */))
{
  FString absPath;
  GetAbsPath(name, absPath);
  NIO::COutFile outFile;
  if (!outFile.Create_NEW(absPath))
    return GetLastError_noZero_HRESULT();
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::Rename(UInt32 index, const wchar_t *newName, IProgress * /* progress */))
{
  if (index >= Files.Size())
    return E_NOTIMPL;
  const CDirItem &fi = Files[index];
  FString fullPrefix = _path;
  if (fi.Parent >= 0)
    fullPrefix += Folders[fi.Parent];
  const FString newPath = fullPrefix + us2fs(newName);
  // rename() would silently replace an existing file
  struct stat st;
  if (lstat(newPath, &st) == 0 && newPath != fullPrefix + fi.Name)
    return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
  if (rename(fullPrefix + fi.Name, newPath) != 0)
    return GetLastError_noZero_HRESULT();
  return S_OK;
}

bool RemoveTree(const FString &path)
{
  struct stat st;
  if (lstat(path, &st) != 0)
    return false;
  if (!S_ISDIR(st.st_mode))
    return unlink(path) == 0;
  DIR *dir = opendir(path);
  if (!dir)
    return false;
  bool ok = true;
  FStringVector names;
  for (;;)
  {
    const struct dirent *de = readdir(dir);
    if (!de)
      break;
    if (!IsDots(de->d_name))
      names.Add(FString(de->d_name));
  }
  closedir(dir);
  FOR_VECTOR (i, names)
  {
    FString sub = path;
    sub.Add_PathSepar();
    sub += names[i];
    if (!RemoveTree(sub))
      ok = false;
  }
  return ok && rmdir(path) == 0;
}

Z7_COM7F_IMF(CFSFolder::Delete(const UInt32 *indices, UInt32 numItems, IProgress *progress))
{
  if (progress)
  {
    RINOK(progress->SetTotal(numItems))
  }
  for (UInt32 i = 0; i < numItems; i++)
  {
    const UInt32 index = indices[i];
    if (index >= Files.Size())
      return E_INVALIDARG;
    if (!RemoveTree(_path + GetRelPath(Files[index])))
      return GetLastError_noZero_HRESULT();
    if (progress)
    {
      const UInt64 completed = i + 1;
      RINOK(progress->SetCompleted(&completed))
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CFSFolder::CopyTo(Int32 /* moveMode */, const UInt32 * /* indices */, UInt32 /* numItems */,
    Int32 /* includeAltStreams */, Int32 /* replaceAltStreamCharsMode */,
    const wchar_t * /* path */, IFolderOperationsExtractCallback * /* callback */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSFolder::CopyFrom(Int32 /* moveMode */, const wchar_t * /* fromFolderPath */,
    const wchar_t * const * /* itemsPaths */, UInt32 /* numItems */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSFolder::CopyFromFile(UInt32 /* index */, const wchar_t * /* fullFilePath */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSFolder::SetProperty(UInt32 index, PROPID propID,
    const PROPVARIANT *value, IProgress * /* progress */))
{
  if (index >= Files.Size())
    return E_INVALIDARG;
  const CDirItem &fi = Files[index];
  if (fi.Parent >= 0)
    return E_NOTIMPL;
  if (propID != kpidComment)
    return E_NOTIMPL;
  if (!_commentsAreLoaded)
    LoadComments();
  UString filename = fs2us(fi.Name);
  filename.Trim();
  if (value->vt == VT_EMPTY)
    _comments.DeletePair(filename);
  else if (value->vt == VT_BSTR)
  {
    CTextPair pair;
    pair.ID = filename;
    pair.Value.SetFromBstr(value->bstrVal);
    pair.Value.Trim();
    if (pair.Value.IsEmpty())
      _comments.DeletePair(filename);
    else
      _comments.AddPair(pair);
  }
  else
    return E_INVALIDARG;
  if (!SaveComments())
    return GetLastError_noZero_HRESULT();
  return S_OK;
}

HRESULT CopyFileSystemItems(
    const UStringVector & /* itemsPaths */,
    const FString & /* destDirPrefix */,
    bool /* moveMode */,
    IFolderOperationsExtractCallback * /* callback */)
{
  return E_NOTIMPL;
}

}
