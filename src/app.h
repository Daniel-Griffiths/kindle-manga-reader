#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include "models/manga.h"
#include "sources/source.h"

typedef enum {
    VIEW_SEARCH,
    VIEW_MANGA,
    VIEW_READER,
    VIEW_SETTINGS,
} ViewType;

typedef struct {
    char *manga_url;
    char *chapter_url;
    int   page_index;
} ReadingProgress;

typedef struct {
    GtkWidget    *window;
    GtkWidget    *container;    /* Main vbox holding current view */
    GtkWidget    *current_view; /* The currently displayed view widget */
    ViewType      current_view_type;
    MangaSource  *source;

    /* State passed between views */
    Manga        *current_manga;
    char         *current_chapter_url;
    int           current_page_index;
    int           current_chapter_index;
    int           current_chapter_total_pages;  /* For progress tracking */
} App;

App  *app_new(void);
void  app_destroy(App *app);
void  app_show_search(App *app);
void  app_show_manga(App *app, const char *manga_url);
void  app_show_reader(App *app, const char *chapter_url);
void  app_show_settings(App *app);
void  app_go_back(App *app);

/* Global app instance accessor */
App  *app_get(void);

/* Reading progress persistence */
void             app_save_reading_progress(App *app);
ReadingProgress *app_load_reading_progress(void);
void             reading_progress_free(ReadingProgress *rp);

#endif /* APP_H */
