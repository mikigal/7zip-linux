// ShimResources.h -- icons, image lists and fonts behind the Win32 handles
// (internal to linux/shim).

#ifndef ZIP7_INC_LINUX_SHIM_RESOURCES_H
#define ZIP7_INC_LINUX_SHIM_RESOURCES_H

#include <QFont>
#include <QIcon>
#include <QList>

#include <windows.h>
#include <CommCtrl.h>

struct HICON__
{
  QIcon Icon;
  bool Shared;   // from LoadIcon(): DestroyIcon() does not free it
};

struct _IMAGELIST
{
  int Cx;
  int Cy;
  QList<QIcon> Icons;
  bool System = false;   // a system image list (SHGetFileInfo): never destroyed
};

struct HFONT__
{
  QFont Font;
};

namespace NShim {

// The icon of an icon resource ID (IDI_*), or a null icon.
QIcon ResourceIcon(UInt32 id);

// A handle for an icon that the caller owns (DestroyIcon() frees it).
HICON NewIconHandle(const QIcon &icon);

// The system image list of small (16 px) or large (32 px) icons.
HIMAGELIST SystemImageList(bool smallIcons);

}

#endif
