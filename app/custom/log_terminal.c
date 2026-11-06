/*
 * Copyright 2026 Toradex
 */
#include "log_terminal.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "log_frame.h"

#define LOG_TERMINAL_LINE_SPACE 2

static void style_terminal(lv_obj_t * ta)
{
    if(ta == NULL) return;

    lv_obj_set_style_text_font(ta, &lv_font_unscii_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ta, LOG_TERMINAL_LINE_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* Idle frame so the M33 terminals are not blank before START is pressed. The
 * first published frame replaces it. */
static void show_idle_frame(gg_ui_t * ui)
{
    log_frame_m33_t idle;
    char frame[512];

    memset(&idle, 0, sizeof(idle));
    snprintf(idle.state, sizeof(idle.state), "IDLE");
    snprintf(idle.event, sizeof(idle.event), "press START");
    if(log_frame_render_m33(&idle, frame, sizeof(frame)) == 0) return;

    /* Skip the leading clear marker; set_text replaces the contents anyway. */
    lv_textarea_set_text(ui->Home.container_cortexM33_ta_coreM33_log, frame + 1);
    lv_textarea_set_text(ui->Console_M33.ta_log, frame + 1);
}

void log_terminal_init(gg_ui_t * ui)
{
    if(ui == NULL) return;

    style_terminal(ui->Home.container_cortexA1_ta_coreA1_log);
    style_terminal(ui->Home.container_cortexM33_ta_coreM33_log);
    style_terminal(ui->Console_A1.ta_log);
    style_terminal(ui->Console_M33.ta_log);

    show_idle_frame(ui);
}
