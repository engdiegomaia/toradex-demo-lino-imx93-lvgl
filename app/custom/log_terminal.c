/*
 * Copyright 2026 Toradex
 */
#include "log_terminal.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "log_frame.h"

#define LOG_TERMINAL_LINE_SPACE 2

void log_terminal_style(lv_obj_t * ta)
{
    if(ta == NULL) return;

    /* Skip when already styled: re-setting a style invalidates the widget. */
    if(lv_obj_get_style_text_font(ta, LV_PART_MAIN) == &lv_font_unscii_16) return;

    lv_obj_set_style_text_font(ta, &lv_font_unscii_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ta, LOG_TERMINAL_LINE_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* Idle frame so the M33 terminal is not blank before START is pressed. The
 * first published frame replaces it. */
static void show_idle_frame(lv_obj_t * ta)
{
    log_frame_m33_t idle;
    char frame[512];

    if(ta == NULL) return;

    memset(&idle, 0, sizeof(idle));
    snprintf(idle.state, sizeof(idle.state), "IDLE");
    snprintf(idle.event, sizeof(idle.event), "press START");
    if(log_frame_render_m33(&idle, frame, sizeof(frame)) == 0) return;

    /* Skip the leading clear marker; set_text replaces the contents anyway. */
    lv_textarea_set_text(ta, frame + 1);
}

void log_terminal_init(gg_ui_t * ui)
{
    if(ui == NULL) return;

    /* Only Home is built at this point. Console_* widgets are still NULL, and
     * passing NULL to an lv_textarea_* call trips LV_ASSERT_NULL, which halts
     * the GUI thread. */
    log_terminal_style(ui->Home.container_cortexA1_ta_coreA1_log);
    log_terminal_style(ui->Home.container_cortexM33_ta_coreM33_log);

    show_idle_frame(ui->Home.container_cortexM33_ta_coreM33_log);
}
