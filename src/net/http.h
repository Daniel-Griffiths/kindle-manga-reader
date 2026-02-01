#ifndef HTTP_H
#define HTTP_H

#include <glib.h>

typedef struct {
    char  *data;
    size_t size;
    long   status_code;
} HttpResponse;

void          http_global_init(void);
void          http_global_cleanup(void);
HttpResponse *http_get(const char *url);
HttpResponse *http_get_with_headers(const char *url, const char *const *headers);
void          http_response_free(HttpResponse *resp);

#endif /* HTTP_H */
