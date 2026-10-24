// Program.cpp -- the location of the running program.

#include "Win32Compat.h"

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

void InitModuleDirPrefix(const char *argv0)
{
  FString path;
  if (ReadExePath(path))
    Set_ModuleDirPrefix_From_ProgArg0(path);
  else if (argv0)
    Set_ModuleDirPrefix_From_ProgArg0(argv0);
}

}
