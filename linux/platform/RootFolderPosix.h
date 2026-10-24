// RootFolderPosix.h -- the root of the folder tree on POSIX: the home
// directory, "/" and the mounted file systems that belong to the user. It is
// the counterpart of FileManager/RootFolder.h and FSDrives.h ("Computer"),
// with the same class name.

#ifndef ZIP7_INC_LINUX_ROOT_FOLDER_POSIX_H
#define ZIP7_INC_LINUX_ROOT_FOLDER_POSIX_H

#include "Win32Compat.h"

#include "../../CPP/Common/MyCom.h"
#include "../../CPP/Common/MyString.h"

#include "../../CPP/7zip/UI/FileManager/IFolder.h"

namespace NRootFolder {

struct CMountPoint
{
  FString Path;   // no trailing '/', except for "/"
  AString FsType;
  AString Source; // device or remote source
};

// Parses /proc/self/mountinfo. A later mount on the same path hides the earlier one.
void ParseMountInfo(const AString &text, CObjectVector<CMountPoint> &mounts);

// False for pseudo file systems and for system directories (/proc, /var, ...).
bool IsUserMountPoint(const CMountPoint &mp);

}

Z7_CLASS_IMP_NOQIB_1(
  CRootFolder
  , IFolderFolder
)
  struct CItem
  {
    UString Name;
    FString Path;
    AString FsType;
    AString Source;
    bool SizeWasRequested;
    bool SizeDefined;
    UInt64 TotalSize;
    UInt64 FreeSpace;
  };
  CObjectVector<CItem> _items;

  void ReadSize(CItem &item);
public:
  /* Items: the home directory, "/", then the user mount points from
     (mountInfo), or from /proc/self/mountinfo if it is NULL. */
  void Init(const AString *mountInfo = NULL);
};

#endif
