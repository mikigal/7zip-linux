/* objbase.h -- the COM and OLE calls of the 7-Zip GUI code. windows.h
   includes it for C++, as <windows.h> includes <ole2.h> on Windows.
   linux/shim implements it (ShimSystem.cpp). */

#ifndef Z7_LINUX_SHIM_OBJBASE_H
#define Z7_LINUX_SHIM_OBJBASE_H

#include "winuser.h"
#include "oleidl.h"

#define CLSCTX_INPROC_SERVER  0x1
#define CLSCTX_INPROC_HANDLER 0x2
#define CLSCTX_LOCAL_SERVER   0x4
#define CLSCTX_ALL            (CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER)

#ifndef REGDB_E_CLASSNOTREG
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)
#endif

HRESULT CoInitialize(LPVOID reserved);
void CoUninitialize();
HRESULT OleInitialize(LPVOID reserved);
void OleUninitialize();

/* No COM classes are registered yet: it returns REGDB_E_CLASSNOTREG, and
   callers go on without the object (the taskbar list, for example). */
HRESULT CoCreateInstance(REFCLSID clsid, IUnknown *outer, DWORD clsContext, REFIID iid, LPVOID *object);

LPVOID CoTaskMemAlloc(size_t size);
void CoTaskMemFree(LPVOID p);


/* Windows/COM.h has CComInitializer and CStgMedium for _WIN32 only; the
   shell and drag and drop code of the file manager uses them. */
namespace NWindows {
namespace NCOM {
class CComInitializer
{
public:
  CComInitializer() { CoInitialize(NULL); }
  ~CComInitializer() { CoUninitialize(); }
};

// as in Windows/COM.h (_WIN32 only there): a medium released at the end
struct CStgMedium: public STGMEDIUM
{
  CStgMedium()
  {
    tymed = TYMED_NULL;
    hGlobal = NULL;
    pUnkForRelease = NULL;
  }
  ~CStgMedium()
  {
    ReleaseStgMedium(this);
  }
};
}}

#endif
