// ShimMessageBox.cpp -- MessageBox() over QMessageBox.

#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>

#include "ShimDialog.h"
#include "ShimWindow.h"

namespace NShim {

static CMessageBoxHook g_MessageBoxHook;

void SetMessageBoxHook(const CMessageBoxHook &hook)
{
  g_MessageBoxHook = hook;
}

struct CButtonDef
{
  int Id;
  QMessageBox::StandardButton Standard;
  const char *Text;   // for buttons without a standard counterpart
};

static int MessageBoxImpl(HWND owner, const QString &text, const QString &caption, UINT type)
{
  if (g_MessageBoxHook)
  {
    const int r = g_MessageBoxHook(owner, text, caption, type);
    if (r != 0)
      return r;
  }

  QWidget *parent = NULL;
  HWND o = FromHandle(owner);
  if (o && o->Widget)
    parent = o->Widget->window();

  QMessageBox box(parent);
  box.setTextFormat(Qt::PlainText);
  box.setText(text);
  box.setWindowTitle(caption.isNull() ? QStringLiteral("Error") : caption);
  switch (type & MB_ICONMASK)
  {
    case MB_ICONHAND: box.setIcon(QMessageBox::Critical); break;
    case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
    case MB_ICONEXCLAMATION: box.setIcon(QMessageBox::Warning); break;
    case MB_ICONASTERISK: box.setIcon(QMessageBox::Information); break;
    default: box.setIcon(QMessageBox::NoIcon); break;
  }

  std::vector<CButtonDef> defs;
  switch (type & MB_TYPEMASK)
  {
    case MB_OKCANCEL:
      defs = { { IDOK, QMessageBox::Ok, NULL }, { IDCANCEL, QMessageBox::Cancel, NULL } };
      break;
    case MB_ABORTRETRYIGNORE:
      defs = { { IDABORT, QMessageBox::Abort, NULL }, { IDRETRY, QMessageBox::Retry, NULL },
          { IDIGNORE, QMessageBox::Ignore, NULL } };
      break;
    case MB_YESNOCANCEL:
      defs = { { IDYES, QMessageBox::Yes, NULL }, { IDNO, QMessageBox::No, NULL },
          { IDCANCEL, QMessageBox::Cancel, NULL } };
      break;
    case MB_YESNO:
      defs = { { IDYES, QMessageBox::Yes, NULL }, { IDNO, QMessageBox::No, NULL } };
      break;
    case MB_RETRYCANCEL:
      defs = { { IDRETRY, QMessageBox::Retry, NULL }, { IDCANCEL, QMessageBox::Cancel, NULL } };
      break;
    case MB_CANCELTRYCONTINUE:
      defs = { { IDCANCEL, QMessageBox::Cancel, NULL }, { IDTRYAGAIN, QMessageBox::NoButton, "&Try Again" },
          { IDCONTINUE, QMessageBox::NoButton, "&Continue" } };
      break;
    default:
      defs = { { IDOK, QMessageBox::Ok, NULL } };
      break;
  }

  std::vector<QAbstractButton *> buttons;
  QAbstractButton *escape = NULL;
  for (const CButtonDef &d : defs)
  {
    QAbstractButton *b;
    if (d.Standard != QMessageBox::NoButton)
      b = box.addButton(d.Standard);
    else
      b = box.addButton(QString::fromLatin1(d.Text), QMessageBox::AcceptRole);
    b->setProperty("z7Id", d.Id);
    buttons.push_back(b);
    if (d.Id == IDCANCEL)
      escape = b;
  }
  // Windows: Esc means Cancel, or OK when it is the only button
  if (!escape && defs.size() == 1)
    escape = buttons[0];
  box.setEscapeButton(escape);
  const size_t def = ((type & MB_DEFMASK) >> 8);
  if (def < buttons.size())
    box.setDefaultButton(qobject_cast<QPushButton *>(buttons[def]));
  if (parent)
    box.setWindowModality(Qt::WindowModal);
  QPointer<QWidget> ownerWindow = parent;
  box.exec();
  ReactivateOwner(ownerWindow);
  QAbstractButton *clicked = box.clickedButton();
  if (!clicked)
    return escape ? escape->property("z7Id").toInt() : IDCANCEL;
  return clicked->property("z7Id").toInt();
}

}

using namespace NShim;

int MessageBoxW(HWND wnd, LPCWSTR text, LPCWSTR caption, UINT type)
{
  const QString t = FromW(text);
  const QString c = caption ? FromW(caption) : QString();
  return OnGuiThread<int>([=]() { return MessageBoxImpl(wnd, t, c, type); });
}

int MessageBoxA(HWND wnd, LPCSTR text, LPCSTR caption, UINT type)
{
  const QString t = FromA(text);
  const QString c = caption ? FromA(caption) : QString();
  return OnGuiThread<int>([=]() { return MessageBoxImpl(wnd, t, c, type); });
}
