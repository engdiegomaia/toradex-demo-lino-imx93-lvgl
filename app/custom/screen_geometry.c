/*
 * Copyright 2026 Toradex
 *
 * Screen geometry normalization for the GUI-Guider generated UI.
 */

#include "screen_geometry.h"

/* Sized from lv_conf.h rather than a local literal so the panel resolution
 * has exactly one source of truth. LV_HOR_RES_MAX/LV_VER_RES_MAX are also
 * what lvgl_gui.c passes to lv_wayland_window_create(), so the screen and
 * the Wayland surface cannot drift apart. */
void screen_geometry_apply(lv_obj_t * screen)
{
    if(screen == NULL) {
        return;
    }

    lv_obj_set_size(screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
}
