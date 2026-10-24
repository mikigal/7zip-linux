// GuiCommand.h -- the 7zG command lines of the context menu commands, which
// UI/Common/CompressCall.cpp builds on Windows, as argument lists. Windows
// passes the file names to 7zG in shared memory (-i#); here they are -i
// switches without wildcards, since names on Linux can contain * and ?.
// The large pages switch (-slp) and the Zone.Identifier one (-snz) of
// Windows are left out.

#ifndef ZIP7_INC_LINUX_GUI_GUI_COMMAND_H
#define ZIP7_INC_LINUX_GUI_GUI_COMMAND_H

#include "Win32Compat.h"

#include "../../CPP/Common/MyString.h"

namespace NGuiCommand {

// CompressFiles(): adds (names) to (arcPath), with the Add to Archive
// dialog if (showDialog); no (arcPath) lets 7zG choose the name.
void Compress(UStringVector &args, const UString &arcPath, const UString &arcType,
    bool addExtension, const UStringVector &names, bool email, bool showDialog);

// ExtractArchives(): (outFolder) may contain "*", the name of each archive.
void Extract(UStringVector &args, const UStringVector &arcPaths, const UString &outFolder,
    bool showDialog, bool elimDup);

// TestArchives()
void Test(UStringVector &args, const UStringVector &arcPaths, bool hashMode);

// CalcChecksum() without a checksum file: the method (CRC32, SHA256, "*")
// or the default one.
void Hash(UStringVector &args, const UStringVector &paths, const UString &methodName);

// Benchmark()
void Benchmark(UStringVector &args, bool totalMode);

}

#endif
