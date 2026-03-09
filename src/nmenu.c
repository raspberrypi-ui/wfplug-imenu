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

conf_table_t conf_table[5] = {
    {CONF_TYPE_INT,  "padding",          N_("Icon horizontal padding"),     NULL},
    {CONF_TYPE_BOOL, "show_tooltips",    N_("Show tooltips"),               NULL},
    {CONF_TYPE_BOOL, "alpha_sort",       N_("Sort items alphabetically"),   NULL},
    {CONF_TYPE_BOOL, "hierarchic",       N_("Use menu categories"),         NULL},
    {CONF_TYPE_NONE, NULL,               NULL,                              NULL}
};

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void create_window (NmenuPlugin *m);
static void destroy_window (NmenuPlugin *m);
static void window_destroyed (GtkWidget *, gpointer data);
static gboolean filter_apps (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
static gboolean handle_clickaway (GtkWidget *, GdkEventButton *, gpointer user_data);
static void handle_iconview_selected (GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
static gboolean handle_iconview_buttonpress (GtkWidget *, GdkEventButton *event, gpointer user_data);
static gboolean handle_iconview_buttonrel (GtkWidget *, GdkEventButton *event, gpointer user_data);
static gboolean handle_iconview_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data);
static void handle_gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer);
static void handle_gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer user_data);
static void handle_drag_and_drop_done (GtkTreeModel *, GtkTreePath *, gpointer user_data);
static gboolean handle_search_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data);
static void handle_search_changed (GtkWidget *, gpointer user_data);
static gboolean handle_search_button (GtkWidget *, GdkEventButton *event, gpointer user_data);
static void append_to_entry (GtkWidget *entry, char val);
static void create_cs_menu (NmenuPlugin *m, char *id, int x, int y);
static void handle_menu_item_add_to_desktop (GtkWidget *mi, gpointer user_data);
static void handle_menu_item_add_to_launcher (GtkWidget *mi, gpointer);
static void handle_menu_item_properties (GtkWidget *mi, gpointer user_data);
static int read_menu_cache (NmenuPlugin *m);
static void free_entry (MenuEntry *ent);
static int compare_entries (MenuEntry *a, MenuEntry *b);
static void load_menu (NmenuPlugin* m, MenuCacheDir* dir);
static int read_menu_cache_hierarchic (NmenuPlugin *m);
static int load_menu_hierarchic (NmenuPlugin* m, MenuCacheDir* dir, int menu);
static gboolean filter_apps_hierarchic (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
static void change_dir (NmenuPlugin *m, char *str);
static void load_sortorder (NmenuPlugin* m);
static void save_sortorder (NmenuPlugin* m);
static void set_alphasort (gboolean state);
static void menu_button_clicked (GtkWidget *, NmenuPlugin *m);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/* Icon window */

static void create_window (NmenuPlugin *m)
{
    GtkCellRenderer *prend, *trend;
    GtkTreePath *path = gtk_tree_path_new_first ();
    GtkBuilder *builder;
    GtkCellLayout *layout;
    GtkGesture *gesture;
    GdkRectangle mon, cell;
    int x, w, h, nr, nc, ni;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/gmenu.ui");
    m->swin = (GtkWidget *) gtk_builder_get_object (builder, "gmenu");
    m->stv = (GtkWidget *) gtk_builder_get_object (builder, "iconview");
    m->srch = (GtkWidget *) gtk_builder_get_object (builder, "searchbar");
    m->scrw = (GtkWidget *) gtk_builder_get_object (builder, "scrollwin");

    m->dir = 0;

    g_signal_connect (m->srch, "changed", G_CALLBACK (handle_search_changed), m);
    g_signal_connect (m->srch, "key-press-event", G_CALLBACK (handle_search_keypress), m);
    g_signal_connect (m->srch, "button_release-event", G_CALLBACK (handle_search_button), m);

    /* create the filtered list for the tree view */
    if (m->hierarchic)
    {
        ni = read_menu_cache_hierarchic (m);
        m->flist = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (m->applist), NULL));
        gtk_tree_model_filter_set_visible_func (m->flist, (GtkTreeModelFilterVisibleFunc) filter_apps_hierarchic, m, NULL);
        gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (m->flist));
    }
    else
    {
        ni = read_menu_cache (m);
        m->flist = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (m->applist), NULL));
        gtk_tree_model_filter_set_visible_func (m->flist, (GtkTreeModelFilterVisibleFunc) filter_apps, m, NULL);
        gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (m->applist));
    }
    if (m->tooltips) gtk_icon_view_set_tooltip_column (GTK_ICON_VIEW (m->stv), 3);

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
    gtk_cell_renderer_text_set_fixed_height_from_font (GTK_CELL_RENDERER_TEXT (trend), 2);
    g_object_set (trend, "wrap-width", CELL_WIDTH, "wrap-mode", PANGO_WRAP_WORD, "alignment", PANGO_ALIGN_CENTER, NULL);
    gtk_cell_layout_pack_start (layout, trend, FALSE);
    gtk_cell_layout_add_attribute (layout, trend, "text", 1);

    g_signal_connect (m->stv, "item-activated", G_CALLBACK (handle_iconview_selected), m);
    g_signal_connect (m->stv, "button-press-event", G_CALLBACK (handle_iconview_buttonpress), m);
    g_signal_connect (m->stv, "button-release-event", G_CALLBACK (handle_iconview_buttonrel), m);
    g_signal_connect (m->stv, "key-press-event", G_CALLBACK (handle_iconview_keypress), m);
    g_signal_connect (m->swin, "destroy", G_CALLBACK (window_destroyed), m);

    gesture = gtk_gesture_long_press_new (m->stv);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
    g_signal_connect (gesture, "pressed", G_CALLBACK (handle_gesture_pressed), m);
    g_signal_connect (gesture, "end", G_CALLBACK (handle_gesture_end), m);
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_TARGET);
    pressed = FALSE;

    /* realise */
    gdk_monitor_get_geometry (gdk_display_get_monitor (gdk_display_get_default (), 0), &mon);
    gtk_window_set_default_size (GTK_WINDOW (m->swin), mon.width, mon.height);
    gtk_layer_init_for_window (GTK_WINDOW (m->swin));
    gtk_layer_set_layer (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_monitor (GTK_WINDOW (m->swin), gdk_display_get_monitor (gdk_display_get_default (), 0));
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_keyboard_interactivity (GTK_WINDOW (m->swin), TRUE);

    gtk_widget_set_events (m->swin, gtk_widget_get_events (m->swin) | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect (m->swin, "button-release-event", G_CALLBACK (handle_clickaway), m);

    gtk_widget_show_all (m->swin);
    gtk_window_present (GTK_WINDOW (m->swin));

    // calculate the size
    gtk_icon_view_get_cell_rect (GTK_ICON_VIEW (m->stv), path, NULL, &cell);

    // find the largest number of columns that will fit...
    nc = mon.width / cell.width;
    ni += ni / 2;
    for (x = nc; x > 0; x--)
    {
        // for each possible number of columns, calculate the window height
        // and compare the resulting window to the aspect ratio of the display
        w = x * cell.width;
        nr = ni / x;
        h = nr * cell.height;
        if (h > (w * mon.height) / mon.width) break;
    }

    // check the resulting window would actually fit - if not, maximise based on display size
    if (h > mon.height - cell.height)
    {
        w = mon.width - cell.width;
        h = mon.height - cell.height;
    }

    gtk_scrolled_window_set_max_content_width (GTK_SCROLLED_WINDOW (m->scrw), w);
    gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (m->scrw), w);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (m->scrw), h);
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (m->scrw), h);
    g_object_unref (builder);
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
    if ((!pstr || g_strcmp0 (str, pstr)) && strcasestr (str, gtk_entry_get_text (GTK_ENTRY (m->srch)))) res = TRUE;
    if (pstr) g_free (pstr);
    g_free (str);
    return res;
}

static gboolean handle_clickaway (GtkWidget *, GdkEventButton *, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    destroy_window (m);
    return FALSE;
}

static void handle_iconview_selected (GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    gchar *str;
    GtkTreeIter fitem;
    GtkTreeModel *mod = gtk_icon_view_get_model (iconview);
    gtk_tree_model_get_iter (mod, &fitem, path);
    gtk_tree_model_get (mod, &fitem, 2, &str, -1);

    if (strstr (str, "desktop"))
    {
        gtk_launch (str);
        destroy_window (m);
    }
    else change_dir (m ,str);
}

static gboolean handle_iconview_buttonpress (GtkWidget *, GdkEventButton *event, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreeIter fitem;
    GtkTreeModel *ivm;
    GtkTreePath *path;
    char *str;

    if (event->button == 3)
    {
        path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (m->stv), event->x, event->y);
        if (path)
        {
            ivm = gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv));
            gtk_tree_model_get_iter (ivm, &fitem, path);
            gtk_tree_model_get (ivm, &fitem, 2, &str, -1);
            create_cs_menu (m, str, event->x, event->y);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean handle_iconview_buttonrel (GtkWidget *, GdkEventButton *event, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreeIter fitem;
    GtkTreeModel *ivm;
    GtkTreePath *path;
    char *str;

    if (gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv)) == GTK_TREE_MODEL (m->applist))
        gtk_icon_view_set_reorderable (GTK_ICON_VIEW (m->stv), TRUE);

    if (pressed)
    {
        pressed = FALSE;
        return FALSE;
    }

    if (event->button == 1)
    {
        path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (m->stv), event->x, event->y);
        if (path)
        {
            ivm = gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv));
            gtk_tree_model_get_iter (ivm, &fitem, path);
            gtk_tree_model_get (ivm, &fitem, 2, &str, -1);
            if (strstr (str, "desktop"))
            {
                gtk_launch (str);
                destroy_window (m);
            }
            else change_dir (m, str);
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

static void handle_gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    gtk_icon_view_set_reorderable (GTK_ICON_VIEW (m->stv), FALSE);
    pressed = TRUE;
    press_x = x;
    press_y = y;
}

static void handle_gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer user_data)
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
            create_cs_menu (m, str, press_x, press_y);
        }
    }
}

static void handle_drag_and_drop_done (GtkTreeModel *, GtkTreePath *, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    save_sortorder (m);
}

/* Search bar handling */

static gboolean handle_search_keypress (GtkWidget *, GdkEventKey *event, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreeModel *mod;
    GtkTreeIter iter;
    gchar *str;
    GList *list;
    gboolean ret;

    switch (event->keyval)
    {
        case GDK_KEY_KP_Enter :
        case GDK_KEY_Return :   mod = gtk_icon_view_get_model (GTK_ICON_VIEW (m->stv));
                                list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (m->stv));
                                if (list)
                                {
                                    gtk_tree_model_get_iter (mod, &iter, (GtkTreePath *) list->data); 
                                    gtk_tree_model_get (mod, &iter, 2, &str, -1);
                                    if (strstr (str, "desktop"))
                                    {
                                        gtk_launch (str);
                                        destroy_window (m);
                                    }
                                    else change_dir (m, str);
                                }
                                return TRUE;

        case GDK_KEY_Escape :   destroy_window (m);
                                return TRUE;

        case GDK_KEY_Up :
        case GDK_KEY_Down :
        case GDK_KEY_Left :
        case GDK_KEY_Right :    gtk_widget_grab_focus (m->stv);
                                // propagate the key press event to the icon view...
                                g_signal_emit_by_name (m->stv, "key-press-event", event, &ret);
                                return FALSE;

        default :               return FALSE;
    }
}

static void handle_search_changed (GtkWidget *entry, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreePath *path = gtk_tree_path_new_from_indices (0, -1);

    if (!m->hierarchic && !strlen (gtk_entry_get_text (GTK_ENTRY (entry))))
    {
        gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (m->applist));
        gtk_icon_view_set_reorderable (GTK_ICON_VIEW (m->stv), TRUE);
    }
    else
    {
        gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (m->flist));
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (m->flist));
        gtk_icon_view_set_reorderable (GTK_ICON_VIEW (m->stv), FALSE);
    }

    gtk_icon_view_select_path (GTK_ICON_VIEW (m->stv), path);
    gtk_tree_path_free (path);
}

static gboolean handle_search_button (GtkWidget *, GdkEventButton *, gpointer)
{
    return TRUE;
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

static void create_cs_menu (NmenuPlugin *m, char *id, int x, int y)
{
    GtkWidget *item, *menu;
    GdkRectangle rect = {x, y, 0, 0};

    if (!strstr (id, "desktop")) return;

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
    gtk_menu_popup_at_rect (GTK_MENU (menu), gtk_widget_get_window (m->stv), &rect,
        GDK_GRAVITY_CENTER, GDK_GRAVITY_NORTH_WEST, NULL);
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
    destroy_window (m);
    show_properties_dialog (item);
}

/* Load menu from cache */

static int read_menu_cache (NmenuPlugin *m)
{
    MenuCacheDir *dir = NULL;
    MenuEntry *entry;
    GList *l;
    int count = 0;

    if (m->apps) g_list_free_full (m->apps, (GDestroyNotify) free_entry);
    m->apps = NULL;

    while (dir == NULL) dir = menu_cache_dup_root_dir (m->menu_cache);
    load_menu (m, dir);
    menu_cache_item_unref (MENU_CACHE_ITEM (dir));

    // need to do more clever things here to handle DnD
    m->apps = g_list_sort (m->apps, (GCompareFunc) compare_entries);

    // load the list store from the sorted list
    g_signal_handlers_block_by_func (m->applist, G_CALLBACK (handle_drag_and_drop_done), m);
    if (m->applist) gtk_list_store_clear (m->applist);
    l = m->apps;
    while (l)
    {
        entry = (MenuEntry *) l->data;
        gtk_list_store_insert_with_values (m->applist, NULL, -1, 0, entry->icon, 1, entry->name, 2, entry->id, 3, entry->comment, -1);
        count++;
        l = l->next;
    }
    g_signal_handlers_unblock_by_func (m->applist, G_CALLBACK (handle_drag_and_drop_done), m);
    return count;
}

static void free_entry (MenuEntry *entry)
{
    g_free (entry->id);
    g_free (entry->name);
    g_free (entry->comment);
    g_object_unref (entry->icon);
    g_free (entry);
}

static int compare_entries (MenuEntry *a, MenuEntry *b)
{
    GList *item;
    int posa, posb;

    posa = -1;
    item = a->sortorder;
    while (item)
    {
        posa++;
        if (!g_strcmp0 (a->id, item->data)) break;
        item = item->next;
    }

    posb = -1;
    item = a->sortorder;
    while (item)
    {
        posb++;
        if (!g_strcmp0 (b->id, item->data)) break;
        item = item->next;
    }

    if (posa == -1 && posb == -1) return g_ascii_strcasecmp (a->name, b->name);
    else if (posa == -1) return 1;
    else if (posb == -1) return -1;
    else return posa - posb;
}

static void load_menu (NmenuPlugin* m, MenuCacheDir* dir)
{
    GSList *l, *children;
    MenuCacheItem *item;
    MenuEntry *entry;

    if (!menu_cache_dir_is_visible (dir)) return;

    children = menu_cache_dir_list_children (dir);

    for (l = children; l; l = l->next)
    {
        item = MENU_CACHE_ITEM (l->data);
        if ((menu_cache_item_get_type (item) != MENU_CACHE_TYPE_APP) || (menu_cache_app_get_is_visible (MENU_CACHE_APP (item), SHOW_IN_LXDE)))
        {
            switch (menu_cache_item_get_type (item))
            {
                case MENU_CACHE_TYPE_DIR :  load_menu (m, MENU_CACHE_DIR (item));
                                            break;

                case MENU_CACHE_TYPE_APP :  entry = g_new0 (MenuEntry, 1);
                                            entry->id = g_strdup (menu_cache_item_get_id (item));
                                            entry->name = g_strdup (menu_cache_item_get_name (item));
                                            entry->comment = g_strdup (menu_cache_item_get_comment (item));
                                            entry->icon = load_taskbar_pixbuf (m->plugin, menu_cache_item_get_icon (item));
                                            entry->sortorder = m->sortorder;
                                            m->apps = g_list_prepend (m->apps, entry);
                                            break;

                default:                    break;
            }
        }
    }

    g_slist_free (children);
}

/* Hierarchic view */

static int read_menu_cache_hierarchic (NmenuPlugin *m)
{
    MenuCacheDir *dir = NULL;
    int count = 0;
    char *str;

    if (m->applist) gtk_list_store_clear (m->applist);

    while (dir == NULL) dir = menu_cache_dup_root_dir (m->menu_cache);
    count = load_menu_hierarchic (m, dir, 0);
    menu_cache_item_unref (MENU_CACHE_ITEM (dir));

    str = g_strdup_printf ("%d", 0);
    gtk_list_store_insert_with_values (m->applist, NULL, -1, 0, load_taskbar_pixbuf (m->plugin, "go-previous"), 1, _("Back"), 2, str, 4, -1, -1);
    g_free (str);

    return count;
}

static int load_menu_hierarchic (NmenuPlugin* m, MenuCacheDir* dir, int menu)
{
    GSList *l, *children;
    MenuCacheItem *item;
    int count = 0, max = 0, this = menu, res;
    char *str;

    if (!menu_cache_dir_is_visible (dir)) return 0;

    children = menu_cache_dir_list_children (dir);

    for (l = children; l; l = l->next)
    {
        item = MENU_CACHE_ITEM (l->data);
        switch (menu_cache_item_get_type (item))
        {
            case MENU_CACHE_TYPE_DIR :  menu++;
                                        res = load_menu_hierarchic (m, MENU_CACHE_DIR (item), menu) + 1;
                                        if (res > 1)
                                        {
                                            str = g_strdup_printf ("%d", menu);
                                            gtk_list_store_insert_with_values (m->applist, NULL, -1, 0, load_taskbar_pixbuf (m->plugin, menu_cache_item_get_icon (item)),
                                                1, menu_cache_item_get_name (item), 2, str, 3, menu_cache_item_get_comment (item), 4, this, -1);
                                            g_free (str);
                                            count++;
                                            if (res > max) max = res;
                                        }
                                        break;

            case MENU_CACHE_TYPE_APP :  if (!menu_cache_app_get_is_visible (MENU_CACHE_APP (item), SHOW_IN_LXDE)) continue;
                                        gtk_list_store_insert_with_values (m->applist, NULL, -1, 0, load_taskbar_pixbuf (m->plugin, menu_cache_item_get_icon (item)),
                                            1, menu_cache_item_get_name (item), 2, menu_cache_item_get_id (item), 3, menu_cache_item_get_comment (item), 4, this, -1);
                                        count++;
                                        break;

            default:                    break;
        }
    }

    g_slist_free (children);
    if (count > max) max = count;
    return max;
}

static gboolean filter_apps_hierarchic (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    gboolean res = FALSE;
    char *str;
    int menu;

    gtk_tree_model_get (model, iter, 1, &str, 4, &menu, -1);
    if (menu == -1 && m->dir != 0) res = TRUE;
    else if (menu == m->dir)
    {
        if (!gtk_entry_get_text (GTK_ENTRY (m->srch)) || strcasestr (str, gtk_entry_get_text (GTK_ENTRY (m->srch)))) res = TRUE;
    }
    g_free (str);
    return res;
}

static void change_dir (NmenuPlugin *m, char *str)
{
    sscanf (str, "%d", &(m->dir));
    gtk_entry_set_text (GTK_ENTRY (m->srch), "");
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (m->flist));
}

/* Sorting */

static void load_sortorder (NmenuPlugin* m)
{
    FILE *fp;
    char line[256];
    char *str;

    m->sortorder = NULL;

    str = g_build_filename (g_get_user_config_dir (), "wf-panel-pi", "icon_order", NULL);
    fp = fopen (str, "rb");
    g_free (str);

    if (fp)
    {
        while (fgets (line, 255, fp))
        {
            line[strlen(line) - 1] = 0;
            m->sortorder = g_list_prepend (m->sortorder, g_strdup (line));
        }
        fclose (fp);
        m->sortorder = g_list_reverse (m->sortorder);
        m->alphasort = FALSE;
    }
    else m->alphasort = TRUE;
    set_alphasort (m->alphasort);
}

static void save_sortorder (NmenuPlugin* m)
{
    FILE *fp;
    GtkTreeIter iter;
    gboolean valid;
    char *str;

    g_list_free_full (m->sortorder, (GDestroyNotify) g_free);
    m->sortorder = NULL;

    str = g_build_filename (g_get_user_config_dir (), "wf-panel-pi", "icon_order", NULL);
    fp = fopen (str, "wb");
    g_free (str);

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (m->applist), &iter);
    while (valid)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (m->applist), &iter, 2, &str, -1);
        fprintf (fp, "%s\n", str);
        m->sortorder = g_list_prepend (m->sortorder, g_strdup (str));
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (m->applist), &iter);
    }

    fclose (fp);

    m->sortorder = g_list_reverse (m->sortorder);
    m->alphasort = FALSE;
    set_alphasort (m->alphasort);
}

void clear_sortorder (NmenuPlugin *m)
{
    char *str;

    str = g_build_filename (g_get_user_config_dir (), "wf-panel-pi", "icon_order", NULL);
    unlink (str);
    g_free (str);

    g_list_free_full (m->sortorder, (GDestroyNotify) g_free);
    m->sortorder = NULL;

    m->alphasort = TRUE;
}

static void set_alphasort (gboolean state)
{
    char *filename, *str;
    GKeyFile *kf;
    gsize len;

    filename = g_build_filename (g_get_user_config_dir (), "wf-panel-pi", "wf-panel-pi.ini", NULL);
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, filename, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
    g_key_file_set_boolean (kf, "panel", "nmenu_alpha_sort", state);
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (filename, str, len, NULL);
    g_free (str);
    g_key_file_free (kf);
    g_free (filename);
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

    if (m->swin && gtk_widget_is_visible (m->swin)) destroy_window (m);
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
    m->applist = gtk_list_store_new (5, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    g_signal_connect (m->applist, "row-deleted", G_CALLBACK (handle_drag_and_drop_done), m);
    m->swin = NULL;

    /* Load the sort list */
    load_sortorder (m);

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
