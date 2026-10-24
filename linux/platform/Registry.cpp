// Registry.cpp -- NWindows::NRegistry::CKey for POSIX, backed by an INI file.
//
// Windows/Registry.h is used unchanged. HKEY_CURRENT_USER is stored in
// $XDG_CONFIG_HOME/7-Zip/7-Zip.ini (see RegistryIni.h); the other predefined
// keys are empty and read-only. The file looks like a .reg file:
//
//   [Software/7-Zip/FM]
//   "ShowDots"=dword:00000001
//   "Lang"="pl"
//   "Columns"=hex:01,00,00,00
//   "PathHistory"=multi:"/home/user","/tmp"
//
// Sections are key paths. Strings are UTF-8 with the escapes \\ \" \] \n \r
// \t and \xHH (for control characters and for bytes that are not valid UTF-8,
// so any file name survives a round trip). Key and value names are compared
// case-insensitively, as on Windows. Other value types are written as
// hex(N):... The file is rewritten as a whole, so comments are not kept.
//
// 7zFM and 7zG can change settings at the same time. Changes are applied in
// memory at once and kept in a list of pending operations. When the last open
// CKey closes, the file is locked (flock on "7-Zip.ini.lock"), reloaded if
// another process has replaced it since, the pending operations are replayed
// on top, and the result is written to a temporary file that replaces the INI
// file through rename(). Readers never lock: they see either the old or the
// new file, and they reload it whenever its inode, size or time changes.

#include "Win32Compat.h"

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../C/CpuArch.h"

#include "../../CPP/Common/IntToString.h"
#include "../../CPP/Common/MyBuffer.h"
#include "../../CPP/Common/UTFConvert.h"

#include "../../CPP/Windows/Registry.h"
#include "../../CPP/Windows/Synchronization.h"

#include "RegistryIni.h"

struct HKEY__
{
  AStringVector Path;
};

namespace NWindows {
namespace NRegistry {
namespace NIni {

struct CValue
{
  AString Name;
  DWORD Type;
  CByteBuffer Data;

  CValue(): Type(REG_NONE) {}
};

static bool IsAscii(const AString &s)
{
  for (unsigned i = 0; i < s.Len(); i++)
    if ((Byte)s[i] >= 0x80)
      return false;
  return true;
}

static bool NamesAreEqual(const AString &a, const AString &b)
{
  if (IsAscii(a) && IsAscii(b))
    return StringsAreEqualNoCase_Ascii(a, b);
  UString ua, ub;
  ConvertUTF8ToUnicode(a, ua);
  ConvertUTF8ToUnicode(b, ub);
  return ua.IsEqualTo_NoCase(ub);
}

struct CNode
{
  AString Name;
  CObjectVector<CValue> Values;
  CObjectVector<CNode> SubKeys;

  int FindValue(const AString &name) const
  {
    FOR_VECTOR (i, Values)
      if (NamesAreEqual(Values[i].Name, name))
        return (int)i;
    return -1;
  }

  int FindSubKey(const AString &name) const
  {
    FOR_VECTOR (i, SubKeys)
      if (NamesAreEqual(SubKeys[i].Name, name))
        return (int)i;
    return -1;
  }

  // An existing value keeps the spelling of its name, as on Windows.
  void SetValue(const CValue &v)
  {
    const int index = FindValue(v.Name);
    if (index < 0)
    {
      Values.Add(v);
      return;
    }
    CValue &dest = Values[(unsigned)index];
    dest.Type = v.Type;
    dest.Data = v.Data;
  }
};

enum EOpType
{
  kOp_CreateKey,
  kOp_DeleteKey,
  kOp_SetValue,
  kOp_DeleteValue
};

struct COp
{
  EOpType Type;
  AStringVector Path;
  CValue Value; // kOp_DeleteValue uses only Value.Name
};

static CNode *FindNode(CNode &root, const AStringVector &path)
{
  CNode *node = &root;
  FOR_VECTOR (i, path)
  {
    const int index = node->FindSubKey(path[i]);
    if (index < 0)
      return NULL;
    node = &node->SubKeys[(unsigned)index];
  }
  return node;
}

static CNode &CreateNode(CNode &root, const AStringVector &path)
{
  CNode *node = &root;
  FOR_VECTOR (i, path)
  {
    const int index = node->FindSubKey(path[i]);
    if (index >= 0)
      node = &node->SubKeys[(unsigned)index];
    else
    {
      node = &node->SubKeys.AddNew();
      node->Name = path[i];
    }
  }
  return *node;
}

static void ApplyOp(CNode &root, const COp &op)
{
  switch (op.Type)
  {
    case kOp_CreateKey:
      CreateNode(root, op.Path);
      break;
    case kOp_DeleteKey:
    {
      AStringVector parentPath = op.Path;
      parentPath.DeleteBack();
      CNode *parent = FindNode(root, parentPath);
      if (parent)
      {
        const int index = parent->FindSubKey(op.Path.Back());
        if (index >= 0)
          parent->SubKeys.Delete((unsigned)index);
      }
      break;
    }
    case kOp_SetValue:
      CreateNode(root, op.Path).SetValue(op.Value);
      break;
    case kOp_DeleteValue:
    {
      CNode *node = FindNode(root, op.Path);
      if (node)
      {
        const int index = node->FindValue(op.Value.Name);
        if (index >= 0)
          node->Values.Delete((unsigned)index);
      }
      break;
    }
  }
}


// ---------- Text format ----------

static void AddHexByte(AString &s, unsigned b)
{
  static const char * const kHexDigits = "0123456789abcdef";
  s.Add_Char(kHexDigits[(b >> 4) & 15]);
  s.Add_Char(kHexDigits[b & 15]);
}

// Returns the length of the valid UTF-8 sequence at (p), or 0.
static unsigned GetUtf8SeqLen(const Byte *p, size_t rem)
{
  const unsigned c = p[0];
  unsigned len;
  UInt32 v;
  if (c < 0xc2)
    return 0;
  if (c < 0xe0) { len = 2; v = c & 0x1f; }
  else if (c < 0xf0) { len = 3; v = c & 0x0f; }
  else if (c < 0xf5) { len = 4; v = c & 0x07; }
  else
    return 0;
  if (rem < len)
    return 0;
  for (unsigned i = 1; i < len; i++)
  {
    if ((p[i] & 0xc0) != 0x80)
      return 0;
    v = (v << 6) | (p[i] & 0x3f);
  }
  if ((len == 3 && (v < 0x800 || (v >= 0xd800 && v < 0xe000)))
      || (len == 4 && (v < 0x10000 || v > 0x10ffff)))
    return 0;
  return len;
}

static void AddEscaped(AString &s, const Byte *p, size_t size, char quote)
{
  for (size_t i = 0; i < size;)
  {
    const unsigned c = p[i];
    unsigned len = 1;
    if (c == '\\' || c == (Byte)quote)
    {
      s.Add_Char('\\');
      s.Add_Char((char)c);
    }
    else if (c == '\n') s += "\\n";
    else if (c == '\r') s += "\\r";
    else if (c == '\t') s += "\\t";
    else if (c >= 0x20 && c < 0x7f)
      s.Add_Char((char)c);
    else if (c >= 0x80 && (len = GetUtf8SeqLen(p + i, size - i)) != 0)
      s.AddFrom((const char *)(p + i), len);
    else
    {
      len = 1;
      s += "\\x";
      AddHexByte(s, c);
    }
    i += len;
  }
}

static void AddQuoted(AString &s, const Byte *p, size_t size)
{
  s.Add_Char('"');
  AddEscaped(s, p, size, '"');
  s.Add_Char('"');
}

static void AddHexList(AString &s, const CByteBuffer &data)
{
  for (size_t i = 0; i < data.Size(); i++)
  {
    if (i != 0)
      s.Add_Char(',');
    AddHexByte(s, data[i]);
  }
}

static void AddValueLine(AString &s, const CValue &v)
{
  if (v.Name.IsEmpty())
    s.Add_Char('@');
  else
    AddQuoted(s, (const Byte *)v.Name.Ptr(), v.Name.Len());
  s.Add_Char('=');
  const CByteBuffer &d = v.Data;
  char temp[16];
  if (v.Type == REG_SZ)
    AddQuoted(s, d, d.Size());
  else if (v.Type == REG_EXPAND_SZ)
  {
    s += "expand:";
    AddQuoted(s, d, d.Size());
  }
  else if (v.Type == REG_DWORD && d.Size() == 4)
  {
    s += "dword:";
    ConvertUInt32ToHex8Digits(GetUi32(d), temp);
    s += temp;
  }
  else if (v.Type == REG_QWORD && d.Size() == 8)
  {
    s += "qword:";
    ConvertUInt32ToHex8Digits(GetUi32(d + 4), temp);
    s += temp;
    ConvertUInt32ToHex8Digits(GetUi32(d), temp);
    s += temp;
  }
  else if (v.Type == REG_MULTI_SZ)
  {
    s += "multi:";
    size_t start = 0;
    for (size_t i = 0; i < d.Size(); i++)
      if (d[i] == 0)
      {
        if (start != 0)
          s.Add_Char(',');
        AddQuoted(s, d + start, i - start);
        start = i + 1;
      }
  }
  else if (v.Type == REG_BINARY)
  {
    s += "hex:";
    AddHexList(s, d);
  }
  else
  {
    s += "hex(";
    ConvertUInt32ToHex(v.Type, temp);
    s += temp;
    s += "):";
    AddHexList(s, d);
  }
  s.Add_LF();
}

static void AddNode(AString &s, const CNode &node, AString &path)
{
  const unsigned pathLen = path.Len();
  if (pathLen != 0 && (!node.Values.IsEmpty() || node.SubKeys.IsEmpty()))
  {
    s.Add_LF();
    s.Add_Char('[');
    s += path;
    s.Add_Char(']');
    s.Add_LF();
    FOR_VECTOR (i, node.Values)
      AddValueLine(s, node.Values[i]);
  }
  FOR_VECTOR (i, node.SubKeys)
  {
    const CNode &sub = node.SubKeys[i];
    if (pathLen != 0)
      path.Add_Slash();
    AddEscaped(path, (const Byte *)sub.Name.Ptr(), sub.Name.Len(), ']');
    AddNode(s, sub, path);
    path.DeleteFrom(pathLen);
  }
}

static void Serialize(const CNode &root, AString &s)
{
  s = "; 7-Zip settings. 7-Zip rewrites this file; comments are not kept.\n";
  AString path;
  AddNode(s, root, path);
}

static int HexValue(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void SkipSpaces(const char *&p)
{
  while (*p == ' ' || *p == '\t')
    p++;
}

/* Reads characters up to the unescaped (end) character and skips it.
   Zero bytes are rejected: names and strings never contain them. */
static bool ParseEscaped(const char *&p, char end, AString &dest)
{
  dest.Empty();
  for (;;)
  {
    char c = *p++;
    if (c == 0)
      return false;
    if (c == end)
      return true;
    if (c == '\\')
    {
      c = *p++;
      if (c == 'n') c = '\n';
      else if (c == 'r') c = '\r';
      else if (c == 't') c = '\t';
      else if (c == 'x')
      {
        const int h = HexValue(p[0]);
        const int l = (h < 0 ? -1 : HexValue(p[1]));
        if (l < 0)
          return false;
        p += 2;
        c = (char)((h << 4) | l);
      }
      if (c == 0)
        return false;
    }
    dest.Add_Char(c);
  }
}

static bool ParseHexList(const char *p, CByteBuffer &dest)
{
  CByteVector v;
  for (;;)
  {
    SkipSpaces(p);
    if (*p == 0)
      break;
    const int h = HexValue(p[0]);
    const int l = (h < 0 ? -1 : HexValue(p[1]));
    if (l < 0)
      return false;
    v.Add((Byte)((h << 4) | l));
    p += 2;
    SkipSpaces(p);
    if (*p == ',')
      p++;
    else if (*p != 0)
      return false;
  }
  dest.CopyFrom(v.ConstData(), v.Size());
  return true;
}

static bool ParseHexNumber(const char *p, unsigned maxDigits, UInt64 &v)
{
  v = 0;
  unsigned n = 0;
  for (; *p != 0; p++, n++)
  {
    const int d = HexValue(*p);
    if (d < 0 || n == maxDigits)
      return false;
    v = (v << 4) | (unsigned)d;
  }
  return n != 0;
}

static bool ParseQuotedData(const char *p, CByteBuffer &dest)
{
  if (*p++ != '"')
    return false;
  AString s;
  if (!ParseEscaped(p, '"', s) || *p != 0)
    return false;
  dest.CopyFrom((const Byte *)s.Ptr(), s.Len());
  return true;
}

static bool ParseData(const char *p, CValue &v)
{
  if (*p == '"')
  {
    v.Type = REG_SZ;
    return ParseQuotedData(p, v.Data);
  }
  if (IsString1PrefixedByString2(p, "expand:"))
  {
    v.Type = REG_EXPAND_SZ;
    return ParseQuotedData(p + 7, v.Data);
  }
  if (IsString1PrefixedByString2(p, "dword:"))
  {
    UInt64 n;
    if (!ParseHexNumber(p + 6, 8, n))
      return false;
    v.Type = REG_DWORD;
    v.Data.Alloc(4);
    SetUi32(v.Data, (UInt32)n)
    return true;
  }
  if (IsString1PrefixedByString2(p, "qword:"))
  {
    UInt64 n;
    if (!ParseHexNumber(p + 6, 16, n))
      return false;
    v.Type = REG_QWORD;
    v.Data.Alloc(8);
    SetUi64(v.Data, n)
    return true;
  }
  if (IsString1PrefixedByString2(p, "multi:"))
  {
    p += 6;
    CByteVector all;
    while (*p != 0)
    {
      if (*p++ != '"')
        return false;
      AString s;
      if (!ParseEscaped(p, '"', s))
        return false;
      for (unsigned i = 0; i < s.Len(); i++)
        all.Add((Byte)s[i]);
      all.Add(0);
      if (*p == ',')
        p++;
      else if (*p != 0)
        return false;
    }
    v.Type = REG_MULTI_SZ;
    v.Data.CopyFrom(all.ConstData(), all.Size());
    return true;
  }
  if (IsString1PrefixedByString2(p, "hex:"))
  {
    v.Type = REG_BINARY;
    return ParseHexList(p + 4, v.Data);
  }
  if (IsString1PrefixedByString2(p, "hex("))
  {
    p += 4;
    const char *close = strchr(p, ')');
    if (!close || close[1] != ':')
      return false;
    AString num;
    num.SetFrom(p, (unsigned)(close - p));
    UInt64 type;
    if (!ParseHexNumber(num, 8, type))
      return false;
    v.Type = (DWORD)type;
    return ParseHexList(close + 2, v.Data);
  }
  return false;
}

static bool ParseSectionLine(const char *p, AStringVector &path)
{
  path.Clear();
  if (*p++ != '[')
    return false;
  AString name;
  if (!ParseEscaped(p, ']', name))
    return false;
  SkipSpaces(p);
  if (*p != 0)
    return false;
  // for hand-edited files: "[ a/b ]" is "[a/b]"
  name.Trim();
  // '/' inside key names is impossible: CKey treats it as a separator
  AString part;
  for (unsigned i = 0;; i++)
  {
    const char c = name[i];
    if (c == 0 || c == '/')
    {
      if (!part.IsEmpty())
        path.Add(part);
      part.Empty();
      if (c == 0)
        break;
    }
    else
      part.Add_Char(c);
  }
  return !path.IsEmpty();
}

static bool ParseValueLine(const char *p, CValue &v)
{
  if (*p == '@')
    p++;
  else if (*p++ != '"' || !ParseEscaped(p, '"', v.Name))
    return false;
  SkipSpaces(p);
  if (*p++ != '=')
    return false;
  SkipSpaces(p);
  return ParseData(p, v);
}

// Lines that cannot be parsed are skipped.
static void Parse(const AString &text, CNode &root)
{
  root.Values.Clear();
  root.SubKeys.Clear();
  CNode *node = NULL;
  for (unsigned pos = 0; pos < text.Len();)
  {
    int lf = text.Find('\n', pos);
    if (lf < 0)
      lf = (int)text.Len();
    AString line;
    line.SetFrom(text.Ptr(pos), (unsigned)lf - pos);
    pos = (unsigned)lf + 1;
    // AString::Trim() keeps '\r', which files edited on Windows have
    line.Replace('\r', ' ');
    line.Trim();
    if (line.IsEmpty() || line[0] == ';' || line[0] == '#')
      continue;
    if (line[0] == '[')
    {
      AStringVector path;
      node = ParseSectionLine(line, path) ? &CreateNode(root, path) : NULL;
      continue;
    }
    CValue v;
    if (node && ParseValueLine(line, v))
      node->SetValue(v);
  }
}


// ---------- File store ----------

struct CFileStamp
{
  bool Exists;
  dev_t Dev;
  ino_t Ino;
  off_t Size;
  struct timespec MTime;

  CFileStamp(): Exists(false), Dev(0), Ino(0), Size(0)
  {
    MTime.tv_sec = 0;
    MTime.tv_nsec = 0;
  }

  void Set(const struct stat &st)
  {
    Exists = true;
    Dev = st.st_dev;
    Ino = st.st_ino;
    Size = st.st_size;
    MTime = st.st_mtim;
  }

  bool IsEqualTo(const CFileStamp &a) const
  {
    if (Exists != a.Exists)
      return false;
    return !Exists || (Dev == a.Dev && Ino == a.Ino && Size == a.Size
        && MTime.tv_sec == a.MTime.tv_sec && MTime.tv_nsec == a.MTime.tv_nsec);
  }
};

static bool ReadAll(int fd, AString &dest)
{
  dest.Empty();
  char buf[1 << 14];
  for (;;)
  {
    const ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (n == 0)
      return true;
    // zero bytes are invalid in the file; drop them so AString stays intact
    for (ssize_t i = 0; i < n; i++)
      if (buf[i] != 0)
        dest.Add_Char(buf[i]);
  }
}

static bool WriteAll(int fd, const char *p, size_t size)
{
  while (size != 0)
  {
    const ssize_t n = write(fd, p, size);
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      return false;
    }
    p += n;
    size -= (size_t)n;
  }
  return true;
}

static LONG CreateDirs(const AString &dirPath)
{
  struct stat st;
  if (stat(dirPath, &st) == 0)
    return S_ISDIR(st.st_mode) ? ERROR_SUCCESS : ENOTDIR;
  const int slash = dirPath.ReverseFind_PathSepar();
  if (slash > 0)
  {
    const LONG res = CreateDirs(dirPath.Left((unsigned)slash));
    if (res != ERROR_SUCCESS)
      return res;
  }
  // XDG base directory spec: create missing directories with mode 0700.
  if (mkdir(dirPath, 0700) != 0 && errno != EEXIST)
    return errno;
  return ERROR_SUCCESS;
}

static FString GetDefaultFilePath()
{
  AString dir;
  const char *xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && xdg[0] == '/')
    dir = xdg;
  else
  {
    const char *home = getenv("HOME");
    if (!home || home[0] != '/')
    {
      const struct passwd *pw = getpwuid(getuid());
      home = (pw && pw->pw_dir && pw->pw_dir[0] == '/') ? pw->pw_dir : NULL;
    }
    if (!home)
      return FString();
    dir = home;
    dir += "/.config";
  }
  dir += "/7-Zip/7-Zip.ini";
  return dir;
}

class CStore
{
  bool _pathIsSet;
  FString _path;
  bool _loaded;
  CFileStamp _stamp;
  CObjectVector<COp> _pending;

  void Reload();
public:
  NSynchronization::CCriticalSection CS;
  CNode Root;
  unsigned NumOpenKeys;

  CStore(): _pathIsSet(false), _loaded(false), NumOpenKeys(0) {}

  const FString &GetPath()
  {
    if (!_pathIsSet)
    {
      _path = GetDefaultFilePath();
      _pathIsSet = true;
    }
    return _path;
  }

  void SetPath(const FString &path)
  {
    _path = path;
    _pathIsSet = true;
    _loaded = false;
    _stamp = CFileStamp();
    _pending.Clear();
    Root.Values.Clear();
    Root.SubKeys.Clear();
  }

  void Refresh();
  void Do(const COp &op)
  {
    ApplyOp(Root, op);
    _pending.Add(op);
  }
  LONG Flush();
};

void CStore::Reload()
{
  CFileStamp stamp;
  AString text;
  const int fd = open(_path, O_RDONLY | O_CLOEXEC);
  if (fd >= 0)
  {
    struct stat st;
    if (fstat(fd, &st) == 0)
    {
      stamp.Set(st);
      if (!ReadAll(fd, text))
        text.Empty();
    }
    close(fd);
  }
  Parse(text, Root);
  FOR_VECTOR (i, _pending)
    ApplyOp(Root, _pending[i]);
  _stamp = stamp;
  _loaded = true;
}

void CStore::Refresh()
{
  if (GetPath().IsEmpty())
    return;
  if (_loaded)
  {
    CFileStamp stamp;
    struct stat st;
    if (stat(_path, &st) == 0)
      stamp.Set(st);
    if (stamp.IsEqualTo(_stamp))
      return;
  }
  Reload();
}

LONG CStore::Flush()
{
  if (_pending.IsEmpty())
    return ERROR_SUCCESS;
  if (GetPath().IsEmpty())
  {
    _pending.Clear();
    return ERROR_SUCCESS;
  }

  const int slash = _path.ReverseFind_PathSepar();
  if (slash > 0)
  {
    const LONG res = CreateDirs(_path.Left((unsigned)slash));
    if (res != ERROR_SUCCESS)
      return res;
  }

  const FString lockPath = _path + ".lock";
  const int lockFd = open(lockPath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if (lockFd < 0)
    return errno;
  while (flock(lockFd, LOCK_EX) != 0)
    if (errno != EINTR)
    {
      const LONG res = errno;
      close(lockFd);
      return res;
    }

  // another process may have replaced the file since it was read
  Refresh();

  AString text;
  Serialize(Root, text);

  LONG res = ERROR_SUCCESS;
  AString tempPath = _path;
  tempPath += ".XXXXXX";
  const int fd = mkostemp(tempPath.GetBuf(), O_CLOEXEC);
  tempPath.ReleaseBuf_CalcLen(tempPath.Len());
  if (fd < 0)
    res = errno;
  else
  {
    struct stat st;
    mode_t mode = 0600;
    if (stat(_path, &st) == 0)
      mode = st.st_mode & 07777;
    if (fchmod(fd, mode) != 0
        || !WriteAll(fd, text, text.Len())
        || fsync(fd) != 0
        || fstat(fd, &st) != 0)
      res = errno;
    if (close(fd) != 0 && res == ERROR_SUCCESS)
      res = errno;
    if (res == ERROR_SUCCESS && rename(tempPath, _path) != 0)
      res = errno;
    if (res == ERROR_SUCCESS)
    {
      // rename() keeps the inode and times, so this is the stamp of _path now
      _stamp.Set(st);
      _pending.Clear();
    }
    else
      unlink(tempPath);
  }
  close(lockFd);
  return res;
}

static CStore g_Store;

}

using namespace NIni;

#define STORE_LOCK NSynchronization::CCriticalSectionLock lock(g_Store.CS);

static void AddKeyPath(AStringVector &path, LPCTSTR keyName)
{
  if (!keyName)
    return;
  AString part;
  for (const char *p = keyName;; p++)
  {
    const char c = *p;
    if (c == 0 || c == '/' || c == '\\')
    {
      if (!part.IsEmpty())
        path.Add(part);
      part.Empty();
      if (c == 0)
        return;
    }
    else
      part.Add_Char(c);
  }
}

static bool IsPredefinedKey(HKEY key)
{
  return key == HKEY_CURRENT_USER || key == HKEY_CLASSES_ROOT || key == HKEY_LOCAL_MACHINE;
}

static bool GetKeyPath(HKEY key, AStringVector &path)
{
  path.Clear();
  if (key == HKEY_CURRENT_USER)
    return true;
  if (key == NULL || IsPredefinedKey(key))
    return false;
  path = key->Path;
  return true;
}

static AString GetName(LPCTSTR name)
{
  return AString(name ? name : "");
}

static AString GetName(LPCWSTR name)
{
  AString s;
  if (name)
    ConvertUnicodeToUTF8(name, s);
  return s;
}

static LONG OpenKey(HKEY parentKey, LPCTSTR keyName, bool create, HKEY &result, bool &created)
{
  result = NULL;
  created = false;
  AStringVector path;
  if (!GetKeyPath(parentKey, path))
    return create ? ERROR_ACCESS_DENIED : ERROR_FILE_NOT_FOUND;
  AddKeyPath(path, keyName);
  {
    STORE_LOCK
    g_Store.Refresh();
    if (!FindNode(g_Store.Root, path))
    {
      if (!create)
        return ERROR_FILE_NOT_FOUND;
      COp op;
      op.Type = kOp_CreateKey;
      op.Path = path;
      g_Store.Do(op);
      created = true;
    }
    g_Store.NumOpenKeys++;
  }
  result = new HKEY__;
  result->Path = path;
  return ERROR_SUCCESS;
}

static LONG DeleteKey(HKEY key, LPCTSTR subKeyName, bool recurse)
{
  COp op;
  op.Type = kOp_DeleteKey;
  if (!GetKeyPath(key, op.Path))
    return ERROR_ACCESS_DENIED;
  const unsigned parentLen = op.Path.Size();
  AddKeyPath(op.Path, subKeyName);
  if (op.Path.Size() == parentLen)
    return ERROR_ACCESS_DENIED; // a key cannot delete itself
  STORE_LOCK
  g_Store.Refresh();
  const CNode *node = FindNode(g_Store.Root, op.Path);
  if (!node)
    return ERROR_FILE_NOT_FOUND;
  if (!recurse && !node->SubKeys.IsEmpty())
    return ERROR_ACCESS_DENIED; // as RegDeleteKey() in Windows NT
  g_Store.Do(op);
  return ERROR_SUCCESS;
}

static LONG SetValueData(HKEY key, const AString &name, DWORD type, const void *data, size_t size)
{
  COp op;
  op.Type = kOp_SetValue;
  if (!GetKeyPath(key, op.Path))
    return ERROR_ACCESS_DENIED;
  op.Value.Name = name;
  op.Value.Type = type;
  op.Value.Data.CopyFrom((const Byte *)data, size);
  STORE_LOCK
  g_Store.Refresh();
  g_Store.Do(op);
  return ERROR_SUCCESS;
}

static LONG DeleteValueData(HKEY key, const AString &name)
{
  COp op;
  op.Type = kOp_DeleteValue;
  if (!GetKeyPath(key, op.Path))
    return ERROR_ACCESS_DENIED;
  op.Value.Name = name;
  STORE_LOCK
  g_Store.Refresh();
  const CNode *node = FindNode(g_Store.Root, op.Path);
  if (!node || node->FindValue(name) < 0)
    return ERROR_FILE_NOT_FOUND;
  g_Store.Do(op);
  return ERROR_SUCCESS;
}

// Copies the value, so that it can be used without the lock.
static LONG GetValueData(HKEY key, const AString &name, CValue &v)
{
  AStringVector path;
  if (!GetKeyPath(key, path))
    return ERROR_FILE_NOT_FOUND;
  STORE_LOCK
  g_Store.Refresh();
  const CNode *node = FindNode(g_Store.Root, path);
  if (!node)
    return ERROR_FILE_NOT_FOUND;
  const int index = node->FindValue(name);
  if (index < 0)
    return ERROR_FILE_NOT_FOUND;
  v = node->Values[(unsigned)index];
  return ERROR_SUCCESS;
}

static LONG QueryString(HKEY key, const AString &name, AString &value)
{
  value.Empty();
  CValue v;
  const LONG res = GetValueData(key, name, v);
  if (res != ERROR_SUCCESS)
    return res;
  if (v.Type != REG_SZ && v.Type != REG_EXPAND_SZ)
    return ERROR_UNSUPPORTED_TYPE;
  // the parser never stores zero bytes in strings
  value.SetFrom_Chars_SizeT((const char *)(const Byte *)v.Data, v.Data.Size());
  return ERROR_SUCCESS;
}


LONG CKey::Create(HKEY parentKey, LPCTSTR keyName,
    LPTSTR /* keyClass */, DWORD /* options */, REGSAM /* accessMask */,
    LPSECURITY_ATTRIBUTES /* securityAttributes */, LPDWORD disposition) throw()
{
  try
  {
    HKEY key;
    bool created;
    LONG res = OpenKey(parentKey, keyName, true, key, created);
    if (disposition)
      *disposition = created ? REG_CREATED_NEW_KEY : REG_OPENED_EXISTING_KEY;
    if (res == ERROR_SUCCESS)
    {
      res = Close();
      _object = key;
    }
    return res;
  }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::Open(HKEY parentKey, LPCTSTR keyName, REGSAM /* accessMask */) throw()
{
  try
  {
    HKEY key;
    bool created;
    LONG res = OpenKey(parentKey, keyName, false, key, created);
    if (res == ERROR_SUCCESS)
    {
      res = Close();
      _object = key;
    }
    return res;
  }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::Close() throw()
{
  if (_object == NULL)
    return ERROR_SUCCESS;
  const HKEY key = _object;
  _object = NULL;
  if (IsPredefinedKey(key)) // attached with Attach()
    return ERROR_SUCCESS;
  delete key;
  try
  {
    STORE_LOCK
    if (g_Store.NumOpenKeys != 0)
      g_Store.NumOpenKeys--;
    if (g_Store.NumOpenKeys != 0)
      return ERROR_SUCCESS;
    return g_Store.Flush();
  }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::DeleteSubKey(LPCTSTR subKeyName) throw()
{
  try { return DeleteKey(_object, subKeyName, false); }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::RecurseDeleteKey(LPCTSTR subKeyName) throw()
{
  try { return DeleteKey(_object, subKeyName, true); }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::DeleteValue(LPCTSTR name) throw()
{
  try { return DeleteValueData(_object, GetName(name)); }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::DeleteValue(LPCWSTR name)
{
  return DeleteValueData(_object, GetName(name));
}

LONG CKey::SetValue(LPCTSTR name, UInt32 value) throw()
{
  Byte buf[4];
  SetUi32(buf, value)
  try { return SetValueData(_object, GetName(name), REG_DWORD, buf, 4); }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::SetValue(LPCTSTR name, bool value) throw()
{
  return SetValue(name, (UInt32)(value ? 1 : 0));
}

LONG CKey::SetValue(LPCTSTR name, LPCTSTR value) throw()
{
  if (!value)
    value = "";
  try { return SetValueData(_object, GetName(name), REG_SZ, value, strlen(value)); }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::SetValue(LPCWSTR name, LPCWSTR value)
{
  AString s;
  if (value)
    ConvertUnicodeToUTF8(value, s);
  return SetValueData(_object, GetName(name), REG_SZ, s.Ptr(), s.Len());
}

LONG CKey::SetValue(LPCTSTR name, const void *value, UInt32 size) throw()
{
  try { return SetValueData(_object, GetName(name), REG_BINARY, value, size); }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG SetValue(HKEY parentKey, LPCTSTR keyName, LPCTSTR valueName, LPCTSTR value)
{
  CKey key;
  LONG res = key.Create(parentKey, keyName);
  if (res == ERROR_SUCCESS)
    res = key.SetValue(valueName, value);
  return res;
}

LONG CKey::SetKeyValue(LPCTSTR keyName, LPCTSTR valueName, LPCTSTR value) throw()
{
  CKey key;
  LONG res = key.Create(_object, keyName);
  if (res == ERROR_SUCCESS)
    res = key.SetValue(valueName, value);
  return res;
}

LONG CKey::SetValue_Strings(LPCTSTR valueName, const UStringVector &strings)
{
  CByteVector all;
  FOR_VECTOR (i, strings)
  {
    AString s;
    ConvertUnicodeToUTF8(strings[i], s);
    for (unsigned k = 0; k < s.Len(); k++)
      all.Add((Byte)s[k]);
    all.Add(0);
  }
  return SetValueData(_object, GetName(valueName), REG_MULTI_SZ, all.ConstData(), all.Size());
}

LONG CKey::GetValue_Strings(LPCTSTR valueName, UStringVector &strings)
{
  strings.Clear();
  CValue v;
  const LONG res = GetValueData(_object, GetName(valueName), v);
  if (res != ERROR_SUCCESS)
    return res;
  if (v.Type != REG_MULTI_SZ)
    return ERROR_UNSUPPORTED_TYPE;
  const char *data = (const char *)(const Byte *)v.Data;
  size_t start = 0;
  for (size_t i = 0; i < v.Data.Size(); i++)
    if (data[i] == 0)
    {
      AString a;
      a.SetFrom_Chars_SizeT(data + start, i - start);
      UString u;
      ConvertUTF8ToUnicode(a, u);
      strings.Add(u);
      start = i + 1;
    }
  return ERROR_SUCCESS;
}

LONG CKey::GetValue_UInt32_IfOk(LPCTSTR name, UInt32 &value) throw()
{
  try
  {
    CValue v;
    const LONG res = GetValueData(_object, GetName(name), v);
    if (res != ERROR_SUCCESS)
      return res;
    if (v.Type != REG_DWORD || v.Data.Size() != 4)
      return ERROR_UNSUPPORTED_TYPE;
    value = GetUi32(v.Data);
    return ERROR_SUCCESS;
  }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::GetValue_UInt64_IfOk(LPCTSTR name, UInt64 &value) throw()
{
  try
  {
    CValue v;
    const LONG res = GetValueData(_object, GetName(name), v);
    if (res != ERROR_SUCCESS)
      return res;
    if (v.Type != REG_QWORD || v.Data.Size() != 8)
      return ERROR_UNSUPPORTED_TYPE;
    value = GetUi64(v.Data);
    return ERROR_SUCCESS;
  }
  catch(...) { return E_OUTOFMEMORY; }
}

LONG CKey::GetValue_bool_IfOk(LPCTSTR name, bool &value) throw()
{
  UInt32 v;
  const LONG res = GetValue_UInt32_IfOk(name, v);
  if (res == ERROR_SUCCESS)
    value = (v != 0);
  return res;
}

LONG CKey::QueryValue(LPCTSTR name, CSysString &value)
{
  return QueryString(_object, GetName(name), value);
}

LONG CKey::QueryValue(LPCWSTR name, UString &value)
{
  value.Empty();
  AString s;
  const LONG res = QueryString(_object, GetName(name), s);
  if (res == ERROR_SUCCESS)
    ConvertUTF8ToUnicode(s, value);
  return res;
}

LONG CKey::QueryValue_Binary(LPCTSTR name, CByteBuffer &value)
{
  value.Free();
  CValue v;
  const LONG res = GetValueData(_object, GetName(name), v);
  if (res != ERROR_SUCCESS)
    return res;
  if (v.Type != REG_BINARY)
    return ERROR_UNSUPPORTED_TYPE;
  value = v.Data;
  return ERROR_SUCCESS;
}

LONG CKey::EnumKeys(CSysStringVector &keyNames)
{
  keyNames.Clear();
  AStringVector path;
  if (!GetKeyPath(_object, path))
    return ERROR_SUCCESS;
  STORE_LOCK
  g_Store.Refresh();
  const CNode *node = FindNode(g_Store.Root, path);
  if (!node)
    return ERROR_FILE_NOT_FOUND;
  FOR_VECTOR (i, node->SubKeys)
    keyNames.Add(node->SubKeys[i].Name);
  return ERROR_SUCCESS;
}


FString Ini_GetFilePath()
{
  STORE_LOCK
  return g_Store.GetPath();
}

void Ini_SetFilePath(const FString &path)
{
  STORE_LOCK
  g_Store.SetPath(path);
}

LONG Ini_Flush()
{
  STORE_LOCK
  return g_Store.Flush();
}

}}
