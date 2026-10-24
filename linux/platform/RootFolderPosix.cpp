// RootFolderPosix.cpp

#include "Win32Compat.h"

#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "../../CPP/Common/ComTry.h"
#include "../../CPP/Common/StringConvert.h"

#include "../../CPP/Windows/FileName.h"
#include "../../CPP/Windows/PropVariant.h"

#include "../../CPP/7zip/PropID.h"

#include "FSFolderPosix.h"
#include "RootFolderPosix.h"

using namespace NWindows;

namespace NRootFolder {

static const char * const kPseudoFileSystems[] =
{
  "autofs", "binfmt_misc", "bpf", "cgroup", "cgroup2", "configfs", "debugfs",
  "devpts", "devtmpfs", "efivarfs", "fusectl", "fuse.gvfsd-fuse", "fuse.portal",
  "hugetlbfs", "mqueue", "nsfs", "overlay", "proc", "pstore", "ramfs",
  "rpc_pipefs", "securityfs", "squashfs", "sysfs", "tmpfs", "tracefs"
};

static const char * const kSystemDirs[] =
{
  "/bin", "/boot", "/dev", "/efi", "/etc", "/lib", "/lib32", "/lib64", "/nix",
  "/opt", "/proc", "/root", "/run", "/sbin", "/snap", "/srv", "/sys", "/tmp",
  "/usr", "/var"
};

// mountinfo escapes ' ', '\t', '\n' and '\\' as \ooo
static AString Unescape(const AString &s)
{
  AString res;
  for (unsigned i = 0; i < s.Len(); i++)
  {
    const char c = s[i];
    if (c == '\\' && i + 3 < s.Len()
        && s[i + 1] >= '0' && s[i + 1] <= '3'
        && s[i + 2] >= '0' && s[i + 2] <= '7'
        && s[i + 3] >= '0' && s[i + 3] <= '7')
    {
      res.Add_Char((char)(((s[i + 1] - '0') << 6) | ((s[i + 2] - '0') << 3) | (s[i + 3] - '0')));
      i += 3;
    }
    else
      res.Add_Char(c);
  }
  return res;
}

static void SplitFields(const AString &line, AStringVector &fields)
{
  fields.Clear();
  AString f;
  for (unsigned i = 0; i <= line.Len(); i++)
  {
    const char c = (i < line.Len() ? line[i] : ' ');
    if (c == ' ')
    {
      if (!f.IsEmpty())
        fields.Add(f);
      f.Empty();
    }
    else
      f.Add_Char(c);
  }
}

void ParseMountInfo(const AString &text, CObjectVector<CMountPoint> &mounts)
{
  mounts.Clear();
  unsigned pos = 0;
  while (pos < text.Len())
  {
    int lf = text.Find('\n', pos);
    if (lf < 0)
      lf = (int)text.Len();
    AString line;
    line.SetFrom(text.Ptr(pos), (unsigned)lf - pos);
    pos = (unsigned)lf + 1;
    // id parent major:minor root mountpoint options [optional...] - fstype source superoptions
    AStringVector f;
    SplitFields(line, f);
    if (f.Size() < 7)
      continue;
    unsigned sep = 6;
    while (sep < f.Size() && f[sep] != "-")
      sep++;
    if (sep + 2 >= f.Size())
      continue;
    CMountPoint mp;
    mp.Path = Unescape(f[4]);
    mp.FsType = f[sep + 1];
    mp.Source = Unescape(f[sep + 2]);
    FOR_VECTOR (i, mounts)
      if (mounts[i].Path == mp.Path)
      {
        mounts.Delete(i);
        break;
      }
    mounts.Add(mp);
  }
}

static bool IsPathUnder(const FString &path, const char *dir)
{
  if (!path.IsPrefixedBy(dir))
    return false;
  const char c = path[MyStringLen(dir)];
  return c == 0 || c == '/';
}

bool IsUserMountPoint(const CMountPoint &mp)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kPseudoFileSystems); i++)
    if (mp.FsType == kPseudoFileSystems[i])
      return false;
  if (mp.Path.IsEmpty() || mp.Path[0] != '/')
    return false;
  if (IsPathUnder(mp.Path, "/run/media"))
    return mp.Path.Len() > MyStringLen("/run/media");
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kSystemDirs); i++)
    if (IsPathUnder(mp.Path, kSystemDirs[i]))
      return false;
  return true;
}

static FString GetHomeDir()
{
  const char *home = getenv("HOME");
  if (!home || home[0] != '/')
  {
    const struct passwd *pw = getpwuid(getuid());
    home = (pw && pw->pw_dir && pw->pw_dir[0] == '/') ? pw->pw_dir : NULL;
  }
  FString s (home ? home : "/");
  while (s.Len() > 1 && s.Back() == '/')
    s.DeleteBack();
  return s;
}

static bool ReadFileText(const char *path, AString &text)
{
  text.Empty();
  const int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return false;
  char buf[4096];
  for (;;)
  {
    const ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0)
      break;
    for (ssize_t i = 0; i < n; i++)
      if (buf[i] != 0)
        text.Add_Char(buf[i]);
  }
  close(fd);
  return true;
}

}

using namespace NRootFolder;

static const Byte kProps[] =
{
  kpidName,
  kpidTotalSize,
  kpidFreeSpace,
  kpidFileSystem,
  kpidVolumeName
};

void CRootFolder::Init(const AString *mountInfo)
{
  _items.Clear();
  CObjectVector<CMountPoint> mounts;
  {
    AString text;
    if (mountInfo)
      text = *mountInfo;
    else
      ReadFileText("/proc/self/mountinfo", text);
    ParseMountInfo(text, mounts);
  }

  FStringVector paths;
  paths.Add(GetHomeDir());
  paths.Add(FString("/"));
  FOR_VECTOR (i, mounts)
    if (IsUserMountPoint(mounts[i]))
      paths.Add(mounts[i].Path);

  FOR_VECTOR (i, paths)
  {
    const FString &path = paths[i];
    bool dup = false;
    FOR_VECTOR (k, _items)
      if (_items[k].Path == path)
        dup = true;
    if (dup)
      continue;
    CItem &item = _items.AddNew();
    item.Path = path;
    item.Name = fs2us(path);
    item.SizeWasRequested = false;
    item.SizeDefined = false;
    item.TotalSize = 0;
    item.FreeSpace = 0;
    // the file system that contains the path: the longest mount point prefix
    unsigned bestLen = 0;
    FOR_VECTOR (k, mounts)
    {
      const CMountPoint &mp = mounts[k];
      const unsigned len = mp.Path.Len();
      if ((mp.Path == "/" || IsPathUnder(path, mp.Path)) && len >= bestLen)
      {
        bestLen = len;
        item.FsType = mp.FsType;
        item.Source = mp.Source;
      }
    }
  }
}

void CRootFolder::ReadSize(CItem &item)
{
  item.SizeWasRequested = true;
  struct statvfs st;
  if (statvfs(item.Path, &st) != 0)
    return;
  item.TotalSize = (UInt64)st.f_blocks * st.f_frsize;
  item.FreeSpace = (UInt64)st.f_bavail * st.f_frsize; // what the user can use
  item.SizeDefined = true;
}

Z7_COM7F_IMF(CRootFolder::LoadItems())
{
  COM_TRY_BEGIN
  Init();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::GetProperty(UInt32 itemIndex, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  if (itemIndex >= _items.Size())
    return E_INVALIDARG;
  CItem &item = _items[itemIndex];
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidIsDir: prop = true; break;
    case kpidName: prop = item.Name; break;
    case kpidTotalSize:
    case kpidFreeSpace:
      if (!item.SizeWasRequested)
        ReadSize(item);
      if (item.SizeDefined)
        prop = (propID == kpidTotalSize ? item.TotalSize : item.FreeSpace);
      break;
    case kpidFileSystem: if (!item.FsType.IsEmpty()) prop = GetUnicodeString(item.FsType); break;
    case kpidVolumeName: if (!item.Source.IsEmpty()) prop = fs2us(item.Source); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  if (index >= _items.Size())
    return E_INVALIDARG;
  FString path = _items[index].Path;
  NFile::NName::NormalizeDirPathPrefix(path);
  NFsFolder::CFSFolder *fsFolderSpec = new NFsFolder::CFSFolder;
  CMyComPtr<IFolderFolder> subFolder = fsFolderSpec;
  RINOK(fsFolderSpec->Init(path))
  *resultFolder = subFolder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  *resultFolder = NULL;
  UString name2 = name;
  name2.Trim();
  if (name2.IsEmpty())
  {
    CRootFolder *rootFolderSpec = new CRootFolder;
    CMyComPtr<IFolderFolder> rootFolder = rootFolderSpec;
    rootFolderSpec->Init();
    *resultFolder = rootFolder.Detach();
    return S_OK;
  }
  // "~" and "~/..." are in the home directory
  if (name2[0] == '~' && (name2.Len() == 1 || name2[1] == '/'))
    name2 = fs2us(GetHomeDir()) + name2.Ptr(1);
  if (name2[0] != '/')
    return E_INVALIDARG;
  FString path = us2fs(name2);
  NFile::NName::NormalizeDirPathPrefix(path);
  NFsFolder::CFSFolder *fsFolderSpec = new NFsFolder::CFSFolder;
  CMyComPtr<IFolderFolder> subFolder = fsFolderSpec;
  if (fsFolderSpec->Init(path) != S_OK)
    return E_INVALIDARG;
  *resultFolder = subFolder.Detach();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  return S_OK;
}

IMP_IFolderFolder_Props(CRootFolder)

Z7_COM7F_IMF(CRootFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "RootFolder"; break;
    case kpidPath: prop = ""; break;
  }
  prop.Detach(value);
  return S_OK;
}
