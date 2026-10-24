// DirWatcher.cpp

#include "Win32Compat.h"

#include <errno.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "../../CPP/Common/MyVector.h"

#include "../../CPP/Windows/Synchronization.h"

#include "DirWatcher.h"

namespace NPlatform {

static const UInt32 kWatchMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
    | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF;

struct CWatch
{
  int Wd;           // inotify watch descriptor, -1 if the slot is free
  unsigned NumRefs;
};

struct CSubscriber
{
  int Watch;        // index in CWatchers::Watches, -1 if the slot is free
  bool Changed;
};

class CWatchers
{
  int _fd;
  CRecordVector<CWatch> _watches;
  CRecordVector<CSubscriber> _subscribers;

  void MarkWatch(int wd, bool all)
  {
    FOR_VECTOR (i, _subscribers)
    {
      CSubscriber &s = _subscribers[i];
      if (s.Watch >= 0 && (all || _watches[(unsigned)s.Watch].Wd == wd))
        s.Changed = true;
    }
  }

  void Drain()
  {
    if (_fd < 0)
      return;
    // aligned as inotify_event requires
    union
    {
      char Buf[4096];
      struct inotify_event Event;
    } u;
    for (;;)
    {
      const ssize_t n = read(_fd, u.Buf, sizeof(u.Buf));
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        return;
      for (ssize_t pos = 0; pos + (ssize_t)sizeof(struct inotify_event) <= n;)
      {
        const struct inotify_event *e = (const struct inotify_event *)(const void *)(u.Buf + pos);
        if (e->mask & IN_Q_OVERFLOW)
          MarkWatch(-1, true);
        else
          MarkWatch(e->wd, false);
        if (e->mask & IN_IGNORED)
        {
          // the kernel removed the watch: the directory was deleted or unmounted
          FOR_VECTOR (i, _watches)
            if (_watches[i].Wd == e->wd)
              _watches[i].Wd = -2; // keep the slot until its subscribers stop
        }
        pos += (ssize_t)sizeof(struct inotify_event) + e->len;
      }
    }
  }

public:
  NWindows::NSynchronization::CCriticalSection CS;

  CWatchers(): _fd(-1) {}
  ~CWatchers()
  {
    if (_fd >= 0)
      close(_fd);
  }

  int Subscribe(CFSTR path)
  {
    if (_fd < 0)
    {
      _fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
      if (_fd < 0)
        return -1;
    }
    Drain();
    const int wd = inotify_add_watch(_fd, path, kWatchMask | IN_ONLYDIR);
    if (wd < 0)
      return -1;
    int watch = -1;
    int freeWatch = -1;
    FOR_VECTOR (i, _watches)
    {
      if (_watches[i].Wd == wd && _watches[i].NumRefs != 0)
        watch = (int)i;
      else if (_watches[i].NumRefs == 0 && freeWatch < 0)
        freeWatch = (int)i;
    }
    if (watch < 0)
    {
      CWatch w;
      w.Wd = wd;
      w.NumRefs = 0;
      if (freeWatch >= 0)
      {
        watch = freeWatch;
        _watches[(unsigned)watch] = w;
      }
      else
        watch = (int)_watches.Add(w);
    }
    _watches[(unsigned)watch].NumRefs++;

    CSubscriber s;
    s.Watch = watch;
    s.Changed = false;
    FOR_VECTOR (i, _subscribers)
      if (_subscribers[i].Watch < 0)
      {
        _subscribers[i] = s;
        return (int)i;
      }
    return (int)_subscribers.Add(s);
  }

  void Unsubscribe(int id)
  {
    CSubscriber &s = _subscribers[(unsigned)id];
    CWatch &w = _watches[(unsigned)s.Watch];
    s.Watch = -1;
    if (--w.NumRefs == 0)
    {
      if (w.Wd >= 0)
        inotify_rm_watch(_fd, w.Wd);
      w.Wd = -1;
    }
  }

  bool WasChanged(int id)
  {
    Drain();
    CSubscriber &s = _subscribers[(unsigned)id];
    const bool changed = s.Changed;
    s.Changed = false;
    return changed;
  }
};

static CWatchers g_Watchers;

#define WATCHERS_LOCK NWindows::NSynchronization::CCriticalSectionLock lock(g_Watchers.CS);

bool CDirWatcher::Start(CFSTR dirPath)
{
  Stop();
  WATCHERS_LOCK
  _id = g_Watchers.Subscribe(dirPath);
  return _id >= 0;
}

void CDirWatcher::Stop()
{
  if (_id < 0)
    return;
  WATCHERS_LOCK
  g_Watchers.Unsubscribe(_id);
  _id = -1;
}

bool CDirWatcher::WasChanged()
{
  if (_id < 0)
    return false;
  WATCHERS_LOCK
  return g_Watchers.WasChanged(_id);
}

}
