// ShimPropSheet.cpp -- PropertySheet(): a modal dialog with a tab per page and
// the OK, Cancel, Apply and Help buttons. Pages are dialogs from their
// templates, created when they are first shown, as on Windows, and they get
// the PSN_* notifications that Windows sends.

#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <prsht.h>

#include "ShimDialog.h"
#include "ShimWindow.h"

#include "RcTables.h"

namespace NShim {

struct CSheetPage
{
  PROPSHEETPAGEW Info;
  std::wstring Title;
  const NRc::CDialog *Template;
  QWidget *Container;
  HWND Page;
  bool Changed;
};

struct CSheet
{
  std::vector<CSheetPage> Pages;
  NRc::CDialog Template;
  std::wstring Caption;
  QTabWidget *Tabs;
  HWND Wnd;
  int Current;
  bool Applied;
  int RestartFlags;
  bool HasHelp;
  bool Switching;

  CSheet(): Tabs(NULL), Wnd(NULL), Current(-1), Applied(false), RestartFlags(0),
      HasHelp(false), Switching(false) {}

  LRESULT Notify(HWND page, UINT code, LPARAM lParam)
  {
    if (!page || !FromHandle(page))
      return 0;
    PSHNOTIFY nm;
    memset(&nm, 0, sizeof(nm));
    nm.hdr.hwndFrom = Wnd;
    nm.hdr.idFrom = 0;
    nm.hdr.code = code;
    nm.lParam = lParam;
    // the page answers through DWLP_MSGRESULT; clear what is left from before
    SetWindowLongPtrW(page, DWLP_MSGRESULT, 0);
    return Send(page, WM_NOTIFY, 0, (LPARAM)&nm);
  }

  void CreatePage(int i)
  {
    CSheetPage &p = Pages[(size_t)i];
    if (p.Page)
      return;
    // pages are child windows without caption, whatever the template says
    NRc::CDialog t = *p.Template;
    t.Style = (t.Style & ~(DWORD)(WS_POPUP | WS_CAPTION | WS_DISABLED | WS_SYSMENU | WS_THICKFRAME)) | WS_CHILD;
    p.Page = CreateDialogFromTemplateIn(&t, Wnd, p.Container, p.Info.pfnDlgProc, (LPARAM)&p.Info);
    if (p.Page && p.Page->Widget)
    {
      p.Page->Widget->move(0, 0);
      p.Page->Widget->show();
    }
  }

  void Activate(int i)
  {
    Current = i;
    CreatePage(i);
    Notify(Pages[(size_t)i].Page, PSN_SETACTIVE, 0);
  }

  void OnTabChanged(int i)
  {
    if (Switching || i == Current)
      return;
    if (Current >= 0 && Notify(Pages[(size_t)Current].Page, PSN_KILLACTIVE, 0))
    {
      // the page does not let the user leave
      Switching = true;
      Tabs->setCurrentIndex(Current);
      Switching = false;
      return;
    }
    Activate(i);
  }

  void UpdateApply()
  {
    bool changed = false;
    for (const CSheetPage &p : Pages)
      changed = changed || p.Changed;
    HWND apply = GetDlgItem(Wnd, ID_APPLY_NOW);
    if (apply)
      EnableWindow(apply, changed);
  }

  // PSN_KILLACTIVE to the current page, then PSN_APPLY to all created pages.
  bool Apply(bool okButton)
  {
    if (Current >= 0 && Notify(Pages[(size_t)Current].Page, PSN_KILLACTIVE, 0))
      return false;
    for (size_t i = 0; i < Pages.size(); i++)
    {
      CSheetPage &p = Pages[i];
      if (!p.Page)
        continue;
      const LRESULT r = Notify(p.Page, PSN_APPLY, okButton ? TRUE : FALSE);
      if (r == PSNRET_INVALID || r == PSNRET_INVALID_NOCHANGEPAGE)
      {
        if (r == PSNRET_INVALID && (int)i != Current)
        {
          Switching = true;
          Tabs->setCurrentIndex((int)i);
          Switching = false;
          Activate((int)i);
        }
        return false;
      }
    }
    Applied = true;
    for (CSheetPage &p : Pages)
      p.Changed = false;
    UpdateApply();
    return true;
  }

  void Init();
  INT_PTR Command(UINT id);
};

static CSheet *SheetOf(HWND wnd)
{
  return (CSheet *)GetWindowLongPtrW(wnd, DWLP_USER);
}

void CSheet::Init()
{
  QWidget *w = Wnd->Widget;
  const CBaseUnits u = DialogBaseUnits(Wnd);

  // the page area fits the largest page
  QSize pageSize(0, 0);
  for (const CSheetPage &p : Pages)
    pageSize = pageSize.expandedTo(QSize(u.ToPixelsX(p.Template->W), u.ToPixelsY(p.Template->H)));

  Tabs = new QTabWidget(w);
  Tabs->setObjectName(QStringLiteral("z7PropertySheetTabs"));
  Tabs->setFocusPolicy(Qt::TabFocus);
  for (CSheetPage &p : Pages)
  {
    p.Container = new QWidget();
    p.Container->setMinimumSize(pageSize);
    Tabs->addTab(p.Container, FromW(p.Title.c_str()));
  }

  const int margin = u.ToPixelsX(7);
  const int marginY = u.ToPixelsY(7);
  const QSize tabsSize = Tabs->sizeHint().expandedTo(pageSize);
  Tabs->setGeometry(margin, marginY, tabsSize.width(), tabsSize.height());

  // OK, Cancel, Apply, Help: 50 x 14 units, 4 units apart, at the right
  const int bw = u.ToPixelsX(50);
  const int bh = u.ToPixelsY(14);
  const int gap = u.ToPixelsX(4);
  const int by = marginY + tabsSize.height() + marginY;
  struct CButton { UINT Id; const wchar_t *Text; DWORD Style; };
  std::vector<CButton> buttons = {
    { IDOK, L"OK", BS_DEFPUSHBUTTON },
    { IDCANCEL, L"Cancel", BS_PUSHBUTTON },
    { ID_APPLY_NOW, L"&Apply", BS_PUSHBUTTON }
  };
  if (HasHelp)
    buttons.push_back({ IDHELP, L"Help", BS_PUSHBUTTON });
  const int width = margin + tabsSize.width() + margin;
  int x = width - margin - (int)buttons.size() * bw - ((int)buttons.size() - 1) * gap;
  for (const CButton &b : buttons)
  {
    CreateWindowImpl(0, QStringLiteral("Button"), FromW(b.Text),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | b.Style, x, by, bw, bh, Wnd, b.Id, NULL, true);
    x += bw + gap;
  }
  SendMessageW(Wnd, DM_SETDEFID, IDOK, 0);
  w->setFixedSize(width, by + bh + marginY);
  UpdateApply();

  CEventProxy *proxy = Wnd->Proxy;
  QObject::connect(Tabs, &QTabWidget::currentChanged, proxy, [proxy, this](int i)
  {
    CWindowLock lock(proxy);
    if (lock)
      OnTabChanged(i);
  });

  int start = 0;
  Tabs->setCurrentIndex(start);
  Activate(start);
}

INT_PTR CSheet::Command(UINT id)
{
  switch (id)
  {
    case IDOK:
      if (Apply(true))
        EndDialog(Wnd, 1);
      return TRUE;
    case IDCANCEL:
      if (Current >= 0 && Notify(Pages[(size_t)Current].Page, PSN_QUERYCANCEL, 0))
        return TRUE;
      for (CSheetPage &p : Pages)
        if (p.Page)
          Notify(p.Page, PSN_RESET, 0);
      EndDialog(Wnd, 0);
      return TRUE;
    case ID_APPLY_NOW:
      Apply(false);
      return TRUE;
    case IDHELP:
      if (Current >= 0)
        Notify(Pages[(size_t)Current].Page, PSN_HELP, 0);
      return TRUE;
    default:
      return FALSE;
  }
}

static INT_PTR CALLBACK SheetProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_INITDIALOG)
  {
    CSheet *sheet = (CSheet *)lParam;
    SetWindowLongPtrW(wnd, DWLP_USER, (LONG_PTR)sheet);
    sheet->Wnd = wnd;
    sheet->Init();
    return FALSE;   // Init() set the focus
  }
  CSheet *sheet = SheetOf(wnd);
  if (!sheet)
    return FALSE;
  switch (message)
  {
    case WM_COMMAND:
      if (HIWORD(wParam) == BN_CLICKED)
        return sheet->Command(LOWORD(wParam));
      return FALSE;
    case WM_HELP:
      sheet->Command(IDHELP);
      return TRUE;
    case PSM_CHANGED:
    case PSM_UNCHANGED:
      for (CSheetPage &p : sheet->Pages)
        if (p.Page == (HWND)wParam)
          p.Changed = (message == PSM_CHANGED);
      sheet->UpdateApply();
      return TRUE;
    case PSM_SETCURSEL:
    {
      int i = (int)wParam;
      for (size_t k = 0; k < sheet->Pages.size(); k++)
        if (lParam && sheet->Pages[k].Page == (HWND)lParam)
          i = (int)k;
      if (i >= 0 && i < (int)sheet->Pages.size())
        sheet->Tabs->setCurrentIndex(i);
      SetWindowLongPtrW(wnd, DWLP_MSGRESULT, TRUE);
      return TRUE;
    }
    case PSM_GETCURRENTPAGEHWND:
      SetWindowLongPtrW(wnd, DWLP_MSGRESULT,
          sheet->Current >= 0 ? (LONG_PTR)sheet->Pages[(size_t)sheet->Current].Page : 0);
      return TRUE;
    case PSM_APPLY:
      SetWindowLongPtrW(wnd, DWLP_MSGRESULT, sheet->Apply(false) ? TRUE : FALSE);
      return TRUE;
    case PSM_PRESSBUTTON:
    {
      const UINT ids[] = { 0, IDOK, 0, 0, 0, IDCANCEL, ID_APPLY_NOW, 0, IDHELP };
      if (wParam < sizeof(ids) / sizeof(ids[0]) && ids[wParam])
        PostMessageW(wnd, WM_COMMAND, MAKEWPARAM(ids[wParam], BN_CLICKED), 0);
      return TRUE;
    }
    case PSM_RESTARTWINDOWS:
      sheet->RestartFlags |= ID_PSRESTARTWINDOWS;
      return TRUE;
    case PSM_REBOOTSYSTEM:
      sheet->RestartFlags |= ID_PSREBOOTSYSTEM;
      return TRUE;
    case PSM_CANCELTOCLOSE:
    {
      HWND cancel = GetDlgItem(wnd, IDCANCEL);
      if (cancel)
        SetWindowTextW(cancel, L"Close");
      return TRUE;
    }
    default:
      return FALSE;
  }
}

static INT_PTR PropertySheetImpl(const PROPSHEETHEADERW *h)
{
  if (!h || !(h->dwFlags & PSH_PROPSHEETPAGE) || h->nPages == 0)
    return -1;
  CSheet sheet;
  for (UINT i = 0; i < h->nPages; i++)
  {
    const PROPSHEETPAGEW &info = h->ppsp[i];
    if (!IS_INTRESOURCE(info.pszTemplate))
      return -1;
    CSheetPage p;
    p.Info = info;
    p.Template = NRc::FindDialog((UInt32)(uintptr_t)info.pszTemplate);
    if (!p.Template)
      return -1;
    if ((info.dwFlags & PSP_USETITLE) && info.pszTitle && !IS_INTRESOURCE(info.pszTitle))
      p.Title = info.pszTitle;
    else if (p.Template->Caption)
      p.Title = p.Template->Caption;
    p.Container = NULL;
    p.Page = NULL;
    p.Changed = false;
    if (info.dwFlags & PSP_HASHELP)
      sheet.HasHelp = true;
    sheet.Pages.push_back(p);
  }
  if (h->pszCaption && !IS_INTRESOURCE(h->pszCaption))
    sheet.Caption = h->pszCaption;
  if (h->dwFlags & PSH_PROPTITLE)
    sheet.Caption = L"Properties for " + sheet.Caption;

  memset(&sheet.Template, 0, sizeof(sheet.Template));
  sheet.Template.Id = 0;
  sheet.Template.Name = "PropertySheet";
  sheet.Template.Style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT;
  sheet.Template.Caption = sheet.Caption.c_str();
  sheet.Template.W = 100;
  sheet.Template.H = 100;

  HWND wnd = CreateDialogFromTemplate(&sheet.Template, h->hwndParent, SheetProc, (LPARAM)&sheet);
  if (!wnd)
    return -1;
  const INT_PTR r = RunModalDialog(wnd);
  if (r < 0)
    return -1;
  if (sheet.RestartFlags)
    return sheet.RestartFlags;
  return sheet.Applied ? 1 : 0;
}

}

using namespace NShim;

INT_PTR PropertySheetW(const PROPSHEETHEADERW *header)
{
  return OnGuiThread<INT_PTR>([=]() { return PropertySheetImpl(header); });
}

INT_PTR PropertySheetA(const PROPSHEETHEADERA * /* header */)
{
  // the narrow form is not used: g_IsNT is true
  return -1;
}
