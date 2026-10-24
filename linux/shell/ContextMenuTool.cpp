// ContextMenuTool.cpp -- 7z-context-menu: prints the 7-Zip context menu for
// files (ShellMenu.h has the format) for the extensions of GTK file
// managers (linux/gtk), which cannot load Qt into their process.
//
// Usage: 7z-context-menu [--] FILE...

#include <QApplication>
#include <QFile>

#include <string.h>

#include "ShellMenu.h"

// no message boxes: nothing is shown
bool g_DisableUserQuestions = true;

int main(int argc, char *argv[])
{
  // the folder of 7zG and 7zFM, which the items start
  NShellMenu::SetProgramDir();
  qputenv("QT_QPA_PLATFORM", "offscreen");
  int qtArgc = 1;
  char *qtArgv[] = { argc > 0 ? argv[0] : (char *)"7z-context-menu", NULL };
  QApplication app(qtArgc, qtArgv);
  QStringList paths;
  for (int i = 1; i < argc; i++)
    if (i != 1 || strcmp(argv[i], "--") != 0)
      paths.append(QFile::decodeName(argv[i]));
  return NShellMenu::PrintMenu(paths);
}
