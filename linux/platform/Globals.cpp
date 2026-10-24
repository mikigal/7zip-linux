// Globals.cpp -- globals that the Windows programs define in their main
// source files (FileManager/FM.cpp and others). Each one is in its own
// object file (see Instance.cpp), so a program that defines one itself does
// not link the platform's definition.

#include "Win32Compat.h"

// The wide-character (NT) code paths are the ones that exist on Linux.
bool g_IsNT = true;
