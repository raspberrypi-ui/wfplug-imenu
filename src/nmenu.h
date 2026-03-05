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

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define PLUGIN_TITLE N_("Icon Menu")

typedef struct 
{
    GtkWidget *plugin;
    GtkGesture *migesture;
    GtkWidget *img;                 /* Taskbar icon */
    GtkWidget *swin;                /* Search window popup */
    GtkWidget *srch;                /* Search window search bar */
    GtkWidget *stv;                 /* Search window tree view */
    GtkWidget *scrw;                /* Search window scrolled window */
    GtkListStore *applist;
    GtkTreeModelFilter *flist;
    int padding;
    int rheight;
    gboolean tooltips;
    gboolean alphasort;
    int width;
    int height;

    MenuCache *menu_cache;
    gpointer reload_notify;

    GList *apps;
    GList *sortorder;
} NmenuPlugin;

typedef struct
{
    char *id;
    char *name;
    char *comment;
    GdkPixbuf *icon;
    GList *sortorder;
} MenuEntry;

extern conf_table_t conf_table[4];

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

extern void menu_init (NmenuPlugin *m);
extern void menu_update_display (NmenuPlugin *m);
extern void menu_set_padding (NmenuPlugin *m);
extern void clear_sortorder (NmenuPlugin *m);
extern gboolean menu_control_msg (NmenuPlugin *m, const char *cmd);
extern void menu_destructor (gpointer user_data);

/* End of file */
/*----------------------------------------------------------------------------*/
