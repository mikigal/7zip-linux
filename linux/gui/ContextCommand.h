// ContextCommand.h -- the commands that file manager context menus give
// 7zG (linux/packaging/*servicemenu*.desktop):
//
//   7zG --context ACTION FILE...
//
// become the 7zG command lines that the context menu of Explorer builds
// (Explorer/ContextMenu.cpp with UI/Common/CompressCall.cpp): the archive
// name comes from CreateArchiveName(), the folder is the folder of the first
// file, and "*" in the output folder stands for the archive name.
// ACTION: extract, extracthere, extractto, test, add, add7z, addzip, email,
// email7z, emailzip, hash:METHOD (CRC32, SHA256, *, ...).

#ifndef ZIP7_INC_LINUX_GUI_CONTEXT_COMMAND_H
#define ZIP7_INC_LINUX_GUI_CONTEXT_COMMAND_H

#include <string>
#include <vector>

namespace NGui {

/* (args) are the arguments after the program name. If they start with
   --context, (command) gets the arguments of the 7zG command and true is
   returned; an unknown action or no files give an error message instead. */
bool TranslateContextCommand(const std::vector<std::string> &args,
    std::vector<std::string> &command, std::string &error);

}

#endif
