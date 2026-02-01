#ifndef CACHE_H
#define CACHE_H

#include <glib.h>

/* Initialize the cache in the given directory. */
void    cache_init(const char *cache_dir);
void    cache_shutdown(void);

/* Store raw bytes under a key. */
void    cache_put(const char *key, const void *data, size_t len);

/* Retrieve cached data. Returns NULL if not found. Caller must g_free. */
void   *cache_get(const char *key, size_t *out_len);

/* Check if a key exists in cache. */
gboolean cache_has(const char *key);

/* Generate a cache key from a URL. Caller must g_free. */
char   *cache_key_from_url(const char *url);

#endif /* CACHE_H */
