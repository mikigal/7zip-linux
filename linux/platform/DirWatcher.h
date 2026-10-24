// DirWatcher.h -- change notification for directories (inotify), the
// POSIX counterpart of NFind::CFindChangeNotification.
//
// All watchers share one inotify instance, so open folders do not use up the
// per-user limit of inotify instances. Watchers of the same directory share
// its watch descriptor.

#ifndef ZIP7_INC_LINUX_DIR_WATCHER_H
#define ZIP7_INC_LINUX_DIR_WATCHER_H

#include "Win32Compat.h"

#include "../../CPP/Common/MyString.h"

namespace NPlatform {

class CDirWatcher
{
  int _id;
public:
  CDirWatcher(): _id(-1) {}
  ~CDirWatcher() { Stop(); }

  // Watches the directory itself (entries created, deleted, renamed or
  // changed), not its subdirectories. Returns false if inotify fails; the
  // directory is then never reported as changed.
  bool Start(CFSTR dirPath);
  void Stop();
  bool IsStarted() const { return _id >= 0; }

  // Returns true once for each batch of changes since the previous call.
  bool WasChanged();
};

}

#endif
