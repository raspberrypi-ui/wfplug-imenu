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
#include "nmenu.hpp"

extern "C" {
    PanelWidget *create () { return new WidgetNmenu; }
    void destroy (PanelWidget *w) { delete w; }

    const conf_table_t *config_params (void) { return conf_table; };
    const char *display_name (void) { return PLUGIN_TITLE; };
    const char *package_name (void) { return GETTEXT_PACKAGE; };
}

void WidgetNmenu::read_settings (void)
{
    conf_table[0].value = (void *) &m->padding;
    conf_table[1].value = (void *) &m->tooltips;
    conf_table[2].value = (void *) &m->alphasort;
    conf_table[3].value = (void *) &m->hierarchic;
    conf_table[4].value = (void *) &m->comp_icons;
    conf_table[5].value = (void *) &m->overlay_col;
    conf_table[6].value = (void *) &m->overlay_text_col;

    load_configuration_data (PLUGIN_NAME, conf_table);
}

void WidgetNmenu::handle_config_reload (void)
{
    load_configuration_data (PLUGIN_NAME, conf_table);

    menu_set_padding (m);
    if (m->alphasort) clear_sortorder (m);
    gtk_widget_set_tooltip_text (m->img, m->tooltips ? _("Click here to open applications menu") : NULL);
    handle_reload_menu (NULL, m);
}

void WidgetNmenu::command (const char *cmd)
{
    menu_control_msg (m, cmd);
}

bool WidgetNmenu::set_icon (void)
{
    handle_reload_menu (NULL, m);
    menu_update_display (m);
    return false;
}

void WidgetNmenu::init (Gtk::HBox *container)
{
    /* Create the button */
    plugin = std::make_unique <Gtk::Button> ();
    plugin->set_name (PLUGIN_NAME);
    container->pack_start (*plugin, false, false);

    /* Setup structure */
    m = g_new0 (NmenuPlugin, 1);
    m->plugin = (GtkWidget *)((*plugin).gobj());
    icon_timer = Glib::signal_idle().connect (sigc::mem_fun (*this, &WidgetNmenu::set_icon));

    /* Initialise the plugin */
    read_settings ();
    menu_init (m);
}

WidgetNmenu::~WidgetNmenu()
{
    icon_timer.disconnect ();
    menu_destructor (m);
}

/* End of file */
/*----------------------------------------------------------------------------*/
