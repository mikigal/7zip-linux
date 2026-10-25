// Theme.cpp

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QRegion>
#include <QStyleFactory>
#include <QStyleOption>
#include <QWidget>

#include "Theme.h"

namespace NTheme {

// Colors of the dark theme.
static const QRgb kWindow = 0x202020;
static const QRgb kBase = 0x191919;
static const QRgb kText = 0xe6e6e6;
static const QRgb kButton = 0x2d2d2d;
static const QRgb kDisabledText = 0x7a7a7a;
static const QRgb kHighlight = 0x0078d4;
static const QRgb kFrame = 0x5c5c5c;          // edit, combo box, list, check box
static const QRgb kFrameHover = 0x7a7a7a;
static const QRgb kFrameDisabled = 0x3a3a3a;
static const QRgb kGroupFrame = 0x444444;

QPalette DarkPalette()
{
  const QColor text(kText);
  const QColor disabledText(kDisabledText);

  QPalette p;
  p.setColor(QPalette::Window, QColor(kWindow));
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, QColor(kBase));
  p.setColor(QPalette::AlternateBase, QColor(0x24, 0x24, 0x24));
  p.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2b, 0x2b));
  p.setColor(QPalette::ToolTipText, text);
  p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x8a, 0x8a));
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, QColor(kButton));
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::BrightText, QColor(0xff, 0x6b, 0x6b));
  p.setColor(QPalette::Light, QColor(0x3c, 0x3c, 0x3c));
  p.setColor(QPalette::Midlight, QColor(0x33, 0x33, 0x33));
  p.setColor(QPalette::Mid, QColor(0x29, 0x29, 0x29));
  p.setColor(QPalette::Dark, QColor(0x14, 0x14, 0x14));
  p.setColor(QPalette::Shadow, QColor(0x00, 0x00, 0x00));
  p.setColor(QPalette::Highlight, QColor(kHighlight));
  p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
  p.setColor(QPalette::Link, QColor(0x4e, 0xa1, 0xff));
  p.setColor(QPalette::LinkVisited, QColor(0xb4, 0x8e, 0xff));

  p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
  p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
  p.setColor(QPalette::Disabled, QPalette::Base, QColor(0x1d, 0x1d, 0x1d));
  p.setColor(QPalette::Disabled, QPalette::Button, QColor(0x26, 0x26, 0x26));
  p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3a, 0x3a, 0x3a));
  p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);
  // selection in a list without focus, like Windows "show selection always"
  p.setColor(QPalette::Inactive, QPalette::Highlight, QColor(0x37, 0x4a, 0x5e));
  p.setColor(QPalette::Inactive, QPalette::HighlightedText, text);
  return p;
}

QString StyleSheet()
{
  // Kept empty: a style sheet rule takes the drawing of a widget away from
  // the style. Controls that need other colors get a palette instead.
  return QString();
}

/* Fusion derives its frame colors by darkening the window color, which
   makes the frames of edit controls, combo boxes, lists and check boxes
   nearly invisible on a dark window. This style draws those frames lighter,
   gives tab panes the window color, and shows keyboard mnemonics only while
   Alt is held, as Windows does. */
class CStyle: public QProxyStyle
{
public:
  bool ShowMnemonics;

  CStyle(): QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))), ShowMnemonics(false) {}

  static QColor FrameColor(const QStyleOption *option, bool focusable)
  {
    if (!(option->state & State_Enabled))
      return QColor(kFrameDisabled);
    if (focusable && (option->state & State_HasFocus))
      return option->palette.color(QPalette::Highlight);
    if (option->state & State_MouseOver)
      return QColor(kFrameHover);
    return QColor(kFrame);
  }

  static void DrawOutline(QPainter *p, const QRect &rect, const QColor &color, qreal radius)
  {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(color, 1));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p->restore();
  }

  // Fusion paints tabs and their pane in a color derived from Button; with
  // the window color there, the pane is a shade lighter than the dialog.
  static QPalette TabPalette(const QPalette &pal)
  {
    QPalette p = pal;
    p.setColor(QPalette::Button, pal.color(QPalette::Window));
    return p;
  }

  void drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *p, const QWidget *w) const override
  {
    switch (pe)
    {
      case PE_FrameLineEdit:
        DrawOutline(p, option->rect, FrameColor(option, true), 2);
        return;
      case PE_Frame:
      {
        // the frame of lists and multi-line edit controls (StyledPanel)
        const QStyleOptionFrame *f = qstyleoption_cast<const QStyleOptionFrame *>(option);
        if (f && f->lineWidth > 0)
        {
          DrawOutline(p, option->rect, FrameColor(option, false), 0);
          return;
        }
        break;
      }
      case PE_FrameGroupBox:
        DrawOutline(p, option->rect, QColor(kGroupFrame), 3);
        return;
      case PE_IndicatorCheckBox:
      case PE_IndicatorRadioButton:
      {
        QProxyStyle::drawPrimitive(pe, option, p, w);
        QRect r = option->rect;
        if (pe == PE_IndicatorRadioButton)
        {
          p->save();
          p->setRenderHint(QPainter::Antialiasing, true);
          p->setPen(QPen(FrameColor(option, false), 1));
          p->setBrush(Qt::NoBrush);
          p->drawEllipse(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
          p->restore();
        }
        else
          DrawOutline(p, r, FrameColor(option, false), 1);
        return;
      }
      case PE_FrameTabWidget:
      {
        QStyleOption o(*option);
        o.palette = TabPalette(option->palette);
        QProxyStyle::drawPrimitive(pe, &o, p, w);
        return;
      }
      default:
        break;
    }
    QProxyStyle::drawPrimitive(pe, option, p, w);
  }

  void drawControl(ControlElement ce, const QStyleOption *option, QPainter *p, const QWidget *w) const override
  {
    switch (ce)
    {
      case CE_TabBarTabShape:
      {
        if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option))
        {
          QStyleOptionTab o(*tab);
          o.palette = TabPalette(tab->palette);
          QProxyStyle::drawControl(ce, &o, p, w);
          return;
        }
        break;
      }
      case CE_ProgressBarGroove:
        p->fillRect(option->rect.adjusted(1, 1, -1, -1), option->palette.color(QPalette::Base));
        DrawOutline(p, option->rect, FrameColor(option, false), 2);
        return;
      default:
        break;
    }
    QProxyStyle::drawControl(ce, option, p, w);
  }

  // A group box as on Windows: the title interrupts the top line of the
  // frame, which runs through the middle of the text (Fusion puts the title
  // above the frame, so the frame would start lower than in the templates).
  static bool IsPlainGroupBox(const QStyleOptionComplex *option)
  {
    const QStyleOptionGroupBox *g = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
    return g && !(g->subControls & SC_GroupBoxCheckBox);
  }

  QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *option, SubControl sc, const QWidget *w) const override
  {
    if (cc == CC_GroupBox && IsPlainGroupBox(option))
    {
      const QStyleOptionGroupBox *g = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
      const QFontMetrics &fm = option->fontMetrics;
      const int th = fm.height();
      const QRect r = option->rect;
      switch (sc)
      {
        case SC_GroupBoxFrame:
          return r.adjusted(0, th / 2, 0, 0);
        case SC_GroupBoxLabel:
        {
          if (g->text.isEmpty())
            return QRect();
          const int indent = fm.averageCharWidth();
          return QRect(r.left() + indent, r.top(), fm.horizontalAdvance(g->text) + 4, th);
        }
        case SC_GroupBoxContents:
          return r.adjusted(1, th, -1, -1);
        default:
          break;
      }
    }
    return QProxyStyle::subControlRect(cc, option, sc, w);
  }

  void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const override
  {
    if (cc == CC_GroupBox && IsPlainGroupBox(option))
    {
      const QStyleOptionGroupBox *g = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
      const QRect frame = subControlRect(cc, option, SC_GroupBoxFrame, w);
      const QRect label = subControlRect(cc, option, SC_GroupBoxLabel, w);
      p->save();
      QRegion clip(option->rect);
      if (label.isValid())
        clip -= label;
      p->setClipRegion(clip);
      DrawOutline(p, frame, QColor(kGroupFrame), 3);
      p->restore();
      if (label.isValid())
        drawItemText(p, label, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextHideMnemonic, option->palette,
            (option->state & State_Enabled) != 0, g->text, QPalette::WindowText);
      return;
    }
    QProxyStyle::drawComplexControl(cc, option, p, w);
    if (cc == CC_ComboBox)
    {
      const QStyleOptionComboBox *c = qstyleoption_cast<const QStyleOptionComboBox *>(option);
      if (c && c->frame)
        DrawOutline(p, option->rect, FrameColor(option, true), 2);
    }
  }

  int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *w, QStyleHintReturn *ret) const override
  {
    if (hint == SH_UnderlineShortcut)
      return ShowMnemonics ? 1 : 0;
    return QProxyStyle::styleHint(hint, option, w, ret);
  }
};

// Shows the mnemonics while Alt is held.
class CAltWatcher: public QObject
{
  CStyle *_style;
public:
  CAltWatcher(CStyle *style, QObject *parent): QObject(parent), _style(style) {}

  void SetShow(bool show)
  {
    if (_style->ShowMnemonics == show)
      return;
    _style->ShowMnemonics = show;
    for (QWidget *w : QApplication::topLevelWidgets())
      if (w->isVisible())
        w->update();
    for (QWidget *w : QApplication::allWidgets())
      if (w->isVisible())
        w->update();
  }

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    switch (event->type())
    {
      case QEvent::KeyPress:
      case QEvent::KeyRelease:
      {
        const QKeyEvent *e = static_cast<QKeyEvent *>(event);
        if (e->key() == Qt::Key_Alt)
          SetShow(event->type() == QEvent::KeyPress);
        break;
      }
      case QEvent::ApplicationDeactivate:
        SetShow(false);
        break;
      default:
        break;
    }
    return QObject::eventFilter(watched, event);
  }
};

QIcon ToolbarIcon(const char *name, bool large)
{
  return QIcon(QStringLiteral(":/z7/toolbar/%1%2.svg").arg(QLatin1String(name), large ? QString() : QStringLiteral("2")));
}

QIcon AppIcon()
{
  return QIcon(QStringLiteral(":/z7/app/7-Zip.svg"));
}

// The icons of files and folders for the dark background: the dark variant
// of the icon theme, when it has one (breeze-dark for breeze). KDE's
// platform theme recolors Breeze to the palette itself; without it (other
// desktops, the portable releases with their own Qt) Breeze draws dark
// icons on the dark theme.
static void UseDarkIconTheme()
{
  const QString name = QIcon::themeName();
  if (name.isEmpty() || name.endsWith(QLatin1String("-dark"), Qt::CaseInsensitive))
    return;
  for (const char *suffix : { "-dark", "-Dark" })
  {
    const QString dark = name + QLatin1String(suffix);
    for (const QString &dir : QIcon::themeSearchPaths())
      if (QFileInfo::exists(dir + QLatin1Char('/') + dark + QLatin1String("/index.theme")))
      {
        QIcon::setThemeName(dark);
        return;
      }
  }
}

void Apply(QApplication &app)
{
  CStyle *style = new CStyle();
  app.setStyle(style);   // the application owns the style
  app.setPalette(DarkPalette());
  app.setStyleSheet(StyleSheet());
  app.setWindowIcon(AppIcon());
  app.installEventFilter(new CAltWatcher(style, &app));
  UseDarkIconTheme();
}

}
