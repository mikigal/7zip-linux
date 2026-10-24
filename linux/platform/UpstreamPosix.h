// UpstreamPosix.h -- helpers of the upstream CPP/Windows code that exist
// only in its Windows branches, but that the file manager sources
// (UI/FileManager, UI/Explorer) call on every system. They have their
// POSIX meaning here: there are no super (\\?\) or device (\\.\) paths,
// the "drives" are the mounted file systems.
//
// The 7zFM build force-includes this header (-include, after the shim's
// windows.h), so those sources build unchanged; linux/platform implements
// the functions (FileDirPosix.cpp, RootFolderPosix.cpp).

#ifndef ZIP7_INC_LINUX_UPSTREAM_POSIX_H
#define ZIP7_INC_LINUX_UPSTREAM_POSIX_H

#ifdef _WIN32
#error "linux/platform is for non-Windows builds only"
#endif

#include "../../CPP/Common/MyString.h"

#include "../../CPP/Windows/PropVariantConv.h"
#include "../../CPP/Windows/Synchronization.h"
#include "../../CPP/Windows/TimeUtils.h"

// File times of CFileInfo are timespec (CFiTime) on POSIX; the file
// manager formats and tests them as FILETIMEs.
inline bool FILETIME_IsZero(const CFiTime &ts)
{
  return ts.tv_sec == 0 && ts.tv_nsec == 0;
}

inline bool ConvertUtcFileTimeToString(const CFiTime &ts, char *s, int level = kTimestampPrintLevel_SEC) throw()
{
  FILETIME ft;
  FiTime_To_FILETIME(ts, ft);
  return ConvertUtcFileTimeToString(ft, s, level);
}

inline bool ConvertUtcFileTimeToString(const CFiTime &ts, wchar_t *s, int level = kTimestampPrintLevel_SEC) throw()
{
  FILETIME ft;
  FiTime_To_FILETIME(ts, ft);
  return ConvertUtcFileTimeToString(ft, s, level);
}

// The events of Windows/Synchronization.h have no HANDLE on POSIX: this
// waits for one at most (milliseconds), and returns WAIT_OBJECT_0 when it
// is set, else WAIT_TIMEOUT (0 and 258, as on Windows).
DWORD WaitForSingleObject(NWindows::NSynchronization::CBaseEvent &event, DWORD milliseconds);

namespace NWindows {
namespace NFile {

namespace NName {

inline bool IsDevicePath(CFSTR /* s */) throw() { return false; }
// "path:" of NTFS alternate streams
inline bool IsAltStreamPrefixWithColon(const UString & /* s */) throw() { return false; }
// returns true, if a super prefix was removed
inline bool If_IsSuperPath_RemoveSuperPrefix(UString & /* s */) { return false; }

}

namespace NFind {

// "/" and the mount points that RootFolder shows, each with a trailing '/'.
bool MyGetLogicalDriveStrings(CObjectVector<FString> &driveStrings);

}

namespace NDir {

// A new directory in the temporary directory ($TMPDIR or /tmp), removed
// with its contents unless DisableDeleting() is called (as CTempDir of
// Windows/FileDir.h).
class CTempDir  MY_UNCOPYABLE
{
  bool _mustBeDeleted;
  FString _path;
public:
  CTempDir(): _mustBeDeleted(false) {}
  ~CTempDir() { Remove(); }
  const FString &GetPath() const Z7_lifetimebound { return _path; }
  void DisableDeleting() { _mustBeDeleted = false; }
  bool Create(CFSTR namePrefix);
  bool Remove();
};

}

}}

#endif
