// ContextCommand.cpp

#include "Win32Compat.h"

#include "../../CPP/Common/StringConvert.h"

#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/FileFind.h"

#include "../../CPP/7zip/UI/Common/ArchiveName.h"
#include "../../CPP/7zip/UI/Common/ZipRegistry.h"

#include "ContextCommand.h"
#include "GuiCommand.h"

using namespace NWindows;
using namespace NFile;

namespace NGui {

static std::string ToStd(const UString &s)
{
  const FString f = us2fs(s);
  return std::string(f.Ptr(), f.Len());
}

bool TranslateContextCommand(const std::vector<std::string> &args,
    std::vector<std::string> &command, std::string &error)
{
  command.clear();
  error.clear();
  if (args.empty() || args[0] != "--context")
    return false;
  if (args.size() < 3)
  {
    error = "usage: 7zG --context ACTION FILE...";
    return true;
  }
  const std::string action = args[1];
  UStringVector files;
  for (size_t i = 2; i < args.size(); i++)
    files.Add(fs2us(FString(args[i].c_str())));

  // the folder of the first file
  FString folderPrefix;
  NDir::GetOnlyDirPrefix(us2fs(files[0]), folderPrefix);
  const UString folder = fs2us(folderPrefix);

  CContextMenuInfo ci;
  ci.Load();

  UStringVector cmd;
  if (action == "extract" || action == "extracthere" || action == "extractto")
  {
    // the folder of the archive, or a folder named after it ("*")
    NGuiCommand::Extract(cmd, files,
        action == "extracthere" ? folder : folder + UString("*/"),
        action == "extract", // showDialog
        action == "extractto" && ci.ElimDup.Val);
  }
  else if (action == "test")
    NGuiCommand::Test(cmd, files, false);
  else if (action.compare(0, 5, "hash:") == 0 && action.size() > 5)
    NGuiCommand::Hash(cmd, files, fs2us(FString(action.substr(5).c_str())));
  else if (action == "add" || action == "add7z" || action == "addzip"
      || action == "email" || action == "email7z" || action == "emailzip")
  {
    NFind::CFileInfo fi0;
    const bool one = (files.Size() == 1 && fi0.Find(us2fs(files[0])));
    UString baseName;
    UString arcName = CreateArchiveName(files, false, one ? &fi0 : NULL, baseName);
    const bool showDialog = (action == "add" || action == "email");
    UString arcType;
    if (!showDialog)
    {
      // with the dialog the type adds its extension
      arcType = (action == "addzip" || action == "emailzip") ? "zip" : "7z";
      arcName.Add_Dot();
      arcName += arcType;
    }
    NGuiCommand::Compress(cmd, folder + arcName, arcType,
        showDialog, // addExtension
        files,
        action.compare(0, 5, "email") == 0,
        showDialog);
  }
  else
  {
    error = "7zG: unknown context menu action " + action;
    return true;
  }
  FOR_VECTOR (i, cmd)
    command.push_back(ToStd(cmd[i]));
  return true;
}

}
