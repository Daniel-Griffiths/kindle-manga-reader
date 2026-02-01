#include "cache.h"
#include <stdio.h>
#include <string.h>

static char *cache_dir = NULL;

void cache_init(const char *dir) {
    g_free(cache_dir);
    cache_dir = g_strdup(dir);
    g_mkdir_with_parents(cache_dir, 0755);
}

void cache_shutdown(void) {
    g_free(cache_dir);
    cache_dir = NULL;
}

static char *cache_path(const char *key) {
    if (!cache_dir) return NULL;
    return g_build_filename(cache_dir, key, NULL);
}

char *cache_key_from_url(const char *url) {
    /* Simple hash-based key: use GChecksum for a hex digest */
    GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(cs, (const guchar *)url, (gssize)strlen(url));
    char *key = g_strdup(g_checksum_get_string(cs));
    g_checksum_free(cs);
    return key;
}

void cache_put(const char *key, const void *data, size_t len) {
    char *path = cache_path(key);
    if (!path) return;

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    }
    g_free(path);
}

void *cache_get(const char *key, size_t *out_len) {
    char *path = cache_path(key);
    if (!path) return NULL;

    gchar *contents = NULL;
    gsize length = 0;
    gboolean ok = g_file_get_contents(path, &contents, &length, NULL);
    g_free(path);

    if (!ok) return NULL;
    if (out_len) *out_len = length;
    return contents;
}

gboolean cache_has(const char *key) {
    char *path = cache_path(key);
    if (!path) return FALSE;
    gboolean exists = g_file_test(path, G_FILE_TEST_EXISTS);
    g_free(path);
    return exists;
}
