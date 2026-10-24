// ShimInput.cpp -- keyboard, mouse and focus input as window messages,
// mouse capture, and the accelerator tables of TranslateAccelerator().
//
// On Windows, input reaches a window as messages that its window procedure
// may handle or pass on to the default processing. Here the Qt event of a
// window's widget is turned into those messages first: if a message gets to
// DefWindowProc() (or to a built-in procedure that leaves it to the
// control), the widget gets the Qt event and does what the control does on
// Windows; otherwise the event stops. This is how a subclassed control (the
// list of a panel, the edit field of the address combo box) can take keys
// away from Qt.
//
// Only windows whose input the program can see get this
// (HWND__::WantsInputMessages()); dialog controls stay pure Qt.

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QWheelEvent>
#include <QWidget>

#include <map>

#include "ShimWindow.h"

#include "../platform/RcTables.h"

namespace NShim {

// ---------- Accelerators ----------

// An accelerator table applies to the keys of a top-level window and its
// children once the message loop has called TranslateAccelerator() for it.
static std::map<HWND, HACCEL> g_Accelerators;

}

struct HACCEL__
{
  const NRc::CAccelTable *Table;
};

namespace NShim {

static bool TranslateAcceleratorKey(HWND wnd, UINT vk, Qt::KeyboardModifiers mods)
{
  HWND top = wnd->TopLevel();
  const auto it = g_Accelerators.find(top);
  if (it == g_Accelerators.end() || !it->second || !it->second->Table)
    return false;
  if (!top->Widget || !top->Widget->isEnabled())
    return false;
  const NRc::CAccelTable *t = it->second->Table;
  const bool shift = (mods & Qt::ShiftModifier) != 0;
  const bool ctrl = (mods & Qt::ControlModifier) != 0;
  const bool alt = (mods & Qt::AltModifier) != 0;
  for (unsigned i = 0; i < t->NumItems; i++)
  {
    const NRc::CAccel &a = t->Items[i];
    // character accelerators (no FVIRTKEY) would need WM_CHAR; 7-Zip has none
    if (!(a.Flags & FVIRTKEY) || a.Key != vk)
      continue;
    if (((a.Flags & FSHIFT) != 0) != shift
        || ((a.Flags & FCONTROL) != 0) != ctrl
        || ((a.Flags & FALT) != 0) != alt)
      continue;
    Send(top, WM_COMMAND, MAKEWPARAM(a.Cmd, 1), 0);
    return true;
  }
  return false;
}

static void ForgetAccelerators(HACCEL accel)
{
  for (auto it = g_Accelerators.begin(); it != g_Accelerators.end();)
  {
    if (it->second == accel)
      it = g_Accelerators.erase(it);
    else
      ++it;
  }
}

// ---------- Keyboard ----------

static QKeyEvent *g_NativeKeyEvent = NULL;

void SendNativeKeyEvent(QWidget *widget, QKeyEvent *event)
{
  QKeyEvent *prev = g_NativeKeyEvent;
  g_NativeKeyEvent = event;
  QCoreApplication::sendEvent(widget, event);
  g_NativeKeyEvent = prev;
}

static bool IsExtendedKey(int key, Qt::KeyboardModifiers mods)
{
  switch (key)
  {
    case Qt::Key_Insert: case Qt::Key_Delete:
    case Qt::Key_Home: case Qt::Key_End:
    case Qt::Key_PageUp: case Qt::Key_PageDown:
    case Qt::Key_Left: case Qt::Key_Right: case Qt::Key_Up: case Qt::Key_Down:
    case Qt::Key_Menu:
      return (mods & Qt::KeypadModifier) == 0;
    case Qt::Key_Enter:
    case Qt::Key_Slash:
      return (mods & Qt::KeypadModifier) != 0;
    default:
      return false;
  }
}

/* lParam of the key messages: repeat count 1; the "scan code" is the
   virtual key (MapVirtualKey() maps it back unchanged); extended key,
   context (Alt) and previous state bits as on Windows. */
static LPARAM KeyLParam(const QKeyEvent *e, UINT vk, bool up)
{
  UInt32 v = 1;
  v |= (UInt32)(vk & 0xFF) << 16;
  if (IsExtendedKey(e->key(), e->modifiers()))
    v |= (UInt32)1 << 24;
  if (e->modifiers() & Qt::AltModifier)
    v |= (UInt32)1 << 29;
  if (up || e->isAutoRepeat())
    v |= (UInt32)1 << 30;
  if (up)
    v |= (UInt32)1 << 31;
  return (LPARAM)v;
}

// The character of WM_CHAR, or 0: Windows sends one for printable keys and
// for Enter, Tab, Esc and Backspace.
static UInt32 KeyChar(const QKeyEvent *e)
{
  switch (e->key())
  {
    case Qt::Key_Return:
    case Qt::Key_Enter: return '\r';
    case Qt::Key_Tab:
    case Qt::Key_Backtab: return '\t';
    case Qt::Key_Escape: return 0x1B;
    case Qt::Key_Backspace: return '\b';
    default: break;
  }
  const QString text = e->text();
  if (text.isEmpty())
    return 0;
  const char32_t c = text.toUcs4().value(0);
  if (c < 0x20 && !(e->modifiers() & Qt::ControlModifier))
    return 0;
  return (UInt32)c;
}

// Sends a message and tells whether it got to the default processing.
static bool SendInputMessage(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam, bool &alive)
{
  wnd->InputDefault = false;
  Send(wnd, message, wParam, lParam);
  alive = (FromHandle(wnd) != NULL);
  return alive && wnd->InputDefault;
}

static bool DispatchKey(HWND wnd, QObject *watched, QKeyEvent *e)
{
  const bool up = (e->type() == QEvent::KeyRelease);
  const UINT vk = VirtualKeyFromQt(e);
  const bool alt = (e->modifiers() & Qt::AltModifier) != 0;
  if (!up && vk != 0 && TranslateAcceleratorKey(wnd, vk, e->modifiers()))
    return true;
  if (vk == 0 && e->text().isEmpty())
    return false;
  bool alive = true;
  if (up)
  {
    if (vk != 0)
      SendInputMessage(wnd, alt ? WM_SYSKEYUP : WM_KEYUP, vk, KeyLParam(e, vk, true), alive);
    return false;
  }

  bool keyDefault = true;
  if (vk != 0)
    keyDefault = SendInputMessage(wnd, alt ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, KeyLParam(e, vk, false), alive);
  if (!alive)
    return true;
  // Windows sends WM_CHAR whether or not WM_KEYDOWN was handled
  // (TranslateMessage() runs before the dispatch).
  const UInt32 c = KeyChar(e);
  bool charDefault = true;
  if (c != 0)
  {
    charDefault = SendInputMessage(wnd, alt ? WM_SYSCHAR : WM_CHAR, c, KeyLParam(e, vk, false), alive);
    if (!alive)
      return true;
  }
  if (keyDefault && charDefault)
    return false;
  QWidget *widget = qobject_cast<QWidget *>(watched);
  if (!widget || (!keyDefault && (c == 0 || !charDefault)))
    return true;
  // one of the two messages was handled: the widget gets the other part
  if (keyDefault)
  {
    QKeyEvent copy(e->type(), e->key(), e->modifiers(), QString(), e->isAutoRepeat(), (ushort)e->count());
    SendNativeKeyEvent(widget, &copy);
  }
  else
  {
    QKeyEvent copy(e->type(), Qt::Key_unknown, e->modifiers() & ~Qt::KeyboardModifiers(Qt::ControlModifier | Qt::AltModifier),
        e->text(), e->isAutoRepeat(), (ushort)e->count());
    SendNativeKeyEvent(widget, &copy);
  }
  return true;
}

// ---------- Mouse ----------

static HWND g_Capture = NULL;

LPARAM MousePosLParam(HWND wnd, const QPoint &globalPos)
{
  const QPoint p = wnd->ClientArea()->mapFromGlobal(globalPos);
  return MAKELPARAM((WORD)(short)p.x(), (WORD)(short)p.y());
}

WPARAM MouseKeyFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
  WPARAM w = 0;
  if (buttons & Qt::LeftButton)
    w |= MK_LBUTTON;
  if (buttons & Qt::RightButton)
    w |= MK_RBUTTON;
  if (buttons & Qt::MiddleButton)
    w |= MK_MBUTTON;
  if (modifiers & Qt::ShiftModifier)
    w |= MK_SHIFT;
  if (modifiers & Qt::ControlModifier)
    w |= MK_CONTROL;
  return w;
}

static UINT MouseMessage(QEvent::Type type, Qt::MouseButton button)
{
  switch (type)
  {
    case QEvent::MouseMove:
      return WM_MOUSEMOVE;
    case QEvent::MouseButtonPress:
      return button == Qt::LeftButton ? WM_LBUTTONDOWN :
          button == Qt::RightButton ? WM_RBUTTONDOWN :
          button == Qt::MiddleButton ? WM_MBUTTONDOWN : 0;
    case QEvent::MouseButtonRelease:
      return button == Qt::LeftButton ? WM_LBUTTONUP :
          button == Qt::RightButton ? WM_RBUTTONUP :
          button == Qt::MiddleButton ? WM_MBUTTONUP : 0;
    case QEvent::MouseButtonDblClick:
      return button == Qt::LeftButton ? WM_LBUTTONDBLCLK :
          button == Qt::RightButton ? WM_RBUTTONDBLCLK : 0;
    default:
      return 0;
  }
}

static bool DispatchMouse(HWND wnd, QMouseEvent *e)
{
  const UINT message = MouseMessage(e->type(), e->button());
  if (message == 0)
    return false;
  bool alive = true;
  const QPoint g = e->globalPosition().toPoint();
  const bool def = SendInputMessage(wnd, message, MouseKeyFlags(e->buttons(), e->modifiers()),
      MousePosLParam(wnd, g), alive);
  return !def;
}

static bool DispatchWheel(HWND wnd, QWheelEvent *e)
{
  bool alive = true;
  const int delta = e->angleDelta().y();
  if (delta == 0)
    return false;
  const QPoint g = e->globalPosition().toPoint();
  // WM_MOUSEWHEEL has screen coordinates
  const bool def = SendInputMessage(wnd, WM_MOUSEWHEEL,
      MAKEWPARAM(MouseKeyFlags(e->buttons(), e->modifiers()), (WORD)(short)delta),
      MAKELPARAM((WORD)(short)g.x(), (WORD)(short)g.y()), alive);
  return !def;
}

// ---------- Dispatch ----------

/* Qt passes a key or mouse event that a widget ignores on to its parent
   widgets, as the same event object. On Windows only the window under the
   mouse or with the focus gets the message, so an event that already went
   to a widget below this one is not dispatched again. */
static const QEvent *g_LastInput = NULL;
static QEvent::Type g_LastInputType = QEvent::None;
static QPointer<QObject> g_LastInputReceiver;

static bool IsAncestor(const QObject *a, const QObject *b)
{
  for (const QObject *o = b ? b->parent() : NULL; o; o = o->parent())
    if (o == a)
      return true;
  return false;
}

static bool IsPropagated(QObject *watched, QEvent *event)
{
  if (event == g_LastInput && event->type() == g_LastInputType && g_LastInputReceiver
      && IsAncestor(watched, g_LastInputReceiver))
    return true;
  g_LastInput = event;
  g_LastInputType = event->type();
  g_LastInputReceiver = watched;
  return false;
}

bool DispatchInputEvent(HWND wnd, QObject *watched, QEvent *event)
{
  switch (event->type())
  {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
      if (event == g_NativeKeyEvent || IsPropagated(watched, event))
        return false;
      return DispatchKey(wnd, watched, static_cast<QKeyEvent *>(event));
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    {
      // a window of a registered class gets the mouse in its client area
      // (the frame has the menu bar); a control in its widgets, but an
      // event that Qt passes on to the parent widget only once
      if (wnd->Kind == kKind_Generic && watched != wnd->ClientArea())
        return false;
      if (IsPropagated(watched, event))
        return false;
      if (event->type() == QEvent::Wheel)
        return DispatchWheel(wnd, static_cast<QWheelEvent *>(event));
      return DispatchMouse(wnd, static_cast<QMouseEvent *>(event));
    }
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    {
      // the widget that gets or loses the focus (a control may consist of
      // several widgets: the focus is in one of them)
      QWidget *w = qobject_cast<QWidget *>(watched);
      if (!w || FromWidget(w) != wnd || (w->focusProxy() && w != wnd->Widget.data()))
        return false;
      if (event->type() == QEvent::FocusIn)
        Send(wnd, WM_SETFOCUS, 0, 0);
      else
      {
        HWND next = FromWidget(QApplication::focusWidget());
        Send(wnd, WM_KILLFOCUS, (WPARAM)(next == wnd ? NULL : next), 0);
      }
      return false;
    }
    default:
      return false;
  }
}

}

using namespace NShim;

HACCEL LoadAcceleratorsW(HINSTANCE /* instance */, LPCWSTR name)
{
  if (!IS_INTRESOURCE(name))
    return NULL;
  const NRc::CAccelTable *t = NRc::FindAccelTable((UInt32)(uintptr_t)name);
  if (!t)
    return NULL;
  HACCEL h = new HACCEL__;
  h->Table = t;
  return h;
}

HACCEL LoadAcceleratorsA(HINSTANCE instance, LPCSTR name)
{
  return LoadAcceleratorsW(instance, (LPCWSTR)(const void *)name);
}

/* GetMessage() gives no keyboard messages (see ShimCore.cpp), so this call
   of the message loop only tells which table belongs to which window: from
   then on DispatchInputEvent() translates the keys of that window. */
int TranslateAcceleratorW(HWND wnd, HACCEL accel, LPMSG /* msg */)
{
  return OnGuiThread<int>([=]()
  {
    HWND w = FromHandle(wnd);
    if (w && accel)
      g_Accelerators[w->TopLevel()] = accel;
    return 0;
  });
}

int TranslateAcceleratorA(HWND wnd, HACCEL accel, LPMSG msg)
{
  return TranslateAcceleratorW(wnd, accel, msg);
}

BOOL DestroyAcceleratorTable(HACCEL accel)
{
  if (!accel)
    return FALSE;
  RunOnGuiThread([=]() { ForgetAccelerators(accel); });
  delete accel;
  return TRUE;
}

UINT MapVirtualKeyW(UINT code, UINT mapType)
{
  // the scan codes of the key messages are the virtual keys
  switch (mapType)
  {
    case MAPVK_VK_TO_VSC:
    case MAPVK_VSC_TO_VK:
      return code & 0xFF;
    case MAPVK_VK_TO_CHAR:
      if ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z'))
        return code;
      return 0;
    default:
      return 0;
  }
}

UINT MapVirtualKeyA(UINT code, UINT mapType)
{
  return MapVirtualKeyW(code, mapType);
}

HWND SetCapture(HWND wnd)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    const HWND prev = FromHandle(g_Capture);
    // Qt keeps sending the mouse events to the pressed widget, which is
    // what the capture is used for
    g_Capture = FromHandle(wnd);
    return prev;
  });
}

BOOL ReleaseCapture()
{
  return OnGuiThread<BOOL>([]()
  {
    HWND prev = FromHandle(g_Capture);
    g_Capture = NULL;
    if (prev)
      Send(prev, WM_CAPTURECHANGED, 0, 0);
    return TRUE;
  });
}

HWND GetCapture()
{
  return OnGuiThread<HWND>([]() -> HWND
  {
    return FromHandle(g_Capture);
  });
}

UINT GetDoubleClickTime()
{
  return (UINT)QApplication::doubleClickInterval();
}

BOOL SetCursorPos(int /* x */, int /* y */)
{
  // Wayland does not let programs move the pointer
  return FALSE;
}

BOOL DragDetect(HWND /* wnd */, POINT /* pt */)
{
  return FALSE;
}
