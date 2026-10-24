// ShimControls.cpp -- the standard controls (Button, Static, Edit, ComboBox),
// the progress bar, up-down and trackbar common controls, and windows of
// registered classes.

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFocusEvent>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <map>

#include "ShimResources.h"
#include "ShimWindow.h"

namespace NShim {

static void SetFocusPolicy(QWidget *w, DWORD style, bool canFocus)
{
  if (!canFocus)
    w->setFocusPolicy(Qt::NoFocus);
  else
    w->setFocusPolicy((style & WS_TABSTOP) ? Qt::StrongFocus : Qt::ClickFocus);
}

// ---------- Button ----------

// On Windows the whole control rectangle of a check box or radio button
// reacts to the mouse, not only the box and the text; the templates make
// them much wider than their text.

class CCheckBox: public QCheckBox
{
public:
  bool Auto;
  CCheckBox(QWidget *parent): QCheckBox(parent), Auto(true) {}
protected:
  void nextCheckState() override
  {
    if (Auto)
      QCheckBox::nextCheckState();
  }
  bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }
};

class CRadioButton: public QRadioButton
{
public:
  bool Auto;
  CRadioButton(QWidget *parent): QRadioButton(parent), Auto(true)
  {
    // exclusivity follows the Win32 groups (WS_GROUP), see OnClicked()
    setAutoExclusive(false);
  }
protected:
  void nextCheckState() override
  {
    if (Auto)
      setChecked(true);
  }
  bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }
};

class CButtonWnd: public HWND__
{
public:
  CButtonWnd(): HWND__(kKind_Button) {}

  UINT Type() const { return Style & BS_TYPEMASK; }
  bool IsPush() const
  {
    const UINT t = Type();
    return t == BS_PUSHBUTTON || t == BS_DEFPUSHBUTTON || t == BS_PUSHBOX
        || t == BS_USERBUTTON || t == BS_OWNERDRAW;
  }
  bool IsCheck() const
  {
    const UINT t = Type();
    return t == BS_CHECKBOX || t == BS_AUTOCHECKBOX || t == BS_3STATE || t == BS_AUTO3STATE;
  }
  bool IsRadio() const { return Type() == BS_RADIOBUTTON || Type() == BS_AUTORADIOBUTTON; }
  bool IsGroup() const { return Type() == BS_GROUPBOX; }
  QAbstractButton *Button() const { return qobject_cast<QAbstractButton *>(Widget.data()); }

  void OnClicked()
  {
    if (Type() == BS_AUTORADIOBUTTON)
      UncheckGroup();
    NotifyCommand(BN_CLICKED);
  }

  // An automatic radio button unchecks the other automatic radio buttons of
  // its group.
  void UncheckGroup()
  {
    if (!Parent)
      return;
    const std::vector<HWND> &c = Parent->Children;
    size_t pos = 0;
    for (size_t i = 0; i < c.size(); i++)
      if (c[i] == this)
        pos = i;
    size_t first = pos;
    while (first > 0 && !(c[first]->Style & WS_GROUP))
      first--;
    size_t end = pos + 1;
    while (end < c.size() && !(c[end]->Style & WS_GROUP))
      end++;
    for (size_t i = first; i < end; i++)
      if (c[i] != this && IsAutoRadioButton(c[i]))
        static_cast<CButtonWnd *>(c[i])->Button()->setChecked(false);
  }

  void SetText(const QString &s) override
  {
    TextValue = s;
    if (IsGroup())
      static_cast<QGroupBox *>(Widget.data())->setTitle(StripMnemonic(s));
    else if (Button())
      Button()->setText(s);
  }

  QString Text() const override { return TextValue; }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    QAbstractButton *b = Button();
    switch (message)
    {
      case BM_GETCHECK:
        if (!b || IsPush())
          return BST_UNCHECKED;
        if (IsCheck())
        {
          const Qt::CheckState s = static_cast<QCheckBox *>(b)->checkState();
          return s == Qt::Checked ? BST_CHECKED : s == Qt::PartiallyChecked ? BST_INDETERMINATE : BST_UNCHECKED;
        }
        return b->isChecked() ? BST_CHECKED : BST_UNCHECKED;
      case BM_SETCHECK:
        if (!b || IsPush())
          return 0;
        if (IsCheck())
          static_cast<QCheckBox *>(b)->setCheckState(wParam == BST_CHECKED ? Qt::Checked :
              wParam == BST_INDETERMINATE ? Qt::PartiallyChecked : Qt::Unchecked);
        else
          b->setChecked(wParam == BST_CHECKED);
        return 0;
      case BM_GETSTATE:
      {
        LRESULT r = Proc(BM_GETCHECK, 0, 0);
        if (b && b->isDown())
          r |= BST_PUSHED;
        if (Widget && Widget->hasFocus())
          r |= BST_FOCUS;
        return r;
      }
      case BM_SETSTATE:
        if (b)
          b->setDown(wParam != 0);
        return 0;
      case BM_CLICK:
        if (b && b->isEnabled())
          b->click();
        return 0;
      case BM_SETSTYLE:
      {
        const UINT type = (UINT)wParam & BS_TYPEMASK;
        if (IsPush() && (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON))
        {
          Style = (Style & ~(DWORD)BS_TYPEMASK) | type;
          static_cast<QPushButton *>(b)->setDefault(type == BS_DEFPUSHBUTTON);
        }
        return 0;
      }
      case BM_SETIMAGE:
        if (b && wParam == IMAGE_ICON)
          b->setIcon(IconFromHandle((HICON)lParam));
        return 0;
      case WM_GETDLGCODE:
        if (IsPush())
          return DLGC_BUTTON | ((Type() == BS_DEFPUSHBUTTON) ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
        if (IsRadio())
          return DLGC_BUTTON | DLGC_RADIOBUTTON;
        if (IsGroup())
          return DLGC_STATIC;
        return DLGC_BUTTON;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateButton(DWORD style, QWidget *parent)
{
  CButtonWnd *w = new CButtonWnd();
  w->Style = style;
  QWidget *widget;
  switch (style & BS_TYPEMASK)
  {
    case BS_GROUPBOX:
    {
      QGroupBox *g = new QGroupBox(parent);
      g->setFocusPolicy(Qt::NoFocus);
      // controls inside are siblings, as on Windows
      g->setAttribute(Qt::WA_TransparentForMouseEvents);
      widget = g;
      break;
    }
    case BS_CHECKBOX:
    case BS_AUTOCHECKBOX:
    case BS_3STATE:
    case BS_AUTO3STATE:
    {
      CCheckBox *c = new CCheckBox(parent);
      const UINT t = style & BS_TYPEMASK;
      c->Auto = (t == BS_AUTOCHECKBOX || t == BS_AUTO3STATE);
      c->setTristate(t == BS_3STATE || t == BS_AUTO3STATE);
      widget = c;
      break;
    }
    case BS_RADIOBUTTON:
    case BS_AUTORADIOBUTTON:
    {
      CRadioButton *r = new CRadioButton(parent);
      r->Auto = ((style & BS_TYPEMASK) == BS_AUTORADIOBUTTON);
      widget = r;
      break;
    }
    default:
    {
      QPushButton *p = new QPushButton(parent);
      p->setDefault((style & BS_TYPEMASK) == BS_DEFPUSHBUTTON);
      widget = p;
      break;
    }
  }
  RegisterWindow(w, widget);
  if (QAbstractButton *b = w->Button())
  {
    SetFocusPolicy(b, style, true);
    Connect(w, b, &QAbstractButton::clicked, [](CButtonWnd *self, bool) { self->OnClicked(); });
    if (w->IsRadio())
    {
      // Windows gives the tab stop of a radio button group to the checked button
      Connect(w, b, &QAbstractButton::toggled, [](CButtonWnd *self, bool checked)
      {
        if (checked)
          self->Style |= WS_TABSTOP;
        else
          self->Style &= ~(DWORD)WS_TABSTOP;
        SetFocusPolicy(self->Widget, self->Style, true);
      });
    }
  }
  return w;
}

bool IsRadioButton(HWND wnd)
{
  return wnd->Kind == kKind_Button && static_cast<CButtonWnd *>(wnd)->IsRadio();
}

bool IsAutoRadioButton(HWND wnd)
{
  return wnd->Kind == kKind_Button && static_cast<CButtonWnd *>(wnd)->Type() == BS_AUTORADIOBUTTON;
}

bool IsPushButton(HWND wnd)
{
  return wnd->Kind == kKind_Button && static_cast<CButtonWnd *>(wnd)->IsPush();
}

void SetDefaultButtonLook(HWND wnd, bool isDefault)
{
  if (!IsPushButton(wnd))
    return;
  CButtonWnd *b = static_cast<CButtonWnd *>(wnd);
  b->Style = (b->Style & ~(DWORD)BS_TYPEMASK) | (isDefault ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
  static_cast<QPushButton *>(b->Button())->setDefault(isDefault);
}

void ClickButton(HWND wnd)
{
  if (wnd->Kind != kKind_Button)
    return;
  QAbstractButton *b = static_cast<CButtonWnd *>(wnd)->Button();
  if (b && b->isEnabled())
    b->click();
}

// ---------- Static ----------

class CLabel: public QLabel
{
public:
  Qt::TextElideMode Elide;
  QString FullText;
  CLabel(QWidget *parent): QLabel(parent), Elide(Qt::ElideNone) {}

  void SetFullText(const QString &s)
  {
    FullText = s;
    UpdateText();
  }

  void UpdateText()
  {
    if (Elide == Qt::ElideNone)
      setText(FullText);
    else
      setText(fontMetrics().elidedText(FullText, Elide, width()));
  }

protected:
  void resizeEvent(QResizeEvent *e) override
  {
    QLabel::resizeEvent(e);
    if (Elide != Qt::ElideNone)
      UpdateText();
  }
};

class CStaticWnd: public HWND__
{
public:
  HICON Icon;
  HWND Buddy;
  CStaticWnd(): HWND__(kKind_Static), Icon(NULL), Buddy(NULL) {}

  UINT Type() const { return Style & SS_TYPEMASK; }
  CLabel *Label() const { return static_cast<CLabel *>(Widget.data()); }
  bool NoPrefix() const { return (Style & SS_NOPREFIX) != 0; }

  void ApplyText()
  {
    if (Type() == SS_ICON)
      return;
    QString s = TextValue;
    if (!NoPrefix() && !Buddy)
      s = StripMnemonic(s);
    Label()->SetFullText(s);
  }

  void SetText(const QString &s) override
  {
    if (Type() == SS_ICON)
    {
      // "#<id>" names an icon resource, as in the templates
      TextValue = s;
      if (s.startsWith(QLatin1Char('#')))
        SetIcon(LoadIconW(NULL, MAKEINTRESOURCEW(s.mid(1).toUInt())));
      return;
    }
    TextValue = s;
    ApplyText();
  }

  QString Text() const override { return TextValue; }

  // Windows sizes an icon static control to the icon: SM_CXICON by
  // SM_CYICON, or the size of the image with SS_REALSIZEIMAGE; with
  // SS_REALSIZECONTROL or SS_CENTERIMAGE the control keeps its size.
  void SetIcon(HICON icon)
  {
    Icon = icon;
    const QIcon qi = IconFromHandle(icon);
    QSize size(32, 32);
    if (Style & SS_REALSIZECONTROL)
      size = Widget->size();
    else if (Style & SS_REALSIZEIMAGE)
    {
      // the largest image of the icon
      QSize best(0, 0);
      for (const QSize &s : qi.availableSizes())
        if (s.width() * s.height() > best.width() * best.height())
          best = s;
      if (!best.isEmpty())
        size = best;
    }
    Label()->setPixmap(qi.isNull() ? QPixmap() : qi.pixmap(size, Widget->devicePixelRatioF()));
    if (!(Style & (SS_REALSIZECONTROL | SS_CENTERIMAGE)))
      Widget->resize(size);
  }

  bool OnEvent(QObject *watched, QEvent *event) override
  {
    if (watched == Widget.data() && (Style & SS_NOTIFY))
    {
      if (event->type() == QEvent::MouseButtonRelease)
        NotifyCommand(STN_CLICKED);
      else if (event->type() == QEvent::MouseButtonDblClick)
        NotifyCommand(STN_DBLCLK);
    }
    return false;
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    switch (message)
    {
      case STM_SETICON:
      {
        const HICON prev = Icon;
        SetIcon((HICON)wParam);
        return (LRESULT)prev;
      }
      case STM_GETICON:
        return (LRESULT)Icon;
      case STM_SETIMAGE:
      {
        const HICON prev = Icon;
        if (wParam == IMAGE_ICON)
          SetIcon((HICON)lParam);
        return (LRESULT)prev;
      }
      case STM_GETIMAGE:
        return wParam == IMAGE_ICON ? (LRESULT)Icon : 0;
      case WM_GETDLGCODE:
        return DLGC_STATIC;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateStatic(DWORD style, QWidget *parent)
{
  CStaticWnd *w = new CStaticWnd();
  w->Style = style;
  CLabel *l = new CLabel(parent);
  RegisterWindow(w, l);
  l->setTextFormat(Qt::PlainText);
  l->setFocusPolicy(Qt::NoFocus);
  const UINT type = style & SS_TYPEMASK;
  Qt::Alignment align = Qt::AlignTop;
  switch (type)
  {
    case SS_CENTER: align |= Qt::AlignHCenter; l->setWordWrap(true); break;
    case SS_RIGHT: align |= Qt::AlignRight; l->setWordWrap(true); break;
    case SS_LEFT: align |= Qt::AlignLeft; l->setWordWrap(true); break;
    case SS_ICON: align = Qt::AlignLeft | Qt::AlignTop; break;
    case SS_ETCHEDHORZ: l->setFrameShape(QFrame::HLine); l->setFrameShadow(QFrame::Sunken); break;
    case SS_ETCHEDVERT: l->setFrameShape(QFrame::VLine); l->setFrameShadow(QFrame::Sunken); break;
    case SS_ETCHEDFRAME:
    case SS_GRAYFRAME:
    case SS_BLACKFRAME:
    case SS_WHITEFRAME: l->setFrameShape(QFrame::Box); l->setFrameShadow(QFrame::Sunken); break;
    default: align |= Qt::AlignLeft; break;   // SS_LEFTNOWORDWRAP, SS_SIMPLE
  }
  if (style & SS_CENTERIMAGE)
    align = (align & ~Qt::AlignVertical_Mask) | Qt::AlignVCenter;
  l->setAlignment(align);
  if (style & SS_SUNKEN)
  {
    l->setFrameShape(QFrame::Panel);
    l->setFrameShadow(QFrame::Sunken);
  }
  switch (style & SS_ELLIPSISMASK)
  {
    case SS_ENDELLIPSIS: l->Elide = Qt::ElideRight; break;
    case SS_PATHELLIPSIS: l->Elide = Qt::ElideMiddle; break;
    case SS_WORDELLIPSIS: l->Elide = Qt::ElideRight; break;
    default: break;
  }
  if (style & SS_NOTIFY)
    l->installEventFilter(w->Proxy);
  return w;
}

void SetupLabelBuddyForStatic(HWND label, HWND buddy)
{
  if (label->Kind != kKind_Static)
    return;
  CStaticWnd *s = static_cast<CStaticWnd *>(label);
  if (s->NoPrefix() || s->Type() == SS_ICON || !s->TextValue.contains(QLatin1Char('&')))
    buddy = NULL;
  s->Buddy = buddy;
  QWidget *b = NULL;
  if (buddy && buddy->Widget)
    b = buddy->Widget->focusProxy() ? buddy->Widget->focusProxy() : buddy->Widget.data();
  s->Label()->setBuddy(b);
  s->ApplyText();
}

// ---------- Edit ----------

// A read-only edit control has the dialog background, as on Windows.
static void SetReadOnlyLook(QWidget *w, bool readOnly)
{
  const QPalette app = QApplication::palette();
  QPalette p = w->palette();
  p.setColor(QPalette::Base, app.color(readOnly ? QPalette::Window : QPalette::Base));
  w->setPalette(p);
}

class CEditWnd: public HWND__
{
public:
  bool InSetText;
  CEditWnd(): HWND__(kKind_Edit), InSetText(false) {}

  bool IsMulti() const { return (Style & ES_MULTILINE) != 0; }
  QLineEdit *Line() const { return qobject_cast<QLineEdit *>(Widget.data()); }
  QPlainTextEdit *Multi() const { return qobject_cast<QPlainTextEdit *>(Widget.data()); }

  void SetText(const QString &s) override
  {
    if (QLineEdit *e = Line())
    {
      // Windows sends EN_CHANGE for WM_SETTEXT to a single-line edit control
      e->setText(s);
      e->setModified(false);
    }
    else
    {
      InSetText = true;
      QString t = s;
      t.replace(QLatin1String("\r\n"), QLatin1String("\n"));
      Multi()->setPlainText(t);
      Multi()->document()->setModified(false);
      InSetText = false;
    }
  }

  QString Text() const override
  {
    if (QLineEdit *e = Line())
      return e->text();
    QString t = Multi()->toPlainText();
    t.replace(QLatin1String("\n"), QLatin1String("\r\n"));
    return t;
  }

  void OnTextChanged()
  {
    if (InSetText)
      return;
    NotifyCommand(EN_UPDATE);
    NotifyCommand(EN_CHANGE);
  }

  bool OnEvent(QObject * /* watched */, QEvent *event) override
  {
    if (event->type() == QEvent::FocusIn)
      NotifyCommand(EN_SETFOCUS);
    else if (event->type() == QEvent::FocusOut)
      NotifyCommand(EN_KILLFOCUS);
    return false;
  }

  void GetSel(int &start, int &end) const
  {
    if (QLineEdit *e = Line())
    {
      if (e->hasSelectedText())
      {
        start = e->selectionStart();
        end = start + (int)e->selectedText().size();
      }
      else
        start = end = e->cursorPosition();
    }
    else
    {
      const QTextCursor c = Multi()->textCursor();
      start = c.selectionStart();
      end = c.selectionEnd();
    }
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    QLineEdit *line = Line();
    QPlainTextEdit *multi = Multi();
    switch (message)
    {
      case EM_SETPASSWORDCHAR:
        if (line)
          line->setEchoMode(wParam ? QLineEdit::Password : QLineEdit::Normal);
        return 0;
      case EM_GETPASSWORDCHAR:
        return (line && line->echoMode() == QLineEdit::Password) ? (LRESULT)0x25CF : 0;
      case EM_LIMITTEXT:
        if (line)
          line->setMaxLength(wParam ? (int)wParam : 32767);
        return 0;
      case EM_SETREADONLY:
        if (line)
          line->setReadOnly(wParam != 0);
        else
          multi->setReadOnly(wParam != 0);
        if (wParam)
          Style |= ES_READONLY;
        else
          Style &= ~(DWORD)ES_READONLY;
        SetReadOnlyLook(Widget, wParam != 0);
        return TRUE;
      case EM_SETSEL:
      {
        int start = (int)(INT_PTR)wParam;
        int end = (int)lParam;
        const int len = line ? (int)line->text().size() : (int)multi->toPlainText().size();
        if (end < 0 || end > len)
          end = len;
        if (start < 0)
        {
          // remove the selection
          if (line)
            line->deselect();
          else
          {
            QTextCursor c = multi->textCursor();
            c.clearSelection();
            multi->setTextCursor(c);
          }
          return 0;
        }
        if (start > len)
          start = len;
        if (line)
          line->setSelection(start, end - start);
        else
        {
          QTextCursor c = multi->textCursor();
          c.setPosition(start);
          c.setPosition(end, QTextCursor::KeepAnchor);
          multi->setTextCursor(c);
        }
        return 0;
      }
      case EM_GETSEL:
      {
        int start, end;
        GetSel(start, end);
        if (wParam)
          *(DWORD *)wParam = (DWORD)start;
        if (lParam)
          *(DWORD *)lParam = (DWORD)end;
        return MAKELRESULT(start, end);
      }
      case EM_REPLACESEL:
      {
        const QString s = FromW((LPCWSTR)lParam);
        if (line)
          line->insert(s);
        else
        {
          QString t = s;
          t.replace(QLatin1String("\r\n"), QLatin1String("\n"));
          multi->insertPlainText(t);
        }
        return 0;
      }
      case EM_GETMODIFY:
        return line ? line->isModified() : multi->document()->isModified();
      case EM_SETMODIFY:
        if (line)
          line->setModified(wParam != 0);
        else
          multi->document()->setModified(wParam != 0);
        return 0;
      case EM_SCROLLCARET:
        if (multi)
          multi->ensureCursorVisible();
        return TRUE;
      case EM_GETLINECOUNT:
        return multi ? multi->document()->blockCount() : 1;
      case WM_GETDLGCODE:
      {
        LRESULT r = DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS;
        if (IsMulti() && (Style & ES_WANTRETURN))
          r |= DLGC_WANTALLKEYS;
        return r;
      }
      case WM_SETFONT:
        if (wParam)
          Widget->setFont(((HFONT)wParam)->Font);
        return 0;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateEdit(DWORD style, DWORD exStyle, QWidget *parent)
{
  CEditWnd *w = new CEditWnd();
  w->Style = style;
  const bool border = (style & WS_BORDER) || (exStyle & WS_EX_CLIENTEDGE);
  Qt::Alignment align = Qt::AlignLeft;
  if (style & ES_CENTER)
    align = Qt::AlignHCenter;
  else if (style & ES_RIGHT)
    align = Qt::AlignRight;
  if (style & ES_MULTILINE)
  {
    QPlainTextEdit *e = new QPlainTextEdit(parent);
    RegisterWindow(w, e);
    e->setTabChangesFocus(true);
    e->setReadOnly((style & ES_READONLY) != 0);
    e->setLineWrapMode((style & (WS_HSCROLL | ES_AUTOHSCROLL)) ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
    e->setVerticalScrollBarPolicy((style & WS_VSCROLL) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    e->setHorizontalScrollBarPolicy((style & WS_HSCROLL) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (!border)
      e->setFrameShape(QFrame::NoFrame);
    Connect(w, e, &QPlainTextEdit::textChanged, [](CEditWnd *self) { self->OnTextChanged(); });
  }
  else
  {
    QLineEdit *e = new QLineEdit(parent);
    RegisterWindow(w, e);
    e->setFrame(border);
    e->setAlignment(align | Qt::AlignVCenter);
    e->setReadOnly((style & ES_READONLY) != 0);
    if (style & ES_PASSWORD)
      e->setEchoMode(QLineEdit::Password);
    if (style & ES_NUMBER)
      e->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), e));
    Connect(w, e, &QLineEdit::textChanged, [](CEditWnd *self, const QString &) { self->OnTextChanged(); });
  }
  if (style & ES_READONLY)
    SetReadOnlyLook(w->Widget, true);
  w->Widget->installEventFilter(w->Proxy);
  SetFocusPolicy(w->Widget, style, true);
  return w;
}

// ---------- ComboBox ----------

class CComboWnd;

class CComboWidget: public QComboBox
{
public:
  CComboWnd *Wnd;
  CComboWidget(QWidget *parent): QComboBox(parent), Wnd(NULL) {}
  void showPopup() override;
  void hidePopup() override;
};

class CComboWnd: public HWND__
{
public:
  CComboWnd(): HWND__(kKind_ComboBox) {}

  CComboWidget *Combo() const { return static_cast<CComboWidget *>(Widget.data()); }
  bool IsEditable() const { return (Style & 3) != CBS_DROPDOWNLIST; }

  void SetText(const QString &s) override
  {
    QComboBox *c = Combo();
    if (c->isEditable())
      c->setEditText(s);
    else
    {
      const int i = c->findText(s, Qt::MatchFixedString);
      if (i >= 0)
        c->setCurrentIndex(i);
    }
  }

  QString Text() const override
  {
    return Combo()->currentText();
  }

  // Adding an item to an empty QComboBox selects it; a Win32 combo box keeps
  // its selection (none) and its edit text.
  void Insert(int pos, const QString &s)
  {
    QComboBox *c = Combo();
    const bool noSelection = (c->currentIndex() < 0);
    const QString text = c->isEditable() ? c->currentText() : QString();
    c->insertItem(pos, s, QVariant((qlonglong)0));
    if (noSelection && c->currentIndex() >= 0)
    {
      c->setCurrentIndex(-1);
      if (c->isEditable())
        c->setEditText(text);
    }
  }

  int SortedPos(const QString &s) const
  {
    QComboBox *c = Combo();
    int i = 0;
    while (i < c->count() && QString::compare(c->itemText(i), s, Qt::CaseInsensitive) <= 0)
      i++;
    return i;
  }

  int Find(int start, const QString &s, bool exact) const
  {
    QComboBox *c = Combo();
    const int n = c->count();
    if (n == 0)
      return CB_ERR;
    if (start < -1 || start >= n)
      start = -1;
    for (int k = 1; k <= n; k++)
    {
      const int i = (start + k) % n;
      const QString t = c->itemText(i);
      if (exact ? QString::compare(t, s, Qt::CaseInsensitive) == 0
                : t.startsWith(s, Qt::CaseInsensitive))
        return i;
    }
    return CB_ERR;
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    QComboBox *c = Combo();
    const int index = (int)(INT_PTR)wParam;
    switch (message)
    {
      case CB_ADDSTRING:
      {
        const QString s = FromW((LPCWSTR)lParam);
        const int pos = (Style & CBS_SORT) ? SortedPos(s) : c->count();
        Insert(pos, s);
        return pos;
      }
      case CB_INSERTSTRING:
      {
        const int pos = (index < 0 || index > c->count()) ? c->count() : index;
        Insert(pos, FromW((LPCWSTR)lParam));
        return pos;
      }
      case CB_DELETESTRING:
        if (index < 0 || index >= c->count())
          return CB_ERR;
        c->removeItem(index);
        return c->count();
      case CB_RESETCONTENT:
        c->clear();
        if (c->isEditable())
          c->setEditText(QString());
        return CB_OKAY;
      case CB_GETCOUNT:
        return c->count();
      case CB_GETCURSEL:
        return c->currentIndex();
      case CB_SETCURSEL:
        if (index < 0 || index >= c->count())
        {
          c->setCurrentIndex(-1);
          if (c->isEditable())
            c->setEditText(QString());
          return CB_ERR;
        }
        c->setCurrentIndex(index);
        return index;
      case CB_GETLBTEXTLEN:
        if (index < 0 || index >= c->count())
          return CB_ERR;
        return LengthW(c->itemText(index));
      case CB_GETLBTEXT:
      {
        if (index < 0 || index >= c->count())
          return CB_ERR;
        const std::wstring w = c->itemText(index).toStdWString();
        wmemcpy((wchar_t *)lParam, w.c_str(), w.size() + 1);
        return (LRESULT)w.size();
      }
      case CB_SETITEMDATA:
        if (index < 0 || index >= c->count())
          return CB_ERR;
        c->setItemData(index, QVariant((qlonglong)lParam));
        return CB_OKAY;
      case CB_GETITEMDATA:
        if (index < 0 || index >= c->count())
          return CB_ERR;
        return (LRESULT)c->itemData(index).toLongLong();
      case CB_FINDSTRING:
        return Find(index, FromW((LPCWSTR)lParam), false);
      case CB_FINDSTRINGEXACT:
        return Find(index, FromW((LPCWSTR)lParam), true);
      case CB_SELECTSTRING:
      {
        const int i = Find(index, FromW((LPCWSTR)lParam), false);
        if (i >= 0)
          c->setCurrentIndex(i);
        return i;
      }
      case CB_SHOWDROPDOWN:
        if (wParam)
          c->showPopup();
        else
          c->hidePopup();
        return TRUE;
      case CB_GETDROPPEDSTATE:
        return c->view() && c->view()->isVisible();
      case CB_LIMITTEXT:
        if (c->lineEdit())
          c->lineEdit()->setMaxLength(wParam ? (int)wParam : 32767);
        return TRUE;
      case CB_GETEDITSEL:
      {
        int start = 0, end = 0;
        if (QLineEdit *e = c->lineEdit())
        {
          start = e->hasSelectedText() ? e->selectionStart() : e->cursorPosition();
          end = start + (int)e->selectedText().size();
        }
        if (wParam)
          *(DWORD *)wParam = (DWORD)start;
        if (lParam)
          *(DWORD *)lParam = (DWORD)end;
        return MAKELRESULT(start, end);
      }
      case CB_SETEDITSEL:
        if (QLineEdit *e = c->lineEdit())
        {
          const int start = (short)LOWORD(lParam);
          int end = (short)HIWORD(lParam);
          if (start < 0)
            e->deselect();
          else
          {
            if (end < 0 || end > (int)e->text().size())
              end = (int)e->text().size();
            e->setSelection(start, end - start);
          }
          return TRUE;
        }
        return CB_ERR;
      case CB_SETDROPPEDWIDTH:
        c->view()->setMinimumWidth((int)wParam);
        return (LRESULT)wParam;
      case CB_GETITEMHEIGHT:
        return c->fontMetrics().height();
      case CB_SETEXTENDEDUI:
      case CB_SETITEMHEIGHT:
        return CB_OKAY;
      case WM_GETDLGCODE:
        return DLGC_WANTCHARS | DLGC_WANTARROWS;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

void CComboWidget::showPopup()
{
  if (Wnd && FromHandle(Wnd))
    Wnd->NotifyCommand(CBN_DROPDOWN);
  QComboBox::showPopup();
}

void CComboWidget::hidePopup()
{
  QComboBox::hidePopup();
  if (Wnd && FromHandle(Wnd))
    Wnd->NotifyCommand(CBN_CLOSEUP);
}

HWND CreateComboBox(DWORD style, QWidget *parent)
{
  CComboWnd *w = new CComboWnd();
  w->Style = style;
  CComboWidget *c = new CComboWidget(parent);
  c->Wnd = w;
  RegisterWindow(w, c);
  c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  c->setMinimumContentsLength(1);
  c->setMaxVisibleItems(30);
  if (w->IsEditable())
  {
    c->setEditable(true);
    c->setInsertPolicy(QComboBox::NoInsert);
    c->setDuplicatesEnabled(true);
    c->setCompleter(nullptr);
    Connect(w, c->lineEdit(), &QLineEdit::textEdited, [](CComboWnd *self, const QString &)
    {
      self->NotifyCommand(CBN_EDITUPDATE);
      self->NotifyCommand(CBN_EDITCHANGE);
    });
  }
  Connect(w, c, &QComboBox::activated, [](CComboWnd *self, int) { self->NotifyCommand(CBN_SELCHANGE); });
  SetFocusPolicy(c, style, true);
  return w;
}

int ComboBoxClosedHeight(HWND wnd)
{
  if (wnd->Kind != kKind_ComboBox || !wnd->Widget)
    return 0;
  return wnd->Widget->sizeHint().height();
}

// ---------- Progress bar ----------

class CProgressWnd: public HWND__
{
public:
  int Step;
  int State;
  CProgressWnd(): HWND__(kKind_Progress), Step(10), State(PBST_NORMAL) {}
  QProgressBar *Bar() const { return static_cast<QProgressBar *>(Widget.data()); }

  void SetBarColor(QPalette::ColorRole role, COLORREF color)
  {
    QPalette p = Bar()->palette();
    if (color == 0xFF000000)  // CLR_DEFAULT
      p.setColor(role, QApplication::palette().color(role));
    else
      p.setColor(role, QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    Bar()->setPalette(p);
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    QProgressBar *b = Bar();
    switch (message)
    {
      case PBM_SETRANGE:
      {
        const LRESULT prev = MAKELRESULT(b->minimum(), b->maximum());
        b->setRange(LOWORD(lParam), HIWORD(lParam));
        return prev;
      }
      case PBM_SETRANGE32:
      {
        const LRESULT prev = MAKELRESULT(b->minimum(), b->maximum());
        b->setRange((int)wParam, (int)lParam);
        return prev;
      }
      case PBM_GETRANGE:
        if (lParam)
        {
          ((PBRANGE *)lParam)->iLow = b->minimum();
          ((PBRANGE *)lParam)->iHigh = b->maximum();
        }
        return wParam ? b->minimum() : b->maximum();
      case PBM_SETPOS:
      {
        const int prev = b->value() < b->minimum() ? b->minimum() : b->value();
        b->setValue((int)wParam);
        return prev;
      }
      case PBM_DELTAPOS:
      {
        const int prev = b->value();
        b->setValue(prev + (int)wParam);
        return prev;
      }
      case PBM_GETPOS:
        return b->value() < b->minimum() ? b->minimum() : b->value();
      case PBM_SETSTEP:
      {
        const int prev = Step;
        Step = (int)wParam;
        return prev;
      }
      case PBM_STEPIT:
      {
        const int prev = b->value();
        int v = prev + Step;
        if (v > b->maximum())
          v = b->minimum() + (v - b->maximum());
        b->setValue(v);
        return prev;
      }
      case PBM_SETBARCOLOR:
        SetBarColor(QPalette::Highlight, (COLORREF)lParam);
        return 0;
      case PBM_SETBKCOLOR:
        SetBarColor(QPalette::Base, (COLORREF)lParam);
        return 0;
      case PBM_SETSTATE:
      {
        const LRESULT prev = State;
        State = (int)wParam;
        // paused is yellow and error is red, as on Windows
        const COLORREF colors[] = { 0xFF000000, 0xFF000000, RGB(0xc4, 0x2b, 0x1c), RGB(0xc7, 0xa6, 0x00) };
        SetBarColor(QPalette::Highlight, (State >= 0 && State <= 3) ? colors[State] : 0xFF000000);
        return prev;
      }
      case PBM_GETSTATE:
        return State;
      case PBM_SETMARQUEE:
        if (wParam)
          b->setRange(0, 0);
        else
          b->setRange(0, 100);
        return TRUE;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateProgress(DWORD style, QWidget *parent)
{
  CProgressWnd *w = new CProgressWnd();
  w->Style = style;
  QProgressBar *b = new QProgressBar(parent);
  RegisterWindow(w, b);
  b->setRange(0, 100);
  b->setValue(0);
  b->setTextVisible(false);
  b->setFocusPolicy(Qt::NoFocus);
  if (style & PBS_VERTICAL)
    b->setOrientation(Qt::Vertical);
  if (style & PBS_MARQUEE)
    b->setRange(0, 0);
  return w;
}

// ---------- Up-down ----------

class CUpDownWnd: public HWND__
{
public:
  int Min;
  int Max;
  int Pos;
  int Base;
  HWND Buddy;
  QToolButton *Up;
  QToolButton *Down;

  CUpDownWnd(): HWND__(kKind_UpDown), Min(100), Max(0), Pos(0), Base(10),
      Buddy(NULL), Up(NULL), Down(NULL) {}

  bool Horizontal() const { return (Style & UDS_HORZ) != 0; }

  QString FormatPos(int pos) const
  {
    if (Base == 16)
      return QStringLiteral("0x") + QString::number((unsigned)pos, 16).toUpper();
    if (Style & UDS_NOTHOUSANDS)
      return QString::number(pos);
    return QLocale::system().toString(pos);
  }

  void UpdateBuddy()
  {
    if (Buddy && FromHandle(Buddy) && (Style & UDS_SETBUDDYINT))
    {
      const std::wstring w = FormatPos(Pos).toStdWString();
      Send(Buddy, WM_SETTEXT, 0, (LPARAM)w.c_str());
    }
  }

  // Reads the position from the buddy text; false if it is not a number.
  bool ReadBuddy()
  {
    if (!Buddy || !FromHandle(Buddy) || !(Style & UDS_SETBUDDYINT))
      return true;
    QString s = Buddy->Text().trimmed();
    s.remove(QLocale::system().groupSeparator());
    bool ok = false;
    const int v = (Base == 16) ? s.toInt(&ok, 16) : s.toInt(&ok, 10);
    if (!ok)
      return false;
    const int lo = Min < Max ? Min : Max;
    const int hi = Min < Max ? Max : Min;
    if (v < lo || v > hi)
      return false;
    Pos = v;
    return true;
  }

  void StepBy(int dir)
  {
    ReadBuddy();
    // the up arrow moves toward Max
    const int delta = (Min <= Max) ? dir : -dir;
    NMUPDOWN nm;
    memset(&nm, 0, sizeof(nm));
    nm.iPos = Pos;
    nm.iDelta = delta;
    if (NotifyParent(&nm.hdr, UDN_DELTAPOS) != 0)
      return;
    if (!FromHandle(this))
      return;
    const int lo = Min < Max ? Min : Max;
    const int hi = Min < Max ? Max : Min;
    int v = Pos + nm.iDelta;
    if (v > hi)
      v = (Style & UDS_WRAP) ? lo : hi;
    if (v < lo)
      v = (Style & UDS_WRAP) ? hi : lo;
    if (v == Pos)
      return;
    Pos = v;
    UpdateBuddy();
    if (Parent)
    {
      const UINT msg = Horizontal() ? WM_HSCROLL : WM_VSCROLL;
      Send(Parent, msg, MAKEWPARAM(SB_THUMBPOSITION, (WORD)Pos), (LPARAM)this);
      if (FromHandle(this) && Parent)
        Send(Parent, msg, MAKEWPARAM(SB_ENDSCROLL, (WORD)Pos), (LPARAM)this);
    }
  }

  bool OnEvent(QObject *watched, QEvent *event) override
  {
    // UDS_ARROWKEYS: the arrow keys in the buddy change the position
    if (Buddy && FromHandle(Buddy) && watched == Buddy->Widget.data()
        && event->type() == QEvent::KeyPress && (Style & UDS_ARROWKEYS))
    {
      const int key = static_cast<QKeyEvent *>(event)->key();
      if (key == Qt::Key_Up || key == Qt::Key_Down)
      {
        StepBy(key == Qt::Key_Up ? 1 : -1);
        return true;
      }
    }
    return false;
  }

  void AttachBuddy(HWND buddy)
  {
    Buddy = buddy;
    if (!buddy || !buddy->Widget)
      return;
    buddy->Widget->installEventFilter(Proxy);
    if (Style & (UDS_ALIGNRIGHT | UDS_ALIGNLEFT))
    {
      QRect b = buddy->Widget->geometry();
      const int w = Widget->width() > 0 ? Widget->width() : 16;
      QRect u;
      if (Style & UDS_ALIGNRIGHT)
      {
        u = QRect(b.right() + 1 - w, b.top(), w, b.height());
        b.setWidth(b.width() - w);
      }
      else
      {
        u = QRect(b.left(), b.top(), w, b.height());
        b.setLeft(b.left() + w);
      }
      buddy->Widget->setGeometry(b);
      Widget->setGeometry(u);
    }
    UpdateBuddy();
  }

  void OnDialogCreated() override
  {
    if ((Style & UDS_AUTOBUDDY) && Parent)
    {
      const std::vector<HWND> &c = Parent->Children;
      for (size_t i = 1; i < c.size(); i++)
        if (c[i] == this)
        {
          AttachBuddy(c[i - 1]);
          break;
        }
    }
  }

  void SetText(const QString &s) override { TextValue = s; }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    switch (message)
    {
      case UDM_SETRANGE:
        Max = (short)LOWORD(lParam);
        Min = (short)HIWORD(lParam);
        return 0;
      case UDM_SETRANGE32:
        Min = (int)wParam;
        Max = (int)lParam;
        return 0;
      case UDM_GETRANGE:
        return MAKELRESULT((WORD)Max, (WORD)Min);
      case UDM_GETRANGE32:
        if (wParam)
          *(int *)wParam = Min;
        if (lParam)
          *(int *)lParam = Max;
        return 0;
      case UDM_SETPOS:
      case UDM_SETPOS32:
      {
        const int prev = Pos;
        int v = (message == UDM_SETPOS) ? (short)LOWORD(lParam) : (int)lParam;
        const int lo = Min < Max ? Min : Max;
        const int hi = Min < Max ? Max : Min;
        if (v < lo)
          v = lo;
        if (v > hi)
          v = hi;
        Pos = v;
        UpdateBuddy();
        return prev;
      }
      case UDM_GETPOS:
      {
        const bool ok = ReadBuddy();
        return MAKELRESULT((WORD)Pos, ok ? 0 : 1);
      }
      case UDM_GETPOS32:
      {
        const bool ok = ReadBuddy();
        if (lParam)
          *(BOOL *)lParam = ok ? FALSE : TRUE;
        return Pos;
      }
      case UDM_SETBUDDY:
      {
        const HWND prev = Buddy;
        AttachBuddy(FromHandle((HWND)wParam));
        return (LRESULT)prev;
      }
      case UDM_GETBUDDY:
        return (LRESULT)Buddy;
      case UDM_SETBASE:
      {
        const int prev = Base;
        if (wParam == 10 || wParam == 16)
          Base = (int)wParam;
        UpdateBuddy();
        return prev;
      }
      case UDM_SETACCEL:
        return TRUE;
      default:
        return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateUpDown(DWORD style, QWidget *parent)
{
  CUpDownWnd *w = new CUpDownWnd();
  w->Style = style;
  QWidget *box = new QWidget(parent);
  RegisterWindow(w, box);
  box->setFocusPolicy(Qt::NoFocus);
  QBoxLayout *layout = (style & UDS_HORZ) ? (QBoxLayout *)new QHBoxLayout(box) : (QBoxLayout *)new QVBoxLayout(box);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  w->Up = new QToolButton(box);
  w->Down = new QToolButton(box);
  w->Up->setArrowType((style & UDS_HORZ) ? Qt::RightArrow : Qt::UpArrow);
  w->Down->setArrowType((style & UDS_HORZ) ? Qt::LeftArrow : Qt::DownArrow);
  for (QToolButton *b : { w->Up, w->Down })
  {
    b->setAutoRepeat(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    b->setObjectName(QStringLiteral("z7UpDownButton"));
  }
  if (style & UDS_HORZ)
  {
    layout->addWidget(w->Down);
    layout->addWidget(w->Up);
  }
  else
  {
    layout->addWidget(w->Up);
    layout->addWidget(w->Down);
  }
  Connect(w, w->Up, &QToolButton::clicked, [](CUpDownWnd *self, bool) { self->StepBy(1); });
  Connect(w, w->Down, &QToolButton::clicked, [](CUpDownWnd *self, bool) { self->StepBy(-1); });
  return w;
}

// ---------- Trackbar ----------

class CTrackbarWnd: public HWND__
{
public:
  CTrackbarWnd(): HWND__(kKind_Trackbar) {}
  QSlider *Slider() const { return static_cast<QSlider *>(Widget.data()); }

  void OnMoved(int code)
  {
    if (!Parent)
      return;
    const UINT msg = (Style & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
    Send(Parent, msg, MAKEWPARAM(code, (WORD)Slider()->value()), (LPARAM)this);
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    QSlider *s = Slider();
    switch (message)
    {
      case TBM_GETPOS: return s->value();
      case TBM_GETRANGEMIN: return s->minimum();
      case TBM_GETRANGEMAX: return s->maximum();
      case TBM_SETPOS: s->setValue((int)lParam); return 0;
      case TBM_SETRANGE: s->setRange((short)LOWORD(lParam), (short)HIWORD(lParam)); return 0;
      case TBM_SETRANGEMIN: s->setMinimum((int)lParam); return 0;
      case TBM_SETRANGEMAX: s->setMaximum((int)lParam); return 0;
      case TBM_SETTICFREQ: s->setTickInterval((int)wParam); return 0;
      case TBM_SETPAGESIZE: { const int p = s->pageStep(); s->setPageStep((int)lParam); return p; }
      case TBM_SETLINESIZE: { const int p = s->singleStep(); s->setSingleStep((int)lParam); return p; }
      default: return DefWindowProcImpl(this, message, wParam, lParam);
    }
  }
};

HWND CreateTrackbar(DWORD style, QWidget *parent)
{
  CTrackbarWnd *w = new CTrackbarWnd();
  w->Style = style;
  QSlider *s = new QSlider((style & TBS_VERT) ? Qt::Vertical : Qt::Horizontal, parent);
  RegisterWindow(w, s);
  s->setRange(0, 100);
  if (!(style & TBS_NOTICKS))
    s->setTickPosition((style & TBS_BOTH) ? QSlider::TicksBothSides :
        (style & TBS_TOP) ? QSlider::TicksAbove : QSlider::TicksBelow);
  SetFocusPolicy(s, style, true);
  Connect(w, s, &QSlider::sliderMoved, [](CTrackbarWnd *self, int) { self->OnMoved(SB_THUMBTRACK); });
  Connect(w, s, &QSlider::sliderReleased, [](CTrackbarWnd *self) { self->OnMoved(SB_THUMBPOSITION); });
  Connect(w, s, &QSlider::actionTriggered, [](CTrackbarWnd *self, int action)
  {
    if (action == QAbstractSlider::SliderPageStepAdd || action == QAbstractSlider::SliderPageStepSub
        || action == QAbstractSlider::SliderSingleStepAdd || action == QAbstractSlider::SliderSingleStepSub)
      self->OnMoved(SB_ENDSCROLL);
  });
  return w;
}

// ---------- Registered classes ----------

struct CClassInfo
{
  WNDCLASSW Info;
  std::wstring Name;
};

static std::map<QString, CClassInfo> &Classes()
{
  static std::map<QString, CClassInfo> classes;
  return classes;
}

static QString ClassKey(const QString &name)
{
  return name.toLower();
}

class CGenericWnd: public HWND__
{
public:
  QSize LastSize;
  CGenericWnd(): HWND__(kKind_Generic) {}

  bool OnEvent(QObject *watched, QEvent *event) override
  {
    if (watched != Widget.data())
      return false;
    switch (event->type())
    {
      case QEvent::Resize:
      {
        const QSize size = static_cast<QResizeEvent *>(event)->size();
        if (size != LastSize)
        {
          LastSize = size;
          Send(this, WM_SIZE, SIZE_RESTORED, MAKELPARAM(size.width(), size.height()));
        }
        return false;
      }
      case QEvent::Close:
        if (!Destroying)
        {
          event->ignore();
          Send(this, WM_CLOSE, 0, 0);
          return true;
        }
        return false;
      default:
        return false;
    }
  }
};

const char *ClassName(HWND wnd)
{
  switch (wnd->Kind)
  {
    case kKind_Dialog: return "#32770";
    case kKind_Button: return "Button";
    case kKind_Static: return "Static";
    case kKind_Edit: return "Edit";
    case kKind_ComboBox: return "ComboBox";
    case kKind_ListView: return "SysListView32";
    case kKind_Progress: return "msctls_progress32";
    case kKind_UpDown: return "msctls_updown32";
    case kKind_Trackbar: return "msctls_trackbar32";
    default: return "";
  }
}

HWND CreateWindowImpl(DWORD exStyle, const QString &className, const QString &text,
    DWORD style, int x, int y, int width, int height, HWND parent, UINT id, LPVOID param,
    bool fromDialogTemplate)
{
  HWND parentWnd = FromHandle(parent);
  const bool child = (style & WS_CHILD) != 0;
  if (child && !parentWnd)
    return NULL;
  QWidget *parentWidget = parentWnd ? parentWnd->Widget.data() : NULL;
  const QString cls = ClassKey(className);

  HWND w = NULL;
  if (child)
  {
    if (cls == QLatin1String("button"))
      w = CreateButton(style, parentWidget);
    else if (cls == QLatin1String("static"))
      w = CreateStatic(style, parentWidget);
    else if (cls == QLatin1String("edit"))
      w = CreateEdit(style, exStyle, parentWidget);
    else if (cls == QLatin1String("combobox"))
      w = CreateComboBox(style, parentWidget);
    else if (cls == QLatin1String("syslistview32"))
      w = CreateListView(style, exStyle, parentWidget);
    else if (cls == QLatin1String("msctls_progress32"))
      w = CreateProgress(style, parentWidget);
    else if (cls == QLatin1String("msctls_updown32"))
      w = CreateUpDown(style, parentWidget);
    else if (cls == QLatin1String("msctls_trackbar32"))
      w = CreateTrackbar(style, parentWidget);
  }

  const auto it = Classes().find(cls);
  if (!w && it == Classes().end())
  {
    qWarning("7-Zip shim: unknown window class %s", qPrintable(className));
    return NULL;
  }

  if (w)
  {
    w->Id = id;
    w->ExStyle = exStyle;
    w->Parent = parentWnd;
    parentWnd->Children.push_back(w);
    QWidget *q = w->Widget;
    q->setObjectName(QStringLiteral("z7ctl_%1").arg(id));
    q->setGeometry(x, y, width, height);
    if (w->Kind == kKind_ComboBox)
      q->setGeometry(x, y, width, ComboBoxClosedHeight(w));
    q->setEnabled(!(style & WS_DISABLED));
    q->setVisible((style & WS_VISIBLE) != 0);
    w->SetText(text);
    return w;
  }

  // a window of a registered class
  CGenericWnd *g = new CGenericWnd();
  g->Style = style;
  g->ExStyle = exStyle;
  g->Id = child ? id : 0;
  g->Parent = parentWnd;
  g->WndProc = it->second.Info.lpfnWndProc;
  QWidget *q;
  if (child)
    q = new QWidget(parentWidget);
  else
  {
    q = new QWidget(parentWidget ? parentWidget->window() : NULL, parentWidget ? Qt::Dialog : Qt::Window);
    q->setWindowTitle(text);
  }
  RegisterWindow(g, q);
  q->installEventFilter(g->Proxy);
  if (child)
    parentWnd->Children.push_back(g);
  else
    g->TextValue = text;
  if (x != CW_USEDEFAULT && width != CW_USEDEFAULT)
    q->setGeometry(x, y, width, height);
  else if (width != CW_USEDEFAULT)
    q->resize(width, height);
  g->LastSize = q->size();
  if (child)
    g->TextValue = text;

  CWindowLock lock(g);
  const std::wstring name = text.toStdWString();
  const std::wstring clsName = className.toStdWString();
  CREATESTRUCTW cs;
  memset(&cs, 0, sizeof(cs));
  cs.lpCreateParams = param;
  cs.hwndParent = parentWnd;
  cs.x = x;
  cs.y = y;
  cs.cx = width;
  cs.cy = height;
  cs.style = (LONG)style;
  cs.dwExStyle = exStyle;
  cs.lpszName = name.c_str();
  cs.lpszClass = clsName.c_str();
  cs.hMenu = child ? (HMENU)(uintptr_t)id : NULL;
  if (!Send(g, WM_NCCREATE, 0, (LPARAM)&cs) || Send(g, WM_CREATE, 0, (LPARAM)&cs) == -1)
  {
    DestroyWindowTree(g);
    return NULL;
  }
  if (!FromHandle(g))
    return NULL;
  if (style & WS_VISIBLE)
    q->show();
  (void)fromDialogTemplate;
  return g;
}

}

using namespace NShim;

ATOM RegisterClassW(const WNDCLASSW *wndClass)
{
  return OnGuiThread<ATOM>([=]() -> ATOM
  {
    if (!wndClass || !wndClass->lpszClassName || IS_INTRESOURCE(wndClass->lpszClassName))
      return 0;
    CClassInfo info;
    info.Info = *wndClass;
    info.Name = wndClass->lpszClassName;
    const QString key = ClassKey(FromW(wndClass->lpszClassName));
    if (Classes().find(key) != Classes().end())
      return 0;  // ERROR_CLASS_ALREADY_EXISTS
    auto &entry = Classes()[key];
    entry = info;
    entry.Info.lpszClassName = entry.Name.c_str();
    entry.Info.lpszMenuName = NULL;
    return (ATOM)(0xC000 + Classes().size());
  });
}

ATOM RegisterClassA(const WNDCLASSA *wndClass)
{
  if (!wndClass || !wndClass->lpszClassName)
    return 0;
  const std::wstring name = FromA(wndClass->lpszClassName).toStdWString();
  WNDCLASSW w;
  w.style = wndClass->style;
  w.lpfnWndProc = wndClass->lpfnWndProc;
  w.cbClsExtra = wndClass->cbClsExtra;
  w.cbWndExtra = wndClass->cbWndExtra;
  w.hInstance = wndClass->hInstance;
  w.hIcon = wndClass->hIcon;
  w.hCursor = wndClass->hCursor;
  w.hbrBackground = wndClass->hbrBackground;
  w.lpszMenuName = NULL;
  w.lpszClassName = name.c_str();
  return RegisterClassW(&w);
}

BOOL GetClassInfoW(HINSTANCE /* instance */, LPCWSTR className, WNDCLASSW *wndClass)
{
  return OnGuiThread<BOOL>([=]()
  {
    if (!className || IS_INTRESOURCE(className))
      return FALSE;
    const auto it = Classes().find(ClassKey(FromW(className)));
    if (it == Classes().end())
      return FALSE;
    if (wndClass)
      *wndClass = it->second.Info;
    return TRUE;
  });
}

BOOL GetClassInfoA(HINSTANCE instance, LPCSTR className, WNDCLASSA *wndClass)
{
  if (!className || IS_INTRESOURCE(className))
    return FALSE;
  const std::wstring name = FromA(className).toStdWString();
  WNDCLASSW w;
  if (!GetClassInfoW(instance, name.c_str(), &w))
    return FALSE;
  if (wndClass)
  {
    memset(wndClass, 0, sizeof(*wndClass));
    wndClass->style = w.style;
    wndClass->lpfnWndProc = w.lpfnWndProc;
    wndClass->hInstance = w.hInstance;
    wndClass->hIcon = w.hIcon;
    wndClass->hCursor = w.hCursor;
    wndClass->hbrBackground = w.hbrBackground;
    wndClass->lpszClassName = className;
  }
  return TRUE;
}

HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style,
    int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE /* instance */, LPVOID param)
{
  if (!className || IS_INTRESOURCE(className))
    return NULL;
  const QString cls = FromW(className);
  const QString text = FromW(windowName);
  return OnGuiThread<HWND>([=]()
  {
    return CreateWindowImpl(exStyle, cls, text, style, x, y, width, height, parent,
        (style & WS_CHILD) ? (UINT)(uintptr_t)menu : 0, param, false);
  });
}

HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style,
    int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
  if (!className || IS_INTRESOURCE(className))
    return NULL;
  const std::wstring cls = FromA(className).toStdWString();
  const std::wstring text = FromA(windowName).toStdWString();
  return CreateWindowExW(exStyle, cls.c_str(), windowName ? text.c_str() : NULL, style,
      x, y, width, height, parent, menu, instance, param);
}

void InitCommonControls()
{
}

BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX * /* icc */)
{
  return TRUE;
}
