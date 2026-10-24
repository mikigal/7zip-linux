// ResourceString.cpp -- Windows/ResourceString.h on top of the string
// tables that rcgen.py generates from the upstream .rc files.

#include "Win32Compat.h"

#include "../../CPP/Windows/ResourceString.h"

#include "RcTables.h"

namespace NWindows {

void MyLoadString(HINSTANCE /* hInstance */, UINT resourceID, UString &dest)
{
  // An unknown ID gives an empty string, as LoadString() does on Windows.
  dest.Empty();
  const wchar_t *s = NRc::FindString(resourceID);
  if (s)
    dest = s;
}

void MyLoadString(UINT resourceID, UString &dest)
{
  MyLoadString(NULL, resourceID, dest);
}

UString MyLoadString(UINT resourceID)
{
  UString s;
  MyLoadString(NULL, resourceID, s);
  return s;
}

}
