// CommandLine.cpp -- GetCommandLineW() and the POSIX versions of the
// NCommandLineParser::SplitCommandLine() functions.
//
// The GUI programs read their arguments the Windows way: GUI.cpp and FM.cpp
// split GetCommandLineW(). Upstream defines SplitCommandLine() for Windows
// only, where quotes cannot be escaped, so names with quotes could not be
// passed. Here the command line is built from argv with every argument in
// quotes and with backslash escapes for '"' and '\', and the parser below
// reads that syntax, so every argument comes back unchanged.

#include "Win32Compat.h"

#include "../../CPP/Common/CommandLineParser.h"
#include "../../CPP/Common/StringConvert.h"

#include "Platform.h"

static UString g_CommandLine;

namespace NPlatform {

void SetCommandLine(int argc, const char * const *argv)
{
  g_CommandLine.Empty();
  for (int i = 0; i < argc; i++)
  {
    if (i != 0)
      g_CommandLine.Add_Space();
    const UString arg = MultiByteToUnicodeString(AString(argv[i]));
    g_CommandLine += L'\"';
    for (unsigned k = 0; k < arg.Len(); k++)
    {
      const wchar_t c = arg[k];
      if (c == '\"' || c == '\\')
        g_CommandLine += L'\\';
      g_CommandLine += c;
    }
    g_CommandLine += L'\"';
  }
}

}

WCHAR *GetCommandLineW()
{
  return g_CommandLine.Ptr_non_const();
}

namespace NCommandLineParser {

static bool IsSpace(wchar_t c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Reads one argument starting at pos (after spaces): a quote starts and ends
// a quoted part, and a backslash takes the next character literally.
// Returns false if there is no argument.
static bool ReadArg(const UString &s, unsigned &pos, UString &arg)
{
  arg.Empty();
  while (pos < s.Len() && IsSpace(s[pos]))
    pos++;
  if (pos == s.Len())
    return false;
  bool quoteMode = false;
  for (; pos < s.Len(); pos++)
  {
    const wchar_t c = s[pos];
    if (!quoteMode && IsSpace(c))
      break;
    if (c == '\\' && pos + 1 < s.Len())
      arg += s[++pos];
    else if (c == '\"')
      quoteMode = !quoteMode;
    else
      arg += c;
  }
  return true;
}

bool SplitCommandLine(const UString &src, UString &dest1, UString &dest2)
{
  unsigned pos = 0;
  dest2.Empty();
  if (!ReadArg(src, pos, dest1))
    return false;
  while (pos < src.Len() && IsSpace(src[pos]))
    pos++;
  dest2 = src.Ptr(pos);
  return true;
}

void SplitCommandLine(const UString &s, UStringVector &parts)
{
  parts.Clear();
  unsigned pos = 0;
  UString arg;
  while (ReadArg(s, pos, arg))
    parts.Add(arg);
}

}
