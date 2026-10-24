// ContextMenuRegistration.cpp -- the shell integration switch of the 7-Zip
// options page (Explorer/RegistryContextMenu.h), which registers the
// Explorer context menu handler on Windows. File manager context menus on
// Linux come with Phase 6 of linux/docs/PLAN.md; until then 7-Zip is never
// integrated and the switch does nothing.

#include <errno.h>

#include "../../CPP/Common/MyString.h"

#include "../../CPP/7zip/UI/Explorer/RegistryContextMenu.h"

bool CheckContextMenuHandler(const UString & /* path */, UInt32 /* wow */)
{
  return false;
}

LONG SetContextMenuHandler(bool /* setMode */, const UString & /* path */, UInt32 /* wow */)
{
  return ENOTSUP;
}
