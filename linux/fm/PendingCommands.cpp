// PendingCommands.cpp -- file manager commands whose upstream code uses
// Windows file APIs only, with the "not supported" message until they are
// ported: the version control commands (FileManager/VerCtrl.cpp), which
// the File menu shows only when a VerCtrlPath is set in the registry.

#include "../../CPP/7zip/UI/FileManager/StdAfx.h"

#include "../../CPP/7zip/UI/FileManager/App.h"

void CApp::VerCtrl(unsigned /* id */)
{
  GetFocusedPanel().MessageBox_Error_UnsupportOperation();
}
