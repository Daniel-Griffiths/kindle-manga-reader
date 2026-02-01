#include "app.h"
#include "ui/search_view.h"
#include "ui/manga_view.h"
#include "ui/reader_view.h"
#include "ui/settings_view.h"
#include "ui/widgets.h"
#include "sources/source_registry.h"
#include "util/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static App *g_app = NULL;

App *app_get(void) {
    return g_app;
}

static void app_leave_current_view(App *app) {
    if (app->current_view_type == VIEW_READER) {
        app_save_reading_progress(app);
    }
}

static void app_set_view(App *app, GtkWidget *view, ViewType type) {
    if (app->current_view) {
        gtk_container_remove(GTK_CONTAINER(app->container), app->current_view);
    }
    app->current_view = view;
    app->current_view_type = type;
    gtk_box_pack_start(GTK_BOX(app->container), view, TRUE, TRUE, 0);
    gtk_widget_show_all(app->container);
}

App *app_new(void) {
    App *app = g_new0(App, 1);
    g_app = app;

    app->source = source_registry_get_default();

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window),
                         "L:A_N:application_PC:N_ID:com.manga.reader");
    
    /* On Kindle: fullscreen at native resolution.
     * On Mac/dev: use Kindle screen dimensions (758x1024) so the
     * layout matches what you'll see on the device. */
#ifdef KINDLE
    GdkScreen *screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);
    gtk_window_set_default_size(GTK_WINDOW(app->window), screen_width, screen_height);
#else
    gtk_window_set_default_size(GTK_WINDOW(app->window), 758, 1024);
    gtk_window_set_resizable(GTK_WINDOW(app->window), TRUE);
#endif
    
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    app->container = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(app->window), app->container);

    return app;
}

void app_destroy(App *app) {
    if (!app) return;
    manga_free(app->current_manga);
    g_free(app->current_chapter_url);
    g_free(app);
    if (g_app == app) g_app = NULL;
}

void app_show_search(App *app) {
    app_leave_current_view(app);
    GtkWidget *view = search_view_new();
    app_set_view(app, view, VIEW_SEARCH);
}

void app_show_manga(App *app, const char *manga_url) {
    app_leave_current_view(app);
    GtkWidget *view = manga_view_new(manga_url);
    app_set_view(app, view, VIEW_MANGA);
}

void app_show_reader(App *app, const char *chapter_url) {
    app_leave_current_view(app);
    GtkWidget *view = reader_view_new(chapter_url);
    app_set_view(app, view, VIEW_READER);
}

void app_show_settings(App *app) {
    app_leave_current_view(app);
    GtkWidget *view = settings_view_new();
    app_set_view(app, view, VIEW_SETTINGS);
}

void app_go_back(App *app) {
    switch (app->current_view_type) {
    case VIEW_READER:
        if (app->current_manga && app->current_manga->url) {
            app_show_manga(app, app->current_manga->url);
        } else {
            app_show_search(app);
        }
        break;
    case VIEW_MANGA:
        app_show_search(app);
        break;
    case VIEW_SETTINGS:
        app_show_search(app);
        break;
    case VIEW_SEARCH:
    default:
        break;
    }
}


void app_save_reading_progress(App *app) {
    if (!app->current_manga || !app->current_manga->url ||
        !app->current_chapter_url) {
        return;
    }

    db_save_chapter_progress(app->current_manga->url, app->current_chapter_url, 
                              app->current_page_index, app->current_chapter_total_pages);
}

ReadingProgress *app_load_reading_progress(void) {
    DbProgress *db_prog = db_load_progress();
    if (!db_prog) return NULL;

    ReadingProgress *rp = g_new0(ReadingProgress, 1);
    rp->manga_url = db_prog->manga_url;      /* Transfer ownership */
    rp->chapter_url = db_prog->chapter_url;  /* Transfer ownership */
    rp->page_index = db_prog->page_index;
    g_free(db_prog);  /* Free the wrapper struct only */

    return rp;
}

void reading_progress_free(ReadingProgress *rp) {
    if (!rp) return;
    g_free(rp->manga_url);
    g_free(rp->chapter_url);
    g_free(rp);
}
