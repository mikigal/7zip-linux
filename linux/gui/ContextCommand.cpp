// ContextCommand.cpp

#include "Win32Compat.h"

#include "../../CPP/Common/StringConvert.h"

#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/FileFind.h"

#include "../../CPP/7zip/UI/Common/ArchiveName.h"
#include "../../CPP/7zip/UI/Common/ZipRegistry.h"

#include "ContextCommand.h"

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
  const std::string folder = std::string(folderPrefix.Ptr(), folderPrefix.Len());

  std::vector<std::string> names;
  for (size_t i = 2; i < args.size(); i++)
    names.push_back(args[i]);

  CContextMenuInfo ci;
  ci.Load();

  if (action == "extract" || action == "extracthere" || action == "extractto")
  {
    // the folder of the archive, or a folder named after it ("*")
    command = { "x" };
    if (action == "extract")
      command.push_back("-ad");
    command.push_back("-o" + folder + (action == "extracthere" ? "" : "*/"));
    if (action == "extractto" && ci.ElimDup.Val)
      command.push_back("-spe");
  }
  else if (action == "test")
    command = { "t" };
  else if (action.compare(0, 5, "hash:") == 0 && action.size() > 5)
    command = { "h", "-scrc" + action.substr(5) };
  else if (action == "add" || action == "add7z" || action == "addzip"
      || action == "email" || action == "email7z" || action == "emailzip")
  {
    NFind::CFileInfo fi0;
    const bool one = (files.Size() == 1 && fi0.Find(us2fs(files[0])));
    UString baseName;
    UString arcName = CreateArchiveName(files, false, one ? &fi0 : NULL, baseName);
    const bool showDialog = (action == "add" || action == "email");
    command = { "a" };
    if (!showDialog)
    {
      const bool zip = (action == "addzip" || action == "emailzip");
      command.push_back(zip ? "-tzip" : "-t7z");
      arcName += zip ? ".zip" : ".7z";
    }
    if (action.compare(0, 5, "email") == 0)
      command.push_back("-seml.");
    if (showDialog)
      command.push_back("-ad");
    // with the dialog the type adds its extension
    command.push_back(showDialog ? "-saa" : "-sae");
    command.push_back("--");
    command.push_back(folder + ToStd(arcName));
    command.insert(command.end(), names.begin(), names.end());
    return true;
  }
  else
  {
    error = "7zG: unknown context menu action " + action;
    command.clear();
    return true;
  }
  command.push_back("--");
  command.insert(command.end(), names.begin(), names.end());
  return true;
}

}
