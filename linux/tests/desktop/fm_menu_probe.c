/* fm_menu_probe.c
   z7-nemo-probe, z7-thunar-probe, z7-nautilus-probe: load the 7-Zip
   extension of a GTK file manager (linux/gtk) as the file manager does,
   ask its menu provider for the items of files (file infos of the probe),
   and print them as z7-gtk-menu-probe does: "submenu > text<TAB><TAB>icon",
   "text >" for a submenu, "-" for a separator; then activate an item.

   Usage: PROBE MODULE [--trigger "SUBMENU > TEXT"] FILE... */

#include <stdio.h>
#include <string.h>

#include <gmodule.h>

#if defined(Z7_PROBE_NEMO)
#include <libnemo-extension/nemo-file-info.h>
#include <libnemo-extension/nemo-menu-provider.h>
#define FM_FILE_INFO_TYPE NEMO_TYPE_FILE_INFO
typedef NemoFileInfoInterface FmFileInfoInterface;
typedef NemoFileInfo FmFileInfo;
typedef NemoMenu FmMenu;
typedef NemoMenuItem FmMenuItem;
#define fm_menu_get_items nemo_menu_get_items
#define fm_menu_item_list_free nemo_menu_item_list_free
#define fm_menu_item_activate nemo_menu_item_activate
#define FM_INIT "nemo_module_initialize"
#define FM_LIST "nemo_module_list_types"
#elif defined(Z7_PROBE_THUNAR)
#include <thunarx/thunarx.h>
#define FM_FILE_INFO_TYPE THUNARX_TYPE_FILE_INFO
typedef ThunarxFileInfoIface FmFileInfoInterface;
typedef ThunarxFileInfo FmFileInfo;
typedef ThunarxMenu FmMenu;
typedef ThunarxMenuItem FmMenuItem;
#define fm_menu_get_items thunarx_menu_get_items
#define fm_menu_item_list_free thunarx_menu_item_list_free
#define fm_menu_item_activate thunarx_menu_item_activate
#define FM_INIT "thunar_extension_initialize"
#define FM_LIST "thunar_extension_list_types"
#elif defined(Z7_PROBE_NAUTILUS)
#include <nautilus-extension.h>
#define FM_FILE_INFO_TYPE NAUTILUS_TYPE_FILE_INFO
typedef NautilusFileInfoInterface FmFileInfoInterface;
typedef NautilusFileInfo FmFileInfo;
typedef NautilusMenu FmMenu;
typedef NautilusMenuItem FmMenuItem;
#define fm_menu_get_items nautilus_menu_get_items
#define fm_menu_item_list_free nautilus_menu_item_list_free
#define fm_menu_item_activate nautilus_menu_item_activate
#define FM_INIT "nautilus_module_initialize"
#define FM_LIST "nautilus_module_list_types"
#else
#error "the file manager of the probe"
#endif

/* ---------- a file info of the probe: a path ---------- */

typedef struct
{
  GObject parent;
  GFile *location;
} ProbeFile;
typedef GObjectClass ProbeFileClass;

static void probe_file_info_init (FmFileInfoInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ProbeFile, probe_file, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (FM_FILE_INFO_TYPE, probe_file_info_init))

static void
probe_file_finalize (GObject *object)
{
  g_clear_object (&((ProbeFile *) object)->location);
  G_OBJECT_CLASS (probe_file_parent_class)->finalize (object);
}

static void
probe_file_class_init (ProbeFileClass *klass)
{
  klass->finalize = probe_file_finalize;
}

static void
probe_file_init (ProbeFile *self)
{
  (void) self;
}

static GFile *
probe_file_get_location (FmFileInfo *info)
{
  return g_object_ref (((ProbeFile *) info)->location);
}

static char *
probe_file_get_uri (FmFileInfo *info)
{
  return g_file_get_uri (((ProbeFile *) info)->location);
}

static void
probe_file_info_init (FmFileInfoInterface *iface)
{
  iface->get_location = probe_file_get_location;
  iface->get_uri = probe_file_get_uri;
}

/* ---------- the module ---------- */

/* The type module of the extension, as the file manager makes it: the
   extension registers its types into it. Thunar's is a provider plugin
   (thunarx's own module type is not in its API). */
typedef GTypeModule ProbeModule;
typedef GTypeModuleClass ProbeModuleClass;

#if defined(Z7_PROBE_THUNAR)
static void probe_plugin_init (ThunarxProviderPluginIface *iface);

G_DEFINE_TYPE_WITH_CODE (ProbeModule, probe_module, G_TYPE_TYPE_MODULE,
    G_IMPLEMENT_INTERFACE (THUNARX_TYPE_PROVIDER_PLUGIN, probe_plugin_init))

static gboolean
probe_plugin_get_resident (const ThunarxProviderPlugin *plugin)
{
  (void) plugin;
  return TRUE;
}

static void
probe_plugin_set_resident (ThunarxProviderPlugin *plugin, gboolean resident)
{
  (void) plugin;
  (void) resident;
}

static GType
probe_plugin_register_type (ThunarxProviderPlugin *plugin, GType parent, const gchar *name,
    const GTypeInfo *info, GTypeFlags flags)
{
  return g_type_module_register_type (G_TYPE_MODULE (plugin), parent, name, info, flags);
}

static void
probe_plugin_add_interface (ThunarxProviderPlugin *plugin, GType type, GType iface_type,
    const GInterfaceInfo *info)
{
  g_type_module_add_interface (G_TYPE_MODULE (plugin), type, iface_type, info);
}

static GType
probe_plugin_register_enum (ThunarxProviderPlugin *plugin, const gchar *name, const GEnumValue *values)
{
  return g_type_module_register_enum (G_TYPE_MODULE (plugin), name, values);
}

static GType
probe_plugin_register_flags (ThunarxProviderPlugin *plugin, const gchar *name, const GFlagsValue *values)
{
  return g_type_module_register_flags (G_TYPE_MODULE (plugin), name, values);
}

static void
probe_plugin_init (ThunarxProviderPluginIface *iface)
{
  iface->get_resident = probe_plugin_get_resident;
  iface->set_resident = probe_plugin_set_resident;
  iface->register_type = probe_plugin_register_type;
  iface->add_interface = probe_plugin_add_interface;
  iface->register_enum = probe_plugin_register_enum;
  iface->register_flags = probe_plugin_register_flags;
}
#else
G_DEFINE_TYPE (ProbeModule, probe_module, G_TYPE_TYPE_MODULE)
#endif

static gboolean
probe_module_load (GTypeModule *module)
{
  (void) module;
  return TRUE;
}

static void
probe_module_unload (GTypeModule *module)
{
  (void) module;
}

#if defined(Z7_PROBE_THUNAR)
/* the "resident" property of the plugin interface: always resident */
static void
probe_module_get_property (GObject *object, guint id, GValue *value, GParamSpec *spec)
{
  (void) object;
  (void) id;
  (void) spec;
  g_value_set_boolean (value, TRUE);
}

static void
probe_module_set_property (GObject *object, guint id, const GValue *value, GParamSpec *spec)
{
  (void) object;
  (void) id;
  (void) value;
  (void) spec;
}
#endif

static void
probe_module_class_init (ProbeModuleClass *klass)
{
  klass->load = probe_module_load;
  klass->unload = probe_module_unload;
#if defined(Z7_PROBE_THUNAR)
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = probe_module_get_property;
  object_class->set_property = probe_module_set_property;
  g_object_class_override_property (object_class, 1, "resident");
#endif
}

static void
probe_module_init (ProbeModule *self)
{
  (void) self;
}

/* The menu providers of the extension. */
static GList *
load_providers (const char *path)
{
  GList *providers = NULL;
  GModule *lib = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  void (*init) (gpointer) = NULL;
  void (*list) (const GType **, int *) = NULL;
  if (!lib || !g_module_symbol (lib, FM_INIT, (gpointer *) &init)
      || !g_module_symbol (lib, FM_LIST, (gpointer *) &list))
  {
    fprintf (stderr, "cannot load %s: %s\n", path, g_module_error ());
    return NULL;
  }
  g_module_make_resident (lib);
  GTypeModule *module = g_object_new (probe_module_get_type (), NULL);
  g_type_module_use (module);
  init (module);
  const GType *types;
  int num;
  list (&types, &num);
  for (int i = 0; i < num; i++)
    providers = g_list_append (providers, g_object_new (types[i], NULL));
  return providers;
}

static GList *
file_items (gpointer provider, GList *files)
{
#if defined(Z7_PROBE_NEMO)
  return nemo_menu_provider_get_file_items (provider, NULL, files);
#elif defined(Z7_PROBE_THUNAR)
  /* the API wants Thunar's window, which the extension does not use */
  return THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_menu_items (provider, NULL, files);
#else
  return nautilus_menu_provider_get_file_items (provider, files);
#endif
}

/* ---------- printing ---------- */

static FmMenuItem *found;

static void
print_items (GList *items, const char *prefix, const char *trigger)
{
  for (GList *l = items; l; l = l->next)
  {
    FmMenuItem *item = l->data;
    char *label = NULL;
    char *icon = NULL;
    FmMenu *menu = NULL;
#if defined(Z7_PROBE_NEMO)
    gboolean separator = FALSE;
    g_object_get (item, "separator", &separator, NULL);
    if (separator)
    {
      printf ("%s-\n", prefix);
      continue;
    }
#endif
    g_object_get (item, "label", &label, "icon", &icon, "menu", &menu, NULL);
    char *path = g_strconcat (prefix, label ? label : "", NULL);
    if (menu)
    {
      printf ("%s >\t\t%s\n", path, icon ? icon : "");
      char *sub = g_strconcat (path, " > ", NULL);
      GList *children = fm_menu_get_items (menu);
      print_items (children, sub, trigger);
      fm_menu_item_list_free (children);
      g_free (sub);
      g_object_unref (menu);
    }
    else
    {
      printf ("%s\t\t%s\n", path, icon ? icon : "");
      if (trigger && strcmp (path, trigger) == 0)
        found = g_object_ref (item);
    }
    g_free (path);
    g_free (label);
    g_free (icon);
  }
}

int
main (int argc, char **argv)
{
  if (argc < 2)
  {
    fprintf (stderr, "usage: %s MODULE [--trigger PATH] FILE...\n", argv[0]);
    return 2;
  }
  const char *trigger = NULL;
  GList *files = NULL;
  for (int i = 2; i < argc; i++)
  {
    if (strcmp (argv[i], "--trigger") == 0 && i + 1 < argc)
      trigger = argv[++i];
    else
    {
      ProbeFile *file = g_object_new (probe_file_get_type (), NULL);
      file->location = g_file_new_for_commandline_arg (argv[i]);
      files = g_list_append (files, file);
    }
  }
  GList *providers = load_providers (argv[1]);
  if (!providers)
  {
    fprintf (stderr, "no menu provider in %s\n", argv[1]);
    return 1;
  }
  for (GList *p = providers; p; p = p->next)
  {
    GList *items = file_items (p->data, files);
    print_items (items, "", trigger);
    g_list_free_full (items, g_object_unref);
  }
  fflush (stdout);
  int code = 0;
  if (trigger)
  {
    if (found)
    {
      fm_menu_item_activate (found);
      g_object_unref (found);
    }
    else
    {
      fprintf (stderr, "no item %s\n", trigger);
      code = 1;
    }
  }
  g_list_free_full (providers, g_object_unref);
  g_list_free_full (files, g_object_unref);
  return code;
}
