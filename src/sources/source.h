#ifndef SOURCE_H
#define SOURCE_H

#include "../models/manga.h"

typedef struct MangaSource MangaSource;

struct MangaSource {
    const char *name;
    const char *base_url;

    MangaList *(*search)(MangaSource *self, const char *query);
    Manga     *(*get_manga_details)(MangaSource *self, const char *url);
    PageList  *(*get_chapter_pages)(MangaSource *self, const char *chapter_url);
    MangaList *(*get_latest)(MangaSource *self);
    void       (*destroy)(MangaSource *self);
};

#endif /* SOURCE_H */
