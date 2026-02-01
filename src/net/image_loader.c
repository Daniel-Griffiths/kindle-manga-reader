#include "image_loader.h"
#include "http.h"
#include "../util/cache.h"
#include <string.h>

static GdkPixbuf *scale_pixbuf(GdkPixbuf *orig, int max_w, int max_h) {
    if (max_w <= 0 && max_h <= 0) return g_object_ref(orig);

    int w = gdk_pixbuf_get_width(orig);
    int h = gdk_pixbuf_get_height(orig);

    if (max_w <= 0) max_w = w;
    if (max_h <= 0) max_h = h;

    double scale_x = (double)max_w / w;
    double scale_y = (double)max_h / h;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;

    /* Allow scaling up or down to fit the display */
    if (scale == 1.0) return g_object_ref(orig);

    int new_w = (int)(w * scale);
    int new_h = (int)(h * scale);
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;

    return gdk_pixbuf_scale_simple(orig, new_w, new_h, GDK_INTERP_BILINEAR);
}

GdkPixbuf *image_loader_from_bytes(const char *data, size_t len,
                                   int max_width, int max_height) {
    GInputStream *stream = g_memory_input_stream_new_from_data(
        g_memdup(data, len), len, g_free);
    GError *err = NULL;
    GdkPixbuf *orig = gdk_pixbuf_new_from_stream(stream, NULL, &err);
    g_object_unref(stream);

    if (!orig) {
        g_warning("Failed to decode image: %s", err ? err->message : "unknown");
        g_clear_error(&err);
        return NULL;
    }

    GdkPixbuf *scaled = scale_pixbuf(orig, max_width, max_height);
    g_object_unref(orig);
    return scaled;
}

GdkPixbuf *image_loader_fetch(const char *url, int max_width, int max_height) {
    /* Check cache first */
    char *key = cache_key_from_url(url);
    size_t cached_len = 0;
    void *cached = cache_get(key, &cached_len);
    if (cached) {
        GdkPixbuf *pb = image_loader_from_bytes(cached, cached_len,
                                                 max_width, max_height);
        g_free(cached);
        g_free(key);
        return pb;
    }

    HttpResponse *resp = http_get(url);
    if (!resp || resp->status_code != 200 || !resp->data) {
        http_response_free(resp);
        g_free(key);
        return NULL;
    }

    /* Cache the raw image data */
    cache_put(key, resp->data, resp->size);
    g_free(key);

    GdkPixbuf *pb = image_loader_from_bytes(resp->data, resp->size,
                                            max_width, max_height);
    http_response_free(resp);
    return pb;
}

GdkPixbuf *image_loader_to_grayscale(GdkPixbuf *src) {
    if (!src) return NULL;

    int width = gdk_pixbuf_get_width(src);
    int height = gdk_pixbuf_get_height(src);
    int n_channels = gdk_pixbuf_get_n_channels(src);
    int rowstride = gdk_pixbuf_get_rowstride(src);
    gboolean has_alpha = gdk_pixbuf_get_has_alpha(src);

    GdkPixbuf *dest = gdk_pixbuf_copy(src);
    guchar *src_pixels = gdk_pixbuf_get_pixels(src);
    guchar *dst_pixels = gdk_pixbuf_get_pixels(dest);

    for (int y = 0; y < height; y++) {
        guchar *src_row = src_pixels + y * rowstride;
        guchar *dst_row = dst_pixels + y * rowstride;
        for (int x = 0; x < width; x++) {
            int offset = x * n_channels;
            guchar r = src_row[offset];
            guchar g = src_row[offset + 1];
            guchar b = src_row[offset + 2];
            /* Luminosity method */
            guchar gray = (guchar)(0.299 * r + 0.587 * g + 0.114 * b);
            dst_row[offset] = gray;
            dst_row[offset + 1] = gray;
            dst_row[offset + 2] = gray;
            if (has_alpha)
                dst_row[offset + 3] = src_row[offset + 3];
        }
    }
    return dest;
}

GdkPixbuf *image_loader_fetch_processed(const char *url, int max_width,
                                        int max_height, gboolean grayscale) {
    GdkPixbuf *pb = image_loader_fetch(url, max_width, max_height);
    if (!pb) return NULL;

    if (grayscale) {
        GdkPixbuf *gray = image_loader_to_grayscale(pb);
        g_object_unref(pb);
        return gray;
    }
    return pb;
}
