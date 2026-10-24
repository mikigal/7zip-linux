/* oleidl.h -- the OLE data transfer and drag-and-drop interfaces of the
   7-Zip file manager (FileManager/PanelDrag.cpp, Windows/Shell.h, the
   Explorer context menu). objbase.h includes it, as <ole2.h> does on
   Windows. linux/shim implements drag and drop over Qt (ShimDragDrop.cpp):
   RegisterDragDrop() gives a window's IDropTarget the Qt drops, with a data
   object over their QMimeData, and DoDragDrop() runs a QDrag with the data
   object of the source. */

#ifndef Z7_LINUX_SHIM_OLEIDL_H
#define Z7_LINUX_SHIM_OLEIDL_H

#include "winuser.h"

typedef WORD CLIPFORMAT;
typedef DWORD DROPEFFECT;

#define DROPEFFECT_NONE   0
#define DROPEFFECT_COPY   1
#define DROPEFFECT_MOVE   2
#define DROPEFFECT_LINK   4
#define DROPEFFECT_SCROLL 0x80000000

#define DRAGDROP_S_DROP   ((HRESULT)0x00040100L)
#define DRAGDROP_S_CANCEL ((HRESULT)0x00040101L)
#define DRAGDROP_S_USEDEFAULTCURSORS ((HRESULT)0x00040102L)
#define DRAGDROP_E_NOTREGISTERED     ((HRESULT)0x80040100L)
#define DRAGDROP_E_ALREADYREGISTERED ((HRESULT)0x80040101L)

#define DVASPECT_CONTENT 1
#define TYMED_NULL    0
#define TYMED_HGLOBAL 1
#define TYMED_FILE    2
#define TYMED_ISTREAM 4

#define DATADIR_GET 1
#define DATADIR_SET 2

typedef struct tagDVTARGETDEVICE
{
  DWORD tdSize;
  WORD tdDriverNameOffset;
  WORD tdDeviceNameOffset;
  WORD tdPortNameOffset;
  WORD tdExtDevmodeOffset;
  BYTE tdData[1];
} DVTARGETDEVICE;

#define DV_E_FORMATETC           ((HRESULT)0x80040064L)
#define DV_E_DVTARGETDEVICE      ((HRESULT)0x80040065L)
#define DV_E_STGMEDIUM           ((HRESULT)0x80040066L)
#define DV_E_LINDEX              ((HRESULT)0x80040068L)
#define DV_E_TYMED               ((HRESULT)0x80040069L)
#define DV_E_DVASPECT            ((HRESULT)0x8004006BL)
#define DATA_E_FORMATETC         DV_E_FORMATETC
#define OLE_E_ADVISENOTSUPPORTED ((HRESULT)0x80040003L)
#ifndef E_UNEXPECTED
#define E_UNEXPECTED             ((HRESULT)0x8000FFFFL)
#endif

// the key state of drag and drop calls
#define MK_LBUTTON  0x0001
#define MK_RBUTTON  0x0002
#define MK_SHIFT    0x0004
#define MK_CONTROL  0x0008
#define MK_MBUTTON  0x0010
#define MK_ALT      0x0020

typedef struct tagFORMATETC
{
  CLIPFORMAT cfFormat;
  DVTARGETDEVICE *ptd;
  DWORD dwAspect;
  LONG lindex;
  DWORD tymed;
} FORMATETC, *LPFORMATETC;

typedef struct tagSTGMEDIUM
{
  DWORD tymed;
  union
  {
    HBITMAP hBitmap;
    HANDLE hGlobal;
    LPOLESTR lpszFileName;
    IUnknown *pstm;
  };
  IUnknown *pUnkForRelease;
} STGMEDIUM, *LPSTGMEDIUM;

struct IAdviseSink;
struct IEnumSTATDATA;
typedef IAdviseSink *LPADVISESINK;
typedef IEnumSTATDATA *LPENUMSTATDATA;

struct IEnumFORMATETC: public IUnknown
{
  STDMETHOD(Next)(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched) = 0;
  STDMETHOD(Skip)(ULONG celt) = 0;
  STDMETHOD(Reset)() = 0;
  STDMETHOD(Clone)(IEnumFORMATETC **ppenum) = 0;
};
typedef IEnumFORMATETC *LPENUMFORMATETC;

struct IDataObject: public IUnknown
{
  STDMETHOD(GetData)(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) = 0;
  STDMETHOD(GetDataHere)(FORMATETC *pformatetc, STGMEDIUM *pmedium) = 0;
  STDMETHOD(QueryGetData)(FORMATETC *pformatetc) = 0;
  STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) = 0;
  STDMETHOD(SetData)(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) = 0;
  STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) = 0;
  STDMETHOD(DAdvise)(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) = 0;
  STDMETHOD(DUnadvise)(DWORD dwConnection) = 0;
  STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise) = 0;
};
typedef IDataObject *LPDATAOBJECT;

typedef struct _POINTL
{
  LONG x;
  LONG y;
} POINTL;

struct IDropTarget: public IUnknown
{
  STDMETHOD(DragEnter)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) = 0;
  STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) = 0;
  STDMETHOD(DragLeave)() = 0;
  STDMETHOD(Drop)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) = 0;
};
typedef IDropTarget *LPDROPTARGET;

struct IDropSource: public IUnknown
{
  STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD grfKeyState) = 0;
  STDMETHOD(GiveFeedback)(DWORD dwEffect) = 0;
};
typedef IDropSource *LPDROPSOURCE;

/* RegisterDragDrop(): the window accepts drops. Qt drop events become
   calls of (target); a drag from this process gives the target the source's
   data object, a drag from another program a data object with CF_HDROP for
   its file URLs, or for the Ark protocol (archives of 7-Zip or Ark): the
   folder that the target sets with "7-Zip::SetTargetFolder" goes to the
   source through org.kde.ark.DndExtract.
   DoDragDrop(): a QDrag. Files of the file system go as text/uri-list;
   items that the source extracts (k_SourceFlags_TempFiles of
   "7-Zip::GetTransfer") go through the Ark protocol, so that the target
   names the folder and the source extracts into it after the drop. */
HRESULT RegisterDragDrop(HWND wnd, IDropTarget *target);
HRESULT RevokeDragDrop(HWND wnd);
HRESULT DoDragDrop(IDataObject *dataObject, IDropSource *dropSource, DWORD okEffects, DWORD *effect);
void ReleaseStgMedium(STGMEDIUM *medium);

#include "oleidlguids.h"

#endif
