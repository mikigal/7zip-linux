/* winbase.h -- the kernel32 subset of the 7-Zip GUI code that
   CPP/Common/MyWindows.h does not declare on POSIX. windows.h includes it
   for C++; linux/shim implements it (ShimSystem.cpp). */

#ifndef Z7_LINUX_SHIM_WINBASE_H
#define Z7_LINUX_SHIM_WINBASE_H

#include "winuser.h"

void Sleep(DWORD milliseconds);

LPSTR lstrcatA(LPSTR dest, LPCSTR src);
LPWSTR lstrcatW(LPWSTR dest, LPCWSTR src);
#define lstrcat lstrcatA

/* Modules: the program is one module (resources are linked in), so there
   are no module handles to find; callers fall back as they do when a
   function is missing from a DLL. */
HMODULE GetModuleHandleA(LPCSTR name);
HMODULE GetModuleHandleW(LPCWSTR name);
#define GetModuleHandle GetModuleHandleA
/* Windows/DLL.cpp implements it with dlsym() (and declares it the same way
   in Windows/DLL.h). */
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

#endif
