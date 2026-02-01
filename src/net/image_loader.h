#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <gdk-pixbuf/gdk-pixbuf.h>

/* Download an image from url and return as a GdkPixbuf.
 * If max_width/max_height > 0, scale to fit within those bounds. */
GdkPixbuf *image_loader_fetch(const char *url, int max_width, int max_height);

/* Load a pixbuf from raw bytes. */
GdkPixbuf *image_loader_from_bytes(const char *data, size_t len,
                                   int max_width, int max_height);

/* Convert a pixbuf to grayscale using luminosity method. Returns new pixbuf. */
GdkPixbuf *image_loader_to_grayscale(GdkPixbuf *src);

/* Fetch image, scale, and optionally convert to grayscale in one call. */
GdkPixbuf *image_loader_fetch_processed(const char *url, int max_width,
                                        int max_height, gboolean grayscale);

#endif /* IMAGE_LOADER_H */
