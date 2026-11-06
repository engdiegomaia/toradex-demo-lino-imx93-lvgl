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

/* Called from custom_init(). Only Home exists at that point; the Console
 * screens are created on demand, so their terminals are styled lazily through
 * log_terminal_style(). */
void log_terminal_init(gg_ui_t * ui);

/* Apply the fixed-width style to one terminal. Idempotent and NULL-safe, so it
 * can be called every time text is written to the widget. */
void log_terminal_style(lv_obj_t * ta);

#endif /* LOG_TERMINAL_H */
