/* oleidl.h -- the OLE data transfer and drag-and-drop interfaces that the
   7-Zip file manager declares its classes with (FileManager/App.h,
   Windows/Shell.h, the Explorer context menu). objbase.h includes it, as
   <ole2.h> does on Windows.

   Drag and drop on Linux goes through Qt (Phase 6 of linux/docs/PLAN.md),
   not through these interfaces: RegisterDragDrop() and DoDragDrop() do
   nothing, and no data object is ever passed to the 7-Zip code. */

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

#define DVASPECT_CONTENT 1
#define TYMED_NULL    0
#define TYMED_HGLOBAL 1
#define TYMED_FILE    2
#define TYMED_ISTREAM 4

#define DATADIR_GET 1
#define DATADIR_SET 2

typedef struct tagDVTARGETDEVICE DVTARGETDEVICE;

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

struct IEnumFORMATETC: public IUnknown
{
  STDMETHOD(Next)(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched) = 0;
  STDMETHOD(Skip)(ULONG celt) = 0;
  STDMETHOD(Reset)() = 0;
  STDMETHOD(Clone)(IEnumFORMATETC **ppenum) = 0;
};

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

HRESULT RegisterDragDrop(HWND wnd, IDropTarget *target);
HRESULT RevokeDragDrop(HWND wnd);
HRESULT DoDragDrop(IDataObject *dataObject, IDropSource *dropSource, DWORD okEffects, DWORD *effect);
void ReleaseStgMedium(STGMEDIUM *medium);

#endif
