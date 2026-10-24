// ShimDialog.h -- the dialog engine of linux/shim, for the property sheet
// code, the dialog gallery and the tests.

#ifndef ZIP7_INC_LINUX_SHIM_DIALOG_H
#define ZIP7_INC_LINUX_SHIM_DIALOG_H

#include <QFont>
#include <QIcon>
#include <QString>
#include <QStringList>

#include <functional>

#include <windows.h>

class QWidget;

namespace NRc {
struct CDialog;
}

class QDrag;

namespace NShim {

// Dialog base units: the pixel size of 4 horizontal and 8 vertical dialog units.
struct CBaseUnits
{
  int X;
  int Y;
  int ToPixelsX(int units) const;
  int ToPixelsY(int units) const;
};

CBaseUnits ComputeBaseUnits(const QFont &font);
QFont DialogFont();
CBaseUnits DialogBaseUnits(HWND dlg);

// Creates the dialog of a template and sends WM_INITDIALOG; it is not shown.
HWND CreateDialogFromTemplate(const NRc::CDialog *t, HWND parent, DLGPROC proc, LPARAM param);
// Shows a dialog modally until EndDialog(), destroys it and returns the result.
INT_PTR RunModalDialog(HWND dlg);

// Creates a dialog from a template in a given Qt parent (a property page in
// its tab); the window parent is parent.
HWND CreateDialogFromTemplateIn(const NRc::CDialog *t, HWND parent, QWidget *qtParent,
    DLGPROC proc, LPARAM param);

// The widget of a window, for tests and tools; NULL if wnd is not a window.
QWidget *WidgetOf(HWND wnd);
// The icon of an icon handle, for tests.
QIcon IconFromHandleForTest(HICON icon);
// Whether the modal loop of a dialog runs and EndDialog() was not called.
bool IsInModalLoop(HWND dlg);

/* Test hooks. The modal hook runs on the GUI thread inside the loop of each
   modal dialog, right after the dialog is shown. The message box hook runs
   instead of the message box; it returns the button ID (IDOK, ...) or 0 to
   show the message box normally. */
typedef std::function<void(HWND)> CModalHook;
typedef std::function<int(HWND owner, const QString &text, const QString &caption, UINT type)> CMessageBoxHook;
void SetModalHook(const CModalHook &hook);
void SetMessageBoxHook(const CMessageBoxHook &hook);

/* The file dialog hook runs instead of the Qt file dialogs (the folder
   browser and the open and save dialogs). It returns false for Cancel, or
   true with the chosen path in Path (and the filter in FilterIndex). */
struct CFileDialogRequest
{
  enum EKind { kFolder, kOpenFile, kSaveFile };
  EKind Kind;
  HWND Owner;
  QString Title;
  QString Path;          // in: the initial folder or file; out: the result
  QStringList Filters;   // Qt name filters ("7z archive (*.7z)")
  int FilterIndex;       // in and out; -1: none
};
typedef std::function<bool(CFileDialogRequest &request)> CFileDialogHook;
void SetFileDialogHook(const CFileDialogHook &hook);

/* The drag hook runs instead of QDrag::exec() in DoDragDrop(): it plays
   the drop target with the drag's mime data and the actions that the
   source allows, and returns the action the target took
   (Qt::IgnoreAction: no drop). */
typedef std::function<Qt::DropAction(QDrag *drag, Qt::DropActions actions)> CDragHook;
void SetDragHook(const CDragHook &hook);

}

#endif
