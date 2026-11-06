/*
 * Copyright 2026 Toradex
 *
 * Monospace styling for the on-screen log terminals.
 *
 * The generated screens use a proportional font, which breaks the column
 * alignment of the log rows. This applies a fixed-width font at runtime so it
 * survives a GUI Guider regeneration.
 */
#ifndef LOG_TERMINAL_H
#define LOG_TERMINAL_H

#include "gui_guider.h"

/* Called from custom_init() once every screen has been built. */
void log_terminal_init(gg_ui_t * ui);

#endif /* LOG_TERMINAL_H */
