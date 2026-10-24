// ShimSystem.cpp -- the kernel32 and ole32 calls of the GUI code: sleeping,
// strings, modules, process priority and COM start-up.

#include <dirent.h>
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include <windows.h>

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

HMODULE GetModuleHandleA(LPCSTR /* name */)
{
  return NULL;
}

HMODULE GetModuleHandleW(LPCWSTR /* name */)
{
  return NULL;
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

HRESULT CoCreateInstance(REFCLSID /* clsid */, IUnknown * /* outer */, DWORD /* clsContext */,
    REFIID /* iid */, LPVOID *object)
{
  if (object)
    *object = NULL;
  return REGDB_E_CLASSNOTREG;
}

LPVOID CoTaskMemAlloc(size_t size)
{
  return malloc(size);
}

void CoTaskMemFree(LPVOID p)
{
  free(p);
}
