/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __LVGL_GUI_H__
#define __LVGL_GUI_H__

#include "basedef.h"
#include "rt_stats.h"
#include "custom.h"


#define TERMINAL_LOG_LENGTH  320
#define TERMINAL_LARGE_LOG_LENGTH  1600
/* Widget type definitions */
#define WIDGET_TYPE_DROPDOWN         1
#define WIDGET_TYPE_TERMINAL_A1      2
#define WIDGET_TYPE_TERMINAL_M33     3
#define WIDGET_TYPE_CHART_A1_SERIES0 4
#define WIDGET_TYPE_CHART_A1_SERIES1 5
#define WIDGET_TYPE_CHART_A1_SERIES2 6
#define WIDGET_TYPE_CHART_M33_SERIES0 7
#define WIDGET_TYPE_CHART_M33_SERIES1 8
#define WIDGET_TYPE_CHART_M33_SERIES2 9
#define WIDGET_TYPE_LARGE_TERMINAL_A1 10
#define WIDGET_TYPE_LARGE_TERMINAL_M33 11

typedef struct {
    int16_t *array;
    size_t length;
} IntArray;


void gui_app_init();
int gui_rf_loop(void);

void stop_refresh_anim(int cortexID);

int notify_update_gui_latency(uint8_t am_idx, rt_latency_shm_stat *p);

int start_lvgl_gui(void);

int stop_lvgl_gui(void);

int query_operation_status(uint8_t *pboot_type, uint16_t *pflag);


#endif
