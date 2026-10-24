// RootFolderNames.cpp -- the names of the special folders that the address
// combo box of the panel lists (from FileManager/RootFolder.cpp, which is
// the Windows root folder): "Computer" is the root folder, "Documents" the
// home directory, and there is no network folder.

#include <ShlObj.h>

#include "../../CPP/Common/MyString.h"
#include "../../CPP/Common/StringConvert.h"

#include "../../CPP/7zip/UI/FileManager/LangUtils.h"
#include "../../CPP/7zip/UI/FileManager/SysIconUtils.h"
#include "../../CPP/7zip/UI/FileManager/resource.h"

#include "Platform.h"

UString RootFolder_GetName_Computer(int &iconIndex);
UString RootFolder_GetName_Computer(int &iconIndex)
{
  iconIndex = Shell_GetFileInfo_SysIconIndex_for_CSIDL(CSIDL_DRIVES);
  return LangString(IDS_COMPUTER);
}

UString RootFolder_GetName_Network(int &iconIndex);
UString RootFolder_GetName_Network(int &iconIndex)
{
  iconIndex = -1;
  return UString();
}

UString RootFolder_GetName_Documents(int &iconIndex);
UString RootFolder_GetName_Documents(int &iconIndex)
{
  iconIndex = Shell_GetFileInfo_SysIconIndex_for_CSIDL(CSIDL_PERSONAL);
  return fs2us(NPlatform::GetHomeDir());
}
