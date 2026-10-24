// ShimSystem.cpp -- the kernel32, advapi32 and ole32 calls of the GUI code:
// sleeping, strings, modules, handles, processes, the user name and
// environment strings, process priority, COM start-up and OLE drag and
// drop registration.

#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <pwd.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include <windows.h>
#include <shellapi.h>
#include <TlHelp32.h>

#include "../../CPP/Common/CommandLineParser.h"
#include "../../CPP/Windows/ProcessUtils.h"

#include "ShimWindow.h"

void Sleep(DWORD milliseconds)
{
  struct timespec t;
  t.tv_sec = (time_t)(milliseconds / 1000);
  t.tv_nsec = (long)(milliseconds % 1000) * 1000000;
  while (nanosleep(&t, &t) != 0 && errno == EINTR)
  {
  }
}

LPSTR lstrcatA(LPSTR dest, LPCSTR src)
{
  return strcat(dest, src);
}

LPWSTR lstrcatW(LPWSTR dest, LPCWSTR src)
{
  return wcscat(dest, src);
}

// ---------- Modules ----------

struct CProc
{
  const char *Name;
  void *Func;
};

struct CModule
{
  const char *Name;   // lower case, without ".dll"
  const CProc *Procs;
  unsigned NumProcs;
};

static const CProc kShell32Procs[] =
{
  { "SHFileOperationW", reinterpret_cast<void *>(&SHFileOperationW) },
  { "SHGetFileInfoW", reinterpret_cast<void *>(&SHGetFileInfoW) },
  { "ShellExecuteExW", reinterpret_cast<void *>(&ShellExecuteExW) }
};

static const CModule kModules[] =
{
  { "shell32", kShell32Procs, Z7_ARRAY_SIZE(kShell32Procs) },
  { "kernel32", NULL, 0 }
};

static HMODULE FindModule(const QString &path)
{
  QString name = path.section(QLatin1Char('\\'), -1).toLower();
  if (name.endsWith(QLatin1String(".dll")))
    name.chop(4);
  for (const CModule &m : kModules)
    if (name == QLatin1String(m.Name))
      return (HMODULE)const_cast<CModule *>(&m);
  return NULL;
}

HMODULE GetModuleHandleA(LPCSTR name)
{
  return name ? FindModule(QString::fromLocal8Bit(name)) : NULL;
}

HMODULE GetModuleHandleW(LPCWSTR name)
{
  return name ? FindModule(QString::fromWCharArray(name)) : NULL;
}

void *GetProcAddress(HMODULE module, LPCSTR procName)
{
  if (!module || !procName)
    return NULL;
  for (const CModule &m : kModules)
    if (module == (HMODULE)&m)
    {
      for (unsigned i = 0; i < m.NumProcs; i++)
        if (strcmp(m.Procs[i].Name, procName) == 0)
          return m.Procs[i].Func;
      return NULL;
    }
  return dlsym(module, procName);
}

HMODULE LoadLibraryA(LPCSTR /* name */)
{
  return NULL;
}

HMODULE LoadLibraryW(LPCWSTR /* name */)
{
  return NULL;
}

DWORD GetVersion()
{
  // Windows 10 (10.0, build 19041)
  return ((DWORD)19041 << 16) | 0x000A;
}

// ---------- Handles ----------

BOOL CloseHandle(HANDLE handle)
{
  return (handle && handle != INVALID_HANDLE_VALUE) ? TRUE : FALSE;
}

DWORD WaitForSingleObject(HANDLE /* handle */, DWORD /* milliseconds */)
{
  return WAIT_FAILED;
}

// ---------- User and environment ----------

static QString UserName()
{
  const struct passwd *pw = getpwuid(geteuid());
  if (pw && pw->pw_name)
    return QString::fromLocal8Bit(pw->pw_name);
  const char *s = getenv("USER");
  return QString::fromLocal8Bit(s ? s : "");
}

// The buffer sizes count characters with the terminating zero.
BOOL GetUserNameW(LPWSTR buffer, LPDWORD size)
{
  const QString name = UserName();
  const DWORD need = (DWORD)NShim::LengthW(name) + 1;
  if (!buffer || !size || *size < need)
  {
    if (size)
      *size = need;
    SetLastError(ERANGE);
    return FALSE;
  }
  NShim::CopyToW(name, buffer, (int)*size);
  *size = need;
  return TRUE;
}

BOOL GetUserNameA(LPSTR buffer, LPDWORD size)
{
  const QString name = UserName();
  const DWORD need = (DWORD)NShim::LengthA(name) + 1;
  if (!buffer || !size || *size < need)
  {
    if (size)
      *size = need;
    SetLastError(ERANGE);
    return FALSE;
  }
  NShim::CopyToA(name, buffer, (int)*size);
  *size = need;
  return TRUE;
}

// %NAME% is the variable NAME; an unknown name stays as it is.
static QString ExpandEnvironment(const QString &src)
{
  QString r;
  int i = 0;
  while (i < src.size())
  {
    const int start = src.indexOf(QLatin1Char('%'), i);
    if (start < 0)
      break;
    const int end = src.indexOf(QLatin1Char('%'), start + 1);
    if (end < 0)
      break;
    r += src.mid(i, start - i);
    const QString name = src.mid(start + 1, end - start - 1);
    const QByteArray value = qgetenv(name.toLocal8Bit().constData());
    if (!name.isEmpty() && !value.isNull())
    {
      r += QString::fromLocal8Bit(value);
      i = end + 1;
    }
    else
    {
      r += src.mid(start, end - start);
      i = end;
    }
  }
  r += src.mid(i);
  return r;
}

DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dest, DWORD size)
{
  const QString r = ExpandEnvironment(NShim::FromW(src));
  const DWORD need = (DWORD)NShim::LengthW(r) + 1;
  if (dest && size >= need)
    NShim::CopyToW(r, dest, (int)size);
  return need;
}

DWORD ExpandEnvironmentStringsA(LPCSTR src, LPSTR dest, DWORD size)
{
  const QString r = ExpandEnvironment(NShim::FromA(src));
  const DWORD need = (DWORD)NShim::LengthA(r) + 1;
  if (dest && size >= need)
    NShim::CopyToA(r, dest, (int)size);
  return need;
}

// ---------- Processes ----------

/* Programs are started detached (QProcess::startDetached()): there are no
   process handles to wait for or to ask about, so the calls below fail. */

HANDLE OpenProcess(DWORD /* desiredAccess */, BOOL /* inheritHandle */, DWORD /* processId */)
{
  return NULL;
}

BOOL GetExitCodeProcess(HANDLE /* process */, LPDWORD /* exitCode */)
{
  return FALSE;
}

BOOL TerminateProcess(HANDLE /* process */, UINT /* exitCode */)
{
  return FALSE;
}

BOOL GetProcessTimes(HANDLE /* process */, LPFILETIME /* creationTime */, LPFILETIME /* exitTime */,
    LPFILETIME /* kernelTime */, LPFILETIME /* userTime */)
{
  return FALSE;
}

DWORD WaitForInputIdle(HANDLE /* process */, DWORD /* milliseconds */)
{
  return WAIT_FAILED;
}

BOOL ReadProcessMemory(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T *)
{
  return FALSE;
}

BOOL WriteProcessMemory(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T *)
{
  return FALSE;
}

BOOL FlushInstructionCache(HANDLE, LPCVOID, SIZE_T)
{
  return FALSE;
}

LPVOID VirtualAllocEx(HANDLE, LPVOID, SIZE_T, DWORD, DWORD)
{
  return NULL;
}

BOOL VirtualFreeEx(HANDLE, LPVOID, SIZE_T, DWORD)
{
  return FALSE;
}

DWORD GetProcessId(HANDLE /* process */)
{
  return 0;
}

DWORD GetCurrentProcessId()
{
  return (DWORD)getpid();
}

HANDLE CreateToolhelp32Snapshot(DWORD /* flags */, DWORD /* processId */)
{
  return INVALID_HANDLE_VALUE;
}

BOOL Process32First(HANDLE /* snapshot */, LPPROCESSENTRY32 /* entry */)
{
  return FALSE;
}

BOOL Process32Next(HANDLE /* snapshot */, LPPROCESSENTRY32 /* entry */)
{
  return FALSE;
}

namespace NWindows {

/* Windows/ProcessUtils.cpp runs CreateProcess(). Here the program is
   started detached, with params split like a Windows command line, and
   the object gets no handle. Upstream names its own programs with the
   Windows suffix ("7zFM.exe"), which the Linux ones do not have. */
WRes CProcess::Create(LPCWSTR imageName, const UString &params, LPCWSTR curDir)
{
  Close();
  UStringVector parts;
  NCommandLineParser::SplitCommandLine(params, parts);
  QStringList args;
  FOR_VECTOR (i, parts)
    args.append(NShim::FromU(parts[i]));
  QString program = NShim::FromW(imageName);
  if (program.isEmpty())
    return EINVAL;
  if (program.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) && !QFileInfo::exists(program))
    program.chop(4);
  if (!QProcess::startDetached(program, args, curDir ? NShim::FromW(curDir) : QString()))
    return ENOENT;
  return 0;
}

WRes MyCreateProcess(LPCWSTR imageName, const UString &params)
{
  CProcess process;
  return process.Create(imageName, params, NULL);
}

}

// ---------- Process priority ----------

// The pseudo handle of the current process, as on Windows.
static HANDLE const kCurrentProcess = (HANDLE)(INT_PTR)-1;

static DWORD g_PriorityClass = NORMAL_PRIORITY_CLASS;

HANDLE GetCurrentProcess()
{
  return kCurrentProcess;
}

// Each thread has its own scheduling policy on Linux, so the policy is set
// for every thread of the process; threads created later inherit it from
// the thread that creates them.
static bool SetPolicyOfAllThreads(int policy)
{
  DIR *dir = opendir("/proc/self/task");
  if (!dir)
    return false;
  bool ok = true;
  for (;;)
  {
    const struct dirent *e = readdir(dir);
    if (!e)
      break;
    char *end;
    const long tid = strtol(e->d_name, &end, 10);
    if (end == e->d_name || *end != 0)
      continue;
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    if (sched_setscheduler((pid_t)tid, policy, &param) != 0 && errno != ESRCH)
      ok = false;
  }
  closedir(dir);
  return ok;
}

BOOL SetPriorityClass(HANDLE process, DWORD priorityClass)
{
  if (process != kCurrentProcess)
    return FALSE;
  const int policy = (priorityClass == IDLE_PRIORITY_CLASS) ? SCHED_IDLE : SCHED_OTHER;
  if (!SetPolicyOfAllThreads(policy))
    return FALSE;
  g_PriorityClass = priorityClass;
  return TRUE;
}

DWORD GetPriorityClass(HANDLE process)
{
  return process == kCurrentProcess ? g_PriorityClass : 0;
}

// ---------- COM ----------

HRESULT CoInitialize(LPVOID /* reserved */)
{
  return S_OK;
}

void CoUninitialize()
{
}

HRESULT OleInitialize(LPVOID /* reserved */)
{
  return S_OK;
}

void OleUninitialize()
{
}

namespace NShim {
// ShimTaskbar.cpp: CLSID_TaskbarList
HRESULT CreateShimObject(REFCLSID clsid, REFIID iid, void **object);
}

HRESULT CoCreateInstance(REFCLSID clsid, IUnknown * /* outer */, DWORD /* clsContext */,
    REFIID iid, LPVOID *object)
{
  if (!object)
    return E_INVALIDARG;
  *object = NULL;
  return NShim::CreateShimObject(clsid, iid, object);
}

// OLE drag and drop and ReleaseStgMedium() are in ShimDragDrop.cpp.

LPVOID CoTaskMemAlloc(size_t size)
{
  return malloc(size);
}

void CoTaskMemFree(LPVOID p)
{
  free(p);
}
