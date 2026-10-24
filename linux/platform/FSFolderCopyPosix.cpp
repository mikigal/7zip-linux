// FSFolderCopyPosix.cpp -- copying and moving in the POSIX file system
// folder: the counterpart of FileManager/FSFolderCopy.cpp.
//
// It follows the Windows code: the callback asks before a file is replaced
// (IFolderOperationsExtractCallback::AskWrite deletes the old file or picks
// a new name), folders are merged into existing ones, and the first error is
// reported through ShowMessage and stops the operation. On POSIX, symbolic
// links are copied as links and never followed, permissions and times are
// kept, and file data is shared (FICLONE) or copied in the kernel
// (copy_file_range) where the file system can do that.

#include "Win32Compat.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <linux/fs.h>

#include "../../CPP/Common/MyBuffer.h"
#include "../../CPP/Common/StringConvert.h"
#include "../../CPP/Common/Wildcard.h"

#include "../../CPP/Windows/ErrorMsg.h"
#include "../../CPP/Windows/FileDir.h"
#include "../../CPP/Windows/TimeUtils.h"

#include "FSFolderPosix.h"

using namespace NWindows;
using namespace NFile;

namespace NFsFolder {

static const size_t kChunkSize = (size_t)1 << 20;

struct CCopyState
{
  IFolderOperationsExtractCallback *Callback;
  bool MoveMode;
  UInt64 TotalSize;
  UInt64 StartPos;
  CByteBuffer Buf;

  CCopyState(): Callback(NULL), MoveMode(false), TotalSize(0), StartPos(0) {}

  HRESULT CallProgress() { return Callback->SetCompleted(&StartPos); }

  HRESULT Message(const UString &message, const FString &path)
  {
    UString s = message;
    s += " : ";
    s += fs2us(path);
    return Callback->ShowMessage(s);
  }

  HRESULT Message(const char *message, const FString &path)
  {
    return Message(MultiByteToUnicodeString(message), path);
  }

  // reports errno; returns E_ABORT unless the callback failed
  HRESULT ErrnoMessage(int error, const FString &path)
  {
    RINOK(Message(NError::MyFormatMessage((DWORD)(error ? error : EIO)), path))
    return E_ABORT;
  }

  HRESULT ErrorMessage(const char *message, const FString &path)
  {
    RINOK(Message(message, path))
    return E_ABORT;
  }
};


static FString CombinePath(const FString &folderPath, const FString &fileName)
{
  FString s (folderPath);
  s.Add_PathSepar();
  s += fileName;
  return s;
}

static bool IsDestChild(const FString &src, const FString &dest)
{
  const unsigned len = src.Len();
  if (dest.Len() < len)
    return false;
  if (dest.Len() != len && dest[len] != FCHAR_PATH_SEPARATOR
      && !(len != 0 && src[len - 1] == FCHAR_PATH_SEPARATOR))
    return false;
  return CompareFileNames(dest.Left(len), src) == 0;
}

// The path without symbolic links, also for a path that does not exist yet:
// the part that exists is resolved, and the rest is appended.
static FString RealPath(const FString &path)
{
  FString rest;
  FString head = path;
  for (;;)
  {
    char *resolved = realpath(head, NULL);
    if (resolved)
    {
      FString s (resolved);
      free(resolved);
      if (!rest.IsEmpty())
      {
        if (s.IsEmpty() || s.Back() != FCHAR_PATH_SEPARATOR)
          s.Add_PathSepar();
        s += rest;
      }
      return s;
    }
    const int pos = head.ReverseFind_PathSepar();
    if (pos < 0 || head.Len() == 1)
      return path;
    const FString name (head.Ptr((unsigned)pos + 1));
    if (!name.IsEmpty())
      rest = rest.IsEmpty() ? name : CombinePath(name, rest);
    head.DeleteFrom(pos == 0 ? 1 : (unsigned)pos);
  }
}

static bool SameFile(const struct stat &a, const struct stat &b)
{
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}


// ---------- files ----------

static bool IsUnsupportedByCopyRange(int error)
{
  return error == EXDEV || error == EINVAL || error == ENOSYS
      || error == EOPNOTSUPP || error == EPERM || error == EBADF || error == ETXTBSY;
}

/* Copies the data of (inFd) to (outFd), with progress. On an error,
   (errorIsRead) tells which file failed and errno is set. */
static HRESULT CopyData(CCopyState &state, int inFd, int outFd, bool &failed, bool &errorIsRead)
{
  failed = false;
  errorIsRead = false;

  // one call where the file system shares the data (btrfs, xfs)
  {
    struct stat st;
    if (fstat(inFd, &st) == 0 && st.st_size > 0 && ioctl(outFd, FICLONE, inFd) == 0)
    {
      state.StartPos += (UInt64)st.st_size;
      return state.CallProgress();
    }
  }

  bool useCopyRange = true;
  for (;;)
  {
    ssize_t n;
    if (useCopyRange)
    {
      n = copy_file_range(inFd, NULL, outFd, NULL, kChunkSize, 0);
      if (n < 0 && errno == EINTR)
        continue;
      /* Some file systems report 0 for files that they cannot copy this
         way (procfs), so read() decides where the end is. */
      if (n <= 0 && (n == 0 || IsUnsupportedByCopyRange(errno)))
      {
        useCopyRange = false;
        continue;
      }
      if (n < 0)
      {
        failed = true;
        return S_OK;
      }
    }
    else
    {
      if (state.Buf.Size() == 0)
        state.Buf.Alloc(kChunkSize);
      n = read(inFd, state.Buf, kChunkSize);
      if (n < 0)
      {
        if (errno == EINTR)
          continue;
        failed = true;
        errorIsRead = true;
        return S_OK;
      }
      if (n == 0)
        break;
      for (ssize_t done = 0; done < n;)
      {
        const ssize_t w = write(outFd, state.Buf + done, (size_t)(n - done));
        if (w < 0)
        {
          if (errno == EINTR)
            continue;
          failed = true;
          return S_OK;
        }
        done += w;
      }
    }
    state.StartPos += (UInt64)n;
    RINOK(state.CallProgress())
  }
  return S_OK;
}

// The permission bits to give a copy: set-user-ID and set-group-ID are kept
// only when the copy has the owner and group of the original.
static mode_t CopyMode(const struct stat &st)
{
  mode_t mode = st.st_mode & 07777;
  if (st.st_uid != geteuid() || st.st_gid != getegid())
    mode &= ~(mode_t)(S_ISUID | S_ISGID);
  return mode;
}

static void SetTimes(int dirFd, const char *path, const struct stat &st, int flags)
{
  const struct timespec times[2] = { st.st_atim, st.st_mtim };
  utimensat(dirFd, path, times, flags);
}

/* Creates (destPath) as a copy of the file, link or special file (srcPath),
   which must not exist. Partial copies are removed. */
static HRESULT CopyItem(CCopyState &state, const FString &srcPath, const struct stat &st,
    const FString &destPath)
{
  if (S_ISLNK(st.st_mode))
  {
    char target[PATH_MAX + 1];
    const ssize_t n = readlink(srcPath, target, PATH_MAX);
    if (n < 0)
      return state.ErrnoMessage(errno, srcPath);
    target[n] = 0;
    if (symlink(target, destPath) != 0)
      return state.ErrnoMessage(errno, destPath);
    SetTimes(AT_FDCWD, destPath, st, AT_SYMLINK_NOFOLLOW);
    state.StartPos += (UInt64)st.st_size;
    return state.CallProgress();
  }

  if (!S_ISREG(st.st_mode))
  {
    // FIFOs and sockets; device files need privileges
    if (mknod(destPath, (st.st_mode & S_IFMT) | CopyMode(st), st.st_rdev) != 0)
      return state.ErrnoMessage(errno, destPath);
    SetTimes(AT_FDCWD, destPath, st, 0);
    return state.CallProgress();
  }

  const int inFd = open(srcPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (inFd < 0)
    return state.ErrnoMessage(errno, srcPath);
  const int outFd = open(destPath, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (outFd < 0)
  {
    const int error = errno;
    close(inFd);
    return state.ErrnoMessage(error, destPath);
  }

  bool failed, errorIsRead;
  HRESULT res = CopyData(state, inFd, outFd, failed, errorIsRead);
  int error = errno;
  if (res == S_OK && !failed)
  {
    fchmod(outFd, CopyMode(st));
    const struct timespec times[2] = { st.st_atim, st.st_mtim };
    futimens(outFd, times);
  }
  close(inFd);
  if (close(outFd) != 0 && res == S_OK && !failed)
  {
    failed = true;
    errorIsRead = false;
    error = errno;
  }
  if (res != S_OK || failed)
  {
    unlink(destPath);
    if (res != S_OK)
      return res;
    return state.ErrnoMessage(error, errorIsRead ? srcPath : destPath);
  }
  return S_OK;
}

// rename() that does not replace an existing (destPath)
static bool RenameNoReplace(const FString &srcPath, const FString &destPath)
{
  if (renameat2(AT_FDCWD, srcPath, AT_FDCWD, destPath, RENAME_NOREPLACE) == 0)
    return true;
  if (errno != EINVAL && errno != ENOSYS)
    return false;
  // a file system without RENAME_NOREPLACE
  struct stat st;
  if (lstat(destPath, &st) == 0)
  {
    errno = EEXIST;
    return false;
  }
  return rename(srcPath, destPath) == 0;
}

static HRESULT CopyFile_Ask(CCopyState &state, const FString &srcPath, const struct stat &srcSt,
    const FString &destPath)
{
  struct stat destSt;
  if (CompareFileNames(destPath, srcPath) == 0
      || (lstat(destPath, &destSt) == 0 && SameFile(srcSt, destSt)))
    return state.ErrorMessage(state.MoveMode ?
        "Cannot move file onto itself" :
        "Cannot copy file onto itself", destPath);

  const bool isFile = S_ISREG(srcSt.st_mode);
  const UInt64 size = isFile ? (UInt64)srcSt.st_size : 0;
  FILETIME mTime;
  FiTime_To_FILETIME(srcSt.st_mtim, mTime);

  Int32 writeAskResult;
  CMyComBSTR destPathResult;
  RINOK(state.Callback->AskWrite(
      fs2us(srcPath),
      BoolToInt(false),
      &mTime, &size,
      fs2us(destPath),
      &destPathResult,
      &writeAskResult))

  if (!IntToBool(writeAskResult))
  {
    if (state.TotalSize >= size)
    {
      state.TotalSize -= size;
      RINOK(state.Callback->SetTotal(state.TotalSize))
    }
    return state.CallProgress();
  }

  const FString destPathNew = us2fs((LPCOLESTR)destPathResult);
  RINOK(state.Callback->SetCurrentFilePath(fs2us(srcPath)))

  if (state.MoveMode)
  {
    if (RenameNoReplace(srcPath, destPathNew))
    {
      state.StartPos += S_ISLNK(srcSt.st_mode) ? (UInt64)srcSt.st_size : size;
      return state.CallProgress();
    }
    if (errno != EXDEV)
      return state.ErrnoMessage(errno, destPathNew);
  }

  RINOK(CopyItem(state, srcPath, srcSt, destPathNew))

  if (state.MoveMode && unlink(srcPath) != 0)
    return state.ErrnoMessage(errno, srcPath);
  return state.CallProgress();
}


// ---------- folders ----------

static HRESULT CopyFolder(CCopyState &state, const FString &srcPath, const struct stat &srcSt,
    const FString &destPath)
{
  RINOK(state.CallProgress())

  // also through symbolic links in the destination path
  if (IsDestChild(srcPath, destPath) || IsDestChild(RealPath(srcPath), RealPath(destPath)))
    return state.ErrorMessage(state.MoveMode ?
        "Cannot move folder onto itself" :
        "Cannot copy folder onto itself", destPath);

  if (state.MoveMode)
  {
    if (RenameNoReplace(srcPath, destPath))
      return S_OK;
    // another file system, or a folder to merge with
  }

  struct stat destSt;
  const bool existed = (stat(destPath, &destSt) == 0);
  if (existed && SameFile(srcSt, destSt))
    return state.ErrorMessage(state.MoveMode ?
        "Cannot move folder onto itself" :
        "Cannot copy folder onto itself", destPath);
  if (!NDir::CreateComplexDir(destPath))
    return state.ErrorMessage("Cannot create folder", destPath);

  DIR *dir = opendir(srcPath);
  if (!dir)
  {
    // as on Windows: the error is reported, and the operation goes on
    state.Message(NError::MyFormatMessage((DWORD)errno), srcPath);
    return S_OK;
  }
  FStringVector names;
  for (;;)
  {
    errno = 0;
    const struct dirent *de = readdir(dir);
    if (!de)
      break;
    const char *name = de->d_name;
    if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
      continue;
    names.Add(FString(name));
  }
  const int readError = errno;
  closedir(dir);
  if (readError != 0)
  {
    state.Message(NError::MyFormatMessage((DWORD)readError), srcPath);
    return S_OK;
  }

  FOR_VECTOR (i, names)
  {
    const FString srcPath2 = CombinePath(srcPath, names[i]);
    const FString destPath2 = CombinePath(destPath, names[i]);
    struct stat st;
    if (lstat(srcPath2, &st) != 0)
      return state.ErrnoMessage(errno, srcPath2);
    if (S_ISDIR(st.st_mode))
    {
      RINOK(CopyFolder(state, srcPath2, st, destPath2))
    }
    else
    {
      RINOK(CopyFile_Ask(state, srcPath2, st, destPath2))
    }
  }

  if (!existed)
  {
    // the times last: copying the contents changed them
    chmod(destPath, CopyMode(srcSt));
    SetTimes(AT_FDCWD, destPath, srcSt, 0);
  }

  if (state.MoveMode)
  {
    if (rmdir(srcPath) != 0)
      return state.ErrorMessage("Cannot remove folder", srcPath);
  }
  return S_OK;
}

static HRESULT CopyPath(CCopyState &state, const FString &srcPath, const struct stat &st,
    const FString &destPath)
{
  if (S_ISDIR(st.st_mode))
    return CopyFolder(state, srcPath, st, destPath);
  return CopyFile_Ask(state, srcPath, st, destPath);
}

// Adds an item to the totals; the size of a link is that of its text.
static HRESULT AddToStat(CFsFolderStat &stat, const FString &path, const struct stat &st)
{
  if (S_ISDIR(st.st_mode))
  {
    stat.Path = path;
    RINOK(stat.Enumerate())
    stat.NumFolders++;
  }
  else
  {
    stat.NumFiles++;
    stat.Size += (UInt64)st.st_size;
  }
  return S_OK;
}


Z7_COM7F_IMF(CFSFolder::CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems,
    Int32 /* includeAltStreams */, Int32 /* replaceAltStreamCharsMode */,
    const wchar_t *path, IFolderOperationsExtractCallback *callback))
{
  if (numItems == 0)
    return S_OK;

  const FString destPath = us2fs(path);
  if (destPath.IsEmpty() || !callback)
    return E_INVALIDARG;

  // without a trailing separator, the path is the new name of one item
  const bool isDirectPath = !IsPathSepar(destPath.Back());
  if (isDirectPath && numItems > 1)
    return E_INVALIDARG;

  CObjArray<struct stat> stats(numItems);
  CFsFolderStat stat;
  stat.Progress = callback;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = indices[i];
    if (index >= Files.Size())
      return E_INVALIDARG;
    FString srcPath;
    GetFullPath(Files[index], srcPath);
    if (lstat(srcPath, &stats[i]) != 0)
    {
      // gone since the folder was read: reported when it is copied
      stats[i].st_mode = 0;
      continue;
    }
    RINOK(AddToStat(stat, srcPath, stats[i]))
  }

  RINOK(callback->SetTotal(stat.Size))
  RINOK(callback->SetNumFiles(stat.NumFiles))
  UInt64 completedSize = 0;
  RINOK(callback->SetCompleted(&completedSize))

  CCopyState state;
  state.Callback = callback;
  state.MoveMode = IntToBool(moveMode);
  state.TotalSize = stat.Size;

  for (i = 0; i < numItems; i++)
  {
    const CDirItem &fi = Files[indices[i]];
    FString srcPath;
    GetFullPath(fi, srcPath);
    if (stats[i].st_mode == 0)
    {
      RINOK(state.Message("Cannot find the file", srcPath))
      continue;
    }
    FString destPath2 = destPath;
    if (!isDirectPath)
      destPath2 += fi.Name;
    RINOK(CopyPath(state, srcPath, stats[i], destPath2))
  }
  return S_OK;
}


/* PanelCopy.cpp calls it for paths that come from outside the panels
   (CDropTarget::Drop() on Windows). */

HRESULT CopyFileSystemItems(
    const UStringVector &itemsPaths,
    const FString &destDirPrefix,
    bool moveMode,
    IFolderOperationsExtractCallback *callback)
{
  if (itemsPaths.IsEmpty())
    return S_OK;
  if (destDirPrefix.IsEmpty() || !callback)
    return E_INVALIDARG;

  CObjArray<struct stat> stats(itemsPaths.Size());
  CFsFolderStat stat;
  stat.Progress = callback;
  FOR_VECTOR (i, itemsPaths)
  {
    if (lstat(us2fs(itemsPaths[i]), &stats[i]) != 0)
    {
      stats[i].st_mode = 0;
      continue;
    }
    RINOK(AddToStat(stat, us2fs(itemsPaths[i]), stats[i]))
  }

  RINOK(callback->SetTotal(stat.Size))
  UInt64 completedSize = 0;
  RINOK(callback->SetCompleted(&completedSize))

  CCopyState state;
  state.Callback = callback;
  state.MoveMode = moveMode;
  state.TotalSize = stat.Size;

  FOR_VECTOR (i, itemsPaths)
  {
    FString srcPath = us2fs(itemsPaths[i]);
    if (stats[i].st_mode == 0)
    {
      RINOK(state.Message("Cannot find the file", srcPath))
      continue;
    }
    // the name of the item: the last part of its path
    while (srcPath.Len() > 1 && IsPathSepar(srcPath.Back()))
      srcPath.DeleteBack();
    const int pos = srcPath.ReverseFind_PathSepar();
    FString destPath = destDirPrefix;
    if (!IsPathSepar(destPath.Back()))
      destPath.Add_PathSepar();
    destPath += srcPath.Ptr((unsigned)(pos + 1));
    RINOK(CopyPath(state, srcPath, stats[i], destPath))
  }
  return S_OK;
}


/* As on Windows, the panel copies into a file system folder through CopyTo()
   of the source folder or CopyFileSystemItems(), not through CopyFrom(). */

Z7_COM7F_IMF(CFSFolder::CopyFrom(Int32 /* moveMode */, const wchar_t * /* fromFolderPath */,
    const wchar_t * const * /* itemsPaths */, UInt32 /* numItems */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSFolder::CopyFromFile(UInt32 /* index */, const wchar_t * /* fullFilePath */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

}
