// RegistryIni.h -- Linux-only controls for the INI file behind
// NWindows::NRegistry::CKey (see Registry.cpp).

#ifndef ZIP7_INC_LINUX_REGISTRY_INI_H
#define ZIP7_INC_LINUX_REGISTRY_INI_H

#include "../../CPP/Common/MyString.h"

namespace NWindows {
namespace NRegistry {

/* Path of the INI file that stores HKEY_CURRENT_USER:
   $XDG_CONFIG_HOME/7-Zip/7-Zip.ini, or ~/.config/7-Zip/7-Zip.ini.
   It is empty if no home directory is known; settings are then kept in
   memory only. */
FString Ini_GetFilePath();

/* Uses (path) instead of the default file. An empty path keeps settings in
   memory only. Cached data and unsaved changes are dropped. */
void Ini_SetFilePath(const FString &path);

/* Writes unsaved changes. Changes are also written automatically when the
   last open CKey is closed. */
LONG Ini_Flush();

}}

#endif
