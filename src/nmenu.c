/*============================================================================
Copyright (c) 2026 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <fcntl.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <menu-cache.h>

#include "lxutils.h"
#include "nmenu.h"
#include "launcher.h"

extern void gtk_launch (const char *app_name);
extern void show_properties_dialog (MenuCacheItem *item);

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define CELL_WIDTH 100

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

conf_table_t conf_table[3] = {
    {CONF_TYPE_INT,  "padding",          N_("Icon horizontal padding"),     NULL},
    {CONF_TYPE_BOOL, "show_tooltips",    N_("Show tooltips for menu items"), NULL},
    {CONF_TYPE_NONE, NULL,               NULL,                              NULL}
};

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void create_window (NmenuPlugin *m);
static void destroy_window (NmenuPlugin *m);
static void window_destroyed (GtkWidget *, gpointer data);
static gboolean filter_apps (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
static void handle_iconview_selected (GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
static gboolean handle_iconview_buttonpress (GtkWidget *, GdkEventButton *event, gpointer user_data);
static gboolean handle_iconview_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data);
static void gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer);
static void gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer user_data);
static gboolean handle_search_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data);
static void handle_search_changed (GtkEditable *, gpointer user_data);
static void append_to_entry (GtkWidget *entry, char val);
static void create_cs_menu (NmenuPlugin *m, char *id);
static void handle_menu_item_add_to_desktop (GtkWidget *mi, gpointer user_data);
static void handle_menu_item_add_to_launcher (GtkWidget *mi, gpointer);
static void handle_menu_item_properties (GtkWidget *mi, gpointer user_data);
static void insert_system_menu (NmenuPlugin *m, GtkMenu *menu, int position);
static void sys_menu_load_submenu (NmenuPlugin* m, MenuCacheDir* dir, GtkWidget* menu, int pos);
static void create_system_menu_item (MenuCacheItem *item, NmenuPlugin *m);
static void menu_button_clicked (GtkWidget *, NmenuPlugin *m);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/


GtkAllocation alloc;
/* Icon window */

static void create_window (NmenuPlugin *m)
{
    GtkCellRenderer *prend, *trend;
    GtkTreeModelSort *slist;
    GtkTreeModelFilter *flist;
    GtkBuilder *builder;
    GtkCellLayout *layout;
    GtkGesture *gesture;
    GdkRectangle monitor_geometry;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/gmenu.ui");
    m->swin = (GtkWidget *) gtk_builder_get_object (builder, "gmenu");
    m->stv = (GtkWidget *) gtk_builder_get_object (builder, "iconview");
    m->srch = (GtkWidget *) gtk_builder_get_object (builder, "searchbar");
    
    insert_system_menu (m, GTK_MENU (m->menu), -1);

    g_signal_connect (m->srch, "changed", G_CALLBACK (handle_search_changed), m);
    g_signal_connect (m->srch, "key-press-event", G_CALLBACK (handle_search_keypress), m);

    /* create the filtered list for the tree view */
    slist = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (m->applist)));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (slist), 1, GTK_SORT_ASCENDING);
    flist = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (slist), NULL));
    gtk_tree_model_filter_set_visible_func (flist, (GtkTreeModelFilterVisibleFunc) filter_apps, m, NULL);

    gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (flist));
    gtk_icon_view_set_tooltip_column (GTK_ICON_VIEW (m->stv), 3);
    g_object_unref (slist);
    g_object_unref (flist);

    /* set up the icon view */
    layout = GTK_CELL_LAYOUT (m->stv);

    prend = gtk_cell_renderer_pixbuf_new ();
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_widget_get_scale_factor (m->img));
    g_object_set_property (G_OBJECT (prend), "scale", &val);
    gtk_cell_renderer_set_fixed_size (prend, CELL_WIDTH, -1);
    gtk_cell_layout_pack_start (layout, prend, FALSE);
    gtk_cell_layout_add_attribute (layout, prend, "pixbuf", 0);

    trend = gtk_cell_renderer_text_new ();
    gtk_cell_renderer_set_alignment (trend, 0.5, 0.0);
    g_object_set (trend, "wrap-width", CELL_WIDTH, "wrap-mode", PANGO_WRAP_WORD, "alignment", PANGO_ALIGN_CENTER, NULL);
    gtk_cell_layout_pack_start (layout, trend, FALSE);
    gtk_cell_layout_add_attribute (layout, trend, "markup", 1);

    g_signal_connect (m->stv, "item-activated", G_CALLBACK (handle_iconview_selected), m);
    g_signal_connect (m->stv, "button-press-event", G_CALLBACK (handle_iconview_buttonpress), m);
    g_signal_connect (m->stv, "key-press-event", G_CALLBACK (handle_iconview_keypress), m);
    g_signal_connect (m->swin, "destroy", G_CALLBACK (window_destroyed), m);

    gesture = gtk_gesture_long_press_new (m->stv);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
    g_signal_connect (gesture, "pressed", G_CALLBACK (gesture_pressed), m);
    g_signal_connect (gesture, "end", G_CALLBACK (gesture_end), m);
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_TARGET);
    pressed = FALSE;

    /* realise */
    gdk_monitor_get_geometry (gdk_display_get_monitor (gdk_display_get_default (), 0), &monitor_geometry);
    gtk_window_set_default_size (GTK_WINDOW (m->swin), monitor_geometry.width, monitor_geometry.height);
    gtk_layer_init_for_window (GTK_WINDOW (m->swin));
    gtk_layer_set_layer (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_monitor (GTK_WINDOW (m->swin), gdk_display_get_monitor (gdk_display_get_default (), 0));
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_keyboard_interactivity (GTK_WINDOW (m->swin), TRUE);

    gtk_widget_show_all (m->swin);
    gtk_window_present (GTK_WINDOW (m->swin));
    gtk_widget_get_allocation (m->stv, &alloc);
    gtk_widget_set_size_request (m->stv, alloc.width, alloc.height);
}

static void destroy_window (NmenuPlugin *m)
{
    if (m->swin) gtk_widget_destroy (m->swin);
    m->swin = NULL;
}

static void window_destroyed (GtkWidget *, gpointer data)
{
    NmenuPlugin *m = (NmenuPlugin *) data;
    g_signal_handlers_disconnect_matched (m->swin, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, m);
    g_signal_handlers_disconnect_matched (m->srch, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, m);
    g_signal_handlers_disconnect_matched (m->stv, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, m);
    m->swin = NULL;
}

static gboolean filter_apps (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    gboolean res = FALSE;
    char *str, *pstr = NULL;
    GtkTreeIter prev = *iter;

    gtk_tree_model_get (model, iter, 1, &str, -1);
    if (gtk_tree_model_iter_previous (model, &prev)) gtk_tree_model_get (model, &prev, 1, &pstr, -1);
    if ((!pstr || strcmp (str, pstr)) && strcasestr (str, gtk_entry_get_text (GTK_ENTRY (m->srch)))) res = TRUE;
    if (pstr) g_free (pstr);
    g_free (str);
    return res;
}

static void handle_iconview_selected (GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    gchar *str;
    GtkTreeIter fitem;
    GtkTreeModel *mod = gtk_icon_view_get_model (iconview);
    gtk_tree_model_get_iter (mod, &fitem, path);
    gtk_tree_model_get (mod, &fitem, 2, &str, -1);

    gtk_launch (str);
    destroy_window (m);
}

static gboolean handle_iconview_buttonpress (GtkWidget *, GdkEventButton *event, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreeIter fitem;
    GtkTreeModel *ivm;
    char *str;

    if (event->button == 3)
    {
        GtkTreePath *path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (m->stv), event->x, event->y);
        if (path)
        {
            ivm = gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv));
            gtk_tree_model_get_iter (ivm, &fitem, path);
            gtk_tree_model_get (ivm, &fitem, 2, &str, -1);
            create_cs_menu (m, str);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean handle_iconview_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;

    if ((event->keyval >= 'a' && event->keyval <= 'z') ||
        (event->keyval >= 'A' && event->keyval <= 'Z'))
    {
        append_to_entry (m->srch, event->keyval);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_BackSpace)
    {
        append_to_entry (m->srch, 0);
        return TRUE;
    }

    switch (event->keyval)
    {
        case GDK_KEY_Escape :   destroy_window (m);
                                return TRUE;

        default :               return FALSE;
    }
}

static void gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer)
{
    pressed = TRUE;
    press_x = x;
    press_y = y;
}

static void gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreeIter fitem;
    GtkTreeModel *ivm;
    char *str;
    if (pressed)
    {
        GtkTreePath *path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (m->stv), press_x, press_y);
        if (path)
        {
            ivm = gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv));
            gtk_tree_model_get_iter (ivm, &fitem, path);
            gtk_tree_model_get (ivm, &fitem, 2, &str, -1);
            create_cs_menu (m, str);
        }

        pressed = FALSE;
    }
}

/* Search bar handling */

static gboolean handle_search_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreeModel *mod;
    GtkTreeIter iter;
    gchar *str;
    GList *list;

    switch (event->keyval)
    {
        case GDK_KEY_KP_Enter :
        case GDK_KEY_Return :   mod = gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv));
                                list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (m->stv));
                                if (list)
                                {
                                    gtk_tree_model_get_iter (mod, &iter, (GtkTreePath *) list->data); 
                                    gtk_tree_model_get (mod, &iter, 2, &str, -1);
                                    gtk_launch (str);
                                    destroy_window (m);
                                }
                                return TRUE;

        case GDK_KEY_Escape :   destroy_window (m);
                                return TRUE;

        case GDK_KEY_Up :
        case GDK_KEY_Down :
        case GDK_KEY_Left :
        case GDK_KEY_Right :    gtk_widget_grab_focus (m->stv);
                                // propagate the key press event to the icon view...
                                return FALSE;

        default :               return FALSE;
    }
}

static void handle_search_changed (GtkEditable *, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreePath *path = gtk_tree_path_new_from_indices (0, -1);
    
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv))));
    gtk_icon_view_select_path (GTK_ICON_VIEW (m->stv), path);
    gtk_tree_path_free (path);
    gtk_widget_set_size_request (m->stv, alloc.width, alloc.height);
}

static void append_to_entry (GtkWidget *entry, char val)
{
    int len = strlen (gtk_entry_get_text (GTK_ENTRY (entry)));
    if (val) gtk_editable_insert_text (GTK_EDITABLE (entry), &val, 1, &len);
    else if (len) gtk_editable_delete_text (GTK_EDITABLE (entry), (len - 1), -1);
    gtk_widget_grab_focus (entry);
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
}

/* Popup menu */

static void create_cs_menu (NmenuPlugin *m, char *id)
{
    GtkWidget *item, *menu;

    menu = gtk_menu_new ();

    item = gtk_menu_item_new_with_label (_("Add to desktop"));
    gtk_widget_set_name (item, id);
    g_signal_connect (item, "activate", G_CALLBACK (handle_menu_item_add_to_desktop), m);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_menu_item_new_with_label (_("Add to Launcher"));
    gtk_widget_set_name (item, id);
    g_signal_connect (item, "activate", G_CALLBACK (handle_menu_item_add_to_launcher), m);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_menu_item_new_with_label (_("Properties"));
    gtk_widget_set_name (item, id);
    g_signal_connect (item, "activate", G_CALLBACK (handle_menu_item_properties), m);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void handle_menu_item_add_to_desktop (GtkWidget *mi, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    MenuCacheItem *item = menu_cache_find_item_by_id (m->menu_cache, gtk_widget_get_name (mi));
    char *path = g_build_filename (g_get_home_dir (), "Desktop", menu_cache_item_get_file_basename (item), NULL);
    FILE *fp = fopen (path, "wb");
    fprintf (fp, "[Desktop Entry]\nType=Link\n");
    fprintf (fp, "Name=%s\n", menu_cache_item_get_name (item));
    fprintf (fp, "Icon=%s\n", menu_cache_item_get_icon (item));
    fprintf (fp, "URL=/usr/share/applications/%s\n", menu_cache_item_get_file_basename (item));
    fclose (fp);
    g_free (path);
    destroy_window (m);
}

static void handle_menu_item_add_to_launcher (GtkWidget *mi, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    add_to_launcher (gtk_widget_get_name (mi));
    destroy_window (m);
}

static void handle_menu_item_properties (GtkWidget *mi, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    MenuCacheItem *item = menu_cache_find_item_by_id (m->menu_cache, gtk_widget_get_name (mi));
    show_properties_dialog (item);
}

/* Load menu from cache */

static void insert_system_menu (NmenuPlugin *m, GtkMenu *menu, int position)
{
    MenuCacheDir *dir = NULL;

    if (m->applist) gtk_list_store_clear (m->applist);
    while (dir == NULL) dir = menu_cache_dup_root_dir (m->menu_cache);

    sys_menu_load_submenu (m, dir, GTK_WIDGET (menu), position);
    menu_cache_item_unref (MENU_CACHE_ITEM (dir));
}

static void sys_menu_load_submenu (NmenuPlugin* m, MenuCacheDir* dir, GtkWidget *, int)
{
    GSList *l, *children;

    if (!menu_cache_dir_is_visible (dir)) return;

    children = menu_cache_dir_list_children (dir);

    for (l = children; l; l = l->next)
    {
        MenuCacheItem* item = MENU_CACHE_ITEM (l->data);
        if ((menu_cache_item_get_type (item) != MENU_CACHE_TYPE_APP) || (menu_cache_app_get_is_visible (MENU_CACHE_APP (item), SHOW_IN_LXDE)))
        {
            /* process subentries */
            if (menu_cache_item_get_type (item) == MENU_CACHE_TYPE_DIR)
            {
                sys_menu_load_submenu (m, MENU_CACHE_DIR (item), NULL, -1);
            }
            else create_system_menu_item (item, m);
        }
    }

    g_slist_free (children);
}

static void create_system_menu_item (MenuCacheItem *item, NmenuPlugin *m)
{
    GdkPixbuf *icon;

    if (menu_cache_item_get_type (item) == MENU_CACHE_TYPE_APP)
    {
        icon = NULL;
        const char *icon_name = menu_cache_item_get_icon (item);
        int scale = gtk_widget_get_scale_factor (m->img);
        if (icon_name)
        {
            if (strstr (icon_name, "/"))
                icon = gdk_pixbuf_new_from_file_at_size (icon_name, wrap_icon_size (m) * scale, wrap_icon_size (m) * scale, NULL);
            else
            {
                icon = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (), icon_name,
                    wrap_icon_size (m), scale, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

                // fallback for packages using obsolete icon location
                if (!icon)
                {
                    char *fname = g_strdup_printf ("/usr/share/pixmaps/%s", icon_name);
                    icon = gdk_pixbuf_new_from_file_at_size (fname, wrap_icon_size (m) * scale, wrap_icon_size (m) * scale, NULL);
                    g_free (fname);
                }
            }
        }
        if (!icon)
            icon = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (), "application-x-executable",
                wrap_icon_size (m), scale, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

        gtk_list_store_insert_with_values (m->applist, NULL, -1, 0, icon, 1, menu_cache_item_get_name (item), 2, menu_cache_item_get_file_basename (item), 3, menu_cache_item_get_comment (item), -1);

        if (icon) g_object_unref (icon);
    }
}

/*----------------------------------------------------------------------------*/
/* wf-panel plugin functions                                                  */
/*----------------------------------------------------------------------------*/

/* Handler for button click */
static void menu_button_clicked (GtkWidget *, NmenuPlugin *m)
{
    CHECK_LONGPRESS
    if (m->swin && gtk_widget_is_visible (m->swin)) destroy_window (m);
    else create_window (m);
}

/* Handler for system config changed message from panel */
void menu_update_display (NmenuPlugin *m)
{
    wrap_set_taskbar_icon (m, m->img, "start-here");
    if (m->img) gtk_widget_set_size_request (m->img, wrap_icon_size (m) + 2 * m->padding, -1);

    destroy_window (m);
}

/* Handler for control message */
gboolean menu_control_msg (NmenuPlugin *m, const char *cmd)
{
    if (!strncmp (cmd, "menu", 4))
    {
        if (m->swin && gtk_widget_is_visible (m->swin)) destroy_window (m);
        else create_window (m);
        return TRUE;
    }

    return FALSE;
}

/* Handler for padding update from variable watcher */
void menu_set_padding (NmenuPlugin *m)
{
    gtk_widget_set_size_request (m->img, wrap_icon_size (m) + 2 * m->padding, -1);
}

void menu_init (NmenuPlugin *m)
{
    char *path;

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    /* Create directories for the menu cache to monitor */
    path = g_build_filename (g_get_home_dir (), ".local", "share", "applications", NULL);
    g_mkdir_with_parents (path, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (path);

    path = g_build_filename (g_get_home_dir (), ".local", "share", "desktop-directories", NULL);
    g_mkdir_with_parents (path, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (path);

    /* Allocate icon as a child of top level */
    m->img = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (m->plugin), m->img);
    wrap_set_taskbar_icon (m, m->img, "start-here");
    gtk_widget_set_size_request (m->img, wrap_icon_size (m) + 2 * m->padding, -1);
    gtk_widget_set_tooltip_text (m->img, m->tooltips ? _("Click here to open applications menu") : NULL);

    /* Set up button */
    gtk_button_set_relief (GTK_BUTTON (m->plugin), GTK_RELIEF_NONE);
    g_signal_connect (m->plugin, "clicked", G_CALLBACK (menu_button_clicked), m);

    /* Set up variables */
    m->applist = gtk_list_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    m->swin = NULL;
    m->menu = NULL;

    gboolean need_prefix = (g_getenv ("XDG_MENU_PREFIX") == NULL);
    m->menu_cache = menu_cache_lookup (need_prefix ? "lxde-applications.menu" : "applications.menu");
    if (m->menu_cache == NULL) g_warning ("Error loading applications menu");

    // we don't need a notification, but if you don't call this, the cache never loads...
    m->reload_notify = menu_cache_add_reload_notify (m->menu_cache, NULL, NULL);

    /* Show the widget and return */
    gtk_widget_show_all (m->plugin);
}

void menu_destructor (gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;

    destroy_window (m);

    if (m->applist) gtk_list_store_clear (m->applist);
    if (m->menu_cache)
    {
        menu_cache_remove_reload_notify (m->menu_cache, m->reload_notify);
        // unref'ing the menu cache causes a segfault because its io thread isn't being closed...
    }

    if (m->migesture) g_object_unref (m->migesture);

    g_free (m);
}

/* End of file */
/*----------------------------------------------------------------------------*/
