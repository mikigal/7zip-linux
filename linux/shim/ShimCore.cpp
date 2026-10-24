// ShimCore.cpp -- window handles, threads, messages and timers of the user32
// subset over Qt, and the generic window functions.

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QEventLoop>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <map>
#include <unordered_set>

#include "ShimDialog.h"
#include "ShimWindow.h"

namespace NShim {

// ---------- Threads ----------

bool IsGuiThread()
{
  const QCoreApplication *app = QCoreApplication::instance();
  return app && QThread::currentThread() == app->thread();
}

void RunOnGuiThread(const std::function<void()> &f)
{
  if (IsGuiThread())
  {
    f();
    return;
  }
  QCoreApplication *app = QCoreApplication::instance();
  if (!app)
    return;
  QMetaObject::invokeMethod(app, f, Qt::BlockingQueuedConnection);
}

static void RunVoid(const std::function<void()> &f)
{
  RunOnGuiThread(f);
}

// ---------- Handles ----------

static std::unordered_set<HWND> g_Windows;
static QHash<const QObject *, HWND> g_WidgetWindows;
static std::atomic<UInt64> g_NextSerial(1);

void RegisterWindow(HWND wnd, QWidget *widget)
{
  wnd->Widget = widget;
  wnd->Proxy = new CEventProxy(wnd, widget);
  g_Windows.insert(wnd);
  g_WidgetWindows.insert(widget, wnd);
}

HWND FromHandle(HWND wnd)
{
  if (!wnd || g_Windows.find(wnd) == g_Windows.end())
    return NULL;
  return wnd;
}

HWND FromWidget(const QObject *widget)
{
  for (const QObject *o = widget; o; o = o->parent())
  {
    const auto it = g_WidgetWindows.constFind(o);
    if (it != g_WidgetWindows.constEnd())
      return *it;
  }
  return NULL;
}

QWidget *WidgetOf(HWND wnd)
{
  return OnGuiThread<QWidget *>([=]() -> QWidget *
  {
    HWND w = FromHandle(wnd);
    return w ? w->Widget.data() : NULL;
  });
}

QWidget *ParentWidgetFor(HWND parent)
{
  parent = FromHandle(parent);
  return parent ? parent->Widget.data() : NULL;
}

CWindowLock::CWindowLock(HWND wnd): _wnd(FromHandle(wnd))
{
  if (_wnd)
    _wnd->Locks++;
}

CWindowLock::~CWindowLock()
{
  if (_wnd && --_wnd->Locks == 0 && _wnd->Dead)
    delete _wnd;
}

bool CEventProxy::eventFilter(QObject *watched, QEvent *event)
{
  CWindowLock lock(_wnd);
  if (!lock)
    return false;
  return lock.Get()->OnEvent(watched, event);
}

static void Unregister(HWND wnd)
{
  g_Windows.erase(wnd);
  if (wnd->Widget)
    g_WidgetWindows.remove(wnd->Widget.data());
  if (wnd->Proxy)
    wnd->Proxy->Detach();
  if (wnd->Parent)
  {
    std::vector<HWND> &c = wnd->Parent->Children;
    for (size_t i = 0; i < c.size(); i++)
      if (c[i] == wnd)
      {
        c.erase(c.begin() + (ptrdiff_t)i);
        break;
      }
  }
}

static void DestroyRecursive(HWND wnd)
{
  wnd->Destroying = true;
  KillAllTimers(wnd);
  Send(wnd, WM_DESTROY, 0, 0);
  const std::vector<HWND> children = wnd->Children;
  for (HWND c : children)
    if (FromHandle(c) && !c->Destroying)
      DestroyRecursive(c);
  Send(wnd, WM_NCDESTROY, 0, 0);
}

bool DestroyWindowTree(HWND wnd)
{
  wnd = FromHandle(wnd);
  if (!wnd)
    return false;
  if (wnd->Destroying)
    return true;
  CWindowLock lock(wnd);
  QWidget *widget = wnd->Widget.data();
  if (widget && !wnd->IsChild())
    widget->hide();
  DestroyRecursive(wnd);

  // Unregister bottom-up: owned top-level windows and children first.
  std::vector<HWND> all;
  all.push_back(wnd);
  for (size_t i = 0; i < all.size(); i++)
    for (HWND c : all[i]->Children)
      all.push_back(c);
  for (size_t i = all.size(); i != 0; i--)
  {
    HWND w = all[i - 1];
    Unregister(w);
    w->Dead = true;
  }
  if (widget)
  {
    if (wnd->IsChild())
      widget->hide();
    // The widget may be in a signal or event handler right now.
    widget->deleteLater();
  }
  for (size_t i = all.size(); i > 1; i--)
  {
    HWND w = all[i - 1];
    if (w->Locks == 0)
      delete w;
  }
  return true;
}

}

using namespace NShim;

HWND__::HWND__(NShim::EKind kind):
    Kind(kind),
    Serial(g_NextSerial++),
    Parent(NULL),
    Id(0),
    Style(0),
    ExStyle(0),
    UserData(0),
    WndProc(NULL),
    Destroying(false),
    Dead(false),
    AlwaysDisabled(false),
    AlwaysHidden(false),
    Locks(0),
    Proxy(NULL)
{
}

HWND__::~HWND__()
{
  // DestroyWindowTree() unregisters a window before deleting it; this handles
  // objects that never got that far.
  if (FromHandle(this))
    Unregister(this);
}

HWND HWND__::TopLevel()
{
  HWND w = this;
  while (w->IsChild() && w->Parent)
    w = w->Parent;
  return w;
}

LRESULT HWND__::Proc(UINT message, WPARAM wParam, LPARAM lParam)
{
  return DefWindowProcImpl(this, message, wParam, lParam);
}

bool HWND__::OnEvent(QObject * /* watched */, QEvent * /* event */)
{
  return false;
}

void HWND__::SetText(const QString &s)
{
  if (!IsChild() && Widget)
    Widget->setWindowTitle(s);
  else
    TextValue = s;
}

QString HWND__::Text() const
{
  if (!IsChild() && Widget)
    return Widget->windowTitle();
  return TextValue;
}

void HWND__::NotifyCommand(UINT code)
{
  if (Parent && !Destroying)
    Send(Parent, WM_COMMAND, MAKEWPARAM(Id, code), (LPARAM)this);
}

LRESULT HWND__::NotifyParent(NMHDR *hdr, UINT code)
{
  if (!Parent || Destroying)
    return 0;
  hdr->hwndFrom = this;
  hdr->idFrom = Id;
  hdr->code = code;
  return Send(Parent, WM_NOTIFY, Id, (LPARAM)hdr);
}

namespace NShim {

// ---------- Text ----------

QString FromW(LPCWSTR s)
{
  return s ? QString::fromWCharArray(s) : QString();
}

QString FromW(LPCWSTR s, size_t len)
{
  return s ? QString::fromWCharArray(s, (qsizetype)len) : QString();
}

QString FromA(LPCSTR s)
{
  return s ? QString::fromUtf8(s) : QString();
}

QString FromU(const UString &s)
{
  return QString::fromWCharArray(s.Ptr(), (qsizetype)s.Len());
}

UString ToU(const QString &s)
{
  UString u;
  const std::wstring w = s.toStdWString();
  u.SetFrom(w.c_str(), (unsigned)w.size());
  return u;
}

int CopyToW(const QString &s, LPWSTR buf, int maxCount)
{
  if (!buf || maxCount <= 0)
    return 0;
  const std::wstring w = s.toStdWString();
  size_t n = w.size();
  if (n > (size_t)maxCount - 1)
    n = (size_t)maxCount - 1;
  wmemcpy(buf, w.c_str(), n);
  buf[n] = 0;
  return (int)n;
}

int CopyToA(const QString &s, LPSTR buf, int maxCount)
{
  if (!buf || maxCount <= 0)
    return 0;
  const QByteArray a = s.toUtf8();
  size_t n = (size_t)a.size();
  if (n > (size_t)maxCount - 1)
  {
    n = (size_t)maxCount - 1;
    // do not cut a UTF-8 sequence
    while (n > 0 && ((Byte)a[(qsizetype)n] & 0xC0) == 0x80)
      n--;
  }
  memcpy(buf, a.constData(), n);
  buf[n] = 0;
  return (int)n;
}

int LengthA(const QString &s)
{
  return (int)s.toUtf8().size();
}

int LengthW(const QString &s)
{
  return (int)s.toStdWString().size();
}

QString StripMnemonic(const QString &s)
{
  QString r;
  r.reserve(s.size());
  for (qsizetype i = 0; i < s.size(); i++)
  {
    if (s[i] == QLatin1Char('&'))
    {
      if (i + 1 < s.size() && s[i + 1] == QLatin1Char('&'))
        r += QLatin1Char('&');
      i++;
      if (i < s.size() && s[i] != QLatin1Char('&'))
        r += s[i];
      continue;
    }
    r += s[i];
  }
  return r;
}

// ---------- Messages ----------

LRESULT Send(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindowLock lock(wnd);
  if (!lock)
    return 0;
  if (wnd->WndProc)
    return wnd->WndProc(wnd, message, wParam, lParam);
  return wnd->Proc(message, wParam, lParam);
}

static int g_QuitCode = 0;
static bool g_QuitPosted = false;

void Post(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  const UInt64 serial = wnd->Serial;
  QCoreApplication *app = QCoreApplication::instance();
  if (!app)
    return;
  QMetaObject::invokeMethod(app, [wnd, serial, message, wParam, lParam]()
  {
    // The window may be gone, and its address reused by a new window.
    if (FromHandle(wnd) && wnd->Serial == serial && !wnd->Destroying)
      Send(wnd, message, wParam, lParam);
  }, Qt::QueuedConnection);
}

LRESULT DefWindowProcImpl(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_SETTEXT:
      wnd->SetText(FromW((LPCWSTR)lParam));
      return TRUE;
    case WM_GETTEXT:
      return CopyToW(wnd->Text(), (LPWSTR)lParam, (int)wParam);
    case WM_GETTEXTLENGTH:
      return LengthW(wnd->Text());
    case WM_CLOSE:
      DestroyWindowTree(wnd);
      return 0;
    case WM_HELP:
    case WM_CONTEXTMENU:
      // a child passes these to its parent
      if (wnd->IsChild() && wnd->Parent)
        return Send(wnd->Parent, message, wParam, lParam);
      return 0;
    case WM_SETREDRAW:
      if (wnd->Widget)
        wnd->Widget->setUpdatesEnabled(wParam != 0);
      return 0;
    case WM_SETICON:
      if (wnd->Widget && !wnd->IsChild())
        wnd->Widget->setWindowIcon(IconFromHandle((HICON)lParam));
      return 0;
    case WM_GETDLGCODE:
      return 0;
    default:
      return 0;
  }
}

// ---------- Input ----------

UINT VirtualKeyFromQt(const QKeyEvent *e)
{
  const int key = e->key();
  if (key >= Qt::Key_A && key <= Qt::Key_Z)
    return (UINT)('A' + (key - Qt::Key_A));
  if (key >= Qt::Key_0 && key <= Qt::Key_9)
  {
    if (e->modifiers() & Qt::KeypadModifier)
      return (UINT)(VK_NUMPAD0 + (key - Qt::Key_0));
    return (UINT)('0' + (key - Qt::Key_0));
  }
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
    return (UINT)(VK_F1 + (key - Qt::Key_F1));
  switch (key)
  {
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Shift: return VK_SHIFT;
    case Qt::Key_Control: return VK_CONTROL;
    case Qt::Key_Alt: return VK_MENU;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_CapsLock: return VK_CAPITAL;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_End: return VK_END;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Menu: return VK_APPS;
    case Qt::Key_Asterisk: return VK_MULTIPLY;
    case Qt::Key_Plus: return (e->modifiers() & Qt::KeypadModifier) ? VK_ADD : 0xBB;       // VK_OEM_PLUS
    case Qt::Key_Minus: return (e->modifiers() & Qt::KeypadModifier) ? VK_SUBTRACT : 0xBD; // VK_OEM_MINUS
    case Qt::Key_Slash: return (e->modifiers() & Qt::KeypadModifier) ? VK_DIVIDE : 0xBF;   // VK_OEM_2
    case Qt::Key_Period: return (e->modifiers() & Qt::KeypadModifier) ? VK_DECIMAL : 0xBE; // VK_OEM_PERIOD
    case Qt::Key_Comma: return 0xBC;      // VK_OEM_COMMA
    case Qt::Key_Semicolon: return 0xBA;  // VK_OEM_1
    case Qt::Key_QuoteLeft: return 0xC0;  // VK_OEM_3
    case Qt::Key_Backslash: return 0xDC;  // VK_OEM_5
    case Qt::Key_NumLock: return VK_NUMLOCK;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    default: return 0;
  }
}

// ---------- Timers ----------

struct CTimerKey
{
  HWND Wnd;
  UINT_PTR Id;
  bool operator<(const CTimerKey &k) const
    { return Wnd != k.Wnd ? Wnd < k.Wnd : Id < k.Id; }
};

static std::map<CTimerKey, QTimer *> g_Timers;

void KillAllTimers(HWND wnd)
{
  for (auto it = g_Timers.begin(); it != g_Timers.end();)
  {
    if (it->first.Wnd == wnd)
    {
      it->second->stop();
      it->second->deleteLater();
      it = g_Timers.erase(it);
    }
    else
      ++it;
  }
}

}

// ---------- Messages: API ----------

/* SendMessageA() converts the text of the narrow messages that the 7-Zip code
   sends to the wide form, because the built-in window procedures are wide
   only. List view and ComboBoxEx narrow messages are mapped in their files. */

namespace NShim {
LRESULT SendNarrowListViewMessage(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam, bool &handled);
}

static LRESULT SendMessageA_Gui(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (!FromHandle(wnd))
    return 0;
  switch (message)
  {
    case WM_SETTEXT:
    case CB_ADDSTRING:
    case CB_INSERTSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_SELECTSTRING:
    case EM_REPLACESEL:
    {
      if (!lParam)
        return Send(wnd, message, wParam, lParam);
      const std::wstring w = FromA((LPCSTR)lParam).toStdWString();
      return Send(wnd, message, wParam, (LPARAM)w.c_str());
    }
    case WM_GETTEXT:
    {
      const int len = (int)Send(wnd, WM_GETTEXTLENGTH, 0, 0);
      std::wstring w((size_t)len + 1, L'\0');
      const int n = (int)Send(wnd, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)&w[0]);
      return CopyToA(FromW(w.c_str(), (size_t)n), (LPSTR)lParam, (int)wParam);
    }
    case WM_GETTEXTLENGTH:
    {
      const int len = (int)Send(wnd, WM_GETTEXTLENGTH, 0, 0);
      std::wstring w((size_t)len + 1, L'\0');
      const int n = (int)Send(wnd, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)&w[0]);
      return LengthA(FromW(w.c_str(), (size_t)n));
    }
    case CB_GETLBTEXT:
    case CB_GETLBTEXTLEN:
    {
      const LRESULT len = Send(wnd, CB_GETLBTEXTLEN, wParam, 0);
      if (len == CB_ERR)
        return CB_ERR;
      std::wstring w((size_t)len + 1, L'\0');
      Send(wnd, CB_GETLBTEXT, wParam, (LPARAM)&w[0]);
      const QString s = FromW(w.c_str(), (size_t)len);
      if (message == CB_GETLBTEXTLEN)
        return LengthA(s);
      const QByteArray a = s.toUtf8();
      memcpy((void *)lParam, a.constData(), (size_t)a.size() + 1);
      return a.size();
    }
    default:
    {
      bool handled = false;
      const LRESULT r = SendNarrowListViewMessage(wnd, message, wParam, lParam, handled);
      if (handled)
        return r;
      return Send(wnd, message, wParam, lParam);
    }
  }
}

LRESULT SendMessageA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return OnGuiThread<LRESULT>([=]() { return SendMessageA_Gui(wnd, message, wParam, lParam); });
}

LRESULT SendMessageW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return OnGuiThread<LRESULT>([=]() { return Send(wnd, message, wParam, lParam); });
}

BOOL PostMessageW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (!wnd)
  {
    // a thread message: only WM_QUIT is used
    if (message == WM_QUIT)
      PostQuitMessage((int)wParam);
    return TRUE;
  }
  // FromHandle() reads the handle table, which belongs to the GUI thread.
  return OnGuiThread<BOOL>([=]()
  {
    if (!FromHandle(wnd))
      return FALSE;
    Post(wnd, message, wParam, lParam);
    return TRUE;
  });
}

BOOL PostMessageA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // posted messages with text are not used
  return PostMessageW(wnd, message, wParam, lParam);
}

// GetWindowLongPtr(GWLP_WNDPROC) of a window that uses its built-in procedure.
static LRESULT CALLBACK BuiltinWndProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindowLock lock(wnd);
  if (!lock)
    return 0;
  return wnd->Proc(message, wParam, lParam);
}

LRESULT CallWindowProcW(WNDPROC proc, HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return OnGuiThread<LRESULT>([=]()
  {
    if (!proc || proc == BuiltinWndProc)
      return BuiltinWndProc(wnd, message, wParam, lParam);
    return proc(wnd, message, wParam, lParam);
  });
}

LRESULT CallWindowProcA(WNDPROC proc, HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return CallWindowProcW(proc, wnd, message, wParam, lParam);
}

LRESULT DefWindowProcW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return OnGuiThread<LRESULT>([=]()
  {
    CWindowLock lock(wnd);
    return lock ? DefWindowProcImpl(wnd, message, wParam, lParam) : 0;
  });
}

LRESULT DefWindowProcA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_SETTEXT && lParam)
  {
    const std::wstring w = FromA((LPCSTR)lParam).toStdWString();
    return DefWindowProcW(wnd, message, wParam, (LPARAM)w.c_str());
  }
  return DefWindowProcW(wnd, message, wParam, lParam);
}

/* The message loop. Qt dispatches events itself, so GetMessage() runs the Qt
   event loop until events were processed and returns them as one WM_NULL
   message; TranslateMessage(), DispatchMessage(), IsDialogMessage() and
   TranslateAccelerator() do nothing with it. PeekMessage() processes the
   pending events and reports an empty queue. */

BOOL GetMessageW(LPMSG msg, HWND /* wnd */, UINT /* filterMin */, UINT /* filterMax */)
{
  memset(msg, 0, sizeof(*msg));
  if (!g_QuitPosted)
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
  if (g_QuitPosted)
  {
    g_QuitPosted = false;
    msg->message = WM_QUIT;
    msg->wParam = (WPARAM)g_QuitCode;
    return FALSE;
  }
  msg->message = WM_NULL;
  return TRUE;
}

BOOL GetMessageA(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax)
{
  return GetMessageW(msg, wnd, filterMin, filterMax);
}

BOOL PeekMessageW(LPMSG msg, HWND /* wnd */, UINT /* filterMin */, UINT /* filterMax */, UINT /* removeMsg */)
{
  memset(msg, 0, sizeof(*msg));
  if (IsGuiThread())
    QCoreApplication::processEvents();
  return FALSE;
}

BOOL PeekMessageA(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax, UINT removeMsg)
{
  return PeekMessageW(msg, wnd, filterMin, filterMax, removeMsg);
}

BOOL TranslateMessage(const MSG * /* msg */)
{
  return FALSE;
}

LRESULT DispatchMessageW(const MSG *msg)
{
  if (msg->message == WM_NULL || !msg->hwnd)
    return 0;
  return SendMessageW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

LRESULT DispatchMessageA(const MSG *msg)
{
  return DispatchMessageW(msg);
}

void PostQuitMessage(int exitCode)
{
  RunVoid([=]()
  {
    g_QuitPosted = true;
    g_QuitCode = exitCode;
    // wake up GetMessage()
    QMetaObject::invokeMethod(QCoreApplication::instance(), []() {}, Qt::QueuedConnection);
  });
}

UINT_PTR SetTimer(HWND wnd, UINT_PTR idEvent, UINT elapse, TIMERPROC timerFunc)
{
  return OnGuiThread<UINT_PTR>([=]() -> UINT_PTR
  {
    if (!FromHandle(wnd))
      return 0;   // timers without a window are not used
    const CTimerKey key = { wnd, idEvent };
    QTimer *&t = g_Timers[key];
    if (!t)
    {
      // a child of the application: KillTimer() deletes it later, and the
      // event loop may not run again before the program ends
      t = new QTimer(QCoreApplication::instance());
      const UInt64 serial = wnd->Serial;
      QObject::connect(t, &QTimer::timeout, t, [wnd, serial, idEvent, timerFunc]()
      {
        if (!FromHandle(wnd) || wnd->Serial != serial)
          return;
        if (timerFunc)
          timerFunc(wnd, WM_TIMER, idEvent, GetTickCount());
        else
          Send(wnd, WM_TIMER, idEvent, 0);
      });
    }
    t->start((int)(elapse < 10 ? 10 : elapse));
    return idEvent;
  });
}

BOOL KillTimer(HWND wnd, UINT_PTR idEvent)
{
  return OnGuiThread<BOOL>([=]()
  {
    const CTimerKey key = { wnd, idEvent };
    const auto it = g_Timers.find(key);
    if (it == g_Timers.end())
      return FALSE;
    it->second->stop();
    it->second->deleteLater();
    g_Timers.erase(it);
    return TRUE;
  });
}

// ---------- Windows ----------

BOOL IsWindow(HWND wnd)
{
  return OnGuiThread<BOOL>([=]() { return FromHandle(wnd) ? TRUE : FALSE; });
}

BOOL DestroyWindow(HWND wnd)
{
  return OnGuiThread<BOOL>([=]() { return DestroyWindowTree(wnd) ? TRUE : FALSE; });
}

BOOL IsWindowEnabled(HWND wnd)
{
  return OnGuiThread<BOOL>([=]() mutable
  {
    wnd = FromHandle(wnd);
    return (wnd && wnd->Widget && wnd->Widget->isEnabled()) ? TRUE : FALSE;
  });
}

BOOL IsWindowVisible(HWND wnd)
{
  return OnGuiThread<BOOL>([=]() mutable
  {
    wnd = FromHandle(wnd);
    return (wnd && wnd->Widget && wnd->Widget->isVisible()) ? TRUE : FALSE;
  });
}

BOOL EnableWindow(HWND wnd, BOOL enable)
{
  return OnGuiThread<BOOL>([=]() mutable
  {
    wnd = FromHandle(wnd);
    if (!wnd || !wnd->Widget)
      return FALSE;
    // the result tells whether the window was disabled before
    const BOOL wasDisabled = (wnd->Style & WS_DISABLED) ? TRUE : FALSE;
    if (wnd->AlwaysDisabled || wnd->AlwaysHidden)
      return wasDisabled;
    if (enable)
      wnd->Style &= ~(DWORD)WS_DISABLED;
    else
      wnd->Style |= WS_DISABLED;
    wnd->Widget->setEnabled(enable != FALSE);
    if (!enable && wnd->Widget->hasFocus() && !wnd->IsChild())
      wnd->Widget->clearFocus();
    return wasDisabled;
  });
}

BOOL ShowWindow(HWND wnd, int cmdShow)
{
  return OnGuiThread<BOOL>([=]() mutable
  {
    wnd = FromHandle(wnd);
    if (!wnd || !wnd->Widget)
      return FALSE;
    QWidget *w = wnd->Widget;
    const BOOL wasVisible = (wnd->Style & WS_VISIBLE) ? TRUE : FALSE;
    if (wnd->AlwaysHidden && cmdShow != SW_HIDE)
      return wasVisible;
    switch (cmdShow)
    {
      case SW_HIDE:
        wnd->Style &= ~(DWORD)WS_VISIBLE;
        w->hide();
        break;
      case SW_SHOWMAXIMIZED:
        wnd->Style |= WS_VISIBLE;
        if (wnd->IsChild())
          w->show();
        else
          w->showMaximized();
        break;
      case SW_MINIMIZE:
      case SW_SHOWMINIMIZED:
      case SW_SHOWMINNOACTIVE:
        wnd->Style |= WS_VISIBLE;
        if (wnd->IsChild())
          w->show();
        else
          w->showMinimized();
        break;
      case SW_RESTORE:
      case SW_SHOWNORMAL:
        wnd->Style |= WS_VISIBLE;
        if (wnd->IsChild())
          w->show();
        else
          w->showNormal();
        break;
      default:
        wnd->Style |= WS_VISIBLE;
        w->show();
        break;
    }
    return wasVisible;
  });
}

BOOL UpdateWindow(HWND wnd)
{
  return OnGuiThread<BOOL>([=]() mutable
  {
    wnd = FromHandle(wnd);
    if (!wnd || !wnd->Widget)
      return FALSE;
    wnd->Widget->update();
    return TRUE;
  });
}

BOOL InvalidateRect(HWND wnd, const RECT *rect, BOOL /* erase */)
{
  return OnGuiThread<BOOL>([=]() mutable
  {
    wnd = FromHandle(wnd);
    if (!wnd || !wnd->Widget)
      return FALSE;
    if (rect)
      wnd->Widget->update(QRect(QPoint(rect->left, rect->top), QPoint(rect->right - 1, rect->bottom - 1)));
    else
      wnd->Widget->update();
    return TRUE;
  });
}

// Qt delivers the resize event of a hidden widget when it is shown; Win32
// sends WM_SIZE at once, and dialog code that resizes itself in
// WM_INITDIALOG lays out its controls in WM_SIZE.
static void SendResizeOfHidden(QWidget *w, const QSize &oldSize)
{
  if (!w->isVisible() && w->size() != oldSize)
  {
    QResizeEvent e(w->size(), oldSize);
    QCoreApplication::sendEvent(w, &e);
  }
}

static void MoveWidget(HWND wnd, int x, int y, int width, int height, bool move, bool size)
{
  QWidget *w = wnd->Widget;
  const QSize oldSize = w->size();
  if (width < 0)
    width = 0;
  if (height < 0)
    height = 0;
  if (wnd->IsChild())
  {
    QRect r = w->geometry();
    if (move)
      r.moveTo(x, y);
    if (size)
      r.setSize(QSize(width, height));
    if (wnd->Kind == kKind_ComboBox)
      r.setHeight(ComboBoxClosedHeight(wnd));
    w->setGeometry(r);
    SendResizeOfHidden(w, oldSize);
    return;
  }
  // A top-level window cannot place itself on Wayland; the size still applies.
  if (size)
    w->resize(width, height);
  if (move && QGuiApplication::platformName() != QLatin1String("wayland"))
    w->move(x, y);
  SendResizeOfHidden(w, oldSize);
}

BOOL MoveWindow(HWND wnd, int x, int y, int width, int height, BOOL /* repaint */)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget)
      return FALSE;
    MoveWidget(w, x, y, width, height, true, true);
    return TRUE;
  });
}

BOOL SetWindowPos(HWND wnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget)
      return FALSE;
    if (!(flags & SWP_NOMOVE) || !(flags & SWP_NOSIZE))
      MoveWidget(w, x, y, cx, cy, !(flags & SWP_NOMOVE), !(flags & SWP_NOSIZE));
    if (!(flags & SWP_NOZORDER))
    {
      if (insertAfter == HWND_TOP || insertAfter == HWND_TOPMOST)
        w->Widget->raise();
      else if (insertAfter == HWND_BOTTOM)
        w->Widget->lower();
    }
    if (flags & SWP_SHOWWINDOW)
      ShowWindow(w, SW_SHOWNA);
    if (flags & SWP_HIDEWINDOW)
      ShowWindow(w, SW_HIDE);
    return TRUE;
  });
}

static QRect ScreenRect(QWidget *w)
{
  if (w->isWindow())
    return QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
  return QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
}

static void SetRect(RECT *r, const QRect &q)
{
  r->left = q.left();
  r->top = q.top();
  r->right = q.left() + q.width();
  r->bottom = q.top() + q.height();
}

BOOL GetWindowRect(HWND wnd, LPRECT rect)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget || !rect)
      return FALSE;
    SetRect(rect, ScreenRect(w->Widget));
    return TRUE;
  });
}

BOOL GetClientRect(HWND wnd, LPRECT rect)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget || !rect)
      return FALSE;
    SetRect(rect, QRect(QPoint(0, 0), w->Widget->size()));
    return TRUE;
  });
}

BOOL ClientToScreen(HWND wnd, LPPOINT point)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget || !point)
      return FALSE;
    const QPoint p = w->Widget->mapToGlobal(QPoint(point->x, point->y));
    point->x = p.x();
    point->y = p.y();
    return TRUE;
  });
}

BOOL ScreenToClient(HWND wnd, LPPOINT point)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget || !point)
      return FALSE;
    const QPoint p = w->Widget->mapFromGlobal(QPoint(point->x, point->y));
    point->x = p.x();
    point->y = p.y();
    return TRUE;
  });
}

int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count)
{
  return OnGuiThread<int>([=]()
  {
    QWidget *wf = from ? ParentWidgetFor(from) : NULL;
    QWidget *wt = to ? ParentWidgetFor(to) : NULL;
    const QPoint of = wf ? wf->mapToGlobal(QPoint(0, 0)) : QPoint();
    const QPoint ot = wt ? wt->mapToGlobal(QPoint(0, 0)) : QPoint();
    const QPoint d = of - ot;
    for (UINT i = 0; i < count; i++)
    {
      points[i].x += d.x();
      points[i].y += d.y();
    }
    return (int)MAKELONG(d.x(), d.y());
  });
}

BOOL IsZoomed(HWND wnd)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    return (w && w->Widget && w->Widget->isMaximized()) ? TRUE : FALSE;
  });
}

BOOL IsIconic(HWND wnd)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    return (w && w->Widget && w->Widget->isMinimized()) ? TRUE : FALSE;
  });
}

BOOL GetWindowPlacement(HWND wnd, WINDOWPLACEMENT *placement)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget)
      return FALSE;
    QWidget *q = w->Widget;
    placement->flags = 0;
    placement->showCmd = q->isMaximized() ? SW_SHOWMAXIMIZED :
        q->isMinimized() ? SW_SHOWMINIMIZED : SW_SHOWNORMAL;
    placement->ptMinPosition.x = placement->ptMinPosition.y = -1;
    placement->ptMaxPosition.x = placement->ptMaxPosition.y = -1;
    const QRect r = q->isWindow() ? q->normalGeometry() : q->geometry();
    SetRect(&placement->rcNormalPosition, r.isValid() ? r : q->geometry());
    return TRUE;
  });
}

BOOL SetWindowPlacement(HWND wnd, const WINDOWPLACEMENT *placement)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget)
      return FALSE;
    const RECT &r = placement->rcNormalPosition;
    MoveWidget(w, r.left, r.top, r.right - r.left, r.bottom - r.top, true, true);
    if (placement->showCmd == SW_SHOWMAXIMIZED && !w->IsChild())
      w->Widget->setWindowState(w->Widget->windowState() | Qt::WindowMaximized);
    return TRUE;
  });
}

HWND GetParent(HWND wnd)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    HWND w = FromHandle(wnd);
    return w ? w->Parent : NULL;
  });
}

HWND GetAncestor(HWND wnd, UINT flags)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    HWND w = FromHandle(wnd);
    if (!w)
      return NULL;
    switch (flags)
    {
      case GA_PARENT: return w->IsChild() ? w->Parent : NULL;
      case GA_ROOT: return w->TopLevel();
      default:
      {
        HWND r = w->TopLevel();
        while (r->Parent && FromHandle(r->Parent))
          r = r->Parent->TopLevel();
        return r;
      }
    }
  });
}

HWND GetDesktopWindow()
{
  return NULL;
}

HMENU GetSystemMenu(HWND /* wnd */, BOOL /* revert */)
{
  return NULL;
}

BOOL SetForegroundWindow(HWND wnd)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget)
      return FALSE;
    QWidget *top = w->Widget->window();
    top->raise();
    top->activateWindow();
    return TRUE;
  });
}

HWND GetForegroundWindow()
{
  return OnGuiThread<HWND>([]() -> HWND
  {
    return FromWidget(QApplication::activeWindow());
  });
}

HWND GetActiveWindow()
{
  return GetForegroundWindow();
}

HWND SetFocus(HWND wnd)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    const HWND prev = FromWidget(QApplication::focusWidget());
    HWND w = FromHandle(wnd);
    if (w && w->Widget)
    {
      QWidget *q = w->Widget;
      if (q->focusProxy())
        q = q->focusProxy();
      q->setFocus(Qt::OtherFocusReason);
    }
    return prev;
  });
}

HWND GetFocus()
{
  return OnGuiThread<HWND>([]() -> HWND
  {
    return FromWidget(QApplication::focusWidget());
  });
}

SHORT GetKeyState(int virtKey)
{
  const Qt::KeyboardModifiers m = QGuiApplication::queryKeyboardModifiers();
  bool down = false;
  switch (virtKey)
  {
    case VK_SHIFT: down = (m & Qt::ShiftModifier) != 0; break;
    case VK_CONTROL: down = (m & Qt::ControlModifier) != 0; break;
    case VK_MENU: down = (m & Qt::AltModifier) != 0; break;
    case VK_LWIN:
    case VK_RWIN: down = (m & Qt::MetaModifier) != 0; break;
    default: break;
  }
  return down ? (SHORT)0x8000 : 0;
}

SHORT GetAsyncKeyState(int virtKey)
{
  return GetKeyState(virtKey);
}

BOOL GetCursorPos(LPPOINT point)
{
  const QPoint p = QCursor::pos();
  point->x = p.x();
  point->y = p.y();
  return TRUE;
}

BOOL MessageBeep(UINT /* type */)
{
  QApplication::beep();
  return TRUE;
}

// ---------- Text ----------

BOOL SetWindowTextW(HWND wnd, LPCWSTR s)
{
  return (BOOL)SendMessageW(wnd, WM_SETTEXT, 0, (LPARAM)s);
}

BOOL SetWindowTextA(HWND wnd, LPCSTR s)
{
  return (BOOL)SendMessageA(wnd, WM_SETTEXT, 0, (LPARAM)s);
}

int GetWindowTextW(HWND wnd, LPWSTR s, int maxCount)
{
  SetLastError(0);
  return (int)SendMessageW(wnd, WM_GETTEXT, (WPARAM)maxCount, (LPARAM)s);
}

int GetWindowTextA(HWND wnd, LPSTR s, int maxCount)
{
  SetLastError(0);
  return (int)SendMessageA(wnd, WM_GETTEXT, (WPARAM)maxCount, (LPARAM)s);
}

int GetWindowTextLengthW(HWND wnd)
{
  SetLastError(0);
  return (int)SendMessageW(wnd, WM_GETTEXTLENGTH, 0, 0);
}

int GetWindowTextLengthA(HWND wnd)
{
  SetLastError(0);
  return (int)SendMessageA(wnd, WM_GETTEXTLENGTH, 0, 0);
}

// ---------- Window data ----------

namespace NShim {
LONG_PTR GetDialogLong(HWND wnd, int index, bool &handled);
LONG_PTR SetDialogLong(HWND wnd, int index, LONG_PTR value, bool &handled);
}

static LONG_PTR GetLongImpl(HWND wnd, int index)
{
  wnd = FromHandle(wnd);
  if (!wnd)
    return 0;
  switch (index)
  {
    case GWLP_WNDPROC: return wnd->WndProc ? (LONG_PTR)wnd->WndProc : (LONG_PTR)BuiltinWndProc;
    case GWLP_HINSTANCE: return 0;
    case GWLP_HWNDPARENT: return (LONG_PTR)wnd->Parent;
    case GWLP_ID: return (LONG_PTR)wnd->Id;
    case GWL_STYLE:
    {
      DWORD style = wnd->Style;
      if (wnd->Widget)
      {
        if (wnd->Widget->isVisible())
          style |= WS_VISIBLE;
        else if (!wnd->Widget->isWindow() && wnd->Widget->isHidden())
          style &= ~(DWORD)WS_VISIBLE;
        if (wnd->Widget->isWindow() && wnd->Widget->isMaximized())
          style |= WS_MAXIMIZE;
      }
      return (LONG_PTR)style;
    }
    case GWL_EXSTYLE: return (LONG_PTR)wnd->ExStyle;
    case GWLP_USERDATA: return wnd->UserData;
    default:
    {
      bool handled = false;
      const LONG_PTR r = GetDialogLong(wnd, index, handled);
      return handled ? r : 0;
    }
  }
}

static LONG_PTR SetLongImpl(HWND wnd, int index, LONG_PTR value)
{
  wnd = FromHandle(wnd);
  if (!wnd)
    return 0;
  const LONG_PTR prev = GetLongImpl(wnd, index);
  switch (index)
  {
    case GWLP_WNDPROC:
      wnd->WndProc = ((WNDPROC)value == BuiltinWndProc) ? NULL : (WNDPROC)value;
      break;
    case GWLP_ID: wnd->Id = (UINT)value; break;
    case GWL_STYLE: wnd->Style = (DWORD)value; break;
    case GWL_EXSTYLE: wnd->ExStyle = (DWORD)value; break;
    case GWLP_USERDATA: wnd->UserData = value; break;
    default:
    {
      bool handled = false;
      SetDialogLong(wnd, index, value, handled);
      break;
    }
  }
  return prev;
}

LONG_PTR GetWindowLongPtrW(HWND wnd, int index)
{
  return OnGuiThread<LONG_PTR>([=]() { return GetLongImpl(wnd, index); });
}

LONG_PTR GetWindowLongPtrA(HWND wnd, int index)
{
  return GetWindowLongPtrW(wnd, index);
}

LONG_PTR SetWindowLongPtrW(HWND wnd, int index, LONG_PTR value)
{
  return OnGuiThread<LONG_PTR>([=]() { return SetLongImpl(wnd, index, value); });
}

LONG_PTR SetWindowLongPtrA(HWND wnd, int index, LONG_PTR value)
{
  return SetWindowLongPtrW(wnd, index, value);
}

LONG GetWindowLongW(HWND wnd, int index)
{
  return (LONG)GetWindowLongPtrW(wnd, index);
}

LONG GetWindowLongA(HWND wnd, int index)
{
  return (LONG)GetWindowLongPtrW(wnd, index);
}

LONG SetWindowLongW(HWND wnd, int index, LONG value)
{
  return (LONG)SetWindowLongPtrW(wnd, index, value);
}

LONG SetWindowLongA(HWND wnd, int index, LONG value)
{
  return (LONG)SetWindowLongPtrW(wnd, index, value);
}

// ---------- Screen ----------

static QRect WorkArea(HWND wnd)
{
  QScreen *screen = NULL;
  wnd = FromHandle(wnd);
  if (wnd && wnd->Widget)
    screen = wnd->Widget->screen();
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  return screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
}

BOOL SystemParametersInfoW(UINT action, UINT /* param */, PVOID data, UINT /* winIni */)
{
  if (action == SPI_GETWORKAREA && data)
  {
    const QRect r = OnGuiThread<QRect>([]() { return WorkArea(NULL); });
    SetRect((RECT *)data, r);
    return TRUE;
  }
  return FALSE;
}

BOOL SystemParametersInfoA(UINT action, UINT param, PVOID data, UINT winIni)
{
  return SystemParametersInfoW(action, param, data, winIni);
}

int GetSystemMetrics(int index)
{
  return OnGuiThread<int>([=]()
  {
    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect g = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);
    switch (index)
    {
      case SM_CXSCREEN: return g.width();
      case SM_CYSCREEN: return g.height();
      case SM_CXICON:
      case SM_CYICON: return 32;
      case SM_CXSMICON:
      case SM_CYSMICON: return 16;
      case SM_CXVSCROLL:
      case SM_CYHSCROLL: return 16;
      case SM_CYCAPTION: return 24;
      default: return 0;
    }
  });
}

// A monitor handle is the window it was asked for, or a marker for the
// primary screen; GetMonitorInfo() resolves it to that window's screen.
HMONITOR MonitorFromWindow(HWND wnd, DWORD /* flags */)
{
  return wnd ? (HMONITOR)(void *)wnd : (HMONITOR)(uintptr_t)1;
}

BOOL GetMonitorInfoW(HMONITOR monitor, LPMONITORINFO info)
{
  const HWND wnd = ((uintptr_t)monitor == 1) ? NULL : (HWND)(void *)monitor;
  OnGuiThread<int>([=]()
  {
    QScreen *screen = NULL;
    HWND w = FromHandle(wnd);
    if (w && w->Widget)
      screen = w->Widget->screen();
    if (!screen)
      screen = QGuiApplication::primaryScreen();
    const QRect g = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);
    const QRect a = screen ? screen->availableGeometry() : g;
    SetRect(&info->rcMonitor, g);
    SetRect(&info->rcWork, a);
    info->dwFlags = (screen == QGuiApplication::primaryScreen()) ? 1 : 0;
    return 0;
  });
  return TRUE;
}

BOOL GetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info)
{
  return GetMonitorInfoW(monitor, info);
}

DWORD GetSysColor(int index)
{
  return OnGuiThread<DWORD>([=]() -> DWORD
  {
    const QPalette p = QApplication::palette();
    QColor c;
    switch (index)
    {
      case COLOR_WINDOW: c = p.color(QPalette::Base); break;
      case COLOR_WINDOWTEXT: c = p.color(QPalette::Text); break;
      case COLOR_HIGHLIGHT: c = p.color(QPalette::Highlight); break;
      case COLOR_HIGHLIGHTTEXT: c = p.color(QPalette::HighlightedText); break;
      case COLOR_BTNFACE: c = p.color(QPalette::Button); break;
      case COLOR_BTNTEXT: c = p.color(QPalette::ButtonText); break;
      case COLOR_GRAYTEXT: c = p.color(QPalette::Disabled, QPalette::Text); break;
      default: c = p.color(QPalette::Window); break;
    }
    return RGB(c.red(), c.green(), c.blue());
  });
}
