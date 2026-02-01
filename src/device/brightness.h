#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H

#include <glib.h>

/* Initialize brightness control */
gboolean brightness_init(void);

/* Get current brightness level (0-24) */
int brightness_get(void);

/* Set brightness level (0-24, where 0 = off) */
gboolean brightness_set(int level);

/* Increase brightness by one step */
gboolean brightness_increase(void);

/* Decrease brightness by one step */
gboolean brightness_decrease(void);

/* Turn frontlight completely off */
gboolean brightness_off(void);

/* Get initialization error message (NULL if no error) */
const char* brightness_get_error(void);

/* Cleanup */
void brightness_shutdown(void);

#endif /* BRIGHTNESS_H */
