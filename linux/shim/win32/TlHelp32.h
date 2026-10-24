/* TlHelp32.h -- the process snapshot of FileManager/PanelItemOpen.cpp,
   which looks for the child processes of a program that opened a file.
   CreateToolhelp32Snapshot() fails on Linux (INVALID_HANDLE_VALUE), so
   that search finds nothing (ShimSystem.cpp). */

#ifndef Z7_LINUX_SHIM_TLHELP32_H
#define Z7_LINUX_SHIM_TLHELP32_H

#include <windows.h>

#define TH32CS_SNAPPROCESS 0x00000002

typedef struct tagPROCESSENTRY32
{
  DWORD dwSize;
  DWORD cntUsage;
  DWORD th32ProcessID;
  ULONG_PTR th32DefaultHeapID;
  DWORD th32ModuleID;
  DWORD cntThreads;
  DWORD th32ParentProcessID;
  LONG pcPriClassBase;
  DWORD dwFlags;
  CHAR szExeFile[MAX_PATH];
} PROCESSENTRY32, *LPPROCESSENTRY32;

HANDLE CreateToolhelp32Snapshot(DWORD flags, DWORD processId);
BOOL Process32First(HANDLE snapshot, LPPROCESSENTRY32 entry);
BOOL Process32Next(HANDLE snapshot, LPPROCESSENTRY32 entry);

#endif
