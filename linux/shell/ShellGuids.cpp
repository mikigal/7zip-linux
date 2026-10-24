// ShellGuids.cpp -- what the shell extension DLL defines for the Explorer
// code on Windows (Explorer/DllExportsExplorer.cpp): its object count and
// the IDs of the Explorer command interfaces. g_DisableUserQuestions,
// which the DLL defines too, comes from the program (7zG) or the plugin.

// declared only
#include <ShlObj.h>

// INITGUID without MyInitGuid.h, which also defines IID_IUnknown
#define INITGUID
#include "../../CPP/Common/MyGuidDef.h"

#include "../../CPP/7zip/UI/Explorer/MyExplorerCommand.h"

LONG g_DllRefCount = 0;
