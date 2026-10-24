// ClassDefsShell.cpp -- defines the interface IDs of the Windows shell
// interfaces that 7zFM uses (taskbar list, Explorer commands and the
// context menu) once. FileManager/ClassDefs.cpp also defines the folder
// IDs, which linux/platform/ClassDefs.cpp defines for every program.

// INITGUID without MyInitGuid.h, which also defines IID_IUnknown
#define INITGUID
#include "../../CPP/Common/MyGuidDef.h"

#include <ShlObj.h>

#include "../../CPP/7zip/UI/FileManager/MyWindowsNew.h"
#include "../../CPP/7zip/UI/Explorer/MyExplorerCommand.h"
