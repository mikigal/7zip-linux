// Platform.h -- Linux-specific helpers of the platform layer.

#ifndef ZIP7_INC_LINUX_PLATFORM_H
#define ZIP7_INC_LINUX_PLATFORM_H

#include "Win32Compat.h"

#include "../../CPP/Common/MyString.h"

namespace NPlatform {

/* Sets NWindows::NDLL::GetModuleDirPrefix() -- the directory with Lang/ and
   the other program files -- to the directory of the running executable
   (/proc/self/exe), or of argv0 if that cannot be read. Call it at program
   start. */
void InitModuleDirPrefix(const char *argv0);

/* Sets NWindows::NDLL::GetModuleDirPrefix() to a directory, for 7-Zip code
   that runs in another program (the plugin of KDE file managers): the
   directory of 7zG, 7zFM and Lang/. */
void SetModuleDir(const FString &dir);

/* Keeps a folder that a CTempDir would remove until the program exits: a
   drop target that copies the files of a drag after the drag has ended
   (GTK file managers) still reads what 7-Zip extracted for it there. */
void KeepTempDirUntilExit(const FString &path);

/* The home directory ($HOME, else the password database), without a
   trailing '/'. */
FString GetHomeDir();

/* Sets the command line that GetCommandLineW() returns from the arguments
   of main() (argv[0] included). Call it at program start. */
void SetCommandLine(int argc, const char * const *argv);

/* Maps a POSIX locale name ("pl_PL.UTF-8", "sr_RS@latin", "C") to the
   Windows LANGID whose 7-Zip language file names (see kLangs in
   FileManager/LangUtils.cpp) match it. Unknown languages, "C" and "POSIX"
   give English (US). */
LANGID LangIdFromLocale(const char *locale);

/* The locale name for messages: the first entry of $LANGUAGE (unless the
   locale is "C"), else $LC_ALL, $LC_MESSAGES or $LANG. */
AString GetMessagesLocale();

}

#endif
