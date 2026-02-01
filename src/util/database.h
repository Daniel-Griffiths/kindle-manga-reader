#ifndef DATABASE_H
#define DATABASE_H

#include <glib.h>

/* Initialize the database */
gboolean db_init(void);

/* Cleanup and close database */
void db_shutdown(void);

/* ── Reading Progress ───────────────────────────────────────────────── */

/* Save current reading position */
gboolean db_save_progress(const char *manga_url, const char *chapter_url, int page_index);

/* Load last reading position */
typedef struct {
    char *manga_url;
    char *chapter_url;
    int page_index;
} DbProgress;

DbProgress *db_load_progress(void);
void db_progress_free(DbProgress *progress);

/* ── Chapter Completion ─────────────────────────────────────────────── */

/* Mark a chapter as completed */
gboolean db_mark_chapter_completed(const char *manga_url, const char *chapter_url);

/* Check if a chapter is completed */
gboolean db_is_chapter_completed(const char *manga_url, const char *chapter_url);

/* Get all completed chapters for a manga */
GPtrArray *db_get_completed_chapters(const char *manga_url);

/* Get chapter progress for a specific chapter (returns -1 if no progress) */
int db_get_chapter_progress(const char *manga_url, const char *chapter_url);

/* Get total pages for a chapter (used for progress calculation) 
   Note: This needs to be set when reading, returns -1 if unknown */
int db_get_chapter_total_pages(const char *manga_url, const char *chapter_url);

/* Save chapter progress with total pages */
gboolean db_save_chapter_progress(const char *manga_url, const char *chapter_url, 
                                   int page_index, int total_pages);

/* ── Recently Read ──────────────────────────────────────────────────── */

typedef struct {
    char *manga_url;
    char *manga_title;
    char *chapter_url;
    char *chapter_title;
    int page_index;
    int total_pages;
    time_t last_read;
} RecentManga;

/* Get recently read manga (up to limit, ordered by most recent) */
GPtrArray *db_get_recent_manga(int limit);

/* Free a RecentManga struct */
void db_recent_manga_free(RecentManga *recent);

/* Update recently read manga list */
gboolean db_update_recent_manga(const char *manga_url, const char *manga_title,
                                 const char *chapter_url, const char *chapter_title);

/* ── Settings ──────────────────────────────────────────────────────── */

/* Get a setting value (caller must g_free the result) */
char *db_get_setting(const char *key);

/* Set a setting value */
gboolean db_set_setting(const char *key, const char *value);

/* ── Favorites ─────────────────────────────────────────────────────── */

typedef struct {
    char *manga_url;
    char *manga_title;
    char *cover_url;
    time_t added_at;
} FavoriteManga;

/* Add a manga to favorites */
gboolean db_add_favorite(const char *manga_url, const char *manga_title, const char *cover_url);

/* Remove a manga from favorites */
gboolean db_remove_favorite(const char *manga_url);

/* Check if a manga is favorited */
gboolean db_is_favorite(const char *manga_url);

/* Get all favorites ordered by most recently added */
GPtrArray *db_get_favorites(void);

/* Free a FavoriteManga struct */
void db_favorite_manga_free(FavoriteManga *fav);

#endif /* DATABASE_H */
