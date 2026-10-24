/* ThunarExtension.c -- the 7-Zip context menu in Thunar: a menu provider
   with the menu of the Windows shell extension from 7z-context-menu
   (MenuTool.h). */

#include <thunarx/thunarx.h>

#include "MenuTool.h"

typedef GObject Z7ThunarMenu;
typedef GObjectClass Z7ThunarMenuClass;

static void menu_provider_init (ThunarxMenuProviderIface *iface);

THUNARX_DEFINE_TYPE_WITH_CODE (Z7ThunarMenu, z7_thunar_menu, G_TYPE_OBJECT,
    THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER, menu_provider_init))

static void
z7_thunar_menu_init (Z7ThunarMenu *self)
{
  (void) self;
}

static void
z7_thunar_menu_class_init (Z7ThunarMenuClass *klass)
{
  (void) klass;
}

static void
activate (ThunarxMenuItem *item, gpointer data)
{
  (void) data;
  z7_menu_run (g_object_get_data (G_OBJECT (item), "z7-argv"));
}

/* Thunar has no separators in the menus of providers: the items of a
   submenu follow each other. */
static GList *
make_items (GPtrArray *items, const char *prefix)
{
  GList *list = NULL;
  for (guint i = 0; i < items->len; i++)
  {
    Z7MenuItem *item = g_ptr_array_index (items, i);
    if (item->separator)
      continue;
    char *name = g_strdup_printf ("%s-%u", prefix, i);
    char *label = z7_menu_text (item->text, TRUE);
    ThunarxMenuItem *mi = thunarx_menu_item_new (name, label, NULL,
        item->icon ? z7_menu_icon_name () : NULL);
    g_free (label);
    if (item->children)
    {
      ThunarxMenu *sub = thunarx_menu_new ();
      GList *children = make_items (item->children, name);
      for (GList *l = children; l; l = l->next)
        thunarx_menu_append_item (sub, l->data);
      g_list_free_full (children, g_object_unref);
      thunarx_menu_item_set_menu (mi, sub);
      g_object_unref (sub);
    }
    else
    {
      g_object_set_data_full (G_OBJECT (mi), "z7-argv", g_strdupv (item->argv),
          (GDestroyNotify) g_strfreev);
      g_signal_connect (mi, "activate", G_CALLBACK (activate), NULL);
    }
    g_free (name);
    list = g_list_append (list, mi);
  }
  return list;
}

static GList *
get_file_menu_items (ThunarxMenuProvider *provider, GtkWidget *window, GList *files)
{
  (void) provider;
  (void) window;
  GList *locations = NULL;
  for (GList *l = files; l; l = l->next)
    locations = g_list_append (locations, thunarx_file_info_get_location (l->data));
  GPtrArray *menu = z7_menu_query (locations);
  g_list_free_full (locations, g_object_unref);
  if (!menu)
    return NULL;
  GList *items = make_items (menu, "SevenZip");
  g_ptr_array_unref (menu);
  return items;
}

static void
menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_menu_items = get_file_menu_items;
}

static GType types[1];

G_MODULE_EXPORT void
thunar_extension_initialize (ThunarxProviderPlugin *plugin)
{
  /* GLib cannot unload types */
  thunarx_provider_plugin_set_resident (plugin, TRUE);
  z7_thunar_menu_register_type (plugin);
  types[0] = z7_thunar_menu_get_type ();
}

G_MODULE_EXPORT void
thunar_extension_shutdown (void)
{
}

G_MODULE_EXPORT void
thunar_extension_list_types (const GType **list, gint *num)
{
  *list = types;
  *num = G_N_ELEMENTS (types);
}
