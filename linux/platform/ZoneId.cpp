// ZoneId.cpp -- the Zone.Identifier helpers of UI/Common/ArchiveExtractCallback
// for POSIX. Upstream defines them for Windows only, but FileManager's
// ExtractCallback.cpp calls them on every system.
//
// Linux file systems have no alternate streams, so a Zone.Identifier stream
// is only recognized (and left out), never written.

#include "Win32Compat.h"

#include "../../CPP/Common/StringConvert.h"
#include "../../CPP/7zip/UI/Common/ArchiveExtractCallback.h"
#include "../../CPP/7zip/UI/Common/ExtractingFilePath.h"

bool Is_ZoneId_StreamName(const wchar_t *s)
{
  UString s2 = s;
  Correct_AltStream_Name(s2);
  return StringsAreEqualNoCase_Ascii(s2, "Zone.Identifier");
}

void ReadZoneFile_Of_BaseFile(CFSTR /* fileName */, CByteBuffer &buf)
{
  buf.Free();
}

bool WriteZoneFile_To_BaseFile(CFSTR /* fileName */, const CByteBuffer & /* buf */)
{
  return false;
}
