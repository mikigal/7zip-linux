// UpstreamPosix.cpp -- POSIX versions of functions that the upstream
// CPP/Windows headers declare for every system but define for Windows only,
// and of the helpers in UpstreamPosix.h.

#include "Win32Compat.h"

#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/FileFind.h"
#include "../../CPP/Windows/FileName.h"
#include "../../CPP/Windows/MemoryLock.h"

#include "UpstreamPosix.h"

namespace NWindows {
namespace NFile {

namespace NName {

// "name:" names an alternate stream on Windows; POSIX has none.
bool IsAltPathPrefix(CFSTR /* s */) throw()
{
  return false;
}

}

namespace NDir {

// There is no Windows directory (the help viewer and Notepad live there).
bool GetWindowsDir(FString & /* path */)
{
  return false;
}

/* As the Windows version: a symbolic link is removed itself, not the
   directory it points to. */
bool RemoveDirWithSubItems(const FString &path)
{
  {
    NFind::CFileInfo fi;
    if (!fi.Find(path))
      return false;
    if (fi.IsPosixLink())
      return DeleteFileAlways(path);
    if (!fi.IsDir())
    {
      ::SetLastError(ENOTDIR);
      return false;
    }
  }

  FString s (path);
  s.Add_PathSepar();
  const unsigned prefixSize = s.Len();
  bool isError = false;
  DWORD lastError = 0;
  {
    NFind::CEnumerator enumerator;
    enumerator.SetDirPrefix(s);
    NFind::CFileInfo fi;
    while (enumerator.Next(fi))
    {
      s.DeleteFrom(prefixSize);
      s += fi.Name;
      const bool ok = fi.IsDir() ? RemoveDirWithSubItems(s) : DeleteFileAlways(s);
      if (!ok)
      {
        lastError = ::GetLastError();
        isError = true;
      }
    }
  }
  if (isError)
  {
    ::SetLastError(lastError);
    return false;
  }
  return RemoveDir(path);
}

bool CTempDir::Create(CFSTR prefix)
{
  if (!Remove())
    return false;
  _path.Empty();
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  tempPath += prefix;
  AString postfix;
  if (!CreateTempFile2(tempPath, true, postfix, NULL))
    return false;
  _path = tempPath;
  _path += postfix;
  _mustBeDeleted = true;
  return true;
}

bool CTempDir::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !RemoveDirWithSubItems(_path);
  return !_mustBeDeleted;
}

}}

namespace NSecurity {

// The privileges of Windows (lock memory, symbolic links) do not exist.
bool EnablePrivilege(LPCTSTR /* privilegeName */, bool /* enable */)
{
  return false;
}

unsigned Get_LargePages_RiskLevel()
{
  return 0;
}

}

}
