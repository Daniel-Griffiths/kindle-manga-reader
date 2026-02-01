#include "keyboard.h"
#include "widgets.h"
#include <string.h>

#define KEY_HEIGHT (100 / EINK_SCALE)

typedef struct {
    GtkEntry  *target;
    GCallback  on_submit;
    gpointer   submit_data;
    gboolean   shifted;
    gboolean   symbols;
    GtkWidget *rows_alpha;
    GtkWidget *rows_symbol;
    GtkWidget *shift_btn;
} KeyboardData;

/* ── helpers ─────────────────────────────────────────────────────── */

static void set_button_font(GtkWidget *btn, const char *font) {
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(btn));
    if (GTK_IS_LABEL(child)) {
        PangoFontDescription *fd = pango_font_description_from_string(font);
        gtk_widget_modify_font(child, fd);
        pango_font_description_free(fd);
    }
}

static GtkWidget *make_key(const char *label, int expand) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NORMAL);
    gtk_widget_set_size_request(btn, -1, KEY_HEIGHT);
    set_button_font(btn, EINK_FONT_MED);
    if (expand)
        g_object_set_data(G_OBJECT(btn), "kb-expand", GINT_TO_POINTER(1));
    return btn;
}

/* ── callbacks ───────────────────────────────────────────────────── */

static void update_shift_label(KeyboardData *kb) {
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(kb->shift_btn));
    if (GTK_IS_LABEL(child))
        gtk_label_set_text(GTK_LABEL(child), kb->shifted ? "⇧" : "⇧");
}

static void on_char_key(GtkWidget *btn, gpointer user_data) {
    KeyboardData *kb = user_data;
    const char *label = gtk_button_get_label(GTK_BUTTON(btn));
    if (!label || !label[0]) return;

    char buf[8];
    if (kb->shifted && !kb->symbols) {
        /* uppercase the first UTF-8 char */
        gunichar ch = g_utf8_get_char(label);
        ch = g_unichar_toupper(ch);
        int len = g_unichar_to_utf8(ch, buf);
        buf[len] = '\0';
    } else {
        g_strlcpy(buf, label, sizeof(buf));
    }

    gint pos = gtk_editable_get_position(GTK_EDITABLE(kb->target));
    gtk_editable_insert_text(GTK_EDITABLE(kb->target), buf, -1, &pos);
    gtk_editable_set_position(GTK_EDITABLE(kb->target), pos);

    /* single-shot shift */
    if (kb->shifted && !kb->symbols) {
        kb->shifted = FALSE;
        update_shift_label(kb);
    }
}

static void on_backspace(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    KeyboardData *kb = user_data;
    gint pos = gtk_editable_get_position(GTK_EDITABLE(kb->target));
    if (pos > 0) {
        gtk_editable_delete_text(GTK_EDITABLE(kb->target), pos - 1, pos);
        gtk_editable_set_position(GTK_EDITABLE(kb->target), pos - 1);
    }
}

static void on_space(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    KeyboardData *kb = user_data;
    gint pos = gtk_editable_get_position(GTK_EDITABLE(kb->target));
    gtk_editable_insert_text(GTK_EDITABLE(kb->target), " ", -1, &pos);
    gtk_editable_set_position(GTK_EDITABLE(kb->target), pos);
}

static void on_enter(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    KeyboardData *kb = user_data;
    if (kb->on_submit) {
        ((void (*)(gpointer))kb->on_submit)(kb->submit_data);
    }
}

static void on_shift(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    KeyboardData *kb = user_data;
    kb->shifted = !kb->shifted;
    update_shift_label(kb);
}

static void on_toggle_symbols(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    KeyboardData *kb = user_data;
    kb->symbols = !kb->symbols;
    if (kb->symbols) {
        gtk_widget_hide(kb->rows_alpha);
        gtk_widget_show(kb->rows_symbol);
    } else {
        gtk_widget_hide(kb->rows_symbol);
        gtk_widget_show(kb->rows_alpha);
    }
    kb->shifted = FALSE;
    update_shift_label(kb);
}

static void on_data_destroy(gpointer user_data) {
    g_free(user_data);
}

/* ── row builders ────────────────────────────────────────────────── */

static GtkWidget *build_char_row(const char **keys, int count,
                                  KeyboardData *kb) {
    GtkWidget *hbox = gtk_hbox_new(TRUE, 2);
    for (int i = 0; i < count; i++) {
        GtkWidget *btn = make_key(keys[i], 0);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_char_key), kb);
        gtk_box_pack_start(GTK_BOX(hbox), btn, TRUE, TRUE, 0);
    }
    return hbox;
}

static GtkWidget *build_alpha_rows(KeyboardData *kb) {
    GtkWidget *vbox = gtk_vbox_new(FALSE, 2);

    /* Row 1: q w e r t y u i o p */
    const char *r1[] = {"q","w","e","r","t","y","u","i","o","p"};
    gtk_box_pack_start(GTK_BOX(vbox), build_char_row(r1, 10, kb),
                       FALSE, FALSE, 0);

    /* Row 2: a s d f g h j k l */
    const char *r2[] = {"a","s","d","f","g","h","j","k","l"};
    gtk_box_pack_start(GTK_BOX(vbox), build_char_row(r2, 9, kb),
                       FALSE, FALSE, 0);

    /* Row 3: shift z x c v b n m backspace */
    {
        GtkWidget *hbox = gtk_hbox_new(FALSE, 2);

        kb->shift_btn = make_key("⇧", 0);
        gtk_widget_set_size_request(kb->shift_btn, 120, KEY_HEIGHT);
        g_signal_connect(kb->shift_btn, "clicked",
                         G_CALLBACK(on_shift), kb);
        gtk_box_pack_start(GTK_BOX(hbox), kb->shift_btn, FALSE, FALSE, 0);

        const char *r3[] = {"z","x","c","v","b","n","m"};
        for (int i = 0; i < 7; i++) {
            GtkWidget *btn = make_key(r3[i], 0);
            g_signal_connect(btn, "clicked", G_CALLBACK(on_char_key), kb);
            gtk_box_pack_start(GTK_BOX(hbox), btn, TRUE, TRUE, 0);
        }

        GtkWidget *bksp = make_key("⌫", 0);
        gtk_widget_set_size_request(bksp, 120, KEY_HEIGHT);
        g_signal_connect(bksp, "clicked", G_CALLBACK(on_backspace), kb);
        gtk_box_pack_start(GTK_BOX(hbox), bksp, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    /* Row 4: 123 | Space | Enter */
    {
        GtkWidget *hbox = gtk_hbox_new(FALSE, 2);

        GtkWidget *sym_btn = make_key("123", 0);
        gtk_widget_set_size_request(sym_btn, 120, KEY_HEIGHT);
        g_signal_connect(sym_btn, "clicked",
                         G_CALLBACK(on_toggle_symbols), kb);
        gtk_box_pack_start(GTK_BOX(hbox), sym_btn, FALSE, FALSE, 0);

        GtkWidget *space = make_key("Space", 1);
        g_signal_connect(space, "clicked", G_CALLBACK(on_space), kb);
        gtk_box_pack_start(GTK_BOX(hbox), space, TRUE, TRUE, 0);

        GtkWidget *enter = make_key("Enter", 0);
        gtk_widget_set_size_request(enter, 140, KEY_HEIGHT);
        g_signal_connect(enter, "clicked", G_CALLBACK(on_enter), kb);
        gtk_box_pack_start(GTK_BOX(hbox), enter, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    return vbox;
}

static GtkWidget *build_symbol_rows(KeyboardData *kb) {
    GtkWidget *vbox = gtk_vbox_new(FALSE, 2);

    /* Row 1: 1 2 3 4 5 6 7 8 9 0 */
    const char *r1[] = {"1","2","3","4","5","6","7","8","9","0"};
    gtk_box_pack_start(GTK_BOX(vbox), build_char_row(r1, 10, kb),
                       FALSE, FALSE, 0);

    /* Row 2: - / : ; ( ) $ & @ " */
    const char *r2[] = {"-","/",":",";","(",")","$","&","@","\""};
    gtk_box_pack_start(GTK_BOX(vbox), build_char_row(r2, 10, kb),
                       FALSE, FALSE, 0);

    /* Row 3: # . , ? ! ' backspace */
    {
        GtkWidget *hbox = gtk_hbox_new(FALSE, 2);

        const char *r3[] = {"#",".",",","?","!","'"};
        for (int i = 0; i < 6; i++) {
            GtkWidget *btn = make_key(r3[i], 0);
            g_signal_connect(btn, "clicked", G_CALLBACK(on_char_key), kb);
            gtk_box_pack_start(GTK_BOX(hbox), btn, TRUE, TRUE, 0);
        }

        GtkWidget *bksp = make_key("⌫", 0);
        gtk_widget_set_size_request(bksp, 120, KEY_HEIGHT);
        g_signal_connect(bksp, "clicked", G_CALLBACK(on_backspace), kb);
        gtk_box_pack_start(GTK_BOX(hbox), bksp, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    /* Row 4: ABC | Space | Enter */
    {
        GtkWidget *hbox = gtk_hbox_new(FALSE, 2);

        GtkWidget *abc_btn = make_key("ABC", 0);
        gtk_widget_set_size_request(abc_btn, 120, KEY_HEIGHT);
        g_signal_connect(abc_btn, "clicked",
                         G_CALLBACK(on_toggle_symbols), kb);
        gtk_box_pack_start(GTK_BOX(hbox), abc_btn, FALSE, FALSE, 0);

        GtkWidget *space = make_key("Space", 1);
        g_signal_connect(space, "clicked", G_CALLBACK(on_space), kb);
        gtk_box_pack_start(GTK_BOX(hbox), space, TRUE, TRUE, 0);

        GtkWidget *enter = make_key("Enter", 0);
        gtk_widget_set_size_request(enter, 140, KEY_HEIGHT);
        g_signal_connect(enter, "clicked", G_CALLBACK(on_enter), kb);
        gtk_box_pack_start(GTK_BOX(hbox), enter, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    return vbox;
}

/* ── public API ──────────────────────────────────────────────────── */

GtkWidget *keyboard_new(GtkEntry *target, GCallback on_submit, gpointer data) {
    KeyboardData *kb = g_new0(KeyboardData, 1);
    kb->target      = target;
    kb->on_submit   = on_submit;
    kb->submit_data = data;
    kb->shifted     = FALSE;
    kb->symbols     = FALSE;

    GtkWidget *container = gtk_vbox_new(FALSE, 0);
    g_object_set_data_full(G_OBJECT(container), "kb-data", kb,
                           on_data_destroy);

    kb->rows_alpha  = build_alpha_rows(kb);
    kb->rows_symbol = build_symbol_rows(kb);

    gtk_box_pack_start(GTK_BOX(container), kb->rows_alpha,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), kb->rows_symbol, FALSE, FALSE, 0);

    /* Show all children so the container just needs show/hide */
    gtk_widget_show_all(container);

    /* Symbol rows start hidden */
    gtk_widget_hide(kb->rows_symbol);

    return container;
}
