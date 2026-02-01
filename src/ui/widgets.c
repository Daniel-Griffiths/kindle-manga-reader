#include "widgets.h"

void widgets_apply_eink_style(void) {
    const char *rc_style =
        /* ── Base style for all widgets ─────────────────────────── */
        "style \"eink\" {\n"
        "  bg[NORMAL]   = \"" EINK_BG_COLOR "\"\n"
        "  bg[PRELIGHT] = \"" EINK_BG_COLOR "\"\n"
        "  bg[ACTIVE]   = \"#E0E0E0\"\n"
        "  bg[SELECTED] = \"#333333\"\n"
        "  bg[INSENSITIVE] = \"" EINK_BG_COLOR "\"\n"
        "  fg[NORMAL]   = \"" EINK_FG_COLOR "\"\n"
        "  fg[PRELIGHT] = \"" EINK_FG_COLOR "\"\n"
        "  fg[ACTIVE]   = \"" EINK_FG_COLOR "\"\n"
        "  fg[SELECTED] = \"" EINK_BG_COLOR "\"\n"
        "  fg[INSENSITIVE] = \"" EINK_MID_COLOR "\"\n"
        "  text[NORMAL]     = \"" EINK_FG_COLOR "\"\n"
        "  text[PRELIGHT]   = \"" EINK_FG_COLOR "\"\n"
        "  text[ACTIVE]     = \"" EINK_FG_COLOR "\"\n"
        "  text[SELECTED]   = \"" EINK_BG_COLOR "\"\n"
        "  text[INSENSITIVE]= \"" EINK_MID_COLOR "\"\n"
        "  base[NORMAL]     = \"" EINK_BG_COLOR "\"\n"
        "  base[PRELIGHT]   = \"" EINK_BG_COLOR "\"\n"
        "  base[ACTIVE]     = \"#E0E0E0\"\n"
        "  base[SELECTED]   = \"#333333\"\n"
        "  base[INSENSITIVE]= \"" EINK_BG_COLOR "\"\n"
        "  font_name = \"" EINK_FONT_MED "\"\n"
        "  GtkWidget::focus-line-width   = 0\n"
        "  GtkWidget::focus-padding      = 0\n"
        "  GtkWidget::interior-focus     = 0\n"
        "}\n"
        "widget \"*\" style \"eink\"\n"
        "\n"
        /* ── Buttons ─────────────────────────────────────────────── */
        "style \"eink-button\" {\n"
        "  bg[NORMAL]   = \"#F0F0F0\"\n"
        "  bg[PRELIGHT] = \"#E0E0E0\"\n"
        "  bg[ACTIVE]   = \"#D0D0D0\"\n"
        "  bg[INSENSITIVE] = \"#F0F0F0\"\n"
        "  fg[NORMAL]   = \"" EINK_FG_COLOR "\"\n"
        "  fg[PRELIGHT] = \"" EINK_FG_COLOR "\"\n"
        "  fg[ACTIVE]   = \"" EINK_FG_COLOR "\"\n"
        "  font_name = \"" EINK_FONT_MED "\"\n"
        "  GtkButton::inner-border       = " EINK_BTN_PADDING "\n"
        "  GtkButton::default-border     = { 0, 0, 0, 0 }\n"
        "  GtkButton::child-displacement-x = 0\n"
        "  GtkButton::child-displacement-y = 0\n"
        "}\n"
        "widget \"*GtkButton*\" style \"eink-button\"\n"
        "\n"
        /* ── Text entries ────────────────────────────────────────── */
        "style \"eink-entry\" {\n"
        "  bg[NORMAL]     = \"" EINK_BG_COLOR "\"\n"
        "  bg[ACTIVE]     = \"" EINK_BG_COLOR "\"\n"
        "  base[NORMAL]   = \"" EINK_BG_COLOR "\"\n"
        "  base[ACTIVE]   = \"" EINK_BG_COLOR "\"\n"
        "  text[NORMAL]   = \"" EINK_FG_COLOR "\"\n"
        "  text[ACTIVE]   = \"" EINK_FG_COLOR "\"\n"
        "  font_name = \"" EINK_FONT_MED "\"\n"
        "  GtkEntry::inner-border = " EINK_ENTRY_PADDING "\n"
        "}\n"
        "widget \"*GtkEntry*\" style \"eink-entry\"\n"
        "\n"
        /* ── Scrollbars ──────────────────────────────────────────── */
        "style \"eink-scrollbar\" {\n"
        "  GtkRange::slider-width        = 30\n"
        "  GtkRange::trough-border       = 2\n"
        "  GtkRange::stepper-size        = 0\n"
        "  GtkRange::min-slider-length   = 60\n"
        "  GtkScrollbar::min-slider-length = 60\n"
        "  bg[NORMAL]   = \"#888888\"\n"
        "  bg[PRELIGHT] = \"#666666\"\n"
        "  bg[ACTIVE]   = \"#444444\"\n"
        "}\n"
        "widget \"*GtkScrollbar*\" style \"eink-scrollbar\"\n"
        "\n"
        /* ── Horizontal scale (page slider) ──────────────────────── */
        "style \"eink-scale\" {\n"
        "  bg[NORMAL]   = \"#CCCCCC\"\n"
        "  bg[PRELIGHT] = \"#AAAAAA\"\n"
        "  bg[ACTIVE]   = \"#888888\"\n"
        "  GtkRange::slider-width  = 30\n"
        "  GtkScale::slider-length = 30\n"
        "  GtkRange::trough-border = 1\n"
        "}\n"
        "widget \"*GtkScale*\" style \"eink-scale\"\n"
        "\n"
        /* ── Separator ───────────────────────────────────────────── */
        "style \"eink-separator\" {\n"
        "  bg[NORMAL] = \"" EINK_BORDER "\"\n"
        "  fg[NORMAL] = \"" EINK_BORDER "\"\n"
        "}\n"
        "widget \"*GtkSeparator*\" style \"eink-separator\"\n"
        "\n"
        /* ── Viewport (remove extra borders inside scrolled windows) */
        "style \"eink-viewport\" {\n"
        "  bg[NORMAL] = \"" EINK_BG_COLOR "\"\n"
        "}\n"
        "widget \"*GtkViewport*\" style \"eink-viewport\"\n"
        "\n"
        /* ── Tooltips off (not useful on e-ink) ──────────────────── */
        "style \"eink-tooltip\" {\n"
        "  bg[NORMAL] = \"" EINK_BG_COLOR "\"\n"
        "  fg[NORMAL] = \"" EINK_FG_COLOR "\"\n"
        "  font_name = \"" EINK_FONT_SMALL "\"\n"
        "}\n"
        "widget \"gtk-tooltip*\" style \"eink-tooltip\"\n";

    gtk_rc_parse_string(rc_style);
}

GtkWidget *widgets_button_new(const char *label) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NORMAL);
    gtk_widget_set_size_request(btn, -1, TOUCH_MIN_SIZE);

    /* Explicitly set font on the button's label child */
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(btn));
    if (GTK_IS_LABEL(child)) {
        PangoFontDescription *fd =
            pango_font_description_from_string(EINK_FONT_MED);
        gtk_widget_modify_font(child, fd);
        pango_font_description_free(fd);
    }

    return btn;
}

GtkWidget *widgets_icon_button_new(const char *icon) {
    GtkWidget *btn = gtk_button_new_with_label(icon);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NORMAL);
    gtk_widget_set_size_request(btn, TOUCH_MIN_SIZE + TOUCH_MIN_SIZE / 2, TOUCH_MIN_SIZE);

    GtkWidget *child = gtk_bin_get_child(GTK_BIN(btn));
    if (GTK_IS_LABEL(child)) {
        PangoFontDescription *fd =
            pango_font_description_from_string(EINK_FONT_LARGE);
        gtk_widget_modify_font(child, fd);
        pango_font_description_free(fd);
    }

    return btn;
}

GtkWidget *widgets_label_new(const char *text, const char *font_desc) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    PangoFontDescription *fd = pango_font_description_from_string(
        font_desc ? font_desc : EINK_FONT_MED);
    gtk_widget_modify_font(label, fd);
    pango_font_description_free(fd);

    return label;
}

GtkWidget *widgets_entry_new(void) {
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_size_request(entry, -1, TOUCH_MIN_SIZE);

    PangoFontDescription *fd =
        pango_font_description_from_string(EINK_FONT_MED);
    gtk_widget_modify_font(entry, fd);
    pango_font_description_free(fd);

    return entry;
}

GtkWidget *widgets_scrolled_new(GtkWidget *child) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                                        GTK_SHADOW_NONE);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), child);

    /* Remove the viewport's default shadow too */
    GtkWidget *viewport = gtk_bin_get_child(GTK_BIN(sw));
    if (GTK_IS_VIEWPORT(viewport))
        gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);

    return sw;
}

GtkWidget *widgets_separator_new(void) {
    GtkWidget *sep = gtk_hseparator_new();
    gtk_widget_set_size_request(sep, -1, 1);
    return sep;
}

GtkWidget *widgets_status_label_new(const char *text) {
    GtkWidget *label = widgets_label_new(text, EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    GdkColor gray;
    gdk_color_parse(EINK_MID_COLOR, &gray);
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gray);
    return label;
}

/* ── Animated spinner ─────────────────────────────────────────────── */

static const char *spinner_frames[] = {
    "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
    "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
    "\xe2\xa0\x87", "\xe2\xa0\x8f",
};
#define SPINNER_FRAME_COUNT 10

typedef struct {
    guint tick_id;
    int   frame;
    char *text;
} SpinnerData;

static gboolean spinner_tick(gpointer user_data) {
    GtkWidget *label = GTK_WIDGET(user_data);
    SpinnerData *sd = g_object_get_data(G_OBJECT(label), "spinner-data");
    if (!sd) return FALSE;

    sd->frame = (sd->frame + 1) % SPINNER_FRAME_COUNT;
    char *display = g_strdup_printf("%s  %s",
                                     spinner_frames[sd->frame],
                                     sd->text ? sd->text : "");
    gtk_label_set_text(GTK_LABEL(label), display);
    g_free(display);
    return TRUE;
}

static void on_spinner_destroy(gpointer user_data) {
    SpinnerData *sd = user_data;
    if (sd->tick_id) {
        g_source_remove(sd->tick_id);
        sd->tick_id = 0;
    }
    g_free(sd->text);
    g_free(sd);
}

GtkWidget *widgets_spinner_new(const char *text) {
    GtkWidget *label = widgets_label_new("", EINK_FONT_MED);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);

    SpinnerData *sd = g_new0(SpinnerData, 1);
    sd->text = g_strdup(text);
    sd->frame = 0;
    g_object_set_data_full(G_OBJECT(label), "spinner-data", sd,
                           on_spinner_destroy);

    /* Show first frame immediately */
    char *display = g_strdup_printf("%s  %s", spinner_frames[0], text);
    gtk_label_set_text(GTK_LABEL(label), display);
    g_free(display);

    sd->tick_id = g_timeout_add(80, spinner_tick, label);
    return label;
}

void widgets_spinner_set_text(GtkWidget *spinner, const char *text) {
    SpinnerData *sd = g_object_get_data(G_OBJECT(spinner), "spinner-data");
    if (!sd) return;
    g_free(sd->text);
    sd->text = g_strdup(text);
}
