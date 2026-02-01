#define _POSIX_C_SOURCE 200809L
#include "brightness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 24

static int current_brightness = -1;
static char brightness_file[256] = {0};
static char init_error[512] = {0};  /* Store initialization error */

/* LIPC function pointers (dynamically loaded) */
typedef void LIPC;
typedef int LIPCcode;
#define LIPC_OK 0

static void *lipc_handle = NULL;
static LIPC *(*lipc_open)(const char *) = NULL;
static void (*lipc_close)(LIPC *) = NULL;
static LIPCcode (*lipc_set_int_property)(LIPC *, const char *, const char *, int) = NULL;
static LIPCcode (*lipc_get_int_property)(LIPC *, const char *, const char *, int *) = NULL;
static LIPC *lipc_connection = NULL;

/* Detect the correct sysfs file for this Kindle model */
static gboolean detect_brightness_file(void) {
    const char *paths[] = {
        "/sys/class/backlight/max77696-bl/brightness",     /* PW2, PW3, PW4 */
        "/sys/devices/system/fl_tps6116x/fl_tps6116x0/fl_intensity", /* PW1 */
        "/sys/class/backlight/mxc_msp430_fl/brightness",   /* Kindle Touch */
        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {
        if (access(paths[i], W_OK) == 0) {
            strncpy(brightness_file, paths[i], sizeof(brightness_file) - 1);
            return TRUE;
        }
    }

    return FALSE;
}

/* Initialize LIPC library connection */
static gboolean init_lipc(void) {
    /* Try to load liblipc.so dynamically */
    lipc_handle = dlopen("liblipc.so.1", RTLD_LAZY);
    if (!lipc_handle) {
        lipc_handle = dlopen("liblipc.so", RTLD_LAZY);
    }
    
    if (!lipc_handle) {
        snprintf(init_error, sizeof(init_error), 
                 "LIPC library not found: %s", dlerror());
        g_message("%s", init_error);
        return FALSE;
    }

    /* Load function pointers - try LipcOpenNoName first */
    LIPC *(*lipc_open_no_name)(void) = dlsym(lipc_handle, "LipcOpenNoName");
    lipc_open = dlsym(lipc_handle, "LipcOpen");
    lipc_close = dlsym(lipc_handle, "LipcClose");
    lipc_set_int_property = dlsym(lipc_handle, "LipcSetIntProperty");
    lipc_get_int_property = dlsym(lipc_handle, "LipcGetIntProperty");

    if (!lipc_close || !lipc_set_int_property || !lipc_get_int_property) {
        snprintf(init_error, sizeof(init_error),
                 "Failed to load LIPC functions: %s", dlerror());
        g_warning("%s", init_error);
        dlclose(lipc_handle);
        lipc_handle = NULL;
        return FALSE;
    }

    /* Try LipcOpenNoName first (doesn't require service name) */
    if (lipc_open_no_name) {
        lipc_connection = lipc_open_no_name();
        if (lipc_connection) {
            g_message("LIPC initialized via LipcOpenNoName");
            return TRUE;
        }
    }

    /* Fallback to LipcOpen with a service name */
    if (lipc_open) {
        lipc_connection = lipc_open("com.github.mangareader");
        if (lipc_connection) {
            g_message("LIPC initialized via LipcOpen");
            return TRUE;
        }
    }

    snprintf(init_error, sizeof(init_error),
             "Failed to open LIPC connection (tried both LipcOpenNoName and LipcOpen)");
    g_warning("%s", init_error);
    dlclose(lipc_handle);
    lipc_handle = NULL;
    return FALSE;
}

/* Read brightness using lipc (preferred) or sysfs fallback */
static int read_brightness(void) {
    /* Try LIPC first if available */
    if (lipc_connection && lipc_get_int_property) {
        int value = -1;
        LIPCcode result = lipc_get_int_property(lipc_connection, 
                                                 "com.lab126.powerd", 
                                                 "flIntensity", 
                                                 &value);
        if (result == LIPC_OK && value >= 0) {
            return value;
        }
    }

    /* Fallback to sysfs */
    FILE *f = fopen(brightness_file, "r");
    if (!f) return -1;

    int value = -1;
    if (fscanf(f, "%d", &value) != 1) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return value;
}

/* Write brightness using lipc (preferred) or sysfs fallback */
static gboolean write_brightness(int value) {
    if (value < BRIGHTNESS_MIN) value = BRIGHTNESS_MIN;
    if (value > BRIGHTNESS_MAX) value = BRIGHTNESS_MAX;

    /* Try LIPC first if available */
    if (lipc_connection && lipc_set_int_property) {
        LIPCcode result = lipc_set_int_property(lipc_connection,
                                                 "com.lab126.powerd",
                                                 "flIntensity",
                                                 value);
        if (result == LIPC_OK) {
            current_brightness = value;
            g_message("Set brightness to %d via LIPC", value);
            return TRUE;
        } else {
            g_warning("LIPC set brightness failed with code %d", result);
        }
    }

    /* Fallback to sysfs if lipc not available or failed */
    FILE *f = fopen(brightness_file, "w");
    if (!f) {
        g_warning("Failed to open sysfs brightness file");
        return FALSE;
    }

    fprintf(f, "%d\n", value);
    fclose(f);

    current_brightness = value;
    g_message("Set brightness to %d via sysfs", value);
    return TRUE;
}

gboolean brightness_init(void) {
#ifdef KINDLE
    if (!detect_brightness_file()) {
        g_warning("Could not find brightness control file");
        return FALSE;
    }

    /* Try to initialize LIPC */
    init_lipc();

    current_brightness = read_brightness();
    if (current_brightness < 0) {
        g_warning("Could not read initial brightness");
        return FALSE;
    }

    if (lipc_connection) {
        g_message("Brightness control initialized via LIPC (current: %d)", current_brightness);
    } else {
        g_message("Brightness control initialized via sysfs: %s (current: %d)", 
                  brightness_file, current_brightness);
    }
    return TRUE;
#else
    g_message("Brightness control not available on this platform");
    return FALSE;
#endif
}

int brightness_get(void) {
    if (current_brightness < 0) {
        current_brightness = read_brightness();
    }
    return current_brightness;
}

gboolean brightness_set(int level) {
    if (brightness_file[0] == '\0') return FALSE;
    
    gboolean result = write_brightness(level);
    
    /* Store error if brightness change failed and LIPC was supposed to work */
    if (!result && lipc_connection) {
        snprintf(init_error, sizeof(init_error),
                 "Brightness change to %d failed", level);
    }
    
    return result;
}

gboolean brightness_increase(void) {
    int current = brightness_get();
    if (current < 0) return FALSE;
    
    if (current >= BRIGHTNESS_MAX) return FALSE;
    return brightness_set(current + 1);
}

gboolean brightness_decrease(void) {
    int current = brightness_get();
    if (current < 0) return FALSE;
    
    if (current <= BRIGHTNESS_MIN) return FALSE;
    return brightness_set(current - 1);
}

gboolean brightness_off(void) {
    return brightness_set(BRIGHTNESS_MIN);
}

void brightness_shutdown(void) {
    /* Close LIPC connection if open */
    if (lipc_connection && lipc_close) {
        lipc_close(lipc_connection);
        lipc_connection = NULL;
    }
    
    /* Unload LIPC library */
    if (lipc_handle) {
        dlclose(lipc_handle);
        lipc_handle = NULL;
    }
}

const char* brightness_get_error(void) {
    return init_error[0] ? init_error : NULL;
}
