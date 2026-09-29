/*
 * Copyright 2026 Toradex
 *
 * Home screen branding quadrant.
 *
 * The left slot of Home's panel row used to be a static Cortex-A55-0 spec sheet
 * (container_cortexA0). Once the middle panel became the *live* A55-0 readout
 * that sheet was redundant, so the slot is reused for the platform lockups:
 * Torizon OS, LVGL and Toradex.
 *
 * Applied at runtime rather than by editing app/generated/, so it survives a
 * GUI-Guider regeneration (see docs/DEVELOPMENT.md).
 */

#ifndef __BRANDING_H_
#define __BRANDING_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

/* Called from cpu_load_ui_init(), i.e. straight after setup_ui() built Home. */
void branding_build_home(gg_ui_t * ui);

#ifdef __cplusplus
}
#endif
#endif
