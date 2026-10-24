// ShimBars.cpp -- the bars of the file manager window: toolbars
// (ToolbarWindow32), rebars (ReBarWindow32) and status bars
// (msctls_statusbar32), and SetParent().
//
// The bars are laid out by hand, as their Windows counterparts are: a
// toolbar places its buttons in rows (TBSTYLE_WRAPABLE) and sizes itself
// on TB_AUTOSIZE, a rebar puts the child windows of its bands in one row
// and keeps the height of its tallest band, and a status bar draws its
// parts at the edges that SB_SETPARTS gives.

#include <QApplication>
#include <QEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QToolButton>
#include <QToolTip>

#include <vector>

#include "ShimResources.h"
#include "ShimWindow.h"

namespace NShim {

// ---------- Toolbar ----------

class CToolbarWnd;

// A toolbar button sends WM_COMMAND to the parent of the toolbar.
class CToolbarButton: public QToolButton
{
public:
  CToolbarButton(QWidget *parent): QToolButton(parent) {}
};

class CToolbarWnd: public HWND__
{
public:
  struct CButton
  {
    int Bitmap;
    int Command;
    BYTE State;
    BYTE Style;
    DWORD_PTR Data;
    QString Text;
    QToolButton *Button;   // NULL for a separator
  };

  std::vector<CButton> Buttons;
  QStringList Strings;      // TB_ADDSTRING
  HIMAGELIST ImageList;
  UINT_PTR StdBitmap;       // a comctl32 bitmap (CreateToolbarEx), or -1
  QSize BitmapSize;
  DWORD TbExStyle;

  CToolbarWnd(): HWND__(kKind_Toolbar), ImageList(NULL), StdBitmap((UINT_PTR)-1),
      BitmapSize(16, 16), TbExStyle(0) {}

  QSize IconSize() const
  {
    if (ImageList)
      return QSize(ImageList->Cx, ImageList->Cy);
    return BitmapSize;
  }

  QIcon ButtonIcon(int bitmap) const
  {
    if (ImageList)
      return (bitmap >= 0 && bitmap < (int)ImageList->Icons.size()) ? ImageList->Icons[bitmap] : QIcon();
    if (StdBitmap != (UINT_PTR)-1)
      return StandardToolbarIcon(StdBitmap, bitmap);
    return QIcon();
  }

  bool HasText() const
  {
    for (const CButton &b : Buttons)
      if (!b.Text.isEmpty())
        return true;
    return false;
  }

  // All buttons have the size of the largest one, as without BTNS_AUTOSIZE.
  QSize ButtonSize() const
  {
    const QSize icon = IconSize();
    int w = icon.width() + 8;
    int h = icon.height() + 8;
    if (HasText())
    {
      const QFontMetrics fm(Widget->font());
      for (const CButton &b : Buttons)
        if (!b.Text.isEmpty())
          w = qMax(w, fm.horizontalAdvance(b.Text) + 12);
      h += fm.height() + 2;
    }
    return QSize(w, h);
  }

  static int SeparatorWidth() { return 8; }

  // Places the buttons in rows of the given width; returns the height.
  int Layout(int width, bool apply)
  {
    const QSize size = ButtonSize();
    const bool wrap = (Style & TBSTYLE_WRAPABLE) != 0;
    int x = 0;
    int y = 0;
    for (CButton &b : Buttons)
    {
      if (b.State & TBSTATE_HIDDEN)
      {
        if (apply && b.Button)
          b.Button->hide();
        continue;
      }
      const int w = b.Button ? size.width() : SeparatorWidth();
      if (wrap && x > 0 && x + w > width && b.Button)
      {
        x = 0;
        y += size.height();
      }
      if (apply && b.Button)
      {
        b.Button->setGeometry(x, y, size.width(), size.height());
        b.Button->show();
      }
      x += w;
    }
    return Buttons.empty() ? size.height() : y + size.height();
  }

  int TotalWidth() const
  {
    const QSize size = ButtonSize();
    int w = 0;
    for (const CButton &b : Buttons)
      if (!(b.State & TBSTATE_HIDDEN))
        w += b.Button ? size.width() : SeparatorWidth();
    return w;
  }

  void UpdateButton(CButton &b)
  {
    QToolButton *t = b.Button;
    if (!t)
      return;
    t->setIcon(ButtonIcon(b.Bitmap));
    t->setIconSize(IconSize());
    t->setText(b.Text);
    t->setToolButtonStyle(HasText() ?
        ((Style & TBSTYLE_LIST) ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonTextUnderIcon) :
        Qt::ToolButtonIconOnly);
    t->setEnabled((b.State & TBSTATE_ENABLED) != 0);
    t->setCheckable((b.Style & BTNS_CHECK) != 0);
    t->setChecked((b.State & TBSTATE_CHECKED) != 0);
  }

  void UpdateAll()
  {
    for (CButton &b : Buttons)
      UpdateButton(b);
    Layout(Widget->width(), true);
  }

  void Clicked(int command)
  {
    if (Parent)
      Send(Parent, WM_COMMAND, MAKEWPARAM(command, BN_CLICKED), (LPARAM)(HWND)this);
  }

  // The tooltip of a button comes from the parent (TTN_GETDISPINFO).
  QString ToolTipText(int command)
  {
    if (!(Style & TBSTYLE_TOOLTIPS))
      return QString();
    NMTTDISPINFOW nm;
    memset(&nm, 0, sizeof(nm));
    nm.hdr.idFrom = (UINT_PTR)command;
    nm.lpszText = nm.szText;
    NotifyParent(&nm.hdr, TTN_GETDISPINFOW);
    if (!nm.lpszText || nm.lpszText == LPSTR_TEXTCALLBACKW)
      return QString();
    if (IS_INTRESOURCE(nm.lpszText))
      return QString();
    return StripMnemonic(FromW(nm.lpszText));
  }

  int IndexOfCommand(int command) const
  {
    for (size_t i = 0; i < Buttons.size(); i++)
      if (Buttons[i].Button && Buttons[i].Command == command)
        return (int)i;
    return -1;
  }

  template <class TButton>
  bool AddButtons(unsigned num, const TButton *buttons, int pos, bool wide)
  {
    for (unsigned i = 0; i < num; i++)
    {
      const TButton &tb = buttons[i];
      CButton b;
      b.Bitmap = tb.iBitmap;
      b.Command = tb.idCommand;
      b.State = tb.fsState;
      b.Style = tb.fsStyle;
      b.Data = tb.dwData;
      b.Button = NULL;
      if (!IS_INTRESOURCE(tb.iString) && tb.iString != -1)
        b.Text = wide ? FromW((LPCWSTR)tb.iString) : FromA((LPCSTR)tb.iString);
      else if (tb.iString >= 0 && tb.iString < Strings.size())
        b.Text = Strings[(int)tb.iString];
      if (!(b.Style & BTNS_SEP))
      {
        CToolbarButton *t = new CToolbarButton(Widget);
        // flat buttons, as TBSTYLE_FLAT and the toolbars of Windows 10
        t->setAutoRaise(true);
        t->setFocusPolicy(Qt::NoFocus);
        t->installEventFilter(Proxy);
        const int command = b.Command;
        Connect(this, t, &QToolButton::clicked, [command](CToolbarWnd *self, bool) { self->Clicked(command); });
        b.Button = t;
      }
      const size_t at = (pos < 0 || pos > (int)Buttons.size()) ? Buttons.size() : (size_t)pos++;
      Buttons.insert(Buttons.begin() + (ptrdiff_t)at, b);
    }
    UpdateAll();
    return true;
  }

  bool OnEvent(QObject *watched, QEvent *event) override
  {
    if (event->type() == QEvent::ToolTip)
    {
      for (const CButton &b : Buttons)
        if (b.Button == watched)
        {
          const QString text = ToolTipText(b.Command);
          QHelpEvent *e = static_cast<QHelpEvent *>(event);
          if (text.isEmpty())
            QToolTip::hideText();
          else
            QToolTip::showText(e->globalPos(), text, b.Button);
          return true;
        }
      return false;
    }
    if (watched == Widget.data() && event->type() == QEvent::Resize)
      Layout(Widget->width(), true);
    return false;
  }

  // TB_AUTOSIZE: without CCS_NORESIZE a toolbar takes the width of its
  // parent and the height of its rows, at the top (or bottom) of it.
  void AutoSize()
  {
    if (Style & CCS_NORESIZE)
    {
      Layout(Widget->width(), true);
      return;
    }
    QWidget *parent = Widget->parentWidget();
    const int width = parent ? parent->width() : Widget->width();
    const int height = Layout(width, false);
    int y = 0;
    if (!(Style & CCS_NOPARENTALIGN))
    {
      if ((Style & CCS_BOTTOM) == CCS_BOTTOM && parent)
        y = parent->height() - height;
      Widget->setGeometry(0, y, width, height);
    }
    else
      Widget->resize(width, height);
    Layout(width, true);
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    switch (message)
    {
      case TB_BUTTONSTRUCTSIZE:
        return 0;
      case TB_SETIMAGELIST:
      {
        HIMAGELIST prev = ImageList;
        ImageList = (HIMAGELIST)lParam;
        UpdateAll();
        return (LRESULT)prev;
      }
      case TB_GETIMAGELIST:
        return (LRESULT)ImageList;
      case TB_SETBITMAPSIZE:
        BitmapSize = QSize(LOWORD(lParam), HIWORD(lParam));
        UpdateAll();
        return TRUE;
      case TB_ADDBITMAP:
      {
        const TBADDBITMAP *ab = (const TBADDBITMAP *)lParam;
        if (ab && ab->hInst == HINST_COMMCTRL)
        {
          StdBitmap = ab->nID;
          UpdateAll();
          return 0;
        }
        return -1;
      }
      case TB_ADDBUTTONSW:
        return AddButtons((unsigned)wParam, (const TBBUTTON *)lParam, -1, true) ? TRUE : FALSE;
      case TB_ADDBUTTONSA:
        return AddButtons((unsigned)wParam, (const TBBUTTON *)lParam, -1, false) ? TRUE : FALSE;
      case TB_INSERTBUTTONW:
        return AddButtons(1, (const TBBUTTON *)lParam, (int)wParam, true) ? TRUE : FALSE;
      case TB_INSERTBUTTONA:
        return AddButtons(1, (const TBBUTTON *)lParam, (int)wParam, false) ? TRUE : FALSE;
      case TB_ADDSTRINGW:
      case TB_ADDSTRINGA:
      {
        if (wParam != 0)
          return -1;  // string resources are not used
        const int first = Strings.size();
        if (message == TB_ADDSTRINGW)
          for (LPCWSTR s = (LPCWSTR)lParam; s && *s; s += wcslen(s) + 1)
            Strings.append(FromW(s));
        else
          for (LPCSTR s = (LPCSTR)lParam; s && *s; s += strlen(s) + 1)
            Strings.append(FromA(s));
        return first;
      }
      case TB_DELETEBUTTON:
      {
        const int i = (int)wParam;
        if (i < 0 || i >= (int)Buttons.size())
          return FALSE;
        delete Buttons[(size_t)i].Button;
        Buttons.erase(Buttons.begin() + i);
        UpdateAll();
        return TRUE;
      }
      case TB_BUTTONCOUNT:
        return (LRESULT)Buttons.size();
      case TB_COMMANDTOINDEX:
        return IndexOfCommand((int)wParam);
      case TB_ENABLEBUTTON:
      case TB_CHECKBUTTON:
      case TB_HIDEBUTTON:
      case TB_PRESSBUTTON:
      {
        const int i = IndexOfCommand((int)wParam);
        if (i < 0)
          return FALSE;
        CButton &b = Buttons[(size_t)i];
        const bool on = LOWORD(lParam) != 0;
        const BYTE flag = message == TB_ENABLEBUTTON ? TBSTATE_ENABLED :
            message == TB_CHECKBUTTON ? TBSTATE_CHECKED :
            message == TB_HIDEBUTTON ? TBSTATE_HIDDEN : TBSTATE_PRESSED;
        if (on)
          b.State |= flag;
        else
          b.State &= (BYTE)~flag;
        UpdateButton(b);
        if (message == TB_HIDEBUTTON)
          Layout(Widget->width(), true);
        return TRUE;
      }
      case TB_ISBUTTONENABLED:
      case TB_ISBUTTONCHECKED:
      case TB_ISBUTTONHIDDEN:
      case TB_GETSTATE:
      {
        const int i = IndexOfCommand((int)wParam);
        if (i < 0)
          return message == TB_GETSTATE ? -1 : FALSE;
        const BYTE st = Buttons[(size_t)i].State;
        switch (message)
        {
          case TB_ISBUTTONENABLED: return (st & TBSTATE_ENABLED) != 0;
          case TB_ISBUTTONCHECKED: return (st & TBSTATE_CHECKED) != 0;
          case TB_ISBUTTONHIDDEN: return (st & TBSTATE_HIDDEN) != 0;
          default: return st;
        }
      }
      case TB_SETSTATE:
      {
        const int i = IndexOfCommand((int)wParam);
        if (i < 0)
          return FALSE;
        Buttons[(size_t)i].State = (BYTE)LOWORD(lParam);
        UpdateButton(Buttons[(size_t)i]);
        Layout(Widget->width(), true);
        return TRUE;
      }
      case TB_AUTOSIZE:
        AutoSize();
        return 0;
      case TB_GETBUTTONSIZE:
      {
        const QSize size = ButtonSize();
        return MAKELONG(size.width(), size.height());
      }
      case TB_SETBUTTONSIZE:
        return TRUE;
      case TB_GETMAXSIZE:
      {
        SIZE *sz = (SIZE *)lParam;
        if (!sz)
          return FALSE;
        sz->cx = TotalWidth();
        sz->cy = ButtonSize().height();
        return TRUE;
      }
      case TB_GETITEMRECT:
      case TB_GETRECT:
      {
        RECT *r = (RECT *)lParam;
        const int i = message == TB_GETITEMRECT ? (int)wParam : IndexOfCommand((int)wParam);
        if (!r || i < 0 || i >= (int)Buttons.size() || !Buttons[(size_t)i].Button)
          return FALSE;
        const QRect q = Buttons[(size_t)i].Button->geometry();
        r->left = q.left();
        r->top = q.top();
        r->right = q.left() + q.width();
        r->bottom = q.top() + q.height();
        return TRUE;
      }
      case TB_GETROWS:
        return qMax(1, Layout(Widget->width(), false) / qMax(1, ButtonSize().height()));
      case TB_SETEXTENDEDSTYLE:
      {
        const DWORD prev = TbExStyle;
        TbExStyle = (DWORD)lParam;
        return prev;
      }
      case TB_GETEXTENDEDSTYLE:
        return TbExStyle;
      case TB_SETMAXTEXTROWS:
      case TB_SETINDENT:
      case TB_SETTOOLTIPS:
      case TB_SETPARENT:
      case TB_SETPADDING:
        return 0;
      case CCM_SETUNICODEFORMAT:
        return TRUE;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateToolbar(DWORD style, QWidget *parent)
{
  CToolbarWnd *w = new CToolbarWnd();
  w->Style = style;
  QWidget *q = new QWidget(parent);
  RegisterWindow(w, q);
  q->installEventFilter(w->Proxy);
  return w;
}

// ---------- Rebar ----------

class CReBarWnd: public HWND__
{
public:
  struct CBand
  {
    UINT Style;
    HWND Child;
    UINT CxMinChild;
    UINT CyMinChild;
    UINT Cx;
    UINT Id;
    LPARAM Param;
  };
  std::vector<CBand> Bands;
  int LastHeight;

  CReBarWnd(): HWND__(kKind_ReBar), LastHeight(-1) {}

  static int Border() { return 2; }

  int RowHeight() const
  {
    int h = 0;
    for (const CBand &b : Bands)
      if (!(b.Style & RBBS_HIDDEN))
        h = qMax(h, (int)b.CyMinChild);
    return h;
  }

  int OwnHeight() const
  {
    return RowHeight() + 2 * Border();
  }

  // One row: the bands get their widths, the last one the rest of the row.
  void LayoutBands()
  {
    const int width = Widget->width();
    const int height = RowHeight();
    std::vector<size_t> visible;
    for (size_t i = 0; i < Bands.size(); i++)
      if (!(Bands[i].Style & RBBS_HIDDEN))
        visible.push_back(i);
    int x = Border();
    for (size_t k = 0; k < visible.size(); k++)
    {
      const CBand &b = Bands[visible[k]];
      const bool last = (k + 1 == visible.size());
      int w = last ? width - Border() - x : qMax((int)b.Cx, (int)b.CxMinChild);
      if (w < (int)b.CxMinChild)
        w = (int)b.CxMinChild;
      HWND child = FromHandle(b.Child);
      if (child && child->Widget)
      {
        int ch = (int)b.CyMinChild;
        if (child->Kind == kKind_ComboBox || child->Kind == kKind_ComboBoxEx)
          ch = ComboBoxClosedHeight(child);
        child->Widget->setGeometry(x, Border() + (height - ch) / 2, w, ch);
      }
      x += w + Border();
    }
  }

  void HeightMayHaveChanged()
  {
    const int h = OwnHeight();
    if (Widget->height() != h)
    {
      Widget->resize(Widget->width(), h);
      LayoutBands();
    }
    if (h != LastHeight)
    {
      LastHeight = h;
      NMHDR hdr;
      memset(&hdr, 0, sizeof(hdr));
      NotifyParent(&hdr, RBN_HEIGHTCHANGE);
    }
  }

  template <class TInfo>
  void ApplyBand(CBand &b, const TInfo *info)
  {
    const UINT mask = info->fMask;
    if (mask & RBBIM_STYLE)
      b.Style = info->fStyle;
    if (mask & RBBIM_CHILD)
    {
      b.Child = FromHandle(info->hwndChild);
      if (b.Child)
      {
        // as on Windows, the band's window becomes a child of the rebar
        Reparent(b.Child, this);
        if (b.Child->Widget)
          b.Child->Widget->show();
      }
    }
    if (mask & RBBIM_CHILDSIZE)
    {
      b.CxMinChild = info->cxMinChild;
      b.CyMinChild = info->cyMinChild;
    }
    if (mask & RBBIM_SIZE)
      b.Cx = info->cx;
    if (mask & RBBIM_ID)
      b.Id = info->wID;
    if (mask & RBBIM_LPARAM)
      b.Param = info->lParam;
  }

  template <class TInfo>
  LRESULT InsertBand(int index, const TInfo *info)
  {
    if (!info)
      return FALSE;
    CBand b;
    memset(&b, 0, sizeof(b));
    ApplyBand(b, info);
    const size_t at = (index < 0 || index > (int)Bands.size()) ? Bands.size() : (size_t)index;
    Bands.insert(Bands.begin() + (ptrdiff_t)at, b);
    LayoutBands();
    HeightMayHaveChanged();
    return TRUE;
  }

  template <class TInfo>
  LRESULT SetBandInfo(unsigned index, const TInfo *info)
  {
    if (!info || index >= Bands.size())
      return FALSE;
    ApplyBand(Bands[index], info);
    LayoutBands();
    HeightMayHaveChanged();
    return TRUE;
  }

  bool OnEvent(QObject *watched, QEvent *event) override
  {
    if (watched == Widget.data() && event->type() == QEvent::Resize)
      LayoutBands();
    return false;
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    switch (message)
    {
      case RB_SETBARINFO:
        return TRUE;
      case RB_INSERTBANDW:
        return InsertBand((int)(INT_PTR)wParam, (const REBARBANDINFOW *)lParam);
      case RB_INSERTBANDA:
        return InsertBand((int)(INT_PTR)wParam, (const REBARBANDINFOA *)lParam);
      case RB_SETBANDINFOW:
        return SetBandInfo((unsigned)wParam, (const REBARBANDINFOW *)lParam);
      case RB_SETBANDINFOA:
        return SetBandInfo((unsigned)wParam, (const REBARBANDINFOA *)lParam);
      case RB_DELETEBAND:
        if (wParam >= Bands.size())
          return FALSE;
        Bands.erase(Bands.begin() + (ptrdiff_t)wParam);
        LayoutBands();
        HeightMayHaveChanged();
        return TRUE;
      case RB_GETBANDCOUNT:
        return (LRESULT)Bands.size();
      case RB_GETROWCOUNT:
        return Bands.empty() ? 0 : 1;
      case RB_GETBARHEIGHT:
      case RB_GETROWHEIGHT:
        return OwnHeight();
      case RB_SHOWBAND:
        if (wParam >= Bands.size())
          return FALSE;
        if (lParam)
          Bands[wParam].Style &= ~(UINT)RBBS_HIDDEN;
        else
          Bands[wParam].Style |= RBBS_HIDDEN;
        if (HWND c = FromHandle(Bands[wParam].Child))
          if (c->Widget)
            c->Widget->setVisible(lParam != 0);
        LayoutBands();
        HeightMayHaveChanged();
        return TRUE;
      case RB_MAXIMIZEBAND:
      case RB_MINIMIZEBAND:
      case RB_SIZETORECT:
        LayoutBands();
        return TRUE;
      case RB_IDTOINDEX:
        for (size_t i = 0; i < Bands.size(); i++)
          if (Bands[i].Id == (UINT)wParam)
            return (LRESULT)i;
        return -1;
      case WM_COMMAND:
      case WM_NOTIFY:
        // the band windows report to the parent of the rebar
        if (Parent)
          return Send(Parent, message, wParam, lParam);
        return 0;
      case CCM_SETUNICODEFORMAT:
        return TRUE;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateReBar(DWORD style, QWidget *parent)
{
  CReBarWnd *w = new CReBarWnd();
  w->Style = style;
  QWidget *q = new QWidget(parent);
  RegisterWindow(w, q);
  q->installEventFilter(w->Proxy);
  q->resize(q->width(), w->OwnHeight());
  return w;
}

// ---------- Status bar ----------

class CStatusBarWnd;

class CStatusBarWidget: public QWidget
{
public:
  CStatusBarWnd *Wnd;
  CStatusBarWidget(QWidget *parent): QWidget(parent), Wnd(NULL) {}
  void paintEvent(QPaintEvent *event) override;
};

class CStatusBarWnd: public HWND__
{
public:
  std::vector<int> Edges;     // right edges; -1 is the right side of the bar
  std::vector<QString> Texts;
  std::vector<UINT> Types;    // SBT_*
  bool Simple;
  QString SimpleText;

  CStatusBarWnd(): HWND__(kKind_StatusBar), Simple(false)
  {
    Edges.push_back(-1);
    Texts.push_back(QString());
    Types.push_back(0);
  }

  int OwnHeight() const
  {
    return Widget->fontMetrics().height() + 8;
  }

  QRect PartRect(size_t i) const
  {
    const int width = Widget->width();
    const int left = (i == 0) ? 0 : qMin(Edges[i - 1] < 0 ? width : Edges[i - 1], width);
    const int right = Edges[i] < 0 ? width : qMin(Edges[i], width);
    return QRect(left, 0, qMax(0, right - left), Widget->height());
  }

  void SetText(const QString &s) override
  {
    if (Simple)
      SimpleText = s;
    else
      Texts[0] = s;
    Widget->update();
  }

  QString Text() const override
  {
    return Simple ? SimpleText : Texts[0];
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    switch (message)
    {
      case SB_SETPARTS:
      {
        const unsigned n = (unsigned)wParam;
        const int *edges = (const int *)lParam;
        if (n == 0 || n > 256 || !edges)
          return FALSE;
        Edges.assign(edges, edges + n);
        Texts.resize(n);
        Types.resize(n);
        Widget->update();
        return TRUE;
      }
      case SB_GETPARTS:
      {
        int *edges = (int *)lParam;
        for (unsigned i = 0; edges && i < (unsigned)wParam && i < Edges.size(); i++)
          edges[i] = Edges[i];
        return (LRESULT)Edges.size();
      }
      case SB_SETTEXTW:
      case SB_SETTEXTA:
      {
        const unsigned index = (unsigned)(wParam & 0xFF);
        const QString s = message == SB_SETTEXTW ? FromW((LPCWSTR)lParam) : FromA((LPCSTR)lParam);
        if (index == 0xFF || Simple)
        {
          SimpleText = s;
          Widget->update();
          return TRUE;
        }
        if (index >= Texts.size())
          return FALSE;
        Texts[index] = s;
        Types[index] = (UINT)(wParam & 0xFF00);
        Widget->update(PartRect(index));
        return TRUE;
      }
      case SB_GETTEXTW:
      case SB_GETTEXTLENGTHW:
      {
        const unsigned index = (unsigned)wParam;
        if (index >= Texts.size())
          return 0;
        const QString &s = Texts[index];
        if (message == SB_GETTEXTW && lParam)
          CopyToW(s, (LPWSTR)lParam, LengthW(s) + 1);
        return MAKELRESULT(LengthW(s), Types[index]);
      }
      case SB_SIMPLE:
        Simple = (wParam != 0);
        Widget->update();
        return TRUE;
      case SB_ISSIMPLE:
        return Simple;
      case SB_GETRECT:
      {
        RECT *r = (RECT *)lParam;
        if (!r || wParam >= Edges.size())
          return FALSE;
        const QRect q = PartRect((size_t)wParam);
        r->left = q.left();
        r->top = q.top();
        r->right = q.left() + q.width();
        r->bottom = q.top() + q.height();
        return TRUE;
      }
      case SB_GETBORDERS:
      {
        int *b = (int *)lParam;
        if (b)
        {
          b[0] = 0;
          b[1] = 2;
          b[2] = 2;
        }
        return TRUE;
      }
      case SB_SETMINHEIGHT:
      case SB_SETICON:
        return TRUE;
      case CCM_SETUNICODEFORMAT:
        return TRUE;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

// Parts as on Windows 10: text on the window color, a thin line between
// the parts.
void CStatusBarWidget::paintEvent(QPaintEvent * /* event */)
{
  if (!Wnd)
    return;
  QPainter p(this);
  const QPalette &pal = palette();
  p.fillRect(rect(), pal.color(QPalette::Window));
  p.setPen(pal.color(QPalette::Mid).lighter(150));
  p.drawLine(0, 0, width(), 0);
  const int pad = fontMetrics().averageCharWidth();
  if (Wnd->Simple)
  {
    p.setPen(pal.color(QPalette::WindowText));
    p.drawText(rect().adjusted(pad, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft,
        fontMetrics().elidedText(Wnd->SimpleText, Qt::ElideRight, width() - 2 * pad));
    return;
  }
  for (size_t i = 0; i < Wnd->Edges.size(); i++)
  {
    const QRect r = Wnd->PartRect(i);
    if (r.width() <= 0)
      continue;
    QString s = Wnd->Texts[i];
    // a tab centers (one) or right-aligns (two) the text that follows
    Qt::Alignment align = Qt::AlignLeft;
    if (s.startsWith(QLatin1String("\t\t")))
    {
      align = Qt::AlignRight;
      s = s.mid(2);
    }
    else if (s.startsWith(QLatin1Char('\t')))
    {
      align = Qt::AlignHCenter;
      s = s.mid(1);
    }
    p.setPen(pal.color(QPalette::WindowText));
    const QRect tr = r.adjusted(pad, 1, -pad, 0);
    p.drawText(tr, Qt::AlignVCenter | align, fontMetrics().elidedText(s, Qt::ElideRight, tr.width()));
    if (i + 1 < Wnd->Edges.size() && !(Wnd->Types[i] & SBT_NOBORDERS))
    {
      p.setPen(pal.color(QPalette::Mid).lighter(150));
      p.drawLine(r.right(), 4, r.right(), r.bottom() - 3);
    }
  }
}

HWND CreateStatusBar(DWORD style, QWidget *parent)
{
  CStatusBarWnd *w = new CStatusBarWnd();
  w->Style = style;
  CStatusBarWidget *q = new CStatusBarWidget(parent);
  q->Wnd = w;
  RegisterWindow(w, q);
  q->installEventFilter(w->Proxy);
  // a status bar sits at the bottom of its parent, as wide as the parent
  if (parent && !(style & CCS_NOPARENTALIGN))
    q->setGeometry(0, parent->height() - w->OwnHeight(), parent->width(), w->OwnHeight());
  else
    q->resize(q->width(), w->OwnHeight());
  return w;
}

// ---------- Sizes ----------

int BarOwnHeight(HWND wnd)
{
  if (!wnd->Widget)
    return 0;
  switch (wnd->Kind)
  {
    case kKind_ReBar:
      return static_cast<CReBarWnd *>(wnd)->OwnHeight();
    case kKind_StatusBar:
      return static_cast<CStatusBarWnd *>(wnd)->OwnHeight();
    case kKind_Toolbar:
      if (!(wnd->Style & CCS_NORESIZE))
        return static_cast<CToolbarWnd *>(wnd)->Layout(wnd->Widget->width(), false);
      return 0;
    default:
      return 0;
  }
}

// ---------- SetParent ----------

void Reparent(HWND wnd, HWND newParent)
{
  if (!wnd || !newParent || wnd == newParent || !wnd->Widget)
    return;
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
  wnd->Parent = newParent;
  newParent->Children.push_back(wnd);
  QWidget *w = wnd->Widget;
  const bool visible = w->isVisibleTo(w->parentWidget());
  w->setParent(newParent->ClientArea());
  w->setVisible(visible);
}

}

using namespace NShim;

HWND CreateToolbarEx(HWND wnd, DWORD style, UINT id, int /* numBitmaps */, HINSTANCE bitmapInstance,
    UINT_PTR bitmapId, LPCTBBUTTON buttons, int numButtons, int /* dxButton */, int /* dyButton */,
    int dxBitmap, int dyBitmap, UINT /* structSize */)
{
  HWND w = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL, style, 0, 0, 0, 0, wnd,
      (HMENU)(UINT_PTR)id, NULL, NULL);
  if (!w)
    return NULL;
  return OnGuiThread<HWND>([=]() -> HWND
  {
    CToolbarWnd *t = static_cast<CToolbarWnd *>(FromHandle(w));
    if (!t)
      return NULL;
    if (bitmapInstance == HINST_COMMCTRL)
      t->StdBitmap = bitmapId;
    if (dxBitmap > 0 && dyBitmap > 0)
      t->BitmapSize = QSize(dxBitmap, dyBitmap);
    else if (bitmapId == IDB_VIEW_LARGE_COLOR || bitmapId == IDB_STD_LARGE_COLOR)
      t->BitmapSize = QSize(24, 24);
    if (buttons && numButtons > 0)
      t->AddButtons((unsigned)numButtons, buttons, -1, false);
    t->AutoSize();
    return t;
  });
}

HWND CreateStatusWindowW(LONG style, LPCWSTR text, HWND parent, UINT id)
{
  HWND w = CreateWindowExW(0, STATUSCLASSNAMEW, text, (DWORD)style, 0, 0, 0, 0, parent,
      (HMENU)(UINT_PTR)id, NULL, NULL);
  return w;
}

HWND CreateStatusWindowA(LONG style, LPCSTR text, HWND parent, UINT id)
{
  const std::wstring t = FromA(text).toStdWString();
  return CreateStatusWindowW(style, text ? t.c_str() : NULL, parent, id);
}

HWND SetParent(HWND child, HWND newParent)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    HWND c = FromHandle(child);
    HWND p = FromHandle(newParent);
    if (!c || !p)
      return NULL;
    const HWND prev = c->Parent;
    Reparent(c, p);
    return prev;
  });
}

BOOL IsChild(HWND parent, HWND wnd)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    for (w = w ? w->Parent : NULL; w; w = w->IsChild() ? w->Parent : NULL)
      if (w == parent)
        return TRUE;
    return FALSE;
  });
}
