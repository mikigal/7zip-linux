// RcTables.cpp -- lookups in the generated resource tables.

#include "Win32Compat.h"

#include "RcTables.h"

namespace NRc {

template <class T>
static const T *FindById(const T *items, unsigned num, UInt32 id)
{
  unsigned left = 0, right = num;
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const UInt32 midId = items[mid].Id;
    if (id == midId)
      return &items[mid];
    if (id < midId)
      right = mid;
    else
      left = mid + 1;
  }
  return NULL;
}

const CDialog *FindDialog(UInt32 id)
{
  return FindById(g_Resources.Dialogs, g_Resources.NumDialogs, id);
}

const CMenu *FindMenu(UInt32 id)
{
  return FindById(g_Resources.Menus, g_Resources.NumMenus, id);
}

const CAccelTable *FindAccelTable(UInt32 id)
{
  return FindById(g_Resources.AccelTables, g_Resources.NumAccelTables, id);
}

const wchar_t *FindString(UInt32 id)
{
  const CString *s = FindById(g_Resources.Strings, g_Resources.NumStrings, id);
  return s ? s->Text : NULL;
}

}
