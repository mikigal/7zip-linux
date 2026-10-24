// Platform.h -- Linux-specific helpers of the platform layer.

#ifndef ZIP7_INC_LINUX_PLATFORM_H
#define ZIP7_INC_LINUX_PLATFORM_H

#include "Win32Compat.h"

namespace NPlatform {

/* Sets NWindows::NDLL::GetModuleDirPrefix() -- the directory with Lang/ and
   the other program files -- to the directory of the running executable
   (/proc/self/exe), or of argv0 if that cannot be read. Call it at program
   start. */
void InitModuleDirPrefix(const char *argv0);

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
