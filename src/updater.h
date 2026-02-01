#ifndef UPDATER_H
#define UPDATER_H

#include "app.h"

/* Kick off a background update check against GitHub releases.
 * Safe to call from the main thread — network I/O runs on a GThread. */
void updater_check(App *app);

#endif /* UPDATER_H */
