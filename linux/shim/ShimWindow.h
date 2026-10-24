// ShimWindow.h -- the window objects behind HWND (internal to linux/shim).
//
// Each HWND points to an HWND__ object that owns or wraps a QWidget and keeps
// the Win32 state that upstream code reads and writes (control ID, styles,
// user data, window procedure, timers). Messages go to the window procedure
// set with GWLP_WNDPROC or, by default, to the built-in procedure of the
// window class (HWND__::Proc). All of this lives on the GUI thread: the API
// functions move calls from other threads there first (see OnGuiThread).

#ifndef ZIP7_INC_LINUX_SHIM_WINDOW_H
#define ZIP7_INC_LINUX_SHIM_WINDOW_H

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

#include <windows.h>
#include <CommCtrl.h>

#include "../../CPP/Common/MyString.h"

class QEvent;
class QKeyEvent;

namespace NShim {

enum EKind
{
  kKind_Generic,   // a registered class (CreateWindowEx) or a wrapped Qt widget
  kKind_Dialog,
  kKind_Button,
  kKind_Static,
  kKind_Edit,
  kKind_ComboBox,
  kKind_ListView,
  kKind_Header,
  kKind_Progress,
  kKind_UpDown,
  kKind_Trackbar,
  kKind_ComboBoxEx,
  kKind_Toolbar,
  kKind_ReBar,
  kKind_StatusBar
};

class CEventProxy;

}

struct HWND__
{
  Z7_CLASS_NO_COPY(HWND__)
public:
  const NShim::EKind Kind;
  const UInt64 Serial;         // unique for the process; detects reused addresses
  QPointer<QWidget> Widget;
  // The client area of a top-level window with a frame (menu bar), else
  // NULL: children, client coordinates and WM_SIZE belong to it.
  QPointer<QWidget> Client;
  HWND Parent;                 // parent of a child window, owner of a top-level one
  std::vector<HWND> Children;  // in creation order, which is the Z order and tab order
  UINT Id;                     // GWL_ID
  DWORD Style;                 // GWL_STYLE
  DWORD ExStyle;               // GWL_EXSTYLE
  LONG_PTR UserData;           // GWLP_USERDATA
  WNDPROC WndProc;             // GWLP_WNDPROC; NULL selects the built-in procedure
  bool Destroying;
  bool Dead;                   // destroyed; deleted when Locks drops to 0
  // controls of Windows-only options (see ShimDialog.cpp)
  bool AlwaysDisabled;         // EnableWindow() cannot enable it
  bool AlwaysHidden;           // ShowWindow() cannot show it (and it is disabled)
  int Locks;                   // calls into the object that are in progress
  NShim::CEventProxy *Proxy;   // receives the widget's signals and events
  QString TextValue;           // the window text of a child without its own text
  HMENU Menu;                  // the menu bar of a top-level window (SetMenu)
  // Set when an input message (WM_KEYDOWN, WM_CHAR, mouse messages) reaches
  // the default processing (DefWindowProc): Qt then handles the event.
  bool InputDefault;

  HWND__(NShim::EKind kind);
  virtual ~HWND__();

  bool IsChild() const { return (Style & WS_CHILD) != 0; }
  HWND TopLevel();
  QWidget *ClientArea() const { return Client ? Client.data() : Widget.data(); }

  // Whether Qt input events of the widget become window messages (see
  // ShimInput.cpp): for windows with a window procedure of the program
  // (registered classes, subclassed controls) and for classes whose
  // built-in procedure handles input messages.
  virtual bool WantsInputMessages() const { return WndProc != NULL; }

  // Called after SetWindowLong(GWL_STYLE) changed the style.
  virtual void OnStyleChanged(DWORD /* oldStyle */) {}

  // The built-in window procedure of the class. Text parameters are always
  // wide: SendMessageA converts the narrow messages before they get here.
  virtual LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);

  // Called through Proxy for events of Widget (and of its internal children
  // for which the class installed the proxy). Return true to stop the event.
  virtual bool OnEvent(QObject *watched, QEvent *event);

  // Window text (WM_SETTEXT, WM_GETTEXT) in Win32 form.
  virtual void SetText(const QString &s);
  virtual QString Text() const;

  // Called after all controls of a dialog template exist.
  virtual void OnDialogCreated() {}

  // Sends WM_COMMAND with a notification code to the parent.
  void NotifyCommand(UINT code);
  // Sends WM_NOTIFY to the parent; hdr must start the notification structure.
  LRESULT NotifyParent(NMHDR *hdr, UINT code);
};

namespace NShim {

// The proxy is a child of the widget, so it outlives the HWND__ object: a
// signal or event that arrives after DestroyWindow() finds Wnd() == NULL.
class CEventProxy: public QObject
{
  HWND _wnd;
public:
  CEventProxy(HWND wnd, QObject *parent): QObject(parent), _wnd(wnd) {}
  HWND Wnd() const { return _wnd; }
  void Detach() { _wnd = NULL; }
  bool eventFilter(QObject *watched, QEvent *event) override;
};

// Keeps a window object alive while code runs in it, because a message
// handler may destroy the window it was called for.
class CWindowLock
{
  HWND _wnd;
  Z7_CLASS_NO_COPY(CWindowLock)
public:
  explicit CWindowLock(HWND wnd);
  explicit CWindowLock(const CEventProxy *proxy): CWindowLock(proxy->Wnd()) {}
  ~CWindowLock();
  HWND Get() const { return _wnd; }
  operator bool() const { return _wnd != NULL; }
};

// Connects a widget signal to a member of the window class T. The call is
// dropped once the window is destroyed.
template <class T, class Sender, class Signal, class F>
void Connect(T *wnd, Sender *sender, Signal signal, F f)
{
  CEventProxy *proxy = wnd->Proxy;
  QObject::connect(sender, signal, proxy, [proxy, f](auto... args)
  {
    CWindowLock lock(proxy);
    if (lock)
      f(static_cast<T *>(lock.Get()), args...);
  });
}

// ---------- Threads ----------

bool IsGuiThread();
// Runs f on the GUI thread and waits for it. Blocking is safe because the GUI
// thread never waits for a worker thread while it runs the shim.
void RunOnGuiThread(const std::function<void()> &f);

template <class T, class F>
T OnGuiThread(F f)
{
  if (IsGuiThread())
    return f();
  T result = T();
  RunOnGuiThread([&]() { result = f(); });
  return result;
}

// ---------- Handles ----------

// Sets Widget, creates the proxy and registers the handle.
void RegisterWindow(HWND wnd, QWidget *widget);
HWND FromHandle(HWND wnd);        // NULL if wnd is not a live window
HWND FromWidget(const QObject *widget);  // the window of a widget or of its nearest ancestor
QWidget *ParentWidgetFor(HWND parent);

// Destroys a window and its children (WM_DESTROY, WM_NCDESTROY).
bool DestroyWindowTree(HWND wnd);

// ---------- Messages ----------

// Sends a wide message to a live window on the GUI thread.
LRESULT Send(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
void Post(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT DefWindowProcImpl(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);
void KillAllTimers(HWND wnd);

// ---------- Text ----------

QString FromW(LPCWSTR s);
QString FromW(LPCWSTR s, size_t len);
QString FromA(LPCSTR s);
QString FromU(const UString &s);
UString ToU(const QString &s);
// Copies s with a terminating zero into buf of maxCount characters (wchar_t
// or UTF-8 bytes) and returns the number of characters copied without it.
int CopyToW(const QString &s, LPWSTR buf, int maxCount);
int CopyToA(const QString &s, LPSTR buf, int maxCount);
int LengthA(const QString &s);   // UTF-8 bytes
int LengthW(const QString &s);   // wchar_t units (UTF-32)

// Win32 mnemonic text ("&Name", "&&") is Qt's syntax too; this removes the
// markers for places that show text without mnemonics.
QString StripMnemonic(const QString &s);

// ---------- Controls ----------

// Creates a window of a built-in control class or of a registered class.
// Returns NULL for an unknown class.
HWND CreateWindowImpl(DWORD exStyle, const QString &className, const QString &text,
    DWORD style, int x, int y, int width, int height, HWND parent, UINT id, LPVOID param,
    bool fromDialogTemplate);

HWND CreateButton(DWORD style, QWidget *parent);
HWND CreateStatic(DWORD style, QWidget *parent);
HWND CreateEdit(DWORD style, DWORD exStyle, QWidget *parent);
HWND CreateComboBox(DWORD style, QWidget *parent);
HWND CreateListView(DWORD style, DWORD exStyle, QWidget *parent);
HWND CreateProgress(DWORD style, QWidget *parent);
HWND CreateUpDown(DWORD style, QWidget *parent);
HWND CreateTrackbar(DWORD style, QWidget *parent);
HWND CreateComboBoxEx(DWORD style, QWidget *parent);
// ShimBars.cpp
HWND CreateToolbar(DWORD style, QWidget *parent);
HWND CreateReBar(DWORD style, QWidget *parent);
HWND CreateStatusBar(DWORD style, QWidget *parent);

// The height that a bar keeps whatever height it is given (rebars, status
// bars and toolbars without CCS_NORESIZE size themselves), or 0.
int BarOwnHeight(HWND wnd);

// Moves a window to another parent (SetParent()).
void Reparent(HWND wnd, HWND newParent);

// The window class of a control ("Button", "SysListView32", ...).
const char *ClassName(HWND wnd);

// Pixel size of a closed combo box (the template height includes the list);
// also for ComboBoxEx.
int ComboBoxClosedHeight(HWND wnd);

// For the dialog code: radio buttons and push buttons.
bool IsRadioButton(HWND wnd);
bool IsAutoRadioButton(HWND wnd);
bool IsPushButton(HWND wnd);
void SetDefaultButtonLook(HWND wnd, bool isDefault);
void ClickButton(HWND wnd);

// ---------- Input ----------

// The Win32 virtual key of a Qt key event, or 0.
UINT VirtualKeyFromQt(const QKeyEvent *e);

// Turns a Qt input or focus event of a window's widget into window
// messages (WM_KEYDOWN, WM_CHAR, WM_LBUTTONDOWN, WM_SETFOCUS, ...) and the
// accelerators of the top-level window into WM_COMMAND. Returns true if
// the event must not reach the widget: the window procedure handled it
// instead of passing it to the default processing.
bool DispatchInputEvent(HWND wnd, QObject *watched, QEvent *event);

// Sends a key event to a widget past DispatchInputEvent(), for the default
// processing of a key message.
void SendNativeKeyEvent(QWidget *widget, QKeyEvent *event);

// The mouse position in a window message: client coordinates of wnd.
LPARAM MousePosLParam(HWND wnd, const QPoint &globalPos);
WPARAM MouseKeyFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

// ---------- Menus (ShimMenu.cpp) ----------

// Shows a menu as the menu bar of a top-level window (NULL removes it).
void SetWindowMenuBar(HWND wnd, HMENU menu);
void DestroyWindowMenu(HWND wnd);

// ---------- Icons ----------

QIcon IconFromHandle(HICON icon);

}

#endif
