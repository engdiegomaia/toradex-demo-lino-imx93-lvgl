/*
 * Copyright 2026 Toradex
 *
 * Runtime overrides for static text that the generated Home screen inherited
 * from the NXP demo (board name, core title). Done here so it survives a GUI
 * Guider regeneration.
 */
#ifndef HOME_LABELS_H
#define HOME_LABELS_H

#include "gui_guider.h"

/* Called from custom_init(); Home exists by then. */
void home_labels_init(gg_ui_t * ui);

#endif /* HOME_LABELS_H */
