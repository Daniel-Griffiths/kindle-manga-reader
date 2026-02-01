#include "updater.h"
#include "net/http.h"
#include "ui/widgets.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef BUILD_COMMIT
#define BUILD_COMMIT "unknown"
#endif

#define RELEASES_URL \
    "https://api.github.com/repos/Daniel-Griffiths/kindle-manga-reader/releases/latest"

#ifdef KINDLE
#define INSTALL_BIN_PATH "/mnt/us/extensions/manga-reader/bin/manga-reader"
#endif

/* ------------------------------------------------------------------ */
/* Minimal JSON helpers (no dependency — we only need two fields)      */
/* ------------------------------------------------------------------ */

/* Return a g_malloc'd copy of the string value for "key" in json.
 * Expects "key": "value" with no escaped quotes inside value. */
static char *json_get_string(const char *json, const char *key) {
    /* Build "\"key\":" */
    char *needle = g_strdup_printf("\"%s\"", key);
    const char *pos = strstr(json, needle);
    g_free(needle);
    if (!pos) return NULL;

    /* Advance past the key and find the opening quote of the value */
    pos = strchr(pos + strlen(key) + 2, '"');
    if (!pos) return NULL;
    pos++;  /* skip opening quote */

    const char *end = strchr(pos, '"');
    if (!end) return NULL;

    return g_strndup(pos, (gsize)(end - pos));
}

/* Extract the SHA from the release body.
 * The CI writes: "Automated Kindle build from `<sha>`" */
static char *parse_release_sha(const char *body) {
    const char *tick = strchr(body, '`');
    if (!tick) return NULL;
    tick++;
    const char *end = strchr(tick, '`');
    if (!end || end == tick) return NULL;
    return g_strndup(tick, (gsize)(end - tick));
}

/* Find the browser_download_url for the platform-specific .zip asset.
 * Tries the platform name first, falls back to any .zip asset. */
static char *find_asset_url(const char *json) {
#ifdef KINDLE
    const char *platform_name = "manga-reader-kindle.zip";
#else
    const char *platform_name = "manga-reader-mac.zip";
#endif
    const char *key = "browser_download_url";
    const char *pos;
    char *fallback = NULL;

    /* First pass: look for platform-specific asset */
    pos = json;
    while ((pos = strstr(pos, key)) != NULL) {
        /* Skip past: browser_download_url": "  →  land on the URL itself */
        pos = strchr(pos + strlen(key), '"');   /* closing " of key */
        if (!pos) break;
        pos = strchr(pos + 1, '"');             /* opening " of value */
        if (!pos) break;
        pos++;                                  /* start of URL text */
        const char *end = strchr(pos, '"');
        if (!end) break;
        char *url = g_strndup(pos, (gsize)(end - pos));
        if (strstr(url, platform_name)) return url;
        /* Remember first .zip as fallback */
        if (!fallback && strstr(url, ".zip"))
            fallback = g_strdup(url);
        g_free(url);
        pos = end + 1;
    }

    return fallback;
}

/* ------------------------------------------------------------------ */
/* Update prompt UI (fullscreen views)                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    App       *app;
    char      *download_url;
} UpdatePromptData;

static void replace_view(App *app, GtkWidget *view) {
    if (app->current_view) {
        gtk_container_remove(GTK_CONTAINER(app->container), app->current_view);
    }
    app->current_view = view;
    gtk_box_pack_start(GTK_BOX(app->container), view, TRUE, TRUE, 0);
    gtk_widget_show_all(app->container);
}

static void on_dismiss_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    UpdatePromptData *d = user_data;
    app_show_search(d->app);
    g_free(d->download_url);
    g_free(d);
}

/* ---- Apply thread (downloads + installs) ------------------------- */

/* Resolve the path to the running binary so we can replace it. */
static char *get_install_path(void) {
#ifdef KINDLE
    return g_strdup(INSTALL_BIN_PATH);
#elif defined(__APPLE__)
    /* _NSGetExecutablePath gives the actual binary path */
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        char *resolved = realpath(buf, NULL);
        if (resolved) return resolved;
        return g_strdup(buf);
    }
    return NULL;
#else
    char *path = g_file_read_link("/proc/self/exe", NULL);
    return path;
#endif
}

typedef struct {
    UpdatePromptData *prompt;
    gboolean          success;
    char             *error_msg;
} ApplyThreadData;

static gboolean apply_done(gpointer user_data) {
    ApplyThreadData *td = user_data;
    UpdatePromptData *d = td->prompt;
    App *app = d->app;

    GtkWidget *view = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(view), 16);

    /* Header */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *title = widgets_label_new(
        td->success ? "Update Complete" : "Update Failed", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(view), header, FALSE, FALSE, 16);

    gtk_box_pack_start(GTK_BOX(view), widgets_separator_new(), FALSE, FALSE, 12);

    /* Message */
    const char *msg_text;
    char *dynamic_msg = NULL;
    if (td->success) {
        msg_text = "Update installed successfully.\n\nPlease restart the app to use the new version.";
    } else {
        dynamic_msg = g_strdup_printf("The update could not be installed.\n\n%s",
                                       td->error_msg ? td->error_msg : "Unknown error");
        msg_text = dynamic_msg;
    }
    GtkWidget *msg = widgets_label_new(msg_text, EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);
    gtk_label_set_justify(GTK_LABEL(msg), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(view), msg, TRUE, TRUE, 20);
    g_free(dynamic_msg);

    /* OK button */
    GtkWidget *btn_box = gtk_vbox_new(FALSE, 12);
    GtkWidget *ok_btn = widgets_button_new("OK");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_dismiss_clicked), d);
    gtk_box_pack_start(GTK_BOX(btn_box), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(view), btn_box, FALSE, FALSE, 20);

    replace_view(app, view);

    g_free(td->error_msg);
    g_free(td);
    return FALSE;
}

static gpointer apply_thread_func(gpointer user_data) {
    ApplyThreadData *td = user_data;
    UpdatePromptData *d = td->prompt;
    td->success = FALSE;

    char *install_path = get_install_path();
    if (!install_path) {
        td->error_msg = g_strdup("could not determine binary path");
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Download zip */
    const char *headers[] = {
        "Accept: application/octet-stream",
        "User-Agent: kindle-manga-reader",
        NULL
    };
    HttpResponse *resp = http_get_with_headers(d->download_url, headers);
    if (!resp || resp->status_code != 200) {
        td->error_msg = g_strdup("download failed");
        http_response_free(resp);
        g_free(install_path);
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Write zip to temp file */
    const char *tmp_zip = "/tmp/manga-reader-update.zip";
    FILE *f = fopen(tmp_zip, "wb");
    if (!f) {
        td->error_msg = g_strdup("could not write temp file");
        http_response_free(resp);
        g_free(install_path);
        g_idle_add(apply_done, td);
        return NULL;
    }
    fwrite(resp->data, 1, resp->size, f);
    fclose(f);
    http_response_free(resp);

    /* Extract — zip layout differs per platform:
     *   Kindle: extensions/manga-reader/bin/manga-reader
     *   Mac:    manga-reader                              */
#ifdef KINDLE
    #define ZIP_BINARY_PATH "extensions/manga-reader/bin/manga-reader"
#else
    #define ZIP_BINARY_PATH "manga-reader"
#endif
    int ret = system("cd /tmp && unzip -o manga-reader-update.zip "
                     ZIP_BINARY_PATH " 2>/dev/null");
    if (ret != 0) {
        td->error_msg = g_strdup("unzip failed");
        g_free(install_path);
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Move extracted binary to a known location */
    const char *extracted = "/tmp/" ZIP_BINARY_PATH;
    const char *tmp_bin   = "/tmp/manga-reader-update-bin";
    char *mv_cmd = g_strdup_printf("mv '%s' '%s'", extracted, tmp_bin);
    ret = system(mv_cmd);
    g_free(mv_cmd);
    if (ret != 0) {
        td->error_msg = g_strdup("could not move extracted binary");
        g_free(install_path);
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Clean up extracted directory structure (Kindle) */
#ifdef KINDLE
    system("rm -rf /tmp/extensions");
#endif

    /* Sanity-check extracted binary (must be > 100KB) */
    struct stat st;
    if (stat(tmp_bin, &st) != 0 || st.st_size < 100 * 1024) {
        td->error_msg = g_strdup("extracted binary looks invalid");
        remove("/tmp/manga-reader-update.zip");
        remove(tmp_bin);
        g_free(install_path);
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Back up current binary */
    char *bak_path = g_strdup_printf("%s.bak", install_path);
    char *bak_cmd = g_strdup_printf("cp '%s' '%s'", install_path, bak_path);
    system(bak_cmd);  /* best-effort */
    g_free(bak_cmd);
    g_free(bak_path);

    /* Replace binary */
    char *cmd = g_strdup_printf("cp '%s' '%s' && chmod +x '%s'",
                                 tmp_bin, install_path, install_path);
    ret = system(cmd);
    g_free(cmd);
    g_free(install_path);
    if (ret != 0) {
        td->error_msg = g_strdup("binary replacement failed");
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Cleanup */
    remove("/tmp/manga-reader-update.zip");
    remove(tmp_bin);

    td->success = TRUE;
    g_idle_add(apply_done, td);
    return NULL;
}

static void on_install_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    UpdatePromptData *d = user_data;
    App *app = d->app;

    /* Show downloading spinner view */
    GtkWidget *view = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(view), 16);

    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *title = widgets_label_new("Updating", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(view), header, FALSE, FALSE, 16);

    gtk_box_pack_start(GTK_BOX(view), widgets_separator_new(), FALSE, FALSE, 12);

    GtkWidget *spinner = widgets_spinner_new("Downloading update...");
    gtk_box_pack_start(GTK_BOX(view), spinner, TRUE, TRUE, 20);

    replace_view(app, view);

    /* Start download thread */
    ApplyThreadData *td = g_new0(ApplyThreadData, 1);
    td->prompt = d;
    g_thread_create(apply_thread_func, td, FALSE, NULL);
}

static void show_update_view(App *app, const char *download_url) {
    UpdatePromptData *d = g_new0(UpdatePromptData, 1);
    d->app = app;
    d->download_url = g_strdup(download_url);

    GtkWidget *view = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(view), 16);

    /* Header */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *title = widgets_label_new("Update Available", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(view), header, FALSE, FALSE, 16);

    gtk_box_pack_start(GTK_BOX(view), widgets_separator_new(), FALSE, FALSE, 12);

    /* Message */
    GtkWidget *msg = widgets_label_new(
        "A new version of Manga Reader is available.",
        EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);
    gtk_label_set_justify(GTK_LABEL(msg), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(view), msg, TRUE, TRUE, 20);

    /* Buttons */
    GtkWidget *btn_box = gtk_vbox_new(FALSE, 12);

    if (download_url) {
        GtkWidget *install_btn = widgets_button_new("Install Update");
        g_signal_connect(install_btn, "clicked", G_CALLBACK(on_install_clicked), d);
        gtk_box_pack_start(GTK_BOX(btn_box), install_btn, FALSE, FALSE, 0);
    }

    GtkWidget *skip_btn = widgets_button_new("Skip");
    g_signal_connect(skip_btn, "clicked", G_CALLBACK(on_dismiss_clicked), d);
    gtk_box_pack_start(GTK_BOX(btn_box), skip_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(view), btn_box, FALSE, FALSE, 20);

    replace_view(app, view);
}

static void show_up_to_date_view(App *app, const char *download_url) {
    UpdatePromptData *d = g_new0(UpdatePromptData, 1);
    d->app = app;
    d->download_url = g_strdup(download_url);

    GtkWidget *view = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(view), 16);

    /* Header */
    GtkWidget *header = gtk_hbox_new(FALSE, 0);
    GtkWidget *title = widgets_label_new("Up to Date", EINK_FONT_LARGE);
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(view), header, FALSE, FALSE, 16);

    gtk_box_pack_start(GTK_BOX(view), widgets_separator_new(), FALSE, FALSE, 12);

    /* Message */
    GtkWidget *msg = widgets_label_new(
        "You are running the latest version.",
        EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);
    gtk_label_set_justify(GTK_LABEL(msg), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(view), msg, TRUE, TRUE, 20);

    /* Buttons */
    GtkWidget *btn_box = gtk_vbox_new(FALSE, 12);

    if (download_url) {
        GtkWidget *reinstall_btn = widgets_button_new("Reinstall Anyway");
        g_signal_connect(reinstall_btn, "clicked",
                         G_CALLBACK(on_install_clicked), d);
        gtk_box_pack_start(GTK_BOX(btn_box), reinstall_btn, FALSE, FALSE, 0);
    }

    GtkWidget *ok_btn = widgets_button_new("OK");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_dismiss_clicked), d);
    gtk_box_pack_start(GTK_BOX(btn_box), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(view), btn_box, FALSE, FALSE, 20);

    replace_view(app, view);
}

/* ------------------------------------------------------------------ */
/* Background update check                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    App      *app;
    char     *download_url;  /* NULL if no update */
    gboolean  force;         /* TRUE = manual check from settings */
    gboolean  up_to_date;    /* TRUE when SHAs match */
} CheckThreadData;

static gboolean check_done(gpointer user_data) {
    CheckThreadData *td = user_data;

    if (td->up_to_date && td->force) {
        g_message("Already up to date (build: %s) — showing prompt", BUILD_COMMIT);
        show_up_to_date_view(td->app, td->download_url);
    } else if (td->download_url && !td->up_to_date) {
        g_message("Update available — showing prompt");
        show_update_view(td->app, td->download_url);
    } else {
        g_message("No update available (build: %s)", BUILD_COMMIT);
    }

    g_free(td->download_url);
    g_free(td);
    return FALSE;
}

static gpointer check_thread_func(gpointer user_data) {
    CheckThreadData *td = user_data;
    td->download_url = NULL;

    if (strcmp(BUILD_COMMIT, "unknown") == 0) {
        g_message("updater: no build commit compiled in, skipping check");
        g_idle_add(check_done, td);
        return NULL;
    }

    const char *headers[] = {
        "Accept: application/vnd.github+json",
        "User-Agent: kindle-manga-reader",
        NULL
    };

    HttpResponse *resp = http_get_with_headers(RELEASES_URL, headers);
    if (!resp || resp->status_code != 200) {
        g_message("updater: could not fetch releases (status %ld)",
                  resp ? resp->status_code : 0);
        http_response_free(resp);
        g_idle_add(check_done, td);
        return NULL;
    }

    /* Parse body field for the SHA */
    char *body = json_get_string(resp->data, "body");
    if (!body) {
        g_message("updater: no body field in release JSON");
        http_response_free(resp);
        g_idle_add(check_done, td);
        return NULL;
    }

    char *remote_sha = parse_release_sha(body);
    g_free(body);

    if (!remote_sha) {
        g_message("updater: could not parse SHA from release body");
        http_response_free(resp);
        g_idle_add(check_done, td);
        return NULL;
    }

    g_message("updater: local=%s remote=%s", BUILD_COMMIT, remote_sha);

    td->download_url = find_asset_url(resp->data);

    if (strcmp(BUILD_COMMIT, remote_sha) != 0) {
        if (!td->download_url) {
            g_message("updater: update found but no .zip asset URL");
        }
    } else {
        td->up_to_date = TRUE;
    }

    g_free(remote_sha);
    http_response_free(resp);
    g_idle_add(check_done, td);
    return NULL;
}

void updater_check(App *app, gboolean force) {
    CheckThreadData *td = g_new0(CheckThreadData, 1);
    td->app = app;
    td->force = force;
    g_thread_create(check_thread_func, td, FALSE, NULL);
}
