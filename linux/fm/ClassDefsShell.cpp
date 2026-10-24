// ClassDefsShell.cpp -- defines the interface IDs of the Windows shell
// interfaces that 7zFM uses (ITaskbarList3, Explorer commands) once. The
// IDs of ShObjIdl.h are defined in linux/shim/ShimGuids.cpp.
// FileManager/ClassDefs.cpp also defines the folder IDs, which
// linux/platform/ClassDefs.cpp defines for every program.

// declared only
#include <ShlObj.h>

// INITGUID without MyInitGuid.h, which also defines IID_IUnknown
#define INITGUID
#include "../../CPP/Common/MyGuidDef.h"

#include "../../CPP/7zip/UI/FileManager/MyWindowsNew.h"
#include "../../CPP/7zip/UI/Explorer/MyExplorerCommand.h"
