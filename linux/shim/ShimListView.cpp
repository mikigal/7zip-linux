// ShimListView.cpp -- the list view common control (SysListView32) in report
// view, as dialogs use it, over QTreeWidget.

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTreeWidget>

#include <algorithm>

#include "ShimResources.h"
#include "ShimWindow.h"

namespace NShim {

enum
{
  kRole_Param = Qt::UserRole,       // LPARAM of the item
  kRole_Image = Qt::UserRole + 1,   // image index
  kRole_State = Qt::UserRole + 2    // state of the last LVN_ITEMCHANGED
};

class CListViewWnd: public HWND__
{
public:
  DWORD LvExStyle;
  HIMAGELIST Small;
  HIMAGELIST Normal;
  HIMAGELIST StateList;
  std::vector<int> Formats;    // LVCFMT_* of the columns
  int Suppress;                // > 0 while state changes must not notify
  int SelectionMark;

  CListViewWnd(): HWND__(kKind_ListView), LvExStyle(0), Small(NULL), Normal(NULL), StateList(NULL),
      Suppress(0), SelectionMark(-1) {}

  QTreeWidget *Tree() const { return static_cast<QTreeWidget *>(Widget.data()); }
  int Count() const { return Tree()->topLevelItemCount(); }
  QTreeWidgetItem *Item(int i) const
    { return (i >= 0 && i < Count()) ? Tree()->topLevelItem(i) : NULL; }
  int NumColumns() const { return Tree()->columnCount(); }
  bool HasCheckBoxes() const { return (LvExStyle & LVS_EX_CHECKBOXES) != 0; }

  Qt::Alignment ColumnAlignment(int col) const
  {
    const int fmt = (col >= 0 && col < (int)Formats.size()) ? Formats[(size_t)col] : LVCFMT_LEFT;
    switch (fmt & LVCFMT_JUSTIFYMASK)
    {
      case LVCFMT_RIGHT: return Qt::AlignRight | Qt::AlignVCenter;
      case LVCFMT_CENTER: return Qt::AlignHCenter | Qt::AlignVCenter;
      default: return Qt::AlignLeft | Qt::AlignVCenter;
    }
  }

  UINT CurrentState(QTreeWidgetItem *item) const
  {
    UINT s = 0;
    if (item->isSelected())
      s |= LVIS_SELECTED;
    if (Tree()->currentItem() == item)
      s |= LVIS_FOCUSED;
    if (HasCheckBoxes())
      s |= INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 2 : 1);
    return s;
  }

  void SetIcon(QTreeWidgetItem *item, int image)
  {
    item->setData(0, kRole_Image, image);
    HIMAGELIST list = Small ? Small : Normal;
    if (list && image >= 0 && image < (int)list->Icons.size())
      item->setIcon(0, list->Icons[image]);
    else
      item->setIcon(0, QIcon());
  }

  void ApplyCheckBoxes(QTreeWidgetItem *item)
  {
    if (HasCheckBoxes())
    {
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      if (!item->data(0, Qt::CheckStateRole).isValid())
        item->setCheckState(0, Qt::Unchecked);
    }
    else
    {
      item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
      item->setData(0, Qt::CheckStateRole, QVariant());
    }
  }

  // Sends LVN_ITEMCHANGED if the state of the item differs from the state of
  // the last notification.
  void SyncState(QTreeWidgetItem *item)
  {
    if (!item)
      return;
    const UINT newState = CurrentState(item);
    const UINT oldState = (UINT)item->data(0, kRole_State).toUInt();
    if (newState == oldState)
      return;
    item->setData(0, kRole_State, newState);
    if (Suppress > 0)
      return;
    NMLISTVIEW nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = Tree()->indexOfTopLevelItem(item);
    nm.uChanged = LVIF_STATE;
    nm.uOldState = oldState;
    nm.uNewState = newState;
    nm.lParam = (LPARAM)item->data(0, kRole_Param).toLongLong();
    NotifyParent(&nm.hdr, LVN_ITEMCHANGED);
  }

  void SyncAllStatesSilently()
  {
    for (int i = 0; i < Count(); i++)
    {
      QTreeWidgetItem *item = Item(i);
      item->setData(0, kRole_State, CurrentState(item));
    }
  }

  void SetState(QTreeWidgetItem *item, UINT state, UINT mask)
  {
    QTreeWidget *t = Tree();
    if (mask & LVIS_FOCUSED)
    {
      if (state & LVIS_FOCUSED)
        t->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
      else if (t->currentItem() == item)
        t->setCurrentItem(NULL, 0, QItemSelectionModel::NoUpdate);
    }
    if (mask & LVIS_SELECTED)
    {
      const bool sel = (state & LVIS_SELECTED) != 0;
      if (sel && t->selectionMode() == QAbstractItemView::SingleSelection)
        t->clearSelection();
      item->setSelected(sel);
      if (sel)
        SelectionMark = t->indexOfTopLevelItem(item);
    }
    if ((mask & LVIS_STATEIMAGEMASK) && HasCheckBoxes())
    {
      const UINT index = (state & LVIS_STATEIMAGEMASK) >> 12;
      if (index != 0)
        item->setCheckState(0, index == 2 ? Qt::Checked : Qt::Unchecked);
    }
    SyncState(item);
  }

  // LVITEMW fields to an item; iSubItem selects the column of the text.
  void ApplyItem(QTreeWidgetItem *item, const LVITEMW *lv)
  {
    if (lv->mask & LVIF_TEXT)
    {
      const int col = lv->iSubItem;
      if (col >= 0 && col < NumColumns())
      {
        const QString s = (lv->pszText == LPSTR_TEXTCALLBACKW) ? QString() : FromW(lv->pszText);
        item->setText(col, s);
        item->setTextAlignment(col, ColumnAlignment(col));
      }
    }
    if (lv->iSubItem != 0)
      return;
    if (lv->mask & LVIF_PARAM)
      item->setData(0, kRole_Param, (qlonglong)lv->lParam);
    if (lv->mask & LVIF_IMAGE)
      SetIcon(item, lv->iImage);
    if (lv->mask & LVIF_STATE)
      SetState(item, lv->state, lv->stateMask);
  }

  int InsertItem(const LVITEMW *lv)
  {
    QTreeWidget *t = Tree();
    if (lv->iSubItem != 0)
      return -1;
    int pos = lv->iItem;
    if (pos < 0 || pos > Count())
      pos = Count();
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setData(0, kRole_Param, (qlonglong)0);
    item->setData(0, kRole_Image, -1);
    item->setData(0, kRole_State, 0u);
    for (int c = 0; c < NumColumns(); c++)
      item->setTextAlignment(c, ColumnAlignment(c));
    ApplyCheckBoxes(item);
    Suppress++;
    t->insertTopLevelItem(pos, item);
    item->setData(0, kRole_State, CurrentState(item));
    LVITEMW copy = *lv;
    copy.mask &= ~(UINT)LVIF_STATE;
    ApplyItem(item, &copy);
    Suppress--;
    if (lv->mask & LVIF_STATE)
      SetState(item, lv->state, lv->stateMask);
    return pos;
  }

  bool GetItem(LVITEMW *lv) const
  {
    QTreeWidgetItem *item = Item(lv->iItem);
    if (!item)
      return false;
    if (lv->mask & LVIF_TEXT)
    {
      const int col = lv->iSubItem;
      const QString s = (col >= 0 && col < NumColumns()) ? item->text(col) : QString();
      CopyToW(s, lv->pszText, lv->cchTextMax);
    }
    if (lv->iSubItem == 0)
    {
      if (lv->mask & LVIF_PARAM)
        lv->lParam = (LPARAM)item->data(0, kRole_Param).toLongLong();
      if (lv->mask & LVIF_IMAGE)
        lv->iImage = item->data(0, kRole_Image).toInt();
      if (lv->mask & LVIF_STATE)
        lv->state = CurrentState(item) & lv->stateMask;
    }
    return true;
  }

  int InsertColumn(int index, const LVCOLUMNW *c)
  {
    QTreeWidget *t = Tree();
    const int n = NumColumns();
    if (index < 0 || index > n)
      index = n;
    // QTreeWidget starts with one column; the first insertion takes it over
    const bool first = (Formats.empty());
    if (!first)
    {
      if (index == n)
        t->setColumnCount(n + 1);
      else
        t->model()->insertColumn(index);
    }
    Formats.insert(Formats.begin() + index, (c->mask & LVCF_FMT) ? c->fmt : LVCFMT_LEFT);
    if (first)
      index = 0;
    SetColumn(index, c);
    if (!(c->mask & LVCF_WIDTH))
      t->header()->resizeSection(index, 60);
    return index;
  }

  void SetColumn(int index, const LVCOLUMNW *c)
  {
    QTreeWidget *t = Tree();
    if (c->mask & LVCF_TEXT)
      t->headerItem()->setText(index, FromW(c->pszText));
    if (c->mask & LVCF_FMT)
    {
      Formats[(size_t)index] = c->fmt;
      t->headerItem()->setTextAlignment(index, ColumnAlignment(index));
      for (int i = 0; i < Count(); i++)
        Item(i)->setTextAlignment(index, ColumnAlignment(index));
    }
    if (c->mask & LVCF_WIDTH)
      SetColumnWidth(index, c->cx);
  }

  void SetColumnWidth(int col, int cx)
  {
    QTreeWidget *t = Tree();
    if (col < 0 || col >= NumColumns())
      return;
    if (cx == LVSCW_AUTOSIZE)
      t->resizeColumnToContents(col);
    else if (cx == LVSCW_AUTOSIZE_USEHEADER)
    {
      t->resizeColumnToContents(col);
      if (col == NumColumns() - 1)
      {
        // the last column fills the rest of the list
        int used = 0;
        for (int i = 0; i < col; i++)
          used += t->header()->sectionSize(i);
        const int rest = t->viewport()->width() - used;
        if (rest > t->header()->sectionSize(col))
          t->header()->resizeSection(col, rest);
      }
    }
    else
      t->header()->resizeSection(col, cx);
  }

  int NextItem(int start, UINT flags) const
  {
    const int n = Count();
    if (flags & (LVNI_ABOVE | LVNI_TOLEFT))
    {
      for (int i = (start < 0 ? n : start) - 1; i >= 0; i--)
        if (Matches(i, flags))
          return i;
      return -1;
    }
    for (int i = start + 1; i < n; i++)
      if (Matches(i, flags))
        return i;
    return -1;
  }

  bool Matches(int i, UINT flags) const
  {
    QTreeWidgetItem *item = Item(i);
    if ((flags & LVNI_SELECTED) && !item->isSelected())
      return false;
    if ((flags & LVNI_FOCUSED) && Tree()->currentItem() != item)
      return false;
    return true;
  }

  struct CSortEntry
  {
    QTreeWidgetItem *Item;
    LPARAM Param;
  };

  void Sort(PFNLVCOMPARE compare, LPARAM sortParam)
  {
    QTreeWidget *t = Tree();
    std::vector<CSortEntry> items;
    QTreeWidgetItem *current = t->currentItem();
    std::vector<QTreeWidgetItem *> selected;
    Suppress++;
    while (Count() > 0)
    {
      QTreeWidgetItem *item = Item(0);
      if (item->isSelected())
        selected.push_back(item);
      CSortEntry e = { t->takeTopLevelItem(0), (LPARAM)item->data(0, kRole_Param).toLongLong() };
      items.push_back(e);
    }
    std::stable_sort(items.begin(), items.end(), [=](const CSortEntry &a, const CSortEntry &b)
    {
      return compare(a.Param, b.Param, sortParam) < 0;
    });
    for (const CSortEntry &e : items)
      t->addTopLevelItem(e.Item);
    for (QTreeWidgetItem *item : selected)
      item->setSelected(true);
    if (current)
      t->setCurrentItem(current, 0, QItemSelectionModel::NoUpdate);
    Suppress--;
    SyncAllStatesSilently();
  }

  int HitTest(LVHITTESTINFO *info, bool sub) const
  {
    QTreeWidget *t = Tree();
    const QPoint p = QPoint(info->pt.x, info->pt.y) - t->viewport()->geometry().topLeft();
    const QModelIndex index = t->indexAt(p);
    info->iSubItem = 0;
    if (!index.isValid())
    {
      info->flags = LVHT_NOWHERE;
      info->iItem = -1;
      return -1;
    }
    info->flags = LVHT_ONITEMLABEL;
    info->iItem = index.row();
    if (sub)
      info->iSubItem = index.column();
    return index.row();
  }

  int ItemAt(const QPoint &viewportPos, int *column = NULL) const
  {
    const QModelIndex index = Tree()->indexAt(viewportPos);
    if (column)
      *column = index.isValid() ? index.column() : 0;
    return index.isValid() ? index.row() : -1;
  }

  void NotifyMouse(UINT code, const QPoint &viewportPos)
  {
    int col = 0;
    NMITEMACTIVATE nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = ItemAt(viewportPos, &col);
    nm.iSubItem = col;
    const QPoint p = viewportPos + Tree()->viewport()->geometry().topLeft();
    nm.ptAction.x = p.x();
    nm.ptAction.y = p.y();
    const Qt::KeyboardModifiers m = QApplication::keyboardModifiers();
    if (m & Qt::AltModifier)
      nm.uKeyFlags |= LVKF_ALT;
    if (m & Qt::ControlModifier)
      nm.uKeyFlags |= LVKF_CONTROL;
    if (m & Qt::ShiftModifier)
      nm.uKeyFlags |= LVKF_SHIFT;
    if (nm.iItem >= 0)
      nm.lParam = (LPARAM)Item(nm.iItem)->data(0, kRole_Param).toLongLong();
    NotifyParent(&nm.hdr, code);
  }

  bool OnEvent(QObject *watched, QEvent *event) override
  {
    QTreeWidget *t = Tree();
    if (watched == t->viewport())
    {
      switch (event->type())
      {
        case QEvent::MouseButtonRelease:
        {
          const QMouseEvent *e = static_cast<QMouseEvent *>(event);
          const QPoint pos = e->position().toPoint();
          if (e->button() == Qt::LeftButton)
          {
            NotifyMouse(NM_CLICK, pos);
            if ((LvExStyle & LVS_EX_ONECLICKACTIVATE) && FromHandle(this) && ItemAt(pos) >= 0)
              NotifyMouse(LVN_ITEMACTIVATE, pos);
          }
          else if (e->button() == Qt::RightButton)
          {
            NotifyMouse(NM_RCLICK, pos);
            if (FromHandle(this))
            {
              const QPoint g = e->globalPosition().toPoint();
              Send(this, WM_CONTEXTMENU, (WPARAM)this, MAKELPARAM(g.x(), g.y()));
            }
          }
          return false;
        }
        case QEvent::MouseButtonDblClick:
        {
          const QMouseEvent *e = static_cast<QMouseEvent *>(event);
          const QPoint pos = e->position().toPoint();
          if (e->button() == Qt::LeftButton)
          {
            NotifyMouse(NM_DBLCLK, pos);
            if (FromHandle(this) && ItemAt(pos) >= 0 && !(LvExStyle & LVS_EX_ONECLICKACTIVATE))
              NotifyMouse(LVN_ITEMACTIVATE, pos);
          }
          else if (e->button() == Qt::RightButton)
            NotifyMouse(NM_RDBLCLK, pos);
          return false;
        }
        default:
          return false;
      }
    }
    if (watched == t && event->type() == QEvent::KeyPress)
    {
      const UINT vk = VirtualKeyFromQt(static_cast<QKeyEvent *>(event));
      if (vk != 0)
      {
        NMLVKEYDOWN nm;
        memset(&nm, 0, sizeof(nm));
        nm.wVKey = (WORD)vk;
        NotifyParent(&nm.hdr, LVN_KEYDOWN);
      }
      return false;
    }
    return false;
  }

  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override;
};

LRESULT CListViewWnd::Proc(UINT message, WPARAM wParam, LPARAM lParam)
{
  QTreeWidget *t = Tree();
  const int index = (int)(INT_PTR)wParam;
  switch (message)
  {
    case LVM_GETITEMCOUNT:
      return Count();
    case LVM_INSERTITEMW:
      return InsertItem((const LVITEMW *)lParam);
    case LVM_SETITEMW:
    {
      const LVITEMW *lv = (const LVITEMW *)lParam;
      QTreeWidgetItem *item = Item(lv->iItem);
      if (!item)
        return FALSE;
      ApplyItem(item, lv);
      return TRUE;
    }
    case LVM_GETITEMW:
      return GetItem((LVITEMW *)lParam) ? TRUE : FALSE;
    case LVM_SETITEMTEXTW:
    {
      QTreeWidgetItem *item = Item(index);
      if (!item)
        return FALSE;
      LVITEMW lv = *(const LVITEMW *)lParam;
      lv.mask = LVIF_TEXT;
      ApplyItem(item, &lv);
      return TRUE;
    }
    case LVM_GETITEMTEXTW:
    {
      LVITEMW *lv = (LVITEMW *)lParam;
      QTreeWidgetItem *item = Item(index);
      const QString s = (item && lv->iSubItem >= 0 && lv->iSubItem < NumColumns()) ? item->text(lv->iSubItem) : QString();
      return CopyToW(s, lv->pszText, lv->cchTextMax);
    }
    case LVM_DELETEITEM:
    {
      QTreeWidgetItem *item = Item(index);
      if (!item)
        return FALSE;
      Suppress++;
      delete item;
      Suppress--;
      SyncState(t->currentItem());
      return TRUE;
    }
    case LVM_DELETEALLITEMS:
      Suppress++;
      t->clear();
      Suppress--;
      SelectionMark = -1;
      return TRUE;
    case LVM_SETITEMCOUNT:
      return TRUE;
    case LVM_GETNEXTITEM:
      return NextItem(index, LOWORD(lParam));
    case LVM_GETSELECTEDCOUNT:
      return (LRESULT)t->selectedItems().size();
    case LVM_GETSELECTIONMARK:
      return SelectionMark;
    case LVM_SETSELECTIONMARK:
    {
      const int prev = SelectionMark;
      SelectionMark = (int)lParam;
      return prev;
    }
    case LVM_SETITEMSTATE:
    {
      const LVITEMW *lv = (const LVITEMW *)lParam;
      if (index == -1)
      {
        for (int i = 0; i < Count(); i++)
          SetState(Item(i), lv->state, lv->stateMask);
        return TRUE;
      }
      QTreeWidgetItem *item = Item(index);
      if (!item)
        return FALSE;
      SetState(item, lv->state, lv->stateMask);
      return TRUE;
    }
    case LVM_GETITEMSTATE:
    {
      QTreeWidgetItem *item = Item(index);
      return item ? (LRESULT)(CurrentState(item) & (UINT)lParam) : 0;
    }
    case LVM_INSERTCOLUMNW:
      return InsertColumn(index, (const LVCOLUMNW *)lParam);
    case LVM_SETCOLUMNW:
      if (index < 0 || index >= (int)Formats.size())
        return FALSE;
      SetColumn(index, (const LVCOLUMNW *)lParam);
      return TRUE;
    case LVM_GETCOLUMNW:
    {
      LVCOLUMNW *c = (LVCOLUMNW *)lParam;
      if (index < 0 || index >= (int)Formats.size())
        return FALSE;
      if (c->mask & LVCF_FMT)
        c->fmt = Formats[(size_t)index];
      if (c->mask & LVCF_WIDTH)
        c->cx = t->header()->sectionSize(index);
      if (c->mask & LVCF_TEXT)
        CopyToW(t->headerItem()->text(index), c->pszText, c->cchTextMax);
      if (c->mask & LVCF_SUBITEM)
        c->iSubItem = index;
      if (c->mask & LVCF_ORDER)
        c->iOrder = t->header()->visualIndex(index);
      return TRUE;
    }
    case LVM_DELETECOLUMN:
      if (index < 0 || index >= (int)Formats.size())
        return FALSE;
      Formats.erase(Formats.begin() + index);
      if (NumColumns() > 1)
        t->model()->removeColumn(index);
      else
        t->headerItem()->setText(0, QString());
      return TRUE;
    case LVM_SETCOLUMNWIDTH:
      SetColumnWidth(index, (short)LOWORD(lParam));
      return TRUE;
    case LVM_GETCOLUMNWIDTH:
      return (index >= 0 && index < NumColumns()) ? t->header()->sectionSize(index) : 0;
    case LVM_SETCOLUMNORDERARRAY:
    {
      const int *order = (const int *)lParam;
      for (int v = 0; v < (int)wParam && v < NumColumns(); v++)
        t->header()->moveSection(t->header()->visualIndex(order[v]), v);
      return TRUE;
    }
    case LVM_GETCOLUMNORDERARRAY:
    {
      int *order = (int *)lParam;
      for (int v = 0; v < (int)wParam && v < NumColumns(); v++)
        order[v] = t->header()->logicalIndex(v);
      return TRUE;
    }
    case LVM_SORTITEMS:
      Sort((PFNLVCOMPARE)lParam, (LPARAM)wParam);
      return TRUE;
    case LVM_SETEXTENDEDLISTVIEWSTYLE:
    {
      const DWORD prev = LvExStyle;
      const DWORD mask = wParam ? (DWORD)wParam : 0xFFFFFFFF;
      LvExStyle = (LvExStyle & ~mask) | ((DWORD)lParam & mask);
      if ((prev ^ LvExStyle) & LVS_EX_CHECKBOXES)
      {
        Suppress++;
        for (int i = 0; i < Count(); i++)
          ApplyCheckBoxes(Item(i));
        Suppress--;
        SyncAllStatesSilently();
      }
      t->setAllColumnsShowFocus(true);
      return prev;
    }
    case LVM_GETEXTENDEDLISTVIEWSTYLE:
      return LvExStyle;
    case LVM_SETIMAGELIST:
    {
      HIMAGELIST prev = NULL;
      HIMAGELIST list = (HIMAGELIST)lParam;
      if (wParam == LVSIL_SMALL)
      {
        prev = Small;
        Small = list;
        if (list)
          t->setIconSize(QSize(list->Cx, list->Cy));
      }
      else if (wParam == LVSIL_NORMAL)
      {
        prev = Normal;
        Normal = list;
      }
      else if (wParam == LVSIL_STATE)
      {
        prev = StateList;
        StateList = list;
      }
      for (int i = 0; i < Count(); i++)
        SetIcon(Item(i), Item(i)->data(0, kRole_Image).toInt());
      return (LRESULT)prev;
    }
    case LVM_GETIMAGELIST:
      return (LRESULT)(wParam == LVSIL_SMALL ? Small : wParam == LVSIL_NORMAL ? Normal : StateList);
    case LVM_ENSUREVISIBLE:
      if (QTreeWidgetItem *item = Item(index))
        t->scrollToItem(item);
      return TRUE;
    case LVM_GETITEMRECT:
    {
      RECT *r = (RECT *)lParam;
      QTreeWidgetItem *item = Item(index);
      if (!item || !r)
        return FALSE;
      const int code = r->left;
      QRect q = t->visualItemRect(item);
      const QRect col0 = t->visualRect(t->model()->index(index, 0));
      const int icon = t->iconSize().width() > 0 ? t->iconSize().width() + 4 : 0;
      if (code == LVIR_ICON)
        q = QRect(col0.left(), col0.top(), icon, col0.height());
      else if (code == LVIR_LABEL)
        q = QRect(col0.left() + icon, col0.top(), col0.width() - icon, col0.height());
      q.translate(t->viewport()->geometry().topLeft());
      r->left = q.left();
      r->top = q.top();
      r->right = q.left() + q.width();
      r->bottom = q.top() + q.height();
      return TRUE;
    }
    case LVM_HITTEST:
      return HitTest((LVHITTESTINFO *)lParam, false);
    case LVM_SUBITEMHITTEST:
      return HitTest((LVHITTESTINFO *)lParam, true);
    case LVM_REDRAWITEMS:
    case LVM_UPDATE:
      t->viewport()->update();
      return TRUE;
    case LVM_GETTOPINDEX:
    {
      const QModelIndex i = t->indexAt(QPoint(1, 1));
      return i.isValid() ? i.row() : 0;
    }
    case LVM_GETCOUNTPERPAGE:
    {
      const int h = Count() > 0 ? t->visualItemRect(Item(0)).height() : t->fontMetrics().height() + 4;
      return h > 0 ? t->viewport()->height() / h : 0;
    }
    case LVM_SCROLL:
      t->horizontalScrollBar()->setValue(t->horizontalScrollBar()->value() + index);
      t->verticalScrollBar()->setValue(t->verticalScrollBar()->value() + (int)lParam / (t->fontMetrics().height() + 4));
      return TRUE;
    case LVM_FINDITEMW:
    {
      const LVFINDINFOW *f = (const LVFINDINFOW *)lParam;
      const QString s = (f->flags & LVFI_STRING) ? FromW(f->psz) : QString();
      const int n = Count();
      for (int k = 1; k <= n; k++)
      {
        int i = index + k;
        if (i >= n)
        {
          if (!(f->flags & LVFI_WRAP))
            break;
          i -= n;
        }
        QTreeWidgetItem *item = Item(i);
        if (f->flags & LVFI_PARAM)
        {
          if ((LPARAM)item->data(0, kRole_Param).toLongLong() == f->lParam)
            return i;
        }
        else if (f->flags & LVFI_PARTIAL)
        {
          if (item->text(0).startsWith(s, Qt::CaseInsensitive))
            return i;
        }
        else if (QString::compare(item->text(0), s, Qt::CaseInsensitive) == 0)
          return i;
      }
      return -1;
    }
    case LVM_GETSTRINGWIDTHW:
      return t->fontMetrics().horizontalAdvance(FromW((LPCWSTR)lParam));
    case LVM_SETUNICODEFORMAT:
    case LVM_GETUNICODEFORMAT:
      return TRUE;
    case LVM_GETBKCOLOR:
    case LVM_GETTEXTBKCOLOR:
    {
      const QColor c = t->palette().color(QPalette::Base);
      return RGB(c.red(), c.green(), c.blue());
    }
    case LVM_GETTEXTCOLOR:
    {
      const QColor c = t->palette().color(QPalette::Text);
      return RGB(c.red(), c.green(), c.blue());
    }
    case LVM_SETBKCOLOR:
    case LVM_SETTEXTCOLOR:
    case LVM_SETTEXTBKCOLOR:
      // the theme decides the colors
      return TRUE;
    case LVM_GETEDITCONTROL:
    case LVM_EDITLABELW:
    case LVM_GETHEADER:
      return 0;
    case WM_GETDLGCODE:
      return DLGC_WANTARROWS | DLGC_WANTCHARS;
    default:
      return DefWindowProcImpl(this, message, wParam, lParam);
  }
}

HWND CreateListView(DWORD style, DWORD exStyle, QWidget *parent)
{
  CListViewWnd *w = new CListViewWnd();
  w->Style = style;
  QTreeWidget *t = new QTreeWidget(parent);
  RegisterWindow(w, t);
  t->setRootIsDecorated(false);
  t->setItemsExpandable(false);
  t->setUniformRowHeights(true);
  t->setAllColumnsShowFocus(true);
  t->setSortingEnabled(false);
  t->setColumnCount(1);
  t->headerItem()->setText(0, QString());
  t->setHeaderHidden((style & LVS_NOCOLUMNHEADER) != 0);
  t->header()->setStretchLastSection(false);
  t->header()->setSectionsClickable(!(style & LVS_NOSORTHEADER));
  t->header()->setSectionsMovable(false);
  t->setSelectionMode((style & LVS_SINGLESEL) ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
  t->setSelectionBehavior(QAbstractItemView::SelectRows);
  t->setEditTriggers(QAbstractItemView::NoEditTriggers);
  t->setTextElideMode(Qt::ElideRight);
  if (!(exStyle & WS_EX_CLIENTEDGE) && !(style & WS_BORDER))
    t->setFrameShape(QFrame::NoFrame);
  t->setFocusPolicy((style & WS_TABSTOP) ? Qt::StrongFocus : Qt::ClickFocus);
  t->installEventFilter(w->Proxy);
  t->viewport()->installEventFilter(w->Proxy);

  Connect(w, t->selectionModel(), &QItemSelectionModel::selectionChanged,
      [](CListViewWnd *self, const QItemSelection &selected, const QItemSelection &deselected)
  {
    std::vector<int> rows;
    for (const QModelIndex &i : deselected.indexes())
      rows.push_back(i.row());
    for (const QModelIndex &i : selected.indexes())
      rows.push_back(i.row());
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int r : rows)
      if (FromHandle(self))
        self->SyncState(self->Item(r));
  });
  Connect(w, t->selectionModel(), &QItemSelectionModel::currentChanged,
      [](CListViewWnd *self, const QModelIndex &current, const QModelIndex &previous)
  {
    if (previous.isValid())
      self->SyncState(self->Item(previous.row()));
    if (current.isValid() && FromHandle(self))
    {
      self->SelectionMark = current.row();
      self->SyncState(self->Item(current.row()));
    }
  });
  Connect(w, t, &QTreeWidget::itemChanged, [](CListViewWnd *self, QTreeWidgetItem *item, int column)
  {
    if (column == 0)
      self->SyncState(item);
  });
  Connect(w, t->header(), &QHeaderView::sectionClicked, [](CListViewWnd *self, int section)
  {
    NMLISTVIEW nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = -1;
    nm.iSubItem = section;
    self->NotifyParent(&nm.hdr, LVN_COLUMNCLICK);
  });
  return w;
}

// ---------- Narrow messages ----------

static void ToWide(const LVITEMA *a, LVITEMW *w, std::wstring &text)
{
  memcpy(w, a, sizeof(*w));
  w->pszText = NULL;
  if ((a->mask & LVIF_TEXT) && a->pszText)
  {
    if (a->pszText == LPSTR_TEXTCALLBACKA)
      w->pszText = LPSTR_TEXTCALLBACKW;
    else
    {
      text = FromA(a->pszText).toStdWString();
      w->pszText = &text[0];
    }
  }
}

LRESULT SendNarrowListViewMessage(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam, bool &handled)
{
  handled = true;
  switch (message)
  {
    case LVM_INSERTITEMA:
    case LVM_SETITEMA:
    case LVM_SETITEMTEXTA:
    {
      LVITEMW w;
      std::wstring text;
      ToWide((const LVITEMA *)lParam, &w, text);
      if (message == LVM_SETITEMTEXTA)
        w.mask = LVIF_TEXT;
      const UINT wm = message == LVM_INSERTITEMA ? LVM_INSERTITEMW :
          message == LVM_SETITEMA ? LVM_SETITEMW : LVM_SETITEMTEXTW;
      return Send(wnd, wm, wParam, (LPARAM)&w);
    }
    case LVM_GETITEMA:
    case LVM_GETITEMTEXTA:
    {
      LVITEMA *a = (LVITEMA *)lParam;
      LVITEMW w;
      memcpy(&w, a, sizeof(w));
      std::wstring buf;
      if (message == LVM_GETITEMTEXTA)
        w.mask = LVIF_TEXT;
      if ((w.mask & LVIF_TEXT) && a->pszText && a->cchTextMax > 0)
      {
        buf.assign((size_t)a->cchTextMax * 4 + 1, L'\0');
        w.pszText = &buf[0];
        w.cchTextMax = (int)buf.size();
      }
      else
        w.pszText = NULL;
      const LRESULT r = Send(wnd, message == LVM_GETITEMA ? LVM_GETITEMW : LVM_GETITEMTEXTW, wParam, (LPARAM)&w);
      char *text = a->pszText;
      const int max = a->cchTextMax;
      memcpy(a, &w, sizeof(w));
      a->pszText = text;
      a->cchTextMax = max;
      int len = 0;
      if (w.pszText && text)
        len = CopyToA(FromW(w.pszText), text, max);
      return message == LVM_GETITEMA ? r : len;
    }
    case LVM_INSERTCOLUMNA:
    case LVM_SETCOLUMNA:
    {
      const LVCOLUMNA *a = (const LVCOLUMNA *)lParam;
      LVCOLUMNW w;
      memcpy(&w, a, sizeof(w));
      std::wstring text;
      w.pszText = NULL;
      if ((a->mask & LVCF_TEXT) && a->pszText)
      {
        text = FromA(a->pszText).toStdWString();
        w.pszText = &text[0];
      }
      return Send(wnd, message == LVM_INSERTCOLUMNA ? LVM_INSERTCOLUMNW : LVM_SETCOLUMNW, wParam, (LPARAM)&w);
    }
    case LVM_GETCOLUMNA:
    {
      LVCOLUMNA *a = (LVCOLUMNA *)lParam;
      LVCOLUMNW w;
      memcpy(&w, a, sizeof(w));
      std::wstring buf;
      if ((a->mask & LVCF_TEXT) && a->pszText && a->cchTextMax > 0)
      {
        buf.assign((size_t)a->cchTextMax * 4 + 1, L'\0');
        w.pszText = &buf[0];
        w.cchTextMax = (int)buf.size();
      }
      else
        w.pszText = NULL;
      const LRESULT r = Send(wnd, LVM_GETCOLUMNW, wParam, (LPARAM)&w);
      char *text = a->pszText;
      const int max = a->cchTextMax;
      memcpy(a, &w, sizeof(w));
      a->pszText = text;
      a->cchTextMax = max;
      if (w.pszText && text)
        CopyToA(FromW(w.pszText), text, max);
      return r;
    }
    case LVM_FINDITEMA:
    {
      const LVFINDINFOA *a = (const LVFINDINFOA *)lParam;
      LVFINDINFOW w;
      memcpy(&w, a, sizeof(w));
      const std::wstring text = FromA(a->psz).toStdWString();
      w.psz = a->psz ? text.c_str() : NULL;
      return Send(wnd, LVM_FINDITEMW, wParam, (LPARAM)&w);
    }
    case LVM_GETSTRINGWIDTHA:
    {
      const std::wstring text = FromA((LPCSTR)lParam).toStdWString();
      return Send(wnd, LVM_GETSTRINGWIDTHW, wParam, (LPARAM)text.c_str());
    }
    default:
      handled = false;
      return 0;
  }
}

}
