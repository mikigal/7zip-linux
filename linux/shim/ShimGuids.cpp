// ShimGuids.cpp -- defines the interface IDs of the shell interfaces in
// ShObjIdl.h (taskbar list, shell folders, context menus) and of the OLE
// drag and drop interfaces (oleidl.h) once, for the shim and the programs
// on top of it.

#include <windows.h>
#include <CommCtrl.h>

// INITGUID only for ShObjIdl.h: the headers above declare IDs that are
// defined elsewhere
#define INITGUID
#include "../../CPP/Common/MyGuidDef.h"

#include <ShObjIdl.h>

#undef Z7_LINUX_SHIM_OLEIDLGUIDS_H
#include <oleidlguids.h>
