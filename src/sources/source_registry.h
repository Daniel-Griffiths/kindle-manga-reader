#ifndef SOURCE_REGISTRY_H
#define SOURCE_REGISTRY_H

#include "source.h"

void           source_registry_init(void);
void           source_registry_shutdown(void);
void           source_registry_add(MangaSource *source);
MangaSource   *source_registry_get(const char *name);
MangaSource   *source_registry_get_default(void);
unsigned int   source_registry_count(void);
MangaSource   *source_registry_get_by_index(unsigned int index);

#endif /* SOURCE_REGISTRY_H */
