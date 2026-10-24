// Instance.cpp -- g_hInstance for programs whose main source file does not
// define it (GUI/GUI.cpp and FileManager/FM.cpp do; see Globals.cpp).

#include "Win32Compat.h"

// Resources are linked into the program, so no module handle is needed.
HINSTANCE g_hInstance = NULL;
