#ifndef MANGA_H
#define MANGA_H

#include <glib.h>

typedef struct {
    char *url;
    char *title;
    char *cover_url;
} MangaListItem;

typedef struct {
    GPtrArray *items; /* MangaListItem* */
} MangaList;

typedef struct {
    char *url;
    char *title;
    int number;
} Chapter;

typedef struct {
    char *url;
    char *title;
    char *cover_url;
    char *author;
    char *status;
    char *description;
    GPtrArray *chapters; /* Chapter* */
} Manga;

typedef struct {
    GPtrArray *image_urls; /* char* */
} PageList;

static inline MangaListItem *manga_list_item_new(void) {
    return g_new0(MangaListItem, 1);
}

static inline void manga_list_item_free(MangaListItem *item) {
    if (!item) return;
    g_free(item->url);
    g_free(item->title);
    g_free(item->cover_url);
    g_free(item);
}

static inline MangaList *manga_list_new(void) {
    MangaList *list = g_new0(MangaList, 1);
    list->items = g_ptr_array_new_with_free_func((GDestroyNotify)manga_list_item_free);
    return list;
}

static inline void manga_list_free(MangaList *list) {
    if (!list) return;
    g_ptr_array_free(list->items, TRUE);
    g_free(list);
}

static inline Chapter *chapter_new(void) {
    return g_new0(Chapter, 1);
}

static inline void chapter_free(Chapter *ch) {
    if (!ch) return;
    g_free(ch->url);
    g_free(ch->title);
    g_free(ch);
}

static inline Manga *manga_new(void) {
    Manga *m = g_new0(Manga, 1);
    m->chapters = g_ptr_array_new_with_free_func((GDestroyNotify)chapter_free);
    return m;
}

static inline void manga_free(Manga *m) {
    if (!m) return;
    g_free(m->url);
    g_free(m->title);
    g_free(m->cover_url);
    g_free(m->author);
    g_free(m->status);
    g_free(m->description);
    g_ptr_array_free(m->chapters, TRUE);
    g_free(m);
}

static inline PageList *page_list_new(void) {
    PageList *pl = g_new0(PageList, 1);
    pl->image_urls = g_ptr_array_new_with_free_func(g_free);
    return pl;
}

static inline void page_list_free(PageList *pl) {
    if (!pl) return;
    g_ptr_array_free(pl->image_urls, TRUE);
    g_free(pl);
}

#endif /* MANGA_H */
