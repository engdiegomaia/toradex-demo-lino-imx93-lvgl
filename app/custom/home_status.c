/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms.
*/

#include "home_status.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "custom.h"
#include "events/gg_event.h"
#include "real_time_edge/basedef.h"

/* Colors kept in sync with the palette used in setup_Home() so the app never
 * has to depend on runtime widget-state introspection. */
#define STATUS_COLOR_IDLE     0xF59E0B  /* amber — matches GUI-Guider default */
#define STATUS_COLOR_BOOTING  0x2563EB  /* blue */
#define STATUS_COLOR_RUNNING  0x22C55E  /* green */
#define STATUS_COLOR_FAILED   0xEF4444  /* red */

#define STATUS_TEXT_IDLE     "Pending"
#define STATUS_TEXT_BOOTING  "Booting"
#define STATUS_TEXT_RUNNING  "Running"
#define STATUS_TEXT_FAILED   "Failed"

typedef struct {
    lv_obj_t *icon;
    lv_obj_t *status_label;
    lv_obj_t *anim_target;
    lv_obj_t *lt_container;   /* Latency Test button — hidden for hello_world */
    home_core_state_t last_state;
} home_core_widgets_t;

static home_core_widgets_t s_widgets[AM_MAX];
static int s_widgets_bound = 0;

static void bind_widgets(void)
{
    s_widgets[AC_IDX].icon         = guider_ui.Home.container_cortexA1_coreA1_icon;
    s_widgets[AC_IDX].status_label = guider_ui.Home.container_cortexA1_coreA1_status;
    s_widgets[AC_IDX].anim_target  = guider_ui.Home.container_cortexA1_coreA1_run;
    s_widgets[AC_IDX].lt_container = guider_ui.Home.container_cortexA1_coreA1_LT;
    s_widgets[AC_IDX].last_state   = (home_core_state_t)-1;

    s_widgets[MC_IDX].icon         = guider_ui.Home.container_cortexM33_coreM33_icon;
    s_widgets[MC_IDX].status_label = guider_ui.Home.container_cortexM33_coreM33_status;
    s_widgets[MC_IDX].anim_target  = guider_ui.Home.container_cortexM33_coreM33_run;
    s_widgets[MC_IDX].lt_container = guider_ui.Home.container_cortexM33_coreM33_LT;
    s_widgets[MC_IDX].last_state   = (home_core_state_t)-1;

    s_widgets_bound = 1;
}

static home_core_state_t derive_state(uint8_t am_idx, int op_status_for_core)
{
    core_session_t *ps = get_core_session(am_idx);
    if (!ps) return HOME_STATE_IDLE;

    if (ps->boot_fail)  return HOME_STATE_FAILED;
    if (ps->boot_flag)  return HOME_STATE_RUNNING;
    if (op_status_for_core) return HOME_STATE_BOOTING;
    return HOME_STATE_IDLE;
}

static void paint_state(int am_idx, home_core_state_t state)
{
    home_core_widgets_t *w = &s_widgets[am_idx];

    if (!w->icon || !w->status_label || !w->anim_target) return;

    /* Always re-assert LT-button visibility from the observed state, even
     * when the coarse HOME_STATE didn't change. The Start-click handler in
     * gg_event_Home.c hides LT for hello_world at click time; the observed
     * state may not have transitioned yet (still IDLE when the click fires),
     * so relying on the last_state gate below would trap LT hidden forever. */
    if (w->lt_container) {
        if (state == HOME_STATE_IDLE || state == HOME_STATE_FAILED)
            lv_obj_remove_flag(w->lt_container, LV_OBJ_FLAG_HIDDEN);
    }

    if (state == w->last_state) return;

    uint32_t color;
    const char *text;

    switch (state) {
    case HOME_STATE_RUNNING: color = STATUS_COLOR_RUNNING; text = STATUS_TEXT_RUNNING; break;
    case HOME_STATE_BOOTING: color = STATUS_COLOR_BOOTING; text = STATUS_TEXT_BOOTING; break;
    case HOME_STATE_FAILED:  color = STATUS_COLOR_FAILED;  text = STATUS_TEXT_FAILED;  break;
    case HOME_STATE_IDLE:
    default:                 color = STATUS_COLOR_IDLE;    text = STATUS_TEXT_IDLE;    break;
    }

    lv_label_set_text(w->status_label, text);
    lv_obj_set_style_text_color(w->status_label, lv_color_hex(color),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(w->icon, lv_color_hex(color),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor_opa(w->icon,
                                       state == HOME_STATE_RUNNING ? 255 : 100,
                                       LV_PART_MAIN);

    /* Spin only while running; stop the animation for every other state.
     * Mirrors the anim setup in gg_event_Home.c click handler. */
    if (state == HOME_STATE_RUNNING) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_duration(&anim, 1000);
        lv_anim_set_delay(&anim, 0);
        lv_anim_set_path_cb(&anim, lv_anim_path_linear);
        lv_anim_set_repeat_count(&anim, -1);
        lv_anim_set_repeat_delay(&anim, 0);
        lv_anim_set_var(&anim, w->anim_target);
        lv_anim_set_exec_cb(&anim, anim_rotation_cb);
        lv_anim_set_values(&anim, 0, 3600);
        lv_anim_start(&anim);
    } else {
        gg_pause_anim(w->anim_target, anim_rotation_cb);
    }

    w->last_state = state;
}

void home_status_tick(void)
{
    if (!s_widgets_bound) bind_widgets();

    /* OperationInfo tells us which core the user asked to start most recently.
     * If op->status == 1 for this coreId, and the backend hasn't yet set the
     * boot_flag / boot_fail, we're in the transient BOOTING window. */
    OperationInfo op = {0};
    int have_op = (copy_operation_info(&op) == 0);

    for (uint8_t am_idx = 0; am_idx < AM_MAX; am_idx++) {
        int op_starting = 0;
        if (have_op && op.status == 1 && op.coreId == am_idx)
            op_starting = 1;

        paint_state(am_idx, derive_state(am_idx, op_starting));
    }
}
