#include "search_view.h"
#include "widgets.h"
#include "keyboard.h"
#include "../app.h"
#include "../net/image_loader.h"
#include "../util/database.h"
#include <string.h>

typedef struct {
    GtkWidget *vbox;
    GtkWidget *entry;
    GtkWidget *results_box;
    GtkWidget *status;
    GtkWidget *spinner;
    GtkWidget *scroll;
    GtkWidget *keyboard;
    char      *search_query;
} SearchViewData;

/* Forward declaration */
static void show_favorites(SearchViewData *data);

static void clear_results(SearchViewData *data) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(data->results_box));
    for (GList *l = children; l; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
}

static void on_result_clicked(GtkWidget *button, gpointer user_data) {
    (void)user_data;
    const char *url = g_object_get_data(G_OBJECT(button), "manga-url");
    if (url) {
        app_show_manga(app_get(), url);
    }
}

/* Returns TRUE if grid layout is selected, FALSE for list (default) */
static gboolean use_grid_layout(void) {
    char *val = db_get_setting("browse_layout");
    gboolean grid = (val && strcmp(val, "grid") == 0);
    g_free(val);
    return grid;
}

/* ── Grid layout helpers ──────────────────────────────────────────── */

/* Number of columns in the grid */
#define GRID_COLS 3

/* Create a single grid cell: cover image on top, title underneath */
static GtkWidget *make_grid_cell(const char *cover_url, const char *title,
                                  const char *url) {
    GtkWidget *cell = gtk_vbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(cell), 4);

    /* Cover image */
    if (cover_url) {
        GdkPixbuf *thumb = image_loader_fetch(cover_url, THUMB_GRID_W, THUMB_GRID_H);
        if (thumb) {
            GtkWidget *img = gtk_image_new_from_pixbuf(thumb);
            gtk_box_pack_start(GTK_BOX(cell), img, FALSE, FALSE, 0);
            g_object_unref(thumb);
        }
    }

    /* Title label */
    GtkWidget *label = widgets_label_new(title, EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 16);
    gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
    gtk_box_pack_start(GTK_BOX(cell), label, FALSE, FALSE, 0);

    /* Wrap in a clickable button */
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(btn), cell);
    g_object_set_data_full(G_OBJECT(btn), "manga-url",
                           g_strdup(url), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_result_clicked), NULL);

    return btn;
}

/* Start a new grid row (homogeneous so all columns are equal width) */
static GtkWidget *make_grid_row(void) {
    return gtk_hbox_new(TRUE, 8);
}

/* Pad an incomplete row with invisible placeholders so cells keep their size */
static void pad_grid_row(GtkWidget *row, unsigned int filled) {
    for (unsigned int j = filled; j < GRID_COLS; j++) {
        GtkWidget *spacer = gtk_label_new(NULL);
        gtk_box_pack_start(GTK_BOX(row), spacer, TRUE, TRUE, 0);
    }
}

/* ── List layout helper ────────────────────────────────────────────── */

static GtkWidget *make_list_item(const char *cover_url, const char *title,
                                  const char *url) {
    (void)cover_url;
    GtkWidget *hbox = gtk_hbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

    /* Title */
    GtkWidget *label = widgets_label_new(title, EINK_FONT_MED_BOLD);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 40);
    gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    /* Wrap in a flat button */
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(btn), hbox);
    gtk_widget_set_size_request(btn, -1, TOUCH_MIN_SIZE);
    g_object_set_data_full(G_OBJECT(btn), "manga-url",
                           g_strdup(url), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_result_clicked), NULL);

    return btn;
}

/* ── Spinner helpers ───────────────────────────────────────────────── */

static void show_spinner(SearchViewData *data, const char *text) {
    gtk_widget_hide(data->status);
    if (data->spinner) {
        gtk_widget_destroy(data->spinner);
    }
    data->spinner = widgets_spinner_new(text);
    gtk_box_pack_start(GTK_BOX(data->vbox), data->spinner, FALSE, FALSE, 0);
    /* Place spinner right after the separator (index 3: header, search, sep) */
    gtk_box_reorder_child(GTK_BOX(data->vbox), data->spinner, 3);
    gtk_widget_show(data->spinner);
}

static void hide_spinner(SearchViewData *data) {
    if (data->spinner) {
        gtk_widget_destroy(data->spinner);
        data->spinner = NULL;
    }
}

/* ── Search ───────────────────────────────────────────────────────── */

static void populate_results(SearchViewData *data, MangaList *results) {
    if (results->items->len == 0) {
        gtk_label_set_text(GTK_LABEL(data->status), "No results found.");
        gtk_widget_show(data->status);
    } else {
        gtk_widget_hide(data->status);

        gboolean grid = use_grid_layout();
        unsigned int limit = grid ? 12 : 10;
        unsigned int count = results->items->len < limit
                                 ? results->items->len : limit;

        if (grid) {
            GtkWidget *row = NULL;
            for (unsigned int i = 0; i < count; i++) {
                MangaListItem *item = g_ptr_array_index(results->items, i);
                if (i % GRID_COLS == 0) {
                    row = make_grid_row();
                    gtk_box_pack_start(GTK_BOX(data->results_box), row,
                                       FALSE, FALSE, 4);
                }
                GtkWidget *cell = make_grid_cell(item->cover_url, item->title,
                                                  item->url);
                gtk_box_pack_start(GTK_BOX(row), cell, TRUE, TRUE, 0);
            }
            unsigned int remainder = count % GRID_COLS;
            if (remainder != 0 && row)
                pad_grid_row(row, remainder);
        } else {
            for (unsigned int i = 0; i < count; i++) {
                MangaListItem *item = g_ptr_array_index(results->items, i);
                GtkWidget *btn = make_list_item(item->cover_url, item->title,
                                                 item->url);
                gtk_box_pack_start(GTK_BOX(data->results_box), btn,
                                   FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(data->results_box),
                                   widgets_separator_new(), FALSE, FALSE, 0);
            }
        }
    }

    gtk_widget_show_all(data->results_box);
    manga_list_free(results);
}

typedef struct {
    SearchViewData *view;
    MangaList      *results;
    GtkWidget      *vbox;   /* snapshot to check staleness */
} SearchThreadData;

static gboolean search_done(gpointer user_data) {
    SearchThreadData *td = user_data;

    /* Guard: view may have been replaced while thread was running */
    App *app = app_get();
    if (app->current_view != td->vbox) {
        manga_list_free(td->results);
        g_free(td);
        return FALSE;
    }

    hide_spinner(td->view);
    populate_results(td->view, td->results);
    g_free(td);
    return FALSE;
}

static gpointer search_thread_func(gpointer user_data) {
    SearchThreadData *td = user_data;
    App *app = app_get();
    td->results = app->source->search(app->source, td->view->search_query);
    g_idle_add(search_done, td);
    return NULL;
}

static void do_search(gpointer user_data) {
    SearchViewData *data = user_data;
    const char *query = gtk_entry_get_text(GTK_ENTRY(data->entry));
    if (!query || strlen(query) == 0) return;

    gtk_widget_hide(data->keyboard);
    clear_results(data);
    show_spinner(data, "Searching...");

    /* Save query so the thread can read it (entry might change) */
    g_free(data->search_query);
    data->search_query = g_strdup(query);

    SearchThreadData *td = g_new0(SearchThreadData, 1);
    td->view = data;
    td->vbox = data->vbox;

    g_thread_create(search_thread_func, td, FALSE, NULL);
}

static void show_favorites(SearchViewData *data) {
    clear_results(data);

    GPtrArray *favorites = db_get_favorites();
    if (!favorites || favorites->len == 0) {
        gtk_label_set_text(GTK_LABEL(data->status),
                          "No favorites yet. Search above to get started.");
        gtk_widget_show(data->status);
        if (favorites) g_ptr_array_free(favorites, TRUE);
        return;
    }

    gtk_widget_hide(data->status);

    /* Section heading */
    GtkWidget *heading = widgets_label_new("Favorites", EINK_FONT_MED_BOLD);
    gtk_box_pack_start(GTK_BOX(data->results_box), heading, FALSE, FALSE, 8);

    gboolean grid = use_grid_layout();

    if (grid) {
        GtkWidget *row = NULL;
        for (guint i = 0; i < favorites->len; i++) {
            FavoriteManga *fav = g_ptr_array_index(favorites, i);
            if (i % GRID_COLS == 0) {
                row = make_grid_row();
                gtk_box_pack_start(GTK_BOX(data->results_box), row,
                                   FALSE, FALSE, 4);
            }
            GtkWidget *cell = make_grid_cell(fav->cover_url, fav->manga_title,
                                              fav->manga_url);
            gtk_box_pack_start(GTK_BOX(row), cell, TRUE, TRUE, 0);
        }
        guint remainder = favorites->len % GRID_COLS;
        if (remainder != 0 && row)
            pad_grid_row(row, remainder);
    } else {
        for (guint i = 0; i < favorites->len; i++) {
            FavoriteManga *fav = g_ptr_array_index(favorites, i);
            GtkWidget *btn = make_list_item(fav->cover_url, fav->manga_title,
                                             fav->manga_url);
            gtk_box_pack_start(GTK_BOX(data->results_box), btn,
                               FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(data->results_box),
                               widgets_separator_new(), FALSE, FALSE, 0);
        }
    }

    g_ptr_array_free(favorites, TRUE);
    gtk_widget_show_all(data->results_box);
}

static void on_search_activate(GtkWidget *entry, gpointer user_data) {
    (void)entry;
    do_search(user_data);
}

static gboolean on_entry_clicked(GtkWidget *widget, GdkEventButton *event,
                                  gpointer user_data) {
    (void)widget;
    (void)event;
    SearchViewData *data = user_data;
    
    /* Clear recent manga when user clicks to search */
    clear_results(data);
    gtk_label_set_text(GTK_LABEL(data->status), "Enter a search term above.");
    
    /* Show keyboard */
    gtk_widget_show(data->keyboard);
    return FALSE;
}

static void on_exit_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; (void)user_data;
    gtk_main_quit();
}

static void on_settings_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; (void)user_data;
    app_show_settings(app_get());
}

static void on_data_destroy(gpointer user_data) {
    SearchViewData *data = user_data;
    g_free(data->search_query);
    g_free(data);
}

GtkWidget *search_view_new(void) {
    SearchViewData *data = g_new0(SearchViewData, 1);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    g_object_set_data_full(G_OBJECT(vbox), "view-data", data, on_data_destroy);
    data->vbox = vbox;

    /* Header: title + settings + exit button */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *title = widgets_label_new("Manga Reader", EINK_FONT_LARGE);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    
    GtkWidget *exit_btn = widgets_icon_button_new("✕");
    g_signal_connect(exit_btn, "clicked", G_CALLBACK(on_exit_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(header), exit_btn, FALSE, FALSE, 0);

    GtkWidget *settings_btn = widgets_icon_button_new("⚙");
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(header), settings_btn, FALSE, FALSE, 4);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 16);

    /* Search bar */
    GtkWidget *search_box = gtk_hbox_new(FALSE, 8);
    data->entry = widgets_entry_new();
    g_signal_connect(data->entry, "activate",
                     G_CALLBACK(on_search_activate), data);
    g_signal_connect(data->entry, "button-press-event",
                     G_CALLBACK(on_entry_clicked), data);
    gtk_box_pack_start(GTK_BOX(search_box), data->entry, TRUE, TRUE, 0);

    GtkWidget *search_btn = widgets_button_new("Search");
    g_signal_connect_swapped(search_btn, "clicked",
                             G_CALLBACK(gtk_widget_activate), data->entry);
    gtk_box_pack_start(GTK_BOX(search_box), search_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), search_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), widgets_separator_new(), FALSE, FALSE, 12);

    /* Status label */
    data->status = widgets_status_label_new("Enter a search term above.");
    gtk_box_pack_start(GTK_BOX(vbox), data->status, FALSE, FALSE, 0);

    /* Scrollable results list */
    data->results_box = gtk_vbox_new(FALSE, 0);
    data->scroll = widgets_scrolled_new(data->results_box);
    gtk_box_pack_start(GTK_BOX(vbox), data->scroll, TRUE, TRUE, 0);

    /* On-screen keyboard pinned to bottom (hidden by default) */
    data->keyboard = keyboard_new(GTK_ENTRY(data->entry),
                                  G_CALLBACK(do_search), data);
    gtk_widget_set_no_show_all(data->keyboard, TRUE);
    gtk_box_pack_end(GTK_BOX(vbox), data->keyboard, FALSE, FALSE, 0);

    /* Show favorited manga on startup */
    show_favorites(data);

    return vbox;
}
