// ShellExtension.cpp -- what the shell extension DLL defines for the
// Explorer code on Windows (Explorer/DllExportsExplorer.cpp): its object
// count, the switch of the user questions, and the IDs of the Explorer
// command interfaces.

// declared only
#include <ShlObj.h>

// INITGUID without MyInitGuid.h, which also defines IID_IUnknown
#define INITGUID
#include "../../CPP/Common/MyGuidDef.h"

#include "../../CPP/7zip/UI/Explorer/MyExplorerCommand.h"

LONG g_DllRefCount = 0;
bool g_DisableUserQuestions;
