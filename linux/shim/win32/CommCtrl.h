/* CommCtrl.h -- the common-controls subset used by the 7-Zip resource
   scripts and GUI code on Linux. Constants have their Win32 values (see
   windows.h). */

#ifndef Z7_LINUX_SHIM_COMMCTRL_H
#define Z7_LINUX_SHIM_COMMCTRL_H

#include <windows.h>

/* Window classes */
#define WC_LISTVIEWA      "SysListView32"
#define PROGRESS_CLASSA   "msctls_progress32"
#define UPDOWN_CLASSA     "msctls_updown32"
#define TRACKBAR_CLASSA   "msctls_trackbar32"

/* List view styles */
#define LVS_ICON            0x0000
#define LVS_REPORT          0x0001
#define LVS_SMALLICON       0x0002
#define LVS_LIST            0x0003
#define LVS_TYPEMASK        0x0003
#define LVS_SINGLESEL       0x0004
#define LVS_SHOWSELALWAYS   0x0008
#define LVS_SORTASCENDING   0x0010
#define LVS_SORTDESCENDING  0x0020
#define LVS_SHAREIMAGELISTS 0x0040
#define LVS_NOLABELWRAP     0x0080
#define LVS_AUTOARRANGE     0x0100
#define LVS_EDITLABELS      0x0200
#define LVS_OWNERDRAWFIXED  0x0400
#define LVS_ALIGNTOP        0x0000
#define LVS_ALIGNLEFT       0x0800
#define LVS_OWNERDATA       0x1000
#define LVS_NOSCROLL        0x2000
#define LVS_NOCOLUMNHEADER  0x4000
#define LVS_NOSORTHEADER    0x8000

/* Up-down control styles */
#define UDS_WRAP        0x0001
#define UDS_SETBUDDYINT 0x0002
#define UDS_ALIGNRIGHT  0x0004
#define UDS_ALIGNLEFT   0x0008
#define UDS_AUTOBUDDY   0x0010
#define UDS_ARROWKEYS   0x0020
#define UDS_HORZ        0x0040
#define UDS_NOTHOUSANDS 0x0080
#define UDS_HOTTRACK    0x0100

/* Progress bar styles */
#define PBS_SMOOTH   0x01
#define PBS_VERTICAL 0x04
#define PBS_MARQUEE  0x08

/* Trackbar styles */
#define TBS_AUTOTICKS      0x0001
#define TBS_VERT           0x0002
#define TBS_HORZ           0x0000
#define TBS_TOP            0x0004
#define TBS_BOTTOM         0x0000
#define TBS_LEFT           0x0004
#define TBS_RIGHT          0x0000
#define TBS_BOTH           0x0008
#define TBS_NOTICKS        0x0010
#define TBS_ENABLESELRANGE 0x0020
#define TBS_FIXEDLENGTH    0x0040
#define TBS_NOTHUMB        0x0080
#define TBS_TOOLTIPS       0x0100

/* Common control messages */
#define CCM_FIRST             0x2000
#define CCM_SETBKCOLOR        (CCM_FIRST + 1)
#define CCM_SETUNICODEFORMAT  (CCM_FIRST + 5)
#define CCM_GETUNICODEFORMAT  (CCM_FIRST + 6)

/* Notification code ranges */
#define NM_FIRST   (0U - 0U)
#define LVN_FIRST  (0U - 100U)
#define HDN_FIRST  (0U - 300U)
#define UDN_FIRST  (0U - 721U)
#define CBEN_FIRST (0U - 800U)

#define NM_OUTOFMEMORY (NM_FIRST - 1)
#define NM_CLICK       (NM_FIRST - 2)
#define NM_DBLCLK      (NM_FIRST - 3)
#define NM_RETURN      (NM_FIRST - 4)
#define NM_RCLICK      (NM_FIRST - 5)
#define NM_RDBLCLK     (NM_FIRST - 6)
#define NM_SETFOCUS    (NM_FIRST - 7)
#define NM_KILLFOCUS   (NM_FIRST - 8)
#define NM_CUSTOMDRAW  (NM_FIRST - 12)
#define NM_HOVER       (NM_FIRST - 13)

/* List view messages */
#define LVM_FIRST                    0x1000
#define LVM_GETBKCOLOR               (LVM_FIRST + 0)
#define LVM_SETBKCOLOR               (LVM_FIRST + 1)
#define LVM_GETIMAGELIST             (LVM_FIRST + 2)
#define LVM_SETIMAGELIST             (LVM_FIRST + 3)
#define LVM_GETITEMCOUNT             (LVM_FIRST + 4)
#define LVM_GETITEMA                 (LVM_FIRST + 5)
#define LVM_SETITEMA                 (LVM_FIRST + 6)
#define LVM_INSERTITEMA              (LVM_FIRST + 7)
#define LVM_DELETEITEM               (LVM_FIRST + 8)
#define LVM_DELETEALLITEMS           (LVM_FIRST + 9)
#define LVM_GETCALLBACKMASK          (LVM_FIRST + 10)
#define LVM_SETCALLBACKMASK          (LVM_FIRST + 11)
#define LVM_GETNEXTITEM              (LVM_FIRST + 12)
#define LVM_FINDITEMA                (LVM_FIRST + 13)
#define LVM_GETITEMRECT              (LVM_FIRST + 14)
#define LVM_GETSTRINGWIDTHA          (LVM_FIRST + 17)
#define LVM_HITTEST                  (LVM_FIRST + 18)
#define LVM_ENSUREVISIBLE            (LVM_FIRST + 19)
#define LVM_SCROLL                   (LVM_FIRST + 20)
#define LVM_REDRAWITEMS              (LVM_FIRST + 21)
#define LVM_ARRANGE                  (LVM_FIRST + 22)
#define LVM_EDITLABELA               (LVM_FIRST + 23)
#define LVM_GETEDITCONTROL           (LVM_FIRST + 24)
#define LVM_GETCOLUMNA               (LVM_FIRST + 25)
#define LVM_SETCOLUMNA               (LVM_FIRST + 26)
#define LVM_INSERTCOLUMNA            (LVM_FIRST + 27)
#define LVM_DELETECOLUMN             (LVM_FIRST + 28)
#define LVM_GETCOLUMNWIDTH           (LVM_FIRST + 29)
#define LVM_SETCOLUMNWIDTH           (LVM_FIRST + 30)
#define LVM_GETHEADER                (LVM_FIRST + 31)
#define LVM_GETTEXTCOLOR             (LVM_FIRST + 35)
#define LVM_SETTEXTCOLOR             (LVM_FIRST + 36)
#define LVM_GETTEXTBKCOLOR           (LVM_FIRST + 37)
#define LVM_SETTEXTBKCOLOR           (LVM_FIRST + 38)
#define LVM_GETTOPINDEX              (LVM_FIRST + 39)
#define LVM_GETCOUNTPERPAGE          (LVM_FIRST + 40)
#define LVM_UPDATE                   (LVM_FIRST + 42)
#define LVM_SETITEMSTATE             (LVM_FIRST + 43)
#define LVM_GETITEMSTATE             (LVM_FIRST + 44)
#define LVM_GETITEMTEXTA             (LVM_FIRST + 45)
#define LVM_SETITEMTEXTA             (LVM_FIRST + 46)
#define LVM_SETITEMCOUNT             (LVM_FIRST + 47)
#define LVM_SORTITEMS                (LVM_FIRST + 48)
#define LVM_GETSELECTEDCOUNT         (LVM_FIRST + 50)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVM_GETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 55)
#define LVM_GETSUBITEMRECT           (LVM_FIRST + 56)
#define LVM_SUBITEMHITTEST           (LVM_FIRST + 57)
#define LVM_SETCOLUMNORDERARRAY      (LVM_FIRST + 58)
#define LVM_GETCOLUMNORDERARRAY      (LVM_FIRST + 59)
#define LVM_GETSELECTIONMARK         (LVM_FIRST + 66)
#define LVM_SETSELECTIONMARK         (LVM_FIRST + 67)
#define LVM_GETITEMW                 (LVM_FIRST + 75)
#define LVM_SETITEMW                 (LVM_FIRST + 76)
#define LVM_INSERTITEMW              (LVM_FIRST + 77)
#define LVM_SORTITEMSEX              (LVM_FIRST + 81)
#define LVM_FINDITEMW                (LVM_FIRST + 83)
#define LVM_GETSTRINGWIDTHW          (LVM_FIRST + 87)
#define LVM_GETCOLUMNW               (LVM_FIRST + 95)
#define LVM_SETCOLUMNW               (LVM_FIRST + 96)
#define LVM_INSERTCOLUMNW            (LVM_FIRST + 97)
#define LVM_GETITEMTEXTW             (LVM_FIRST + 115)
#define LVM_SETITEMTEXTW             (LVM_FIRST + 116)
#define LVM_EDITLABELW               (LVM_FIRST + 118)
#define LVM_SETUNICODEFORMAT         CCM_SETUNICODEFORMAT
#define LVM_GETUNICODEFORMAT         CCM_GETUNICODEFORMAT
/* Windows builds without UNICODE use the narrow messages */
#define LVM_GETITEM       LVM_GETITEMA
#define LVM_SETITEM       LVM_SETITEMA
#define LVM_INSERTITEM    LVM_INSERTITEMA
#define LVM_FINDITEM      LVM_FINDITEMA
#define LVM_EDITLABEL     LVM_EDITLABELA
#define LVM_GETCOLUMN     LVM_GETCOLUMNA
#define LVM_SETCOLUMN     LVM_SETCOLUMNA
#define LVM_INSERTCOLUMN  LVM_INSERTCOLUMNA
#define LVM_GETITEMTEXT   LVM_GETITEMTEXTA
#define LVM_SETITEMTEXT   LVM_SETITEMTEXTA
#define LVM_GETSTRINGWIDTH LVM_GETSTRINGWIDTHA

/* List view notifications */
#define LVN_ITEMCHANGING     (LVN_FIRST - 0)
#define LVN_ITEMCHANGED      (LVN_FIRST - 1)
#define LVN_INSERTITEM       (LVN_FIRST - 2)
#define LVN_DELETEITEM       (LVN_FIRST - 3)
#define LVN_DELETEALLITEMS   (LVN_FIRST - 4)
#define LVN_BEGINLABELEDITA  (LVN_FIRST - 5)
#define LVN_ENDLABELEDITA    (LVN_FIRST - 6)
#define LVN_COLUMNCLICK      (LVN_FIRST - 8)
#define LVN_BEGINDRAG        (LVN_FIRST - 9)
#define LVN_BEGINRDRAG       (LVN_FIRST - 11)
#define LVN_ODCACHEHINT      (LVN_FIRST - 13)
#define LVN_ITEMACTIVATE     (LVN_FIRST - 14)
#define LVN_ODSTATECHANGED   (LVN_FIRST - 15)
#define LVN_HOTTRACK         (LVN_FIRST - 21)
#define LVN_GETDISPINFOA     (LVN_FIRST - 50)
#define LVN_SETDISPINFOA     (LVN_FIRST - 51)
#define LVN_ODFINDITEMA      (LVN_FIRST - 52)
#define LVN_KEYDOWN          (LVN_FIRST - 55)
#define LVN_MARQUEEBEGIN     (LVN_FIRST - 56)
#define LVN_GETINFOTIPA      (LVN_FIRST - 57)
#define LVN_BEGINLABELEDITW  (LVN_FIRST - 75)
#define LVN_ENDLABELEDITW    (LVN_FIRST - 76)
#define LVN_GETDISPINFOW     (LVN_FIRST - 77)
#define LVN_SETDISPINFOW     (LVN_FIRST - 78)
#define LVN_ODFINDITEMW      (LVN_FIRST - 79)
#define LVN_BEGINLABELEDIT   LVN_BEGINLABELEDITA
#define LVN_ENDLABELEDIT     LVN_ENDLABELEDITA
#define LVN_GETDISPINFO      LVN_GETDISPINFOA
#define LVN_SETDISPINFO      LVN_SETDISPINFOA
#define LVN_ODFINDITEM       LVN_ODFINDITEMA

/* List view items, columns and states */
#define LVIF_TEXT        0x0001
#define LVIF_IMAGE       0x0002
#define LVIF_PARAM       0x0004
#define LVIF_STATE       0x0008
#define LVIF_INDENT      0x0010
#define LVIF_NORECOMPUTE 0x0800
#define LVIF_DI_SETITEM  0x1000
#define LVIS_FOCUSED        0x0001
#define LVIS_SELECTED       0x0002
#define LVIS_CUT            0x0004
#define LVIS_DROPHILITED    0x0008
#define LVIS_OVERLAYMASK    0x0F00
#define LVIS_STATEIMAGEMASK 0xF000
#define INDEXTOSTATEIMAGEMASK(i) ((i) << 12)
#define LVNI_ALL         0x0000
#define LVNI_FOCUSED     0x0001
#define LVNI_SELECTED    0x0002
#define LVNI_CUT         0x0004
#define LVNI_DROPHILITED 0x0008
#define LVNI_ABOVE       0x0100
#define LVNI_BELOW       0x0200
#define LVNI_TOLEFT      0x0400
#define LVNI_TORIGHT     0x0800
#define LVCF_FMT     0x0001
#define LVCF_WIDTH   0x0002
#define LVCF_TEXT    0x0004
#define LVCF_SUBITEM 0x0008
#define LVCF_IMAGE   0x0010
#define LVCF_ORDER   0x0020
#define LVCFMT_LEFT        0x0000
#define LVCFMT_RIGHT       0x0001
#define LVCFMT_CENTER      0x0002
#define LVCFMT_JUSTIFYMASK 0x0003
#define LVSIL_NORMAL 0
#define LVSIL_SMALL  1
#define LVSIL_STATE  2
#define LVSCW_AUTOSIZE           (-1)
#define LVSCW_AUTOSIZE_USEHEADER (-2)
#define LVIR_BOUNDS       0
#define LVIR_ICON         1
#define LVIR_LABEL        2
#define LVIR_SELECTBOUNDS 3
#define LVHT_NOWHERE        0x0001
#define LVHT_ONITEMICON     0x0002
#define LVHT_ONITEMLABEL    0x0004
#define LVHT_ONITEMSTATEICON 0x0008
#define LVHT_ONITEM         (LVHT_ONITEMICON | LVHT_ONITEMLABEL | LVHT_ONITEMSTATEICON)
#define LVHT_ABOVE          0x0008
#define LVHT_BELOW          0x0010
#define LVHT_TORIGHT        0x0020
#define LVHT_TOLEFT         0x0040
#define LVSICF_NOINVALIDATEALL 0x0001
#define LVSICF_NOSCROLL        0x0002
#define LVKF_ALT     0x0001
#define LVKF_CONTROL 0x0002
#define LVKF_SHIFT   0x0004
#define LVFI_PARAM   0x0001
#define LVFI_STRING  0x0002
#define LVFI_PARTIAL 0x0008
#define LVFI_WRAP    0x0020
#define LPSTR_TEXTCALLBACKA ((LPSTR)-1L)
#define LPSTR_TEXTCALLBACKW ((LPWSTR)-1L)
#define I_IMAGECALLBACK (-1)

/* Extended list view styles */
#define LVS_EX_GRIDLINES        0x00000001
#define LVS_EX_SUBITEMIMAGES    0x00000002
#define LVS_EX_CHECKBOXES       0x00000004
#define LVS_EX_TRACKSELECT      0x00000008
#define LVS_EX_HEADERDRAGDROP   0x00000010
#define LVS_EX_FULLROWSELECT    0x00000020
#define LVS_EX_ONECLICKACTIVATE 0x00000040
#define LVS_EX_TWOCLICKACTIVATE 0x00000080
#define LVS_EX_FLATSB           0x00000100
#define LVS_EX_INFOTIP          0x00000400
#define LVS_EX_UNDERLINEHOT     0x00000800
#define LVS_EX_UNDERLINECOLD    0x00001000
#define LVS_EX_LABELTIP         0x00004000
#define LVS_EX_BORDERSELECT     0x00008000
#define LVS_EX_DOUBLEBUFFER     0x00010000

/* Progress bar messages */
#define PBM_SETRANGE    (WM_USER + 1)
#define PBM_SETPOS      (WM_USER + 2)
#define PBM_DELTAPOS    (WM_USER + 3)
#define PBM_SETSTEP     (WM_USER + 4)
#define PBM_STEPIT      (WM_USER + 5)
#define PBM_SETRANGE32  (WM_USER + 6)
#define PBM_GETRANGE    (WM_USER + 7)
#define PBM_GETPOS      (WM_USER + 8)
#define PBM_SETBARCOLOR (WM_USER + 9)
#define PBM_SETMARQUEE  (WM_USER + 10)
#define PBM_SETSTATE    (WM_USER + 16)
#define PBM_GETSTATE    (WM_USER + 17)
#define PBM_SETBKCOLOR  CCM_SETBKCOLOR
#define PBST_NORMAL 0x0001
#define PBST_ERROR  0x0002
#define PBST_PAUSED 0x0003

/* Up-down control messages and notifications */
#define UDM_SETRANGE   (WM_USER + 101)
#define UDM_GETRANGE   (WM_USER + 102)
#define UDM_SETPOS     (WM_USER + 103)
#define UDM_GETPOS     (WM_USER + 104)
#define UDM_SETBUDDY   (WM_USER + 105)
#define UDM_GETBUDDY   (WM_USER + 106)
#define UDM_SETACCEL   (WM_USER + 107)
#define UDM_SETBASE    (WM_USER + 109)
#define UDM_SETRANGE32 (WM_USER + 111)
#define UDM_GETRANGE32 (WM_USER + 112)
#define UDM_SETPOS32   (WM_USER + 113)
#define UDM_GETPOS32   (WM_USER + 114)
#define UDN_DELTAPOS   (UDN_FIRST - 1)

/* Trackbar messages */
#define TBM_GETPOS      (WM_USER)
#define TBM_GETRANGEMIN (WM_USER + 1)
#define TBM_GETRANGEMAX (WM_USER + 2)
#define TBM_SETPOS      (WM_USER + 5)
#define TBM_SETRANGE    (WM_USER + 6)
#define TBM_SETRANGEMIN (WM_USER + 7)
#define TBM_SETRANGEMAX (WM_USER + 8)
#define TBM_SETTICFREQ  (WM_USER + 20)
#define TBM_SETPAGESIZE (WM_USER + 21)
#define TBM_SETLINESIZE (WM_USER + 23)

/* ComboBoxEx messages and item flags */
#define CBEM_INSERTITEMA      (WM_USER + 1)
#define CBEM_SETIMAGELIST     (WM_USER + 2)
#define CBEM_GETIMAGELIST     (WM_USER + 3)
#define CBEM_GETITEMA         (WM_USER + 4)
#define CBEM_SETITEMA         (WM_USER + 5)
#define CBEM_DELETEITEM       CB_DELETESTRING
#define CBEM_GETCOMBOCONTROL  (WM_USER + 6)
#define CBEM_GETEDITCONTROL   (WM_USER + 7)
#define CBEM_SETEXSTYLE       (WM_USER + 8)
#define CBEM_GETEXTENDEDSTYLE (WM_USER + 9)
#define CBEM_HASEDITCHANGED   (WM_USER + 10)
#define CBEM_INSERTITEMW      (WM_USER + 11)
#define CBEM_SETITEMW         (WM_USER + 12)
#define CBEM_GETITEMW         (WM_USER + 13)
#define CBEM_SETEXTENDEDSTYLE (WM_USER + 14)
#define CBEM_SETUNICODEFORMAT CCM_SETUNICODEFORMAT
#define CBEM_INSERTITEM CBEM_INSERTITEMA
#define CBEM_SETITEM    CBEM_SETITEMA
#define CBEM_GETITEM    CBEM_GETITEMA
#define CBEIF_TEXT          0x00000001
#define CBEIF_IMAGE         0x00000002
#define CBEIF_SELECTEDIMAGE 0x00000004
#define CBEIF_OVERLAY       0x00000008
#define CBEIF_INDENT        0x00000010
#define CBEIF_LPARAM        0x00000020

/* Image lists */
#define ILC_MASK     0x00000001
#define ILC_COLOR    0x00000000
#define ILC_COLORDDB 0x000000FE
#define ILC_COLOR4   0x00000004
#define ILC_COLOR8   0x00000008
#define ILC_COLOR16  0x00000010
#define ILC_COLOR24  0x00000018
#define ILC_COLOR32  0x00000020
#define ILD_NORMAL      0x00000000
#define ILD_TRANSPARENT 0x00000001
#define ILD_MASK        0x00000010

/* InitCommonControlsEx() */
#define ICC_LISTVIEW_CLASSES 0x00000001
#define ICC_TREEVIEW_CLASSES 0x00000002
#define ICC_BAR_CLASSES      0x00000004
#define ICC_TAB_CLASSES      0x00000008
#define ICC_UPDOWN_CLASS     0x00000010
#define ICC_PROGRESS_CLASS   0x00000020
#define ICC_WIN95_CLASSES    0x000000FF
#define ICC_STANDARD_CLASSES 0x00004000

#if !defined(RC_INVOKED) && defined(__cplusplus)

#define WC_LISTVIEWW      L"SysListView32"
#define WC_LISTVIEW       WC_LISTVIEWA
#define WC_COMBOBOXEXA    "ComboBoxEx32"
#define WC_COMBOBOXEXW    L"ComboBoxEx32"
#define WC_COMBOBOXEX     WC_COMBOBOXEXA
#define PROGRESS_CLASS    PROGRESS_CLASSA
#define UPDOWN_CLASS      UPDOWN_CLASSA
#define TRACKBAR_CLASS    TRACKBAR_CLASSA

#define SNDMSG SendMessage

struct _IMAGELIST;
typedef struct _IMAGELIST *HIMAGELIST;

typedef struct tagINITCOMMONCONTROLSEX
{
  DWORD dwSize;
  DWORD dwICC;
} INITCOMMONCONTROLSEX, *LPINITCOMMONCONTROLSEX;

void InitCommonControls();
BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX *icc);

typedef struct tagLVITEMA
{
  UINT mask;
  int iItem;
  int iSubItem;
  UINT state;
  UINT stateMask;
  LPSTR pszText;
  int cchTextMax;
  int iImage;
  LPARAM lParam;
  int iIndent;
} LVITEMA, LV_ITEMA;

typedef struct tagLVITEMW
{
  UINT mask;
  int iItem;
  int iSubItem;
  UINT state;
  UINT stateMask;
  LPWSTR pszText;
  int cchTextMax;
  int iImage;
  LPARAM lParam;
  int iIndent;
} LVITEMW, LV_ITEMW;

typedef LVITEMA LVITEM;
typedef LVITEMA LV_ITEM;

typedef struct tagLVCOLUMNA
{
  UINT mask;
  int fmt;
  int cx;
  LPSTR pszText;
  int cchTextMax;
  int iSubItem;
  int iImage;
  int iOrder;
} LVCOLUMNA, LV_COLUMNA;

typedef struct tagLVCOLUMNW
{
  UINT mask;
  int fmt;
  int cx;
  LPWSTR pszText;
  int cchTextMax;
  int iSubItem;
  int iImage;
  int iOrder;
} LVCOLUMNW, LV_COLUMNW;

typedef LVCOLUMNA LVCOLUMN;
typedef LVCOLUMNA LV_COLUMN;

typedef struct tagLVHITTESTINFO
{
  POINT pt;
  UINT flags;
  int iItem;
  int iSubItem;
} LVHITTESTINFO, LV_HITTESTINFO, *LPLVHITTESTINFO;

typedef struct tagLVFINDINFOA
{
  UINT flags;
  LPCSTR psz;
  LPARAM lParam;
  POINT pt;
  UINT vkDirection;
} LVFINDINFOA;

typedef struct tagLVFINDINFOW
{
  UINT flags;
  LPCWSTR psz;
  LPARAM lParam;
  POINT pt;
  UINT vkDirection;
} LVFINDINFOW;

typedef struct tagNMLISTVIEW
{
  NMHDR hdr;
  int iItem;
  int iSubItem;
  UINT uNewState;
  UINT uOldState;
  UINT uChanged;
  POINT ptAction;
  LPARAM lParam;
} NMLISTVIEW, NM_LISTVIEW, *LPNMLISTVIEW;

typedef struct tagNMITEMACTIVATE
{
  NMHDR hdr;
  int iItem;
  int iSubItem;
  UINT uNewState;
  UINT uOldState;
  UINT uChanged;
  POINT ptAction;
  LPARAM lParam;
  UINT uKeyFlags;
} NMITEMACTIVATE, *LPNMITEMACTIVATE;

typedef struct tagLVKEYDOWN
{
  NMHDR hdr;
  WORD wVKey;
  UINT flags;
} NMLVKEYDOWN, LV_KEYDOWN, *LPNMLVKEYDOWN;

typedef struct tagLVDISPINFOA
{
  NMHDR hdr;
  LVITEMA item;
} NMLVDISPINFOA, LV_DISPINFOA;

typedef struct tagLVDISPINFOW
{
  NMHDR hdr;
  LVITEMW item;
} NMLVDISPINFOW, LV_DISPINFOW;

typedef NMLVDISPINFOA NMLVDISPINFO;
typedef NMLVDISPINFOA LV_DISPINFO;

typedef struct tagNMLVODSTATECHANGE
{
  NMHDR hdr;
  int iFrom;
  int iTo;
  UINT uNewState;
  UINT uOldState;
} NMLVODSTATECHANGE, *LPNMLVODSTATECHANGE;

typedef struct tagNMUPDOWN
{
  NMHDR hdr;
  int iPos;
  int iDelta;
} NMUPDOWN, *LPNMUPDOWN;

typedef struct tagPBRANGE
{
  int iLow;
  int iHigh;
} PBRANGE, *PPBRANGE;

typedef int (CALLBACK *PFNLVCOMPARE)(LPARAM, LPARAM, LPARAM);

typedef struct tagCOMBOBOXEXITEMA
{
  UINT mask;
  INT_PTR iItem;
  LPSTR pszText;
  int cchTextMax;
  int iImage;
  int iSelectedImage;
  int iOverlay;
  int iIndent;
  LPARAM lParam;
} COMBOBOXEXITEMA;

typedef struct tagCOMBOBOXEXITEMW
{
  UINT mask;
  INT_PTR iItem;
  LPWSTR pszText;
  int cchTextMax;
  int iImage;
  int iSelectedImage;
  int iOverlay;
  int iIndent;
  LPARAM lParam;
} COMBOBOXEXITEMW;

typedef COMBOBOXEXITEMA COMBOBOXEXITEM;

typedef struct _IMAGEINFO
{
  HBITMAP hbmImage;
  HBITMAP hbmMask;
  int Unused1;
  int Unused2;
  RECT rcImage;
} IMAGEINFO;

HIMAGELIST ImageList_Create(int cx, int cy, UINT flags, int initial, int grow);
BOOL ImageList_Destroy(HIMAGELIST list);
int ImageList_GetImageCount(HIMAGELIST list);
BOOL ImageList_GetImageInfo(HIMAGELIST list, int index, IMAGEINFO *info);
int ImageList_Add(HIMAGELIST list, HBITMAP image, HBITMAP mask);
int ImageList_AddMasked(HIMAGELIST list, HBITMAP image, COLORREF mask);
int ImageList_ReplaceIcon(HIMAGELIST list, int index, HICON icon);
#define ImageList_AddIcon(list, icon) ImageList_ReplaceIcon(list, -1, icon)
BOOL ImageList_Remove(HIMAGELIST list, int index);
#define ImageList_RemoveAll(list) ImageList_Remove(list, -1)
HICON ImageList_GetIcon(HIMAGELIST list, int index, UINT flags);
#define ImageList_ExtractIcon(instance, list, index) ImageList_GetIcon(list, index, 0)
BOOL ImageList_GetIconSize(HIMAGELIST list, int *cx, int *cy);
BOOL ImageList_SetIconSize(HIMAGELIST list, int cx, int cy);
COLORREF ImageList_SetBkColor(HIMAGELIST list, COLORREF color);

/* Functions rather than macros where callers drop the result, so that GCC
   does not warn about an unused cast value. */
inline BOOL ListView_SetUnicodeFormat(HWND w, BOOL f)
  { return (BOOL)SNDMSG(w, LVM_SETUNICODEFORMAT, (WPARAM)f, 0); }
inline DWORD ListView_SetExtendedListViewStyle(HWND w, DWORD style)
  { return (DWORD)SNDMSG(w, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)style); }
inline DWORD ListView_SetExtendedListViewStyleEx(HWND w, DWORD mask, DWORD style)
  { return (DWORD)SNDMSG(w, LVM_SETEXTENDEDLISTVIEWSTYLE, (WPARAM)mask, (LPARAM)style); }
#define ListView_DeleteAllItems(w) \
  (BOOL)SNDMSG((w), LVM_DELETEALLITEMS, 0, 0)
#define ListView_DeleteColumn(w, col) \
  (BOOL)SNDMSG((w), LVM_DELETECOLUMN, (WPARAM)(int)(col), 0)
#define ListView_InsertColumn(w, col, pcol) \
  (int)SNDMSG((w), LVM_INSERTCOLUMN, (WPARAM)(int)(col), (LPARAM)(const LV_COLUMN *)(pcol))
#define ListView_SetColumnOrderArray(w, count, pi) \
  (BOOL)SNDMSG((w), LVM_SETCOLUMNORDERARRAY, (WPARAM)(count), (LPARAM)(int *)(pi))
#define ListView_InsertItem(w, pitem) \
  (int)SNDMSG((w), LVM_INSERTITEM, 0, (LPARAM)(const LV_ITEM *)(pitem))
#define ListView_SetItem(w, pitem) \
  (BOOL)SNDMSG((w), LVM_SETITEM, 0, (LPARAM)(const LV_ITEM *)(pitem))
#define ListView_DeleteItem(w, i) \
  (BOOL)SNDMSG((w), LVM_DELETEITEM, (WPARAM)(int)(i), 0)
#define ListView_GetSelectedCount(w) \
  (UINT)SNDMSG((w), LVM_GETSELECTEDCOUNT, 0, 0)
#define ListView_GetItemCount(w) \
  (int)SNDMSG((w), LVM_GETITEMCOUNT, 0, 0)
#define ListView_GetSelectionMark(w) \
  (int)SNDMSG((w), LVM_GETSELECTIONMARK, 0, 0)
#define ListView_SetItemCount(w, n) \
  SNDMSG((w), LVM_SETITEMCOUNT, (WPARAM)(n), 0)
#define ListView_SetItemCountEx(w, n, flags) \
  SNDMSG((w), LVM_SETITEMCOUNT, (WPARAM)(n), (LPARAM)(flags))
#define ListView_GetNextItem(w, i, flags) \
  (int)SNDMSG((w), LVM_GETNEXTITEM, (WPARAM)(int)(i), MAKELPARAM((flags), 0))
#define ListView_GetItem(w, pitem) \
  (BOOL)SNDMSG((w), LVM_GETITEM, 0, (LPARAM)(LV_ITEM *)(pitem))
#define ListView_SortItems(w, compare, param) \
  (BOOL)SNDMSG((w), LVM_SORTITEMS, (WPARAM)(LPARAM)(param), (LPARAM)(PFNLVCOMPARE)(compare))
#define ListView_SetItemState(w, i, data, mask) \
  { LV_ITEM ms_lvi_; ms_lvi_.stateMask = (mask); ms_lvi_.state = (data); \
    SNDMSG((w), LVM_SETITEMSTATE, (WPARAM)(i), (LPARAM)(LV_ITEM *)&ms_lvi_); }
#define ListView_GetItemState(w, i, mask) \
  (UINT)SNDMSG((w), LVM_GETITEMSTATE, (WPARAM)(i), (LPARAM)(mask))
#define ListView_GetColumn(w, col, pcol) \
  (BOOL)SNDMSG((w), LVM_GETCOLUMN, (WPARAM)(int)(col), (LPARAM)(LV_COLUMN *)(pcol))
#define ListView_SetImageList(w, list, type) \
  (HIMAGELIST)SNDMSG((w), LVM_SETIMAGELIST, (WPARAM)(type), (LPARAM)(HIMAGELIST)(list))
#define ListView_GetExtendedListViewStyle(w) \
  (DWORD)SNDMSG((w), LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0)
#define ListView_SetCheckState(w, i, check) \
  ListView_SetItemState(w, i, INDEXTOSTATEIMAGEMASK((check) ? 2 : 1), LVIS_STATEIMAGEMASK)
#define ListView_GetCheckState(w, i) \
  ((((UINT)(SNDMSG((w), LVM_GETITEMSTATE, (WPARAM)(i), LVIS_STATEIMAGEMASK))) >> 12) - 1)
#define ListView_EnsureVisible(w, i, partialOK) \
  (BOOL)SNDMSG((w), LVM_ENSUREVISIBLE, (WPARAM)(int)(i), MAKELPARAM((partialOK), 0))
#define ListView_GetItemRect(w, i, prc, code) \
  (BOOL)SNDMSG((w), LVM_GETITEMRECT, (WPARAM)(int)(i), \
    ((prc) ? (((RECT *)(prc))->left = (code), (LPARAM)(RECT *)(prc)) : (LPARAM)(RECT *)NULL))
#define ListView_GetEditControl(w) \
  (HWND)SNDMSG((w), LVM_GETEDITCONTROL, 0, 0)
#define ListView_EditLabel(w, i) \
  (HWND)SNDMSG((w), LVM_EDITLABEL, (WPARAM)(int)(i), 0)
#define ListView_RedrawItems(w, first, last) \
  (BOOL)SNDMSG((w), LVM_REDRAWITEMS, (WPARAM)(int)(first), (LPARAM)(last))
#define ListView_HitTest(w, pinfo) \
  (int)SNDMSG((w), LVM_HITTEST, 0, (LPARAM)(LV_HITTESTINFO *)(pinfo))
#define ListView_GetBkColor(w) \
  (COLORREF)SNDMSG((w), LVM_GETBKCOLOR, 0, 0)
#define ListView_SetColumnWidth(w, col, cx) \
  (BOOL)SNDMSG((w), LVM_SETCOLUMNWIDTH, (WPARAM)(int)(col), MAKELPARAM((cx), 0))
#define ListView_GetColumnWidth(w, col) \
  (int)SNDMSG((w), LVM_GETCOLUMNWIDTH, (WPARAM)(int)(col), 0)
#define ListView_GetTopIndex(w) \
  (int)SNDMSG((w), LVM_GETTOPINDEX, 0, 0)
#define ListView_GetCountPerPage(w) \
  (int)SNDMSG((w), LVM_GETCOUNTPERPAGE, 0, 0)
#define ListView_Update(w, i) \
  (BOOL)SNDMSG((w), LVM_UPDATE, (WPARAM)(i), 0)

#endif /* !RC_INVOKED && __cplusplus */

#endif
