/*
 * Copyright 2026 Toradex
 *
 * Screen geometry normalization for the GUI-Guider generated UI.
 */

#ifndef SCREEN_GEOMETRY_H
#define SCREEN_GEOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Force a generated root screen to the panel's native resolution.
 *
 * GUI-Guider emits every screen at the project canvas size, but it does not
 * size the root object itself. Without this the root keeps LVGL's default
 * geometry and Wayland clips the surface.
 *
 * Deliberately sets an explicit size rather than a style transform: applying
 * a transform to an LVGL root screen makes Wayland clip its contents.
 *
 * This lives under app/custom/ because app/generated/ is overwritten on every
 * GUI-Guider regeneration. The single call site inside gg_utils.c is the one
 * piece that regeneration can drop — and because this header declares the
 * symbol, dropping it surfaces as a link error rather than a silent revert.
 *
 * @param screen Root screen object to size. NULL is ignored.
 */
void screen_geometry_apply(lv_obj_t * screen);

#ifdef __cplusplus
}
#endif
#endif /* SCREEN_GEOMETRY_H */
