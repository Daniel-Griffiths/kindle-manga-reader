#include "http.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_MAX_RETRIES 3
#define HTTP_RETRY_DELAY_US 500000  /* 0.5s */

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
    size_t total = size * nmemb;
    HttpResponse *resp = userp;
    char *tmp = realloc(resp->data, resp->size + total + 1);
    if (!tmp) return 0;
    resp->data = tmp;
    memcpy(resp->data + resp->size, contents, total);
    resp->size += total;
    resp->data[resp->size] = '\0';
    return total;
}

void http_global_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void http_global_cleanup(void) {
    curl_global_cleanup();
}

HttpResponse *http_get(const char *url) {
    return http_get_with_headers(url, NULL);
}

HttpResponse *http_get_with_headers(const char *url,
                                    const char *const *headers) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HttpResponse *resp = g_new0(HttpResponse, 1);

    char errbuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
#ifdef __APPLE__
    /* Use macOS Secure Transport native CA store */
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Linux; Android 4.4.2) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Version/4.0 Chrome/30.0.0.0 Safari/537.36");

    struct curl_slist *header_list = NULL;
    if (headers) {
        for (int i = 0; headers[i]; i++) {
            header_list = curl_slist_append(header_list, headers[i]);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = CURLE_OK;
    for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
        /* Reset response buffer between retries */
        free(resp->data);
        resp->data = NULL;
        resp->size = 0;
        errbuf[0] = '\0';

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                              &resp->status_code);
            break;
        }

        if (attempt < HTTP_MAX_RETRIES - 1) {
            g_warning("HTTP GET attempt %d/%d failed for %s: %s%s%s",
                      attempt + 1, HTTP_MAX_RETRIES, url,
                      curl_easy_strerror(res),
                      errbuf[0] ? " — " : "", errbuf);
            g_usleep(HTTP_RETRY_DELAY_US);
        }
    }

    if (res != CURLE_OK) {
        g_warning("HTTP GET failed after %d attempts for %s: %s%s%s",
                  HTTP_MAX_RETRIES, url,
                  curl_easy_strerror(res),
                  errbuf[0] ? " — " : "", errbuf);
        http_response_free(resp);
        resp = NULL;
    }

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return resp;
}

void http_response_free(HttpResponse *resp) {
    if (!resp) return;
    free(resp->data);
    g_free(resp);
}
