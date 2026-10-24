/* NemoExtension.c -- the 7-Zip context menu in Nemo: a menu provider with
   the menu of the Windows shell extension from 7z-context-menu
   (MenuTool.h). */

#include <libnemo-extension/nemo-file-info.h>
#include <libnemo-extension/nemo-menu-provider.h>
#include <libnemo-extension/nemo-name-and-desc-provider.h>

#include "MenuTool.h"

typedef GObject Z7NemoMenu;
typedef GObjectClass Z7NemoMenuClass;

static void menu_provider_init (NemoMenuProviderIface *iface);
static void name_and_desc_init (NemoNameAndDescProviderIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (Z7NemoMenu, z7_nemo_menu, G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC (NEMO_TYPE_MENU_PROVIDER, menu_provider_init)
    G_IMPLEMENT_INTERFACE_DYNAMIC (NEMO_TYPE_NAME_AND_DESC_PROVIDER, name_and_desc_init))

static void
z7_nemo_menu_init (Z7NemoMenu *self)
{
  (void) self;
}

static void
z7_nemo_menu_class_init (Z7NemoMenuClass *klass)
{
  (void) klass;
}

static void
z7_nemo_menu_class_finalize (Z7NemoMenuClass *klass)
{
  (void) klass;
}

static void
activate (NemoMenuItem *item, gpointer data)
{
  (void) data;
  z7_menu_run (g_object_get_data (G_OBJECT (item), "z7-argv"));
}

static GList *
make_items (GPtrArray *items, const char *prefix)
{
  GList *list = NULL;
  for (guint i = 0; i < items->len; i++)
  {
    Z7MenuItem *item = g_ptr_array_index (items, i);
    char *name = g_strdup_printf ("%s-%u", prefix, i);
    NemoMenuItem *mi;
    if (item->separator)
      mi = nemo_menu_item_new_separator (name);
    else
    {
      char *label = z7_menu_text (item->text, TRUE);
      /* Nemo needs a tip; the menu of the Windows shell has none */
      mi = nemo_menu_item_new (name, label, "", item->icon ? z7_menu_icon_name () : NULL);
      g_free (label);
      if (item->children)
      {
        NemoMenu *sub = nemo_menu_new ();
        GList *children = make_items (item->children, name);
        for (GList *l = children; l; l = l->next)
          nemo_menu_append_item (sub, l->data);
        g_list_free_full (children, g_object_unref);
        nemo_menu_item_set_submenu (mi, sub);
        g_object_unref (sub);
      }
      else
      {
        g_object_set_data_full (G_OBJECT (mi), "z7-argv", g_strdupv (item->argv),
            (GDestroyNotify) g_strfreev);
        g_signal_connect (mi, "activate", G_CALLBACK (activate), NULL);
      }
    }
    g_free (name);
    list = g_list_append (list, mi);
  }
  return list;
}

static GList *
get_file_items (NemoMenuProvider *provider, GtkWidget *window, GList *files)
{
  (void) provider;
  (void) window;
  GList *locations = NULL;
  for (GList *l = files; l; l = l->next)
    locations = g_list_append (locations, nemo_file_info_get_location (l->data));
  GPtrArray *menu = z7_menu_query (locations);
  g_list_free_full (locations, g_object_unref);
  if (!menu)
    return NULL;
  GList *items = make_items (menu, "SevenZip");
  g_ptr_array_unref (menu);
  return items;
}

static void
menu_provider_init (NemoMenuProviderIface *iface)
{
  iface->get_file_items = get_file_items;
}

/* the entry of the plugins list of Nemo */
static GList *
get_name_and_desc (NemoNameAndDescProvider *provider)
{
  (void) provider;
  return g_list_append (NULL, g_strdup ("7-Zip:::Open, extract, test and compress archives with 7-Zip"));
}

static void
name_and_desc_init (NemoNameAndDescProviderIface *iface)
{
  iface->get_name_and_desc = get_name_and_desc;
}

static GType types[1];

G_MODULE_EXPORT void
nemo_module_initialize (GTypeModule *module)
{
  z7_nemo_menu_register_type (module);
  types[0] = z7_nemo_menu_get_type ();
}

G_MODULE_EXPORT void
nemo_module_shutdown (void)
{
}

G_MODULE_EXPORT void
nemo_module_list_types (const GType **list, int *num)
{
  *list = types;
  *num = G_N_ELEMENTS (types);
}
