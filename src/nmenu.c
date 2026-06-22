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

#define ENTRY_ICON      0
#define ENTRY_NAME      1
#define ENTRY_ID        2
#define ENTRY_COMMENT   3
#define ENTRY_LABEL     4
#define ENTRY_MENU      5

#define NUM_LINES   2

typedef enum
{
    FM_WP_COLOR,
    FM_WP_STRETCH,
    FM_WP_FIT,
    FM_WP_CENTER,
    FM_WP_TILE,
    FM_WP_CROP
}FmWallpaperMode;

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

conf_table_t conf_table[8] = {
    {CONF_TYPE_INT,     "padding",          N_("Icon horizontal padding"),          NULL},
    {CONF_TYPE_BOOL,    "show_tooltips",    N_("Show tooltips"),                    NULL},
    {CONF_TYPE_BOOL,    "alpha_sort",       N_("Sort items alphabetically"),        NULL},
    {CONF_TYPE_BOOL,    "hierarchic",       N_("Use menu categories"),              NULL},
    {CONF_TYPE_BOOL,    "comp_icons",       N_("Show composite category icons"),    NULL},
    {CONF_TYPE_COLOUR,  "overlay_col",      N_("Overlay colour"),                   NULL},
    {CONF_TYPE_COLOUR,  "overlay_text_col", N_("Overlay text colour"),              NULL},
    {CONF_TYPE_NONE,    NULL,               NULL,                                   NULL}
};

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static GdkMonitor *get_monitor (NmenuPlugin *m);
static void create_window (NmenuPlugin *m);
static void preload_background (NmenuPlugin *m);
static void load_background (NmenuPlugin *m, GdkWindow *window);
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
static int load_menu_hierarchic (NmenuPlugin* m, MenuCacheDir* dir, int menu, char **list, GdkPixbuf **icon);
static gboolean filter_apps_hierarchic (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
static void change_dir (NmenuPlugin *m, char *str);
static void set_title (NmenuPlugin *m);
static char *ellipsize_lines (NmenuPlugin *m, const char *text);
static void load_sortorder (NmenuPlugin* m);
static void save_sortorder (NmenuPlugin* m);
static void set_alphasort (gboolean state);
static void menu_button_clicked (GtkWidget *, NmenuPlugin *m);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

static GdkMonitor *get_monitor (NmenuPlugin *m)
{
    GtkWindow *panel = find_panel (m->plugin);
    return gtk_layer_get_monitor (panel);
}

/* Icon window */

static void create_window (NmenuPlugin *m)
{
    GtkCellRenderer *prend, *trend;
    GtkTreePath *path = gtk_tree_path_new_first ();
    GtkBuilder *builder;
    GtkCellLayout *layout;
    GtkGesture *gesture;
    GdkRectangle mon, cell;
    GdkMonitor *monitor;
    GtkStyleContext *style_context;
    GtkStateFlags state;
    PangoContext *context;
    PangoFontMetrics *metrics;
    PangoFontDescription *font_desc;
    int x, w, h, nr, nc, xs, ys;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/gmenu.ui");
    m->swin = (GtkWidget *) gtk_builder_get_object (builder, "gmenu");
    m->stv = (GtkWidget *) gtk_builder_get_object (builder, "iconview");
    m->srch = (GtkWidget *) gtk_builder_get_object (builder, "searchbar");
    m->scrw = (GtkWidget *) gtk_builder_get_object (builder, "scrollwin");
    m->title = (GtkWidget *) gtk_builder_get_object (builder, "title");

    m->dir = 0;

    g_signal_connect (m->srch, "changed", G_CALLBACK (handle_search_changed), m);
    g_signal_connect (m->srch, "key-press-event", G_CALLBACK (handle_search_keypress), m);
    g_signal_connect (m->srch, "button_release-event", G_CALLBACK (handle_search_button), m);

    /* create the filtered list for the tree view */
    if (m->hierarchic)
    {
        m->flist = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (m->applist), NULL));
        gtk_tree_model_filter_set_visible_func (m->flist, (GtkTreeModelFilterVisibleFunc) filter_apps_hierarchic, m, NULL);
        gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (m->flist));
    }
    else
    {
        m->flist = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (m->applist), NULL));
        gtk_tree_model_filter_set_visible_func (m->flist, (GtkTreeModelFilterVisibleFunc) filter_apps, m, NULL);
        gtk_icon_view_set_model (GTK_ICON_VIEW (m->stv), GTK_TREE_MODEL (m->applist));
    }
    if (m->tooltips) gtk_icon_view_set_tooltip_column (GTK_ICON_VIEW (m->stv), ENTRY_COMMENT);
    set_title (m);

    /* set up the icon view */
    layout = GTK_CELL_LAYOUT (m->stv);

    prend = gtk_cell_renderer_pixbuf_new ();
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, gtk_widget_get_scale_factor (m->img));
    g_object_set_property (G_OBJECT (prend), "scale", &val);
    gtk_cell_layout_pack_start (layout, prend, FALSE);
    gtk_cell_layout_add_attribute (layout, prend, "pixbuf", ENTRY_ICON);

    /* get the font height */
    style_context = gtk_widget_get_style_context (m->stv);
    state = gtk_widget_get_state_flags (m->stv);
    gtk_style_context_get (style_context, state, "font", &font_desc, NULL);
    context = gtk_widget_get_pango_context (m->stv);
    metrics = pango_context_get_metrics (context, font_desc, pango_context_get_language (context));
    h = pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics);
    pango_font_metrics_unref (metrics);
    pango_font_description_free (font_desc);

    trend = gtk_cell_renderer_text_new ();
    gtk_cell_renderer_set_alignment (trend, 0.5, 0.0);
    gtk_cell_renderer_set_fixed_size (trend, h / 192, NUM_LINES * PANGO_PIXELS (h));
    g_object_set (trend, "wrap-width", h / 192, "wrap-mode", PANGO_WRAP_WORD, "alignment", PANGO_ALIGN_CENTER, NULL);
    gtk_cell_layout_pack_start (layout, trend, FALSE);
    gtk_cell_layout_add_attribute (layout, trend, "markup", ENTRY_LABEL);

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
    monitor = get_monitor (m);
    gdk_monitor_get_geometry (monitor, &mon);
    gtk_window_set_default_size (GTK_WINDOW (m->swin), mon.width, mon.height);
    gtk_layer_init_for_window (GTK_WINDOW (m->swin));
    gtk_layer_set_layer (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_monitor (GTK_WINDOW (m->swin), monitor);
    gtk_layer_set_exclusive_zone (GTK_WINDOW (m->swin), -1);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor (GTK_WINDOW (m->swin), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_keyboard_interactivity (GTK_WINDOW (m->swin), TRUE);

    gtk_widget_set_events (m->swin, gtk_widget_get_events (m->swin) | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect (m->swin, "button-release-event", G_CALLBACK (handle_clickaway), m);

    gtk_widget_show_all (m->swin);  // 30ms
    load_background (m, gtk_widget_get_window (m->swin));    // 40ms
    gtk_widget_set_visible (m->title, m->hierarchic);
    gtk_window_present (GTK_WINDOW (m->swin));

    // calculate the size
    gtk_icon_view_get_cell_rect (GTK_ICON_VIEW (m->stv), path, NULL, &cell);
    xs = gtk_icon_view_get_row_spacing (GTK_ICON_VIEW (m->stv));
    ys = gtk_icon_view_get_column_spacing (GTK_ICON_VIEW (m->stv));

    // find the largest number of columns that will fit...
    w = 0;  // fix warning
    nc = mon.width / cell.width;
    for (x = 1; x <= nc - 1; x++)
    {
        // for each possible number of columns, calculate the window height
        // and compare the resulting window to the aspect ratio of the display
        w = x * cell.width + (x - 1) * xs;
        nr = (m->napps + x - 1) / x;
        h = nr * cell.height + (nr - 1) * ys;
        if (h <= (w * mon.height) / mon.width) break;
    }

    // constrain to display size
    if (h > mon.height - cell.height) h = mon.height - cell.height;

    gtk_scrolled_window_set_max_content_width (GTK_SCROLLED_WINDOW (m->scrw), w);
    gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (m->scrw), w);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (m->scrw), h);
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (m->scrw), h);
    g_object_unref (builder);
}

static void preload_background (NmenuPlugin *m)
{
    GdkPixbuf *pix, *modpix;
    GdkRGBA desktop_bg;
    GdkRectangle geom;
    GdkMonitor *mon;
    GdkDisplay *disp;
    int src_x, src_y, src_w, src_h, dest_x, dest_y, dest_w, dest_h, w, h, mnum;
    guint32 pixcol;
    FmWallpaperMode wp_mode;
    char *fname, *buf, *wallpaper = NULL;
    GKeyFile *kf;
    GError *err;
    gboolean common = FALSE;

    // get common desktop setting
    for (w = 0; w < 2; w++)
    {
        fname = g_build_filename (w == 0 ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", "default", "pcmanfm.conf", NULL);
        kf = g_key_file_new ();
        if (g_key_file_load_from_file (kf, fname, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
        {
            err = NULL;
            mnum = g_key_file_get_integer (kf, "ui", "common_bg", &err);
            if (err == NULL) common = mnum;
            g_key_file_free (kf);
        }
        g_free (fname);
    }

    // find the monitor number
    disp = gdk_display_get_default ();
    mon = get_monitor (m);
    for (mnum = 0; mnum < gdk_display_get_n_monitors (disp); mnum++)
    {
        if (gdk_display_get_monitor (disp, mnum) == mon) break;
    }

    wp_mode = FM_WP_COLOR;  // fix warning
    for (w = 0; w < 2; w++)
    {
        if (w == 0)
        {
            // load sys defaults
            fname = g_strdup_printf ("desktop-items-%u.conf", common ? 0 : mnum);
            buf = g_build_filename ("/etc", "xdg", "pcmanfm", "default", fname, NULL);
            g_free (fname);
        }
        else
        {
            // load user config
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (disp), mnum);
#pragma GCC diagnostic pop
            fname = g_strdup_printf ("desktop-items-%s.conf", common ? "0" : buf);
            g_free (buf);
            buf = g_build_filename (g_get_user_config_dir (), "pcmanfm", "default", fname, NULL);
            g_free (fname);
        }

        // read in data from file to a key file
        kf = g_key_file_new ();
        if (g_key_file_load_from_file (kf, buf, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
        {
            g_free (buf);
            // get data from the key file
            err = NULL;
            buf = g_key_file_get_string (kf, "*", "desktop_bg", &err);
            if (err == NULL && buf) gdk_rgba_parse (&desktop_bg, buf);
            g_free (buf);

            err = NULL;
            buf = g_key_file_get_string (kf, "*", "wallpaper", &err);
            if (err == NULL && buf)
            {
                if (wallpaper) g_free (wallpaper);
                wallpaper = g_strdup (buf);
            }
            g_free (buf);

            err = NULL;
            buf = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
            if (err == NULL && buf)
            {
                if (!g_strcmp0 (buf, "color")) wp_mode = FM_WP_COLOR;
                if (!g_strcmp0 (buf, "stretch")) wp_mode = FM_WP_STRETCH;
                if (!g_strcmp0 (buf, "fit")) wp_mode = FM_WP_FIT;
                if (!g_strcmp0 (buf, "center")) wp_mode = FM_WP_CENTER;
                if (!g_strcmp0 (buf, "tile")) wp_mode = FM_WP_TILE;
                if (!g_strcmp0 (buf, "crop")) wp_mode = FM_WP_CROP;
            }
        }
        g_free (buf);
        g_key_file_free (kf);
    }

    gdk_monitor_get_geometry (get_monitor (m), &geom);
    dest_w = geom.width;
    dest_h = geom.height;

    if (wp_mode != FM_WP_COLOR)
    {
        pix = gdk_pixbuf_new_from_file (wallpaper, NULL);
        src_w = gdk_pixbuf_get_width (pix);
        src_h = gdk_pixbuf_get_height (pix);
    }
    else pix = NULL;

    pixcol = (int)(desktop_bg.alpha * 255) + (((int)(desktop_bg.blue * 255)) << 8)
        + (((int)(desktop_bg.green * 255)) << 16) + (((int)(desktop_bg.red * 255)) << 24);

    // create a new pixbuf filled with background
    m->background = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, dest_w, dest_h);
    gdk_pixbuf_fill (m->background, pixcol);

    switch (wp_mode)
    {
        case FM_WP_STRETCH:
            // simple scaling to new size
            modpix = gdk_pixbuf_scale_simple (pix, dest_w, dest_h, GDK_INTERP_BILINEAR);
            g_object_unref (pix);
            pix = modpix;
            gdk_pixbuf_composite (pix, m->background, 0, 0, dest_w, dest_h, 0, 0, 1, 1, GDK_INTERP_BILINEAR, 255);
            break;

        case FM_WP_FIT:
        case FM_WP_CROP:
            // create a consistent x-y scaling to fit either the shortest or the longest side to the screen
            w = dest_w * src_h;
            h = dest_h * src_w;
            if (w != h)
            {
                if ((wp_mode == FM_WP_FIT && w < h) || (wp_mode == FM_WP_CROP && w > h))
                {
                    src_h = w / src_w;
                    src_w = dest_w;
                }
                else
                {
                    src_w = h / src_h;
                    src_h = dest_h;
                }
                modpix = gdk_pixbuf_scale_simple (pix, src_w, src_h, GDK_INTERP_BILINEAR);
                g_object_unref (pix);
                pix = modpix;
            }
            // fallthrough
        case FM_WP_CENTER:
            // calculate how to centre the scaled pixbuf, discarding edges if needed
            src_x = src_w > dest_w ? (src_w - dest_w) / 2 : 0;
            src_y = src_h > dest_h ? (src_h - dest_h) / 2 : 0;
            w = MIN (src_w, dest_w);
            h = MIN (src_h, dest_h);
            dest_x = dest_w > src_w ? (dest_w - src_w) / 2 : 0;
            dest_y = dest_h > src_h ? (dest_h - src_h) / 2 : 0;
            gdk_pixbuf_composite (pix, m->background, dest_x, dest_y, w, h, dest_x - src_x, dest_y - src_y, 1, 1, GDK_INTERP_BILINEAR, 255);
            break;

        case FM_WP_TILE:
            // loop x and y, copying the source repeatedly into the destination pixbuf
            dest_y = 0;
            while (dest_y < dest_h)
            {
                dest_x = 0;
                while (dest_x < dest_w)
                {
                    w = dest_x + src_w > dest_w ? dest_w - dest_x : src_w;
                    h = dest_y + src_h > dest_h ? dest_h - dest_y : src_h;
                    gdk_pixbuf_composite (pix, m->background, dest_x, dest_y, w, h, dest_x, dest_y, 1, 1, GDK_INTERP_BILINEAR, 255);
                    dest_x += src_w;
                }
                dest_y += src_h;
            }
            break;

        default : break;
    }
    if (pix) g_object_unref (pix);
}

static void load_background (NmenuPlugin *m, GdkWindow *window)
{
    cairo_t *cr;
    cairo_surface_t *bg;
    cairo_pattern_t *pattern;
    GdkRectangle geom;

    gdk_monitor_get_geometry (get_monitor (m), &geom);

    bg = cairo_image_surface_create (CAIRO_FORMAT_RGB24, geom.width, geom.height);
    cr = cairo_create (bg);

    gdk_cairo_set_source_pixbuf (cr, m->background, 0, 0);
    cairo_paint (cr);

    gdk_cairo_set_source_rgba (cr, &(m->overlay_col));
    cairo_rectangle (cr, 0, 0, geom.width, geom.height);
    cairo_fill (cr);
    cairo_destroy (cr);

    pattern = cairo_pattern_create_for_surface (bg);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gdk_window_set_background_pattern (window, pattern);
#pragma GCC diagnostic pop

    gdk_window_invalidate_rect (window, NULL, TRUE);
    cairo_pattern_destroy (pattern);
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

    if (!m->swin) return TRUE;
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

    if (strstr (str, ".desktop"))
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
            if (strstr (str, ".desktop"))
            {
                gtk_launch (str);
                destroy_window (m);
            }
            else change_dir (m, str);
        } else destroy_window (m);

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
        case GDK_KEY_Escape :   if (m->dir != 0)
                                {
                                    m->dir = 0;
                                    gtk_entry_set_text (GTK_ENTRY (m->srch), "");
                                    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (m->flist));
                                    set_title (m);
                                }
                                else destroy_window (m);
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
                                    if (strstr (str, ".desktop"))
                                    {
                                        gtk_launch (str);
                                        destroy_window (m);
                                    }
                                    else change_dir (m, str);
                                }
                                return TRUE;

        case GDK_KEY_Escape :   if (m->dir != 0)
                                {
                                    m->dir = 0;
                                    gtk_entry_set_text (GTK_ENTRY (m->srch), "");
                                    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (m->flist));
                                    set_title (m);
                                }
                                else destroy_window (m);
                                return TRUE;

        case GDK_KEY_Up :
        case GDK_KEY_Left :     gtk_widget_grab_focus (m->stv);
                                // propagate the key press event to the icon view...
                                g_signal_emit_by_name (m->stv, "key-press-event", event, &ret);
                                return FALSE;

        case GDK_KEY_Right :    gtk_widget_grab_focus (m->stv);
                                // propagate the key press event to the icon view. Twice...
                                g_signal_emit_by_name (m->stv, "key-press-event", event, &ret);
                                g_signal_emit_by_name (m->stv, "key-press-event", event, &ret);
                                return FALSE;

        case GDK_KEY_Down :     gtk_widget_grab_focus (m->stv);
                                // propagate the key press event to the icon view. Twice...
                                g_signal_emit_by_name (m->stv, "key-press-event", event, &ret);
                                g_signal_emit_by_name (m->stv, "key-press-event", event, &ret);
                                // stop the text in the search view becoming selected
                                return TRUE;

        default :               return FALSE;
    }
}

static void handle_search_changed (GtkWidget *entry, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    GtkTreePath *path = gtk_tree_path_new_from_indices (0, -1);
    char *str;

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
        if (strlen (gtk_entry_get_text (GTK_ENTRY (entry))))
        {
            str = g_strdup_printf ("<b><span color=\"#%02X%02X%02X\">%s</span></b>", (int) (m->overlay_text_col.red * 255),
                (int) (m->overlay_text_col.green * 255), (int) (m->overlay_text_col.blue * 255), _("Search Results"));
            gtk_label_set_markup (GTK_LABEL (m->title), str);
            g_free (str);
        }
        else set_title (m);
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

    if (!strstr (id, ".desktop")) return;

    menu = gtk_menu_new ();

    if (system ("pgrep swaybg > /dev/null"))
    {
        item = gtk_menu_item_new_with_label (_("Add to desktop"));
        gtk_widget_set_name (item, id);
        g_signal_connect (item, "activate", G_CALLBACK (handle_menu_item_add_to_desktop), m);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

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
    if (fp)
    {
        fprintf (fp, "[Desktop Entry]\nType=Link\n");
        fprintf (fp, "Name=%s\n", menu_cache_item_get_name (item));
        fprintf (fp, "Icon=%s\n", menu_cache_item_get_icon (item));
        fprintf (fp, "URL=/usr/share/applications/%s\n", menu_cache_item_get_file_basename (item));
        fclose (fp);
    }
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

void handle_reload_menu (MenuCache *, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;

    if (m->hierarchic)
        m->napps = read_menu_cache_hierarchic (m);
    else
        m->napps = read_menu_cache (m);
}

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
        gtk_list_store_insert_with_values (m->applist, NULL, -1, ENTRY_ICON, entry->icon, ENTRY_NAME, entry->name,
            ENTRY_ID, entry->id, ENTRY_COMMENT, entry->comment, ENTRY_LABEL, entry->label, -1);
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
    g_free (entry->label);
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
                                            entry->label = ellipsize_lines (m, menu_cache_item_get_name (item));
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
    char *str, *act;

    if (m->applist) gtk_list_store_clear (m->applist);

    act = g_strdup_printf ("%d", 0);
    str = g_strdup_printf ("<b><span color=\"#%02X%02X%02X\">%s</span></b>", (int) (m->overlay_text_col.red * 255),
        (int) (m->overlay_text_col.green * 255), (int) (m->overlay_text_col.blue * 255), _("Back"));

    gtk_list_store_insert_with_values (m->applist, NULL, -1, ENTRY_ICON, load_taskbar_pixbuf (m->plugin, "go-previous"),
        ENTRY_LABEL, str, ENTRY_NAME, _("Back"), ENTRY_ID, act, ENTRY_MENU, -1, -1);
    g_free (str);
    g_free (act);

    while (dir == NULL) dir = menu_cache_dup_root_dir (m->menu_cache);
    count = load_menu_hierarchic (m, dir, 0, NULL, NULL);
    menu_cache_item_unref (MENU_CACHE_ITEM (dir));

    return count;
}

static int load_menu_hierarchic (NmenuPlugin* m, MenuCacheDir* dir, int menu, char **list, GdkPixbuf **icon)
{
    GSList *l, *children;
    MenuCacheItem *item;
    MenuCacheType type;
    int count = 0, max = 0, this = menu, res, dim;
    char *name, *str, *title, *apptt;
    GdkPixbuf *cpb;

    if (!menu_cache_dir_is_visible (dir)) return 0;

    dim = gtk_widget_get_scale_factor (m->plugin) * get_icon_size (m->plugin) / 2;

    children = menu_cache_dir_list_children (dir);

    for (l = children; l; l = l->next)
    {
        item = MENU_CACHE_ITEM (l->data);
        type = menu_cache_item_get_type (item);

        if (type != MENU_CACHE_TYPE_DIR && type != MENU_CACHE_TYPE_APP) continue;

        name = ellipsize_lines (m, menu_cache_item_get_name (item));

        switch (menu_cache_item_get_type (item))
        {
            case MENU_CACHE_TYPE_DIR :  menu++;
                                        apptt = NULL;
                                        if (m->comp_icons)
                                            cpb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, dim * 2, dim * 2);
                                        else
                                            cpb = load_taskbar_pixbuf (m->plugin, menu_cache_item_get_icon (item));
                                        res = load_menu_hierarchic (m, MENU_CACHE_DIR (item), menu, &apptt, m->comp_icons ? &cpb : NULL) + 1;
                                        if (res > 1)
                                        {
                                            title = g_strdup_printf ("<b>%s</b>", name);
                                            str = g_strdup_printf ("%d", menu);
                                            gtk_list_store_insert_with_values (m->applist, NULL, -1, ENTRY_ICON, cpb,
                                                ENTRY_LABEL, title, ENTRY_ID, str, ENTRY_COMMENT, apptt,
                                                ENTRY_NAME, menu_cache_item_get_name (item), ENTRY_MENU, this, -1);
                                            g_free (title);
                                            g_free (str);
                                            g_free (apptt);
                                            count++;
                                            if (res > max) max = res;
                                        }
                                        g_object_unref (cpb);
                                        break;

            case MENU_CACHE_TYPE_APP :  if (!menu_cache_app_get_is_visible (MENU_CACHE_APP (item), SHOW_IN_LXDE)) continue;
                                        gtk_list_store_insert_with_values (m->applist, NULL, -1, ENTRY_ICON, load_taskbar_pixbuf (m->plugin, menu_cache_item_get_icon (item)),
                                            ENTRY_LABEL, name, ENTRY_ID, menu_cache_item_get_id (item), ENTRY_COMMENT, menu_cache_item_get_comment (item),
                                            ENTRY_NAME, menu_cache_item_get_name (item), ENTRY_MENU, this, -1);
                                        if (list)
                                        {
                                            if (*list)
                                            {
                                                str = g_strdup (*list);
                                                g_free (*list);
                                                *list = g_strdup_printf ("%s\n%s", str, name);
                                                g_free (str);
                                            }
                                            else *list = g_strdup (name);
                                        }

                                        if (icon)
                                        {
                                            if (count < 4)
                                            {
                                                cpb = load_taskbar_pixbuf (m->plugin, menu_cache_item_get_icon (item));
                                                gdk_pixbuf_composite (cpb, *icon, count % 2 == 0 ? 0 : dim, count < 2 ? 0 : dim, dim, dim,
                                                    count % 2 == 0 ? 0 : dim, count < 2 ? 0 : dim, 0.5, 0.5, GDK_INTERP_BILINEAR, 255);
                                                g_object_unref (cpb);
                                            }
                                        }
                                        count++;
                                        break;

            default:                    break;
        }
        g_free (name);
    }

    g_slist_free (children);
    if (count > max) max = count;
    return max;
}

static gboolean filter_apps_hierarchic (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NmenuPlugin *m = (NmenuPlugin *) user_data;
    gboolean res = FALSE, tmatch = FALSE;
    char *name, *id;
    int menu;

    if (!m->swin) return TRUE;

    gtk_tree_model_get (model, iter, ENTRY_NAME, &name, ENTRY_ID, &id, ENTRY_MENU, &menu, -1);

    if (strlen (gtk_entry_get_text (GTK_ENTRY (m->srch))) && strcasestr (name, gtk_entry_get_text (GTK_ENTRY (m->srch))) && strcasestr (id, ".desktop")) tmatch = TRUE;

    if (tmatch && (m->dir == 0 || m->dir == menu)) res = TRUE;
    else if (m->dir != 0 && menu == -1) res = TRUE;
    else if (!strlen (gtk_entry_get_text (GTK_ENTRY (m->srch))) && m->dir == menu) res = TRUE;

    g_free (name);
    g_free (id);
    return res;
}

static void change_dir (NmenuPlugin *m, char *str)
{
    sscanf (str, "%d", &(m->dir));
    gtk_entry_set_text (GTK_ENTRY (m->srch), "");
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (m->flist));
    set_title (m);
}

static void set_title (NmenuPlugin *m)
{
    GtkTreeIter iter;
    char *name, *id, *str;
    int menu;
    gboolean found;

    if (!m->hierarchic) return;
    if (m->dir == 0)
    {
        str = g_strdup_printf ("<b><span color=\"#%02X%02X%02X\">%s</span></b>", (int) (m->overlay_text_col.red * 255),
            (int) (m->overlay_text_col.green * 255), (int) (m->overlay_text_col.blue * 255), _("Categories"));
        gtk_label_set_markup (GTK_LABEL (m->title), str);
        g_free (str);
        return;
    }

    found = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (m->applist), &iter);
    while (found)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (m->applist), &iter, ENTRY_NAME, &name, ENTRY_ID, &id, -1);
        if (!strstr (id, ".desktop"))
        {
            sscanf (id, "%d", &menu);
            if (menu == m->dir)
            {
                str = g_strdup_printf ("<b><span color=\"#%02X%02X%02X\">%s</span></b>", (int) (m->overlay_text_col.red * 255),
                    (int) (m->overlay_text_col.green * 255), (int) (m->overlay_text_col.blue * 255), name);
                gtk_label_set_markup (GTK_LABEL (m->title), str);
                g_free (str);
                g_free (name);
                g_free (id);
                return;
            }
        }
        g_free (name);
        g_free (id);
        found = gtk_tree_model_iter_next (GTK_TREE_MODEL (m->applist), &iter);
    }
}

static char *ellipsize_lines (NmenuPlugin *m, const char *text)
{
    PangoContext *context;
    PangoLayout *layout;
    PangoFontMetrics *metrics;
    PangoFontDescription *font_desc;
    char *ptr, *str;
    int h;

    context = gtk_widget_get_pango_context (m->plugin);
    gtk_style_context_get (gtk_widget_get_style_context (m->plugin), gtk_widget_get_state_flags (m->plugin),
        "font", &font_desc, NULL);
    metrics = pango_context_get_metrics (context, font_desc, pango_context_get_language (context));
    h = pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics);
    pango_font_metrics_unref (metrics);
    pango_font_description_free (font_desc);

    layout = pango_layout_new (context);
    pango_layout_set_width (layout, PANGO_SCALE * h / 192);

    str = g_strdup (text);
    pango_layout_set_text (layout, str, -1);
    while (pango_layout_get_line_count (layout) > NUM_LINES)
    {
        ptr = strrchr (str, ' ');
        if (!ptr || ptr - 3 < str) break;
        sprintf (ptr - 3, "...");
        pango_layout_set_text (layout, str, -1);
    }

    ptr = g_markup_escape_text (str, -1);
    g_object_unref (layout);
    g_free (str);

    str = g_strdup_printf ("<span color=\"#%02X%02X%02X\">%s</span>", (int) (m->overlay_text_col.red * 255),
        (int) (m->overlay_text_col.green * 255), (int) (m->overlay_text_col.blue * 255), ptr);
    g_free (ptr);
    return str;
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
        if (fp) fprintf (fp, "%s\n", str);
        m->sortorder = g_list_prepend (m->sortorder, g_strdup (str));
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (m->applist), &iter);
    }

    if (fp) fclose (fp);

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

    preload_background (m);
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

    if (!strncmp (cmd, "bg", 4))
    {
        g_object_unref (m->background);
        preload_background (m);
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
    gtk_widget_set_tooltip_text (m->img, m->tooltips ? _("Application Launcher") : NULL);

    /* Set up button */
    gtk_button_set_relief (GTK_BUTTON (m->plugin), GTK_RELIEF_NONE);
    g_signal_connect (m->plugin, "clicked", G_CALLBACK (menu_button_clicked), m);

    /* Set up variables */
    m->applist = gtk_list_store_new (6, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    g_signal_connect (m->applist, "row-deleted", G_CALLBACK (handle_drag_and_drop_done), m);
    m->swin = NULL;

    /* Load the sort list */
    load_sortorder (m);

    gboolean need_prefix = (g_getenv ("XDG_MENU_PREFIX") == NULL);
    m->menu_cache = menu_cache_lookup (need_prefix ? "lxde-applications.menu" : "applications.menu");
    if (m->menu_cache == NULL) g_warning ("Error loading applications menu");

    m->reload_notify = menu_cache_add_reload_notify (m->menu_cache, handle_reload_menu, m);

    /* Show the widget and return */
    gtk_widget_show_all (m->plugin);

    preload_background (m);
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

    g_object_unref (m->background);
    g_free (m);
}

/* End of file */
/*----------------------------------------------------------------------------*/
