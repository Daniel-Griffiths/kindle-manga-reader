#include "updater.h"
#include "net/http.h"
#include "ui/widgets.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifndef BUILD_COMMIT
#define BUILD_COMMIT "unknown"
#endif

#define RELEASES_URL \
    "https://api.github.com/repos/Daniel-Griffiths/kindle-manga-reader/releases/latest"

#define KINDLE_BIN_PATH \
    "/mnt/us/extensions/manga-reader/bin/manga-reader"

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

/* Find the browser_download_url for the .zip asset.
 * We look for "browser_download_url": "...kindle-manga-reader.zip" */
static char *find_asset_url(const char *json) {
    const char *key = "browser_download_url";
    const char *pos = json;
    while ((pos = strstr(pos, key)) != NULL) {
        pos = strchr(pos + strlen(key), '"');
        if (!pos) return NULL;
        pos++;  /* skip opening quote */
        const char *end = strchr(pos, '"');
        if (!end) return NULL;
        char *url = g_strndup(pos, (gsize)(end - pos));
        if (strstr(url, ".zip")) return url;
        g_free(url);
        pos = end + 1;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Update prompt UI                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    App       *app;
    char      *download_url;
    GtkWidget *banner;
} UpdatePromptData;

static void update_prompt_data_free(UpdatePromptData *d) {
    g_free(d->download_url);
    g_free(d);
}

/* Forward declarations */
static void on_update_clicked(GtkWidget *btn, gpointer user_data);
static void on_skip_clicked(GtkWidget *btn, gpointer user_data);

static void dismiss_banner(UpdatePromptData *d) {
    if (d->banner && gtk_widget_get_parent(d->banner)) {
        gtk_container_remove(GTK_CONTAINER(d->app->container), d->banner);
    }
    d->banner = NULL;
}

/* ---- Apply thread (downloads + installs, Kindle only) ------------ */

#ifdef KINDLE
typedef struct {
    UpdatePromptData *prompt;
    gboolean          success;
    char             *error_msg;
} ApplyThreadData;

static gboolean apply_done(gpointer user_data) {
    ApplyThreadData *td = user_data;
    UpdatePromptData *d = td->prompt;

    dismiss_banner(d);

    if (td->success) {
        GtkWidget *bar = gtk_hbox_new(FALSE, 8);
        gtk_container_set_border_width(GTK_CONTAINER(bar), 8);
        GtkWidget *lbl = widgets_label_new(
            "Update installed. Please restart the app.", EINK_FONT_MED_BOLD);
        gtk_box_pack_start(GTK_BOX(bar), lbl, TRUE, TRUE, 0);
        GtkWidget *ok = widgets_button_new("OK");
        d->banner = bar;
        g_signal_connect(ok, "clicked", G_CALLBACK(on_skip_clicked), d);
        gtk_box_pack_end(GTK_BOX(bar), ok, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(d->app->container), bar, FALSE, FALSE, 0);
        gtk_box_reorder_child(GTK_BOX(d->app->container), bar, 0);
        gtk_widget_show_all(bar);
    } else {
        GtkWidget *bar = gtk_hbox_new(FALSE, 8);
        gtk_container_set_border_width(GTK_CONTAINER(bar), 8);
        char *msg = g_strdup_printf("Update failed: %s",
                                     td->error_msg ? td->error_msg : "unknown error");
        GtkWidget *lbl = widgets_label_new(msg, EINK_FONT_MED);
        g_free(msg);
        gtk_box_pack_start(GTK_BOX(bar), lbl, TRUE, TRUE, 0);
        GtkWidget *ok = widgets_button_new("OK");
        d->banner = bar;
        g_signal_connect(ok, "clicked", G_CALLBACK(on_skip_clicked), d);
        gtk_box_pack_end(GTK_BOX(bar), ok, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(d->app->container), bar, FALSE, FALSE, 0);
        gtk_box_reorder_child(GTK_BOX(d->app->container), bar, 0);
        gtk_widget_show_all(bar);
    }

    g_free(td->error_msg);
    g_free(td);
    return FALSE;
}

static gpointer apply_thread_func(gpointer user_data) {
    ApplyThreadData *td = user_data;
    UpdatePromptData *d = td->prompt;
    td->success = FALSE;

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
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Write zip to temp file */
    const char *tmp_zip = "/tmp/manga-reader-update.zip";
    FILE *f = fopen(tmp_zip, "wb");
    if (!f) {
        td->error_msg = g_strdup("could not write temp file");
        http_response_free(resp);
        g_idle_add(apply_done, td);
        return NULL;
    }
    fwrite(resp->data, 1, resp->size, f);
    fclose(f);
    http_response_free(resp);

    /* Extract — the zip should contain the manga-reader binary */
    int ret = system("cd /tmp && unzip -o manga-reader-update.zip manga-reader 2>/dev/null");
    if (ret != 0) {
        td->error_msg = g_strdup("unzip failed");
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Sanity-check extracted binary (must be > 100KB to be a real ELF) */
    struct stat st;
    if (stat("/tmp/manga-reader", &st) != 0 || st.st_size < 100 * 1024) {
        td->error_msg = g_strdup("extracted binary looks invalid");
        remove("/tmp/manga-reader-update.zip");
        remove("/tmp/manga-reader");
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Back up current binary so KUAL "Restore Previous Version" works */
    char *bak_path = g_strdup_printf("%s.bak", KINDLE_BIN_PATH);
    char *bak_cmd = g_strdup_printf("cp %s %s", KINDLE_BIN_PATH, bak_path);
    system(bak_cmd);  /* best-effort — don't fail the update if this fails */
    g_free(bak_cmd);
    g_free(bak_path);

    /* Replace binary */
    char *cmd = g_strdup_printf("cp /tmp/manga-reader %s && chmod +x %s",
                                 KINDLE_BIN_PATH, KINDLE_BIN_PATH);
    ret = system(cmd);
    g_free(cmd);
    if (ret != 0) {
        td->error_msg = g_strdup("binary replacement failed");
        g_idle_add(apply_done, td);
        return NULL;
    }

    /* Cleanup */
    remove("/tmp/manga-reader-update.zip");
    remove("/tmp/manga-reader");

    td->success = TRUE;
    g_idle_add(apply_done, td);
    return NULL;
}
#endif /* KINDLE */

static void on_skip_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    UpdatePromptData *d = user_data;
    dismiss_banner(d);
}

static void on_update_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
#ifdef KINDLE
    UpdatePromptData *d = user_data;

    /* Replace banner content with spinner */
    dismiss_banner(d);

    GtkWidget *bar = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(bar), 8);
    GtkWidget *spinner = widgets_spinner_new("Downloading update");
    gtk_box_pack_start(GTK_BOX(bar), spinner, TRUE, TRUE, 0);
    d->banner = bar;
    gtk_box_pack_start(GTK_BOX(d->app->container), bar, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(d->app->container), bar, 0);
    gtk_widget_show_all(bar);

    /* Start download thread */
    ApplyThreadData *td = g_new0(ApplyThreadData, 1);
    td->prompt = d;
    g_thread_create(apply_thread_func, td, FALSE, NULL);
#else
    (void)user_data;
#endif
}

static void show_update_banner(App *app, const char *download_url) {
    UpdatePromptData *d = g_new0(UpdatePromptData, 1);
    d->app = app;
    d->download_url = g_strdup(download_url);

    GtkWidget *bar = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(bar), 8);

    GtkWidget *lbl = widgets_label_new("Update available!", EINK_FONT_MED_BOLD);
    gtk_box_pack_start(GTK_BOX(bar), lbl, FALSE, FALSE, 0);

#ifdef KINDLE
    GtkWidget *update_btn = widgets_button_new("Install");
    g_signal_connect(update_btn, "clicked", G_CALLBACK(on_update_clicked), d);
    gtk_box_pack_end(GTK_BOX(bar), update_btn, FALSE, FALSE, 0);
#endif

    GtkWidget *skip_btn = widgets_button_new("Skip");
    g_signal_connect(skip_btn, "clicked", G_CALLBACK(on_skip_clicked), d);
    gtk_box_pack_end(GTK_BOX(bar), skip_btn, FALSE, FALSE, 0);

    d->banner = bar;

    /* Pack at the top of the container */
    gtk_box_pack_start(GTK_BOX(app->container), bar, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(app->container), bar, 0);
    gtk_widget_show_all(bar);
}

/* ------------------------------------------------------------------ */
/* Background update check                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    App  *app;
    char *download_url;  /* NULL if no update */
} CheckThreadData;

static gboolean check_done(gpointer user_data) {
    CheckThreadData *td = user_data;

    if (td->download_url) {
        g_message("Update available — showing prompt");
        show_update_banner(td->app, td->download_url);
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

    if (strcmp(BUILD_COMMIT, remote_sha) != 0) {
        td->download_url = find_asset_url(resp->data);
        if (!td->download_url) {
            g_message("updater: update found but no .zip asset URL");
        }
    }

    g_free(remote_sha);
    http_response_free(resp);
    g_idle_add(check_done, td);
    return NULL;
}

void updater_check(App *app) {
    CheckThreadData *td = g_new0(CheckThreadData, 1);
    td->app = app;
    g_thread_create(check_thread_func, td, FALSE, NULL);
}
