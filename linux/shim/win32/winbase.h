/* winbase.h -- the kernel32 subset of the 7-Zip GUI code that
   CPP/Common/MyWindows.h does not declare on POSIX. windows.h includes it
   for C++; linux/shim implements it (ShimSystem.cpp). */

#ifndef Z7_LINUX_SHIM_WINBASE_H
#define Z7_LINUX_SHIM_WINBASE_H

#include <time.h>

#include "winuser.h"

void Sleep(DWORD milliseconds);

LPSTR lstrcatA(LPSTR dest, LPCSTR src);
LPWSTR lstrcatW(LPWSTR dest, LPCWSTR src);
#define lstrcat lstrcatA

/* Modules: the program is one module (resources are linked in).
   GetModuleHandle() finds "shell32.dll" and "kernel32.dll" only, for the
   functions that the GUI code looks up by name because it is built without
   _UNICODE (Z7_GET_PROC_ADDRESS): GetProcAddress() gives the shim functions
   that have those names, and NULL for the others, so callers fall back as
   they do when a function is missing from a DLL. For other modules it is
   the dlsym() of Windows/DLL.cpp, which declares it the same way in
   Windows/DLL.h; the macro keeps the two apart. */
HMODULE GetModuleHandleA(LPCSTR name);
HMODULE GetModuleHandleW(LPCWSTR name);
#define GetModuleHandle GetModuleHandleA
#define GetProcAddress Shim_GetProcAddress
void *GetProcAddress(HMODULE module, LPCSTR procName);

/* Process priority. IDLE_PRIORITY_CLASS runs the threads of the process
   with SCHED_IDLE; going back to NORMAL_PRIORITY_CLASS needs a RLIMIT_NICE
   that allows nice 0 (or CAP_SYS_NICE), as raising a priority does on Linux. */
#define NORMAL_PRIORITY_CLASS       0x00000020
#define IDLE_PRIORITY_CLASS         0x00000040
#define HIGH_PRIORITY_CLASS         0x00000080
#define BELOW_NORMAL_PRIORITY_CLASS 0x00004000
#define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000

HANDLE GetCurrentProcess();
BOOL SetPriorityClass(HANDLE process, DWORD priorityClass);
DWORD GetPriorityClass(HANDLE process);


typedef BYTE BOOLEAN;

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

/* Handles of the shim (pseudo handles and failed snapshots) need no
   closing. */
BOOL CloseHandle(HANDLE handle);

/* File times are timespec on POSIX (CFileInfo::MTime): FileManager/Panel.h
   compares them with CompareFileTime(), as it compares FILETIMEs on
   Windows. */
inline LONG CompareFileTime(const timespec *a, const timespec *b)
{
  if (a->tv_sec != b->tv_sec)
    return a->tv_sec < b->tv_sec ? -1 : 1;
  if (a->tv_nsec != b->tv_nsec)
    return a->tv_nsec < b->tv_nsec ? -1 : 1;
  return 0;
}

/* The login name of the user (getpwuid()). */
BOOL GetUserNameA(LPSTR buffer, LPDWORD size);
BOOL GetUserNameW(LPWSTR buffer, LPDWORD size);
#define GetUserName GetUserNameA

/* %NAME% is replaced with the environment variable NAME, as on Windows. */
DWORD ExpandEnvironmentStringsA(LPCSTR src, LPSTR dest, DWORD size);
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dest, DWORD size);
#define ExpandEnvironmentStrings ExpandEnvironmentStringsA


#define ZeroMemory(dest, len) memset((dest), 0, (len))
#ifndef NOERROR
#define NOERROR 0
#endif

/* C/Threads.c implements them on POSIX (Common/MyCom.h uses them for
   objects that several threads may share). */
EXTERN_C LONG InterlockedIncrement(LONG volatile *addend);
EXTERN_C LONG InterlockedDecrement(LONG volatile *addend);

/* Windows 10, for code that checks the version (it is NT 6+ on Linux). */
DWORD GetVersion();


/* Processes (Windows/ProcessUtils.h). The file manager starts programs
   through ShellExecuteEx() (shellapi.h); there are no process handles. */
#define SYNCHRONIZE               0x00100000L
#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define STILL_ACTIVE 259
HANDLE OpenProcess(DWORD desiredAccess, BOOL inheritHandle, DWORD processId);
BOOL GetExitCodeProcess(HANDLE process, LPDWORD exitCode);
BOOL TerminateProcess(HANDLE process, UINT exitCode);
BOOL GetProcessTimes(HANDLE process, LPFILETIME creationTime, LPFILETIME exitTime,
    LPFILETIME kernelTime, LPFILETIME userTime);
DWORD WaitForInputIdle(HANDLE process, DWORD milliseconds);
BOOL ReadProcessMemory(HANDLE process, LPCVOID baseAddress, LPVOID buffer, SIZE_T size, SIZE_T *numberOfBytesRead);
BOOL WriteProcessMemory(HANDLE process, LPVOID baseAddress, LPCVOID buffer, SIZE_T size, SIZE_T *numberOfBytesWritten);
BOOL FlushInstructionCache(HANDLE process, LPCVOID baseAddress, SIZE_T size);
LPVOID VirtualAllocEx(HANDLE process, LPVOID address, SIZE_T size, DWORD allocationType, DWORD protect);
BOOL VirtualFreeEx(HANDLE process, LPVOID address, SIZE_T size, DWORD freeType);
DWORD GetProcessId(HANDLE process);
DWORD GetCurrentProcessId();


/* Waiting: the shim has no waitable handles (the threads and events of the
   7-Zip code are the POSIX classes of Windows/Synchronization.h), so this
   returns WAIT_FAILED. The values are those of Windows/Synchronization.h. */
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258L
#define WAIT_FAILED ((DWORD)0xFFFFFFFF)
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);

/* DLLs: there are none to load (returns NULL). */
HMODULE LoadLibraryA(LPCSTR name);
HMODULE LoadLibraryW(LPCWSTR name);
#define LoadLibrary LoadLibraryA

#endif
