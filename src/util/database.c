#include "database.h"
#include <sqlite3.h>
#include <stdio.h>

static sqlite3 *db = NULL;

static const char *schema =
    "CREATE TABLE IF NOT EXISTS reading_progress ("
    "  id INTEGER PRIMARY KEY,"
    "  manga_url TEXT NOT NULL,"
    "  chapter_url TEXT NOT NULL,"
    "  page_index INTEGER NOT NULL,"
    "  total_pages INTEGER DEFAULT 0,"
    "  updated_at INTEGER NOT NULL,"
    "  UNIQUE(manga_url, chapter_url)"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS completed_chapters ("
    "  id INTEGER PRIMARY KEY,"
    "  manga_url TEXT NOT NULL,"
    "  chapter_url TEXT NOT NULL,"
    "  completed_at INTEGER NOT NULL,"
    "  UNIQUE(manga_url, chapter_url)"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS recent_manga ("
    "  id INTEGER PRIMARY KEY,"
    "  manga_url TEXT NOT NULL UNIQUE,"
    "  manga_title TEXT NOT NULL,"
    "  chapter_url TEXT,"
    "  chapter_title TEXT,"
    "  last_read INTEGER NOT NULL"
    ");"
    ""
    "CREATE INDEX IF NOT EXISTS idx_completed_manga ON completed_chapters(manga_url);"
    "CREATE INDEX IF NOT EXISTS idx_progress_lookup ON reading_progress(manga_url, chapter_url);"
    "CREATE INDEX IF NOT EXISTS idx_recent_read ON recent_manga(last_read DESC);"
    ""
    "CREATE TABLE IF NOT EXISTS settings ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS favorites ("
    "  id INTEGER PRIMARY KEY,"
    "  manga_url TEXT NOT NULL UNIQUE,"
    "  manga_title TEXT NOT NULL,"
    "  cover_url TEXT,"
    "  added_at INTEGER NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_favorites_added ON favorites(added_at DESC);";

gboolean db_init(void) {
    char *db_dir = g_build_filename(g_get_user_cache_dir(), "manga-reader", NULL);
    g_mkdir_with_parents(db_dir, 0755);
    
    char *db_path = g_build_filename(db_dir, "manga-reader.db", NULL);
    g_free(db_dir);

    int rc = sqlite3_open(db_path, &db);
    g_free(db_path);

    if (rc != SQLITE_OK) {
        g_warning("Failed to open database: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    /* Create schema */
    char *err_msg = NULL;
    rc = sqlite3_exec(db, schema, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_warning("Failed to create schema: %s", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }

    g_message("Database initialized successfully");
    return TRUE;
}

void db_shutdown(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

/* ── Reading Progress ───────────────────────────────────────────────── */

gboolean db_save_progress(const char *manga_url, const char *chapter_url, int page_index) {
    if (!db) return FALSE;

    /* Get current total_pages for this specific chapter if it exists */
    const char *check_sql =
        "SELECT total_pages FROM reading_progress WHERE manga_url = ? AND chapter_url = ? LIMIT 1";
    sqlite3_stmt *check_stmt;
    int total_pages = 0;

    if (sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(check_stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(check_stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            total_pages = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }

    const char *sql =
        "INSERT OR REPLACE INTO reading_progress (manga_url, chapter_url, page_index, total_pages, updated_at) "
        "VALUES (?, ?, ?, ?, strftime('%s', 'now'))";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare save progress: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, page_index);
    sqlite3_bind_int(stmt, 4, total_pages);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

gboolean db_save_chapter_progress(const char *manga_url, const char *chapter_url,
                                   int page_index, int total_pages) {
    if (!db) return FALSE;

    const char *sql =
        "INSERT OR REPLACE INTO reading_progress (manga_url, chapter_url, page_index, total_pages, updated_at) "
        "VALUES (?, ?, ?, ?, strftime('%s', 'now'))";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare save chapter progress: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, page_index);
    sqlite3_bind_int(stmt, 4, total_pages);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

DbProgress *db_load_progress(void) {
    if (!db) return NULL;

    const char *sql = "SELECT manga_url, chapter_url, page_index FROM reading_progress LIMIT 1";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return NULL;
    }

    DbProgress *progress = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        progress = g_new0(DbProgress, 1);
        progress->manga_url = g_strdup((const char *)sqlite3_column_text(stmt, 0));
        progress->chapter_url = g_strdup((const char *)sqlite3_column_text(stmt, 1));
        progress->page_index = sqlite3_column_int(stmt, 2);
    }

    sqlite3_finalize(stmt);
    return progress;
}

void db_progress_free(DbProgress *progress) {
    if (!progress) return;
    g_free(progress->manga_url);
    g_free(progress->chapter_url);
    g_free(progress);
}

/* ── Chapter Completion ─────────────────────────────────────────────── */

gboolean db_mark_chapter_completed(const char *manga_url, const char *chapter_url) {
    if (!db) return FALSE;

    const char *sql = 
        "INSERT OR IGNORE INTO completed_chapters (manga_url, chapter_url, completed_at) "
        "VALUES (?, ?, strftime('%s', 'now'))";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare mark completed: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

gboolean db_is_chapter_completed(const char *manga_url, const char *chapter_url) {
    if (!db) return FALSE;

    const char *sql = 
        "SELECT 1 FROM completed_chapters WHERE manga_url = ? AND chapter_url = ? LIMIT 1";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);

    gboolean completed = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return completed;
}

GPtrArray *db_get_completed_chapters(const char *manga_url) {
    if (!db) return NULL;

    const char *sql = "SELECT chapter_url FROM completed_chapters WHERE manga_url = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);

    GPtrArray *chapters = g_ptr_array_new_with_free_func(g_free);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *chapter_url = (const char *)sqlite3_column_text(stmt, 0);
        g_ptr_array_add(chapters, g_strdup(chapter_url));
    }

    sqlite3_finalize(stmt);
    return chapters;
}

int db_get_chapter_progress(const char *manga_url, const char *chapter_url) {
    if (!db) return -1;

    const char *sql = 
        "SELECT page_index FROM reading_progress "
        "WHERE manga_url = ? AND chapter_url = ? LIMIT 1";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);

    int page_index = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        page_index = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return page_index;
}

int db_get_chapter_total_pages(const char *manga_url, const char *chapter_url) {
    if (!db) return -1;

    const char *sql = 
        "SELECT total_pages FROM reading_progress "
        "WHERE manga_url = ? AND chapter_url = ? LIMIT 1";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chapter_url, -1, SQLITE_TRANSIENT);

    int total_pages = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total_pages = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return total_pages;
}

/* ── Recently Read ──────────────────────────────────────────────────── */

gboolean db_update_recent_manga(const char *manga_url, const char *manga_title,
                                 const char *chapter_url, const char *chapter_title) {
    if (!db) return FALSE;

    const char *sql = 
        "INSERT OR REPLACE INTO recent_manga (manga_url, manga_title, chapter_url, chapter_title, last_read) "
        "VALUES (?, ?, ?, ?, strftime('%s', 'now'))";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare update recent manga: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, manga_title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, chapter_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, chapter_title, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

GPtrArray *db_get_recent_manga(int limit) {
    if (!db) return NULL;

    const char *sql = 
        "SELECT manga_url, manga_title, last_read "
        "FROM recent_manga "
        "ORDER BY last_read DESC LIMIT ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, limit);

    GPtrArray *recent = g_ptr_array_new_with_free_func((GDestroyNotify)db_recent_manga_free);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RecentManga *rm = g_new0(RecentManga, 1);
        rm->manga_url = g_strdup((const char *)sqlite3_column_text(stmt, 0));
        rm->manga_title = g_strdup((const char *)sqlite3_column_text(stmt, 1));
        rm->last_read = sqlite3_column_int64(stmt, 2);
        
        /* We don't need chapter info for the list, just manga */
        rm->chapter_url = NULL;
        rm->chapter_title = NULL;
        rm->page_index = -1;
        rm->total_pages = -1;
        
        g_ptr_array_add(recent, rm);
    }

    sqlite3_finalize(stmt);
    return recent;
}

void db_recent_manga_free(RecentManga *recent) {
    if (!recent) return;
    g_free(recent->manga_url);
    g_free(recent->manga_title);
    g_free(recent->chapter_url);
    g_free(recent->chapter_title);
    g_free(recent);
}

/* ── Settings ──────────────────────────────────────────────────────── */

char *db_get_setting(const char *key) {
    if (!db) return NULL;

    const char *sql = "SELECT value FROM settings WHERE key = ? LIMIT 1";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);

    char *value = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = g_strdup((const char *)sqlite3_column_text(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return value;
}

gboolean db_set_setting(const char *key, const char *value) {
    if (!db) return FALSE;

    const char *sql =
        "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare set setting: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

/* ── Favorites ─────────────────────────────────────────────────────── */

gboolean db_add_favorite(const char *manga_url, const char *manga_title, const char *cover_url) {
    if (!db) return FALSE;

    const char *sql =
        "INSERT OR IGNORE INTO favorites (manga_url, manga_title, cover_url, added_at) "
        "VALUES (?, ?, ?, strftime('%s', 'now'))";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare add favorite: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, manga_title, -1, SQLITE_TRANSIENT);
    if (cover_url)
        sqlite3_bind_text(stmt, 3, cover_url, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 3);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

gboolean db_remove_favorite(const char *manga_url) {
    if (!db) return FALSE;

    const char *sql = "DELETE FROM favorites WHERE manga_url = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare remove favorite: %s", sqlite3_errmsg(db));
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

gboolean db_is_favorite(const char *manga_url) {
    if (!db) return FALSE;

    const char *sql = "SELECT 1 FROM favorites WHERE manga_url = ? LIMIT 1";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return FALSE;

    sqlite3_bind_text(stmt, 1, manga_url, -1, SQLITE_TRANSIENT);

    gboolean found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

GPtrArray *db_get_favorites(void) {
    if (!db) return NULL;

    const char *sql =
        "SELECT manga_url, manga_title, cover_url, added_at "
        "FROM favorites ORDER BY added_at DESC";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    GPtrArray *favs = g_ptr_array_new_with_free_func((GDestroyNotify)db_favorite_manga_free);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FavoriteManga *fav = g_new0(FavoriteManga, 1);
        fav->manga_url = g_strdup((const char *)sqlite3_column_text(stmt, 0));
        fav->manga_title = g_strdup((const char *)sqlite3_column_text(stmt, 1));
        const char *cover = (const char *)sqlite3_column_text(stmt, 2);
        fav->cover_url = cover ? g_strdup(cover) : NULL;
        fav->added_at = sqlite3_column_int64(stmt, 3);
        g_ptr_array_add(favs, fav);
    }

    sqlite3_finalize(stmt);
    return favs;
}

void db_favorite_manga_free(FavoriteManga *fav) {
    if (!fav) return;
    g_free(fav->manga_url);
    g_free(fav->manga_title);
    g_free(fav->cover_url);
    g_free(fav);
}
