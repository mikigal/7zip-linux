// ShimMenu.cpp -- menus: HMENU trees, the menu bar of a top-level window
// and popup menus.
//
// An HMENU is a list of items that lives apart from any window, as on
// Windows; the program builds and changes it with the menu calls. Qt menus
// are made from it only when they are shown: a QMenuBar for SetMenu(),
// whose menus are filled when they open, and a QMenu for
// TrackPopupMenuEx(). Before a menu opens, its window gets WM_INITMENUPOPUP,
// so the program can update the items first (7-Zip rebuilds its File menu
// there); a chosen item comes back as WM_COMMAND, posted as on Windows.

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>

#include <unordered_set>
#include <vector>

#include "ShimResources.h"
#include "ShimWindow.h"

#include "RcTables.h"

namespace NShim {

struct CMenuEntry
{
  UINT Type;      // MFT_*
  UINT State;     // MFS_*
  UINT Id;
  HMENU SubMenu;  // owned: destroyed with the menu
  ULONG_PTR Data;
  QString Text;
  HBITMAP BmpChecked;
  HBITMAP BmpUnchecked;
  HBITMAP BmpItem;

  CMenuEntry(): Type(MFT_STRING), State(0), Id(0), SubMenu(NULL), Data(0),
      BmpChecked(NULL), BmpUnchecked(NULL), BmpItem(NULL) {}
  bool IsSeparator() const { return (Type & MFT_SEPARATOR) != 0; }
};

}

namespace NShim { class CMenuBarContext; }

struct HMENU__
{
  bool Popup;
  std::vector<NShim::CMenuEntry> Items;
  HWND Window;                // the window that shows it as its menu bar
  QPointer<QMenuBar> Bar;
  NShim::CMenuBarContext *BarContext;  // a child of Bar
  HMENU__(bool popup): Popup(popup), Window(NULL), BarContext(NULL) {}
};

namespace NShim {

static std::unordered_set<HMENU> g_Menus;

static HMENU Live(HMENU menu)
{
  return (menu && g_Menus.find(menu) != g_Menus.end()) ? menu : NULL;
}

static void DestroyMenuTree(HMENU menu)
{
  if (!Live(menu))
    return;
  g_Menus.erase(menu);
  for (CMenuEntry &e : menu->Items)
    DestroyMenuTree(e.SubMenu);
  if (menu->Bar)
    menu->Bar->deleteLater();
  if (menu->Window && FromHandle(menu->Window) && menu->Window->Menu == menu)
    menu->Window->Menu = NULL;
  delete menu;
}

// Windows frees the menus of a program when it ends; 7zFM keeps some
// (the copy of the File menu) until then.
static void FreeAllMenus()
{
  while (!g_Menus.empty())
    DestroyMenuTree(*g_Menus.begin());
}

static HMENU NewMenu(bool popup)
{
  static bool s_FreeAtEnd = false;
  if (!s_FreeAtEnd)
  {
    s_FreeAtEnd = true;
    qAddPostRoutine(FreeAllMenus);
  }
  HMENU m = new HMENU__(popup);
  g_Menus.insert(m);
  return m;
}

/* Finds an item by position or (recursively, as Windows does) by command:
   submenus are searched first; a popup item with the ID is the fallback. */
static bool FindItem(HMENU menu, UINT item, UINT flags, HMENU &owner, unsigned &pos)
{
  menu = Live(menu);
  if (!menu)
    return false;
  if (flags & MF_BYPOSITION)
  {
    if (item >= menu->Items.size())
      return false;
    owner = menu;
    pos = item;
    return true;
  }
  bool haveFallback = false;
  HMENU fbOwner = NULL;
  unsigned fbPos = 0;
  for (unsigned i = 0; i < menu->Items.size(); i++)
  {
    const CMenuEntry &e = menu->Items[i];
    if (e.SubMenu)
    {
      if (FindItem(e.SubMenu, item, flags, owner, pos))
        return true;
      if (e.Id == item && !haveFallback)
      {
        haveFallback = true;
        fbOwner = menu;
        fbPos = i;
      }
    }
    else if (e.Id == item && !e.IsSeparator())
    {
      owner = menu;
      pos = i;
      return true;
    }
  }
  if (haveFallback)
  {
    owner = fbOwner;
    pos = fbPos;
  }
  return haveFallback;
}

static CMenuEntry *FindEntry(HMENU menu, UINT item, UINT flags)
{
  HMENU owner = NULL;
  unsigned pos = 0;
  if (!FindItem(menu, item, flags, owner, pos))
    return NULL;
  return &owner->Items[pos];
}

// MF_* flags (MENU resources, AppendMenu()) to item types and states.
static void FromMenuFlags(UINT flags, CMenuEntry &e)
{
  e.Type = 0;
  if (flags & MF_SEPARATOR)
    e.Type |= MFT_SEPARATOR;
  if (flags & MF_MENUBREAK)
    e.Type |= MFT_MENUBREAK;
  if (flags & MF_MENUBARBREAK)
    e.Type |= MFT_MENUBARBREAK;
  if (flags & MF_OWNERDRAW)
    e.Type |= MFT_OWNERDRAW;
  if (flags & MF_RIGHTJUSTIFY)
    e.Type |= MFT_RIGHTJUSTIFY;
  e.State = 0;
  if (flags & (MF_GRAYED | MF_DISABLED))
    e.State |= MFS_GRAYED;
  if (flags & MF_CHECKED)
    e.State |= MFS_CHECKED;
  if (flags & MF_DEFAULT)
    e.State |= MFS_DEFAULT;
}

static UINT StateToMenuFlags(const CMenuEntry &e)
{
  UINT f = 0;
  if (e.State & MFS_GRAYED)
    f |= MF_GRAYED | MF_DISABLED;
  if (e.State & MFS_CHECKED)
    f |= MF_CHECKED;
  if (e.State & MFS_DEFAULT)
    f |= MF_DEFAULT;
  if (e.State & MFS_HILITE)
    f |= MF_HILITE;
  if (e.Type & MFT_SEPARATOR)
    f |= MF_SEPARATOR;
  if (e.Type & MFT_MENUBREAK)
    f |= MF_MENUBREAK;
  if (e.Type & MFT_MENUBARBREAK)
    f |= MF_MENUBARBREAK;
  if (e.Type & MFT_RADIOCHECK)
    f |= MFT_RADIOCHECK;
  return f;
}

// ---------- Resources ----------

static unsigned LoadMenuItems(HMENU menu, const NRc::CMenu &res, unsigned index, unsigned level)
{
  while (index < res.NumItems)
  {
    const NRc::CMenuItem &it = res.Items[index];
    if (it.Level < level)
      break;
    CMenuEntry e;
    if (res.IsEx)
    {
      e.Type = it.Type;
      e.State = it.State;
    }
    else
      FromMenuFlags(it.Type | it.State, e);
    e.Id = it.Id;
    e.Text = FromW(it.Text);
    index++;
    if (it.IsPopup)
    {
      e.SubMenu = NewMenu(true);
      index = LoadMenuItems(e.SubMenu, res, index, level + 1);
    }
    menu->Items.push_back(e);
  }
  return index;
}

static HMENU LoadMenuImpl(UInt32 id)
{
  const NRc::CMenu *res = NRc::FindMenu(id);
  if (!res)
    return NULL;
  HMENU menu = NewMenu(false);
  LoadMenuItems(menu, *res, 0, 0);
  return menu;
}

// ---------- Qt menus ----------

// What a popup menu that is open returns (TrackPopupMenuEx with
// TPM_RETURNCMD) or where its commands go.
struct CMenuContext
{
  HWND Window;
  bool Notify;     // send WM_INITMENUPOPUP
  bool ReturnCmd;  // the command is returned, not posted
  UINT Chosen;
};

static void FillQMenuWithContext(QMenu *qm, HMENU menu, CMenuContext *ctx);

static void SendInitMenuPopup(const CMenuContext &ctx, HMENU menu, unsigned pos, bool systemMenu)
{
  if (ctx.Notify && FromHandle(ctx.Window))
    Send(ctx.Window, WM_INITMENUPOPUP, (WPARAM)menu, MAKELPARAM(pos, systemMenu ? 1 : 0));
}

static QString ActionText(const CMenuEntry &e)
{
  // "&Open\tEnter": Qt menus show the part after the tab as the shortcut
  return e.Text;
}

static void ChooseCommand(CMenuContext *ctx, UINT id)
{
  if (ctx->ReturnCmd)
  {
    ctx->Chosen = id;
    return;
  }
  if (FromHandle(ctx->Window))
    Post(ctx->Window, WM_COMMAND, MAKEWPARAM(id, 0), 0);
}

/* Fills a Qt menu with the items of an HMENU. A submenu is filled when it
   opens, after WM_INITMENUPOPUP. The actions keep the context by pointer:
   menu bar contexts live in the bar, popup contexts in TrackPopupMenuEx(). */
static void FillQMenuWithContext(QMenu *qm, HMENU menu, CMenuContext *ctx)
{
  qm->clear();
  menu = Live(menu);
  if (!menu)
    return;
  QActionGroup *radioGroup = NULL;
  for (unsigned i = 0; i < menu->Items.size(); i++)
  {
    const CMenuEntry e = menu->Items[i];
    if (e.IsSeparator())
    {
      qm->addSeparator();
      radioGroup = NULL;
      continue;
    }
    QAction *a;
    if (e.SubMenu)
    {
      QMenu *sub = new QMenu(ActionText(e), qm);
      a = sub->menuAction();
      qm->addAction(a);
      const HMENU subMenu = e.SubMenu;
      QObject::connect(sub, &QMenu::aboutToShow, sub, [sub, subMenu, ctx, i]()
      {
        SendInitMenuPopup(*ctx, subMenu, i, false);
        FillQMenuWithContext(sub, subMenu, ctx);
      });
      // Qt does not open a menu without items, so the submenu gets its
      // items now and again when it opens
      FillQMenuWithContext(sub, subMenu, ctx);
      radioGroup = NULL;
    }
    else
    {
      a = qm->addAction(ActionText(e));
      const UINT id = e.Id;
      QObject::connect(a, &QAction::triggered, qm, [ctx, id]() { ChooseCommand(ctx, id); });
      if (e.Type & MFT_RADIOCHECK)
      {
        if (!radioGroup)
        {
          radioGroup = new QActionGroup(qm);
          radioGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
        }
        radioGroup->addAction(a);
        a->setCheckable(true);
      }
      else
      {
        radioGroup = NULL;
        if (e.State & MFS_CHECKED)
          a->setCheckable(true);
      }
      a->setChecked((e.State & MFS_CHECKED) != 0);
    }
    a->setEnabled(!(e.State & MFS_GRAYED));
    if (e.State & MFS_DEFAULT)
    {
      QFont f = a->font();
      f.setBold(true);
      a->setFont(f);
    }
    HBITMAP bmp = e.BmpItem ? e.BmpItem : e.BmpUnchecked;
    if (bmp)
      a->setIcon(bmp->Icon);
  }
}

// The menu bar of a window: one action per top item, with a Qt menu that
// is filled when it opens.
class CMenuBarContext: public QObject
{
public:
  CMenuContext Ctx;
  CMenuBarContext(HWND wnd, QObject *parent): QObject(parent)
  {
    Ctx.Window = wnd;
    Ctx.Notify = true;
    Ctx.ReturnCmd = false;
    Ctx.Chosen = 0;
  }
};

static void BuildMenuBar(HMENU menu)
{
  QMenuBar *bar = menu->Bar;
  if (!bar)
    return;
  CMenuBarContext *ctx = menu->BarContext;
  // the old menus may be open right now (a command that reloads the menu)
  for (QAction *a : bar->actions())
  {
    bar->removeAction(a);
    if (a->menu())
      a->menu()->deleteLater();
    else
      a->deleteLater();
  }
  for (unsigned i = 0; i < menu->Items.size(); i++)
  {
    const CMenuEntry &e = menu->Items[i];
    if (e.IsSeparator())
      continue;
    if (e.SubMenu)
    {
      QMenu *sub = new QMenu(ActionText(e), bar);
      const HMENU subMenu = e.SubMenu;
      CMenuContext *c = &ctx->Ctx;
      QObject::connect(sub, &QMenu::aboutToShow, sub, [sub, subMenu, c, i]()
      {
        SendInitMenuPopup(*c, subMenu, i, false);
        FillQMenuWithContext(sub, subMenu, c);
      });
      FillQMenuWithContext(sub, subMenu, c);
      QAction *a = bar->addMenu(sub);
      a->setEnabled(!(e.State & MFS_GRAYED));
    }
    else
    {
      QAction *a = bar->addAction(ActionText(e));
      const UINT id = e.Id;
      CMenuContext *c = &ctx->Ctx;
      QObject::connect(a, &QAction::triggered, bar, [c, id]() { ChooseCommand(c, id); });
      a->setEnabled(!(e.State & MFS_GRAYED));
    }
  }
}

void SetWindowMenuBar(HWND wnd, HMENU menu)
{
  QWidget *frame = wnd->Widget;
  QLayout *layout = frame ? frame->layout() : NULL;
  HMENU old = Live(wnd->Menu);
  if (old && old != menu)
  {
    if (old->Bar)
      old->Bar->deleteLater();
    old->Bar = NULL;
    old->Window = NULL;
  }
  wnd->Menu = menu;
  if (!menu || !layout)
  {
    if (layout)
      layout->setMenuBar(NULL);
    return;
  }
  menu->Window = wnd;
  if (!menu->Bar)
  {
    QMenuBar *bar = new QMenuBar(frame);
    // 7-Zip windows have their menu in the window, as on Windows
    bar->setNativeMenuBar(false);
    menu->BarContext = new CMenuBarContext(wnd, bar);
    menu->Bar = bar;
  }
  layout->setMenuBar(menu->Bar);
  BuildMenuBar(menu);
}

void DestroyWindowMenu(HWND wnd)
{
  HMENU menu = Live(wnd->Menu);
  wnd->Menu = NULL;
  if (menu)
    DestroyMenuTree(menu);
}

// ---------- Item info ----------

template <class TInfo, class TChar>
static BOOL GetItemInfoImpl(HMENU menu, UINT item, BOOL byPosition, TInfo *info,
    int (*copy)(const QString &, TChar *, int), int (*length)(const QString &))
{
  CMenuEntry *e = FindEntry(menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND);
  if (!e || !info)
    return FALSE;
  const UINT mask = info->fMask;
  if (mask & (MIIM_TYPE | MIIM_FTYPE))
    info->fType = e->Type;
  if (mask & MIIM_STATE)
    info->fState = e->State;
  if (mask & MIIM_ID)
    info->wID = e->Id;
  if (mask & MIIM_SUBMENU)
    info->hSubMenu = e->SubMenu;
  if (mask & MIIM_CHECKMARKS)
  {
    info->hbmpChecked = e->BmpChecked;
    info->hbmpUnchecked = e->BmpUnchecked;
  }
  if (mask & MIIM_DATA)
    info->dwItemData = e->Data;
  if (mask & MIIM_BITMAP)
    info->hbmpItem = e->BmpItem;
  if ((mask & MIIM_STRING) || ((mask & MIIM_TYPE) && !e->IsSeparator()))
  {
    // without a buffer: the length; else the copied length
    if (!info->dwTypeData || info->cch == 0)
      info->cch = (UINT)length(e->Text);
    else
      info->cch = (UINT)copy(e->Text, info->dwTypeData, (int)info->cch);
  }
  return TRUE;
}

template <class TInfo>
static void ApplyItemInfo(CMenuEntry &e, const TInfo *info, QString (*text)(const TInfo *))
{
  const UINT mask = info->fMask;
  if (mask & (MIIM_TYPE | MIIM_FTYPE))
    e.Type = info->fType;
  if (mask & MIIM_STATE)
    e.State = info->fState;
  if (mask & MIIM_ID)
    e.Id = info->wID;
  // a replaced submenu is not destroyed, as on Windows
  if (mask & MIIM_SUBMENU)
    e.SubMenu = Live(info->hSubMenu);
  if (mask & MIIM_CHECKMARKS)
  {
    e.BmpChecked = info->hbmpChecked;
    e.BmpUnchecked = info->hbmpUnchecked;
  }
  if (mask & MIIM_DATA)
    e.Data = info->dwItemData;
  if (mask & MIIM_BITMAP)
    e.BmpItem = info->hbmpItem;
  if ((mask & MIIM_STRING) || ((mask & MIIM_TYPE) && !(info->fType & MFT_SEPARATOR)))
    e.Text = text(info);
}

static QString TextW(const MENUITEMINFOW *info) { return FromW(info->dwTypeData); }
static QString TextA(const MENUITEMINFOA *info) { return FromA(info->dwTypeData); }

template <class TInfo>
static BOOL InsertItemImpl(HMENU menu, UINT item, BOOL byPosition, const TInfo *info, QString (*text)(const TInfo *))
{
  menu = Live(menu);
  if (!menu || !info)
    return FALSE;
  HMENU owner = menu;
  unsigned pos = (unsigned)menu->Items.size();
  if (byPosition)
  {
    if (item < pos)
      pos = item;
  }
  else if (!FindItem(menu, item, MF_BYCOMMAND, owner, pos))
  {
    owner = menu;
    pos = (unsigned)menu->Items.size();
  }
  CMenuEntry e;
  ApplyItemInfo(e, info, text);
  owner->Items.insert(owner->Items.begin() + pos, e);
  return TRUE;
}

template <class TChar>
static BOOL InsertMenuImpl(HMENU menu, UINT position, UINT flags, UINT_PTR idNewItem, const TChar *newItem,
    QString (*text)(const TChar *), bool append)
{
  menu = Live(menu);
  if (!menu)
    return FALSE;
  HMENU owner = menu;
  unsigned pos = (unsigned)menu->Items.size();
  if (!append)
  {
    if (flags & MF_BYPOSITION)
    {
      if (position < pos)
        pos = position;
    }
    else if (!FindItem(menu, position, MF_BYCOMMAND, owner, pos))
      return FALSE;
  }
  CMenuEntry e;
  FromMenuFlags(flags, e);
  if (flags & MF_POPUP)
    e.SubMenu = Live((HMENU)idNewItem);
  else
    e.Id = (UINT)idNewItem;
  if (!(flags & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)) && newItem)
    e.Text = text(newItem);
  owner->Items.insert(owner->Items.begin() + pos, e);
  return TRUE;
}

static QString TextOfW(const WCHAR *s) { return FromW(s); }
static QString TextOfA(const CHAR *s) { return FromA(s); }

}

using namespace NShim;

// ---------- API ----------

HMENU CreateMenu()
{
  return OnGuiThread<HMENU>([]() { return NewMenu(false); });
}

HMENU CreatePopupMenu()
{
  return OnGuiThread<HMENU>([]() { return NewMenu(true); });
}

BOOL DestroyMenu(HMENU menu)
{
  return OnGuiThread<BOOL>([=]()
  {
    if (!Live(menu))
      return FALSE;
    if (menu->Window && FromHandle(menu->Window) && menu->Window->Menu == menu)
      SetWindowMenuBar(menu->Window, NULL);
    DestroyMenuTree(menu);
    return TRUE;
  });
}

BOOL IsMenu(HMENU menu)
{
  return OnGuiThread<BOOL>([=]() { return Live(menu) ? TRUE : FALSE; });
}

HMENU LoadMenuW(HINSTANCE /* instance */, LPCWSTR name)
{
  if (!IS_INTRESOURCE(name))
    return NULL;
  return OnGuiThread<HMENU>([=]() { return LoadMenuImpl((UInt32)(uintptr_t)name); });
}

HMENU LoadMenuA(HINSTANCE instance, LPCSTR name)
{
  return LoadMenuW(instance, (LPCWSTR)(const void *)name);
}

HMENU GetMenu(HWND wnd)
{
  return OnGuiThread<HMENU>([=]() -> HMENU
  {
    HWND w = FromHandle(wnd);
    return (w && !w->IsChild()) ? Live(w->Menu) : NULL;
  });
}

BOOL SetMenu(HWND wnd, HMENU menu)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w || w->IsChild() || (menu && !Live(menu)))
      return FALSE;
    SetWindowMenuBar(w, menu);
    return TRUE;
  });
}

BOOL DrawMenuBar(HWND wnd)
{
  return OnGuiThread<BOOL>([=]()
  {
    HWND w = FromHandle(wnd);
    if (!w)
      return FALSE;
    HMENU menu = Live(w->Menu);
    if (menu)
      BuildMenuBar(menu);
    return TRUE;
  });
}

int GetMenuItemCount(HMENU menu)
{
  return OnGuiThread<int>([=]() { return Live(menu) ? (int)menu->Items.size() : -1; });
}

UINT GetMenuItemID(HMENU menu, int pos)
{
  return OnGuiThread<UINT>([=]() -> UINT
  {
    if (!Live(menu) || pos < 0 || pos >= (int)menu->Items.size())
      return (UINT)-1;
    const CMenuEntry &e = menu->Items[(size_t)pos];
    return e.SubMenu ? (UINT)-1 : e.Id;
  });
}

UINT GetMenuState(HMENU menu, UINT id, UINT flags)
{
  return OnGuiThread<UINT>([=]() -> UINT
  {
    CMenuEntry *e = FindEntry(menu, id, flags);
    if (!e)
      return (UINT)-1;
    UINT r = StateToMenuFlags(*e);
    if (e->SubMenu && Live(e->SubMenu))
      r |= MF_POPUP | ((UINT)e->SubMenu->Items.size() << 8);
    return r;
  });
}

HMENU GetSubMenu(HMENU menu, int pos)
{
  return OnGuiThread<HMENU>([=]() -> HMENU
  {
    if (!Live(menu) || pos < 0 || pos >= (int)menu->Items.size())
      return NULL;
    return Live(menu->Items[(size_t)pos].SubMenu);
  });
}

int GetMenuStringW(HMENU menu, UINT item, LPWSTR s, int maxCount, UINT flags)
{
  return OnGuiThread<int>([=]()
  {
    CMenuEntry *e = FindEntry(menu, item, flags);
    if (!e)
      return 0;
    if (!s || maxCount <= 0)
      return LengthW(e->Text);
    return CopyToW(e->Text, s, maxCount);
  });
}

int GetMenuStringA(HMENU menu, UINT item, LPSTR s, int maxCount, UINT flags)
{
  return OnGuiThread<int>([=]()
  {
    CMenuEntry *e = FindEntry(menu, item, flags);
    if (!e)
      return 0;
    if (!s || maxCount <= 0)
      return LengthA(e->Text);
    return CopyToA(e->Text, s, maxCount);
  });
}

BOOL GetMenuItemInfoW(HMENU menu, UINT item, BOOL byPosition, LPMENUITEMINFOW info)
{
  return OnGuiThread<BOOL>([=]()
  {
    return GetItemInfoImpl<MENUITEMINFOW, WCHAR>(menu, item, byPosition, info, CopyToW, LengthW);
  });
}

BOOL GetMenuItemInfoA(HMENU menu, UINT item, BOOL byPosition, LPMENUITEMINFOA info)
{
  return OnGuiThread<BOOL>([=]()
  {
    return GetItemInfoImpl<MENUITEMINFOA, CHAR>(menu, item, byPosition, info, CopyToA, LengthA);
  });
}

BOOL SetMenuItemInfoW(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOW info)
{
  return OnGuiThread<BOOL>([=]()
  {
    CMenuEntry *e = FindEntry(menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND);
    if (!e || !info)
      return FALSE;
    ApplyItemInfo(*e, info, TextW);
    return TRUE;
  });
}

BOOL SetMenuItemInfoA(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOA info)
{
  return OnGuiThread<BOOL>([=]()
  {
    CMenuEntry *e = FindEntry(menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND);
    if (!e || !info)
      return FALSE;
    ApplyItemInfo(*e, info, TextA);
    return TRUE;
  });
}

BOOL InsertMenuItemW(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOW info)
{
  return OnGuiThread<BOOL>([=]() { return InsertItemImpl(menu, item, byPosition, info, TextW); });
}

BOOL InsertMenuItemA(HMENU menu, UINT item, BOOL byPosition, LPCMENUITEMINFOA info)
{
  return OnGuiThread<BOOL>([=]() { return InsertItemImpl(menu, item, byPosition, info, TextA); });
}

BOOL AppendMenuW(HMENU menu, UINT flags, UINT_PTR idNewItem, LPCWSTR newItem)
{
  return OnGuiThread<BOOL>([=]() { return InsertMenuImpl(menu, 0, flags, idNewItem, newItem, TextOfW, true); });
}

BOOL AppendMenuA(HMENU menu, UINT flags, UINT_PTR idNewItem, LPCSTR newItem)
{
  return OnGuiThread<BOOL>([=]() { return InsertMenuImpl(menu, 0, flags, idNewItem, newItem, TextOfA, true); });
}

BOOL InsertMenuW(HMENU menu, UINT position, UINT flags, UINT_PTR idNewItem, LPCWSTR newItem)
{
  return OnGuiThread<BOOL>([=]() { return InsertMenuImpl(menu, position, flags, idNewItem, newItem, TextOfW, false); });
}

BOOL InsertMenuA(HMENU menu, UINT position, UINT flags, UINT_PTR idNewItem, LPCSTR newItem)
{
  return OnGuiThread<BOOL>([=]() { return InsertMenuImpl(menu, position, flags, idNewItem, newItem, TextOfA, false); });
}

template <class TChar>
static BOOL ModifyMenuImpl(HMENU menu, UINT position, UINT flags, UINT_PTR idNewItem, const TChar *newItem,
    QString (*text)(const TChar *))
{
  CMenuEntry *e = FindEntry(menu, position, flags);
  if (!e)
    return FALSE;
  CMenuEntry n;
  FromMenuFlags(flags, n);
  if (flags & MF_POPUP)
    n.SubMenu = Live((HMENU)idNewItem);
  else
  {
    n.Id = (UINT)idNewItem;
    if (e->SubMenu)
      DestroyMenuTree(e->SubMenu);
  }
  if (!(flags & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)) && newItem)
    n.Text = text(newItem);
  *e = n;
  return TRUE;
}

BOOL ModifyMenuW(HMENU menu, UINT position, UINT flags, UINT_PTR idNewItem, LPCWSTR newItem)
{
  return OnGuiThread<BOOL>([=]() { return ModifyMenuImpl(menu, position, flags, idNewItem, newItem, TextOfW); });
}

BOOL ModifyMenuA(HMENU menu, UINT position, UINT flags, UINT_PTR idNewItem, LPCSTR newItem)
{
  return OnGuiThread<BOOL>([=]() { return ModifyMenuImpl(menu, position, flags, idNewItem, newItem, TextOfA); });
}

static BOOL RemoveItemImpl(HMENU menu, UINT position, UINT flags, bool destroySubMenu)
{
  HMENU owner = NULL;
  unsigned pos = 0;
  if (!FindItem(menu, position, flags, owner, pos))
    return FALSE;
  const HMENU sub = owner->Items[pos].SubMenu;
  owner->Items.erase(owner->Items.begin() + pos);
  if (destroySubMenu && sub)
    DestroyMenuTree(sub);
  return TRUE;
}

BOOL RemoveMenu(HMENU menu, UINT position, UINT flags)
{
  return OnGuiThread<BOOL>([=]() { return RemoveItemImpl(menu, position, flags, false); });
}

BOOL DeleteMenu(HMENU menu, UINT position, UINT flags)
{
  return OnGuiThread<BOOL>([=]() { return RemoveItemImpl(menu, position, flags, true); });
}

DWORD CheckMenuItem(HMENU menu, UINT id, UINT check)
{
  return OnGuiThread<DWORD>([=]() -> DWORD
  {
    CMenuEntry *e = FindEntry(menu, id, check);
    if (!e)
      return (DWORD)-1;
    const DWORD prev = (e->State & MFS_CHECKED) ? MF_CHECKED : MF_UNCHECKED;
    if (check & MF_CHECKED)
      e->State |= MFS_CHECKED;
    else
      e->State &= ~(UINT)MFS_CHECKED;
    return prev;
  });
}

BOOL CheckMenuRadioItem(HMENU menu, UINT first, UINT last, UINT check, UINT flags)
{
  return OnGuiThread<BOOL>([=]()
  {
    HMENU owner = NULL;
    unsigned pos = 0;
    if (!FindItem(menu, first, flags, owner, pos))
      return FALSE;
    bool found = false;
    for (unsigned i = 0; i < owner->Items.size(); i++)
    {
      CMenuEntry &e = owner->Items[i];
      bool inRange;
      bool isCheck;
      if (flags & MF_BYPOSITION)
      {
        inRange = (i >= first && i <= last);
        isCheck = (i == check);
      }
      else
      {
        inRange = !e.SubMenu && !e.IsSeparator() && e.Id >= first && e.Id <= last;
        isCheck = (e.Id == check);
      }
      if (!inRange)
        continue;
      if (isCheck)
      {
        e.Type |= MFT_RADIOCHECK;
        e.State |= MFS_CHECKED;
        found = true;
      }
      else
        e.State &= ~(UINT)MFS_CHECKED;
    }
    return found ? TRUE : FALSE;
  });
}

BOOL EnableMenuItem(HMENU menu, UINT id, UINT enable)
{
  return OnGuiThread<BOOL>([=]() -> BOOL
  {
    CMenuEntry *e = FindEntry(menu, id, enable);
    if (!e)
      return (BOOL)-1;
    const BOOL prev = (e->State & MFS_GRAYED) ? (MF_GRAYED | MF_DISABLED) : MF_ENABLED;
    if (enable & (MF_GRAYED | MF_DISABLED))
      e->State |= MFS_GRAYED;
    else
      e->State &= ~(UINT)MFS_GRAYED;
    return prev;
  });
}

BOOL SetMenuDefaultItem(HMENU menu, UINT item, UINT byPosition)
{
  return OnGuiThread<BOOL>([=]()
  {
    if (!Live(menu))
      return FALSE;
    for (CMenuEntry &e : menu->Items)
      e.State &= ~(UINT)MFS_DEFAULT;
    if (item == (UINT)-1)
      return TRUE;
    CMenuEntry *e = FindEntry(menu, item, byPosition ? MF_BYPOSITION : MF_BYCOMMAND);
    if (!e)
      return FALSE;
    e->State |= MFS_DEFAULT;
    return TRUE;
  });
}

BOOL SetMenuItemBitmaps(HMENU menu, UINT position, UINT flags, HBITMAP unchecked, HBITMAP checked)
{
  return OnGuiThread<BOOL>([=]()
  {
    CMenuEntry *e = FindEntry(menu, position, flags);
    if (!e)
      return FALSE;
    e->BmpUnchecked = unchecked;
    e->BmpChecked = checked;
    return TRUE;
  });
}

static QPointer<QMenu> g_TrackedMenu;

BOOL TrackPopupMenuEx(HMENU menu, UINT flags, int x, int y, HWND wnd, LPTPMPARAMS /* params */)
{
  return OnGuiThread<BOOL>([=]() -> BOOL
  {
    if (!Live(menu))
      return FALSE;
    HWND w = FromHandle(wnd);
    QWidget *parent = (w && w->Widget) ? w->Widget->window() : QApplication::activeWindow();
    CMenuContext ctx;
    ctx.Window = w;
    ctx.Notify = !(flags & TPM_NONOTIFY);
    ctx.ReturnCmd = (flags & TPM_RETURNCMD) != 0;
    ctx.Chosen = 0;
    SendInitMenuPopup(ctx, menu, 0, false);
    if (!Live(menu))
      return FALSE;
    QMenu qm(parent);
    FillQMenuWithContext(&qm, menu, &ctx);
    if (qm.isEmpty())
      return FALSE;
    QPoint pos(x, y);
    const QSize size = qm.sizeHint();
    if (flags & TPM_RIGHTALIGN)
      pos.rx() -= size.width();
    else if (flags & TPM_CENTERALIGN)
      pos.rx() -= size.width() / 2;
    if (flags & TPM_BOTTOMALIGN)
      pos.ry() -= size.height();
    else if (flags & TPM_VCENTERALIGN)
      pos.ry() -= size.height() / 2;
    QPointer<QMenu> prev = g_TrackedMenu;
    g_TrackedMenu = &qm;
    qm.exec(pos);
    g_TrackedMenu = prev;
    if (ctx.ReturnCmd)
      return (BOOL)ctx.Chosen;
    return TRUE;
  });
}

BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int /* reserved */, HWND wnd, const RECT * /* rect */)
{
  return TrackPopupMenuEx(menu, flags, x, y, wnd, NULL);
}

BOOL EndMenu()
{
  return OnGuiThread<BOOL>([]()
  {
    if (g_TrackedMenu)
      g_TrackedMenu->close();
    return TRUE;
  });
}
