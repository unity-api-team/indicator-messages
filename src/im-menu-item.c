/*
An indicator to show information that is in messaging applications
that the user is using.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "im-menu-item.h"

enum {
	TIME_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _ImMenuItemPrivate ImMenuItemPrivate;

struct _ImMenuItemPrivate
{
	IndicateListener *           listener;
	IndicateListenerServer *      server;
	IndicateListenerIndicator *  indicator;

	glong seconds;
	gboolean show_time;

	GtkHBox * hbox;
	GtkLabel * user;
	GtkLabel * time;
	GtkImage * icon;
};

#define IM_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), IM_MENU_ITEM_TYPE, ImMenuItemPrivate))

/* Prototypes */
static void im_menu_item_class_init (ImMenuItemClass *klass);
static void im_menu_item_init       (ImMenuItem *self);
static void im_menu_item_dispose    (GObject *object);
static void im_menu_item_finalize   (GObject *object);
static void sender_cb               (IndicateListener * listener,
                                     IndicateListenerServer * server,
                                     IndicateListenerIndicator * indicator,
                                     gchar * property,
                                     gchar * propertydata,
                                     gpointer data);
static void time_cb                 (IndicateListener * listener,
                                     IndicateListenerServer * server,
                                     IndicateListenerIndicator * indicator,
                                     gchar * property,
                                     GTimeVal * propertydata,
                                     gpointer data);
static void icon_cb                 (IndicateListener * listener,
                                     IndicateListenerServer * server,
                                     IndicateListenerIndicator * indicator,
                                     gchar * property,
                                     GdkPixbuf * propertydata,
                                     gpointer data);
static void activate_cb             (ImMenuItem * self,
                                     gpointer data);
static void indicator_modified_cb   (IndicateListener * listener,
                                     IndicateListenerServer * server,
                                     IndicateListenerIndicator * indicator,
                                     gchar * type,
                                     gchar * property,
                                     ImMenuItem * self);


static GtkSizeGroup * icon_group = NULL;
static GtkSizeGroup * user_group = NULL;
static GtkSizeGroup * time_group = NULL;


G_DEFINE_TYPE (ImMenuItem, im_menu_item, GTK_TYPE_MENU_ITEM);

static void
im_menu_item_class_init (ImMenuItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ImMenuItemPrivate));

	object_class->dispose = im_menu_item_dispose;
	object_class->finalize = im_menu_item_finalize;

	signals[TIME_CHANGED] =  g_signal_new(IM_MENU_ITEM_SIGNAL_TIME_CHANGED,
	                                      G_TYPE_FROM_CLASS(klass),
	                                      G_SIGNAL_RUN_LAST,
	                                      G_STRUCT_OFFSET (ImMenuItemClass, time_changed),
	                                      NULL, NULL,
	                                      g_cclosure_marshal_VOID__LONG,
	                                      G_TYPE_NONE, 1, G_TYPE_LONG);

	return;
}

static void
im_menu_item_init (ImMenuItem *self)
{
	g_debug("Building new IM Menu Item");
	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	priv->listener = NULL;
	priv->server = NULL;
	priv->indicator = NULL;

	priv->seconds = 0;

	/* build widgets first */
	priv->icon = GTK_IMAGE(gtk_image_new());
	priv->user = GTK_LABEL(gtk_label_new(""));
	priv->time = GTK_LABEL(gtk_label_new(""));

	gtk_misc_set_alignment(GTK_MISC(priv->user), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(priv->time), 0.0, 0.5);

	if (icon_group == NULL) {
		icon_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	}
	if (user_group == NULL) {
		user_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	}
	if (time_group == NULL) {
		time_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	}
	gtk_size_group_add_widget(icon_group, GTK_WIDGET(priv->icon));
	gtk_size_group_add_widget(user_group, GTK_WIDGET(priv->user));
	gtk_size_group_add_widget(time_group, GTK_WIDGET(priv->time));

	priv->hbox = GTK_HBOX(gtk_hbox_new(FALSE, 3));
	gtk_box_pack_start(GTK_BOX(priv->hbox), GTK_WIDGET(priv->icon), FALSE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(priv->hbox), GTK_WIDGET(priv->user), TRUE,  TRUE, 3);
	gtk_box_pack_start(GTK_BOX(priv->hbox), GTK_WIDGET(priv->time), FALSE, TRUE, 3);
	gtk_widget_show(GTK_WIDGET(priv->hbox));

	gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->hbox));

	return;
}

static void
im_menu_item_dispose (GObject *object)
{
	G_OBJECT_CLASS (im_menu_item_parent_class)->dispose (object);
}

static void
im_menu_item_finalize (GObject *object)
{
	G_OBJECT_CLASS (im_menu_item_parent_class)->finalize (object);
}

static void
icon_cb (IndicateListener * listener, IndicateListenerServer * server, IndicateListenerIndicator * indicator, gchar * property, GdkPixbuf * propertydata, gpointer data)
{
	ImMenuItem * self = IM_MENU_ITEM(data);
	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	gint height, width;
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);

	GdkPixbuf * scaled = gdk_pixbuf_scale_simple(propertydata, width, height, GDK_INTERP_BILINEAR);
	g_object_unref(propertydata);

	gtk_image_set_from_pixbuf(priv->icon, scaled);
	g_object_unref(scaled);

	gtk_widget_show(GTK_WIDGET(priv->icon));

	return;
}

static void
time_cb (IndicateListener * listener, IndicateListenerServer * server, IndicateListenerIndicator * indicator, gchar * property, GTimeVal * propertydata, gpointer data)
{
	g_debug("Got Time info");
	ImMenuItem * self = IM_MENU_ITEM(data);
	if (self == NULL) {
		g_error("Menu Item callback called without a menu item");
		return;
	}

	if (property == NULL || strcmp(property, "time")) {
		g_warning("Time callback called without being sent the time.");
		return;
	}

	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	priv->seconds = propertydata->tv_sec;

	if (priv->show_time) {
		time_t timet;
		struct tm * structtm;

		timet = propertydata->tv_sec;
		structtm = localtime(&timet);

		/* I can't imagine needing more than 80 characters */
		gchar timestring[80];
		strftime(timestring, 80, _("%I:%M"), structtm);

		gtk_label_set_label(priv->time, timestring);
		gtk_widget_show(GTK_WIDGET(priv->time));
	}

	g_signal_emit(G_OBJECT(self), signals[TIME_CHANGED], 0, priv->seconds, TRUE);

	return;
}

static void
sender_cb (IndicateListener * listener, IndicateListenerServer * server, IndicateListenerIndicator * indicator, gchar * property, gchar * propertydata, gpointer data)
{
	g_debug("Got Sender Information");
	ImMenuItem * self = IM_MENU_ITEM(data);
	if (self == NULL) {
		g_error("Menu Item callback called without a menu item");
		return;
	}

	if (property == NULL || strcmp(property, "sender")) {
		g_warning("Sender callback called without being sent the sender.  We got '%s' with value '%s'.", property, propertydata);
		return;
	}

	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	gtk_label_set_label(priv->user, propertydata);
	gtk_widget_show(GTK_WIDGET(priv->user));

	/* Once we have the user we'll show the menu item */
	gtk_widget_show(GTK_WIDGET(self));

	return;
}

static void
activate_cb (ImMenuItem * self, gpointer data)
{
	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	indicate_listener_display(priv->listener, priv->server, priv->indicator);
}

void
indicator_modified_cb (IndicateListener * listener, IndicateListenerServer * server, IndicateListenerIndicator * indicator, gchar * type, gchar * property, ImMenuItem * self)
{
	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	/* Not meant for us */
	if (INDICATE_LISTENER_INDICATOR_ID(indicator) != INDICATE_LISTENER_INDICATOR_ID(priv->indicator)) return;
	if (strcmp(INDICATE_LISTENER_SERVER_DBUS_NAME(server), INDICATE_LISTENER_SERVER_DBUS_NAME(priv->server))) return;

	if (!strcmp(property, "sender")) {
		indicate_listener_get_property(listener, server, indicator, "sender", sender_cb, self);	
	} else if (!strcmp(property, "time")) {
		indicate_listener_get_property_time(listener, server, indicator, "time",   time_cb, self);	
	} else if (!strcmp(property, "icon")) {
		indicate_listener_get_property_icon(listener, server, indicator, "icon",   icon_cb, self);	
	}
	
	return;
}

ImMenuItem *
im_menu_item_new (IndicateListener * listener, IndicateListenerServer * server, IndicateListenerIndicator * indicator, gboolean show_time)
{
	g_debug("Building a new IM Menu Item");
	ImMenuItem * self = g_object_new(IM_MENU_ITEM_TYPE, NULL);

	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(self);

	priv->listener = listener;
	priv->server = server;
	priv->indicator = indicator;
	priv->show_time = show_time;

	indicate_listener_get_property(listener, server, indicator, "sender", sender_cb, self);	
	indicate_listener_get_property_time(listener, server, indicator, "time",   time_cb, self);	
	indicate_listener_get_property_icon(listener, server, indicator, "icon",   icon_cb, self);	

	g_signal_connect(G_OBJECT(self), "activate", G_CALLBACK(activate_cb), NULL);
	g_signal_connect(G_OBJECT(listener), INDICATE_LISTENER_SIGNAL_INDICATOR_MODIFIED, G_CALLBACK(indicator_modified_cb), self);

	return self;
}

glong
im_menu_item_get_seconds (ImMenuItem * menuitem)
{
	ImMenuItemPrivate * priv = IM_MENU_ITEM_GET_PRIVATE(menuitem);
	return priv->seconds;
}
