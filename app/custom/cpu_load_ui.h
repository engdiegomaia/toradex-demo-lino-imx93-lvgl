/*
 * Copyright 2026 Toradex
 *
 * Binds the Cortex-A55-0 utilisation samples from cpu_load.c to the generated
 * GUI-Guider widgets, and performs the A1-lane swap: the screens and panels that
 * used to describe the Cortex-A55-1 remoteproc demo now describe A55-0, the core
 * that runs Linux and this HMI.
 *
 * Everything here runs on the LVGL thread. All retitling, repositioning and axis
 * work is applied at runtime rather than by editing app/generated/, so it
 * survives a GUI-Guider regeneration (see docs/DEVELOPMENT.md).
 */

#ifndef __CPU_LOAD_UI_H_
#define __CPU_LOAD_UI_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

/* Called from custom_init(), i.e. straight after setup_ui() has built Home.
 * Retires the A55-1 lane and turns the A1 panel into the live A55-0 panel. */
void cpu_load_ui_init(gg_ui_t * ui);

/* Called from the LVGL thread's round-robin loop. Retargets the lazily created
 * screens on first sight and pushes each new sample into the widgets. */
void cpu_load_ui_render(void);

#ifdef __cplusplus
}
#endif
#endif
