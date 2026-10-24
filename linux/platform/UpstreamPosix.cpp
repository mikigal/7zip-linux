// UpstreamPosix.cpp -- POSIX versions of functions that the upstream
// CPP/Windows headers declare for every system but define for Windows only,
// and of the helpers in UpstreamPosix.h.

#include "Win32Compat.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include <mutex>
#include <set>
#include <string>

#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/FileFind.h"
#include "../../CPP/Windows/FileName.h"
#include "../../CPP/Windows/MemoryLock.h"

#include "Platform.h"
#include "UpstreamPosix.h"

// The C event inside CBaseEvent (a protected member, reached through a
// pointer to member of a derived class).
struct CBaseEventAccess: public NWindows::NSynchronization::CBaseEvent
{
  static ::CEvent CBaseEvent::*Object() { return &CBaseEventAccess::_object; }
};

DWORD WaitForSingleObject(NWindows::NSynchronization::CBaseEvent &event, DWORD milliseconds)
{
  ::CEvent &e = event.*CBaseEventAccess::Object();
  if (milliseconds == 0xFFFFFFFF) // INFINITE
    return Event_Wait(&e) == 0 ? 0 : (DWORD)0xFFFFFFFF;
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end); // the clock of the condition (C/Threads.c)
  end.tv_sec += (time_t)(milliseconds / 1000);
  end.tv_nsec += (long)(milliseconds % 1000) * 1000000;
  if (end.tv_nsec >= 1000000000)
  {
    end.tv_sec++;
    end.tv_nsec -= 1000000000;
  }
  pthread_mutex_lock(&e._mutex);
  int res = 0;
  while (!e._state && res != ETIMEDOUT)
    res = pthread_cond_timedwait(&e._cond, &e._mutex, &end);
  const bool set = (e._state != 0);
  if (set && !e._manual_reset)
    e._state = 0;
  pthread_mutex_unlock(&e._mutex);
  return set ? 0 : 258; // WAIT_OBJECT_0, WAIT_TIMEOUT
}

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

// $TMPDIR, as POSIX programs use it; MyGetTempPath() gives /tmp only.
static bool GetTempDirPath(FString &path)
{
  const char *env = getenv("TMPDIR");
  if (env && env[0] == '/' && NFind::DoesDirExist_FollowLink(env))
  {
    path = env;
    NName::NormalizeDirPathPrefix(path);
    return true;
  }
  return MyGetTempPath(path);
}

// The folders that CTempDir leaves for the end of the program
// (NPlatform::KeepTempDirUntilExit()); never destroyed, as exit() uses them.
static std::mutex g_KeptDirsMutex;
static std::set<std::string> *g_KeptDirs;

static std::string KeptDirKey(const FString &path)
{
  std::string s (path.Ptr(), path.Len());
  while (s.size() > 1 && s.back() == '/')
    s.pop_back();
  return s;
}

static void RemoveKeptDirs()
{
  std::lock_guard<std::mutex> lock(g_KeptDirsMutex);
  for (const std::string &dir : *g_KeptDirs)
    RemoveDirWithSubItems(FString(dir.c_str()));
}

static bool IsKeptDir(const FString &path)
{
  std::lock_guard<std::mutex> lock(g_KeptDirsMutex);
  return g_KeptDirs && g_KeptDirs->count(KeptDirKey(path)) != 0;
}

bool CTempDir::Create(CFSTR prefix)
{
  if (!Remove())
    return false;
  _path.Empty();
  FString tempPath;
  if (!GetTempDirPath(tempPath))
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
  if (IsKeptDir(_path))
  {
    // removed when the program exits
    _mustBeDeleted = false;
    return true;
  }
  _mustBeDeleted = !RemoveDirWithSubItems(_path);
  return !_mustBeDeleted;
}

}}

}

namespace NPlatform {

void KeepTempDirUntilExit(const FString &path)
{
  using namespace NWindows::NFile::NDir;
  std::lock_guard<std::mutex> lock(g_KeptDirsMutex);
  if (!g_KeptDirs)
  {
    g_KeptDirs = new std::set<std::string>;
    atexit(RemoveKeptDirs);
  }
  g_KeptDirs->insert(KeptDirKey(path));
}

}

namespace NWindows {

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

// FM.cpp asks for the lock-memory privilege at start (large pages).
bool AddLockMemoryPrivilege()
{
  return false;
}

}

}
