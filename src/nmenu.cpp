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

#include <glibmm.h>
#include "gtk-utils.hpp"
#include "nmenu.hpp"

extern "C" {
    WayfireWidget *create () { return new WayfireNmenu; }
    void destroy (WayfireWidget *w) { delete w; }

    const conf_table_t *config_params (void) { return conf_table; };
    const char *display_name (void) { return PLUGIN_TITLE; };
    const char *package_name (void) { return GETTEXT_PACKAGE; };
}

void WayfireNmenu::read_settings (void)
{
    m->padding = padding;
    m->tooltips = show_tooltips;
    m->alphasort = alphasort;
    m->hierarchic = hierarchic;
    if (!gdk_rgba_parse (&m->overlay_col, ((std::string) overlay_col).c_str()))
        gdk_rgba_parse (&m->overlay_col, "dark gray");
    if (!gdk_rgba_parse (&m->overlay_text_col, ((std::string) overlay_text_col).c_str()))
        gdk_rgba_parse (&m->overlay_text_col, "light gray");
}

void WayfireNmenu::settings_changed_cb (void)
{
    read_settings ();
    menu_set_padding (m);
    if (m->alphasort) clear_sortorder (m);
    gtk_widget_set_tooltip_text (m->img, m->tooltips ? _("Click here to open applications menu") : NULL);
    handle_reload_menu (NULL, m);
}

void WayfireNmenu::command (const char *cmd)
{
    menu_control_msg (m, cmd);
}

bool WayfireNmenu::set_icon (void)
{
    menu_update_display (m);
    return false;
}

void WayfireNmenu::init (Gtk::HBox *container)
{
    /* Create the button */
    plugin = std::make_unique <Gtk::Button> ();
    plugin->set_name (PLUGIN_NAME);
    container->pack_start (*plugin, false, false);

    /* Setup structure */
    m = g_new0 (NmenuPlugin, 1);
    m->plugin = (GtkWidget *)((*plugin).gobj());
    icon_timer = Glib::signal_idle().connect (sigc::mem_fun (*this, &WayfireNmenu::set_icon));

    /* Add long press for right click */
    gesture = add_longpress_default (*plugin);

    /* Initialise the plugin */
    read_settings ();
    menu_init (m);

    /* Setup callbacks */
    padding.set_callback (sigc::mem_fun (*this, &WayfireNmenu::settings_changed_cb));
    show_tooltips.set_callback (sigc::mem_fun (*this, &WayfireNmenu::settings_changed_cb));
    alphasort.set_callback (sigc::mem_fun (*this, &WayfireNmenu::settings_changed_cb));
    hierarchic.set_callback (sigc::mem_fun (*this, &WayfireNmenu::settings_changed_cb));
    overlay_col.set_callback (sigc::mem_fun (*this, &WayfireNmenu::settings_changed_cb));
    overlay_text_col.set_callback (sigc::mem_fun (*this, &WayfireNmenu::settings_changed_cb));
}

WayfireNmenu::~WayfireNmenu()
{
    icon_timer.disconnect ();
    menu_destructor (m);
}

/* End of file */
/*----------------------------------------------------------------------------*/
