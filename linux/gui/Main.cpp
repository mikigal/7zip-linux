// Main.cpp -- the entry point of 7zG on Linux. It prepares what WinMain()
// of GUI/GUI.cpp expects from Windows (the command line, the program
// directory, a running application with the 7-Zip look) and runs it.

#include <QApplication>

#include <windows.h>

#include "Platform.h"
#include <stdio.h>

#include <string>
#include <vector>

#include "AppId.h"
#include "ContextCommand.h"
#include "Theme.h"

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow);

int main(int argc, char *argv[])
{
  // file manager context menus: 7zG --context ACTION FILE...
  std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
  std::vector<std::string> command;
  std::string error;
  if (NGui::TranslateContextCommand(args, command, error))
  {
    if (!error.empty())
    {
      fprintf(stderr, "%s\n", error.c_str());
      return 2;
    }
    std::vector<const char *> list;
    list.push_back(argc > 0 ? argv[0] : "7zG");
    for (const std::string &a : command)
      list.push_back(a.c_str());
    NPlatform::SetCommandLine((int)list.size(), list.data());
  }
  else
    NPlatform::SetCommandLine(argc, argv);
  NPlatform::InitModuleDirPrefix(argc > 0 ? argv[0] : NULL);

  // Qt sees no 7-Zip arguments: it would take its own options (-style,
  // -platform, ...) out of them. Qt settings come from the environment.
  int qtArgc = 1;
  char *qtArgv[] = { argc > 0 ? argv[0] : (char *)"7zG", NULL };
  QApplication app(qtArgc, qtArgv);
  // the Wayland app ID: the name of the .desktop file (linux/packaging)
  QGuiApplication::setDesktopFileName(QStringLiteral(Z7_GUI_APP_ID));
  NTheme::Apply(app);

  char emptyCmdLine[] = "";
  return WinMain(NULL, NULL, emptyCmdLine, SW_SHOWNORMAL);
}
