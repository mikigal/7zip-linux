/* winref.c -- renders the dialog templates of a Windows 7-Zip program
   (7zFM.exe, 7zG.exe) with the Windows dialog manager of Wine, for reference
   pictures next to the gallery of the Linux port (see wine_reference.py).

   Usage: winref MODULE OUTDIR [--sheet ID,ID,...]

   MODULE is loaded as a data file; every RT_DIALOG template is created with
   CreateDialogIndirectParamW() and its client area is written to
   OUTDIR\<id>.bmp. Templates of property pages (WS_CHILD) are created in a
   host window without WS_CAPTION and WS_DISABLED, as a property sheet does.
   With --sheet, the pages with those IDs are also shown in a modeless
   property sheet, written to OUTDIR\sheet.bmp. Like the gallery, the
   dialog procedure does nothing, so dialogs show their templates before
   any dialog code fills them.

   Built as a Winelib program with winegcc; it needs a display (the Wayland
   or X11 driver of Wine). */

#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HMODULE g_module;

static INT_PTR CALLBACK NullProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  (void)wnd; (void)message; (void)wParam; (void)lParam;
  /* FALSE for WM_INITDIALOG too: no focus, as in the gallery */
  return FALSE;
}

static void Pump(DWORD ms)
{
  const DWORD start = GetTickCount();
  MSG msg;
  while (GetTickCount() - start < ms)
  {
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

/* Writes the client area of wnd, with its child windows, as a 24-bit BMP. */
static BOOL SaveClient(HWND wnd, const WCHAR *path)
{
  RECT rc;
  BITMAPINFO bi;
  void *bits = NULL;
  HDC wdc, mdc;
  HBITMAP bmp, old;
  BOOL ok;
  int w, h, y;
  HANDLE f;
  DWORD written;
  BITMAPFILEHEADER fh;
  BITMAPINFOHEADER ih;
  DWORD stride;
  BYTE *row;

  GetClientRect(wnd, &rc);
  w = rc.right - rc.left;
  h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0)
    return FALSE;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  wdc = GetDC(wnd);
  mdc = CreateCompatibleDC(wdc);
  bmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  old = (HBITMAP)SelectObject(mdc, bmp);
  ok = PrintWindow(wnd, mdc, PW_CLIENTONLY);
  GdiFlush();
  SelectObject(mdc, old);
  DeleteDC(mdc);
  ReleaseDC(wnd, wdc);
  if (!ok)
  {
    DeleteObject(bmp);
    return FALSE;
  }

  stride = ((DWORD)w * 3 + 3) & ~3u;
  memset(&fh, 0, sizeof(fh));
  memset(&ih, 0, sizeof(ih));
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(fh) + sizeof(ih);
  fh.bfSize = fh.bfOffBits + stride * (DWORD)h;
  ih.biSize = sizeof(ih);
  ih.biWidth = w;
  ih.biHeight = h;   /* bottom-up rows */
  ih.biPlanes = 1;
  ih.biBitCount = 24;
  ih.biCompression = BI_RGB;
  ih.biSizeImage = stride * (DWORD)h;
  f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
  {
    DeleteObject(bmp);
    return FALSE;
  }
  WriteFile(f, &fh, sizeof(fh), &written, NULL);
  WriteFile(f, &ih, sizeof(ih), &written, NULL);
  row = (BYTE *)calloc(1, stride);
  for (y = h - 1; y >= 0; y--)
  {
    const BYTE *src = (const BYTE *)bits + (size_t)y * (size_t)w * 4;
    int x;
    for (x = 0; x < w; x++)
    {
      row[x * 3] = src[x * 4];
      row[x * 3 + 1] = src[x * 4 + 1];
      row[x * 3 + 2] = src[x * 4 + 2];
    }
    WriteFile(f, row, stride, &written, NULL);
  }
  free(row);
  CloseHandle(f);
  DeleteObject(bmp);
  return TRUE;
}

/* A copy of a template with styles changed; DLGTEMPLATE and DLGTEMPLATEEX
   keep the style at different offsets. */
static void *PatchTemplate(const void *res, DWORD size, DWORD removeStyle, DWORD addStyle, DWORD *style)
{
  BYTE *t = (BYTE *)malloc(size);
  DWORD *ps;
  memcpy(t, res, size);
  if (((const WORD *)t)[0] == 1 && ((const WORD *)t)[1] == 0xFFFF)
    ps = (DWORD *)(t + 12);   /* dlgVer, signature, helpID, exStyle, style */
  else
    ps = (DWORD *)t;
  *style = *ps;
  *ps = (*ps & ~removeStyle) | addStyle;
  return t;
}

static LRESULT CALLBACK HostProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_ERASEBKGND)
  {
    RECT rc;
    GetClientRect(wnd, &rc);
    FillRect((HDC)wParam, &rc, GetSysColorBrush(COLOR_BTNFACE));
    return 1;
  }
  return DefWindowProcW(wnd, message, wParam, lParam);
}

static BOOL CALLBACK CollectDialog(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR param)
{
  WORD *ids = (WORD *)param;
  (void)module; (void)type;
  if (IS_INTRESOURCE(name) && ids[0] < 255)
    ids[++ids[0]] = (WORD)(ULONG_PTR)name;
  return TRUE;
}

static int RenderDialog(WORD id, const WCHAR *outDir, HWND host)
{
  HRSRC r = FindResourceW(g_module, MAKEINTRESOURCEW(id), (LPCWSTR)RT_DIALOG);
  const void *res = r ? LockResource(LoadResource(g_module, r)) : NULL;
  DWORD style, size;
  void *t;
  HWND dlg, target;
  WCHAR path[MAX_PATH];
  BOOL child;

  if (!res)
  {
    fprintf(stderr, "winref: no dialog %u\n", id);
    return 1;
  }
  size = SizeofResource(g_module, r);
  t = PatchTemplate(res, size, 0, 0, &style);
  free(t);
  child = (style & WS_CHILD) != 0;
  if (child)
    t = PatchTemplate(res, size, WS_CAPTION | WS_DISABLED | WS_POPUP | WS_SYSMENU, WS_CHILD | WS_VISIBLE, &style);
  else
    t = PatchTemplate(res, size, WS_VISIBLE, 0, &style);
  dlg = CreateDialogIndirectParamW(g_module, (LPCDLGTEMPLATEW)t, child ? host : NULL, NullProc, 0);
  free(t);
  if (!dlg)
  {
    fprintf(stderr, "winref: cannot create dialog %u (error %u)\n", id, (unsigned)GetLastError());
    return 1;
  }
  target = dlg;
  if (child)
  {
    RECT rc;
    GetWindowRect(dlg, &rc);
    SetWindowPos(host, HWND_TOP, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_SHOWWINDOW);
    SetWindowPos(dlg, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    target = host;
  }
  else
  {
    SetWindowPos(dlg, HWND_TOP, 40, 40, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }
  SetFocus(NULL);
  Pump(400);
  wsprintfW(path, L"%s\\%u.bmp", outDir, id);
  if (!SaveClient(target, path))
  {
    fprintf(stderr, "winref: cannot capture dialog %u\n", id);
    DestroyWindow(dlg);
    return 1;
  }
  printf("%u\n", id);
  fflush(stdout);
  DestroyWindow(dlg);
  if (child)
    ShowWindow(host, SW_HIDE);
  Pump(50);
  return 0;
}

static unsigned long ParseNumber(const WCHAR *p, const WCHAR **end)
{
  unsigned long v = 0;
  while (*p >= L'0' && *p <= L'9')
    v = v * 10 + (unsigned long)(*p++ - L'0');
  *end = p;
  return v;
}

static int RenderSheet(const WCHAR *ids, const WCHAR *outDir)
{
  PROPSHEETPAGEW pages[16];
  PROPSHEETHEADERW h;
  UINT n = 0;
  const WCHAR *p = ids;
  HWND sheet;
  WCHAR path[MAX_PATH];

  while (*p && n < 16)
  {
    const WCHAR *end;
    const unsigned long id = ParseNumber(p, &end);
    memset(&pages[n], 0, sizeof(pages[n]));
    pages[n].dwSize = sizeof(pages[n]);
    pages[n].dwFlags = PSP_HASHELP;
    pages[n].hInstance = g_module;
    pages[n].pszTemplate = MAKEINTRESOURCEW(id);
    pages[n].pfnDlgProc = NullProc;
    n++;
    p = (*end == L',') ? end + 1 : end;
  }
  memset(&h, 0, sizeof(h));
  h.dwSize = sizeof(h);
  h.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS;
  h.hInstance = g_module;
  h.pszCaption = L"Options";
  h.nPages = n;
  h.ppsp = pages;
  sheet = (HWND)PropertySheetW(&h);
  if (!sheet || sheet == (HWND)-1)
  {
    fprintf(stderr, "winref: cannot create the property sheet\n");
    return 1;
  }
  SetFocus(NULL);
  Pump(600);
  wsprintfW(path, L"%s\\sheet.bmp", outDir);
  if (!SaveClient(sheet, path))
  {
    fprintf(stderr, "winref: cannot capture the property sheet\n");
    return 1;
  }
  printf("sheet\n");
  DestroyWindow(sheet);
  return 0;
}

int wmain(int argc, WCHAR **argv)
{
  WORD ids[256];
  WNDCLASSW wc;
  HWND host;
  INITCOMMONCONTROLSEX icc;
  int errors = 0, i;

  if (argc < 3)
  {
    fprintf(stderr, "usage: winref MODULE OUTDIR [--sheet ID,ID,...]\n");
    return 2;
  }
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icc);
  g_module = LoadLibraryExW(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
  if (!g_module)
  {
    fprintf(stderr, "winref: cannot load the module (error %u)\n", (unsigned)GetLastError());
    return 1;
  }

  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = HostProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = L"WinRefHost";
  wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
  RegisterClassW(&wc);
  host = CreateWindowExW(0, L"WinRefHost", L"page", WS_POPUP, 40, 40, 100, 100, NULL, NULL, wc.hInstance, NULL);

  ids[0] = 0;
  EnumResourceNamesW(g_module, (LPCWSTR)RT_DIALOG, CollectDialog, (LONG_PTR)ids);
  for (i = 1; i <= ids[0]; i++)
    errors += RenderDialog(ids[i], argv[2], host);
  if (argc >= 5 && lstrcmpW(argv[3], L"--sheet") == 0)
    errors += RenderSheet(argv[4], argv[2]);
  DestroyWindow(host);
  return errors ? 1 : 0;
}
