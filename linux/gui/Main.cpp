// Main.cpp -- the entry point of 7zG on Linux. It prepares what WinMain()
// of GUI/GUI.cpp expects from Windows (the command line, the program
// directory, a running application with the 7-Zip look) and runs it.

#include <QApplication>

#include <windows.h>

#include "Platform.h"
#include "Theme.h"

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow);

int main(int argc, char *argv[])
{
  NPlatform::SetCommandLine(argc, argv);
  NPlatform::InitModuleDirPrefix(argc > 0 ? argv[0] : NULL);

  // Qt sees no 7-Zip arguments: it would take its own options (-style,
  // -platform, ...) out of them. Qt settings come from the environment.
  int qtArgc = 1;
  char *qtArgv[] = { argc > 0 ? argv[0] : (char *)"7zG", NULL };
  QApplication app(qtArgc, qtArgv);
  NTheme::Apply(app);

  char emptyCmdLine[] = "";
  return WinMain(NULL, NULL, emptyCmdLine, SW_SHOWNORMAL);
}
