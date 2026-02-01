#ifndef UPDATER_H
#define UPDATER_H

#include "app.h"

/* Kick off a background update check against GitHub releases.
 * Safe to call from the main thread — network I/O runs on a GThread.
 * When force is TRUE (manual check), shows "up to date" or offers
 * reinstall even if the build SHA matches the latest release. */
void updater_check(App *app, gboolean force);

#endif /* UPDATER_H */
