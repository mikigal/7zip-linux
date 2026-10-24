// Program.cpp -- the location of the running program.

#include "Win32Compat.h"

#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../CPP/Windows/DLL.h"

#include "Platform.h"

// UI/Common/ArchiveCommandLine.cpp (POSIX part)
void Set_ModuleDirPrefix_From_ProgArg0(const char *s);

static bool ReadExePath(FString &path)
{
  path.Empty();
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0)
    return false;
  buf[n] = 0;
  path = buf;
  return true;
}

namespace NWindows {
namespace NDLL {

// DLL.cpp has it for Windows only; UI/Agent reports it as the icon file.
bool MyGetModuleFileName(FString &path)
{
  return ReadExePath(path);
}

}}

namespace NPlatform {

FString GetHomeDir()
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

void InitModuleDirPrefix(const char *argv0)
{
  FString path;
  if (ReadExePath(path))
    Set_ModuleDirPrefix_From_ProgArg0(path);
  else if (argv0)
    Set_ModuleDirPrefix_From_ProgArg0(argv0);
}

}
