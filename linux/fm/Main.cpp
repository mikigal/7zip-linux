// Main.cpp -- the entry point of 7zFM on Linux. It prepares what WinMain()
// of FileManager/FM.cpp expects from Windows (the command line, the program
// directory, a running application with the 7-Zip look) and runs it.

#include <QApplication>

#include <windows.h>

#include "Platform.h"
#include "AppId.h"
#include "Theme.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow);

int main(int argc, char *argv[])
{
  NPlatform::SetCommandLine(argc, argv);
  NPlatform::InitModuleDirPrefix(argc > 0 ? argv[0] : NULL);

  // Qt sees no 7-Zip arguments: it would take its own options (-style,
  // -platform, ...) out of them. Qt settings come from the environment.
  int qtArgc = 1;
  char *qtArgv[] = { argc > 0 ? argv[0] : (char *)"7zFM", NULL };
  QApplication app(qtArgc, qtArgv);
  // the Wayland app ID: the name of the .desktop file (linux/packaging)
  QGuiApplication::setDesktopFileName(QStringLiteral(Z7_APP_ID));
  NTheme::Apply(app);

  char emptyCmdLine[] = "";
  return WinMain(NULL, NULL, emptyCmdLine, SW_SHOWNORMAL);
}
