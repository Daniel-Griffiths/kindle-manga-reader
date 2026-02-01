#ifndef WIDGETS_H
#define WIDGETS_H

#include <gtk/gtk.h>

/* E-ink friendly constants */
#define EINK_BG_COLOR      "#FFFFFF"
#define EINK_FG_COLOR      "#000000"
#define EINK_MID_COLOR     "#999999"
#define EINK_BORDER        "#CCCCCC"

/* Font sizes: Kindle values are canonical (14/10/8pt).
 * macOS doubles them to compensate for lower DPI. */
#ifdef KINDLE
#define EINK_FONT_LARGE    "Sans Bold 14"
#define EINK_FONT_MED      "Sans 10"
#define EINK_FONT_MED_BOLD "Sans Bold 10"
#define EINK_FONT_SMALL    "Sans 8"
#else
#define EINK_FONT_LARGE    "Sans Bold 21"
#define EINK_FONT_MED      "Sans 15"
#define EINK_FONT_MED_BOLD "Sans Bold 15"
#define EINK_FONT_SMALL    "Sans 12"
#endif

/* Kindle is the reference platform (~212 DPI).
 * macOS (~110 DPI) halves all pixel sizes. */
#ifdef KINDLE
#define EINK_SCALE 1
#define EINK_BTN_PADDING  "{ 12, 12, 8, 8 }"
#define EINK_ENTRY_PADDING "{ 8, 8, 8, 8 }"
#else
#define EINK_SCALE 2
#define EINK_BTN_PADDING  "{ 3, 3, 1, 1 }"
#define EINK_ENTRY_PADDING "{ 4, 4, 4, 4 }"
#endif

#define THUMB_GRID_W   (330 / EINK_SCALE)
#define THUMB_GRID_H   (450 / EINK_SCALE)
#define THUMB_LIST_W   (160 / EINK_SCALE)
#define THUMB_LIST_H   (220 / EINK_SCALE)
#define THUMB_COVER_W  (280 / EINK_SCALE)
#define THUMB_COVER_H  (380 / EINK_SCALE)
#define TOUCH_MIN_SIZE  (72 / EINK_SCALE)

/* Apply e-ink friendly styling to the whole app */
void     widgets_apply_eink_style(void);

/* Create a large, e-ink friendly button */
GtkWidget *widgets_button_new(const char *label);

/* Create a square icon-only button (uses larger font for visibility) */
GtkWidget *widgets_icon_button_new(const char *icon);

/* Create a label with the given font desc string */
GtkWidget *widgets_label_new(const char *text, const char *font_desc);

/* Create a text entry with e-ink styling */
GtkWidget *widgets_entry_new(void);

/* Create a scrollable container */
GtkWidget *widgets_scrolled_new(GtkWidget *child);

/* Create a separator line */
GtkWidget *widgets_separator_new(void);

/* Show a simple status/loading message */
GtkWidget *widgets_status_label_new(const char *text);

/* Animated spinner label — starts automatically, stops on widget destroy */
GtkWidget *widgets_spinner_new(const char *text);
void       widgets_spinner_set_text(GtkWidget *spinner, const char *text);

#endif /* WIDGETS_H */
