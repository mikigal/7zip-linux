// ShimDialog.cpp -- dialogs from the resource templates: creation, layout in
// dialog units, the default dialog procedure, modal loops and the keyboard
// interface that IsDialogMessage() provides on Windows.

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QEventLoop>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>

#include "ShimDialog.h"
#include "ShimWindow.h"

#include "RcTables.h"

#include "../../CPP/7zip/UI/GUI/CompressDialogRes.h"
#include "../../CPP/7zip/UI/GUI/ExtractDialogRes.h"

namespace NShim {

class CDialogWnd: public HWND__
{
public:
  DLGPROC DlgProc;
  LONG_PTR MsgResult;   // DWLP_MSGRESULT
  LONG_PTR DlgUser;     // DWLP_USER
  UINT DefId;
  bool Ended;
  INT_PTR Result;
  QEventLoop *Loop;
  CBaseUnits Units;
  QSize LastSize;
  bool InitDone;

  CDialogWnd():
      HWND__(kKind_Dialog),
      DlgProc(NULL),
      MsgResult(0),
      DlgUser(0),
      DefId(0),
      Ended(false),
      Result(0),
      Loop(NULL),
      InitDone(false)
    {}

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override;
  bool OnEvent(QObject *watched, QEvent *event) override;
  LRESULT DefDlgProc(UINT message, WPARAM wParam, LPARAM lParam);
  void SetDefId(UINT id);
};

static CDialogWnd *AsDialog(HWND wnd)
{
  wnd = FromHandle(wnd);
  if (!wnd || wnd->Kind != kKind_Dialog)
    return NULL;
  return static_cast<CDialogWnd *>(wnd);
}

// ---------- Base units ----------

/* Windows converts dialog units with the average character width and the
   height of the dialog font (GdiGetCharDimensions): one horizontal unit is
   1/4 of the width and one vertical unit is 1/8 of the height. The same
   formula is applied to the Qt font of the dialog, so a dialog grows with the
   font as it does on Windows, and a text of one line fits into the 8 units
   that the templates give a static control. */
CBaseUnits ComputeBaseUnits(const QFont &font)
{
  const QFontMetrics fm(font);
  const int width = fm.horizontalAdvance(
      QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
  CBaseUnits u;
  u.X = (width / 26 + 1) / 2;
  u.Y = fm.height();
  if (u.X < 1)
    u.X = 1;
  if (u.Y < 1)
    u.Y = 1;
  return u;
}

static int MulDivRound(int a, int b, int c)
{
  const long long v = (long long)a * b;
  return (int)(v >= 0 ? (v + c / 2) / c : -((-v + c / 2) / c));
}

int CBaseUnits::ToPixelsX(int units) const { return MulDivRound(units, X, 4); }
int CBaseUnits::ToPixelsY(int units) const { return MulDivRound(units, Y, 8); }

QFont DialogFont()
{
  return QApplication::font();
}

CBaseUnits DialogBaseUnits(HWND dlg)
{
  CDialogWnd *d = AsDialog(dlg);
  if (d)
    return d->Units;
  return ComputeBaseUnits(DialogFont());
}

// ---------- The dialog widget ----------

static bool HandleDialogKey(QKeyEvent *e, QWidget *focus);

// QDialog gives the transient parent and the dialog window type on Wayland.
// Its own handling of Esc, Enter and closing is replaced by the Win32 rules:
// those go through WM_COMMAND and WM_CLOSE to the dialog procedure.
class CDialogWidget: public QDialog
{
public:
  CDialogWidget(QWidget *parent): QDialog(parent)
  {
    setSizeGripEnabled(false);
  }
  void reject() override {}
  void accept() override {}
protected:
  void keyPressEvent(QKeyEvent *e) override
  {
    // a key that the focus widget did not take; the window may be inactive,
    // then the focus widget of this window is the one that had the focus
    QWidget *focus = QApplication::focusWidget();
    if (!focus || focus->window() != this)
      focus = focusWidget();
    if (!HandleDialogKey(e, focus ? focus : this))
      QWidget::keyPressEvent(e);
  }
};

// ---------- Default dialog procedure ----------

static HWND FindItem(HWND dlg, int id)
{
  dlg = FromHandle(dlg);
  if (!dlg)
    return NULL;
  for (HWND c : dlg->Children)
    if ((int)c->Id == id)
      return c;
  return NULL;
}

static bool CanFocus(HWND c)
{
  return c->Widget && c->Widget->isVisible() && c->Widget->isEnabled();
}

static HWND NextTabItem(HWND dlg, HWND ctl, bool previous)
{
  const std::vector<HWND> &c = dlg->Children;
  const size_t n = c.size();
  if (n == 0)
    return NULL;
  size_t start = n - 1;
  if (previous)
    start = 0;
  for (size_t i = 0; i < n; i++)
    if (c[i] == ctl)
      start = i;
  for (size_t k = 1; k <= n; k++)
  {
    const size_t i = previous ? (start + n - k) % n : (start + k) % n;
    if ((c[i]->Style & WS_TABSTOP) && CanFocus(c[i]))
      return c[i];
  }
  return NULL;
}

// The group of ctl: from the nearest control with WS_GROUP at or before it
// to the control before the next WS_GROUP.
static void GroupRange(HWND dlg, HWND ctl, size_t &first, size_t &end)
{
  const std::vector<HWND> &c = dlg->Children;
  size_t pos = 0;
  for (size_t i = 0; i < c.size(); i++)
    if (c[i] == ctl)
      pos = i;
  first = pos;
  while (first > 0 && !(c[first]->Style & WS_GROUP))
    first--;
  end = pos + 1;
  while (end < c.size() && !(c[end]->Style & WS_GROUP))
    end++;
}

// Static controls and group boxes never take the focus.
static bool IsStaticLike(HWND c)
{
  return (Send(c, WM_GETDLGCODE, 0, 0) & DLGC_STATIC) != 0;
}

static HWND NextGroupItem(HWND dlg, HWND ctl, bool previous)
{
  size_t first, end;
  GroupRange(dlg, ctl, first, end);
  const std::vector<HWND> &c = dlg->Children;
  const size_t n = end - first;
  size_t pos = first;
  for (size_t i = first; i < end; i++)
    if (c[i] == ctl)
      pos = i;
  for (size_t k = 1; k <= n; k++)
  {
    const size_t i = first + (previous ? (pos - first + n - k) % n : (pos - first + k) % n);
    if (CanFocus(c[i]) && !IsStaticLike(c[i]))
      return c[i];
  }
  return ctl;
}

void CDialogWnd::SetDefId(UINT id)
{
  const HWND oldDef = DefId ? FindItem(this, (int)DefId) : NULL;
  if (oldDef && IsPushButton(oldDef))
    SetDefaultButtonLook(oldDef, false);
  DefId = id;
  const HWND newDef = FindItem(this, (int)id);
  if (newDef && IsPushButton(newDef))
    SetDefaultButtonLook(newDef, true);
}

LRESULT CDialogWnd::DefDlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_CLOSE:
    {
      // as if Cancel was pressed
      const HWND cancel = FindItem(this, IDCANCEL);
      if (!cancel || (cancel->Widget && cancel->Widget->isEnabled()))
        Post(this, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), (LPARAM)cancel);
      return 0;
    }
    case DM_GETDEFID:
      return DefId ? MAKELRESULT(DefId, DC_HASDEFID) : 0;
    case DM_SETDEFID:
      SetDefId((UINT)wParam);
      return TRUE;
    case WM_NEXTDLGCTL:
    {
      HWND target;
      if (LOWORD(lParam))
        target = FromHandle((HWND)wParam);
      else
        target = NextTabItem(this, FromWidget(QApplication::focusWidget()), wParam != 0);
      if (target && target->Widget)
      {
        QWidget *w = target->Widget->focusProxy() ? target->Widget->focusProxy() : target->Widget.data();
        w->setFocus(Qt::TabFocusReason);
      }
      return 0;
    }
    case WM_GETDLGCODE:
      return 0;
    default:
      return DefWindowProcImpl(this, message, wParam, lParam);
  }
}

LRESULT CDialogWnd::Proc(UINT message, WPARAM wParam, LPARAM lParam)
{
  const INT_PTR r = DlgProc ? DlgProc(this, message, wParam, lParam) : FALSE;
  if (r)
  {
    switch (message)
    {
      case WM_INITDIALOG:
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORSTATIC:
        return r;
      default:
        // the dialog procedure puts the result into DWLP_MSGRESULT
        return FromHandle(this) ? MsgResult : 0;
    }
  }
  if (!FromHandle(this))
    return 0;
  if (message == WM_INITDIALOG)
    return TRUE;
  return DefDlgProc(message, wParam, lParam);
}

bool CDialogWnd::OnEvent(QObject *watched, QEvent *event)
{
  if (watched != Widget.data())
    return false;
  switch (event->type())
  {
    case QEvent::Close:
      if (!Destroying && !Ended)
      {
        event->ignore();
        Send(this, WM_CLOSE, 0, 0);
        return true;
      }
      return false;
    case QEvent::Resize:
    {
      const QSize size = static_cast<QResizeEvent *>(event)->size();
      if (InitDone && size != LastSize)
      {
        LastSize = size;
        const WPARAM type = (Widget->isWindow() && Widget->isMaximized()) ? SIZE_MAXIMIZED : SIZE_RESTORED;
        Send(this, WM_SIZE, type, MAKELPARAM(size.width(), size.height()));
      }
      return false;
    }
    default:
      return false;
  }
}

// ---------- Keyboard (IsDialogMessage) ----------

// The top-level dialog that handles the keys for a focus widget: a property
// page passes them to its property sheet.
static CDialogWnd *KeyDialogFor(QWidget *focus)
{
  HWND w = FromWidget(focus);
  CDialogWnd *found = NULL;
  for (; w; w = w->IsChild() ? w->Parent : NULL)
    if (w->Kind == kKind_Dialog)
      found = static_cast<CDialogWnd *>(w);
  return found;
}

// The dialog (or property page) that directly contains the control.
static CDialogWnd *OwnerDialog(HWND ctl)
{
  if (ctl && ctl->Parent && ctl->Parent->Kind == kKind_Dialog)
    return static_cast<CDialogWnd *>(ctl->Parent);
  return NULL;
}

static bool WantsReturn(QWidget *focus)
{
  HWND ctl = FromWidget(focus);
  if (!ctl)
    return true;  // not a shim control: let it have the key
  if (ctl->Kind == kKind_Edit && (ctl->Style & ES_MULTILINE) && (ctl->Style & ES_WANTRETURN))
    return true;
  return (Send(ctl, WM_GETDLGCODE, VK_RETURN, 0) & DLGC_WANTALLKEYS) != 0;
}

static bool HandleDialogKey(QKeyEvent *e, QWidget *focus)
{
  if (!focus)
    return false;
  CDialogWnd *dlg = KeyDialogFor(focus);
  if (!dlg || dlg->Destroying)
    return false;
  const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier);
  HWND ctl = FromWidget(focus);
  if (ctl == dlg)
    ctl = NULL;

  switch (e->key())
  {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    {
      if (mods & (Qt::AltModifier))
        return false;
      if (WantsReturn(focus))
        return false;
      CDialogWnd *target = dlg;
      UINT id;
      HWND button = NULL;
      if (ctl && IsPushButton(ctl))
      {
        button = ctl;
        id = ctl->Id;
        target = OwnerDialog(ctl) ? OwnerDialog(ctl) : dlg;
      }
      else
      {
        const LRESULT def = Send(dlg, DM_GETDEFID, 0, 0);
        id = (HIWORD(def) == DC_HASDEFID) ? LOWORD(def) : IDOK;
        button = FindItem(dlg, (int)id);
      }
      if (button && button->Widget && !button->Widget->isEnabled())
      {
        QApplication::beep();
        return true;
      }
      Send(target, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)button);
      return true;
    }
    case Qt::Key_Escape:
      if (mods)
        return false;
      Send(dlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), (LPARAM)FindItem(dlg, IDCANCEL));
      return true;
    case Qt::Key_F1:
      if (mods)
        return false;
      Send(ctl ? ctl : dlg, WM_HELP, 0, 0);
      return true;
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    {
      // arrows move through the group when a button has the focus
      if (mods || !ctl || ctl->Kind != kKind_Button || !OwnerDialog(ctl))
        return false;
      const bool previous = (e->key() == Qt::Key_Up || e->key() == Qt::Key_Left);
      HWND next = NextGroupItem(OwnerDialog(ctl), ctl, previous);
      if (!next || next == ctl || !next->Widget)
        return true;
      QWidget *w = next->Widget->focusProxy() ? next->Widget->focusProxy() : next->Widget.data();
      w->setFocus(Qt::TabFocusReason);
      if (IsAutoRadioButton(next))
        ClickButton(next);
      return true;
    }
    default:
      return false;
  }
}

class CKeyFilter: public QObject
{
public:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (event->type() != QEvent::KeyPress)
      return false;
    QWidget *w = qobject_cast<QWidget *>(watched);
    if (!w)
      return false;
    // A key goes to the focus widget first, or to the window if nothing has
    // the focus; the propagation to the parents must not handle it again.
    QWidget *focus = QApplication::focusWidget();
    if (focus ? (w != focus) : !w->isWindow())
      return false;
    return HandleDialogKey(static_cast<QKeyEvent *>(event), w);
  }
};

static void InstallKeyFilter()
{
  static CKeyFilter *filter = NULL;
  if (!filter)
  {
    filter = new CKeyFilter;
    filter->setParent(QCoreApplication::instance());
    QCoreApplication::instance()->installEventFilter(filter);
  }
}

// ---------- Creation ----------

static QString ControlText(const wchar_t *text)
{
  return FromW(text);
}

static void SetupLabelBuddies(CDialogWnd *d);

static CModalHook g_ModalHook;

void SetModalHook(const CModalHook &hook)
{
  g_ModalHook = hook;
}

/* Controls of options that have no meaning on Linux (see Appendix A of
   linux/docs/PLAN.md). They are created, so that the dialog code works
   unchanged, but they stay disabled, or hidden where that leaves no gap
   in the original layout, whatever the code does. */
struct CLinuxOption
{
  UInt32 DialogId;
  UInt32 ControlId;
  bool Hide;
};

static const CLinuxOption kWindowsOnlyOptions[] =
{
  { IDD_EXTRACT, IDX_EXTRACT_NT_SECUR, true },                 // NTFS security
  { IDD_COMPRESS, IDX_COMPRESS_SFX, false },                   // no SFX module for Linux
  { IDD_COMPRESS, IDX_COMPRESS_SHARED, false },                // files open for writing (-ssw)
  { IDD_COMPRESS_OPTIONS, IDX_COMPRESS_NT_ALT_STREAMS, true }, // NTFS alternate streams
  { IDD_COMPRESS_OPTIONS, IDX_COMPRESS_NT_SECUR, true }        // NTFS security
};

static void ApplyLinuxOptions(UInt32 dialogId, HWND c)
{
  for (const CLinuxOption &o : kWindowsOnlyOptions)
  {
    if (o.DialogId != dialogId || o.ControlId != c->Id)
      continue;
    c->Style = (c->Style & ~(DWORD)WS_TABSTOP) | WS_DISABLED;
    c->Widget->setEnabled(false);
    c->AlwaysDisabled = true;
    if (o.Hide)
    {
      c->Style &= ~(DWORD)WS_VISIBLE;
      c->Widget->hide();
      c->AlwaysHidden = true;
    }
  }
}

HWND CreateDialogFromTemplate(const NRc::CDialog *t, HWND parent, DLGPROC proc, LPARAM param)
{
  HWND parentWnd = FromHandle(parent);
  return CreateDialogFromTemplateIn(t, parentWnd, parentWnd ? parentWnd->Widget.data() : NULL, proc, param);
}

HWND CreateDialogFromTemplateIn(const NRc::CDialog *t, HWND parent, QWidget *qtParent,
    DLGPROC proc, LPARAM param)
{
  InstallKeyFilter();
  const bool child = (t->Style & WS_CHILD) != 0;
  HWND parentWnd = FromHandle(parent);
  QWidget *parentWidget = qtParent;

  CDialogWnd *d = new CDialogWnd();
  d->Style = t->Style & ~(DWORD)WS_VISIBLE;
  d->ExStyle = t->ExStyle;
  d->Parent = parentWnd;
  QWidget *w;
  if (child)
  {
    w = new QWidget(parentWidget);
    w->hide();
  }
  else
  {
    w = new CDialogWidget(parentWidget ? parentWidget->window() : NULL);
    w->setWindowTitle(FromW(t->Caption));
    Qt::WindowFlags flags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
    if (t->Style & WS_MAXIMIZEBOX)
      flags |= Qt::WindowMaximizeButtonHint;
    if (t->Style & WS_MINIMIZEBOX)
      flags |= Qt::WindowMinimizeButtonHint;
    w->setWindowFlags(flags);
  }
  w->setAttribute(Qt::WA_DeleteOnClose, false);
  RegisterWindow(d, w);
  w->installEventFilter(d->Proxy);
  if (child && parentWnd)
    parentWnd->Children.push_back(d);
  if (t->Name)
    w->setObjectName(QString::fromLatin1(t->Name));

  const QFont font = DialogFont();
  w->setFont(font);
  d->Units = ComputeBaseUnits(font);
  const QSize size(d->Units.ToPixelsX(t->W), d->Units.ToPixelsY(t->H));
  if (child)
    w->resize(size);
  else
  {
    w->resize(size);
    if (!(t->Style & WS_THICKFRAME))
      w->setFixedSize(size);
  }

  for (unsigned i = 0; i < t->NumControls; i++)
  {
    const NRc::CControl &c = t->Controls[i];
    const int x = d->Units.ToPixelsX(c.X);
    const int y = d->Units.ToPixelsY(c.Y);
    const int cx = d->Units.ToPixelsX(c.W);
    const int cy = d->Units.ToPixelsY(c.H);
    const HWND ctl = CreateWindowImpl(c.ExStyle, QString::fromLatin1(c.Class), ControlText(c.Text),
        c.Style, x, y, cx, cy, d, c.Id, NULL, true);
    if (!ctl)
      qWarning("7-Zip shim: dialog %u: cannot create control %u of class %s", t->Id, c.Id, c.Class);
    else if ((c.Style & 0xF) == BS_DEFPUSHBUTTON && strcmp(c.Class, "Button") == 0 && d->DefId == 0)
      d->DefId = c.Id;
  }

  for (HWND c : std::vector<HWND>(d->Children))
  {
    c->OnDialogCreated();
    ApplyLinuxOptions(t->Id, c);
  }
  SetupLabelBuddies(d);

  d->DlgProc = proc;
  CWindowLock lock(d);
  const HWND first = NextTabItem(d, NULL, false);
  // size changes notify from WM_INITDIALOG on: the dialog object of the
  // NWindows wrappers is attached then
  d->InitDone = true;
  d->LastSize = w->size();
  const LRESULT setFocus = Send(d, WM_INITDIALOG, (WPARAM)first, param);
  if (!FromHandle(d))
    return NULL;
  if (setFocus && first && FromHandle(first) && first->Widget)
  {
    QWidget *fw = first->Widget->focusProxy() ? first->Widget->focusProxy() : first->Widget.data();
    fw->setFocus(Qt::OtherFocusReason);
  }
  return d;
}

/* A static control with a mnemonic gives the focus to the control after it,
   as on Windows; Qt shows the mnemonic of a label only when it has a buddy.
   The tab stops are not final yet (a radio button gets one when it is
   checked), so the buddy is the next control that is not static-like. */
void SetupLabelBuddyForStatic(HWND label, HWND buddy);

static void SetupLabelBuddies(CDialogWnd *d)
{
  const std::vector<HWND> &c = d->Children;
  for (size_t i = 0; i < c.size(); i++)
  {
    if (c[i]->Kind != kKind_Static)
      continue;
    HWND buddy = NULL;
    for (size_t k = i + 1; k < c.size(); k++)
      if (!IsStaticLike(c[k]))
      {
        buddy = c[k];
        break;
      }
    SetupLabelBuddyForStatic(c[i], buddy);
  }
}

static const NRc::CDialog *FindTemplate(const void *name, bool wide)
{
  if (!IS_INTRESOURCE(name))
  {
    // templates are referenced by number only
    (void)wide;
    return NULL;
  }
  return NRc::FindDialog((UInt32)(uintptr_t)name);
}

static HWND CreateDialogImpl(const void *name, bool wide, HWND parent, DLGPROC proc, LPARAM param)
{
  const NRc::CDialog *t = FindTemplate(name, wide);
  if (!t)
  {
    qWarning("7-Zip shim: no dialog template %u", (unsigned)(uintptr_t)name);
    return NULL;
  }
  HWND d = CreateDialogFromTemplate(t, parent, proc, param);
  if (d && (t->Style & WS_VISIBLE))
  {
    d->Style |= WS_VISIBLE;
    d->Widget->show();
  }
  return d;
}

INT_PTR RunModalDialog(HWND dlg)
{
  CDialogWnd *d = AsDialog(dlg);
  if (!d)
    return -1;
  CWindowLock lock(d);
  if (!d->Ended)
  {
    QWidget *w = d->Widget;
    w->setWindowModality(d->Parent ? Qt::WindowModal : Qt::NonModal);
    d->Style |= WS_VISIBLE;
    w->show();
    w->raise();
    w->activateWindow();
    QEventLoop loop;
    d->Loop = &loop;
    if (g_ModalHook)
    {
      const UInt64 serial = d->Serial;
      QMetaObject::invokeMethod(&loop, [d, serial]()
      {
        // a copy: the hook may replace itself
        const CModalHook hook = g_ModalHook;
        if (hook && FromHandle(d) && d->Serial == serial && !d->Ended)
          hook(d);
      }, Qt::QueuedConnection);
    }
    if (!d->Ended)
      loop.exec(QEventLoop::DialogExec);
    d->Loop = NULL;
  }
  const INT_PTR result = d->Result;
  HWND owner = FromHandle(d->Parent);
  QPointer<QWidget> ownerWindow = (owner && owner->Widget) ? owner->Widget->window() : NULL;
  DestroyWindowTree(d);
  ReactivateOwner(ownerWindow);
  return result;
}

static INT_PTR DialogBoxImpl(const void *name, bool wide, HWND parent, DLGPROC proc, LPARAM param)
{
  const NRc::CDialog *t = FindTemplate(name, wide);
  if (!t)
  {
    qWarning("7-Zip shim: no dialog template %u", (unsigned)(uintptr_t)name);
    return -1;
  }
  HWND d = CreateDialogFromTemplate(t, parent, proc, param);
  if (!d)
    return -1;
  return RunModalDialog(d);
}

bool IsInModalLoop(HWND dlg)
{
  return OnGuiThread<bool>([=]()
  {
    CDialogWnd *d = AsDialog(dlg);
    return d && d->Loop && !d->Ended;
  });
}

LONG_PTR GetDialogLong(HWND wnd, int index, bool &handled)
{
  CDialogWnd *d = AsDialog(wnd);
  handled = false;
  if (!d)
    return 0;
  handled = true;
  switch (index)
  {
    case DWLP_MSGRESULT: return d->MsgResult;
    case DWLP_DLGPROC: return (LONG_PTR)d->DlgProc;
    case DWLP_USER: return d->DlgUser;
    default: handled = false; return 0;
  }
}

LONG_PTR SetDialogLong(HWND wnd, int index, LONG_PTR value, bool &handled)
{
  CDialogWnd *d = AsDialog(wnd);
  handled = false;
  if (!d)
    return 0;
  handled = true;
  switch (index)
  {
    case DWLP_MSGRESULT: d->MsgResult = value; break;
    case DWLP_DLGPROC: d->DlgProc = (DLGPROC)value; break;
    case DWLP_USER: d->DlgUser = value; break;
    default: handled = false; break;
  }
  return 0;
}

}

using namespace NShim;

// ---------- API ----------

HWND CreateDialogParamW(HINSTANCE /* instance */, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM param)
{
  return OnGuiThread<HWND>([=]() { return CreateDialogImpl(templateName, true, parent, proc, param); });
}

HWND CreateDialogParamA(HINSTANCE /* instance */, LPCSTR templateName, HWND parent, DLGPROC proc, LPARAM param)
{
  return OnGuiThread<HWND>([=]() { return CreateDialogImpl(templateName, false, parent, proc, param); });
}

INT_PTR DialogBoxParamW(HINSTANCE /* instance */, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM param)
{
  return OnGuiThread<INT_PTR>([=]() { return DialogBoxImpl(templateName, true, parent, proc, param); });
}

INT_PTR DialogBoxParamA(HINSTANCE /* instance */, LPCSTR templateName, HWND parent, DLGPROC proc, LPARAM param)
{
  return OnGuiThread<INT_PTR>([=]() { return DialogBoxImpl(templateName, false, parent, proc, param); });
}

BOOL EndDialog(HWND dlg, INT_PTR result)
{
  return OnGuiThread<BOOL>([=]()
  {
    CDialogWnd *d = AsDialog(dlg);
    if (!d)
      return FALSE;
    d->Ended = true;
    d->Result = result;
    if (d->Widget && !d->IsChild())
      d->Widget->hide();
    if (d->Loop)
      d->Loop->exit();
    return TRUE;
  });
}

HWND GetDlgItem(HWND dlg, int id)
{
  return OnGuiThread<HWND>([=]() { return FindItem(dlg, id); });
}

int GetDlgCtrlID(HWND wnd)
{
  return OnGuiThread<int>([=]()
  {
    HWND w = FromHandle(wnd);
    return w ? (int)w->Id : 0;
  });
}

BOOL SetDlgItemTextW(HWND dlg, int id, LPCWSTR s)
{
  const HWND item = GetDlgItem(dlg, id);
  return item ? SetWindowTextW(item, s) : FALSE;
}

BOOL SetDlgItemTextA(HWND dlg, int id, LPCSTR s)
{
  const HWND item = GetDlgItem(dlg, id);
  return item ? SetWindowTextA(item, s) : FALSE;
}

UINT GetDlgItemTextW(HWND dlg, int id, LPWSTR s, int maxCount)
{
  const HWND item = GetDlgItem(dlg, id);
  if (!item)
  {
    if (s && maxCount > 0)
      s[0] = 0;
    return 0;
  }
  return (UINT)GetWindowTextW(item, s, maxCount);
}

UINT GetDlgItemTextA(HWND dlg, int id, LPSTR s, int maxCount)
{
  const HWND item = GetDlgItem(dlg, id);
  if (!item)
  {
    if (s && maxCount > 0)
      s[0] = 0;
    return 0;
  }
  return (UINT)GetWindowTextA(item, s, maxCount);
}

BOOL SetDlgItemInt(HWND dlg, int id, UINT value, BOOL isSigned)
{
  const QString s = isSigned ? QString::number((int)value) : QString::number(value);
  const std::wstring w = s.toStdWString();
  return SetDlgItemTextW(dlg, id, w.c_str());
}

UINT GetDlgItemInt(HWND dlg, int id, BOOL *translated, BOOL isSigned)
{
  if (translated)
    *translated = FALSE;
  const HWND item = GetDlgItem(dlg, id);
  if (!item)
    return 0;
  const int len = GetWindowTextLengthW(item);
  std::wstring w((size_t)len + 1, L'\0');
  GetWindowTextW(item, &w[0], len + 1);
  const QString s = FromW(w.c_str()).trimmed();
  bool ok = false;
  UINT v = 0;
  if (isSigned)
    v = (UINT)s.toInt(&ok);
  else
    v = s.toUInt(&ok);
  if (!ok)
    return 0;
  if (translated)
    *translated = TRUE;
  return v;
}

BOOL CheckDlgButton(HWND dlg, int id, UINT check)
{
  const HWND item = GetDlgItem(dlg, id);
  if (!item)
    return FALSE;
  SendMessageW(item, BM_SETCHECK, check, 0);
  return TRUE;
}

UINT IsDlgButtonChecked(HWND dlg, int id)
{
  const HWND item = GetDlgItem(dlg, id);
  return item ? (UINT)SendMessageW(item, BM_GETCHECK, 0, 0) : 0;
}

BOOL CheckRadioButton(HWND dlg, int firstId, int lastId, int checkId)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND d = FromHandle(dlg);
    if (!d)
      return FALSE;
    const std::vector<HWND> children = d->Children;
    for (HWND c : children)
      if ((int)c->Id >= firstId && (int)c->Id <= lastId)
        Send(c, BM_SETCHECK, (int)c->Id == checkId ? BST_CHECKED : BST_UNCHECKED, 0);
    return TRUE;
  });
}

LRESULT SendDlgItemMessageW(HWND dlg, int id, UINT message, WPARAM wParam, LPARAM lParam)
{
  const HWND item = GetDlgItem(dlg, id);
  return item ? SendMessageW(item, message, wParam, lParam) : 0;
}

LRESULT SendDlgItemMessageA(HWND dlg, int id, UINT message, WPARAM wParam, LPARAM lParam)
{
  const HWND item = GetDlgItem(dlg, id);
  return item ? SendMessageA(item, message, wParam, lParam) : 0;
}

HWND GetNextDlgGroupItem(HWND dlg, HWND ctl, BOOL previous)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    HWND d = FromHandle(dlg);
    if (!d || !FromHandle(ctl))
      return NULL;
    return NextGroupItem(d, ctl, previous != FALSE);
  });
}

HWND GetNextDlgTabItem(HWND dlg, HWND ctl, BOOL previous)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    HWND d = FromHandle(dlg);
    if (!d)
      return NULL;
    return NextTabItem(d, FromHandle(ctl), previous != FALSE);
  });
}

BOOL MapDialogRect(HWND dlg, LPRECT rect)
{
  return OnGuiThread<BOOL>([=]()
  {
    CDialogWnd *d = AsDialog(dlg);
    if (!d || !rect)
      return FALSE;
    rect->left = d->Units.ToPixelsX(rect->left);
    rect->right = d->Units.ToPixelsX(rect->right);
    rect->top = d->Units.ToPixelsY(rect->top);
    rect->bottom = d->Units.ToPixelsY(rect->bottom);
    return TRUE;
  });
}

LONG GetDialogBaseUnits()
{
  return OnGuiThread<LONG>([]()
  {
    const CBaseUnits u = ComputeBaseUnits(DialogFont());
    return (LONG)MAKELONG(u.X, u.Y);
  });
}

BOOL IsDialogMessageW(HWND /* dlg */, LPMSG /* msg */)
{
  // GetMessage() returns only WM_NULL; the key filter handles the keys
  return FALSE;
}

BOOL IsDialogMessageA(HWND dlg, LPMSG msg)
{
  return IsDialogMessageW(dlg, msg);
}

LRESULT DefDlgProcW(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return OnGuiThread<LRESULT>([=]() -> LRESULT
  {
    CDialogWnd *d = AsDialog(wnd);
    if (!d)
      return 0;
    CWindowLock lock(d);
    return d->DefDlgProc(message, wParam, lParam);
  });
}

LRESULT DefDlgProcA(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return DefDlgProcW(wnd, message, wParam, lParam);
}
