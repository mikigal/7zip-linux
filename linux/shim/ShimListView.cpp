// ShimListView.cpp -- the list view common control (SysListView32) and its
// header (SysHeader32).
//
// The control keeps its items the Win32 way: a vector of items with a
// parameter, an image, texts per column (or LPSTR_TEXTCALLBACK: the parent
// gives the text through LVN_GETDISPINFO when it is shown) and states. The
// states are the truth for the program: LVM_GETITEMSTATE, LVM_GETNEXTITEM
// and LVN_ITEMCHANGED come from them. Qt shows the items through a model of
// that vector, in a QTreeView (LVS_REPORT) or a QListView (LVS_ICON,
// LVS_SMALLICON, LVS_LIST), which share the model and selection model.
// The Qt selection follows the states with a delay (one sync per event loop
// turn), because 7-Zip selects thousands of items one by one; what the user
// selects in the view comes back into the states at once.
//
// Keys reach the control as WM_KEYDOWN and WM_CHAR (see ShimInput.cpp): it
// sends LVN_KEYDOWN to the parent, then lets the view move the focus and
// the selection, as the Windows control does. Enter gives NM_RETURN and
// LVN_ITEMACTIVATE, and Tab is left to the parent. Label editing
// (LVM_EDITLABEL) puts an edit control over the item, and a drag of the
// selected items gives LVN_BEGINDRAG.

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeView>

#include <algorithm>
#include <vector>

#include "ShimDialog.h"
#include "ShimResources.h"
#include "ShimWindow.h"

namespace NShim {

class CListViewWnd;

// ---------- Items and the model ----------

struct CLvItem
{
  LPARAM Param;
  int Image;               // I_IMAGECALLBACK: asked with LVN_GETDISPINFO
  int Indent;
  UINT State;              // LVIS_* including the state image index
  UINT NotifiedState;      // the state of the last LVN_ITEMCHANGED
  UInt64 CallbackMask;     // the columns with LPSTR_TEXTCALLBACK texts
  UInt64 SetMask;          // the columns whose text was set
  std::vector<QString> Texts;

  CLvItem(): Param(0), Image(-1), Indent(0), State(0), NotifiedState(0), CallbackMask(0), SetMask(0) {}
  /* As in the Windows control, a subitem whose text was never set asks
     for it (LPSTR_TEXTCALLBACK); the item itself has no text then. */
  bool IsCallback(int col) const
  {
    if (col >= 64)
      return true;
    const UInt64 bit = (UInt64)1 << col;
    return (CallbackMask & bit) != 0 || (col > 0 && (SetMask & bit) == 0);
  }
};

// Inserts (or removes) bit position col in a column mask.
static UInt64 MaskInsert(UInt64 mask, int col)
{
  const UInt64 low = mask & (((UInt64)1 << col) - 1);
  return low | ((mask & ~(((UInt64)1 << col) - 1)) << 1);
}

static UInt64 MaskRemove(UInt64 mask, int col)
{
  const UInt64 low = mask & (((UInt64)1 << col) - 1);
  return low | ((mask >> 1) & ~(((UInt64)1 << col) - 1));
}

struct CLvColumn
{
  int Fmt;
  QString Text;
  int Width;
};

class CListModel: public QAbstractTableModel
{
public:
  CListViewWnd *Wnd;
  CListModel(CListViewWnd *wnd, QObject *parent): QAbstractTableModel(parent), Wnd(wnd) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  void BeginInsertRows(int first, int last) { beginInsertRows(QModelIndex(), first, last); }
  void EndInsertRows() { endInsertRows(); }
  void BeginRemoveRows(int first, int last) { beginRemoveRows(QModelIndex(), first, last); }
  void EndRemoveRows() { endRemoveRows(); }
  void BeginInsertColumns(int first, int last) { beginInsertColumns(QModelIndex(), first, last); }
  void EndInsertColumns() { endInsertColumns(); }
  void BeginRemoveColumns(int first, int last) { beginRemoveColumns(QModelIndex(), first, last); }
  void EndRemoveColumns() { endRemoveColumns(); }
  void BeginReset() { beginResetModel(); }
  void EndReset() { endResetModel(); }
  void RowsChanged(int first, int last)
  {
    if (first <= last)
      emit dataChanged(index(first, 0), index(last, qMax(0, columnCount() - 1)));
  }
  void HeaderChanged(int first, int last) { emit headerDataChanged(Qt::Horizontal, first, last); }
  void LayoutAboutToChange() { emit layoutAboutToBeChanged(); }
  void LayoutChanged() { emit layoutChanged(); }
  QModelIndexList PersistentIndexes() const { return persistentIndexList(); }
  void ChangePersistentIndexes(const QModelIndexList &from, const QModelIndexList &to)
    { changePersistentIndexList(from, to); }
};

// The views report drags and give the label rectangles.
class CTreeView: public QTreeView
{
public:
  CListViewWnd *Wnd;
  CTreeView(QWidget *parent): QTreeView(parent), Wnd(NULL) {}
  void startDrag(Qt::DropActions actions) override;
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class CIconView: public QListView
{
public:
  CListViewWnd *Wnd;
  CIconView(QWidget *parent): QListView(parent), Wnd(NULL) {}
  void startDrag(Qt::DropActions actions) override;
};

// Item colors of NM_CUSTOMDRAW, and the grid lines of LVS_EX_GRIDLINES.
class CListDelegate: public QStyledItemDelegate
{
public:
  CListViewWnd *Wnd;
  CListDelegate(CListViewWnd *wnd, QObject *parent): QStyledItemDelegate(parent), Wnd(wnd) {}
  void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class CHeaderWnd: public HWND__
{
public:
  CListViewWnd *List;
  CHeaderWnd(): HWND__(kKind_Header), List(NULL) {}
  bool OnEvent(QObject *watched, QEvent *event) override;
  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override;
};

// The edit control of label editing: Enter and focus loss end the editing
// with the text, Esc without it (LVN_ENDLABELEDIT).
class CLabelEditWnd: public HWND__
{
public:
  CListViewWnd *List;
  bool Ending;
  CLabelEditWnd(): HWND__(kKind_Edit), List(NULL), Ending(false) {}
  QLineEdit *Line() const { return static_cast<QLineEdit *>(Widget.data()); }
  void SetText(const QString &s) override { Line()->setText(s); }
  QString Text() const override { return Line()->text(); }
  bool WantsInputMessages() const override { return true; }
  bool OnEvent(QObject *watched, QEvent *event) override;
  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override;
};

class CListViewWnd: public HWND__
{
public:
  std::vector<CLvItem> Items;
  std::vector<CLvColumn> Columns;
  CListModel *Model;
  QItemSelectionModel *Selection;
  CTreeView *Tree;
  CIconView *Icons;
  QStackedLayout *Stack;
  CHeaderWnd *Header;
  CLabelEditWnd *LabelEdit;
  int LabelItem;
  DWORD LvExStyle;
  HIMAGELIST Small;
  HIMAGELIST Normal;
  HIMAGELIST StateList;
  int Focused;
  int SelectionMark;
  UINT CallbackMaskStates;
  int Suppress;              // > 0 while state changes must not notify
  bool SyncPending;          // the Qt selection lags behind the states
  bool InSync;               // the Qt selection is being set from the states
  int NotifyDepth;
  // custom draw: whether the parent wants the item colors in this paint
  bool CustomDrawItems;
  int PaintSerial;

  CListViewWnd(): HWND__(kKind_ListView), Model(NULL), Selection(NULL), Tree(NULL), Icons(NULL),
      Stack(NULL), Header(NULL), LabelEdit(NULL), LabelItem(-1), LvExStyle(0),
      Small(NULL), Normal(NULL), StateList(NULL), Focused(-1), SelectionMark(-1),
      CallbackMaskStates(0), Suppress(0), SyncPending(false), InSync(false), NotifyDepth(0),
      CustomDrawItems(false), PaintSerial(0) {}

  bool WantsInputMessages() const override { return true; }

  int Count() const { return (int)Items.size(); }
  int NumColumns() const { return (int)Columns.size(); }
  bool Valid(int i) const { return i >= 0 && i < Count(); }
  bool HasCheckBoxes() const { return (LvExStyle & LVS_EX_CHECKBOXES) != 0; }
  UINT ViewType() const { return Style & LVS_TYPEMASK; }
  bool IsReport() const { return ViewType() == LVS_REPORT; }
  QAbstractItemView *View() const { return IsReport() ? (QAbstractItemView *)Tree : (QAbstractItemView *)Icons; }

  Qt::Alignment ColumnAlignment(int col) const
  {
    const int fmt = (col >= 0 && col < NumColumns()) ? Columns[(size_t)col].Fmt : LVCFMT_LEFT;
    switch (fmt & LVCFMT_JUSTIFYMASK)
    {
      case LVCFMT_RIGHT: return Qt::AlignRight | Qt::AlignVCenter;
      case LVCFMT_CENTER: return Qt::AlignHCenter | Qt::AlignVCenter;
      default: return Qt::AlignLeft | Qt::AlignVCenter;
    }
  }

  HIMAGELIST ImageListForView() const
  {
    if (ViewType() == LVS_ICON)
      return Normal ? Normal : Small;
    return Small ? Small : Normal;
  }

  // ---------- Texts and images ----------

  // LVN_GETDISPINFO for a callback text or image.
  bool GetDispInfo(int row, int col, UINT mask, QString *text, int *image) const
  {
    CListViewWnd *self = const_cast<CListViewWnd *>(this);
    if (!Valid(row) || self->NotifyDepth > 8)
      return false;
    wchar_t buf[1024];
    buf[0] = 0;
    NMLVDISPINFOW nm;
    memset(&nm, 0, sizeof(nm));
    nm.item.mask = mask;
    nm.item.iItem = row;
    nm.item.iSubItem = col;
    nm.item.lParam = Items[(size_t)row].Param;
    nm.item.state = Items[(size_t)row].State;
    nm.item.pszText = buf;
    nm.item.cchTextMax = (int)Z7_ARRAY_SIZE(buf);
    self->NotifyDepth++;
    self->NotifyParent(&nm.hdr, LVN_GETDISPINFOW);
    self->NotifyDepth--;
    if (text)
    {
      if (nm.item.pszText && nm.item.pszText != LPSTR_TEXTCALLBACKW)
        *text = FromW(nm.item.pszText);
      else
        text->clear();
    }
    if (image)
      *image = nm.item.iImage;
    return true;
  }

  QString ItemText(int row, int col) const
  {
    if (!Valid(row) || col < 0)
      return QString();
    const CLvItem &it = Items[(size_t)row];
    if (it.IsCallback(col))
    {
      QString s;
      GetDispInfo(row, col, LVIF_TEXT, &s, NULL);
      return s;
    }
    return (size_t)col < it.Texts.size() ? it.Texts[(size_t)col] : QString();
  }

  int ItemImage(int row) const
  {
    const CLvItem &it = Items[(size_t)row];
    if (it.Image != I_IMAGECALLBACK)
      return it.Image;
    int image = -1;
    GetDispInfo(row, 0, LVIF_IMAGE, NULL, &image);
    return image;
  }

  QIcon ItemIcon(int row) const
  {
    HIMAGELIST list = ImageListForView();
    const int image = ItemImage(row);
    if (list && image >= 0 && image < (int)list->Icons.size())
      return list->Icons[image];
    return QIcon();
  }

  void SetItemText(CLvItem &it, int col, LPCWSTR s)
  {
    if (col < 0 || col >= 64)
      return;
    if ((size_t)col >= it.Texts.size())
      it.Texts.resize((size_t)col + 1);
    it.SetMask |= (UInt64)1 << col;
    if (s == LPSTR_TEXTCALLBACKW)
    {
      it.CallbackMask |= (UInt64)1 << col;
      it.Texts[(size_t)col].clear();
    }
    else
    {
      it.CallbackMask &= ~((UInt64)1 << col);
      it.Texts[(size_t)col] = FromW(s);
    }
  }

  // ---------- States ----------

  UINT StateOf(int row) const
  {
    UINT s = Items[(size_t)row].State & ~(UINT)(LVIS_SELECTED | LVIS_FOCUSED);
    if (Items[(size_t)row].State & LVIS_SELECTED)
      s |= LVIS_SELECTED;
    if (row == Focused)
      s |= LVIS_FOCUSED;
    return s;
  }

  void Notify(int row)
  {
    if (!Valid(row))
      return;
    CLvItem &it = Items[(size_t)row];
    const UINT newState = StateOf(row);
    const UINT oldState = it.NotifiedState;
    if (newState == oldState)
      return;
    it.NotifiedState = newState;
    if (Suppress > 0)
      return;
    NMLISTVIEW nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = row;
    nm.uChanged = LVIF_STATE;
    nm.uOldState = oldState;
    nm.uNewState = newState;
    nm.lParam = it.Param;
    NotifyParent(&nm.hdr, LVN_ITEMCHANGED);
  }

  void ScheduleSync()
  {
    if (SyncPending)
      return;
    SyncPending = true;
    const UInt64 serial = Serial;
    HWND self = this;
    QTimer::singleShot(0, Widget, [self, serial]()
    {
      CWindowLock lock(self);
      if (lock && self->Serial == serial)
        static_cast<CListViewWnd *>(self)->SyncToView();
    });
  }

  // The Qt selection and current item from the states.
  void SyncToView()
  {
    if (!SyncPending || !Selection)
      return;
    SyncPending = false;
    InSync = true;
    QItemSelection sel;
    const int n = Count();
    const int lastCol = qMax(0, NumColumns() - 1);
    for (int i = 0; i < n;)
    {
      if (!(Items[(size_t)i].State & LVIS_SELECTED))
      {
        i++;
        continue;
      }
      int k = i;
      while (k + 1 < n && (Items[(size_t)k + 1].State & LVIS_SELECTED))
        k++;
      sel.select(Model->index(i, 0), Model->index(k, lastCol));
      i = k + 1;
    }
    Selection->select(sel, QItemSelectionModel::ClearAndSelect);
    if (Valid(Focused))
      Selection->setCurrentIndex(Model->index(Focused, 0), QItemSelectionModel::NoUpdate);
    else
      Selection->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    InSync = false;
  }

  void SetFocused(int row)
  {
    if (row == Focused)
      return;
    const int prev = Focused;
    Focused = row;
    Notify(prev);
    Notify(row);
    ScheduleSync();
  }

  void SetState(int row, UINT state, UINT mask)
  {
    CLvItem &it = Items[(size_t)row];
    if (mask & LVIS_FOCUSED)
    {
      if (state & LVIS_FOCUSED)
      {
        if (Focused != row)
        {
          const int prev = Focused;
          Focused = row;
          Notify(prev);
        }
      }
      else if (Focused == row)
        Focused = -1;
    }
    if ((mask & LVIS_SELECTED) && (state & LVIS_SELECTED) && (Style & LVS_SINGLESEL))
      for (int i = 0; i < Count(); i++)
        if (i != row && (Items[(size_t)i].State & LVIS_SELECTED))
        {
          Items[(size_t)i].State &= ~(UINT)LVIS_SELECTED;
          Notify(i);
        }
    const UINT keep = ~(mask & ~(UINT)LVIS_FOCUSED);
    it.State = (it.State & keep) | (state & mask & ~(UINT)LVIS_FOCUSED);
    if ((mask & LVIS_SELECTED) && (state & LVIS_SELECTED))
      SelectionMark = row;
    Notify(row);
    ScheduleSync();
    if (mask & (LVIS_CUT | LVIS_DROPHILITED | LVIS_STATEIMAGEMASK | LVIS_OVERLAYMASK))
      Model->RowsChanged(row, row);
  }

  // The user changed the selection or the current item in the view.
  void OnViewSelection(const QItemSelection &selected, const QItemSelection &deselected)
  {
    if (InSync)
      return;
    std::vector<int> rows;
    for (const QItemSelectionRange &r : deselected)
      for (int i = r.top(); i <= r.bottom(); i++)
        rows.push_back(i);
    for (const QItemSelectionRange &r : selected)
      for (int i = r.top(); i <= r.bottom(); i++)
        rows.push_back(i);
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : rows)
    {
      if (!Valid(row) || !FromHandle(this))
        continue;
      const bool sel = Selection->isRowSelected(row, QModelIndex());
      CLvItem &it = Items[(size_t)row];
      if (sel)
        it.State |= LVIS_SELECTED;
      else
        it.State &= ~(UINT)LVIS_SELECTED;
      Notify(row);
    }
  }

  void OnViewCurrent(const QModelIndex &current)
  {
    if (InSync)
      return;
    const int row = current.isValid() ? current.row() : -1;
    if (row == Focused)
      return;
    const int prev = Focused;
    Focused = row;
    if (row >= 0)
      SelectionMark = row;
    Notify(prev);
    Notify(row);
  }

  // ---------- Items ----------

  int InsertItem(const LVITEMW *lv)
  {
    if (lv->iSubItem != 0)
      return -1;
    int pos = lv->iItem;
    if (pos < 0 || pos > Count())
      pos = Count();
    CLvItem it;
    it.Texts.resize((size_t)qMax(1, NumColumns()));
    if (lv->mask & LVIF_TEXT)
      SetItemText(it, 0, lv->pszText);
    if (lv->mask & LVIF_PARAM)
      it.Param = lv->lParam;
    if (lv->mask & LVIF_IMAGE)
      it.Image = lv->iImage;
    if (lv->mask & LVIF_INDENT)
      it.Indent = lv->iIndent;
    if (HasCheckBoxes())
      it.State |= INDEXTOSTATEIMAGEMASK(1);
    // the rows after pos move down: the focus too
    Model->BeginInsertRows(pos, pos);
    Items.insert(Items.begin() + pos, it);
    if (Focused >= pos)
      Focused++;
    if (SelectionMark >= pos)
      SelectionMark++;
    Model->EndInsertRows();
    Items[(size_t)pos].NotifiedState = StateOf(pos);
    if (lv->mask & LVIF_STATE)
      SetState(pos, lv->state, lv->stateMask & ~(UINT)LVIS_STATEIMAGEMASK);
    ScheduleSync();
    return pos;
  }

  bool SetItem(const LVITEMW *lv)
  {
    if (!Valid(lv->iItem))
      return false;
    CLvItem &it = Items[(size_t)lv->iItem];
    if (lv->mask & LVIF_TEXT)
      SetItemText(it, lv->iSubItem, lv->pszText);
    if (lv->iSubItem == 0)
    {
      if (lv->mask & LVIF_PARAM)
        it.Param = lv->lParam;
      if (lv->mask & LVIF_IMAGE)
        it.Image = lv->iImage;
      if (lv->mask & LVIF_INDENT)
        it.Indent = lv->iIndent;
    }
    Model->RowsChanged(lv->iItem, lv->iItem);
    if ((lv->mask & LVIF_STATE) && lv->iSubItem == 0)
      SetState(lv->iItem, lv->state, lv->stateMask);
    return true;
  }

  bool GetItem(LVITEMW *lv) const
  {
    if (!Valid(lv->iItem))
      return false;
    const CLvItem &it = Items[(size_t)lv->iItem];
    if (lv->mask & LVIF_TEXT)
    {
      if (it.IsCallback(lv->iSubItem))
        lv->pszText = LPSTR_TEXTCALLBACKW;
      else
        CopyToW((size_t)lv->iSubItem < it.Texts.size() ? it.Texts[(size_t)lv->iSubItem] : QString(),
            lv->pszText, lv->cchTextMax);
    }
    if (lv->iSubItem == 0)
    {
      if (lv->mask & LVIF_PARAM)
        lv->lParam = it.Param;
      if (lv->mask & LVIF_IMAGE)
        lv->iImage = it.Image;
      if (lv->mask & LVIF_INDENT)
        lv->iIndent = it.Indent;
      if (lv->mask & LVIF_STATE)
        lv->state = StateOf(lv->iItem) & lv->stateMask;
    }
    return true;
  }

  bool DeleteItem(int row)
  {
    if (!Valid(row))
      return false;
    if (LabelItem == row)
      EndLabelEdit(false);
    NMLISTVIEW nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = row;
    nm.lParam = Items[(size_t)row].Param;
    NotifyParent(&nm.hdr, LVN_DELETEITEM);
    if (!FromHandle(this) || !Valid(row))
      return false;
    Model->BeginRemoveRows(row, row);
    Items.erase(Items.begin() + row);
    if (Focused == row)
      Focused = -1;
    else if (Focused > row)
      Focused--;
    if (SelectionMark >= row)
      SelectionMark = SelectionMark == row ? -1 : SelectionMark - 1;
    if (LabelItem > row)
      LabelItem--;
    Model->EndRemoveRows();
    ScheduleSync();
    return true;
  }

  void DeleteAll()
  {
    if (LabelEdit)
      EndLabelEdit(false);
    {
      NMLISTVIEW nm;
      memset(&nm, 0, sizeof(nm));
      nm.iItem = -1;
      // nonzero: no LVN_DELETEITEM for each item
      NotifyParent(&nm.hdr, LVN_DELETEALLITEMS);
    }
    if (!FromHandle(this))
      return;
    Model->BeginReset();
    Items.clear();
    Focused = -1;
    SelectionMark = -1;
    SyncPending = false;
    Model->EndReset();
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
    const UINT s = StateOf(i);
    if ((flags & LVNI_SELECTED) && !(s & LVIS_SELECTED))
      return false;
    if ((flags & LVNI_FOCUSED) && !(s & LVIS_FOCUSED))
      return false;
    if ((flags & LVNI_CUT) && !(s & LVIS_CUT))
      return false;
    if ((flags & LVNI_DROPHILITED) && !(s & LVIS_DROPHILITED))
      return false;
    return true;
  }

  int SelectedCount() const
  {
    int n = 0;
    for (const CLvItem &it : Items)
      if (it.State & LVIS_SELECTED)
        n++;
    return n;
  }

  // Sorting moves the items with their states, the focus and the edit.
  void Sort(const std::function<int(const CLvItem &, int, const CLvItem &, int)> &compare)
  {
    std::vector<int> order((size_t)Count());
    for (int i = 0; i < Count(); i++)
      order[(size_t)i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b)
    {
      return compare(Items[(size_t)a], a, Items[(size_t)b], b) < 0;
    });
    Model->LayoutAboutToChange();
    std::vector<CLvItem> sorted;
    sorted.reserve(Items.size());
    int focused = -1, mark = -1, label = -1;
    for (size_t k = 0; k < order.size(); k++)
    {
      const int old = order[k];
      if (old == Focused)
        focused = (int)k;
      if (old == SelectionMark)
        mark = (int)k;
      if (old == LabelItem)
        label = (int)k;
      sorted.push_back(Items[(size_t)old]);
    }
    Items.swap(sorted);
    Focused = focused;
    SelectionMark = mark;
    LabelItem = label;
    // the persistent indexes of the views (current item, selection) move too
    QModelIndexList from = Model->PersistentIndexes();
    QModelIndexList to;
    std::vector<int> newPos(order.size());
    for (size_t k = 0; k < order.size(); k++)
      newPos[(size_t)order[k]] = (int)k;
    for (const QModelIndex &i : from)
      to.append(Model->index(newPos[(size_t)i.row()], i.column()));
    Model->ChangePersistentIndexes(from, to);
    Model->LayoutChanged();
    SyncPending = true;
    SyncToView();
  }

  // ---------- Columns ----------

  /* Column widths of the program are pixels for the font of Windows (the
     dialog font, Tahoma 8 at 96 dpi: base unit 6); on screen they grow
     with the font, as the dialog layouts do, and the program reads back
     the widths it set. */
  double WidthScale() const
  {
    return ComputeBaseUnits(Widget->font()).X / 6.0;
  }
  int ToScreen(int w) const { return (int)(w * WidthScale() + 0.5); }
  int FromScreen(int w) const { return (int)(w / WidthScale() + 0.5); }

  void ApplyColumnWidths()
  {
    if (!Tree)
      return;
    QHeaderView *h = Tree->header();
    // without columns the model still has column 0 (for the list and icon
    // views); the header shows it with no width, as an empty header
    if (NumColumns() == 0)
      h->resizeSection(0, 0);
    for (int c = 0; c < NumColumns(); c++)
      if (h->sectionSize(c) != Columns[(size_t)c].Width)
        h->resizeSection(c, Columns[(size_t)c].Width);
  }

  int InsertColumn(int index, const LVCOLUMNW *c)
  {
    const int n = NumColumns();
    if (index < 0 || index > n)
      index = n;
    CLvColumn col;
    col.Fmt = (c->mask & LVCF_FMT) ? c->fmt : LVCFMT_LEFT;
    col.Text = ((c->mask & LVCF_TEXT) && c->pszText) ? FromW(c->pszText) : QString();
    col.Width = ToScreen((c->mask & LVCF_WIDTH) ? c->cx : 60);
    // the texts of the items move with the columns; the first column takes
    // the place of the model column that a list without columns has
    const bool first = (n == 0);
    if (!first)
      Model->BeginInsertColumns(index, index);
    Columns.insert(Columns.begin() + index, col);
    for (CLvItem &it : Items)
    {
      if ((size_t)index < it.Texts.size())
        it.Texts.insert(it.Texts.begin() + index, QString());
      it.CallbackMask = MaskInsert(it.CallbackMask, index);
      it.SetMask = MaskInsert(it.SetMask, index);
    }
    if (!first)
      Model->EndInsertColumns();
    else
    {
      Model->HeaderChanged(0, 0);
      Model->RowsChanged(0, Count() - 1);
    }
    if (Tree)
    {
      QHeaderView *h = Tree->header();
      h->resizeSection(index, col.Width);
      // LVCF_ORDER: the position on the screen
      if ((c->mask & LVCF_ORDER) && c->iOrder >= 0 && c->iOrder < NumColumns())
        h->moveSection(h->visualIndex(index), c->iOrder);
    }
    return index;
  }

  bool DeleteColumn(int index)
  {
    if (index < 0 || index >= NumColumns())
      return false;
    const bool last = (NumColumns() == 1);
    if (!last)
      Model->BeginRemoveColumns(index, index);
    Columns.erase(Columns.begin() + index);
    for (CLvItem &it : Items)
    {
      if ((size_t)index < it.Texts.size())
        it.Texts.erase(it.Texts.begin() + index);
      it.CallbackMask = MaskRemove(it.CallbackMask, index);
      it.SetMask = MaskRemove(it.SetMask, index);
    }
    if (!last)
      Model->EndRemoveColumns();
    else
    {
      Model->HeaderChanged(0, 0);
      Model->RowsChanged(0, Count() - 1);
      ApplyColumnWidths();
    }
    return true;
  }

  void SetColumn(int index, const LVCOLUMNW *c)
  {
    CLvColumn &col = Columns[(size_t)index];
    if (c->mask & LVCF_TEXT)
      col.Text = FromW(c->pszText);
    if (c->mask & LVCF_FMT)
    {
      col.Fmt = c->fmt;
      Model->RowsChanged(0, Count() - 1);
    }
    Model->HeaderChanged(index, index);
    if (c->mask & LVCF_WIDTH)
      SetColumnWidth(index, c->cx);
    if ((c->mask & LVCF_ORDER) && Tree && c->iOrder >= 0 && c->iOrder < NumColumns())
      Tree->header()->moveSection(Tree->header()->visualIndex(index), c->iOrder);
  }

  int ContentWidth(int col) const
  {
    const QFontMetrics fm(Widget->font());
    int w = 0;
    const int n = qMin(Count(), 5000);
    for (int i = 0; i < n; i++)
      w = qMax(w, fm.horizontalAdvance(ItemText(i, col)));
    if (col == 0 && ImageListForView())
      w += ImageListForView()->Cx + 6;
    return w + 12;
  }

  void SetColumnWidth(int col, int cx)
  {
    if (col < 0 || col >= NumColumns())
      return;
    int w = ToScreen(cx);
    if (cx == LVSCW_AUTOSIZE)
      w = ContentWidth(col);
    else if (cx == LVSCW_AUTOSIZE_USEHEADER)
    {
      w = qMax(ContentWidth(col), QFontMetrics(Widget->font()).horizontalAdvance(Columns[(size_t)col].Text) + 16);
      if (col == NumColumns() - 1 && Tree)
      {
        // the last column fills the rest of the list
        int used = 0;
        for (int i = 0; i < col; i++)
          used += Tree->header()->sectionSize(i);
        w = qMax(w, Tree->viewport()->width() - used);
      }
    }
    Columns[(size_t)col].Width = w;
    if (Tree)
      Tree->header()->resizeSection(col, w);
  }

  int ColumnWidth(int col) const
  {
    if (col < 0 || col >= NumColumns())
      return 0;
    return Tree ? Tree->header()->sectionSize(col) : Columns[(size_t)col].Width;
  }

  // ---------- Views ----------

  void ApplyViewType()
  {
    if (!Stack)
      return;
    const UINT type = ViewType();
    if (type == LVS_REPORT)
    {
      Stack->setCurrentWidget(Tree);
      ApplyColumnWidths();
    }
    else
    {
      HIMAGELIST list = ImageListForView();
      const QSize icon = list ? QSize(list->Cx, list->Cy) : QSize(16, 16);
      Icons->setIconSize(icon);
      if (type == LVS_ICON)
      {
        Icons->setViewMode(QListView::IconMode);
        Icons->setFlow(QListView::LeftToRight);
        Icons->setWrapping(true);
        Icons->setWordWrap(true);
        const int w = qMax(icon.width() + 40, 75);
        Icons->setGridSize(QSize(w, icon.height() + Widget->fontMetrics().height() * 2 + 12));
        Icons->setSpacing(0);
      }
      else
      {
        Icons->setViewMode(QListView::ListMode);
        Icons->setGridSize(QSize());
        Icons->setWordWrap(false);
        Icons->setWrapping(true);
        // LVS_SMALLICON: rows from left to right; LVS_LIST: columns
        Icons->setFlow(type == LVS_SMALLICON ? QListView::LeftToRight : QListView::TopToBottom);
        Icons->setSpacing(1);
      }
      Icons->setMovement(QListView::Static);
      Icons->setResizeMode(QListView::Adjust);
      Icons->setUniformItemSizes(type != LVS_ICON);
      Stack->setCurrentWidget(Icons);
    }
    const bool hadFocus = Tree->hasFocus() || Icons->hasFocus();
    Widget->setFocusProxy(View());
    if (hadFocus)
      View()->setFocus(Qt::OtherFocusReason);
  }

  void OnStyleChanged(DWORD oldStyle) override
  {
    if ((oldStyle ^ Style) & LVS_TYPEMASK)
      ApplyViewType();
    if ((oldStyle ^ Style) & LVS_SINGLESEL)
    {
      const QAbstractItemView::SelectionMode mode = (Style & LVS_SINGLESEL) ?
          QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection;
      Tree->setSelectionMode(mode);
      Icons->setSelectionMode(mode);
    }
    if ((oldStyle ^ Style) & LVS_NOCOLUMNHEADER)
      Tree->setHeaderHidden((Style & LVS_NOCOLUMNHEADER) != 0);
  }

  void ApplyExStyle(DWORD prev)
  {
    if ((prev ^ LvExStyle) & LVS_EX_CHECKBOXES)
    {
      Suppress++;
      for (int i = 0; i < Count(); i++)
      {
        CLvItem &it = Items[(size_t)i];
        if (HasCheckBoxes() && !(it.State & LVIS_STATEIMAGEMASK))
          it.State |= INDEXTOSTATEIMAGEMASK(1);
        else if (!HasCheckBoxes())
          it.State &= ~(UINT)LVIS_STATEIMAGEMASK;
        it.NotifiedState = StateOf(i);
      }
      Suppress--;
      Model->RowsChanged(0, Count() - 1);
    }
    Tree->header()->setSectionsMovable((LvExStyle & LVS_EX_HEADERDRAGDROP) != 0);
    // without LVS_EX_FULLROWSELECT only the name shows the selection
    Tree->viewport()->update();
    Tree->setMouseTracking((LvExStyle & LVS_EX_TRACKSELECT) != 0);
  }

  // A rectangle of the view in client coordinates of the control.
  QRect ToClient(const QRect &viewportRect) const
  {
    QAbstractItemView *v = View();
    return QRect(v->viewport()->mapTo(Widget.data(), viewportRect.topLeft()), viewportRect.size());
  }

  QRect ItemRect(int row, int code) const
  {
    QAbstractItemView *v = View();
    const QModelIndex i0 = Model->index(row, 0);
    QRect r = v->visualRect(i0);
    if (IsReport())
    {
      if (code == LVIR_BOUNDS || code == LVIR_SELECTBOUNDS)
      {
        int width = 0;
        for (int c = 0; c < NumColumns(); c++)
          width += Tree->header()->sectionSize(c);
        r = QRect(-Tree->horizontalScrollBar()->value(), r.top(), width, r.height());
      }
      else
      {
        const int icon = ImageListForView() ? ImageListForView()->Cx + 4 : 0;
        if (code == LVIR_ICON)
          r = QRect(r.left(), r.top(), icon, r.height());
        else if (code == LVIR_LABEL)
          r = QRect(r.left() + icon, r.top(), qMax(0, r.width() - icon), r.height());
      }
    }
    else if (code == LVIR_ICON || code == LVIR_LABEL)
    {
      const QSize icon = Icons->iconSize();
      if (ViewType() == LVS_ICON)
      {
        const QRect ir(r.left() + (r.width() - icon.width()) / 2, r.top() + 2, icon.width(), icon.height());
        r = (code == LVIR_ICON) ? ir : QRect(r.left(), ir.bottom() + 2, r.width(), qMax(0, r.bottom() - ir.bottom() - 2));
      }
      else
      {
        const QRect ir(r.left(), r.top(), icon.width() + 4, r.height());
        r = (code == LVIR_ICON) ? ir : QRect(ir.right() + 1, r.top(), qMax(0, r.width() - ir.width()), r.height());
      }
    }
    return ToClient(r);
  }

  int HitTest(LVHITTESTINFO *info, bool sub) const
  {
    QAbstractItemView *v = View();
    const QPoint p = v->viewport()->mapFrom(Widget.data(), QPoint(info->pt.x, info->pt.y));
    const QModelIndex index = v->indexAt(p);
    info->iSubItem = 0;
    if (!index.isValid())
    {
      info->flags = LVHT_NOWHERE;
      info->iItem = -1;
      return -1;
    }
    const QRect icon = ItemRect(index.row(), LVIR_ICON);
    info->flags = (index.column() == 0 && icon.contains(QPoint(info->pt.x, info->pt.y))) ? LVHT_ONITEMICON : LVHT_ONITEMLABEL;
    info->iItem = index.row();
    if (sub)
      info->iSubItem = index.column();
    return index.row();
  }

  int ItemAt(const QPoint &viewportPos, int *column = NULL) const
  {
    const QModelIndex index = View()->indexAt(viewportPos);
    if (column)
      *column = index.isValid() ? index.column() : 0;
    return index.isValid() ? index.row() : -1;
  }

  // ---------- Notifications of the user's actions ----------

  void NotifyMouse(UINT code, const QPoint &viewportPos)
  {
    int col = 0;
    NMITEMACTIVATE nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = ItemAt(viewportPos, &col);
    nm.iSubItem = col;
    const QPoint p = View()->viewport()->mapTo(Widget.data(), viewportPos);
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
      nm.lParam = Items[(size_t)nm.iItem].Param;
    NotifyParent(&nm.hdr, code);
  }

  void NotifyActivate(int row)
  {
    NMITEMACTIVATE nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = row;
    if (Valid(row))
      nm.lParam = Items[(size_t)row].Param;
    if (GetKeyState(VK_MENU) < 0)
      nm.uKeyFlags |= LVKF_ALT;
    if (GetKeyState(VK_CONTROL) < 0)
      nm.uKeyFlags |= LVKF_CONTROL;
    if (GetKeyState(VK_SHIFT) < 0)
      nm.uKeyFlags |= LVKF_SHIFT;
    NotifyParent(&nm.hdr, LVN_ITEMACTIVATE);
  }

  void BeginDrag(bool right)
  {
    if (!Valid(Focused))
      return;
    NMLISTVIEW nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = Focused;
    nm.lParam = Items[(size_t)Focused].Param;
    const QPoint p = Widget->mapFromGlobal(QCursor::pos());
    nm.ptAction.x = p.x();
    nm.ptAction.y = p.y();
    NotifyParent(&nm.hdr, right ? LVN_BEGINRDRAG : LVN_BEGINDRAG);
  }

  // NM_CUSTOMDRAW before a paint: does the parent want item colors?
  void PrePaint()
  {
    if (SyncPending)
      SyncToView();
    NMLVCUSTOMDRAW cd;
    memset(&cd, 0, sizeof(cd));
    cd.nmcd.dwDrawStage = CDDS_PREPAINT;
    const LRESULT r = NotifyParent(&cd.nmcd.hdr, NM_CUSTOMDRAW);
    CustomDrawItems = (r & CDRF_NOTIFYITEMDRAW) != 0;
    PaintSerial++;
  }

  // The colors of an item from NM_CUSTOMDRAW (CDDS_ITEMPREPAINT).
  bool ItemColors(int row, QColor &text, QColor &back) const
  {
    if (!CustomDrawItems || !Valid(row))
      return false;
    CListViewWnd *self = const_cast<CListViewWnd *>(this);
    NMLVCUSTOMDRAW cd;
    memset(&cd, 0, sizeof(cd));
    cd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
    cd.nmcd.dwItemSpec = (DWORD_PTR)row;
    cd.nmcd.lItemlParam = Items[(size_t)row].Param;
    const COLORREF none = 0xFFFFFFFF;  // CLR_NONE
    const COLORREF bk = BkColor();
    cd.clrText = none;
    cd.clrTextBk = none;
    self->NotifyParent(&cd.nmcd.hdr, NM_CUSTOMDRAW);
    bool any = false;
    if (cd.clrText != none)
    {
      text = AdaptColor(cd.clrText, false);
      any = true;
    }
    if (cd.clrTextBk != none && cd.clrTextBk != bk)
    {
      back = AdaptColor(cd.clrTextBk, true);
      any = true;
    }
    return any;
  }

  COLORREF BkColor() const
  {
    const QColor c = View()->palette().color(QPalette::Base);
    return RGB(c.red(), c.green(), c.blue());
  }

  /* The program picks colors for a light list (pink marks, red text). On
     the dark list they keep their hue, with a lightness that suits a dark
     background: dark for backgrounds, light for texts. */
  static QColor AdaptColor(COLORREF c, bool background)
  {
    QColor q(GetRValue(c), GetGValue(c), GetBValue(c));
    float h, s, l, a;
    q.getHslF(&h, &s, &l, &a);
    if (background)
      l = qMin(l, 0.28f);
    else
      l = qMax(l, 0.62f);
    return QColor::fromHslF(h, s, l, a);
  }

  // ---------- Label editing ----------

  HWND BeginLabelEdit(int row);
  void EndLabelEdit(bool accept);

  // ---------- Events ----------

  bool OnEvent(QObject *watched, QEvent *event) override;
  LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) override;
};

// ---------- Model ----------

int CListModel::rowCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : Wnd->Count();
}

int CListModel::columnCount(const QModelIndex &parent) const
{
  // a list view without columns still shows its items (in column 0)
  return parent.isValid() ? 0 : qMax(1, Wnd->NumColumns());
}

QVariant CListModel::data(const QModelIndex &index, int role) const
{
  const int row = index.row();
  const int col = index.column();
  if (!index.isValid() || !Wnd->Valid(row))
    return QVariant();
  switch (role)
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return Wnd->ItemText(row, col);
    case Qt::DecorationRole:
      if (col == 0)
        return Wnd->ItemIcon(row);
      return QVariant();
    case Qt::TextAlignmentRole:
      if (!Wnd->IsReport())
        return QVariant();
      return QVariant::fromValue(Wnd->ColumnAlignment(col));
    case Qt::CheckStateRole:
      if (col == 0 && Wnd->HasCheckBoxes())
        return ((Wnd->Items[(size_t)row].State & LVIS_STATEIMAGEMASK) >> 12) == 2 ? Qt::Checked : Qt::Unchecked;
      return QVariant();
    default:
      return QVariant();
  }
}

bool CListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (role != Qt::CheckStateRole || !index.isValid() || index.column() != 0 || !Wnd->Valid(index.row()))
    return false;
  const bool checked = (value.toInt() == Qt::Checked);
  Wnd->SetState(index.row(), INDEXTOSTATEIMAGEMASK(checked ? 2 : 1), LVIS_STATEIMAGEMASK);
  return true;
}

Qt::ItemFlags CListModel::flags(const QModelIndex &index) const
{
  Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
  if (index.column() == 0 && Wnd->HasCheckBoxes())
    f |= Qt::ItemIsUserCheckable;
  return f;
}

QVariant CListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || section < 0 || section >= Wnd->NumColumns())
    return QVariant();
  if (role == Qt::DisplayRole)
    return Wnd->Columns[(size_t)section].Text;
  if (role == Qt::TextAlignmentRole)
    return QVariant::fromValue(Wnd->ColumnAlignment(section));
  return QVariant();
}

// ---------- Views and delegate ----------

void CTreeView::startDrag(Qt::DropActions /* actions */)
{
  if (Wnd && FromHandle(Wnd))
    Wnd->BeginDrag((QApplication::mouseButtons() & Qt::RightButton) != 0);
}

void CTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QTreeView::drawRow(painter, option, index);
  if (Wnd && (Wnd->LvExStyle & LVS_EX_GRIDLINES))
  {
    painter->save();
    painter->setPen(palette().color(QPalette::Mid).lighter(160));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    int x = -horizontalScrollBar()->value();
    for (int v = 0; v < header()->count(); v++)
    {
      const int logical = header()->logicalIndex(v);
      if (header()->isSectionHidden(logical))
        continue;
      x += header()->sectionSize(logical);
      painter->drawLine(x - 1, option.rect.top(), x - 1, option.rect.bottom());
    }
    painter->restore();
  }
}

void CIconView::startDrag(Qt::DropActions /* actions */)
{
  if (Wnd && FromHandle(Wnd))
    Wnd->BeginDrag((QApplication::mouseButtons() & Qt::RightButton) != 0);
}

void CListDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
  QStyledItemDelegate::initStyleOption(option, index);
  if (!Wnd || !Wnd->IsReport())
    return;
  // without LVS_EX_FULLROWSELECT only the name column shows the selection
  if (index.column() != 0 && !(Wnd->LvExStyle & LVS_EX_FULLROWSELECT))
    option->state &= ~QStyle::State_Selected;
  QColor text, back;
  if (Wnd->ItemColors(index.row(), text, back))
  {
    if (text.isValid())
      option->palette.setColor(QPalette::Text, text);
    if (back.isValid())
      option->backgroundBrush = back;
  }
  // a cut item (LVIS_CUT) looks faded, as on Windows
  if (Wnd->Valid(index.row()) && (Wnd->Items[(size_t)index.row()].State & LVIS_CUT))
    option->palette.setColor(QPalette::Text, option->palette.color(QPalette::Disabled, QPalette::Text));
}

void CListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QStyleOptionViewItem o(option);
  if (Wnd && !Wnd->IsReport())
  {
    QColor text, back;
    if (Wnd->ItemColors(index.row(), text, back))
    {
      if (text.isValid())
        o.palette.setColor(QPalette::Text, text);
      if (back.isValid())
        painter->fillRect(o.rect, back);
    }
  }
  if (Wnd && Wnd->Valid(index.row()) && (Wnd->Items[(size_t)index.row()].State & LVIS_DROPHILITED))
    o.state |= QStyle::State_Selected;
  QStyledItemDelegate::paint(painter, o, index);
}

// ---------- Header ----------

bool CHeaderWnd::OnEvent(QObject * /* watched */, QEvent *event)
{
  if (event->type() == QEvent::ContextMenu && List)
  {
    const QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
    // NM_RCLICK to the parent of the list, as the list view forwards it
    NMHDR hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.hwndFrom = this;
    hdr.idFrom = Id;
    hdr.code = (UINT)NM_RCLICK;
    if (List->Parent)
      Send(List->Parent, WM_NOTIFY, (WPARAM)Id, (LPARAM)&hdr);
    if (FromHandle(this))
    {
      const QPoint g = e->globalPos();
      Send(this, WM_CONTEXTMENU, (WPARAM)(HWND)this, MAKELPARAM(g.x(), g.y()));
    }
    return true;
  }
  return false;
}

LRESULT CHeaderWnd::Proc(UINT message, WPARAM wParam, LPARAM lParam)
{
  QHeaderView *h = static_cast<QHeaderView *>(Widget.data());
  switch (message)
  {
    case HDM_GETITEMCOUNT:
      return h->count();
    case HDM_ORDERTOINDEX:
      return h->logicalIndex((int)wParam);
    case HDM_GETITEMRECT:
    {
      RECT *r = (RECT *)lParam;
      const int i = (int)wParam;
      if (!r || i < 0 || i >= h->count())
        return FALSE;
      r->left = h->sectionViewportPosition(i);
      r->right = r->left + h->sectionSize(i);
      r->top = 0;
      r->bottom = h->height();
      return TRUE;
    }
    default:
      return DefWindowProcImpl(this, message, wParam, lParam);
  }
}

// ---------- Label editing ----------

HWND CListViewWnd::BeginLabelEdit(int row)
{
  if (LabelEdit)
    EndLabelEdit(true);
  if (!Valid(row) || !FromHandle(this))
    return NULL;
  wchar_t buf[1024];
  const QString text = ItemText(row, 0);
  CopyToW(text, buf, (int)Z7_ARRAY_SIZE(buf));
  NMLVDISPINFOW nm;
  memset(&nm, 0, sizeof(nm));
  nm.item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
  nm.item.iItem = row;
  nm.item.lParam = Items[(size_t)row].Param;
  nm.item.state = StateOf(row);
  nm.item.pszText = buf;
  nm.item.cchTextMax = (int)Z7_ARRAY_SIZE(buf);
  if (NotifyParent(&nm.hdr, LVN_BEGINLABELEDITW) != 0 || !FromHandle(this) || !Valid(row))
    return NULL;
  View()->scrollTo(Model->index(row, 0));
  CLabelEditWnd *e = new CLabelEditWnd();
  e->Style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
  e->List = this;
  QLineEdit *line = new QLineEdit(Widget);
  RegisterWindow(e, line);
  line->installEventFilter(e->Proxy);
  e->Parent = this;
  Children.push_back(e);
  LabelEdit = e;
  LabelItem = row;
  QRect r = ItemRect(row, LVIR_LABEL);
  r.setHeight(qMax(r.height(), line->sizeHint().height()));
  r.setWidth(qMax(r.width(), 60));
  line->setGeometry(r);
  line->setText(text);
  line->selectAll();
  line->show();
  line->setFocus(Qt::OtherFocusReason);
  return e;
}

void CListViewWnd::EndLabelEdit(bool accept)
{
  CLabelEditWnd *e = LabelEdit;
  if (!e || e->Ending)
    return;
  e->Ending = true;
  const int row = LabelItem;
  const QString text = e->Text();
  LabelEdit = NULL;
  LabelItem = -1;
  CWindowLock keep(this);
  if (Valid(row))
  {
    std::wstring w = text.toStdWString();
    NMLVDISPINFOW nm;
    memset(&nm, 0, sizeof(nm));
    nm.item.mask = LVIF_TEXT | LVIF_PARAM;
    nm.item.iItem = row;
    nm.item.lParam = Items[(size_t)row].Param;
    nm.item.pszText = accept ? &w[0] : NULL;
    nm.item.cchTextMax = (int)w.size() + 1;
    const bool hadFocus = e->Widget && e->Widget->hasFocus();
    if (hadFocus && FromHandle(this))
      View()->setFocus(Qt::OtherFocusReason);
    const LRESULT r = NotifyParent(&nm.hdr, LVN_ENDLABELEDITW);
    // TRUE: the new text is the label (for items that keep their texts)
    if (r && accept && FromHandle(this) && Valid(row) && !Items[(size_t)row].IsCallback(0))
    {
      Items[(size_t)row].Texts[0] = text;
      Model->RowsChanged(row, row);
    }
  }
  if (FromHandle(e))
    DestroyWindowTree(e);
}

bool CLabelEditWnd::OnEvent(QObject *watched, QEvent *event)
{
  if (watched == Widget.data() && event->type() == QEvent::FocusOut && List && !Ending)
  {
    const QFocusEvent *f = static_cast<QFocusEvent *>(event);
    // a popup (the context menu of the edit) does not end the editing
    if (f->reason() != Qt::PopupFocusReason && FromHandle(List))
      List->EndLabelEdit(true);
  }
  return false;
}

LRESULT CLabelEditWnd::Proc(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_KEYDOWN && List && FromHandle(List))
  {
    if (wParam == VK_RETURN)
    {
      List->EndLabelEdit(true);
      return 0;
    }
    if (wParam == VK_ESCAPE)
    {
      List->EndLabelEdit(false);
      return 0;
    }
  }
  if (message == WM_GETDLGCODE)
    return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_HASSETSEL;
  if (message == EM_SETSEL)
  {
    const int start = (int)(INT_PTR)wParam;
    int end = (int)lParam;
    const int len = (int)Line()->text().size();
    if (end < 0 || end > len)
      end = len;
    if (start < 0)
      Line()->deselect();
    else
      Line()->setSelection(start, end - start);
    return 0;
  }
  if (message == EM_LIMITTEXT)
  {
    Line()->setMaxLength(wParam ? (int)wParam : 32767);
    return 0;
  }
  return DefWindowProcImpl(this, message, wParam, lParam);
}

// ---------- Events ----------

bool CListViewWnd::OnEvent(QObject *watched, QEvent *event)
{
  QAbstractItemView *v = View();
  if (!v)
    return false;
  const bool onViewport = (watched == Tree->viewport() || watched == Icons->viewport());
  switch (event->type())
  {
    case QEvent::Paint:
      if (onViewport && watched == v->viewport())
        PrePaint();
      return false;
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
      // the user acts on the selection that the states give
      if (SyncPending)
        SyncToView();
      return false;
    case QEvent::MouseButtonRelease:
    {
      if (!onViewport)
        return false;
      const QMouseEvent *e = static_cast<QMouseEvent *>(event);
      const QPoint pos = e->position().toPoint();
      if (e->button() == Qt::LeftButton)
      {
        NotifyMouse(NM_CLICK, pos);
        if ((LvExStyle & LVS_EX_ONECLICKACTIVATE) && FromHandle(this) && ItemAt(pos) >= 0
            && !(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
          NotifyMouse(LVN_ITEMACTIVATE, pos);
      }
      else if (e->button() == Qt::RightButton)
        NotifyMouse(NM_RCLICK, pos);
      return false;
    }
    case QEvent::MouseButtonDblClick:
    {
      if (!onViewport)
        return false;
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
    case QEvent::ContextMenu:
    {
      if (!onViewport && watched != v)
        return false;
      // WM_CONTEXTMENU: screen coordinates, or -1, -1 from the keyboard
      const QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
      LPARAM pos = MAKELPARAM(0xFFFF, 0xFFFF);
      if (e->reason() == QContextMenuEvent::Mouse)
        pos = MAKELPARAM(e->globalPos().x(), e->globalPos().y());
      Send(this, WM_CONTEXTMENU, (WPARAM)(HWND)this, pos);
      return true;
    }
    default:
      return false;
  }
}

LRESULT CListViewWnd::Proc(UINT message, WPARAM wParam, LPARAM lParam)
{
  const int index = (int)(INT_PTR)wParam;
  switch (message)
  {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
      // keys with Alt are notified too (Alt+Enter, Alt+digit, Alt+arrows)
      NMLVKEYDOWN nm;
      memset(&nm, 0, sizeof(nm));
      nm.wVKey = (WORD)wParam;
      NotifyParent(&nm.hdr, LVN_KEYDOWN);
      // Alt+Enter activates the items too (Properties in 7zFM and Explorer)
      if (!FromHandle(this) || (message == WM_SYSKEYDOWN && wParam != VK_RETURN))
        return 0;
      switch (wParam)
      {
        case VK_RETURN:
          if (Count() > 0 && Valid(Focused))
          {
            NMHDR hdr;
            memset(&hdr, 0, sizeof(hdr));
            if (NotifyParent(&hdr, NM_RETURN) == 0 && FromHandle(this))
              NotifyActivate(Focused);
          }
          return 0;
        case VK_TAB:
          // the list does not move the focus itself; its parent does
          return 0;
        default:
          InputDefault = true;
          return 0;
      }
    }
    case WM_SYSCHAR:
      // no incremental search with Alt; Qt has the menu mnemonics already
      return 0;
    case LVM_GETITEMCOUNT:
      return Count();
    case LVM_INSERTITEMW:
      return InsertItem((const LVITEMW *)lParam);
    case LVM_SETITEMW:
      return SetItem((const LVITEMW *)lParam) ? TRUE : FALSE;
    case LVM_GETITEMW:
      return GetItem((LVITEMW *)lParam) ? TRUE : FALSE;
    case LVM_SETITEMTEXTW:
    {
      if (!Valid(index))
        return FALSE;
      LVITEMW lv = *(const LVITEMW *)lParam;
      lv.mask = LVIF_TEXT;
      lv.iItem = index;
      return SetItem(&lv) ? TRUE : FALSE;
    }
    case LVM_GETITEMTEXTW:
    {
      LVITEMW *lv = (LVITEMW *)lParam;
      return CopyToW(Valid(index) ? ItemText(index, lv->iSubItem) : QString(), lv->pszText, lv->cchTextMax);
    }
    case LVM_DELETEITEM:
      return DeleteItem(index) ? TRUE : FALSE;
    case LVM_DELETEALLITEMS:
      DeleteAll();
      return TRUE;
    case LVM_SETITEMCOUNT:
      Items.reserve((size_t)qMax(0, index));
      return TRUE;
    case LVM_GETNEXTITEM:
      return NextItem(index, LOWORD(lParam));
    case LVM_GETSELECTEDCOUNT:
      return SelectedCount();
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
          SetState(i, lv->state, lv->stateMask & ~(UINT)LVIS_FOCUSED);
        return TRUE;
      }
      if (!Valid(index))
        return FALSE;
      SetState(index, lv->state, lv->stateMask);
      return TRUE;
    }
    case LVM_GETITEMSTATE:
      return Valid(index) ? (LRESULT)(StateOf(index) & (UINT)lParam) : 0;
    case LVM_GETCALLBACKMASK:
      return CallbackMaskStates;
    case LVM_SETCALLBACKMASK:
      CallbackMaskStates = (UINT)wParam;
      return TRUE;
    case LVM_INSERTCOLUMNW:
      return InsertColumn(index, (const LVCOLUMNW *)lParam);
    case LVM_SETCOLUMNW:
      if (index < 0 || index >= NumColumns())
        return FALSE;
      SetColumn(index, (const LVCOLUMNW *)lParam);
      return TRUE;
    case LVM_GETCOLUMNW:
    {
      LVCOLUMNW *c = (LVCOLUMNW *)lParam;
      if (index < 0 || index >= NumColumns())
        return FALSE;
      const CLvColumn &col = Columns[(size_t)index];
      if (c->mask & LVCF_FMT)
        c->fmt = col.Fmt;
      if (c->mask & LVCF_WIDTH)
        c->cx = FromScreen(ColumnWidth(index));
      if (c->mask & LVCF_TEXT)
        CopyToW(col.Text, c->pszText, c->cchTextMax);
      if (c->mask & LVCF_SUBITEM)
        c->iSubItem = index;
      if (c->mask & LVCF_ORDER)
        c->iOrder = Tree->header()->visualIndex(index);
      return TRUE;
    }
    case LVM_DELETECOLUMN:
      return DeleteColumn(index) ? TRUE : FALSE;
    case LVM_SETCOLUMNWIDTH:
      SetColumnWidth(index, (short)LOWORD(lParam));
      return TRUE;
    case LVM_GETCOLUMNWIDTH:
      return FromScreen(ColumnWidth(index));
    case LVM_SETCOLUMNORDERARRAY:
    {
      const int *order = (const int *)lParam;
      QHeaderView *h = Tree->header();
      for (int vi = 0; vi < (int)wParam && vi < NumColumns(); vi++)
        if (order[vi] >= 0 && order[vi] < NumColumns())
          h->moveSection(h->visualIndex(order[vi]), vi);
      return TRUE;
    }
    case LVM_GETCOLUMNORDERARRAY:
    {
      int *order = (int *)lParam;
      for (int vi = 0; vi < (int)wParam && vi < NumColumns(); vi++)
        order[vi] = Tree->header()->logicalIndex(vi);
      return TRUE;
    }
    case LVM_SORTITEMS:
    {
      const PFNLVCOMPARE compare = (PFNLVCOMPARE)lParam;
      const LPARAM param = (LPARAM)wParam;
      Sort([=](const CLvItem &a, int, const CLvItem &b, int) { return compare(a.Param, b.Param, param); });
      return TRUE;
    }
    case LVM_SORTITEMSEX:
    {
      // the compare function gets the indexes before sorting
      const PFNLVCOMPARE compare = (PFNLVCOMPARE)lParam;
      const LPARAM param = (LPARAM)wParam;
      Sort([=](const CLvItem &, int a, const CLvItem &, int b) { return compare(a, b, param); });
      return TRUE;
    }
    case LVM_SETEXTENDEDLISTVIEWSTYLE:
    {
      const DWORD prev = LvExStyle;
      const DWORD mask = wParam ? (DWORD)wParam : 0xFFFFFFFF;
      LvExStyle = (LvExStyle & ~mask) | ((DWORD)lParam & mask);
      ApplyExStyle(prev);
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
          Tree->setIconSize(QSize(list->Cx, list->Cy));
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
      ApplyViewType();
      Model->RowsChanged(0, Count() - 1);
      return (LRESULT)prev;
    }
    case LVM_GETIMAGELIST:
      return (LRESULT)(wParam == LVSIL_SMALL ? Small : wParam == LVSIL_NORMAL ? Normal : StateList);
    case LVM_ENSUREVISIBLE:
      if (Valid(index))
      {
        if (SyncPending)
          SyncToView();
        View()->scrollTo(Model->index(index, 0), QAbstractItemView::EnsureVisible);
      }
      return TRUE;
    case LVM_GETITEMRECT:
    case LVM_GETSUBITEMRECT:
    {
      RECT *r = (RECT *)lParam;
      if (!Valid(index) || !r)
        return FALSE;
      QRect q;
      if (message == LVM_GETSUBITEMRECT && r->top > 0 && IsReport())
        q = ToClient(Tree->visualRect(Model->index(index, r->top)));
      else
        q = ItemRect(index, r->left);
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
      Model->RowsChanged(qMax(0, index), qMin((int)lParam, Count() - 1));
      View()->viewport()->update();
      return TRUE;
    case LVM_UPDATE:
      View()->viewport()->update();
      return TRUE;
    case LVM_ARRANGE:
      if (!IsReport())
        Icons->doItemsLayout();
      return TRUE;
    case LVM_GETTOPINDEX:
    {
      const QModelIndex i = View()->indexAt(QPoint(1, 1));
      return i.isValid() ? i.row() : 0;
    }
    case LVM_GETCOUNTPERPAGE:
    {
      const int h = Count() > 0 ? View()->visualRect(Model->index(0, 0)).height() : Widget->fontMetrics().height() + 4;
      return h > 0 ? View()->viewport()->height() / h : 0;
    }
    case LVM_SCROLL:
    {
      QAbstractItemView *v = View();
      v->horizontalScrollBar()->setValue(v->horizontalScrollBar()->value() + index);
      const int h = Count() > 0 ? v->visualRect(Model->index(0, 0)).height() : Widget->fontMetrics().height() + 4;
      v->verticalScrollBar()->setValue(v->verticalScrollBar()->value() + (int)lParam / qMax(1, h));
      return TRUE;
    }
    case LVM_FINDITEMW:
    {
      const LVFINDINFOW *f = (const LVFINDINFOW *)lParam;
      const QString s = (f->flags & (LVFI_STRING | LVFI_PARTIAL)) ? FromW(f->psz) : QString();
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
        if (f->flags & LVFI_PARAM)
        {
          if (Items[(size_t)i].Param == f->lParam)
            return i;
        }
        else if (f->flags & LVFI_PARTIAL)
        {
          if (ItemText(i, 0).startsWith(s, Qt::CaseInsensitive))
            return i;
        }
        else if (QString::compare(ItemText(i, 0), s, Qt::CaseInsensitive) == 0)
          return i;
      }
      return -1;
    }
    case LVM_GETSTRINGWIDTHW:
      return Widget->fontMetrics().horizontalAdvance(FromW((LPCWSTR)lParam));
    case LVM_SETUNICODEFORMAT:
    case LVM_GETUNICODEFORMAT:
      return TRUE;
    case LVM_GETBKCOLOR:
    case LVM_GETTEXTBKCOLOR:
      return BkColor();
    case LVM_GETTEXTCOLOR:
    {
      const QColor c = View()->palette().color(QPalette::Text);
      return RGB(c.red(), c.green(), c.blue());
    }
    case LVM_SETBKCOLOR:
    case LVM_SETTEXTCOLOR:
    case LVM_SETTEXTBKCOLOR:
      // the theme decides the colors
      return TRUE;
    case LVM_EDITLABELW:
      return (LRESULT)BeginLabelEdit(index);
    case LVM_GETEDITCONTROL:
      return (LRESULT)(HWND)LabelEdit;
    case LVM_GETHEADER:
      return (LRESULT)(HWND)Header;
    case WM_SETFOCUS:
      if (LabelEdit && LabelEdit->Widget)
        return 0;
      View()->setFocus(Qt::OtherFocusReason);
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
  QFrame *frame = new QFrame(parent);
  RegisterWindow(w, frame);
  if ((exStyle & WS_EX_CLIENTEDGE) || (style & WS_BORDER))
  {
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setFrameShadow(QFrame::Sunken);
  }
  QStackedLayout *stack = new QStackedLayout(frame);
  const int fw = frame->frameWidth();
  stack->setContentsMargins(fw, fw, fw, fw);
  w->Stack = stack;
  w->Model = new CListModel(w, frame);
  w->Selection = new QItemSelectionModel(w->Model, frame);
  CListDelegate *delegate = new CListDelegate(w, frame);

  CTreeView *t = new CTreeView(frame);
  t->Wnd = w;
  t->setModel(w->Model);
  t->setSelectionModel(w->Selection);
  t->setItemDelegate(delegate);
  t->setFrameShape(QFrame::NoFrame);
  t->setRootIsDecorated(false);
  t->setItemsExpandable(false);
  t->setUniformRowHeights(true);
  t->setAllColumnsShowFocus(true);
  t->setSortingEnabled(false);
  t->setTextElideMode(Qt::ElideRight);
  t->setIconSize(QSize(16, 16));
  QHeaderView *h = t->header();
  h->setStretchLastSection(false);
  h->setMinimumSectionSize(0);
  h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  h->setSectionsClickable(!(style & LVS_NOSORTHEADER));
  h->setSectionsMovable(false);
  h->setHighlightSections(false);
  h->setContextMenuPolicy(Qt::DefaultContextMenu);
  t->setHeaderHidden((style & LVS_NOCOLUMNHEADER) != 0);
  stack->addWidget(t);
  w->Tree = t;

  CIconView *l = new CIconView(frame);
  l->Wnd = w;
  l->setModel(w->Model);
  l->setSelectionModel(w->Selection);
  l->setItemDelegate(delegate);
  l->setFrameShape(QFrame::NoFrame);
  l->setTextElideMode(Qt::ElideRight);
  stack->addWidget(l);
  w->Icons = l;

  for (QAbstractItemView *v : { (QAbstractItemView *)t, (QAbstractItemView *)l })
  {
    v->setSelectionMode((style & LVS_SINGLESEL) ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // drags of selected items become LVN_BEGINDRAG (startDrag)
    v->setDragEnabled(true);
    v->setDragDropMode(QAbstractItemView::DragOnly);
    v->setFocusPolicy((style & WS_TABSTOP) ? Qt::StrongFocus : Qt::ClickFocus);
    v->setContextMenuPolicy(Qt::DefaultContextMenu);
    v->installEventFilter(w->Proxy);
    v->viewport()->installEventFilter(w->Proxy);
  }
  frame->installEventFilter(w->Proxy);

  // the header is a window of its own, a child of the list view
  CHeaderWnd *hw = new CHeaderWnd();
  hw->Style = WS_CHILD | WS_VISIBLE;
  hw->List = w;
  RegisterWindow(hw, h);
  h->installEventFilter(hw->Proxy);
  hw->Parent = w;
  w->Children.push_back(hw);
  w->Header = hw;

  Connect(w, w->Selection, &QItemSelectionModel::selectionChanged,
      [](CListViewWnd *self, const QItemSelection &selected, const QItemSelection &deselected)
  {
    self->OnViewSelection(selected, deselected);
  });
  Connect(w, w->Selection, &QItemSelectionModel::currentChanged,
      [](CListViewWnd *self, const QModelIndex &current, const QModelIndex &)
  {
    self->OnViewCurrent(current);
  });
  Connect(w, h, &QHeaderView::sectionClicked, [](CListViewWnd *self, int section)
  {
    NMLISTVIEW nm;
    memset(&nm, 0, sizeof(nm));
    nm.iItem = -1;
    nm.iSubItem = section;
    self->NotifyParent(&nm.hdr, LVN_COLUMNCLICK);
  });
  Connect(w, h, &QHeaderView::sectionResized, [](CListViewWnd *self, int section, int, int newSize)
  {
    if (section >= 0 && section < self->NumColumns())
      self->Columns[(size_t)section].Width = newSize;
  });
  w->ApplyViewType();
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
      if (w.pszText == LPSTR_TEXTCALLBACKW)
        a->pszText = LPSTR_TEXTCALLBACKA;
      else if (w.pszText && text)
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
    case LVM_EDITLABELA:
      return Send(wnd, LVM_EDITLABELW, wParam, lParam);
    default:
      handled = false;
      return 0;
  }
}

}
