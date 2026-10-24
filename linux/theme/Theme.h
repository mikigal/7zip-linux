// Theme.h -- the look of the 7-Zip programs on Linux: the Fusion style with a
// dark palette and a small style sheet, so that the original 7-Zip layouts
// are shown in a dark theme.

#ifndef ZIP7_INC_LINUX_THEME_H
#define ZIP7_INC_LINUX_THEME_H

#include <QIcon>
#include <QPalette>
#include <QString>

class QApplication;

namespace NTheme {

QPalette DarkPalette();
QString StyleSheet();

// Sets the style, palette and style sheet of the application.
void Apply(QApplication &app);

/* A toolbar icon: name is the upstream bitmap name ("Add", "Extract",
   "Test", "Copy", "Move", "Delete", "Info"); large selects the 48x36
   version used with "Large buttons", else the 24x24 one. */
QIcon ToolbarIcon(const char *name, bool large);

}

#endif
