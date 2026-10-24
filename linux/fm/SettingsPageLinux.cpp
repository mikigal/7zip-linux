// SettingsPageLinux.cpp -- the Linux items of the Settings page of the
// options (FileManager/SettingsPage2.rc): whether archive items are dragged
// to programs other than KDE's (UpstreamPosix.h). The template has the
// English texts; the language files have no texts for them, so the Polish
// ones are here.

#include "../../CPP/7zip/UI/FileManager/LangUtils.h"

static const wchar_t * const kPolishDragFiles =
    L"Przeciągaj elementy archiwum do innych programów (przeglądarki, programy GTK)";

static const wchar_t * const kPolishDragFilesInfo =
    L"Przeglądarki i programy GTK odczytują pliki przeciągania, gdy wskaźnik przesuwa się "
    L"nad ich oknem, jeszcze przed upuszczeniem. Gdy ta opcja jest włączona, przeciągane do "
    L"nich elementy archiwum są wtedy wypakowywane do folderu tymczasowego.\n"
    L"+ Elementy archiwum można upuszczać w przeglądarkach, Thunarze, Nemo, Nautilusie i innych "
    L"programach.\n"
    L"- Wypakowywanie zaczyna się nawet bez upuszczenia: duże elementy zajmują czas i miejsce "
    L"w folderze tymczasowym (często w pamięci RAM). Po upuszczeniu pliki zostają tam do "
    L"zamknięcia 7-Zip, bo program kopiuje je później.\n"
    L"Dolphin i inne programy KDE zawsze dostają elementy bezpośrednio, bez tej opcji.";

void InitLinuxSettings(HWND page, int dragFilesId, int dragFilesInfoId)
{
  // the English name of the language of the language file in use
  if (LangString(1) == L"Polish")
  {
    SetDlgItemTextW(page, dragFilesId, kPolishDragFiles);
    SetDlgItemTextW(page, dragFilesInfoId, kPolishDragFilesInfo);
  }
  CheckDlgButton(page, dragFilesId, ReadDragFilesToOtherPrograms() ? BST_CHECKED : BST_UNCHECKED);
}

void ApplyLinuxSettings(HWND page, int dragFilesId)
{
  SaveDragFilesToOtherPrograms(IsDlgButtonChecked(page, dragFilesId) == BST_CHECKED);
}
