// test_shim.cpp
// Unit tests for linux/shim: the user32 subset over Qt, driven through the
// unchanged upstream wrappers (CModalDialog, CComboBox, CListView,
// CPropertyPage, ...) and the dialog templates of 7zFM.

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <memory>
#include <thread>

#include <windows.h>
#include <CommCtrl.h>

#include "../../../CPP/Windows/Control/ComboBox.h"
#include "../../../CPP/Windows/Control/Dialog.h"
#include "../../../CPP/Windows/Control/Edit.h"
#include "../../../CPP/Windows/Control/ListView.h"
#include "../../../CPP/Windows/Control/PropertyPage.h"

#include "../../../CPP/7zip/UI/GUI/CompressDialogRes.h"
#include "../../../CPP/7zip/UI/GUI/ExtractDialogRes.h"
#include "../../../CPP/7zip/UI/FileManager/ComboDialogRes.h"
#include "../../../CPP/7zip/UI/FileManager/EditPageRes.h"
#include "../../../CPP/7zip/UI/FileManager/FoldersPageRes.h"
#include "../../../CPP/7zip/UI/FileManager/ListViewDialogRes.h"
#include "../../../CPP/7zip/UI/FileManager/MemDialogRes.h"
#include "../../../CPP/7zip/UI/FileManager/PasswordDialogRes.h"

#include "RcTables.h"
#include "ShimDialog.h"

using namespace NWindows;
using namespace NControl;

static QWidget *Item(HWND dlg, int id)
{
  return NShim::WidgetOf(GetDlgItem(dlg, id));
}

template <class T>
static T *ItemAs(HWND dlg, int id)
{
  return qobject_cast<T *>(Item(dlg, id));
}

static QString ToQ(const UString &s)
{
  return QString::fromWCharArray(s.Ptr(), (qsizetype)s.Len());
}

// Runs f inside the loop of the next modal dialog. If the dialog is still
// in its loop two seconds after f returned (a failed QVERIFY returns early),
// it is ended with -2, so that the test does not hang.
struct CModalScript
{
  std::shared_ptr<QTimer> Guard;

  CModalScript(const std::function<void(HWND)> &f): Guard(std::make_shared<QTimer>())
  {
    Guard->setSingleShot(true);
    std::weak_ptr<QTimer> guard = Guard;
    NShim::SetModalHook([f, guard](HWND dlg)
    {
      NShim::SetModalHook(NShim::CModalHook());
      f(dlg);
      if (std::shared_ptr<QTimer> g = guard.lock())
      {
        QObject::connect(g.get(), &QTimer::timeout, [dlg]()
        {
          if (NShim::IsInModalLoop(dlg))
            EndDialog(dlg, -2);
        });
        g->start(2000);
      }
    });
  }
  ~CModalScript() { NShim::SetModalHook(NShim::CModalHook()); }
};

// Joins a thread when the test returns, also after a failed check.
struct CJoiner
{
  std::thread &Thread;
  CJoiner(std::thread &t): Thread(t) {}
  ~CJoiner() { if (Thread.joinable()) Thread.join(); }
};

// ---------- Test dialogs ----------

class CPasswordTestDialog: public CModalDialog
{
public:
  UString Password;
  int ShowClicks = 0;
  bool InitCalled = false;

  bool OnInit() override
  {
    InitCalled = true;
    SetItemText(IDE_PASSWORD_PASSWORD, L"start");
    return CModalDialog::OnInit();
  }
  bool OnButtonClicked(unsigned id, HWND wnd) override
  {
    if (id == IDX_PASSWORD_SHOW)
    {
      ShowClicks++;
      CEdit edit;
      edit.Attach(GetItem(IDE_PASSWORD_PASSWORD));
      edit.SetPasswordChar(IsButtonCheckedBool(IDX_PASSWORD_SHOW) ? 0 : '*');
      return true;
    }
    return CModalDialog::OnButtonClicked(id, wnd);
  }
  void OnOK() override
  {
    GetItemText(IDE_PASSWORD_PASSWORD, Password);
    CModalDialog::OnOK();
  }
};

class CRecordingDialog: public CModalDialog
{
public:
  std::vector<UInt32> Messages;     // WM_APP + n posted messages, in arrival order
  std::vector<unsigned> Commands;   // MAKELONG(id, code) of WM_COMMAND
  std::vector<UINT> Notifications;  // WM_NOTIFY codes
  std::vector<NMLISTVIEW> ItemChanges;
  std::vector<NMITEMACTIVATE> Clicks;
  std::vector<WORD> Keys;
  int Timers = 0;
  int Scrolls = 0;
  int SizeCalls = 0;
  int OkCalls = 0;
  int LastWidth = 0;
  std::function<bool()> OnInitFunc;
  std::function<void(UINT)> OnTimerFunc;
  std::function<void(UInt32)> OnAppFunc;

  bool OnInit() override
  {
    if (OnInitFunc)
      return OnInitFunc();
    return CModalDialog::OnInit();
  }
  bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam) override
  {
    if (message >= WM_APP && message < WM_APP + 100)
    {
      Messages.push_back(message - WM_APP);
      if (OnAppFunc)
        OnAppFunc(message - WM_APP);
      return true;
    }
    if (message == WM_VSCROLL)
      Scrolls++;
    return CModalDialog::OnMessage(message, wParam, lParam);
  }
  bool OnCommand(unsigned code, unsigned id, LPARAM lParam) override
  {
    Commands.push_back(MAKELONG(id, code));
    return CModalDialog::OnCommand(code, id, lParam);
  }
  bool OnNotify(UINT /* id */, LPNMHDR header) override
  {
    Notifications.push_back(header->code);
    if (header->code == LVN_ITEMCHANGED)
      ItemChanges.push_back(*(const NMLISTVIEW *)header);
    if (header->code == NM_CLICK || header->code == NM_DBLCLK || header->code == LVN_ITEMACTIVATE)
      Clicks.push_back(*(const NMITEMACTIVATE *)header);
    if (header->code == LVN_KEYDOWN)
      Keys.push_back(((const NMLVKEYDOWN *)header)->wVKey);
    return false;
  }
  bool OnTimer(WPARAM id, LPARAM) override
  {
    Timers++;
    if (OnTimerFunc)
      OnTimerFunc((UINT)id);
    return true;
  }
  bool OnSize(WPARAM, int xSize, int) override
  {
    SizeCalls++;
    LastWidth = xSize;
    return false;
  }
  void OnOK() override
  {
    OkCalls++;
    CModalDialog::OnOK();
  }
  bool HasCommand(unsigned id, unsigned code) const
  {
    for (unsigned c : Commands)
      if (c == (unsigned)MAKELONG(id, code))
        return true;
    return false;
  }
};

class CTestPage: public CPropertyPage
{
public:
  int Inits = 0;
  int Applies = 0;
  int SetActives = 0;
  int KillActives = 0;
  int Resets = 0;
  bool RefuseKill = false;

  bool OnInit() override { Inits++; return CPropertyPage::OnInit(); }
  LONG OnApply() override { Applies++; return PSNRET_NOERROR; }
  LONG OnSetActive() override { SetActives++; return 0; }
  bool OnKillActive() override { KillActives++; return RefuseKill; }
  void OnReset() override { Resets++; }
};

static int CALLBACK CompareDescending(LPARAM a, LPARAM b, LPARAM)
{
  return a < b ? 1 : a > b ? -1 : 0;
}

static INT_PTR CALLBACK TrivialProc(HWND, UINT message, WPARAM, LPARAM)
{
  return message == WM_INITDIALOG ? TRUE : FALSE;
}

// ---------- Tests ----------

class CShimTest: public QObject
{
  Q_OBJECT

private slots:
  void cleanup()
  {
    NShim::SetModalHook(NShim::CModalHook());
    NShim::SetMessageBoxHook(NShim::CMessageBoxHook());
  }

  void baseUnits()
  {
    const NShim::CBaseUnits u = NShim::ComputeBaseUnits(NShim::DialogFont());
    QVERIFY(u.X >= 4 && u.X <= 20);
    QVERIFY(u.Y >= 8 && u.Y <= 40);
    QCOMPARE(u.ToPixelsX(4), u.X);
    QCOMPARE(u.ToPixelsY(8), u.Y);
    const LONG v = GetDialogBaseUnits();
    QCOMPARE((int)LOWORD(v), u.X);
    QCOMPARE((int)HIWORD(v), u.Y);
  }

  void everyTemplateCreates()
  {
    const NRc::CResources &r = NRc::g_Resources;
    for (unsigned i = 0; i < r.NumDialogs; i++)
    {
      const NRc::CDialog &t = r.Dialogs[i];
      const HWND dlg = NShim::CreateDialogFromTemplate(&t, NULL, TrivialProc, 0);
      QVERIFY2(dlg, t.Name);
      QWidget *w = NShim::WidgetOf(dlg);
      QVERIFY(w);
      const NShim::CBaseUnits u = NShim::DialogBaseUnits(dlg);
      QCOMPARE(w->width(), u.ToPixelsX(t.W));
      for (unsigned k = 0; k < t.NumControls; k++)
      {
        const NRc::CControl &c = t.Controls[k];
        if ((WORD)c.Id == 0xFFFF)
          continue;
        const HWND ctl = GetDlgItem(dlg, (int)c.Id);
        QVERIFY2(ctl, qPrintable(QStringLiteral("%1 control %2").arg(t.Name).arg(c.Id)));
        QCOMPARE(GetDlgCtrlID(ctl), (int)c.Id);
      }
      QVERIFY(DestroyWindow(dlg));
      QVERIFY(!IsWindow(dlg));
      QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
    }
  }

  void windowsOnlyOptions()
  {
    // the SFX option stays in the layout but cannot be enabled; the NTFS
    // security option of the extract dialog cannot be shown
    const HWND compress = NShim::CreateDialogFromTemplate(NRc::FindDialog(IDD_COMPRESS), NULL, TrivialProc, 0);
    QVERIFY(compress);
    const HWND sfx = GetDlgItem(compress, IDX_COMPRESS_SFX);
    QVERIFY(sfx);
    EnableWindow(sfx, TRUE);
    ShowWindow(sfx, SW_SHOW);
    QVERIFY(!IsWindowEnabled(sfx));
    QVERIFY(!(GetWindowLongW(sfx, GWL_STYLE) & WS_TABSTOP));
    QVERIFY(DestroyWindow(compress));

    const HWND extract = NShim::CreateDialogFromTemplate(NRc::FindDialog(IDD_EXTRACT), NULL, TrivialProc, 0);
    QVERIFY(extract);
    ShowWindow(extract, SW_SHOW);
    const HWND secur = GetDlgItem(extract, IDX_EXTRACT_NT_SECUR);
    QVERIFY(secur);
    ShowWindow(secur, SW_SHOW);
    EnableWindow(secur, TRUE);
    QVERIFY(!IsWindowVisible(secur));
    QVERIFY(!IsWindowEnabled(secur));
    QVERIFY(IsWindowVisible(GetDlgItem(extract, IDX_EXTRACT_ELIM_DUP)));
    QVERIFY(DestroyWindow(extract));
    QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
  }

  void iconResources()
  {
    const HICON logo = LoadIconW(NULL, MAKEINTRESOURCEW(100));
    QVERIFY(logo);
    // shared: the same handle again, and DestroyIcon() does not free it
    QCOMPARE(LoadIconW(NULL, MAKEINTRESOURCEW(100)), logo);
    QVERIFY(DestroyIcon(logo));
    QVERIFY(!NShim::IconFromHandleForTest(logo).availableSizes().isEmpty());
    QVERIFY(LoadIconW(NULL, MAKEINTRESOURCEW(1)));
    QVERIFY(!LoadIconW(NULL, MAKEINTRESOURCEW(4321)));
    // the static control of the About dialog shows the logo
    const HWND d = NShim::CreateDialogFromTemplate(NRc::FindDialog(2900), NULL, TrivialProc, 0);
    QVERIFY(d);
    bool found = false;
    for (QLabel *l : NShim::WidgetOf(d)->findChildren<QLabel *>())
      if (!l->pixmap().isNull())
        found = true;
    QVERIFY(found);
    DestroyWindow(d);
    QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
  }

  void labelBuddies()
  {
    // the mnemonic of a label goes to the next control that can take the
    // focus, even a radio button without a tab stop yet
    const HWND d = NShim::CreateDialogFromTemplate(NRc::FindDialog(IDD_FOLDERS), NULL, TrivialProc, 0);
    QVERIFY(d);
    QLabel *label = ItemAs<QLabel>(d, IDT_FOLDERS_WORKING_FOLDER);
    QVERIFY(label);
    QCOMPARE(label->buddy(), Item(d, IDR_FOLDERS_WORK_SYSTEM));
    QCOMPARE(label->text(), QStringLiteral("&Working folder"));
    DestroyWindow(d);
    QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
  }

  void modalDialogWithKeyboard()
  {
    CPasswordTestDialog dlg;
    CModalScript script([](HWND d)
    {
      QLineEdit *edit = ItemAs<QLineEdit>(d, IDE_PASSWORD_PASSWORD);
      QVERIFY(edit);
      QCOMPARE(edit->text(), QStringLiteral("start"));
      QCOMPARE(edit->echoMode(), QLineEdit::Password);
      // the label gives its mnemonic to the edit control after it
      QLabel *label = ItemAs<QLabel>(d, IDT_PASSWORD_ENTER);
      QVERIFY(label);
      QCOMPARE(label->buddy(), (QWidget *)edit);
      edit->setFocus();
      edit->selectAll();
      QTest::keyClicks(edit, QStringLiteral("secret"));
      QCheckBox *show = ItemAs<QCheckBox>(d, IDX_PASSWORD_SHOW);
      QVERIFY(show);
      QTest::mouseClick(show, Qt::LeftButton);
      QCOMPARE(edit->echoMode(), QLineEdit::Normal);
      QTest::keyClick(edit, Qt::Key_Return);
    });
    const INT_PTR r = dlg.Create(IDD_PASSWORD, NULL);
    QCOMPARE(r, (INT_PTR)IDOK);
    QVERIFY(dlg.InitCalled);
    QCOMPARE(dlg.ShowClicks, 1);
    QCOMPARE(ToQ(dlg.Password), QStringLiteral("secret"));
  }

  void escapeAndCloseCancel()
  {
    {
      CPasswordTestDialog dlg;
      CModalScript script([](HWND d)
      {
        QTest::keyClick(Item(d, IDE_PASSWORD_PASSWORD), Qt::Key_Escape);
      });
      QCOMPARE(dlg.Create(IDD_PASSWORD, NULL), (INT_PTR)IDCANCEL);
    }
    {
      // the window close button posts IDCANCEL
      CPasswordTestDialog dlg;
      CModalScript script([](HWND d) { NShim::WidgetOf(d)->close(); });
      QCOMPARE(dlg.Create(IDD_PASSWORD, NULL), (INT_PTR)IDCANCEL);
    }
  }

  void enterWithDisabledDefaultButton()
  {
    CRecordingDialog dlg;
    CModalScript script([&dlg](HWND d)
    {
      EnableWindow(GetDlgItem(d, IDOK), FALSE);
      QTest::keyClick(Item(d, IDE_PASSWORD_PASSWORD), Qt::Key_Return);
      QCOMPARE(dlg.OkCalls, 0);
      EnableWindow(GetDlgItem(d, IDOK), TRUE);
      // Enter on a focused push button presses that button
      QWidget *cancel = Item(d, IDCANCEL);
      cancel->setFocus();
      QTest::keyClick(cancel, Qt::Key_Return);
    });
    QCOMPARE(dlg.Create(IDD_PASSWORD, NULL), (INT_PTR)IDCANCEL);
    QCOMPARE(dlg.OkCalls, 0);
  }

  void editAndComboBox()
  {
    CRecordingDialog dlg;
    CModalScript script([&dlg](HWND d)
    {
      CComboBox combo;
      combo.Attach(GetDlgItem(d, IDC_COMBO));
      QCOMPARE(combo.GetCount(), 0);
      QCOMPARE(combo.AddString(L"alpha"), (LRESULT)0);
      QCOMPARE(combo.AddString(L"beta"), (LRESULT)1);
      QCOMPARE(combo.AddString_SetItemData(L"gamma", 1234), (LRESULT)2);
      QCOMPARE(combo.GetItemData(2), (LRESULT)1234);
      QCOMPARE(combo.GetCurSel(), CB_ERR);
      combo.SetCurSel(1);
      QCOMPARE(combo.GetCurSel(), 1);
      UString s;
      combo.GetLBText(2, s);
      QCOMPARE(ToQ(s), QStringLiteral("gamma"));
      QCOMPARE((int)combo.SendMsgW(CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"BETA"), 1);
      QCOMPARE((int)combo.SendMsgW(CB_FINDSTRING, (WPARAM)-1, (LPARAM)L"ga"), 2);
      // narrow messages carry UTF-8
      QCOMPARE((int)combo.SendMsg(CB_ADDSTRING, 0, (LPARAM)"\xC5\xBC\x6F\xC5\x82\x77"), 3);
      combo.GetLBText(3, s);
      QCOMPARE(ToQ(s), QString::fromUtf8("\xC5\xBC\x6F\xC5\x82\x77"));
      AString a;
      combo.GetLBText(3, a);
      QCOMPARE(QByteArray(a.Ptr()), QByteArray("\xC5\xBC\x6F\xC5\x82\x77"));

      // programmatic changes do not notify; the user does
      const size_t before = dlg.Commands.size();
      QComboBox *qc = ItemAs<QComboBox>(d, IDC_COMBO);
      QVERIFY(qc && qc->isEditable());
      QCOMPARE(dlg.Commands.size(), before);
      qc->setFocus();
      QTest::keyClick(qc, Qt::Key_Down);
      QCOMPARE(combo.GetCurSel(), 2);
      QVERIFY(dlg.HasCommand(IDC_COMBO, CBN_SELCHANGE));
      QTest::keyClicks(qc->lineEdit(), QStringLiteral("x"));
      QVERIFY(dlg.HasCommand(IDC_COMBO, CBN_EDITCHANGE));
      // typing clears the selection of the list, as on Windows: dialogs
      // read the typed text when there is no selection (ExtractDialog.cpp)
      QCOMPARE(combo.GetCurSel(), CB_ERR);
      QTest::keyClick(qc, Qt::Key_Up);
      QCOMPARE(combo.GetCurSel(), 1);

      // window text of the drop-down combo box is the edit text
      CWindow w(GetDlgItem(d, IDC_COMBO));
      w.SetText(L"typed");
      UString t;
      w.GetText(t);
      QCOMPARE(ToQ(t), QStringLiteral("typed"));
      combo.ResetContent();
      QCOMPARE(combo.GetCount(), 0);
      EndDialog(d, IDOK);
    });
    QCOMPARE(dlg.Create(IDD_COMBO, NULL), (INT_PTR)IDOK);
  }

  void editChangeAndText()
  {
    CRecordingDialog dlg;
    CModalScript script([&dlg](HWND d)
    {
      CDialog cd(d);
      dlg.Commands.clear();
      // Windows sends EN_CHANGE for WM_SETTEXT to a single-line edit control
      cd.SetItemText(IDE_PASSWORD_PASSWORD, L"abc");
      QVERIFY(dlg.HasCommand(IDE_PASSWORD_PASSWORD, EN_CHANGE));
      const HWND edit = GetDlgItem(d, IDE_PASSWORD_PASSWORD);
      QVERIFY(SetWindowTextA(edit, "Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87"));
      QCOMPARE(GetWindowTextLengthW(edit), 6);
      QCOMPARE(GetWindowTextLengthA(edit), 10);
      char buf[64];
      QCOMPARE(GetWindowTextA(edit, buf, sizeof(buf)), 10);
      QCOMPARE(QByteArray(buf), QByteArray("Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87"));
      // a short buffer does not cut a UTF-8 sequence
      QCOMPARE(GetWindowTextA(edit, buf, 4), 2);
      QCOMPARE(QByteArray(buf), QByteArray("Za"));
      UINT v = 0;
      cd.SetItemUInt(IDE_PASSWORD_PASSWORD, 4096);
      QVERIFY(cd.GetItemUInt(IDE_PASSWORD_PASSWORD, v));
      QCOMPARE(v, 4096u);
      cd.SetItemText(IDE_PASSWORD_PASSWORD, L"x1");
      QVERIFY(!cd.GetItemUInt(IDE_PASSWORD_PASSWORD, v));
      EndDialog(d, IDOK);
    });
    QCOMPARE(dlg.Create(IDD_PASSWORD, NULL), (INT_PTR)IDOK);
  }

  void radioGroupsAndUpDown()
  {
    CRecordingDialog dlg;
    CModalScript script([&dlg](HWND d)
    {
      CDialog cd(d);
      cd.CheckRadioButton(IDR_MEM_ACTION_ALLOW, IDR_MEM_ACTION_SKIP_ARC, IDR_MEM_ACTION_ALLOW);
      cd.CheckButton(IDX_MEM_REMEMBER, true);
      QVERIFY(cd.IsButtonCheckedBool(IDR_MEM_ACTION_ALLOW));
      // the checked radio button has the tab stop
      QVERIFY(GetWindowLongW(GetDlgItem(d, IDR_MEM_ACTION_ALLOW), GWL_STYLE) & WS_TABSTOP);
      QVERIFY(!(GetWindowLongW(GetDlgItem(d, IDR_MEM_ACTION_SKIP_ARC), GWL_STYLE) & WS_TABSTOP));
      QTest::mouseClick(Item(d, IDR_MEM_ACTION_SKIP_ARC), Qt::LeftButton);
      QVERIFY(!cd.IsButtonCheckedBool(IDR_MEM_ACTION_ALLOW));
      QVERIFY(cd.IsButtonCheckedBool(IDR_MEM_ACTION_SKIP_ARC));
      QVERIFY(dlg.HasCommand(IDR_MEM_ACTION_SKIP_ARC, BN_CLICKED));
      // a check box after the group is not part of it
      QVERIFY(cd.IsButtonCheckedBool(IDX_MEM_REMEMBER));
      // clicking the checked radio button keeps it checked
      QTest::mouseClick(Item(d, IDR_MEM_ACTION_SKIP_ARC), Qt::LeftButton);
      QVERIFY(cd.IsButtonCheckedBool(IDR_MEM_ACTION_SKIP_ARC));
      // arrow keys move through the group and check the radio button; the
      // dialog manager sees keys only in the active window
      QVERIFY(QTest::qWaitForWindowActive(NShim::WidgetOf(d)));
      Item(d, IDR_MEM_ACTION_SKIP_ARC)->setFocus();
      QTest::keyClick(Item(d, IDR_MEM_ACTION_SKIP_ARC), Qt::Key_Up);
      QVERIFY(cd.IsButtonCheckedBool(IDR_MEM_ACTION_ALLOW));

      // the up-down control takes the edit control before it as its buddy
      QWidget *edit = Item(d, IDE_MEM_SPIN_EDIT);
      QWidget *spin = Item(d, IDC_MEM_SPIN);
      QCOMPARE(spin->geometry().left(), edit->geometry().right() + 1);
      cd.SendItemMessage(IDC_MEM_SPIN, UDM_SETRANGE, 0, MAKELPARAM(100, 1));
      cd.SendItemMessage(IDC_MEM_SPIN, UDM_SETPOS, 0, 4);
      UString s;
      cd.GetItemText(IDE_MEM_SPIN_EDIT, s);
      QCOMPARE(ToQ(s), QStringLiteral("4"));
      QList<QToolButton *> arrows = spin->findChildren<QToolButton *>();
      QCOMPARE(arrows.size(), 2);
      QTest::mouseClick(arrows[0], Qt::LeftButton);
      cd.GetItemText(IDE_MEM_SPIN_EDIT, s);
      QCOMPARE(ToQ(s), QStringLiteral("5"));
      QVERIFY(dlg.Scrolls > 0);
      LRESULT pos = cd.SendItemMessage(IDC_MEM_SPIN, UDM_GETPOS, 0, 0);
      QCOMPARE((int)LOWORD(pos), 5);
      QCOMPARE((int)HIWORD(pos), 0);
      cd.SetItemText(IDE_MEM_SPIN_EDIT, L"27");
      pos = cd.SendItemMessage(IDC_MEM_SPIN, UDM_GETPOS, 0, 0);
      QCOMPARE((int)LOWORD(pos), 27);
      cd.SetItemText(IDE_MEM_SPIN_EDIT, L"x");
      pos = cd.SendItemMessage(IDC_MEM_SPIN, UDM_GETPOS, 0, 0);
      QVERIFY(HIWORD(pos) != 0);
      EndDialog(d, IDOK);
    });
    QCOMPARE(dlg.Create(IDD_MEM, NULL), (INT_PTR)IDOK);
  }

  void timersAndPostedMessages()
  {
    CRecordingDialog dlg;
    dlg.OnInitFunc = [&dlg]()
    {
      dlg.SetTimer(1, 20);
      dlg.PostMsg(WM_APP + 1);
      dlg.PostMsg(WM_APP + 2);
      dlg.PostMsg(WM_APP + 3);
      return true;
    };
    dlg.OnTimerFunc = [&dlg](UINT id)
    {
      QCOMPARE(id, 1u);
      if (dlg.Timers == 3)
      {
        dlg.KillTimer(1);
        dlg.PostMsg(WM_APP + 9);
      }
    };
    dlg.OnAppFunc = [&dlg](UInt32 n)
    {
      if (n == 9)
        QTimer::singleShot(100, [&dlg]() { dlg.End(IDOK); });
    };
    QCOMPARE(dlg.Create(IDD_PASSWORD, NULL), (INT_PTR)IDOK);
    QCOMPARE(dlg.Messages, (std::vector<UInt32>{ 1, 2, 3, 9 }));
    QCOMPARE(dlg.Timers, 3);
  }

  void workerThreadCalls()
  {
    CRecordingDialog dlg;
    std::thread worker;
    CJoiner joiner(worker);
    bool hookOnGuiThread = false;
    NShim::SetMessageBoxHook([&](HWND, const QString &text, const QString &, UINT type)
    {
      hookOnGuiThread = (QThread::currentThread() == qApp->thread());
      return (text == QStringLiteral("question") && (type & MB_TYPEMASK) == MB_YESNO) ? IDNO : IDOK;
    });
    UString textFromWorker;
    int boxResult = 0;
    CModalScript script([&](HWND d)
    {
      worker = std::thread([&dlg, d, &textFromWorker, &boxResult]()
      {
        CDialog cd(d);
        cd.SetItemText(IDE_PASSWORD_PASSWORD, L"from worker");
        cd.GetItemText(IDE_PASSWORD_PASSWORD, textFromWorker);
        boxResult = MessageBoxW(d, L"question", L"7-Zip", MB_YESNO | MB_ICONQUESTION);
        dlg.PostMsg(WM_APP + 5);
      });
    });
    dlg.OnAppFunc = [&dlg](UInt32 n)
    {
      if (n == 5)
        dlg.End(IDOK);
    };
    QCOMPARE(dlg.Create(IDD_PASSWORD, NULL), (INT_PTR)IDOK);
    worker.join();
    QCOMPARE(ToQ(textFromWorker), QStringLiteral("from worker"));
    QCOMPARE(boxResult, IDNO);
    QVERIFY(hookOnGuiThread);
  }

  void messageBoxButtons()
  {
    // a real message box: press "No"
    QTimer::singleShot(0, []()
    {
      QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      QVERIFY(box);
      QCOMPARE(box->text(), QStringLiteral("<b>not html</b>"));
      QAbstractButton *no = box->button(QMessageBox::No);
      QVERIFY(no);
      no->click();
    });
    QCOMPARE(MessageBoxW(NULL, L"<b>not html</b>", L"7-Zip", MB_YESNOCANCEL | MB_ICONQUESTION), IDNO);
    QTimer::singleShot(0, []()
    {
      QTest::keyClick(QApplication::activeModalWidget(), Qt::Key_Escape);
    });
    QCOMPARE(MessageBoxW(NULL, L"error", L"7-Zip", MB_OK | MB_ICONERROR), IDOK);
  }

  void resizeSendsSize()
  {
    CRecordingDialog dlg;
    CModalScript script([&dlg](HWND d)
    {
      QWidget *w = NShim::WidgetOf(d);
      const int width = w->width() + 40;
      w->resize(width, w->height() + 10);
      QTRY_COMPARE(dlg.LastWidth, width);
      RECT r;
      GetClientRect(d, &r);
      QCOMPARE((int)r.right, width);
      // MoveItem and GetClientRectOfItem work in client coordinates
      dlg.MoveItem(IDC_COMBO, 10, 12, 100, 20);
      RECT ri;
      dlg.GetClientRectOfItem(IDC_COMBO, ri);
      QCOMPARE((int)ri.left, 10);
      QCOMPARE((int)ri.top, 12);
      QCOMPARE((int)(ri.right - ri.left), 100);
      EndDialog(d, IDOK);
    });
    QCOMPARE(dlg.Create(IDD_COMBO, NULL), (INT_PTR)IDOK);
    QVERIFY(dlg.SizeCalls >= 1);
  }

  void destroyFromHandler()
  {
    // a modeless dialog that destroys itself from a button handler
    class CSelfDestroying: public CModelessDialog
    {
    public:
      bool Destroyed = false;
      bool OnButtonClicked(unsigned id, HWND wnd) override
      {
        if (id == IDOK)
        {
          Destroy();
          Destroyed = true;
          return true;
        }
        return CModelessDialog::OnButtonClicked(id, wnd);
      }
    };
    CSelfDestroying dlg;
    QVERIFY(dlg.Create(IDD_PASSWORD, NULL));
    const HWND d = dlg;
    QVERIFY(IsWindow(d));
    QPushButton *ok = ItemAs<QPushButton>(d, IDOK);
    QVERIFY(ok);
    QTest::mouseClick(ok, Qt::LeftButton);
    QVERIFY(dlg.Destroyed);
    QVERIFY(!IsWindow(d));
    // messages posted to the destroyed window are dropped
    QVERIFY(!PostMessageW(d, WM_APP, 0, 0));
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
  }

  void listView()
  {
    CRecordingDialog dlg;
    CModalScript script([&dlg](HWND d)
    {
      CListView lv;
      lv.Attach(GetDlgItem(d, IDL_LISTVIEW));
      lv.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);
      QCOMPARE(lv.InsertColumn(0, L"Name", 100), 0);
      QCOMPARE(lv.InsertColumn(1, L"Size", 80), 1);
      for (unsigned i = 0; i < 4; i++)
      {
        UString name;
        name.Add_UInt32(i);
        QCOMPARE(lv.InsertItem(i, name), (int)i);
        lv.SetSubItem(i, 1, L"sub");
      }
      QCOMPARE(lv.GetItemCount(), 4);
      LPARAM param = 0;
      QVERIFY(lv.GetItemParam(2, param));
      QCOMPARE(param, (LPARAM)2);

      dlg.ItemChanges.clear();
      lv.SetItemState_FocusedSelected(1);
      QCOMPARE(lv.GetFocusedItem(), 1);
      QCOMPARE(lv.GetNextSelectedItem(-1), 1);
      QCOMPARE(lv.GetSelectedCount(), 1u);
      QVERIFY(!dlg.ItemChanges.empty());
      const NMLISTVIEW &last = dlg.ItemChanges.back();
      QCOMPARE(last.iItem, 1);
      QVERIFY(last.uNewState & LVIS_SELECTED);
      QVERIFY(last.uNewState & LVIS_FOCUSED);

      // check boxes are state images 1 and 2
      QVERIFY(!lv.GetCheckState(2));
      dlg.ItemChanges.clear();
      lv.SetCheckState(2, true);
      QVERIFY(lv.GetCheckState(2));
      QVERIFY(!dlg.ItemChanges.empty());
      QCOMPARE(dlg.ItemChanges.back().uNewState & LVIS_STATEIMAGEMASK, (UINT)INDEXTOSTATEIMAGEMASK(2));

      // sorting keeps parameters, states and texts together
      lv.SortItems(CompareDescending, 0);
      QVERIFY(lv.GetItemParam(0, param));
      QCOMPARE(param, (LPARAM)3);
      QCOMPARE(lv.GetFocusedItem(), 2);
      QVERIFY(lv.GetCheckState(1));
      LVITEMW item;
      wchar_t buf[32];
      item.mask = LVIF_TEXT;
      item.iItem = 0;
      item.iSubItem = 0;
      item.pszText = buf;
      item.cchTextMax = 32;
      QVERIFY(lv.SendMsg(LVM_GETITEMW, 0, (LPARAM)&item));
      QCOMPARE(QString::fromWCharArray(buf), QStringLiteral("3"));

      // the user: click, double-click, keys
      QWidget *listWidget = ItemAs<QWidget>(d, IDL_LISTVIEW);
      QVERIFY(listWidget);
      QTreeView *tree = listWidget->findChild<QTreeView *>();
      QVERIFY(tree);
      const QRect r = tree->visualRect(tree->model()->index(3, 0));
      dlg.Clicks.clear();
      QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
      QCOMPARE(lv.GetFocusedItem(), 3);
      QVERIFY(!dlg.Clicks.empty());
      QCOMPARE(dlg.Clicks.back().hdr.code, (UINT)NM_CLICK);
      QCOMPARE(dlg.Clicks.back().iItem, 3);
      QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
      QCOMPARE(dlg.Clicks.back().hdr.code, (UINT)LVN_ITEMACTIVATE);
      tree->setFocus();
      QTest::keyClick(tree, Qt::Key_Delete);
      QVERIFY(!dlg.Keys.empty());
      QCOMPARE(dlg.Keys.back(), (WORD)VK_DELETE);

      QVERIFY(lv.DeleteItem(0));
      QCOMPARE(lv.GetItemCount(), 3);
      lv.DeleteAllItems();
      QCOMPARE(lv.GetItemCount(), 0);
      EndDialog(d, IDOK);
    });
    QCOMPARE(dlg.Create(IDD_LISTVIEW, NULL), (INT_PTR)IDOK);
  }

  void propertySheet()
  {
    CTestPage edit, folders;
    CObjectVector<CPageInfo> pages;
    {
      CPageInfo &p = pages.AddNew();
      p.Page = &edit;
      p.ID = IDD_EDIT;
      p.Title = L"Editor";
    }
    {
      CPageInfo &p = pages.AddNew();
      p.Page = &folders;
      p.ID = IDD_FOLDERS;
    }

    {
      CModalScript script([&](HWND sheet)
      {
        // pages are created when they are shown first
        QCOMPARE(edit.Inits, 1);
        QCOMPARE(folders.Inits, 0);
        QCOMPARE(edit.SetActives, 1);
        QTabWidget *tabs = NShim::WidgetOf(sheet)->findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Editor"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Folders"));
        QVERIFY(!IsWindowEnabled(GetDlgItem(sheet, ID_APPLY_NOW)));
        tabs->setCurrentIndex(1);
        QCOMPARE(edit.KillActives, 1);
        QCOMPARE(folders.Inits, 1);
        QCOMPARE(folders.SetActives, 1);
        // a page reports a change: Apply is enabled
        folders.Changed();
        QVERIFY(IsWindowEnabled(GetDlgItem(sheet, ID_APPLY_NOW)));
        // a page can refuse to be left
        folders.RefuseKill = true;
        tabs->setCurrentIndex(0);
        QCOMPARE(tabs->currentIndex(), 1);
        folders.RefuseKill = false;
        ItemAs<QPushButton>(sheet, IDOK)->click();
      });
      QCOMPARE(MyPropertySheet(pages, NULL, UString(L"Options")), (INT_PTR)1);
      QCOMPARE(edit.Applies, 1);
      QCOMPARE(folders.Applies, 1);
    }
    {
      CTestPage e2, f2;
      pages[0].Page = &e2;
      pages[1].Page = &f2;
      CModalScript script([&](HWND sheet)
      {
        QTest::keyClick(NShim::WidgetOf(sheet)->focusWidget() ? NShim::WidgetOf(sheet)->focusWidget()
            : NShim::WidgetOf(sheet), Qt::Key_Escape);
      });
      QCOMPARE(MyPropertySheet(pages, NULL, UString(L"Options")), (INT_PTR)0);
      QCOMPARE(e2.Applies, 0);
      QCOMPARE(e2.Resets, 1);
      QCOMPARE(f2.Inits, 0);
    }
  }
};

QTEST_MAIN(CShimTest)
#include "test_shim.moc"
