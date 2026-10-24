/* MenuTool.c */

#include <string.h>

#include "AppId.h"
#include "MenuTool.h"

static void
item_free (gpointer data)
{
  Z7MenuItem *item = data;
  g_free (item->text);
  g_strfreev (item->argv);
  if (item->children)
    g_ptr_array_unref (item->children);
  g_free (item);
}

static GPtrArray *
new_list (void)
{
  return g_ptr_array_new_with_free_func (item_free);
}

/* The fields of the records: NUL-terminated strings. */
typedef struct
{
  const char *pos;
  const char *end;
} Fields;

static const char *
next_field (Fields *f)
{
  if (f->pos >= f->end)
    return NULL;
  const char *s = f->pos;
  const char *nul = memchr (s, 0, (size_t) (f->end - s));
  if (!nul)
    return NULL;
  f->pos = nul + 1;
  return s;
}

static gboolean
next_number (Fields *f, guint *value)
{
  const char *s = next_field (f);
  guint64 v;
  if (!s || !g_ascii_string_to_unsigned (s, 10, 0, G_MAXINT, &v, NULL))
    return FALSE;
  *value = (guint) v;
  return TRUE;
}

/* The records of 7z-context-menu as a tree, or NULL if they are broken. */
static GPtrArray *
parse_menu (const char *data, gsize size)
{
  Fields f = { data, data + size };
  const char *header = next_field (&f);
  if (!header || strcmp (header, "7-Zip menu 1") != 0)
    return NULL;
  GPtrArray *root = new_list ();
  /* the open submenus: levels[depth] gets the items of that depth */
  GPtrArray *levels = g_ptr_array_new ();
  g_ptr_array_add (levels, root);
  for (;;)
  {
    const char *kind = next_field (&f);
    guint depth;
    if (!kind)
      break;
    if (!next_number (&f, &depth) || depth >= levels->len)
      goto broken;
    g_ptr_array_set_size (levels, depth + 1);
    Z7MenuItem *item = g_new0 (Z7MenuItem, 1);
    g_ptr_array_add (g_ptr_array_index (levels, depth), item);
    if (strcmp (kind, "separator") == 0)
    {
      item->separator = TRUE;
      continue;
    }
    const char *text = next_field (&f);
    const char *icon = next_field (&f);
    if (!text || !icon)
      goto broken;
    item->text = g_strdup (text);
    item->icon = strcmp (icon, "1") == 0;
    if (strcmp (kind, "menu") == 0)
    {
      item->children = new_list ();
      g_ptr_array_add (levels, item->children);
      continue;
    }
    guint argc;
    if (strcmp (kind, "item") != 0 || !next_number (&f, &argc))
      goto broken;
    item->argv = g_new0 (char *, argc + 1);
    for (guint i = 0; i < argc; i++)
    {
      const char *arg = next_field (&f);
      if (!arg)
        goto broken;
      item->argv[i] = g_strdup (arg);
    }
  }
  g_ptr_array_unref (levels);
  return root;

broken:
  g_ptr_array_unref (levels);
  g_ptr_array_unref (root);
  return NULL;
}

static char *
tool_path (void)
{
  const char *path = g_getenv ("Z7_CONTEXT_MENU_TOOL");
  return g_strdup (path && *path ? path : Z7_CONTEXT_MENU_TOOL);
}

GPtrArray *
z7_menu_query (GList *files)
{
  if (!files)
    return NULL;
  GPtrArray *argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, tool_path ());
  g_ptr_array_add (argv, g_strdup ("--"));
  for (GList *l = files; l; l = l->next)
  {
    /* 7-Zip works on paths of the file system */
    char *path = g_file_get_path (G_FILE (l->data));
    if (!path)
    {
      g_ptr_array_unref (argv);
      return NULL;
    }
    g_ptr_array_add (argv, path);
  }
  g_ptr_array_add (argv, NULL);

  GPtrArray *menu = NULL;
  GError *error = NULL;
  GSubprocess *process = g_subprocess_newv ((const char * const *) argv->pdata,
      G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error);
  GBytes *out = NULL;
  if (process && g_subprocess_communicate (process, NULL, NULL, &out, NULL, &error)
      && g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) == 0)
  {
    gsize size;
    const char *data = g_bytes_get_data (out, &size);
    menu = parse_menu (data, size);
  }
  if (error)
  {
    g_warning ("7-Zip: %s", error->message);
    g_error_free (error);
  }
  if (out)
    g_bytes_unref (out);
  if (process)
    g_object_unref (process);
  g_ptr_array_unref (argv);
  if (menu && menu->len == 0)
  {
    g_ptr_array_unref (menu);
    menu = NULL;
  }
  return menu;
}

void
z7_menu_run (char **argv)
{
  GError *error = NULL;
  if (!argv || !argv[0])
    return;
  /* not a child of the file manager: GLib reaps it */
  if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, &error))
  {
    g_warning ("7-Zip: %s", error->message);
    g_error_free (error);
  }
}

char *
z7_menu_text (const char *text, gboolean mnemonic)
{
  GString *s = g_string_new (NULL);
  for (const char *p = text; *p; p++)
  {
    if (*p == '&')
    {
      /* "&&" is "&"; "&x" marks x */
      if (p[1] == '&')
        g_string_append_c (s, *++p);
      else if (mnemonic && p[1])
        g_string_append_c (s, '_');
      continue;
    }
    if (*p == '_' && mnemonic)
      g_string_append_c (s, '_');
    g_string_append_c (s, *p);
  }
  return g_string_free (s, FALSE);
}

const char *
z7_menu_icon_name (void)
{
  return Z7_APP_ID;
}
