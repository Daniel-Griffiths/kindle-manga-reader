#include "reader_view.h"
#include "widgets.h"
#include "../app.h"
#include "../device/brightness.h"
#include "../net/image_loader.h"
#include "../net/http.h"
#include "../util/cache.h"
#include "../util/database.h"
#include <string.h>

/* ── Types ─────────────────────────────────────────────────────────── */

typedef enum { FIT_SCREEN, FIT_WIDTH, FIT_HEIGHT } FitMode;

typedef struct {
    char      *chapter_url;
    PageList  *pages;
    int        current_page;
    int        chapter_index;
    int        display_width;
    int        display_height;
    FitMode    fit_mode;
    int        rotation;       /* 0, 90, 180, 270 — resets per page */
    gboolean   toolbar_visible;

    /* Widgets */
    GtkWidget *vbox;
    GtkWidget *image_widget;
    GtkWidget *event_box;
    GtkWidget *scrolled_window;
    GtkWidget *top_bar;
    GtkWidget *bottom_bar;
    GtkWidget *page_label;
    GtkWidget *page_slider;
    GtkWidget *rotate_button;
    GtkWidget *loading_overlay;
    guint      spinner_tick_id;
    guint      page_wait_tick_id;

    /* Bulk prefetch */
    GThread   *prefetch_thread;
    gboolean   prefetch_cancel;
    gint       prefetch_done;     /* pages cached so far (atomic) */
    int        prefetch_total;
    gboolean   reading_started;   /* first page shown, user can read */

    gboolean   slider_updating;
    gboolean   destroyed;
} ReaderViewData;

/* ── Forward declarations ──────────────────────────────────────────── */

static void reader_show_page(ReaderViewData *data);
static void toggle_toolbar(ReaderViewData *data);

/* ── Loading overlay ───────────────────────────────────────────────── */

static gboolean update_download_progress(gpointer user_data) {
    ReaderViewData *data = user_data;
    if (data->destroyed || !data->loading_overlay) return FALSE;

    if (data->prefetch_total > 0) {
        char *text = g_strdup_printf("Downloading %d / %d...",
                                      g_atomic_int_get(&data->prefetch_done),
                                      data->prefetch_total);
        widgets_spinner_set_text(data->loading_overlay, text);
        g_free(text);
    }
    return TRUE;
}

static void show_loading(ReaderViewData *data) {
    if (data->loading_overlay)
        gtk_widget_show(data->loading_overlay);
    if (data->spinner_tick_id == 0)
        data->spinner_tick_id = g_timeout_add(200, update_download_progress, data);
}

static void hide_loading(ReaderViewData *data) {
    if (data->loading_overlay)
        gtk_widget_hide(data->loading_overlay);
    if (data->spinner_tick_id) {
        g_source_remove(data->spinner_tick_id);
        data->spinner_tick_id = 0;
    }
}

/* ── Toolbar toggle ────────────────────────────────────────────────── */

static void toggle_toolbar(ReaderViewData *data) {
    data->toolbar_visible = !data->toolbar_visible;
    if (data->toolbar_visible) {
        gtk_widget_show(data->top_bar);
        gtk_widget_show(data->bottom_bar);
    } else {
        gtk_widget_hide(data->top_bar);
        gtk_widget_hide(data->bottom_bar);
    }
}

/* ── Navigation helpers ────────────────────────────────────────────── */

typedef struct {
    int new_chapter_index;
    int start_page;   /* 0 = first page, -1 = last page */
    char *chapter_url;
} ChapterAdvanceData;

/* ── Inline prompt (replaces popup dialogs) ────────────────────────── */

static GtkWidget *inline_prompt = NULL;

static void dismiss_prompt(void) {
    if (inline_prompt) {
        gtk_widget_destroy(inline_prompt);
        inline_prompt = NULL;
    }
}

static void on_advance_yes(GtkWidget *button, gpointer user_data) {
    (void)button;
    dismiss_prompt();
    ChapterAdvanceData *ad = user_data;
    App *app = app_get();
    app->current_chapter_index = ad->new_chapter_index;
    app->current_page_index = ad->start_page;
    app_show_reader(app, ad->chapter_url);
    g_free(ad->chapter_url);
    g_free(ad);
}

static void on_advance_no(GtkWidget *button, gpointer user_data) {
    (void)button;
    dismiss_prompt();
    ChapterAdvanceData *ad = user_data;
    g_free(ad->chapter_url);
    g_free(ad);
}

static void on_finished_exit(GtkWidget *button, gpointer user_data) {
    (void)button; (void)user_data;
    dismiss_prompt();
    app_go_back(app_get());
}

static void on_finished_stay(GtkWidget *button, gpointer user_data) {
    (void)button; (void)user_data;
    dismiss_prompt();
}

static void on_error_dismiss(GtkWidget *button, gpointer user_data) {
    (void)button; (void)user_data;
    dismiss_prompt();
}

static void show_error_prompt(ReaderViewData *data, const char *message) {
    GtkWidget *ok_btn = widgets_button_new("OK");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_error_dismiss), NULL);
    
    /* Create a container with just one button centered */
    GtkWidget *btn_box = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), ok_btn, TRUE, TRUE, 0);
    
    dismiss_prompt();
    
    GtkWidget *frame = gtk_vbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 16);

    GtkWidget *sep_top = widgets_separator_new();
    gtk_box_pack_start(GTK_BOX(frame), sep_top, FALSE, FALSE, 0);

    GtkWidget *label = widgets_label_new(message, EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(frame), label, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(frame), btn_box, FALSE, FALSE, 4);

    GtkWidget *sep_bot = widgets_separator_new();
    gtk_box_pack_start(GTK_BOX(frame), sep_bot, FALSE, FALSE, 0);

    /* Insert before the bottom bar */
    gtk_box_pack_end(GTK_BOX(data->vbox), frame, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(data->vbox), frame, 2);
    gtk_widget_show_all(frame);

    inline_prompt = frame;
}

static void show_inline_prompt(ReaderViewData *data, const char *message,
                                GtkWidget *btn1, GtkWidget *btn2) {
    dismiss_prompt();

    GtkWidget *frame = gtk_vbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 16);

    GtkWidget *sep_top = widgets_separator_new();
    gtk_box_pack_start(GTK_BOX(frame), sep_top, FALSE, FALSE, 0);

    GtkWidget *label = widgets_label_new(message, EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(frame), label, FALSE, FALSE, 4);

    GtkWidget *btn_box = gtk_hbox_new(TRUE, 16);
    gtk_box_pack_start(GTK_BOX(btn_box), btn1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), btn2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(frame), btn_box, FALSE, FALSE, 4);

    GtkWidget *sep_bot = widgets_separator_new();
    gtk_box_pack_start(GTK_BOX(frame), sep_bot, FALSE, FALSE, 0);

    /* Insert before the bottom bar */
    gtk_box_pack_end(GTK_BOX(data->vbox), frame, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(data->vbox), frame, 2);
    gtk_widget_show_all(frame);

    inline_prompt = frame;
}

static void show_finished_prompt(ReaderViewData *data, const char *message) {
    GtkWidget *exit_btn = widgets_button_new("Back");
    GtkWidget *stay_btn = widgets_button_new("Stay");

    g_signal_connect(exit_btn, "clicked", G_CALLBACK(on_finished_exit), NULL);
    g_signal_connect(stay_btn, "clicked", G_CALLBACK(on_finished_stay), NULL);

    show_inline_prompt(data, message, exit_btn, stay_btn);
}

static void reader_advance_chapter(ReaderViewData *data, int direction) {
    App *app = app_get();
    if (!app->current_manga || !app->current_manga->chapters) return;

    /*
     * Chapter array is ordered newest-first (ch 226 at index 0, ch 1 at end).
     * "Next" in reading order means a higher chapter number = lower index,
     * so we subtract direction instead of adding it.
     */
    int new_idx = data->chapter_index - direction;
    GPtrArray *chapters = app->current_manga->chapters;

    if (new_idx < 0 || new_idx >= (int)chapters->len) {
        if (direction > 0)
            show_finished_prompt(data, "You've reached the last chapter.");
        else
            show_finished_prompt(data, "You're at the first chapter.");
        return;
    }

    Chapter *ch = g_ptr_array_index(chapters, new_idx);

    ChapterAdvanceData *ad = g_new0(ChapterAdvanceData, 1);
    ad->new_chapter_index = new_idx;
    ad->start_page = (direction < 0) ? -1 : 0;  /* backward = last page */
    ad->chapter_url = g_strdup(ch->url);

    const char *dir_label = (direction > 0) ? "next" : "previous";
    char *msg = g_strdup_printf("Continue to %s chapter?\n%s",
                                dir_label,
                                ch->title ? ch->title : "");

    GtkWidget *yes_btn = widgets_button_new("Yes");
    GtkWidget *no_btn = widgets_button_new("No");

    g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_advance_yes), ad);
    g_signal_connect(no_btn, "clicked", G_CALLBACK(on_advance_no), ad);

    show_inline_prompt(data, msg, no_btn, yes_btn);
    g_free(msg);
}

static void reader_go_next(ReaderViewData *data) {
    if (!data->pages || !data->reading_started) return;
    int total = (int)data->pages->image_urls->len;
    if (data->current_page < total - 1) {
        data->current_page++;
        data->rotation = 0;
        reader_show_page(data);
    } else {
        reader_advance_chapter(data, +1);
    }
}

static void reader_go_prev(ReaderViewData *data) {
    if (!data->pages || !data->reading_started) return;
    if (data->current_page > 0) {
        data->current_page--;
        data->rotation = 0;
        reader_show_page(data);
    } else {
        reader_advance_chapter(data, -1);
    }
}

/* ── Tap handler ───────────────────────────────────────────────────── */

static gboolean on_image_button_press(GtkWidget *widget, GdkEventButton *event,
                                       gpointer user_data) {
    (void)widget;
    ReaderViewData *data = user_data;
    if (event->type != GDK_BUTTON_PRESS) return FALSE;

    int alloc_width = widget->allocation.width;
    double third = alloc_width / 3.0;

    if (event->x >= third && event->x <= third * 2.0) {
        toggle_toolbar(data);
    } else if (event->x < third) {
        reader_go_next(data);
    } else {
        reader_go_prev(data);
    }
    return TRUE;
}

/* ── Toolbar button callbacks ──────────────────────────────────────── */

static void on_back_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; (void)user_data;
    app_go_back(app_get());
}

static void on_rotate_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    ReaderViewData *data = user_data;
    data->rotation = data->rotation ? 0 : 90;
    reader_show_page(data);
}

static void on_brightness_up(GtkWidget *button, gpointer user_data) {
    (void)button;
    ReaderViewData *data = user_data;
    if (!brightness_increase()) {
        const char *error = brightness_get_error();
        if (error) {
            show_error_prompt(data, error);
        }
    }
}

static void on_brightness_down(GtkWidget *button, gpointer user_data) {
    (void)button;
    ReaderViewData *data = user_data;
    if (!brightness_decrease()) {
        const char *error = brightness_get_error();
        if (error) {
            show_error_prompt(data, error);
        }
    }
}

static void on_brightness_off(GtkWidget *button, gpointer user_data) {
    (void)button;
    ReaderViewData *data = user_data;
    if (!brightness_off()) {
        const char *error = brightness_get_error();
        if (error) {
            show_error_prompt(data, error);
        }
    }
}

/* ── Page slider ───────────────────────────────────────────────────── */

static void on_slider_changed(GtkRange *range, gpointer user_data) {
    ReaderViewData *data = user_data;
    if (data->slider_updating) return;

    int val = (int)gtk_range_get_value(range) - 1;
    if (val < 0) val = 0;
    if (data->pages && val >= (int)data->pages->image_urls->len)
        val = (int)data->pages->image_urls->len - 1;

    if (val != data->current_page) {
        data->current_page = val;
        data->rotation = 0;
        reader_show_page(data);
    }
}

static void update_slider(ReaderViewData *data) {
    if (!data->pages) return;
    data->slider_updating = TRUE;
    gtk_range_set_range(GTK_RANGE(data->page_slider), 1,
                        (double)data->pages->image_urls->len);
    gtk_range_set_value(GTK_RANGE(data->page_slider),
                        data->current_page + 1);
    data->slider_updating = FALSE;
}

/* ── Page display ──────────────────────────────────────────────────── */

/* Poll timer: waits for the prefetch thread to cache the current page */
static gboolean wait_for_page_tick(gpointer user_data) {
    ReaderViewData *data = user_data;
    if (data->destroyed) {
        data->page_wait_tick_id = 0;
        return FALSE;
    }

    const char *url = g_ptr_array_index(data->pages->image_urls,
                                         data->current_page);
    char *key = cache_key_from_url(url);
    gboolean ready = cache_has(key);
    g_free(key);

    if (ready) {
        data->page_wait_tick_id = 0;
        hide_loading(data);
        reader_show_page(data);
        return FALSE;  /* stop polling */
    }
    return TRUE;  /* keep polling */
}

static void reader_show_page(ReaderViewData *data) {
    if (!data->pages || data->pages->image_urls->len == 0) return;

    const char *url = g_ptr_array_index(data->pages->image_urls,
                                         data->current_page);

    /* Check if this page is cached yet */
    char *key = cache_key_from_url(url);
    gboolean cached = cache_has(key);
    g_free(key);

    if (!cached) {
        /* Show spinner and poll until the prefetch thread caches it */
        gtk_image_clear(GTK_IMAGE(data->image_widget));
        show_loading(data);
        /* Update label/slider even while waiting */
        char *text = g_strdup_printf("Page %d / %d",
                                      data->current_page + 1,
                                      (int)data->pages->image_urls->len);
        gtk_label_set_text(GTK_LABEL(data->page_label), text);
        g_free(text);
        update_slider(data);
        /* Cancel any existing poll before starting a new one */
        if (data->page_wait_tick_id)
            g_source_remove(data->page_wait_tick_id);
        data->page_wait_tick_id = g_timeout_add(200, wait_for_page_tick, data);
        return;
    }

    hide_loading(data);

    int max_w = data->display_width;
    int max_h = data->display_height;
    switch (data->fit_mode) {
    case FIT_SCREEN: break;
    case FIT_WIDTH:  max_h = 0; break;
    case FIT_HEIGHT: max_w = 0; break;
    }

    GdkPixbuf *pb = image_loader_fetch_processed(url, max_w, max_h, TRUE);
    if (pb) {
        if (data->rotation) {
            GdkPixbuf *rotated = gdk_pixbuf_rotate_simple(pb,
                                     GDK_PIXBUF_ROTATE_CLOCKWISE);
            g_object_unref(pb);
            pb = rotated;
        }
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->image_widget), pb);
        g_object_unref(pb);
    } else {
        gtk_image_clear(GTK_IMAGE(data->image_widget));
    }

    /* Update label + slider */
    char *text = g_strdup_printf("Page %d / %d",
                                  data->current_page + 1,
                                  (int)data->pages->image_urls->len);
    gtk_label_set_text(GTK_LABEL(data->page_label), text);
    g_free(text);
    update_slider(data);

    App *app = app_get();
    app->current_page_index = data->current_page;

    /* Mark chapter as completed if we've reached the last page */
    if (data->current_page == (int)data->pages->image_urls->len - 1) {
        if (app->current_manga && app->current_manga->url && data->chapter_url) {
            db_mark_chapter_completed(app->current_manga->url, data->chapter_url);
        }
    }

    /* Scroll to top (for FIT_WIDTH) */
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(data->scrolled_window));
    gtk_adjustment_set_value(vadj, 0);
}

/* ── Bulk prefetch: download all pages to disk cache ───────────────── */

/* Called on main thread once we have the page list — lets the user start reading */
static gboolean prefetch_pages_ready(gpointer user_data) {
    ReaderViewData *data = user_data;
    if (data->destroyed) return FALSE;

    if (!data->pages || data->pages->image_urls->len == 0) {
        hide_loading(data);
        gtk_label_set_text(GTK_LABEL(data->page_label), "No pages found.");
        return FALSE;
    }

    /* Store total pages count in app for progress tracking */
    App *app = app_get();
    app->current_chapter_total_pages = (int)data->pages->image_urls->len;

    /* Resolve -1 sentinel to last page (used when navigating backward) */
    if (data->current_page < 0)
        data->current_page = (int)data->pages->image_urls->len - 1;
    if (data->current_page >= (int)data->pages->image_urls->len)
        data->current_page = 0;

    data->reading_started = TRUE;
    update_slider(data);
    reader_show_page(data);  /* will show spinner if page not cached yet */
    return FALSE;
}

#define PREFETCH_CONCURRENT 4

typedef struct {
    ReaderViewData *reader;
    const char     *url;
} PageDownloadTask;

static void page_download_worker(gpointer task_data, gpointer user_data) {
    PageDownloadTask *task = task_data;
    ReaderViewData *data = user_data;

    if (data->prefetch_cancel || data->destroyed) {
        g_free(task);
        return;
    }

    char *key = cache_key_from_url(task->url);

    if (!cache_has(key)) {
        HttpResponse *resp = http_get(task->url);
        /* Only cache if we haven't been cancelled while downloading */
        if (!data->prefetch_cancel && !data->destroyed &&
            resp && resp->status_code == 200 && resp->data) {
            cache_put(key, resp->data, resp->size);
        }
        http_response_free(resp);
    }

    g_free(key);
    g_atomic_int_inc(&data->prefetch_done);
    g_free(task);
}

static gpointer prefetch_thread_func(gpointer user_data) {
    ReaderViewData *data = user_data;

    /* Step 1: fetch page list */
    App *app = app_get();
    PageList *pages = app->source->get_chapter_pages(app->source,
                                                      data->chapter_url);
    if (data->prefetch_cancel || data->destroyed) {
        page_list_free(pages);
        return NULL;
    }

    data->pages = pages;
    if (!pages || pages->image_urls->len == 0) {
        g_idle_add(prefetch_pages_ready, data);
        return NULL;
    }

    data->prefetch_total = (int)pages->image_urls->len;

    /* Step 2: let the user start reading immediately */
    g_idle_add(prefetch_pages_ready, data);

    /* Step 3: download pages concurrently via thread pool */
    GThreadPool *pool = g_thread_pool_new(page_download_worker, data,
                                           PREFETCH_CONCURRENT, FALSE, NULL);

    for (guint i = 0; i < pages->image_urls->len; i++) {
        if (data->prefetch_cancel || data->destroyed) break;

        PageDownloadTask *task = g_new(PageDownloadTask, 1);
        task->reader = data;
        task->url = g_ptr_array_index(pages->image_urls, i);
        g_thread_pool_push(pool, task, NULL);
    }

    /* Wait for in-flight workers; discard queued ones if cancelled */
    g_thread_pool_free(pool, data->prefetch_cancel, TRUE);

    return NULL;
}

/* ── Cleanup ───────────────────────────────────────────────────────── */

/* Background thread that joins the prefetch thread and frees the data,
 * so the main thread never blocks waiting for in-flight downloads. */
static gpointer cleanup_thread_func(gpointer user_data) {
    ReaderViewData *data = user_data;
    if (data->prefetch_thread) {
        g_thread_join(data->prefetch_thread);
        data->prefetch_thread = NULL;
    }
    g_free(data->chapter_url);
    page_list_free(data->pages);
    g_free(data);
    return NULL;
}

static void on_data_destroy(gpointer user_data) {
    ReaderViewData *data = user_data;
    data->destroyed = TRUE;
    data->prefetch_cancel = TRUE;

    /* Remove all pending timers */
    if (data->spinner_tick_id) {
        g_source_remove(data->spinner_tick_id);
        data->spinner_tick_id = 0;
    }
    if (data->page_wait_tick_id) {
        g_source_remove(data->page_wait_tick_id);
        data->page_wait_tick_id = 0;
    }

    /* Remove any pending idle callbacks that reference this data */
    while (g_idle_remove_by_data(data)) { /* drain all */ }

    /* Join the prefetch thread and free data in the background
     * so we don't block the UI waiting for in-flight downloads. */
    if (data->prefetch_thread) {
        g_thread_create(cleanup_thread_func, data, FALSE, NULL);
    } else {
        g_free(data->chapter_url);
        page_list_free(data->pages);
        g_free(data);
    }
}

/* ── Public constructor ────────────────────────────────────────────── */

GtkWidget *reader_view_new(const char *chapter_url) {
    ReaderViewData *data = g_new0(ReaderViewData, 1);
    data->chapter_url = g_strdup(chapter_url);
    data->current_page = 0;
    
    /* Get actual screen dimensions dynamically */
    GdkScreen *screen = gdk_screen_get_default();
    data->display_width = gdk_screen_get_width(screen);
    data->display_height = gdk_screen_get_height(screen);
    
    data->fit_mode = FIT_SCREEN;
    data->rotation = 0;
    data->toolbar_visible = TRUE;
    data->slider_updating = FALSE;
    data->destroyed = FALSE;

    App *app = app_get();
    data->chapter_index = app->current_chapter_index;

    /* Restore saved page index (or -1 = last page, resolved after prefetch) */
    if (app->current_page_index != 0) {
        data->current_page = app->current_page_index;
    }
    g_free(app->current_chapter_url);
    app->current_chapter_url = g_strdup(chapter_url);

    /* ── Build widget tree ──────────────────────────────────────── */

    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    data->vbox = vbox;
    g_object_set_data_full(G_OBJECT(vbox), "view-data", data, on_data_destroy);

    /* == Top bar ================================================ */
    data->top_bar = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(data->top_bar), 4);

    GtkWidget *back_btn = widgets_button_new("← Back");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(data->top_bar), back_btn, FALSE, FALSE, 0);

    data->page_label = widgets_label_new("Loading...", EINK_FONT_SMALL);
    gtk_misc_set_alignment(GTK_MISC(data->page_label), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(data->top_bar), data->page_label,
                       TRUE, TRUE, 0);

    data->rotate_button = widgets_icon_button_new("⟳");
    g_signal_connect(data->rotate_button, "clicked",
                     G_CALLBACK(on_rotate_clicked), data);
    gtk_box_pack_start(GTK_BOX(data->top_bar), data->rotate_button,
                       FALSE, FALSE, 0);

    /* Brightness control buttons */
    GtkWidget *bright_up = widgets_icon_button_new("☀+");
    g_signal_connect(bright_up, "clicked", G_CALLBACK(on_brightness_up), data);
    gtk_box_pack_start(GTK_BOX(data->top_bar), bright_up, FALSE, FALSE, 0);

    GtkWidget *bright_down = widgets_icon_button_new("☀-");
    g_signal_connect(bright_down, "clicked", G_CALLBACK(on_brightness_down), data);
    gtk_box_pack_start(GTK_BOX(data->top_bar), bright_down, FALSE, FALSE, 0);

    GtkWidget *bright_off = widgets_icon_button_new("☀×");
    g_signal_connect(bright_off, "clicked", G_CALLBACK(on_brightness_off), data);
    gtk_box_pack_start(GTK_BOX(data->top_bar), bright_off, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), data->top_bar, FALSE, FALSE, 0);

    /* == Scrolled window + event box + image ==================== */
    data->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(
        GTK_SCROLLED_WINDOW(data->scrolled_window), GTK_SHADOW_NONE);

    data->event_box = gtk_event_box_new();
    gtk_widget_add_events(data->event_box, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(data->event_box, "button-press-event",
                     G_CALLBACK(on_image_button_press), data);

    GtkWidget *content_vbox = gtk_vbox_new(FALSE, 0);

    data->loading_overlay = widgets_spinner_new("Loading chapter...");
    gtk_widget_set_size_request(data->loading_overlay, -1, 80);
    gtk_box_pack_start(GTK_BOX(content_vbox), data->loading_overlay,
                       FALSE, FALSE, 0);

    data->image_widget = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(content_vbox), data->image_widget,
                       TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(data->event_box), content_vbox);

    gtk_scrolled_window_add_with_viewport(
        GTK_SCROLLED_WINDOW(data->scrolled_window), data->event_box);

    GtkWidget *vp = gtk_bin_get_child(GTK_BIN(data->scrolled_window));
    if (GTK_IS_VIEWPORT(vp))
        gtk_viewport_set_shadow_type(GTK_VIEWPORT(vp), GTK_SHADOW_NONE);

    gtk_box_pack_start(GTK_BOX(vbox), data->scrolled_window, TRUE, TRUE, 0);

    /* == Bottom bar (slider) ==================================== */
    data->bottom_bar = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(data->bottom_bar), 4);

    data->page_slider = gtk_hscale_new_with_range(1, 2, 1);
    gtk_scale_set_draw_value(GTK_SCALE(data->page_slider), FALSE);
    gtk_range_set_inverted(GTK_RANGE(data->page_slider), TRUE);
    gtk_widget_set_size_request(data->page_slider, -1, TOUCH_MIN_SIZE);
    g_signal_connect(data->page_slider, "value-changed",
                     G_CALLBACK(on_slider_changed), data);
    gtk_box_pack_start(GTK_BOX(data->bottom_bar), data->page_slider,
                       TRUE, TRUE, 0);

    gtk_box_pack_end(GTK_BOX(vbox), data->bottom_bar, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);

    /* Show loading and start bulk download */
    show_loading(data);

    data->prefetch_thread = g_thread_create(prefetch_thread_func, data,
                                              TRUE, NULL);

    return vbox;
}
