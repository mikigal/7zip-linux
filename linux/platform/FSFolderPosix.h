// FSFolderPosix.h -- a file system directory as IFolderFolder on POSIX: the
// counterpart of FileManager/FSFolder.h, with the same class names.
//
// Items carry the POSIX metadata: permissions (kpidPosixAttrib), owner and
// group names (kpidUser, kpidGroup), symbolic link targets (kpidSymLink),
// inode, link count and status change time. Names that start with '.' get
// FILE_ATTRIBUTE_HIDDEN in kpidAttrib. A symbolic link to a directory is a
// directory; its own mode is shown as its permissions.
//
// Copying and moving (IFolderOperations::CopyTo, CopyFrom, CopyFromFile) are
// not implemented yet.

#ifndef ZIP7_INC_LINUX_FS_FOLDER_POSIX_H
#define ZIP7_INC_LINUX_FS_FOLDER_POSIX_H

#include "Win32Compat.h"

#include <sys/types.h>

#include "../../CPP/Common/MyCom.h"
#include "../../CPP/Windows/TimeUtils.h"

#include "../../CPP/7zip/UI/FileManager/IFolder.h"
#include "../../CPP/7zip/UI/FileManager/TextPairs.h"

#include "DirWatcher.h"

namespace NFsFolder {

struct CDirItem
{
  FString Name;
  FString LinkTarget;  // for symbolic links
  int Parent;          // index in CFSFolder::Folders (flat mode), or -1
  bool IsDir;          // a directory or a symbolic link to one
  bool IsLink;
  bool CTime_Defined;  // birth time; not every file system has it
  bool FolderStat_Defined;
  mode_t Mode;         // of the item itself (S_IFLNK for links)
  uid_t Uid;
  gid_t Gid;
  UInt32 NumLinks;
  UInt64 INode;
  UInt64 Size;         // of the link target for links
  UInt64 PackSize;     // allocated size
  CFiTime MTime;
  CFiTime ATime;
  CFiTime CTime;       // birth time
  CFiTime ChangeTime;  // status change time (st_ctime)
  UInt64 NumFolders;
  UInt64 NumFiles;

  UInt32 GetWinAttrib() const;
};

struct CFsFolderStat
{
  UInt64 NumFolders;
  UInt64 NumFiles;
  UInt64 Size;
  IProgress *Progress;
  FString Path;

  CFsFolderStat(): NumFolders(0), NumFiles(0), Size(0), Progress(NULL) {}
  CFsFolderStat(const FString &path, IProgress *progress Z7_lifetimebound = NULL):
      NumFolders(0), NumFiles(0), Size(0), Progress(progress), Path(path) {}

  // Does not follow symbolic links.
  HRESULT Enumerate();
};

class CFSFolder Z7_final:
  public IFolderFolder,
  public IFolderCompare,
  public IFolderWasChanged,
  public IFolderOperations,
  public IFolderCalcItemFullSize,
  public IFolderClone,
  public IFolderSetFlatMode,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderFolder)
    Z7_COM_QI_ENTRY(IFolderCompare)
    Z7_COM_QI_ENTRY(IFolderWasChanged)
    Z7_COM_QI_ENTRY(IFolderOperations)
    Z7_COM_QI_ENTRY(IFolderCalcItemFullSize)
    Z7_COM_QI_ENTRY(IFolderClone)
    Z7_COM_QI_ENTRY(IFolderSetFlatMode)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IFolderFolder)
  Z7_IFACE_COM7_IMP(IFolderCompare)
  Z7_IFACE_COM7_IMP(IFolderWasChanged)
  Z7_IFACE_COM7_IMP(IFolderOperations)
  Z7_IFACE_COM7_IMP(IFolderCalcItemFullSize)
  Z7_IFACE_COM7_IMP(IFolderClone)
  Z7_IFACE_COM7_IMP(IFolderSetFlatMode)

  bool _flatMode;
  bool _commentsAreLoaded;
  FString _path;
  CObjectVector<CDirItem> Files;
  FStringVector Folders;
  CPairsStorage _comments;
  NPlatform::CDirWatcher _watcher;

  // names of user and group IDs
  CRecordVector<UInt32> _userIds;
  UStringVector _userNames;
  CRecordVector<UInt32> _groupIds;
  UStringVector _groupNames;

  void GetAbsPath(const wchar_t *name, FString &absPath);
  HRESULT BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder);
  bool LoadComments();
  bool SaveComments();
  HRESULT LoadSubItems(int dirItem, const FString &relPrefix);
  const UString &GetUserName(uid_t uid);
  const UString &GetGroupName(gid_t gid);

public:
  // (path) is a directory path with a trailing '/'.
  HRESULT Init(const FString &path);
  HRESULT InitToRoot() { return Init((FString) FSTRING_PATH_SEPARATOR); }

  CFSFolder():
      _flatMode(false),
      _commentsAreLoaded(false)
      {}

  void GetFullPath(const CDirItem &item, FString &path) const
  {
    path = _path;
    if (item.Parent >= 0)
      path += Folders[item.Parent];
    path += item.Name;
  }

  FString GetRelPath(const CDirItem &item) const;

  void Clear()
  {
    Files.Clear();
    Folders.Clear();
  }
};

/* Deletes a file, or a directory with its contents, without following
   symbolic links. */
bool RemoveTree(const FString &path);

/* Copies or moves files and directories (full paths) into destDirPrefix
   (FileManager/FSFolder.h). Not implemented yet: returns E_NOTIMPL. */
HRESULT CopyFileSystemItems(
    const UStringVector &itemsPaths,
    const FString &destDirPrefix,
    bool moveMode,
    IFolderOperationsExtractCallback *callback);

}

#endif
