// gallery.cpp -- renders every dialog template of 7zFM (which include those
// of 7zG) with the dialog engine and the dark theme into PNG files, for
// visual review and the golden-image tests.
//
// Usage: z7-gallery --out DIR [--font "Family,points"] [--only NAME]...
//
// Dialogs are rendered from their templates as they are before the dialog
// code fills them; property pages are rendered alone and inside a property
// sheet. The exit code is 77 when the requested font is not installed, so
// that the golden test is skipped instead of failing on another machine.

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include <windows.h>
#include <CommCtrl.h>
#include <prsht.h>

#include "RcTables.h"
#include "ShimDialog.h"
#include "Theme.h"

static INT_PTR CALLBACK GalleryProc(HWND, UINT, WPARAM, LPARAM)
{
  // FALSE for WM_INITDIALOG too: no focus, its frame would depend on timing
  return FALSE;
}

static bool IsPage(const NRc::CDialog &t)
{
  return (t.Style & WS_CHILD) != 0;
}

static bool Save(QWidget *w, const QString &path)
{
  w->show();
  QCoreApplication::processEvents();
  // no focus frame or text cursor: they depend on timing
  if (QWidget *f = w->focusWidget())
    f->clearFocus();
  QCoreApplication::processEvents();
  const QPixmap p = w->grab();
  if (!p.save(path))
  {
    fprintf(stderr, "z7-gallery: cannot write %s\n", qPrintable(path));
    return false;
  }
  printf("%s %dx%d\n", qPrintable(QFileInfo(path).fileName()), p.width(), p.height());
  return true;
}

static int RenderSheet(const QString &outDir, const QStringList &only)
{
  // the 7-Zip options: the pages of OptionsDialog.cpp that exist on Linux
  static const UInt32 kPages[] = { 2500 /* IDD_SETTINGS */, 2101 /* IDD_LANG */,
      2400 /* IDD_FOLDERS */, 2103 /* IDD_EDIT */, 2300 /* IDD_MENU */, 2200 /* IDD_SYSTEM */ };
  const QString name = QStringLiteral("PROPERTY_SHEET_OPTIONS");
  if (!only.isEmpty() && !only.contains(name))
    return 0;
  std::vector<PROPSHEETPAGEW> pages;
  for (UInt32 id : kPages)
  {
    if (!NRc::FindDialog(id))
      continue;
    PROPSHEETPAGEW p;
    memset(&p, 0, sizeof(p));
    p.dwSize = sizeof(p);
    p.dwFlags = PSP_HASHELP;
    p.pszTemplate = MAKEINTRESOURCEW(id);
    p.pfnDlgProc = GalleryProc;
    pages.push_back(p);
  }
  PROPSHEETHEADERW h;
  memset(&h, 0, sizeof(h));
  h.dwSize = sizeof(h);
  h.dwFlags = PSH_PROPSHEETPAGE;
  h.pszCaption = L"Options";
  h.nPages = (UINT)pages.size();
  h.ppsp = pages.data();
  bool ok = false;
  NShim::SetModalHook([&](HWND sheet)
  {
    ok = Save(NShim::WidgetOf(sheet), QDir(outDir).filePath(name + QStringLiteral(".png")));
    EndDialog(sheet, 0);
  });
  PropertySheetW(&h);
  NShim::SetModalHook(NShim::CModalHook());
  return ok ? 0 : 1;
}

// The toolbar icons in both sizes on the toolbar background, at 1x and 2x.
static int RenderToolbarIcons(const QString &outDir, const QStringList &only)
{
  const QString name = QStringLiteral("TOOLBAR_ICONS");
  if (!only.isEmpty() && !only.contains(name))
    return 0;
  static const char * const kNames[] = { "Add", "Extract", "Test", "Copy", "Move", "Delete", "Info" };
  const int n = (int)(sizeof(kNames) / sizeof(kNames[0]));
  QImage image(n * 104 + 8, 36 + 24 + 72 + 48 + 40, QImage::Format_RGB32);
  image.fill(QApplication::palette().color(QPalette::Window));
  QPainter p(&image);
  int errors = 0;
  for (int i = 0; i < n; i++)
  {
    const QIcon large = NTheme::ToolbarIcon(kNames[i], true);
    const QIcon small = NTheme::ToolbarIcon(kNames[i], false);
    if (large.isNull() || small.isNull())
    {
      fprintf(stderr, "z7-gallery: no toolbar icon %s\n", kNames[i]);
      errors++;
      continue;
    }
    const int x = 8 + i * 104;
    p.drawPixmap(x, 8, large.pixmap(QSize(48, 36)));
    p.drawPixmap(x + 56, 8 + 6, small.pixmap(QSize(24, 24)));
    p.drawPixmap(x, 8 + 36 + 16, large.pixmap(QSize(96, 72)));
    p.drawPixmap(x, 8 + 36 + 16 + 72 + 8, small.pixmap(QSize(48, 48)));
  }
  p.end();
  const QString path = QDir(outDir).filePath(name + QStringLiteral(".png"));
  if (!image.save(path))
    errors++;
  else
    printf("%s %dx%d\n", qPrintable(QFileInfo(path).fileName()), image.width(), image.height());
  return errors;
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QCommandLineParser parser;
  parser.addHelpOption();
  const QCommandLineOption outOption(QStringLiteral("out"), QStringLiteral("output directory"), QStringLiteral("dir"));
  const QCommandLineOption fontOption(QStringLiteral("font"), QStringLiteral("font as Family,points"), QStringLiteral("font"));
  const QCommandLineOption onlyOption(QStringLiteral("only"), QStringLiteral("render only this dialog"), QStringLiteral("name"));
  parser.addOption(outOption);
  parser.addOption(fontOption);
  parser.addOption(onlyOption);
  parser.process(app);
  if (!parser.isSet(outOption))
  {
    fprintf(stderr, "z7-gallery: --out is required\n");
    return 2;
  }
  const QString outDir = parser.value(outOption);
  QDir().mkpath(outDir);
  const QStringList only = parser.values(onlyOption);

  NTheme::Apply(app);
  if (parser.isSet(fontOption))
  {
    const QStringList f = parser.value(fontOption).split(QLatin1Char(','));
    const QFont font(f.value(0), f.value(1, QStringLiteral("10")).toInt());
    if (!QFontInfo(font).exactMatch() && QFontInfo(font).family() != f.value(0))
    {
      fprintf(stderr, "z7-gallery: font %s is not installed\n", qPrintable(f.value(0)));
      return 77;
    }
    QApplication::setFont(font);
  }

  int errors = 0;
  const NRc::CResources &r = NRc::g_Resources;
  for (unsigned i = 0; i < r.NumDialogs; i++)
  {
    const NRc::CDialog &t = r.Dialogs[i];
    const QString name = t.Name ? QString::fromLatin1(t.Name) : QStringLiteral("DIALOG_%1").arg(t.Id);
    if (!only.isEmpty() && !only.contains(name))
      continue;
    const HWND dlg = NShim::CreateDialogFromTemplate(&t, NULL, GalleryProc, 0);
    if (!dlg)
    {
      fprintf(stderr, "z7-gallery: cannot create %s\n", qPrintable(name));
      errors++;
      continue;
    }
    QWidget *w = NShim::WidgetOf(dlg);
    if (IsPage(t))
    {
      // a page alone has no background of its own
      w->setAutoFillBackground(true);
    }
    if (!Save(w, QDir(outDir).filePath(name + QStringLiteral(".png"))))
      errors++;
    DestroyWindow(dlg);
    QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
  }
  if (RenderSheet(outDir, only) != 0)
    errors++;
  errors += RenderToolbarIcons(outDir, only);
  return errors ? 1 : 0;
}
