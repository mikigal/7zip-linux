/* gtk_menu_probe.c
   z7-gtk-menu-probe: prints the 7-Zip menu that the extensions of GTK file
   managers get from 7z-context-menu (linux/gtk/MenuTool.h), one line per
   item as z7-servicemenu-probe prints it: "submenu > text<TAB><TAB>icon",
   "text >" for a submenu, "-" for a separator; then runs an item.

   Usage: z7-gtk-menu-probe [--trigger "SUBMENU > TEXT"] FILE... */

#include <stdio.h>
#include <string.h>

#include "MenuTool.h"

static char **found;

static void
print_items (GPtrArray *items, const char *prefix, const char *trigger)
{
  for (guint i = 0; i < items->len; i++)
  {
    Z7MenuItem *item = g_ptr_array_index (items, i);
    if (item->separator)
    {
      printf ("%s-\n", prefix);
      continue;
    }
    char *text = z7_menu_text (item->text, FALSE);
    char *path = g_strconcat (prefix, text, NULL);
    if (item->children)
    {
      printf ("%s >\t\t%s\n", path, item->icon ? "icon" : "");
      char *sub = g_strconcat (path, " > ", NULL);
      print_items (item->children, sub, trigger);
      g_free (sub);
    }
    else
    {
      printf ("%s\t\t%s\n", path, item->icon ? "icon" : "");
      if (trigger && strcmp (path, trigger) == 0)
        found = item->argv;
    }
    g_free (path);
    g_free (text);
  }
}

int
main (int argc, char **argv)
{
  const char *trigger = NULL;
  GList *files = NULL;
  for (int i = 1; i < argc; i++)
  {
    if (strcmp (argv[i], "--trigger") == 0 && i + 1 < argc)
      trigger = argv[++i];
    else
      files = g_list_append (files, g_file_new_for_commandline_arg (argv[i]));
  }
  GPtrArray *menu = z7_menu_query (files);
  if (menu)
    print_items (menu, "", trigger);
  fflush (stdout);
  int code = 0;
  if (trigger)
  {
    if (found)
      z7_menu_run (found);
    else
    {
      fprintf (stderr, "no item %s\n", trigger);
      code = 1;
    }
  }
  if (menu)
    g_ptr_array_unref (menu);
  g_list_free_full (files, g_object_unref);
  return code;
}
