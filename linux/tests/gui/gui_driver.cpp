// gui_driver.cpp -- 7zG with a test script: the program is the same
// (GUI/GUI.cpp on top of the shim), but the dialogs, message boxes and file
// dialogs that it opens are answered by a JSON script instead of a user.
//
// Usage: z7-gui-driver --script FILE --log FILE [--shots DIR] [--timeout SEC]
//            -- <7zG arguments>
//
// The script is an array of steps, taken in order as the program asks:
//   {"on": "dialog", "name": "IDD_EXTRACT", "actions": [...]}
//   {"on": "messagebox", "contains": "text", "answer": 6}
//   {"on": "filedialog", "answer": "/path"}        (null answers Cancel)
// Dialog actions, applied in order once the dialog runs its modal loop:
//   {"text": [id, "value"]}       types the value into an edit or combo box
//   {"select": [id, "item"]}      selects a combo box item, as the user does
//   {"check": [id, true]}         clicks a check box or radio button if needed
//   {"click": id}                 clicks a button
//   {"expect": [id, "regex"]}     fails the script unless the text matches
//   {"wait": [id, "regex"]}       waits until the text matches
//   {"wait_file": "path"}         waits until the file exists (a test that
//                                 looks at the screen tells the script to go on)
//   {"screenshot": "name.png"}    saves a picture of the dialog in --shots
// Dialogs named in "passive" ({"passive": ["IDD_PROGRESS"]} as the first
// element; IDD_PROGRESS by default) run on their own unless a step names
// them. Anything else that the script does not expect fails it.
//
// The log has one JSON object per line: each dialog with the state of its
// controls when it is shown and when it is closed, each message box and file
// dialog, the URLs it opens (help), script errors, and the exit code; the
// first line names the Qt
// platform. The exit code is that of 7zG, or 100 when the script failed.

#include <QApplication>

#include <windows.h>

#include "DriverCore.h"
#include "Theme.h"

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow);

int main(int argc, char *argv[])
{
  NDriver::CArgs args;
  if (!NDriver::ParseArgs(argc, argv, args))
  {
    fprintf(stderr, "usage: z7-gui-driver --script FILE --log FILE [--shots DIR] [--timeout SEC] -- ARGS\n");
    return 2;
  }
  NDriver::SetProgramCommandLine(argc, argv, args);

  int qtArgc = 1;
  char *qtArgv[] = { argv[0], NULL };
  QApplication app(qtArgc, qtArgv);
  NTheme::Apply(app);
  if (!NDriver::Start(args, "z7-gui-driver"))
    return 2;

  char emptyCmdLine[] = "";
  return NDriver::Finish(WinMain(NULL, NULL, emptyCmdLine, SW_SHOWNORMAL));
}
