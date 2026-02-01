#include <gtk/gtk.h>
#include <string.h>
#include "app.h"
#include "device/brightness.h"
#include "net/http.h"
#include "util/cache.h"
#include "util/database.h"
#include "ui/widgets.h"
#include "sources/source_registry.h"
#include "sources/mangakatana.h"
#include "updater.h"

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    /* Apply e-ink friendly theme */
    widgets_apply_eink_style();

    /* Initialize subsystems */
    http_global_init();

    /* Initialize brightness control (works on Kindle, no-op elsewhere) */
    brightness_init();

    /* Initialize database */
    if (!db_init()) {
        g_warning("Failed to initialize database");
    }

    char *cache_path = g_build_filename(g_get_user_cache_dir(),
                                         "manga-reader", NULL);
    cache_init(cache_path);
    g_free(cache_path);

    /* Register manga sources */
    source_registry_init();
    source_registry_add(mangakatana_source_new());

    /* Create and show application */
    App *app = app_new();

    /* Start with search view */
    app_show_search(app);

    /* Check for updates in the background */
    updater_check(app);

    gtk_widget_show_all(app->window);

    gtk_main();

    /* Cleanup */
    app_destroy(app);
    source_registry_shutdown();
    cache_shutdown();
    db_shutdown();
    http_global_cleanup();
    brightness_shutdown();

    return 0;
}
