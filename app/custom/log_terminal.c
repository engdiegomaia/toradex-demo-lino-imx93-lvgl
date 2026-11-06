/*
 * Copyright 2026 Toradex
 */
#include "log_terminal.h"

#include "lvgl.h"

#define LOG_TERMINAL_LINE_SPACE 2

static void style_terminal(lv_obj_t * ta)
{
    if(ta == NULL) return;

    lv_obj_set_style_text_font(ta, &lv_font_unscii_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ta, LOG_TERMINAL_LINE_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void log_terminal_init(gg_ui_t * ui)
{
    if(ui == NULL) return;

    style_terminal(ui->Home.container_cortexA1_ta_coreA1_log);
    style_terminal(ui->Home.container_cortexM33_ta_coreM33_log);
    style_terminal(ui->Console_A1.ta_log);
    style_terminal(ui->Console_M33.ta_log);
}
