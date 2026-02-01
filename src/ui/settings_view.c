#include "settings_view.h"
#include "widgets.h"
#include "../app.h"
#include "../updater.h"
#include "../util/database.h"
#include "../util/cache.h"
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

static void on_reset_confirm_yes(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    /* Close and delete database */
    db_shutdown();
    
    char *db_path = g_build_filename(g_get_user_cache_dir(), 
                                     "manga-reader", 
                                     "manga-reader.db", NULL);
    g_remove(db_path);
    g_free(db_path);
    
    /* Reinitialize database */
    db_init();
    
    /* Return to settings view with success message */
    app_show_settings(app_get());
}

static void on_reset_confirm_cancel(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    app_show_settings(app_get());
}

static void on_reset_database_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    /* Show confirmation view inline */
    GtkWidget *confirm_view = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(confirm_view), 16);
    
    /* Header */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *cancel_btn = widgets_button_new("← Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_reset_confirm_cancel), NULL);
    gtk_box_pack_start(GTK_BOX(header), cancel_btn, FALSE, FALSE, 0);
    
    GtkWidget *title = widgets_label_new("Reset Database?", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(confirm_view), header, FALSE, FALSE, 16);
    
    gtk_box_pack_start(GTK_BOX(confirm_view), widgets_separator_new(), FALSE, FALSE, 12);
    
    /* Warning message */
    GtkWidget *msg = widgets_label_new(
        "This will delete all reading progress, completed chapters, and recently read manga.\n\nThis cannot be undone.",
        EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);
    gtk_label_set_justify(GTK_LABEL(msg), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(confirm_view), msg, TRUE, TRUE, 20);
    
    /* Buttons */
    GtkWidget *btn_box = gtk_vbox_new(FALSE, 12);
    
    GtkWidget *yes_btn = widgets_button_new("Reset Database");
    g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_reset_confirm_yes), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), yes_btn, FALSE, FALSE, 0);
    
    GtkWidget *no_btn = widgets_button_new("Cancel");
    g_signal_connect(no_btn, "clicked", G_CALLBACK(on_reset_confirm_cancel), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), no_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(confirm_view), btn_box, FALSE, FALSE, 20);
    
    App *app = app_get();
    /* Replace current view with confirmation */
    if (app->current_view) {
        gtk_container_remove(GTK_CONTAINER(app->container), app->current_view);
    }
    app->current_view = confirm_view;
    gtk_box_pack_start(GTK_BOX(app->container), confirm_view, TRUE, TRUE, 0);
    gtk_widget_show_all(app->container);
}

static void on_clear_confirm_yes(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    cache_shutdown();
    
    char *cache_dir = g_build_filename(g_get_user_cache_dir(),
                                       "manga-reader", "images", NULL);
    
    /* Simple recursive delete using system command */
    char *cmd = g_strdup_printf("rm -rf '%s'", cache_dir);
    system(cmd);
    g_free(cmd);
    g_free(cache_dir);
    
    char *new_cache = g_build_filename(g_get_user_cache_dir(),
                                       "manga-reader", NULL);
    cache_init(new_cache);
    g_free(new_cache);
    
    /* Return to settings view */
    app_show_settings(app_get());
}

static void on_clear_confirm_cancel(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    app_show_settings(app_get());
}

static void on_clear_cache_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    /* Show confirmation view inline */
    GtkWidget *confirm_view = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(confirm_view), 16);
    
    /* Header */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *cancel_btn = widgets_button_new("← Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_clear_confirm_cancel), NULL);
    gtk_box_pack_start(GTK_BOX(header), cancel_btn, FALSE, FALSE, 0);
    
    GtkWidget *title = widgets_label_new("Clear Image Cache?", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(confirm_view), header, FALSE, FALSE, 16);
    
    gtk_box_pack_start(GTK_BOX(confirm_view), widgets_separator_new(), FALSE, FALSE, 12);
    
    /* Warning message */
    GtkWidget *msg = widgets_label_new(
        "This will delete all cached manga page images.\n\nThey will be re-downloaded when needed.",
        EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);
    gtk_label_set_justify(GTK_LABEL(msg), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(confirm_view), msg, TRUE, TRUE, 20);
    
    /* Buttons */
    GtkWidget *btn_box = gtk_vbox_new(FALSE, 12);
    
    GtkWidget *yes_btn = widgets_button_new("Clear Cache");
    g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_clear_confirm_yes), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), yes_btn, FALSE, FALSE, 0);
    
    GtkWidget *no_btn = widgets_button_new("Cancel");
    g_signal_connect(no_btn, "clicked", G_CALLBACK(on_clear_confirm_cancel), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), no_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(confirm_view), btn_box, FALSE, FALSE, 20);
    
    App *app = app_get();
    /* Replace current view with confirmation */
    if (app->current_view) {
        gtk_container_remove(GTK_CONTAINER(app->container), app->current_view);
    }
    app->current_view = confirm_view;
    gtk_box_pack_start(GTK_BOX(app->container), confirm_view, TRUE, TRUE, 0);
    gtk_widget_show_all(app->container);
}

static void on_layout_list_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    GtkWidget *grid_btn = GTK_WIDGET(user_data);
    db_set_setting("browse_layout", "list");
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NORMAL);
    gtk_button_set_relief(GTK_BUTTON(grid_btn), GTK_RELIEF_NONE);
}

static void on_layout_grid_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    GtkWidget *list_btn = GTK_WIDGET(user_data);
    db_set_setting("browse_layout", "grid");
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NORMAL);
    gtk_button_set_relief(GTK_BUTTON(list_btn), GTK_RELIEF_NONE);
}

static void on_check_update_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    updater_check(app_get(), TRUE);
}

static void on_back_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    app_show_search(app_get());
}

GtkWidget *settings_view_new(void) {
    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);

    /* Header */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *back_btn = widgets_button_new("← Back");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(header), back_btn, FALSE, FALSE, 0);
    
    GtkWidget *title = widgets_label_new("Settings", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 16);

    gtk_box_pack_start(GTK_BOX(vbox), widgets_separator_new(), FALSE, FALSE, 12);

    /* Settings options */
    GtkWidget *options = gtk_vbox_new(FALSE, 8);

    /* Browse Layout */
    GtkWidget *layout_box = gtk_vbox_new(FALSE, 4);
    GtkWidget *layout_label = widgets_label_new("Browse Layout", EINK_FONT_MED_BOLD);
    gtk_misc_set_alignment(GTK_MISC(layout_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(layout_box), layout_label, FALSE, FALSE, 0);

    GtkWidget *layout_desc = widgets_label_new(
        "Choose how manga are displayed on the home screen", EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(layout_desc), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(layout_box), layout_desc, FALSE, FALSE, 0);

    char *current_layout = db_get_setting("browse_layout");
    gboolean is_grid = (current_layout && strcmp(current_layout, "grid") == 0);
    g_free(current_layout);

    GtkWidget *layout_btn_box = gtk_hbox_new(TRUE, 8);
    GtkWidget *list_btn = widgets_button_new("List");
    GtkWidget *grid_btn = widgets_button_new("Grid");
    gtk_button_set_relief(GTK_BUTTON(list_btn),
                          is_grid ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);
    gtk_button_set_relief(GTK_BUTTON(grid_btn),
                          is_grid ? GTK_RELIEF_NORMAL : GTK_RELIEF_NONE);
    g_signal_connect(list_btn, "clicked",
                     G_CALLBACK(on_layout_list_clicked), grid_btn);
    g_signal_connect(grid_btn, "clicked",
                     G_CALLBACK(on_layout_grid_clicked), list_btn);
    gtk_box_pack_start(GTK_BOX(layout_btn_box), list_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layout_btn_box), grid_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layout_box), layout_btn_box, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(options), layout_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(options), widgets_separator_new(), FALSE, FALSE, 8);

    /* Reset Database */
    GtkWidget *reset_box = gtk_vbox_new(FALSE, 4);
    GtkWidget *reset_label = widgets_label_new("Reset Database", EINK_FONT_MED_BOLD);
    gtk_misc_set_alignment(GTK_MISC(reset_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(reset_box), reset_label, FALSE, FALSE, 0);
    
    GtkWidget *reset_desc = widgets_label_new(
        "Delete all reading progress and history", EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(reset_desc), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(reset_box), reset_desc, FALSE, FALSE, 0);
    
    GtkWidget *reset_btn = widgets_button_new("Reset Database");
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_database_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(reset_box), reset_btn, FALSE, FALSE, 4);
    
    gtk_box_pack_start(GTK_BOX(options), reset_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(options), widgets_separator_new(), FALSE, FALSE, 8);

    /* Clear Cache */
    GtkWidget *cache_box = gtk_vbox_new(FALSE, 4);
    GtkWidget *cache_label = widgets_label_new("Clear Image Cache", EINK_FONT_MED_BOLD);
    gtk_misc_set_alignment(GTK_MISC(cache_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(cache_box), cache_label, FALSE, FALSE, 0);
    
    GtkWidget *cache_desc = widgets_label_new(
        "Delete all cached manga page images", EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(cache_desc), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(cache_box), cache_desc, FALSE, FALSE, 0);
    
    GtkWidget *cache_btn = widgets_button_new("Clear Cache");
    g_signal_connect(cache_btn, "clicked", G_CALLBACK(on_clear_cache_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(cache_box), cache_btn, FALSE, FALSE, 4);
    
    gtk_box_pack_start(GTK_BOX(options), cache_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(options), widgets_separator_new(), FALSE, FALSE, 8);

    /* Check for Updates */
    GtkWidget *update_box = gtk_vbox_new(FALSE, 4);
    GtkWidget *update_label = widgets_label_new("Check for Updates", EINK_FONT_MED_BOLD);
    gtk_misc_set_alignment(GTK_MISC(update_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(update_box), update_label, FALSE, FALSE, 0);

    GtkWidget *update_desc = widgets_label_new(
        "Check GitHub for a newer version", EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(update_desc), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(update_box), update_desc, FALSE, FALSE, 0);

    GtkWidget *update_btn = widgets_button_new("Check for Updates");
    g_signal_connect(update_btn, "clicked", G_CALLBACK(on_check_update_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(update_box), update_btn, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(options), update_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), options, FALSE, FALSE, 0);

    return vbox;
}
