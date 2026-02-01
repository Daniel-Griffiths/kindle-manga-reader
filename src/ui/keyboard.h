#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <gtk/gtk.h>

GtkWidget *keyboard_new(GtkEntry *target, GCallback on_submit, gpointer data);

#endif /* KEYBOARD_H */
