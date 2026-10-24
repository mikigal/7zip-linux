/* NautilusExtension.c -- the 7-Zip context menu in Nautilus (GNOME Files,
   libnautilus-extension-4): a menu provider with the menu of the Windows
   shell extension from 7z-context-menu (MenuTool.h). */

#include <nautilus-extension.h>

#include "MenuTool.h"

typedef GObject Z7NautilusMenu;
typedef GObjectClass Z7NautilusMenuClass;

static void menu_provider_init (NautilusMenuProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (Z7NautilusMenu, z7_nautilus_menu, G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_MENU_PROVIDER, menu_provider_init))

static void
z7_nautilus_menu_init (Z7NautilusMenu *self)
{
  (void) self;
}

static void
z7_nautilus_menu_class_init (Z7NautilusMenuClass *klass)
{
  (void) klass;
}

static void
z7_nautilus_menu_class_finalize (Z7NautilusMenuClass *klass)
{
  (void) klass;
}

static void
activate (NautilusMenuItem *item, gpointer data)
{
  (void) data;
  z7_menu_run (g_object_get_data (G_OBJECT (item), "z7-argv"));
}

/* Nautilus menus have no separators or icons for extensions. */
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
    NautilusMenuItem *mi = nautilus_menu_item_new (name, label, NULL, NULL);
    g_free (label);
    if (item->children)
    {
      NautilusMenu *sub = nautilus_menu_new ();
      GList *children = make_items (item->children, name);
      for (GList *l = children; l; l = l->next)
        nautilus_menu_append_item (sub, l->data);
      g_list_free_full (children, g_object_unref);
      nautilus_menu_item_set_submenu (mi, sub);
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
get_file_items (NautilusMenuProvider *provider, GList *files)
{
  (void) provider;
  GList *locations = NULL;
  for (GList *l = files; l; l = l->next)
    locations = g_list_append (locations, nautilus_file_info_get_location (l->data));
  GPtrArray *menu = z7_menu_query (locations);
  g_list_free_full (locations, g_object_unref);
  if (!menu)
    return NULL;
  GList *items = make_items (menu, "SevenZip");
  g_ptr_array_unref (menu);
  return items;
}

static void
menu_provider_init (NautilusMenuProviderInterface *iface)
{
  iface->get_file_items = get_file_items;
}

static GType types[1];

G_MODULE_EXPORT void
nautilus_module_initialize (GTypeModule *module)
{
  z7_nautilus_menu_register_type (module);
  types[0] = z7_nautilus_menu_get_type ();
}

G_MODULE_EXPORT void
nautilus_module_shutdown (void)
{
}

G_MODULE_EXPORT void
nautilus_module_list_types (const GType **list, int *num)
{
  *list = types;
  *num = G_N_ELEMENTS (types);
}
