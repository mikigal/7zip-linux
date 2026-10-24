// ShimTaskbar.cpp -- the taskbar list of the Windows shell (CLSID_TaskbarList,
// ITaskbarList3), which the progress dialog of 7-Zip drives: the progress
// goes to the task manager of the desktop through
// com.canonical.Unity.LauncherEntry (the Plasma task manager and Dash to
// Dock show it on the program's task), for the .desktop file named by the
// app ID. Without a session bus nothing is shown.

#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QVariantMap>

#include <windows.h>
#include <ShObjIdl.h>

#include "../../CPP/Common/MyCom.h"

#include "../../CPP/7zip/UI/FileManager/MyWindowsNew.h"

#include "ShimWindow.h"

namespace NShim {

/* IID_ITaskbarList3 is defined by the programs (GUI/GUI.cpp for 7zG,
   linux/fm/ClassDefsShell.cpp for 7zFM); the value is compared here. */
static const GUID kIID_ITaskbarList3 =
    { 0xEA1AFB91, 0x9E28, 0x4B86, { 0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEF, 0xAF } };

// application://<the .desktop file of this program>
static QString AppUri()
{
  QString name = QGuiApplication::desktopFileName();
  if (name.isEmpty())
    return QString();
  if (!name.endsWith(QLatin1String(".desktop")))
    name += QLatin1String(".desktop");
  return QStringLiteral("application://") + name;
}

static void SendLauncherEntry(const QVariantMap &properties)
{
  QDBusConnection bus = QDBusConnection::sessionBus();
  const QString uri = AppUri();
  if (!bus.isConnected() || uri.isEmpty())
    return;
  const QString path = QStringLiteral("/com/canonical/unity/launcherentry/%1").arg(qHash(uri));
  QDBusMessage m = QDBusMessage::createSignal(path, QStringLiteral("com.canonical.Unity.LauncherEntry"),
      QStringLiteral("Update"));
  m << uri << properties;
  bus.send(m);
}

class CTaskbarList Z7_final:
  public ITaskbarList3
{
public:
  // IUnknown with the IDs of ITaskbarList, ITaskbarList2 and ITaskbarList3
  STDMETHOD(QueryInterface)(REFIID iid, void **outObject) throw() Z7_override
  {
    if (iid == IID_IUnknown || iid == IID_ITaskbarList || iid == IID_ITaskbarList2 || iid == kIID_ITaskbarList3)
    {
      *outObject = (ITaskbarList3 *)this;
      AddRef();
      return S_OK;
    }
    *outObject = NULL;
    return E_NOINTERFACE;
  }
  STDMETHOD_(ULONG, AddRef)() throw() Z7_override { return ++_m_RefCount; }
  STDMETHOD_(ULONG, Release)() throw() Z7_override
  {
    if (--_m_RefCount != 0)
      return _m_RefCount;
    delete this;
    return 0;
  }

  STDMETHOD(HrInit)() Z7_override { return S_OK; }
  STDMETHOD(AddTab)(HWND) Z7_override { return S_OK; }
  STDMETHOD(DeleteTab)(HWND) Z7_override { return S_OK; }
  STDMETHOD(ActivateTab)(HWND) Z7_override { return S_OK; }
  STDMETHOD(SetActiveAlt)(HWND) Z7_override { return S_OK; }
  STDMETHOD(MarkFullscreenWindow)(HWND, BOOL) Z7_override { return S_OK; }

  STDMETHOD(SetProgressValue)(HWND, ULONGLONG completed, ULONGLONG total) Z7_override
  {
    const double progress = total ? qBound(0.0, (double)completed / (double)total, 1.0) : 0.0;
    // the dialog sets the value at every update; the desktop needs fewer
    const int percent = (int)(progress * 1000);
    if (percent == _lastPermille && _visible)
      return S_OK;
    _lastPermille = percent;
    _visible = true;
    QVariantMap p;
    p[QStringLiteral("progress")] = progress;
    p[QStringLiteral("progress-visible")] = true;
    RunOnGuiThread([=]() { SendLauncherEntry(p); });
    return S_OK;
  }

  STDMETHOD(SetProgressState)(HWND, TBPFLAG flags) Z7_override
  {
    QVariantMap p;
    if (flags == TBPF_NOPROGRESS)
    {
      _visible = false;
      _lastPermille = -1;
      p[QStringLiteral("progress-visible")] = false;
    }
    // errors ask for attention, as the red bar does on Windows
    p[QStringLiteral("urgent")] = (flags & TBPF_ERROR) != 0;
    RunOnGuiThread([=]() { SendLauncherEntry(p); });
    return S_OK;
  }

  STDMETHOD(RegisterTab)(HWND, HWND) Z7_override { return S_OK; }
  STDMETHOD(UnregisterTab)(HWND) Z7_override { return S_OK; }
  STDMETHOD(SetTabOrder)(HWND, HWND) Z7_override { return S_OK; }
  STDMETHOD(SetTabActive)(HWND, HWND, DWORD) Z7_override { return S_OK; }
  STDMETHOD(ThumbBarAddButtons)(HWND, UINT, LPTHUMBBUTTON) Z7_override { return S_OK; }
  STDMETHOD(ThumbBarUpdateButtons)(HWND, UINT, LPTHUMBBUTTON) Z7_override { return S_OK; }
  STDMETHOD(ThumbBarSetImageList)(HWND, HIMAGELIST) Z7_override { return S_OK; }
  STDMETHOD(SetOverlayIcon)(HWND, HICON, LPCWSTR) Z7_override { return S_OK; }
  STDMETHOD(SetThumbnailTooltip)(HWND, LPCWSTR) Z7_override { return S_OK; }
  STDMETHOD(SetThumbnailClip)(HWND, RECT *) Z7_override { return S_OK; }

  CTaskbarList(): _m_RefCount(0), _lastPermille(-1), _visible(false) {}

private:
  ULONG _m_RefCount;
  int _lastPermille;
  bool _visible;
};

// The COM classes of the shim: the taskbar list.
HRESULT CreateShimObject(REFCLSID clsid, REFIID iid, void **object)
{
  if (clsid != CLSID_TaskbarList)
    return REGDB_E_CLASSNOTREG;
  CMyComPtr<IUnknown> unk = (ITaskbarList3 *)new CTaskbarList;
  return unk->QueryInterface(iid, object);
}

}
