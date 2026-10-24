// ShimGuids.cpp -- defines the interface IDs of the shell interfaces in
// ShObjIdl.h (taskbar list, shell folders, context menus) once, for the
// shim and the programs on top of it.

#include <windows.h>
#include <CommCtrl.h>

// INITGUID only for ShObjIdl.h: the headers above declare IDs that are
// defined elsewhere
#define INITGUID
#include "../../CPP/Common/MyGuidDef.h"

#include <ShObjIdl.h>
