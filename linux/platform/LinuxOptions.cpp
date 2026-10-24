// LinuxOptions.cpp -- the options that the Linux port adds to 7-Zip
// (UpstreamPosix.h), stored with the options of the file manager.

#include "Win32Compat.h"

#include "../../CPP/Windows/Registry.h"

#include "UpstreamPosix.h"

using namespace NWindows::NRegistry;

static LPCTSTR const kFmKey = TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-Zip")
    TEXT(STRING_PATH_SEPARATOR) TEXT("FM");
static LPCTSTR const kDragFilesToOtherPrograms = TEXT("DragFilesToOtherPrograms");

bool ReadDragFilesToOtherPrograms()
{
  bool enable = true;
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kFmKey, KEY_READ) == ERROR_SUCCESS)
    key.GetValue_bool_IfOk(kDragFilesToOtherPrograms, enable);
  return enable;
}

void SaveDragFilesToOtherPrograms(bool enable)
{
  CKey key;
  if (key.Create(HKEY_CURRENT_USER, kFmKey) == ERROR_SUCCESS)
    key.SetValue(kDragFilesToOtherPrograms, enable);
}
