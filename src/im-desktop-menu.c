/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include "im-desktop-menu.h"
#include "indicator-desktop-shortcuts.h"
#include <glib/gi18n.h>

typedef ImMenuClass ImDesktopMenuClass;

struct _ImDesktopMenu
{
  ImMenu parent;

  GHashTable *source_sections;
};

G_DEFINE_TYPE (ImDesktopMenu, im_desktop_menu, IM_TYPE_MENU);

static void
im_desktop_menu_app_added (ImApplicationList *applist,
                           const gchar       *app_id,
                           GDesktopAppInfo   *app_info,
                           gpointer           user_data)
{
  ImDesktopMenu *menu = user_data;
  GMenu *section;
  GMenu *app_section;
  GMenu *source_section;
  gchar *namespace;

  app_section = g_menu_new ();

  /* application launcher */
  {
    GMenuItem *item;
    GVariant *icon;

    item = g_menu_item_new (g_app_info_get_name (G_APP_INFO (app_info)), "launch");
    g_menu_item_set_attribute (item, "x-canonical-type", "s", "com.canonical.application");

    icon = g_icon_serialize (g_app_info_get_icon (G_APP_INFO (app_info)));
    if (icon)
      {
        g_menu_item_set_attribute_value (item, "icon", icon);
        g_variant_unref (icon);
      }

    g_menu_append_item (app_section, item);

    g_object_unref (item);
  }

  /* application actions */
  {
    const gchar * filename = NULL;
    IndicatorDesktopShortcuts * shortcuts = NULL;
    const gchar ** nicks = {NULL};

    filename = g_desktop_app_info_get_filename(app_info);
    if (filename != NULL)
      shortcuts = indicator_desktop_shortcuts_new(filename, "Messaging Menu");

    if (shortcuts != NULL)
      for (nicks = indicator_desktop_shortcuts_get_nicks(shortcuts); *nicks; nicks++)
        {
          gchar *label;

          label = indicator_desktop_shortcuts_nick_get_name (shortcuts, *nicks);
          g_menu_append (app_section, label, *nicks);

          g_free (label);
        }

    g_clear_object(&shortcuts);
  }

  source_section = g_menu_new ();

  section = g_menu_new ();
  g_menu_append_section (section, NULL, G_MENU_MODEL (app_section));
  g_menu_append_section (section, NULL, G_MENU_MODEL (source_section));

  namespace = g_strconcat ("indicator.", app_id, NULL);
  im_menu_insert_section (IM_MENU (menu), g_app_info_get_name(G_APP_INFO(app_info)), namespace, G_MENU_MODEL (section));
  g_hash_table_insert (menu->source_sections, g_strdup (app_id), source_section);

  g_free (namespace);
  g_object_unref (section);
  g_object_unref (app_section);
}

static void
im_desktop_menu_source_section_insert_source (GMenu       *source_section,
                                              const gchar *source_id,
                                              const gchar *label,
                                              GVariant    *serialized_icon,
                                              gint         pos)
{
  GMenuItem *item;
  gchar *action;

  action = g_strconcat ("src.", source_id, NULL);
  item = g_menu_item_new (label, NULL);
  g_menu_item_set_action_and_target_value (item, action, NULL);
  g_menu_item_set_attribute (item, "x-canonical-type", "s", "com.canonical.indicator.messages.source");
  if (serialized_icon)
    g_menu_item_set_attribute_value (item, "icon", serialized_icon);

  if (pos >= 0)
    g_menu_insert_item (source_section, pos, item);
  else
    g_menu_append_item (source_section, item);

  g_free (action);
  g_object_unref (item);
}

static gint
im_desktop_menu_source_section_find_source (GMenu       *source_section,
                                            const gchar *source_id)
{
  gint n_items;
  gchar *action;
  gint i;

  n_items = g_menu_model_get_n_items (G_MENU_MODEL (source_section));
  action = g_strconcat ("src.", source_id, NULL);

  for (i = 0; i < n_items; i++)
    {
      gchar *item_action;

      if (g_menu_model_get_item_attribute (G_MENU_MODEL (source_section), i, "action", "s", &item_action))
        {
          gboolean equal;

          equal = g_str_equal (action, item_action);
          g_free (item_action);

          if (equal)
            break;
        }
    }

  g_free (action);

  return i < n_items ? i : -1;
}


static void
im_desktop_menu_source_added (ImApplicationList *applist,
                              const gchar       *app_id,
                              const gchar       *source_id,
                              const gchar       *label,
                              GVariant          *serialized_icon,
                              gpointer           user_data)
{
  ImDesktopMenu *menu = user_data;
  GMenu *source_section;

  source_section = g_hash_table_lookup (menu->source_sections, app_id);
  g_return_if_fail (source_section != NULL);

  im_desktop_menu_source_section_insert_source (source_section, source_id, label, serialized_icon, -1);
}

static void
im_desktop_menu_source_removed (ImApplicationList *applist,
                                const gchar       *app_id,
                                const gchar       *source_id,
                                gpointer           user_data)
{
  ImDesktopMenu *menu = user_data;
  GMenu *source_section;
  gint pos;

  source_section = g_hash_table_lookup (menu->source_sections, app_id);
  g_return_if_fail (source_section != NULL);

  pos = im_desktop_menu_source_section_find_source (source_section, source_id);
  if (pos >= 0)
    g_menu_remove (source_section, pos);
}

static void
im_desktop_menu_source_changed (ImApplicationList *applist,
                                const gchar       *app_id,
                                const gchar       *source_id,
                                const gchar       *label,
                                GVariant          *serialized_icon,
                                gboolean           visible,
                                gpointer           user_data)
{
  ImDesktopMenu *menu = user_data;
  GMenu *section;
  gint pos;

  section = g_hash_table_lookup (menu->source_sections, app_id);
  g_return_if_fail (section != NULL);

  pos = im_desktop_menu_source_section_find_source (section, source_id);

  if (pos >= 0)
    g_menu_remove (section, pos);

  if (visible)
    im_desktop_menu_source_section_insert_source (section, source_id, label, serialized_icon, pos);
}

static void
im_desktop_menu_remove_all (ImApplicationList *applist,
                            gpointer           user_data)
{
  ImDesktopMenu *menu = user_data;
  GHashTableIter it;
  GMenu *section;

  g_hash_table_iter_init (&it, menu->source_sections);
  while (g_hash_table_iter_next (&it, NULL, (gpointer *) &section))
    {
      while (g_menu_model_get_n_items (G_MENU_MODEL (section)) > 0)
        g_menu_remove (section, 0);
    }
}

static void
im_desktop_menu_app_stopped (ImApplicationList *applist,
                             const gchar       *app_id,
                             gpointer           user_data)
{
  ImDesktopMenu *menu = user_data;
  GMenu *section;

  section = g_hash_table_lookup (menu->source_sections, app_id);
  g_return_if_fail (section != NULL);

  while (g_menu_model_get_n_items (G_MENU_MODEL (section)) > 0)
    g_menu_remove (section, 0);
}

static GMenu *
create_status_section (void)
{
	GMenu *menu;
	GMenuItem *item;
	struct status_item {
		gchar *label;
		gchar *action;
		gchar *icon_name;
	} status_items[] = {
		{ _("Available"), "indicator.status::available", "user-available" },
		{ _("Away"),      "indicator.status::away",      "user-away" },
		{ _("Busy"),      "indicator.status::busy",      "user-busy" },
		{ _("Invisible"), "indicator.status::invisible", "user-invisible" },
		{ _("Offline"),   "indicator.status::offline",   "user-offline" }
	};
	int i;

	menu = g_menu_new ();

	item = g_menu_item_new (NULL, NULL);

	for (i = 0; i < G_N_ELEMENTS (status_items); i++) {
		GIcon *icon;

		g_menu_item_set_label (item, status_items[i].label);
		g_menu_item_set_detailed_action (item, status_items[i].action);

		icon = g_themed_icon_new (status_items[i].icon_name);
		g_menu_item_set_icon (item, icon);

		g_menu_append_item (menu, item);

		g_object_unref (icon);
	}

	g_object_unref (item);
	return menu;
}

static void
im_desktop_menu_constructed (GObject *object)
{
  ImDesktopMenu *menu = IM_DESKTOP_MENU (object);
  ImApplicationList *applist;

  {
    GMenu *status_section;

    status_section = create_status_section();
    im_menu_append_section (IM_MENU (menu), G_MENU_MODEL (status_section));

    g_object_unref (status_section);
  }

  {
    GMenu *clear_section;

    clear_section = g_menu_new ();
    g_menu_append (clear_section, _("Clear"), "indicator.remove-all");
    im_menu_append_section (IM_MENU (menu), G_MENU_MODEL (clear_section));

    g_object_unref (clear_section);
  }

  applist = im_menu_get_application_list (IM_MENU (menu));

  {
    GList *apps;
    GList *it;

    apps = im_application_list_get_applications (applist);
    for (it = apps; it != NULL; it = it->next)
      {
        const gchar *id = it->data;
        im_desktop_menu_app_added (applist, id, im_application_list_get_application (applist, id), menu);
      }

    g_list_free (apps);
  }


  g_signal_connect (applist, "app-added", G_CALLBACK (im_desktop_menu_app_added), menu);
  g_signal_connect (applist, "source-added", G_CALLBACK (im_desktop_menu_source_added), menu);
  g_signal_connect (applist, "source-removed", G_CALLBACK (im_desktop_menu_source_removed), menu);
  g_signal_connect (applist, "source-changed", G_CALLBACK (im_desktop_menu_source_changed), menu);
  g_signal_connect (applist, "remove-all", G_CALLBACK (im_desktop_menu_remove_all), menu);
  g_signal_connect (applist, "app-stopped", G_CALLBACK (im_desktop_menu_app_stopped), menu);

  G_OBJECT_CLASS (im_desktop_menu_parent_class)->constructed (object);
}

static void
im_desktop_menu_finalize (GObject *object)
{
  ImDesktopMenu *menu = IM_DESKTOP_MENU (object);

  g_hash_table_unref (menu->source_sections);

  G_OBJECT_CLASS (im_desktop_menu_parent_class)->finalize (object);
}

static void
im_desktop_menu_class_init (ImDesktopMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = im_desktop_menu_constructed;
  object_class->finalize = im_desktop_menu_finalize;
}

static void
im_desktop_menu_init (ImDesktopMenu *menu)
{
  menu->source_sections = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

ImDesktopMenu *
im_desktop_menu_new (ImApplicationList  *applist)
{
  g_return_val_if_fail (IM_IS_APPLICATION_LIST (applist), NULL);

  return g_object_new (IM_TYPE_DESKTOP_MENU,
                       "application-list", applist,
                       NULL);
}