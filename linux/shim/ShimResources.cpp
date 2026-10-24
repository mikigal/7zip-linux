// ShimResources.cpp -- icons, cursors, image lists, fonts and accelerator
// tables.

#include <QApplication>
#include <QFontDatabase>
#include <QHash>
#include <QStyle>

#include "ShimResources.h"
#include "ShimWindow.h"

#include "RcTables.h"

namespace NShim {

QIcon ResourceIcon(UInt32 id)
{
  // Icon resources come from the .rc files (see rcgen.py); they are built
  // into the program as Qt resources named after the resource ID.
  const QString path = QStringLiteral(":/z7/icons/%1.ico").arg(id);
  QIcon icon(path);
  if (icon.availableSizes().isEmpty())
    return QIcon();
  return icon;
}

static QIcon StandardIcon(UInt32 id)
{
  QStyle *style = QApplication::style();
  switch (id)
  {
    case 32513: return style->standardIcon(QStyle::SP_MessageBoxCritical);    // IDI_HAND
    case 32514: return style->standardIcon(QStyle::SP_MessageBoxQuestion);    // IDI_QUESTION
    case 32515: return style->standardIcon(QStyle::SP_MessageBoxWarning);     // IDI_EXCLAMATION
    case 32516: return style->standardIcon(QStyle::SP_MessageBoxInformation); // IDI_ASTERISK
    default: return QApplication::windowIcon();                               // IDI_APPLICATION
  }
}

HICON NewIconHandle(const QIcon &icon)
{
  HICON h = new HICON__;
  h->Icon = icon;
  h->Shared = false;
  return h;
}

QIcon IconFromHandle(HICON icon)
{
  return icon ? icon->Icon : QIcon();
}

static HICON LoadIconImpl(UInt32 id)
{
  // LoadIcon() returns shared handles: one per resource for the process
  static QHash<UInt32, HICON> cache;
  const auto it = cache.constFind(id);
  if (it != cache.constEnd())
    return *it;
  QIcon icon = (id >= 32512 && id <= 32517) ? StandardIcon(id) : ResourceIcon(id);
  if (icon.isNull())
    return NULL;
  HICON h = new HICON__;
  h->Icon = icon;
  h->Shared = true;
  cache.insert(id, h);
  return h;
}

}

using namespace NShim;

HICON LoadIconW(HINSTANCE /* instance */, LPCWSTR name)
{
  if (!IS_INTRESOURCE(name))
    return NULL;
  return OnGuiThread<HICON>([=]() { return LoadIconImpl((UInt32)(uintptr_t)name); });
}

HICON LoadIconA(HINSTANCE instance, LPCSTR name)
{
  return LoadIconW(instance, (LPCWSTR)(const void *)name);
}

HANDLE LoadImageW(HINSTANCE instance, LPCWSTR name, UINT type, int /* cx */, int /* cy */, UINT /* flags */)
{
  if (type != IMAGE_ICON)
    return NULL;
  return (HANDLE)LoadIconW(instance, name);
}

HANDLE LoadImageA(HINSTANCE instance, LPCSTR name, UINT type, int cx, int cy, UINT flags)
{
  return LoadImageW(instance, (LPCWSTR)(const void *)name, type, cx, cy, flags);
}

BOOL DestroyIcon(HICON icon)
{
  if (!icon)
    return FALSE;
  if (!icon->Shared)
    delete icon;
  return TRUE;
}

HCURSOR LoadCursorW(HINSTANCE /* instance */, LPCWSTR name)
{
  // a cursor handle stands for the cursor shape ID; see SetCursor()
  return (HCURSOR)(void *)name;
}

HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name)
{
  return LoadCursorW(instance, (LPCWSTR)(const void *)name);
}

HCURSOR SetCursor(HCURSOR cursor)
{
  static HCURSOR current = NULL;
  const HCURSOR prev = current;
  current = cursor;
  RunOnGuiThread([=]()
  {
    while (QApplication::overrideCursor())
      QApplication::restoreOverrideCursor();
    if ((uintptr_t)cursor == 32514)  // IDC_WAIT
      QApplication::setOverrideCursor(Qt::WaitCursor);
  });
  return prev;
}

// ---------- Image lists ----------

HIMAGELIST ImageList_Create(int cx, int cy, UINT /* flags */, int /* initial */, int /* grow */)
{
  HIMAGELIST list = new _IMAGELIST;
  list->Cx = cx;
  list->Cy = cy;
  return list;
}

BOOL ImageList_Destroy(HIMAGELIST list)
{
  if (!list)
    return FALSE;
  delete list;
  return TRUE;
}

int ImageList_GetImageCount(HIMAGELIST list)
{
  return list ? (int)list->Icons.size() : 0;
}

BOOL ImageList_GetImageInfo(HIMAGELIST list, int index, IMAGEINFO *info)
{
  if (!list || index < 0 || index >= (int)list->Icons.size() || !info)
    return FALSE;
  memset(info, 0, sizeof(*info));
  info->rcImage.right = list->Cx;
  info->rcImage.bottom = list->Cy;
  return TRUE;
}

int ImageList_Add(HIMAGELIST /* list */, HBITMAP /* image */, HBITMAP /* mask */)
{
  // bitmaps are not used with image lists on Linux
  return -1;
}

int ImageList_AddMasked(HIMAGELIST /* list */, HBITMAP /* image */, COLORREF /* mask */)
{
  return -1;
}

int ImageList_ReplaceIcon(HIMAGELIST list, int index, HICON icon)
{
  if (!list)
    return -1;
  const QIcon qi = IconFromHandle(icon);
  if (index < 0)
  {
    list->Icons.append(qi);
    return (int)list->Icons.size() - 1;
  }
  if (index >= (int)list->Icons.size())
    return -1;
  list->Icons[index] = qi;
  return index;
}

BOOL ImageList_Remove(HIMAGELIST list, int index)
{
  if (!list)
    return FALSE;
  if (index < 0)
  {
    list->Icons.clear();
    return TRUE;
  }
  if (index >= (int)list->Icons.size())
    return FALSE;
  list->Icons.removeAt(index);
  return TRUE;
}

HICON ImageList_GetIcon(HIMAGELIST list, int index, UINT /* flags */)
{
  if (!list || index < 0 || index >= (int)list->Icons.size())
    return NULL;
  return NewIconHandle(list->Icons[index]);
}

BOOL ImageList_GetIconSize(HIMAGELIST list, int *cx, int *cy)
{
  if (!list)
    return FALSE;
  if (cx)
    *cx = list->Cx;
  if (cy)
    *cy = list->Cy;
  return TRUE;
}

BOOL ImageList_SetIconSize(HIMAGELIST list, int cx, int cy)
{
  if (!list)
    return FALSE;
  list->Cx = cx;
  list->Cy = cy;
  list->Icons.clear();
  return TRUE;
}

COLORREF ImageList_SetBkColor(HIMAGELIST /* list */, COLORREF /* color */)
{
  return 0xFFFFFFFF;  // CLR_NONE
}

// ---------- Fonts ----------

static HFONT CreateFontImpl(LONG height, LONG weight, bool italic, BYTE pitchAndFamily, const QString &face)
{
  QFont f = QApplication::font();
  if ((pitchAndFamily & 3) == FIXED_PITCH || (pitchAndFamily & 0xF0) == FF_MODERN)
    f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  if (!face.isEmpty())
    f.setFamily(face);
  if (height != 0)
  {
    // a negative height is the character height, a positive one the cell
    // height; both are in pixels at 96 dpi
    const int px = height < 0 ? -height : height * 4 / 5;
    if (px > 0)
      f.setPixelSize(px);
  }
  if (weight != FW_DONTCARE)
    f.setWeight(weight >= FW_BOLD ? QFont::Bold : QFont::Normal);
  f.setItalic(italic);
  HFONT h = new HFONT__;
  h->Font = f;
  return h;
}

HFONT CreateFontIndirectW(const LOGFONTW *font)
{
  if (!font)
    return NULL;
  const QString face = FromW(font->lfFaceName, wcsnlen(font->lfFaceName, LF_FACESIZE));
  return OnGuiThread<HFONT>([=]()
  {
    return CreateFontImpl(font->lfHeight, font->lfWeight, font->lfItalic != 0, font->lfPitchAndFamily, face);
  });
}

HFONT CreateFontIndirectA(const LOGFONTA *font)
{
  if (!font)
    return NULL;
  const QString face = QString::fromUtf8(font->lfFaceName, (qsizetype)strnlen(font->lfFaceName, LF_FACESIZE));
  return OnGuiThread<HFONT>([=]()
  {
    return CreateFontImpl(font->lfHeight, font->lfWeight, font->lfItalic != 0, font->lfPitchAndFamily, face);
  });
}

BOOL DeleteObject(HGDIOBJ object)
{
  // fonts are the only GDI objects that are created
  delete (HFONT)object;
  return TRUE;
}

// ---------- Accelerators ----------

struct HACCEL__
{
  const NRc::CAccelTable *Table;
};

HACCEL LoadAcceleratorsW(HINSTANCE /* instance */, LPCWSTR name)
{
  if (!IS_INTRESOURCE(name))
    return NULL;
  const NRc::CAccelTable *t = NRc::FindAccelTable((UInt32)(uintptr_t)name);
  if (!t)
    return NULL;
  HACCEL h = new HACCEL__;
  h->Table = t;
  return h;
}

HACCEL LoadAcceleratorsA(HINSTANCE instance, LPCSTR name)
{
  return LoadAcceleratorsW(instance, (LPCWSTR)(const void *)name);
}

int TranslateAcceleratorW(HWND /* wnd */, HACCEL /* accel */, LPMSG /* msg */)
{
  // the Qt main window maps the table to QActions instead
  return 0;
}

int TranslateAcceleratorA(HWND wnd, HACCEL accel, LPMSG msg)
{
  return TranslateAcceleratorW(wnd, accel, msg);
}
