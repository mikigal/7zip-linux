// PanelDragLinux.cpp -- the drag and drop parts of the panel and the
// application that FileManager/PanelDrag.cpp implements with OLE. Drag and
// drop on Linux goes through Qt and the Wayland protocols (Phase 6 of
// linux/docs/PLAN.md); until then nothing is dragged or dropped.

#include "../../CPP/7zip/UI/FileManager/StdAfx.h"

#include "../../CPP/7zip/UI/FileManager/App.h"

void CPanel::OnDrag(LPNMLISTVIEW /* nmListView */, bool /* isRightButton */)
{
}

void CApp::CreateDragTarget()
{
  _dropTargetSpec = NULL;
}

void CApp::SetFocusedPanel(unsigned index)
{
  LastFocusedPanel = index;
}

void CApp::DragBegin(unsigned /* panelIndex */)
{
}

void CApp::DragEnd()
{
}
