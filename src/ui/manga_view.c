#include "manga_view.h"
#include "widgets.h"
#include "../app.h"
#include "../net/image_loader.h"
#include "../util/database.h"
#include <string.h>

typedef struct {
    char      *manga_url;
    GtkWidget *vbox;
    GtkWidget *loading;
    GtkWidget *fav_btn;
} MangaViewData;

typedef struct {
    char *manga_url;
    char *chapter_url;
    int chapter_index;
    int saved_page_index;
} ResumeData;

static void on_resume_yes(GtkWidget *button, gpointer user_data) {
    (void)button;
    ResumeData *data = user_data;
    App *app = app_get();
    app->current_chapter_index = data->chapter_index;
    app->current_page_index = data->saved_page_index;
    app_show_reader(app, data->chapter_url);
    g_free(data->manga_url);
    g_free(data->chapter_url);
    g_free(data);
}

static void on_resume_no(GtkWidget *button, gpointer user_data) {
    (void)button;
    ResumeData *data = user_data;
    App *app = app_get();
    app->current_chapter_index = data->chapter_index;
    app->current_page_index = 0;  /* Start from beginning */
    app_show_reader(app, data->chapter_url);
    g_free(data->manga_url);
    g_free(data->chapter_url);
    g_free(data);
}

static void on_resume_cancel(GtkWidget *button, gpointer user_data) {
    (void)button;
    ResumeData *data = user_data;
    /* Just go back to manga view */
    app_show_manga(app_get(), data->manga_url);
    g_free(data->manga_url);
    g_free(data->chapter_url);
    g_free(data);
}

static void on_chapter_clicked(GtkWidget *button, gpointer user_data) {
    const char *manga_url = user_data;
    const char *chapter_url = g_object_get_data(G_OBJECT(button), "chapter-url");
    int chapter_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "chapter-index"));

    if (!chapter_url) return;

    App *app = app_get();
    app->current_chapter_index = chapter_index;

    /* Check if there's saved progress for this chapter */
    int saved_page = db_get_chapter_progress(manga_url, chapter_url);

    if (saved_page > 0) {
        /* Show resume confirmation inline */
        ResumeData *resume_data = g_new0(ResumeData, 1);
        resume_data->manga_url = g_strdup(manga_url);
        resume_data->chapter_url = g_strdup(chapter_url);
        resume_data->chapter_index = chapter_index;
        resume_data->saved_page_index = saved_page;

        GtkWidget *confirm_view = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(confirm_view), 16);

        /* Header with back button */
        GtkWidget *header = gtk_hbox_new(FALSE, 0);
        GtkWidget *cancel_btn = widgets_button_new("← Cancel");
        g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_resume_cancel), resume_data);
        gtk_box_pack_start(GTK_BOX(header), cancel_btn, FALSE, FALSE, 0);

        GtkWidget *title = widgets_label_new("Resume Reading?", EINK_FONT_LARGE);
        gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(confirm_view), header, FALSE, FALSE, 16);

        gtk_box_pack_start(GTK_BOX(confirm_view), widgets_separator_new(), FALSE, FALSE, 12);

        /* Message */
        char *msg = g_strdup_printf("Resume from page %d?", saved_page + 1);
        GtkWidget *label = widgets_label_new(msg, EINK_FONT_MED);
        g_free(msg);
        gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(confirm_view), label, TRUE, TRUE, 20);

        /* Buttons */
        GtkWidget *btn_box = gtk_vbox_new(FALSE, 12);

        GtkWidget *yes_btn = widgets_button_new("Resume");
        g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_resume_yes), resume_data);
        gtk_box_pack_start(GTK_BOX(btn_box), yes_btn, FALSE, FALSE, 0);

        GtkWidget *no_btn = widgets_button_new("Start Over");
        g_signal_connect(no_btn, "clicked", G_CALLBACK(on_resume_no), resume_data);
        gtk_box_pack_start(GTK_BOX(btn_box), no_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(confirm_view), btn_box, FALSE, FALSE, 20);

        /* Replace current view with confirmation */
        if (app->current_view) {
            gtk_container_remove(GTK_CONTAINER(app->container), app->current_view);
        }
        app->current_view = confirm_view;
        gtk_box_pack_start(GTK_BOX(app->container), confirm_view, TRUE, TRUE, 0);
        gtk_widget_show_all(app->container);
    } else {
        /* No progress, start from beginning */
        app->current_page_index = 0;
        app_show_reader(app, chapter_url);
    }
}

static void on_back_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    app_go_back(app_get());
}

static void on_favorite_clicked(GtkWidget *button, gpointer user_data) {
    MangaViewData *data = user_data;
    App *app = app_get();

    if (db_is_favorite(data->manga_url)) {
        db_remove_favorite(data->manga_url);
        gtk_button_set_label(GTK_BUTTON(button), "★");
    } else {
        const char *title = app->current_manga ? app->current_manga->title : "";
        const char *cover = app->current_manga ? app->current_manga->cover_url : NULL;
        db_add_favorite(data->manga_url, title, cover);
        gtk_button_set_label(GTK_BUTTON(button), "☆");
    }
}

static void on_data_destroy(gpointer user_data) {
    MangaViewData *data = user_data;
    g_free(data->manga_url);
    g_free(data);
}

typedef struct {
    MangaViewData *view;
    Manga         *manga;
    GtkWidget     *vbox;  /* snapshot to check staleness */
} MangaLoadThread;

/* Build the UI on the main thread once data is ready */
static gboolean manga_view_populate(gpointer user_data) {
    MangaLoadThread *td = user_data;
    MangaViewData *data = td->view;
    GtkWidget *vbox = td->vbox;

    /* Guard: view may have been replaced while thread was running */
    App *app = app_get();
    if (app->current_view != vbox) {
        if (td->manga) manga_free(td->manga);
        g_free(td);
        return FALSE;
    }

    /* Remove loading spinner */
    if (data->loading) {
        gtk_widget_destroy(data->loading);
        data->loading = NULL;
    }

    if (!td->manga) {
        GtkWidget *err = widgets_status_label_new("Failed to load manga.");
        gtk_box_pack_start(GTK_BOX(vbox), err, FALSE, FALSE, 0);
        gtk_widget_show_all(vbox);
        g_free(td);
        return FALSE;
    }

    Manga *manga = td->manga;

    /* Store manga in app state */
    if (app->current_manga) manga_free(app->current_manga);
    app->current_manga = manga;

    /* Update favorite button */
    gboolean is_fav = db_is_favorite(data->manga_url);
    gtk_button_set_label(GTK_BUTTON(data->fav_btn), is_fav ? "☆" : "★");

    /* Header area: cover + info side by side */
    GtkWidget *header = gtk_hbox_new(FALSE, 16);

    if (manga->cover_url) {
        GdkPixbuf *cover = image_loader_fetch(manga->cover_url, THUMB_COVER_W, THUMB_COVER_H);
        if (cover) {
            GtkWidget *img = gtk_image_new_from_pixbuf(cover);
            gtk_box_pack_start(GTK_BOX(header), img, FALSE, FALSE, 0);
            g_object_unref(cover);
        }
    }

    GtkWidget *info_box = gtk_vbox_new(FALSE, 6);

    if (manga->title) {
        GtkWidget *title = widgets_label_new(manga->title, EINK_FONT_LARGE);
        gtk_box_pack_start(GTK_BOX(info_box), title, FALSE, FALSE, 0);
    }
    if (manga->author) {
        char *author_text = g_strdup_printf("Author: %s", manga->author);
        GtkWidget *author = widgets_label_new(author_text, EINK_FONT_SMALL);
        gtk_box_pack_start(GTK_BOX(info_box), author, FALSE, FALSE, 0);
        g_free(author_text);
    }
    if (manga->status) {
        char *status_text = g_strdup_printf("Status: %s", manga->status);
        GtkWidget *status = widgets_label_new(status_text, EINK_FONT_SMALL);
        gtk_box_pack_start(GTK_BOX(info_box), status, FALSE, FALSE, 0);
        g_free(status_text);
    }
    gtk_box_pack_start(GTK_BOX(header), info_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 8);

    /* Chapter list */
    gtk_box_pack_start(GTK_BOX(vbox), widgets_separator_new(), FALSE, FALSE, 12);

    char *ch_header = g_strdup_printf("Chapters (%d)", manga->chapters->len);
    GtkWidget *ch_label = widgets_label_new(ch_header, EINK_FONT_MED_BOLD);
    gtk_box_pack_start(GTK_BOX(vbox), ch_label, FALSE, FALSE, 4);
    g_free(ch_header);

    GtkWidget *ch_box = gtk_vbox_new(FALSE, 0);
    for (unsigned int i = 0; i < manga->chapters->len; i++) {
        Chapter *ch = g_ptr_array_index(manga->chapters, i);

        /* Check if chapter is completed */
        gboolean completed = db_is_chapter_completed(data->manga_url, ch->url);

        /* Get progress for in-progress chapters */
        int saved_page = db_get_chapter_progress(data->manga_url, ch->url);
        int total_pages = db_get_chapter_total_pages(data->manga_url, ch->url);

        /* Build label with progress indicator */
        char *title_text = ch->title ? ch->title : "Chapter";
        char *label_text;

        if (completed) {
            label_text = g_strdup_printf("✓ %s", title_text);
        } else if (saved_page > 0 && total_pages > 0) {
            int percent = (int)((saved_page + 1) * 100.0 / total_pages);
            label_text = g_strdup_printf("%s (%d%%)", title_text, percent);
        } else {
            label_text = g_strdup(title_text);
        }

        GtkWidget *ch_item = gtk_hbox_new(FALSE, 8);
        gtk_container_set_border_width(GTK_CONTAINER(ch_item), 8);

        GtkWidget *ch_item_label = widgets_label_new(label_text, EINK_FONT_MED);
        gtk_label_set_ellipsize(GTK_LABEL(ch_item_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_line_wrap(GTK_LABEL(ch_item_label), FALSE);
        gtk_box_pack_start(GTK_BOX(ch_item), ch_item_label, TRUE, TRUE, 0);
        g_free(label_text);

        GtkWidget *btn = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_container_add(GTK_CONTAINER(btn), ch_item);
        gtk_widget_set_size_request(btn, -1, TOUCH_MIN_SIZE);

        g_object_set_data_full(G_OBJECT(btn), "chapter-url",
                               g_strdup(ch->url), g_free);
        g_object_set_data(G_OBJECT(btn), "chapter-index",
                          GINT_TO_POINTER((int)i));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_chapter_clicked),
                         data->manga_url);
        gtk_box_pack_start(GTK_BOX(ch_box), btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(ch_box), widgets_separator_new(),
                           FALSE, FALSE, 0);
    }

    GtkWidget *ch_scroll = widgets_scrolled_new(ch_box);
    gtk_box_pack_start(GTK_BOX(vbox), ch_scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(vbox);
    g_free(td);
    return FALSE;
}

/* Background thread: fetch manga details */
static gpointer manga_load_thread_func(gpointer user_data) {
    MangaLoadThread *td = user_data;
    App *app = app_get();
    td->manga = app->source->get_manga_details(app->source,
                                                 td->view->manga_url);
    g_idle_add(manga_view_populate, td);
    return NULL;
}

GtkWidget *manga_view_new(const char *manga_url) {
    MangaViewData *data = g_new0(MangaViewData, 1);
    data->manga_url = g_strdup(manga_url);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    g_object_set_data_full(G_OBJECT(vbox), "view-data", data, on_data_destroy);
    data->vbox = vbox;

    /* Header: back button + favorite button */
    GtkWidget *header_bar = gtk_hbox_new(FALSE, 0);

    GtkWidget *back_btn = widgets_button_new("← Back");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(header_bar), back_btn, FALSE, FALSE, 0);

    gboolean is_fav = db_is_favorite(manga_url);
    data->fav_btn = widgets_icon_button_new(is_fav ? "☆" : "★");
    g_signal_connect(data->fav_btn, "clicked", G_CALLBACK(on_favorite_clicked), data);
    gtk_box_pack_end(GTK_BOX(header_bar), data->fav_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), header_bar, FALSE, FALSE, 16);

    /* Animated loading spinner — visible immediately */
    data->loading = widgets_spinner_new("Loading...");
    gtk_box_pack_start(GTK_BOX(vbox), data->loading, FALSE, FALSE, 0);

    /* Fetch manga details on a background thread */
    MangaLoadThread *td = g_new0(MangaLoadThread, 1);
    td->view = data;
    td->vbox = vbox;
    g_thread_create(manga_load_thread_func, td, FALSE, NULL);

    return vbox;
}
