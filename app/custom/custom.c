/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include "lvgl.h"
#include "custom.h"
#include "gg_event.h"
#include "cpu_load_ui.h"
#include "log_terminal.h"


/*********************
 *      DEFINES
 *********************/
#define MAX_SERIES 3
#define LABEL_BUF_SIZE 32

#if GG_USE_SIMULATOR
#define pthread_mutex_lock(x)   ((void)0)
#define pthread_mutex_unlock(x) ((void)0)
#define PTHREAD_MUTEX_INITIALIZER 0
typedef int pthread_mutex_t;
#else
#include <pthread.h>
#endif

/**********************
 *      TYPEDEFS
 **********************/
int cortexID = 0;
int applicationID = 0;
OperationInfo * realTime_operation = NULL;
static pthread_mutex_t g_operation_info_mutex = PTHREAD_MUTEX_INITIALIZER;

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**
 * Create a demo application
 */

void custom_init(gg_ui_t *ui)
{
    init_operation_info();

    /* Home exists by now (setup_ui() built it), so the A1 lane can be swapped
     * over to Cortex-A55-0 before the first frame is drawn. */
    cpu_load_ui_init(ui);

    /* Fixed-width font so the log columns line up. */
    log_terminal_init(ui);
}

/**
 * @brief Event handler for radio button selection
 * 
 * This function handles radio button selection events, updating the active button state
 * and setting the appropriate visual indicators. It also updates the global cortexID
 * variable based on the selected radio button.
 * 
 * @param e The event data structure
 */
void radio_event_handler(lv_event_t * e)
{
    uint32_t * active_id = lv_event_get_user_data(e);
    lv_obj_t * cont = lv_event_get_current_target(e);
    lv_obj_t * act_cb = lv_event_get_target(e);
    lv_obj_t * old_cb = lv_obj_get_child(cont, *active_id);

    /*Do nothing if the container was clicked*/
    if(act_cb == cont) return;

    lv_obj_clear_state(old_cb, LV_STATE_CHECKED);   /*Uncheck the previous radio button*/
    lv_obj_add_state(act_cb, LV_STATE_CHECKED);     /*Uncheck the current radio button*/

    *active_id = lv_obj_get_index(act_cb);
    if(*active_id == 0) {
        lv_obj_set_style_border_color(guider_ui.Home.container_cortexA1, lv_color_hex(0x2563EB), LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(guider_ui.Home.container_cortexM33, lv_color_hex(0x27364A), LV_PART_MAIN|LV_STATE_DEFAULT);
        cortexID = 0;
    } else if (*active_id == 1)
    {
        lv_obj_set_style_border_color(guider_ui.Home.container_cortexM33, lv_color_hex(0x2563EB), LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(guider_ui.Home.container_cortexA1, lv_color_hex(0x27364A), LV_PART_MAIN|LV_STATE_DEFAULT);
        cortexID = 1;
    }
}

void stop_refresh_anim(int cortexID)
{
    /* cortexID 0 used to reset the A55-1 remoteproc panel to "Pending". That
     * panel now reports Cortex-A55-0, which is never pending, so only the M33
     * lane has an idle state to fall back to. */
    if (cortexID == 1) {
        lv_obj_set_style_img_recolor_opa(guider_ui.Home.container_cortexM33_coreM33_icon, 100, LV_PART_MAIN);
        lv_label_set_text(guider_ui.Home.container_cortexM33_coreM33_status, "Pending");
        gg_pause_anim(guider_ui.Home.container_cortexM33_coreM33_run, anim_rotation_cb);
    }
}

void init_operation_info() {
    pthread_mutex_lock(&g_operation_info_mutex);

    if (!realTime_operation)
        realTime_operation = (OperationInfo *) malloc(sizeof(OperationInfo));

    if(realTime_operation) {
        realTime_operation->coreId = 0;
        realTime_operation->appName = 0;
        realTime_operation->status = 0;
        realTime_operation->trigger = 0;
    }

    pthread_mutex_unlock(&g_operation_info_mutex);
}

int update_operation_info(int16_t coreid, int16_t appname, int16_t status, int16_t trigger)
{
    int ret = -1;

    pthread_mutex_lock(&g_operation_info_mutex);

    if(realTime_operation) {
        realTime_operation->coreId = coreid;
        realTime_operation->appName = appname;
        realTime_operation->status = status;
        realTime_operation->trigger = trigger;
        ret = 0;
    }

    pthread_mutex_unlock(&g_operation_info_mutex);
    return ret;
}

OperationInfo* get_operation_info()
{
    return realTime_operation;
}

int copy_operation_info(OperationInfo *info)
{
    int ret = -1;

    if (!info)
        return -1;

    pthread_mutex_lock(&g_operation_info_mutex);

    if (realTime_operation) {
        *info = *realTime_operation;
        ret = 0;
    }

    pthread_mutex_unlock(&g_operation_info_mutex);
    return ret;
}

int update_operation_status(int16_t trigger)
{
    int ret = -1;

    pthread_mutex_lock(&g_operation_info_mutex);

    if(realTime_operation) {
        realTime_operation->trigger = trigger;
        ret = 0;
    }

    pthread_mutex_unlock(&g_operation_info_mutex);
    return ret;
}

void chart_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * chart = lv_event_get_target(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_invalidate(chart);
    }
    else if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        int32_t * s = lv_event_get_param(e);
        *s = LV_MAX(*s, 200);
    }
    else if(code == LV_EVENT_DRAW_POST_END) {
        int32_t id = lv_chart_get_pressed_point(chart);
        if(id == LV_CHART_POINT_NONE) return;

        LV_LOG_USER("Selected point %d", (int)id);

        static char buf_pool[MAX_SERIES][LABEL_BUF_SIZE];

        /* One callback serves both benchmarking charts: BenchMarking_A1 plots
         * A55-0 utilisation in percent, BenchMarking_M33 still plots interrupt
         * latency in nanoseconds. */
        const char * unit = (chart == guider_ui.BenchMarking_A1.chart_bm) ? "%" : "ns";

        lv_chart_series_t * ser = lv_chart_get_series_next(chart, NULL);
        int series_index = 0;

        while(ser && series_index < MAX_SERIES) {
            lv_point_t p;
            lv_chart_get_point_pos_by_id(chart, ser, id, &p);

            int32_t * y_array = lv_chart_get_y_array(chart, ser);
            int32_t value = y_array[id];

            lv_snprintf(buf_pool[series_index], LABEL_BUF_SIZE, LV_SYMBOL_DUMMY"%d %s", value, unit);

            lv_draw_rect_dsc_t draw_rect_dsc;
            lv_draw_rect_dsc_init(&draw_rect_dsc);
            draw_rect_dsc.bg_color = lv_color_black();
            draw_rect_dsc.bg_opa = LV_OPA_50;
            draw_rect_dsc.radius = 3;
            draw_rect_dsc.bg_image_src = buf_pool[series_index];
            draw_rect_dsc.bg_image_recolor = lv_color_white();

            lv_area_t chart_obj_coords;
            lv_obj_get_coords(chart, &chart_obj_coords);
            lv_area_t a;
            a.x1 = chart_obj_coords.x1 + p.x - 50;
            a.x2 = chart_obj_coords.x1 + p.x + 50;
            a.y1 = chart_obj_coords.y1 + p.y - 30;
            a.y2 = chart_obj_coords.y1 + p.y - 10;

            lv_layer_t * layer = lv_event_get_layer(e);
            lv_draw_rect(layer, &draw_rect_dsc, &a);

            ser = lv_chart_get_series_next(chart, ser);
            series_index++;
        }
    }
    else if(code == LV_EVENT_RELEASED) {
        lv_obj_invalidate(chart);
    }
}
