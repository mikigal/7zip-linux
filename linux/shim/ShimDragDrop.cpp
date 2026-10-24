// ShimDragDrop.cpp -- OLE drag and drop over Qt, for FileManager/
// PanelDrag.cpp: global memory blocks (HGLOBAL), clipboard format names,
// file lists (CF_HDROP), windows at a point, RegisterDragDrop() and
// DoDragDrop().
//
// Drops: RegisterDragDrop() makes the window accept Qt drops, and passes
// them to its IDropTarget with a data object over the QMimeData. A drag of
// this process gives the target the source's own data object, so the
// 7-Zip formats ("7-Zip::SetTargetFolder", "...SetTransfer",
// "...GetTransfer") work as between 7-Zip windows on Windows. A drag of
// another program offers CF_HDROP with its local files, or, for the Ark
// protocol (Ark, another 7zFM), CF_HDROP without files: the folder that the
// target sets with "7-Zip::SetTargetFolder" goes to the source through
// org.kde.ark.DndExtract.extractSelectedFilesTo() after the drop.
//
// Drags: DoDragDrop() runs a QDrag. Files of the file system are offered as
// text/uri-list. Items that the source still has to extract (archive items:
// k_SourceFlags_TempFiles of "7-Zip::GetTransfer") are offered through the
// Ark protocol only, as Ark does: Dolphin reads text/uri-list while the
// pointer moves over it, and that would extract them before the drop. The
// target calls extractSelectedFilesTo(folder); the folder becomes the
// "SetTargetFolder" of the source, which extracts into it after the drag,
// as when a 7-Zip window names the folder on Windows.
//
// Windows calls IDropSource::QueryContinueDrag() when the mouse button is
// released; here that happens before a target of this process gets the
// drop, when the Ark call comes, or when the drag ends with a drop.

#include <QApplication>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDrag>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QMimeData>
#include <QMutex>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <windows.h>
#include <ShlObj.h>

#include "../../CPP/Common/MyCom.h"
#include "../../CPP/Windows/Shell.h"

#include "ShimDialog.h"
#include "ShimWindow.h"

namespace NShim {

// ---------- Global memory ----------

static const UInt32 kGlobalMagic = 0x4C424C47; // "GLBL"

struct CGlobalHeader
{
  SIZE_T Size;
  UInt32 Magic;
  UInt32 Locks;
  UInt64 Reserved[2]; // the data is 16-byte aligned
};

static CGlobalHeader *GlobalHeader(HGLOBAL mem)
{
  CGlobalHeader *h = (CGlobalHeader *)mem;
  return (h && h->Magic == kGlobalMagic) ? h : NULL;
}

static Byte *GlobalData(CGlobalHeader *h)
{
  return (Byte *)h + sizeof(CGlobalHeader);
}

}

using namespace NShim;

HGLOBAL GlobalAlloc(UINT flags, SIZE_T size)
{
  CGlobalHeader *h = (CGlobalHeader *)malloc(sizeof(CGlobalHeader) + size);
  if (!h)
    return NULL;
  memset(h, 0, sizeof(CGlobalHeader));
  h->Size = size;
  h->Magic = kGlobalMagic;
  if (flags & GMEM_ZEROINIT)
    memset(GlobalData(h), 0, size);
  return (HGLOBAL)h;
}

HGLOBAL GlobalReAlloc(HGLOBAL mem, SIZE_T size, UINT flags)
{
  CGlobalHeader *h = GlobalHeader(mem);
  if (!h)
    return NULL;
  const SIZE_T oldSize = h->Size;
  CGlobalHeader *n = (CGlobalHeader *)realloc(h, sizeof(CGlobalHeader) + size);
  if (!n)
    return NULL;
  n->Size = size;
  if ((flags & GMEM_ZEROINIT) && size > oldSize)
    memset(GlobalData(n) + oldSize, 0, size - oldSize);
  return (HGLOBAL)n;
}

LPVOID GlobalLock(HGLOBAL mem)
{
  CGlobalHeader *h = GlobalHeader(mem);
  // as on Windows: a block of zero bytes has no data
  if (!h || h->Size == 0)
    return NULL;
  h->Locks++;
  return GlobalData(h);
}

BOOL GlobalUnlock(HGLOBAL mem)
{
  CGlobalHeader *h = GlobalHeader(mem);
  if (!h || h->Locks == 0)
    return FALSE;
  h->Locks--;
  return h->Locks != 0 ? TRUE : FALSE;
}

SIZE_T GlobalSize(HGLOBAL mem)
{
  CGlobalHeader *h = GlobalHeader(mem);
  return h ? h->Size : 0;
}

HGLOBAL GlobalFree(HGLOBAL mem)
{
  CGlobalHeader *h = GlobalHeader(mem);
  if (h)
  {
    h->Magic = 0;
    free(h);
  }
  return NULL;
}


// ---------- Clipboard formats ----------

namespace NShim {

static const UINT kFirstRegisteredFormat = 0xC000;
static QMutex g_FormatsMutex;
static QStringList g_FormatNames;

static UINT RegisterFormat(const QString &name)
{
  if (name.isEmpty())
    return 0;
  QMutexLocker lock(&g_FormatsMutex);
  // names are compared without case, as on Windows
  for (int i = 0; i < g_FormatNames.size(); i++)
    if (g_FormatNames[i].compare(name, Qt::CaseInsensitive) == 0)
      return kFirstRegisteredFormat + (UINT)i;
  g_FormatNames.append(name);
  return kFirstRegisteredFormat + (UINT)(g_FormatNames.size() - 1);
}

static QString FormatName(UINT format)
{
  QMutexLocker lock(&g_FormatsMutex);
  const int i = (int)format - (int)kFirstRegisteredFormat;
  return (i >= 0 && i < g_FormatNames.size()) ? g_FormatNames[i] : QString();
}

}

UINT RegisterClipboardFormatA(LPCSTR name)
{
  return name ? RegisterFormat(QString::fromLocal8Bit(name)) : 0;
}

UINT RegisterClipboardFormatW(LPCWSTR name)
{
  return name ? RegisterFormat(FromW(name)) : 0;
}

int GetClipboardFormatNameW(UINT format, LPWSTR name, int maxCount)
{
  const QString s = FormatName(format);
  if (s.isEmpty() || !name || maxCount <= 0)
    return 0;
  const UString u = ToU(s);
  const int n = qMin((int)u.Len(), maxCount - 1);
  wmemcpy(name, u.Ptr(), (size_t)n);
  name[n] = 0;
  return n;
}


// ---------- File lists (CF_HDROP) ----------

namespace NShim {

static HGLOBAL CreateHDrop(const QStringList &paths)
{
  size_t numChars = 1; // the empty name at the end
  QList<UString> names;
  for (const QString &p : paths)
  {
    names.append(ToU(p));
    numChars += names.last().Len() + 1;
  }
  HGLOBAL mem = GlobalAlloc(GHND, sizeof(DROPFILES) + numChars * sizeof(wchar_t));
  if (!mem)
    return NULL;
  Byte *data = (Byte *)GlobalLock(mem);
  DROPFILES *df = (DROPFILES *)data;
  df->pFiles = sizeof(DROPFILES);
  df->fWide = TRUE;
  wchar_t *s = (wchar_t *)(data + sizeof(DROPFILES));
  for (const UString &n : names)
  {
    wmemcpy(s, n.Ptr(), n.Len() + 1);
    s += n.Len() + 1;
  }
  *s = 0;
  GlobalUnlock(mem);
  return mem;
}

static QStringList HDropNames(HGLOBAL mem)
{
  QStringList names;
  const SIZE_T size = GlobalSize(mem);
  const Byte *data = (const Byte *)GlobalLock(mem);
  if (!data || size < sizeof(DROPFILES))
  {
    if (data)
      GlobalUnlock(mem);
    return names;
  }
  const DROPFILES *df = (const DROPFILES *)data;
  if (df->pFiles >= sizeof(DROPFILES) && df->pFiles < size)
  {
    if (df->fWide)
    {
      const wchar_t *s = (const wchar_t *)(data + df->pFiles);
      const wchar_t *end = (const wchar_t *)(data + size);
      while (s < end && *s)
      {
        const wchar_t *e = s;
        while (e < end && *e)
          e++;
        names.append(FromW(s, (size_t)(e - s)));
        s = e + 1;
      }
    }
    else
    {
      const char *s = (const char *)(data + df->pFiles);
      const char *end = (const char *)(data + size);
      while (s < end && *s)
      {
        const char *e = s;
        while (e < end && *e)
          e++;
        names.append(QString::fromLocal8Bit(s, (qsizetype)(e - s)));
        s = e + 1;
      }
    }
  }
  GlobalUnlock(mem);
  return names;
}

static HRESULT MediumFromGlobal(STGMEDIUM *medium, HGLOBAL mem)
{
  medium->tymed = TYMED_NULL;
  medium->hGlobal = NULL;
  medium->pUnkForRelease = NULL;
  if (!mem)
    return E_OUTOFMEMORY;
  medium->tymed = TYMED_HGLOBAL;
  medium->hGlobal = mem;
  return S_OK;
}

static HRESULT MediumFromBytes(STGMEDIUM *medium, const QByteArray &bytes)
{
  HGLOBAL mem = GlobalAlloc(GHND, (SIZE_T)bytes.size());
  if (mem && !bytes.isEmpty())
  {
    memcpy(GlobalLock(mem), bytes.constData(), (size_t)bytes.size());
    GlobalUnlock(mem);
  }
  return MediumFromGlobal(medium, mem);
}

static QByteArray GlobalBytes(HGLOBAL mem)
{
  const SIZE_T size = GlobalSize(mem);
  const void *data = GlobalLock(mem);
  if (!data)
    return QByteArray();
  QByteArray b((const char *)data, (qsizetype)size);
  GlobalUnlock(mem);
  return b;
}

// a wchar_t string ended by 0 or by the end of the block
static QString GlobalWideString(HGLOBAL mem)
{
  const QByteArray b = GlobalBytes(mem);
  const wchar_t *s = (const wchar_t *)(const void *)b.constData();
  const size_t max = (size_t)b.size() / sizeof(wchar_t);
  size_t n = 0;
  while (n < max && s[n])
    n++;
  return FromW(s, n);
}

static FORMATETC HGlobalFormat(UINT format)
{
  FORMATETC etc;
  etc.cfFormat = (CLIPFORMAT)format;
  etc.ptd = NULL;
  etc.dwAspect = DVASPECT_CONTENT;
  etc.lindex = -1;
  etc.tymed = TYMED_HGLOBAL;
  return etc;
}

}

UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT size)
{
  const QStringList names = HDropNames((HGLOBAL)drop);
  if (index == 0xFFFFFFFF)
    return (UINT)names.size();
  if (index >= (UINT)names.size())
    return 0;
  const UString u = ToU(names[(int)index]);
  if (!file || size == 0)
    return u.Len();
  const UINT n = qMin((UINT)u.Len(), size - 1);
  wmemcpy(file, u.Ptr(), n);
  file[n] = 0;
  return n;
}

void DragFinish(HDROP /* drop */)
{
  // the block belongs to the medium that GetData() returned
}

void ReleaseStgMedium(STGMEDIUM *medium)
{
  if (!medium)
    return;
  if (medium->pUnkForRelease)
    medium->pUnkForRelease->Release();
  else if (medium->tymed == TYMED_HGLOBAL && medium->hGlobal)
    GlobalFree(medium->hGlobal);
  medium->tymed = TYMED_NULL;
  medium->hGlobal = NULL;
  medium->pUnkForRelease = NULL;
}

namespace NWindows {
namespace NShell {

// Windows/Shell.cpp is not built: CF_HDROP is the only list of names here.
HRESULT DataObject_GetData_HDROP_or_IDLIST_Names(IDataObject *dataObject, UStringVector &names)
{
  names.Clear();
  FORMATETC etc = HGlobalFormat(CF_HDROP);
  STGMEDIUM medium;
  RINOK(dataObject->GetData(&etc, &medium))
  if (medium.tymed != TYMED_HGLOBAL)
  {
    ReleaseStgMedium(&medium);
    return DV_E_TYMED;
  }
  for (const QString &n : HDropNames(medium.hGlobal))
    names.Add(ToU(n));
  ReleaseStgMedium(&medium);
  return S_OK;
}

}}


// ---------- Windows at a point ----------

namespace NShim {

// the window that a drag is over: points are matched against it first
static QPointer<QWidget> g_DragOverWindow;

// The window (HWND) of a widget or of its nearest ancestor that has one.
static HWND WindowOfWidget(QWidget *w)
{
  for (; w; w = w->parentWidget())
  {
    HWND wnd = FromWidget(w);
    if (wnd && wnd->Widget == w)
      return wnd;
  }
  return NULL;
}

}

HWND WindowFromPoint(POINT point)
{
  return OnGuiThread<HWND>([=]()
  {
    const QPoint p(point.x, point.y);
    QWidget *w = NULL;
    if (g_DragOverWindow)
    {
      const QPoint local = g_DragOverWindow->mapFromGlobal(p);
      if (g_DragOverWindow->rect().contains(local))
      {
        w = g_DragOverWindow->childAt(local);
        if (!w)
          w = g_DragOverWindow;
      }
    }
    if (!w)
      w = QApplication::widgetAt(p);
    return WindowOfWidget(w);
  });
}

HWND ChildWindowFromPointEx(HWND parent, POINT point, UINT flags)
{
  return OnGuiThread<HWND>([=]() -> HWND
  {
    HWND p = FromHandle(parent);
    if (!p || !p->Widget)
      return NULL;
    // client coordinates of the parent are its widget's coordinates
    const QPoint pt(point.x, point.y);
    if (!p->Widget->rect().contains(pt))
      return NULL;
    for (HWND c : p->Children)
    {
      if (!FromHandle(c) || !c->Widget)
        continue;
      if ((flags & CWP_SKIPINVISIBLE) && !c->Widget->isVisible())
        continue;
      if ((flags & CWP_SKIPDISABLED) && !c->Widget->isEnabled())
        continue;
      if (c->Widget->geometry().contains(c->Widget->parentWidget() == p->Widget ?
          pt : c->Widget->parentWidget()->mapFrom(p->Widget, pt)))
        return c;
    }
    return parent;
  });
}

HWND ChildWindowFromPoint(HWND parent, POINT point)
{
  return ChildWindowFromPointEx(parent, point, CWP_ALL);
}


// ---------- Drags ----------

namespace NShim {

static const char * const kArkServiceType = "application/x-kde-ark-dndextract-service";
static const char * const kArkPathType = "application/x-kde-ark-dndextract-path";
static const char * const kArkInterface = "org.kde.ark.DndExtract";
static const char * const kArkMethod = "extractSelectedFilesTo";
// a drag of this process (its data goes through the source's data object)
static const char * const kOwnDragType = "application/x-7zip-own-drag";

// "7-Zip::GetTransfer" (CDataObject_GetTransfer of PanelDrag.cpp): Flags
// follow the 64 bytes of CDataObject_TransferBase.
static const UInt32 kGetTransferStructId = 3;
static const size_t kGetTransferFlagsOffset = 64;
static const UInt32 k_SourceFlags_TempFiles = 1 << 6;

static DWORD KeyState(Qt::KeyboardModifiers mods, Qt::MouseButtons buttons)
{
  DWORD k = 0;
  // the button that drags is not reported on every platform
  if ((buttons & Qt::LeftButton) || buttons == Qt::NoButton)
    k |= MK_LBUTTON;
  if (buttons & Qt::RightButton)
    k |= MK_RBUTTON;
  if (buttons & Qt::MiddleButton)
    k |= MK_MBUTTON;
  if (mods & Qt::ShiftModifier)
    k |= MK_SHIFT;
  if (mods & Qt::ControlModifier)
    k |= MK_CONTROL;
  if (mods & Qt::AltModifier)
    k |= MK_ALT;
  return k;
}

static DWORD EffectsOf(Qt::DropActions actions)
{
  DWORD e = DROPEFFECT_NONE;
  if (actions & Qt::CopyAction)
    e |= DROPEFFECT_COPY;
  if (actions & Qt::MoveAction)
    e |= DROPEFFECT_MOVE;
  if (actions & Qt::LinkAction)
    e |= DROPEFFECT_LINK;
  return e;
}

static Qt::DropAction ActionOf(DWORD effect)
{
  if (effect & DROPEFFECT_MOVE)
    return Qt::MoveAction;
  if (effect & DROPEFFECT_COPY)
    return Qt::CopyAction;
  if (effect & DROPEFFECT_LINK)
    return Qt::LinkAction;
  return Qt::IgnoreAction;
}

static Qt::DropActions ActionsOf(DWORD effects)
{
  Qt::DropActions a = Qt::IgnoreAction;
  if (effects & DROPEFFECT_COPY)
    a |= Qt::CopyAction;
  if (effects & DROPEFFECT_MOVE)
    a |= Qt::MoveAction;
  if (effects & DROPEFFECT_LINK)
    a |= Qt::LinkAction;
  return a;
}

/* The state of the drag that DoDragDrop() runs: the source's objects and
   what the targets have done. */
struct CDragContext
{
  CMyComPtr<IDataObject> DataObject;
  CMyComPtr<IDropSource> DropSource;
  bool Released = false;
  HRESULT ReleaseResult = DRAGDROP_S_CANCEL;
  bool ArkRequested = false;  // a target read the Ark protocol types
  bool ArkCalled = false;     // extractSelectedFilesTo() came

  // the mouse button is released over a target that accepts (effect)
  HRESULT Release(DWORD effect, DWORD keyState)
  {
    if (Released)
      return ReleaseResult;
    Released = true;
    DropSource->GiveFeedback(effect);
    ReleaseResult = DropSource->QueryContinueDrag(FALSE, keyState & ~(DWORD)(MK_LBUTTON | MK_RBUTTON | MK_MBUTTON));
    return ReleaseResult;
  }

  HRESULT SetTargetFolder(const QString &folder)
  {
    QString f = folder;
    if (!f.endsWith(QLatin1Char('/')))
      f += QLatin1Char('/');
    const UString u = ToU(f);
    FORMATETC etc = HGlobalFormat(RegisterClipboardFormatA("7-Zip::SetTargetFolder"));
    STGMEDIUM medium;
    MediumFromBytes(&medium, QByteArray((const char *)(const void *)u.Ptr(), (qsizetype)((u.Len() + 1) * sizeof(wchar_t))));
    const HRESULT res = DataObject->SetData(&etc, &medium, FALSE);
    ReleaseStgMedium(&medium);
    return res;
  }
};

static CDragContext *g_Drag;
static CDragHook g_DragHook;

void SetDragHook(const CDragHook &hook)
{
  g_DragHook = hook;
}

// org.kde.ark.DndExtract of the drag of this process
class CArkObject: public QDBusVirtualObject
{
public:
  QString introspect(const QString & /* path */) const override
  {
    return QStringLiteral(
        "<interface name=\"org.kde.ark.DndExtract\">"
        "<method name=\"extractSelectedFilesTo\"><arg name=\"path\" type=\"s\" direction=\"in\"/></method>"
        "</interface>");
  }

  bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
  {
    if (message.interface() != QLatin1String(kArkInterface) || message.member() != QLatin1String(kArkMethod)
        || message.arguments().size() != 1)
      return false;
    QString path = message.arguments()[0].toString();
    // KIO sends a local path or a file: URL
    const QUrl url(path);
    if (url.isLocalFile())
      path = url.toLocalFile();
    if (g_Drag && !g_Drag->ArkCalled && !path.isEmpty())
    {
      g_Drag->ArkCalled = true;
      if (g_Drag->SetTargetFolder(path) == S_OK)
        g_Drag->Release(DROPEFFECT_COPY, 0);
    }
    connection.send(message.createReply());
    return true;
  }
};

// The data of a drag of this process, read through the source's data object.
class CDragMimeData: public QMimeData
{
public:
  CDragContext *Context;
  QStringList Formats;
  QString ArkPath;
  CDragMimeData(CDragContext *context): Context(context) {}

  QStringList formats() const override { return Formats; }
  bool hasFormat(const QString &mimeType) const override { return Formats.contains(mimeType); }

protected:
  QVariant retrieveData(const QString &mimeType, QMetaType type) const override
  {
    if (!Context || !Formats.contains(mimeType))
      return QVariant();
    if (mimeType == QLatin1String("text/uri-list"))
    {
      FORMATETC etc = HGlobalFormat(CF_HDROP);
      STGMEDIUM medium;
      if (Context->DataObject->GetData(&etc, &medium) != S_OK)
        return QVariant();
      const QStringList names = HDropNames(medium.hGlobal);
      ReleaseStgMedium(&medium);
      // urls() asks for a list; the Wayland data source for the bytes
      if (type.id() == QMetaType::QVariantList)
      {
        QVariantList urls;
        for (const QString &n : names)
          urls.append(QUrl::fromLocalFile(n));
        return urls;
      }
      QByteArray list;
      for (const QString &n : names)
        list += QUrl::fromLocalFile(n).toEncoded() + "\r\n";
      return list;
    }
    if (mimeType == QLatin1String(kArkServiceType))
    {
      Context->ArkRequested = true;
      return QDBusConnection::sessionBus().baseService().toUtf8();
    }
    if (mimeType == QLatin1String(kArkPathType))
    {
      Context->ArkRequested = true;
      return ArkPath.toUtf8();
    }
    if (mimeType == QLatin1String(kOwnDragType))
      return QByteArray::number((qlonglong)getpid());
    return QVariant();
  }
};

// Whether the source extracts its items only for the target (archive items).
static bool SourceHasTempFiles(IDataObject *dataObject)
{
  FORMATETC etc = HGlobalFormat(RegisterClipboardFormatA("7-Zip::GetTransfer"));
  STGMEDIUM medium;
  if (dataObject->GetData(&etc, &medium) != S_OK)
    return false;
  bool tempFiles = false;
  const SIZE_T size = GlobalSize(medium.hGlobal);
  const Byte *p = (const Byte *)GlobalLock(medium.hGlobal);
  if (p)
  {
    if (size >= kGetTransferFlagsOffset + 4)
    {
      UInt32 id, flags;
      memcpy(&id, p, 4);
      memcpy(&flags, p + kGetTransferFlagsOffset, 4);
      tempFiles = (id == kGetTransferStructId && (flags & k_SourceFlags_TempFiles) != 0);
    }
    GlobalUnlock(medium.hGlobal);
  }
  ReleaseStgMedium(&medium);
  return tempFiles;
}

}

HRESULT DoDragDrop(IDataObject *dataObject, IDropSource *dropSource, DWORD okEffects, DWORD *effect)
{
  if (effect)
    *effect = DROPEFFECT_NONE;
  if (!dataObject || !dropSource)
    return E_INVALIDARG;
  return OnGuiThread<HRESULT>([=]() -> HRESULT
  {
    if (g_Drag)
      return E_UNEXPECTED;
    CDragContext context;
    context.DataObject = dataObject;
    context.DropSource = dropSource;

    CDragMimeData *mime = new CDragMimeData(&context);
    mime->Formats.append(QString::fromLatin1(kOwnDragType));
    QDBusConnection bus = QDBusConnection::sessionBus();
    static int s_DragNumber = 0;
    const QString arkPath = QStringLiteral("/io/github/mikigal/SevenZip/DndExtract/%1").arg(++s_DragNumber);
    CArkObject ark;
    bool arkRegistered = false;
    if (SourceHasTempFiles(dataObject))
    {
      if (bus.isConnected() && bus.registerVirtualObject(arkPath, &ark))
      {
        arkRegistered = true;
        mime->ArkPath = arkPath;
        mime->Formats.append(QString::fromLatin1(kArkServiceType));
        mime->Formats.append(QString::fromLatin1(kArkPathType));
      }
    }
    else
    {
      FORMATETC etc = HGlobalFormat(CF_HDROP);
      if (dataObject->QueryGetData(&etc) == S_OK)
        mime->Formats.append(QStringLiteral("text/uri-list"));
    }

    QWidget *source = QApplication::activeWindow();
    if (!source)
      source = QApplication::focusWidget();
    QPointer<QDrag> drag = new QDrag(source ? (QObject *)source : (QObject *)qApp);
    drag->setMimeData(mime);
    const QIcon icon = QApplication::windowIcon();
    if (!icon.isNull())
      drag->setPixmap(icon.pixmap(32, 32));
    g_Drag = &context;
    const Qt::DropAction action = g_DragHook ? g_DragHook(drag, ActionsOf(okEffects)) :
        drag->exec(ActionsOf(okEffects), Qt::CopyAction);
    if (drag)
      drag->deleteLater();

    // the Ark call can come after the drop: Dolphin calls it asynchronously
    if (context.ArkRequested && !context.ArkCalled && action != Qt::IgnoreAction)
    {
      QEventLoop loop;
      QElapsedTimer timer;
      timer.start();
      while (!context.ArkCalled && timer.elapsed() < 10000)
        loop.processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 50);
    }
    g_Drag = NULL;
    if (arkRegistered)
      bus.unregisterObject(arkPath);

    DWORD performed = EffectsOf(action);
    if (context.ArkCalled)
      performed = DROPEFFECT_COPY;
    if (performed != DROPEFFECT_NONE && !context.Released)
      context.Release(performed, 0);
    if (effect)
      *effect = performed;
    if (context.Released && context.ReleaseResult == DRAGDROP_S_DROP && performed != DROPEFFECT_NONE)
      return DRAGDROP_S_DROP;
    return DRAGDROP_S_CANCEL;
  });
}


// ---------- Drops ----------

namespace NShim {

/* The data object of a drop: the source's data object for a drag of this
   process, else the files of the QMimeData, or the Ark protocol. */
class CDropDataObject Z7_final:
  public IDataObject,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1(IDataObject)

  STDMETHOD(GetData)(FORMATETC *etc, STGMEDIUM *medium) Z7_override;
  STDMETHOD(GetDataHere)(FORMATETC *, STGMEDIUM *) Z7_override { return E_NOTIMPL; }
  STDMETHOD(QueryGetData)(FORMATETC *etc) Z7_override;
  STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *, FORMATETC *out) Z7_override
  {
    if (out)
      out->ptd = NULL;
    return E_NOTIMPL;
  }
  STDMETHOD(SetData)(FORMATETC *etc, STGMEDIUM *medium, BOOL release) Z7_override;
  STDMETHOD(EnumFormatEtc)(DWORD direction, IEnumFORMATETC **e) Z7_override
  {
    if (Source)
      return Source->EnumFormatEtc(direction, e);
    if (e)
      *e = NULL;
    return E_NOTIMPL;
  }
  STDMETHOD(DAdvise)(FORMATETC *, DWORD, IAdviseSink *, DWORD *) Z7_override { return OLE_E_ADVISENOTSUPPORTED; }
  STDMETHOD(DUnadvise)(DWORD) Z7_override { return OLE_E_ADVISENOTSUPPORTED; }
  STDMETHOD(EnumDAdvise)(IEnumSTATDATA **e) Z7_override
  {
    if (e)
      *e = NULL;
    return OLE_E_ADVISENOTSUPPORTED;
  }

public:
  CMyComPtr<IDataObject> Source;  // a drag of this process
  QStringList Files;              // the local files of another program's drag
  DWORD PreferredEffect = DROPEFFECT_NONE; // of files on the clipboard
  QString ArkService, ArkPath;    // the Ark protocol of another program
  QString TargetFolder;           // "7-Zip::SetTargetFolder" for the Ark source

  void Load(const QMimeData *mime)
  {
    Source.Release();
    Files.clear();
    ArkService.clear();
    ArkPath.clear();
    if (const CDragMimeData *own = dynamic_cast<const CDragMimeData *>(mime))
    {
      if (own->Context)
        Source = own->Context->DataObject;
      return;
    }
    if (mime->hasFormat(QLatin1String(kArkServiceType)) && mime->hasFormat(QLatin1String(kArkPathType)))
    {
      ArkService = QString::fromUtf8(mime->data(QLatin1String(kArkServiceType)));
      ArkPath = QString::fromUtf8(mime->data(QLatin1String(kArkPathType)));
      return;
    }
    for (const QUrl &u : mime->urls())
      if (u.isLocalFile())
        Files.append(u.toLocalFile());
  }

  bool IsArk() const { return !ArkService.isEmpty() && !ArkPath.isEmpty(); }

  void CallArk() const
  {
    if (!IsArk() || TargetFolder.isEmpty())
      return;
    QDBusMessage m = QDBusMessage::createMethodCall(ArkService, ArkPath,
        QString::fromLatin1(kArkInterface), QString::fromLatin1(kArkMethod));
    m << TargetFolder;
    QDBusConnection::sessionBus().asyncCall(m);
  }
};

STDMETHODIMP CDropDataObject::QueryGetData(FORMATETC *etc)
{
  if (Source)
    return Source->QueryGetData(etc);
  if (etc && PreferredEffect != DROPEFFECT_NONE
      && FormatName(etc->cfFormat) == QLatin1String("Preferred DropEffect"))
    return (etc->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
  if (!etc || etc->cfFormat != CF_HDROP)
    return DV_E_FORMATETC;
  if (!(etc->tymed & TYMED_HGLOBAL))
    return DV_E_TYMED;
  return (IsArk() || !Files.isEmpty()) ? S_OK : DV_E_FORMATETC;
}

STDMETHODIMP CDropDataObject::GetData(FORMATETC *etc, STGMEDIUM *medium)
{
  if (Source)
    return Source->GetData(etc, medium);
  if (!medium)
    return E_INVALIDARG;
  medium->tymed = TYMED_NULL;
  medium->hGlobal = NULL;
  medium->pUnkForRelease = NULL;
  RINOK(QueryGetData(etc))
  if (etc->cfFormat != CF_HDROP)
  {
    const DWORD effect = PreferredEffect;
    return MediumFromBytes(medium, QByteArray((const char *)&effect, sizeof(effect)));
  }
  // the Ark source extracts on its own: no names
  return MediumFromGlobal(medium, CreateHDrop(Files));
}

STDMETHODIMP CDropDataObject::SetData(FORMATETC *etc, STGMEDIUM *medium, BOOL release)
{
  if (Source)
    return Source->SetData(etc, medium, release);
  if (!etc || !medium)
    return E_INVALIDARG;
  HRESULT res = E_NOTIMPL;
  if (IsArk() && medium->tymed == TYMED_HGLOBAL
      && FormatName(etc->cfFormat) == QLatin1String("7-Zip::SetTargetFolder"))
  {
    TargetFolder = GlobalWideString(medium->hGlobal);
    res = S_OK;
  }
  /* with (release) the medium is ours even if the format is not known: the
     target of PanelDrag.cpp does not release it then (as Explorer, as a
     source, takes it) */
  if (release)
    ReleaseStgMedium(medium);
  return res;
}


// Gives the drops of a window to its IDropTarget.
class CDropBridge: public QObject
{
public:
  QPointer<QWidget> Widget;
  HWND Wnd;
  CMyComPtr<IDropTarget> Target;
  CMyComPtr<IDataObject> DataObject;
  CDropDataObject *DataObjectSpec;
  DWORD LastEffect;

  CDropBridge(HWND wnd, IDropTarget *target):
      QObject(wnd->Widget), Widget(wnd->Widget), Wnd(wnd), Target(target),
      DataObjectSpec(NULL), LastEffect(DROPEFFECT_NONE)
  {
    Widget->setAcceptDrops(true);
    Widget->installEventFilter(this);
  }

  POINTL Point(const QDropEvent *e) const
  {
    const QPoint p = Widget->mapToGlobal(e->position().toPoint());
    POINTL pt;
    pt.x = p.x();
    pt.y = p.y();
    return pt;
  }

  void Apply(QDropEvent *e, DWORD effect, bool keepEvents)
  {
    LastEffect = effect;
    const Qt::DropAction a = ActionOf(effect);
    if (a == Qt::IgnoreAction)
    {
      // the enter event is accepted: the moves come, and decide
      if (keepEvents)
        e->accept();
      else
        e->ignore();
      return;
    }
    e->setDropAction(a);
    e->accept();
  }

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched != Widget || !Target)
      return false;
    switch (event->type())
    {
      case QEvent::DragEnter:
      {
        QDragEnterEvent *e = static_cast<QDragEnterEvent *>(event);
        g_DragOverWindow = Widget;
        DataObjectSpec = new CDropDataObject;
        DataObject = DataObjectSpec;
        DataObjectSpec->Load(e->mimeData());
        DWORD effect = EffectsOf(e->possibleActions());
        CMyComPtr<IDropTarget> target = Target;
        if (target->DragEnter(DataObject, KeyState(e->modifiers(), e->buttons()), Point(e), &effect) != S_OK)
          effect = DROPEFFECT_NONE;
        Apply(e, effect, true);
        return true;
      }
      case QEvent::DragMove:
      {
        QDragMoveEvent *e = static_cast<QDragMoveEvent *>(event);
        if (!DataObject)
          return false;
        g_DragOverWindow = Widget;
        DWORD effect = EffectsOf(e->possibleActions());
        CMyComPtr<IDropTarget> target = Target;
        if (target->DragOver(KeyState(e->modifiers(), e->buttons()), Point(e), &effect) != S_OK)
          effect = DROPEFFECT_NONE;
        Apply(e, effect, false);
        return true;
      }
      case QEvent::DragLeave:
      {
        if (DataObject)
        {
          CMyComPtr<IDropTarget> target = Target;
          target->DragLeave();
        }
        DataObject.Release();
        DataObjectSpec = NULL;
        return true;
      }
      case QEvent::Drop:
      {
        QDropEvent *e = static_cast<QDropEvent *>(event);
        if (!DataObject)
          return false;
        g_DragOverWindow = Widget;
        CMyComPtr<IDataObject> dataObject = DataObject;
        CDropDataObject *spec = DataObjectSpec;
        DataObject.Release();
        DataObjectSpec = NULL;
        const DWORD keyState = KeyState(e->modifiers(), e->buttons());
        const POINTL pt = Point(e);
        DWORD effect = EffectsOf(e->possibleActions());
        CMyComPtr<IDropTarget> target = Target;
        if (spec->Source && g_Drag)
        {
          // a drag of this process: the source sees the button released,
          // then the target gets the drop, as on Windows
          if (g_Drag->Release(LastEffect, keyState) != DRAGDROP_S_DROP)
          {
            target->DragLeave();
            e->ignore();
            return true;
          }
          target->Drop(dataObject, keyState, pt, &effect);
          Apply(e, effect, false);
          return true;
        }
        /* Another program's drag: the drop is accepted at once, so that the
           source does not wait while the target asks questions (the Add to
           Archive dialog, ...). */
        e->setDropAction(ActionOf(LastEffect) == Qt::IgnoreAction ? Qt::CopyAction : ActionOf(LastEffect));
        e->accept();
        QPointer<QWidget> widget = Widget;
        QTimer::singleShot(0, this, [target, dataObject, spec, keyState, pt, widget]()
        {
          DWORD eff = DROPEFFECT_COPY | DROPEFFECT_MOVE;
          g_DragOverWindow = widget;
          target->Drop(dataObject, keyState, pt, &eff);
          spec->CallArk();
        });
        return true;
      }
      default:
        return false;
    }
  }
};

}

HRESULT RegisterDragDrop(HWND wnd, IDropTarget *target)
{
  return OnGuiThread<HRESULT>([=]() -> HRESULT
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget || !target)
      return E_INVALIDARG;
    if (w->Widget->findChild<QObject *>(QStringLiteral("z7DropBridge"), Qt::FindDirectChildrenOnly))
      return DRAGDROP_E_ALREADYREGISTERED;
    CDropBridge *bridge = new CDropBridge(w, target);
    bridge->setObjectName(QStringLiteral("z7DropBridge"));
    return S_OK;
  });
}

HRESULT RevokeDragDrop(HWND wnd)
{
  return OnGuiThread<HRESULT>([=]() -> HRESULT
  {
    HWND w = FromHandle(wnd);
    if (!w || !w->Widget)
      return E_INVALIDARG;
    QObject *bridge = w->Widget->findChild<QObject *>(QStringLiteral("z7DropBridge"), Qt::FindDirectChildrenOnly);
    if (!bridge)
      return DRAGDROP_E_NOTREGISTERED;
    w->Widget->removeEventFilter(bridge);
    w->Widget->setAcceptDrops(false);
    delete bridge;
    return S_OK;
  });
}


// ---------- The clipboard ----------

/* OleGetClipboard(): the files on the clipboard of the desktop, as file
   managers put them there: CF_HDROP, and "Preferred DropEffect" with
   DROPEFFECT_MOVE for files that were cut. */
HRESULT OleGetClipboard(IDataObject **dataObject)
{
  if (!dataObject)
    return E_INVALIDARG;
  *dataObject = NULL;
  return OnGuiThread<HRESULT>([=]() -> HRESULT
  {
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime)
      return E_FAIL;
    CDropDataObject *spec = new CDropDataObject;
    CMyComPtr<IDataObject> obj = spec;
    for (const QUrl &u : mime->urls())
      if (u.isLocalFile())
        spec->Files.append(u.toLocalFile());
    const bool cut = mime->data(QStringLiteral("application/x-kde-cutselection")) == "1"
        || mime->data(QStringLiteral("x-special/gnome-copied-files")).startsWith("cut\n");
    spec->PreferredEffect = cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    *dataObject = obj.Detach();
    return S_OK;
  });
}
