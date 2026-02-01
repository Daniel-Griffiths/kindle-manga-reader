#include "source_registry.h"
#include <string.h>

static GPtrArray *sources = NULL;

void source_registry_init(void) {
    if (sources) return;
    sources = g_ptr_array_new();
}

void source_registry_shutdown(void) {
    if (!sources) return;
    for (unsigned int i = 0; i < sources->len; i++) {
        MangaSource *s = g_ptr_array_index(sources, i);
        if (s->destroy) s->destroy(s);
    }
    g_ptr_array_free(sources, TRUE);
    sources = NULL;
}

void source_registry_add(MangaSource *source) {
    if (!sources) source_registry_init();
    g_ptr_array_add(sources, source);
}

MangaSource *source_registry_get(const char *name) {
    if (!sources) return NULL;
    for (unsigned int i = 0; i < sources->len; i++) {
        MangaSource *s = g_ptr_array_index(sources, i);
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

MangaSource *source_registry_get_default(void) {
    if (!sources || sources->len == 0) return NULL;
    return g_ptr_array_index(sources, 0);
}

unsigned int source_registry_count(void) {
    return sources ? sources->len : 0;
}

MangaSource *source_registry_get_by_index(unsigned int index) {
    if (!sources || index >= sources->len) return NULL;
    return g_ptr_array_index(sources, index);
}
