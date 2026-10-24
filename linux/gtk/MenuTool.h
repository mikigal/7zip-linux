/* MenuTool.h -- the 7-Zip context menu for the extensions of GTK file
   managers (Nemo, Thunar, Nautilus), which cannot load Qt into their
   process: 7z-context-menu (linux/shell/ShellMenu.h) builds the menu of
   the Windows shell extension with the upstream code and prints it with
   the command of each item; this reads it into a tree, and starts the
   command of a chosen item. */

#ifndef ZIP7_INC_LINUX_GTK_MENU_TOOL_H
#define ZIP7_INC_LINUX_GTK_MENU_TOOL_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct Z7MenuItem Z7MenuItem;

struct Z7MenuItem
{
  char *text;           /* the Windows menu text ("&" marks a mnemonic) */
  gboolean icon;        /* the 7-Zip logo ("Icons in context menu") */
  gboolean separator;
  char **argv;          /* the program and its arguments; NULL for submenus
                           and separators */
  GPtrArray *children;  /* the Z7MenuItem of a submenu, else NULL */
};

/* The menu for files (GFile), from 7z-context-menu (the installed one, or
   $Z7_CONTEXT_MENU_TOOL); NULL when a file is not on the file system or
   7-Zip offers nothing. Free it with g_ptr_array_unref(). */
GPtrArray *z7_menu_query (GList *files);

/* Starts the command of an item (an argv from Z7MenuItem) on its own. */
void z7_menu_run (char **argv);

/* The text for a GTK label: with "_" for the mnemonic, or without one. */
char *z7_menu_text (const char *text, gboolean mnemonic);

/* The icon name of the 7-Zip logo (the application icon). */
const char *z7_menu_icon_name (void);

G_END_DECLS

#endif
