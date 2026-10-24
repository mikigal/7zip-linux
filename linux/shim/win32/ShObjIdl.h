/* ShObjIdl.h -- the shell interfaces that FileManager/MyWindowsNew.h
   expects from the SDK: ITaskbarList and ITaskbarList2 (MyWindowsNew.h
   defines ITaskbarList3 on top of them). CoCreateInstance() has no taskbar
   class yet, so the progress dialog runs without taskbar progress. */

#ifndef Z7_LINUX_SHIM_SHOBJIDL_H
#define Z7_LINUX_SHIM_SHOBJIDL_H

#include <windows.h>
#include <CommCtrl.h>

#ifndef __ITaskbarList_INTERFACE_DEFINED__
#define __ITaskbarList_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITaskbarList, 0x56FDF342, 0xFD6D, 0x11d0, 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90);
DEFINE_GUID(CLSID_TaskbarList, 0x56FDF344, 0xFD6D, 0x11d0, 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90);
struct ITaskbarList: public IUnknown
{
  STDMETHOD(HrInit)(void) = 0;
  STDMETHOD(AddTab)(HWND hwnd) = 0;
  STDMETHOD(DeleteTab)(HWND hwnd) = 0;
  STDMETHOD(ActivateTab)(HWND hwnd) = 0;
  STDMETHOD(SetActiveAlt)(HWND hwnd) = 0;
};
#endif

#ifndef __ITaskbarList2_INTERFACE_DEFINED__
#define __ITaskbarList2_INTERFACE_DEFINED__
DEFINE_GUID(IID_ITaskbarList2, 0x602D4995, 0xB13A, 0x429b, 0xA6, 0x6E, 0x19, 0x35, 0xE4, 0x4F, 0x43, 0x17);
struct ITaskbarList2: public ITaskbarList
{
  STDMETHOD(MarkFullscreenWindow)(HWND hwnd, BOOL fFullscreen) = 0;
};
#endif

#endif
