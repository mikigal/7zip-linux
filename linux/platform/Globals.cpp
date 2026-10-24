// Globals.cpp -- globals that the Windows programs define in their main
// source files (FileManager/FM.cpp and others). They are in their own object
// file, so a program that defines them itself does not link these.

#include "Win32Compat.h"

// Resources are linked into the program, so no module handle is needed.
HINSTANCE g_hInstance = NULL;

// The wide-character (NT) code paths are the ones that exist on Linux.
bool g_IsNT = true;
