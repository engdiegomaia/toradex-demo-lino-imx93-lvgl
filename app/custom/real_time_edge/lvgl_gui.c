/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

 /*********************
 *      INCLUDES
 *********************/
#include "ram_console.h"
#include "log_tools.h"
#include "net_tools.h"
#include "lvgl_gui.h"

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE /* needed for usleep() */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <poll.h>
#include "lvgl.h"
#include "gui_guider.h"
#include "custom.h"
#include "home_status.h"
#include "cpu_load_ui.h"
#include <sys/time.h>
#include "gg_utils.h"

gg_ui_t guider_ui;
#define TERMINAL_LOG_LENGTH  320
#define TERMINAL_LARGE_LOG_LENGTH  1600

static pthread_t slvgl_gui_thread;
static int srun_flag;
static int s_gui_thread_started;
static pthread_mutex_t s_gui_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static char txt_log_a1[TERMINAL_LOG_LENGTH + 1];
static char txt_log_m33[TERMINAL_LOG_LENGTH + 1];
static char large_txt_log_a1[TERMINAL_LARGE_LOG_LENGTH + 1];
static char large_txt_log_m33[TERMINAL_LARGE_LOG_LENGTH + 1];
static volatile int s_stop_refresh_pending[2];
static pthread_mutex_t s_rt_latency_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_gui_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_gui_init_cond = PTHREAD_COND_INITIALIZER;
static int s_gui_init_done;
static int s_gui_init_status;

static void hal_init(void);


void gui_app_init(void)
{
    /* Initialize LVGL */
    lv_init();

    /* Initialize the HAL (display, input devices) for LVGL */
    hal_init();

    /* Create a GUI-Guider app */
    setup_ui(&guider_ui);
    custom_init(&guider_ui);
}

/**
 * @brief Processes GUI rendering and window events
 * 
 * This function handles the LVGL timer events for the Wayland backend.
 * It processes rendering cycles and checks if windows are still open.
 * 
 * @return 0 if processing continues normally, -1 if all windows are closed
 */
int gui_rf_loop(void)
{
#if LV_USE_WAYLAND
    bool completed;
	completed = lv_wayland_timer_handler();
	if (completed) {
		/* wait only if the cycle was completed */
		// usleep(LV_DEF_REFR_PERIOD * 1000);
		return 0;
	}
	/* Run until the last window closes */
	if (!lv_wayland_window_is_open(NULL)) {
		return -1;
	}
#else
	lv_timer_handler();
#endif
	return 0;
}

static void hal_init(void)
{
    lv_display_t * disp;
    lv_indev_t * touch;

#if LV_USE_WAYLAND
    disp = lv_wayland_window_create(LV_HOR_RES_MAX, LV_VER_RES_MAX, "GUI Guider", NULL);
    /* Kiosk mode: request a fullscreen xdg_toplevel so Weston skips the
     * client-side title bar entirely. Combined with panel-position=none in
     * weston.ini this yields an unbroken app surface. */
    lv_wayland_window_set_fullscreen(disp, true);
    /* Do NOT override the display color format here. lv_wl_shm.c already
     * negotiates WL_SHM_FORMAT_ARGB8888 when LV_COLOR_DEPTH == 32 (see
     * shm_format(): "Wayland compositors MUST support ARGB8888") and it
     * handles alpha premultiplication internally at flush time. Forcing
     * LV_COLOR_FORMAT_ARGB8888 after lv_wayland_window_create() has already
     * bound its buffer pool desyncs the display's format from the shm
     * buffer's, which is a candidate cause of the all-black surface on
     * kiosk-shell. */
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
#if LV_USE_GESTURE_RECOGNITION
    touch = lv_wayland_get_touchscreen(disp);
    lv_indev_set_group(touch, g);
    lv_indev_set_pinch_up_threshold(touch, 1.5);
    lv_indev_set_pinch_down_threshold(touch, 0.75);
    lv_indev_set_rotation_rad_threshold(touch, 0.2);
#else
    lv_indev_set_group(lv_wayland_get_pointer(disp), g);
#endif
#elif LV_USE_LINUX_DRM
    disp = lv_linux_drm_create();

#if LV_USE_EVDEV
    touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, LV_EVDEV_DEVICE);
    if(touch != NULL) {
        lv_indev_set_display(touch, disp);

        /* Set the cursor icon */
        LV_IMAGE_DECLARE(mouse_cursor_icon);
        lv_obj_t * cursor_obj = lv_image_create(lv_screen_active());
        lv_image_set_src(cursor_obj, &mouse_cursor_icon);
        lv_indev_set_cursor(touch, cursor_obj);
    } else {
        printf("Warnning: Can't open %s device, please run 'evtest' to check.\n", LV_EVDEV_DEVICE);
    }
#endif

    lv_linux_drm_set_file(disp, LV_LINUX_DRM_CARD, -1);
#else
#error Unsupported Backend
#endif
}

/**
 * @brief Adds text to a terminal widget with overflow handling
 * 
 * This function adds text to a terminal (text area) widget and manages the text buffer.
 * If the combined length of existing text and new text exceeds the terminal's capacity,
 * it replaces the entire content with just the new text.
 * 
 * @param txt_log Pointer to the buffer storing the terminal's text content
 * @param isLarge Flag indicating if this is a large terminal (0 for standard, non-zero for large)
 * @param ta Pointer to the text area object
 * @param txt_in Pointer to the new text to be added
 */
static void terminal_add(char* txt_log, int8_t isLarge, lv_obj_t * ta, const char * txt_in)
{
    if(ta == NULL) return;                /*Check if the window is exists*/
    int terninal_length = 0;
    if(isLarge == 0) {
        terninal_length = TERMINAL_LOG_LENGTH;
    } else {
        terninal_length = TERMINAL_LARGE_LOG_LENGTH;
    }

    uint16_t txt_len = strlen(txt_in);
    uint16_t old_len = strlen(txt_log);

    if(old_len + txt_len > terninal_length) {
        lv_textarea_set_text(ta, txt_in);
        memset(txt_log, '\0', terninal_length + 1);
    } else {
        lv_textarea_add_text(ta, txt_in);
        memcpy(&txt_log[old_len], txt_in, txt_len);
        txt_log[old_len + txt_len] = '\0';
    }
}

/**
 * @brief Updates widget value data based on widget object
 * 
 * This function updates various UI widgets with new data. It handles different
 * widget types including dropdowns, text areas, and charts directly using the widget object.
 * 
 * @param widget Pointer to the widget object to sync
 * @param new_data Pointer to the new data to be displayed
 * @param widget_type Type identifier for the widget or widget item
 * 
 * @return Always returns 0 (success)
 */
static int sync_widget_value(lv_obj_t *widget, void *new_data, int widget_type)
{
    if (!lv_obj_is_valid(widget) || lv_obj_get_screen(widget) != lv_scr_act()) {
        return 0;
    }

    switch (widget_type)
    {
    case WIDGET_TYPE_DROPDOWN:
        lv_dropdown_set_options(widget, new_data);
        break;
    case WIDGET_TYPE_TERMINAL_A1:
        terminal_add(txt_log_a1, 0, widget, new_data);
        break;
    case WIDGET_TYPE_TERMINAL_M33:
        terminal_add(txt_log_m33, 0, widget, new_data);
        break;
    case WIDGET_TYPE_CHART_M33_SERIES0:
        {
            IntArray *data = (IntArray *)new_data;
            size_t len = data->length;
            for (size_t i = 0; i < len; i++) {
                lv_chart_set_next_value(widget, guider_ui.BenchMarking_M33.chart_bm_ser_1, data->array[i]);
            }
            lv_chart_refresh(widget);
        }
        break;
    case WIDGET_TYPE_CHART_M33_SERIES1:
        {
            IntArray *data = (IntArray *)new_data;
            size_t len = data->length;
            for (size_t i = 0; i < len; i++) {
                lv_chart_set_next_value(widget, guider_ui.BenchMarking_M33.chart_bm_ser_2, data->array[i]);
            }
            lv_chart_refresh(widget);
        }
        break;
    case WIDGET_TYPE_CHART_M33_SERIES2:
        {
            IntArray *data = (IntArray *)new_data;
            size_t len = data->length;
            for (size_t i = 0; i < len; i++) {
                lv_chart_set_next_value(widget, guider_ui.BenchMarking_M33.chart_bm_ser_3, data->array[i]);
            }
            lv_chart_refresh(widget);
        }
        break;
    case WIDGET_TYPE_LARGE_TERMINAL_A1:
        terminal_add(large_txt_log_a1, 1, widget, new_data);
        break;
    case WIDGET_TYPE_LARGE_TERMINAL_M33:
        terminal_add(large_txt_log_m33, 1, widget, new_data);
        break;
    default:
        break;
    }
    return 0;
}


static int gui_sync_widget_value(lv_obj_t *obj, void *new_data, int widget_type)
{
	int ret = 0;

	if (get_debug_mode() & DEBUG_MODE(6)) {
		printf("%s() DEBUG id:%d new_data:%p\n", __func__, widget_type, new_data);
		return 0;
	}

	if (get_debug_mode() & DEBUG_MODE(0))
		printf("%s() id:%d new_data:%p\n", __func__, widget_type, new_data);

	ret = sync_widget_value(obj, new_data, widget_type);
	return ret;
}

static int gui_update_log_data(void)
{
	core_session_t *ps = NULL;
	uint8_t boot_type = 0;
	uint8_t am_idx = 0;
	int size = 32;
	int len = 0;

	for (am_idx = 0; am_idx < AM_MAX; am_idx++) {
		ps = get_core_session(am_idx);

		boot_type = ps->boot_type;

		/* AC_IDX no longer tracks a remoteproc session; it carries the
		 * Cortex-A55-0 utilisation line that main() emits every second, which is
		 * valid with no firmware loaded. Gating it on boot_type would leave the
		 * ring to fill up and the terminal permanently blank. */
		if (am_idx != AC_IDX && !boot_type)
			continue;

		len = pop_gui_buf_try(am_idx, ps->gui_log, size);
		if (len <= 0)
			continue;

		ps->gui_log[len] = '\0';

		if (get_debug_mode() & DEBUG_MODE(1))
			printf("%s() am_idx:%d boot_type:%d len:%d str:###%s###\n", __func__, am_idx, boot_type, len, ps->gui_log);

		if(am_idx == 0) {
			gui_sync_widget_value(guider_ui.Home.container_cortexA1_ta_coreA1_log, ps->gui_log, WIDGET_TYPE_TERMINAL_A1);
			gui_sync_widget_value(guider_ui.Console_A1.ta_log, ps->gui_log, WIDGET_TYPE_LARGE_TERMINAL_A1);
		} else if(am_idx == 1) {
			gui_sync_widget_value(guider_ui.Home.container_cortexM33_ta_coreM33_log, ps->gui_log, WIDGET_TYPE_TERMINAL_M33);
			gui_sync_widget_value(guider_ui.Console_M33.ta_log, ps->gui_log, WIDGET_TYPE_LARGE_TERMINAL_M33);
		}
	}

	return 0;
}

static int update_gui_latency(uint8_t am_idx, rt_latency_shm_stat *p)
{
	core_session_t *ps = get_core_session(am_idx);
	int16_t *pdata[3];
	int16_t data[3];
	int i = 0;

	/* Only the M33 lane still has a latency chart. BenchMarking_A1 now plots
	 * Cortex-A55-0 utilisation and is owned by cpu_load_ui.c, so A-core latency
	 * samples have nowhere to go. Reaching here with AC_IDX would need a ca55
	 * remoteproc node, which Torizon does not expose. */
	if (am_idx != MC_IDX)
		return 0;

	data[0] = p->min;
	data[1] = p->max;
	data[2] = p->mean;

	for (i = 0; i < 3; i++) {
		if (data[i] < 0)
			data[i] = 0;

		if (data[i] > GUI_LATENCY_MAX)
			data[i] = GUI_LATENCY_MAX;
	}

	pdata[0] = ps->min_latency;
	pdata[1] = ps->max_latency;
	pdata[2] = ps->mean_latency;

	/* One sample per stat per tick. GUI_LATENCY_NUM is the number of
	 * statistics (min/max/mean), not a sample count — writing the same
	 * value across the whole array and pushing all of it into the chart
	 * consumed 3 of the widget's 10 points per second with duplicates,
	 * leaving ~3s of visible history as a stair-step instead of a trend. */
	pdata[0][0] = data[0];
	pdata[1][0] = data[1];
	pdata[2][0] = data[2];
#ifdef DEBUG_TEST
	if (get_debug_mode() & DEBUG_MODE(5)) {
		printf("%s() am_idx:%d min:%d max:%d mean:%d\n",\
			__func__, am_idx, pdata[0][0], pdata[1][0], pdata[2][0]);
	}
#endif
	if (get_debug_mode() & DEBUG_MODE(1))
	      printf("%s() am_idx:%d min:%d max:%d mean:%d\n", __func__, am_idx, p->min, p->max, p->mean);

	for (i = 0; i < 3; i++) {
		ps->int_array[i].array = pdata[i];
		ps->int_array[i].length = 1;

		if(i == 0) {
			gui_sync_widget_value(guider_ui.BenchMarking_M33.chart_bm, &ps->int_array[i], WIDGET_TYPE_CHART_M33_SERIES0);
		} else if(i == 1) {
			gui_sync_widget_value(guider_ui.BenchMarking_M33.chart_bm, &ps->int_array[i], WIDGET_TYPE_CHART_M33_SERIES1);
		} else if(i == 2) {
			gui_sync_widget_value(guider_ui.BenchMarking_M33.chart_bm, &ps->int_array[i], WIDGET_TYPE_CHART_M33_SERIES2);
		}
	}

	return 0;
}

static int gui_update_latency_data(void)
{
	core_session_t *ps = NULL;
	uint8_t am_idx = 0;

	for (am_idx = 0; am_idx < AM_MAX; am_idx++) {
		rt_latency_shm_stat *stat = NULL;
		ps = get_core_session(am_idx);

		pthread_mutex_lock(&s_rt_latency_mutex);
		if (ps->rt_shm_flag) {
			stat = ps->rt_shm_stat;
			ps->rt_shm_flag = 0;
		}
		pthread_mutex_unlock(&s_rt_latency_mutex);

		if (stat) {
			update_gui_latency(am_idx, stat);
		}
	}

	return 0;
}

void request_stop_refresh_anim(int cortexID)
{
	if (cortexID < 0 || cortexID > 1)
		return;

	s_stop_refresh_pending[cortexID] = 1;
}

static void gui_handle_pending_ui_ops(void)
{
	if (s_stop_refresh_pending[0]) {
		s_stop_refresh_pending[0] = 0;
		stop_refresh_anim(0);
	}

	if (s_stop_refresh_pending[1]) {
		s_stop_refresh_pending[1] = 0;
		stop_refresh_anim(1);
	}
}

static void *lvgl_gui_thread(void *p)
{
	uint32_t cnt = 0;
	uint32_t idx;
	char *pboot_type = NULL;

	(void)p;

	gui_app_init();

	pboot_type = get_boot_type_string(NULL);
	if (pboot_type)
		sync_widget_value(guider_ui.Home.ddlist_apps, pboot_type, WIDGET_TYPE_DROPDOWN);

	pthread_mutex_lock(&s_gui_init_mutex);
	s_gui_init_status = 0;
	s_gui_init_done = 1;
	pthread_cond_signal(&s_gui_init_cond);
	pthread_mutex_unlock(&s_gui_init_mutex);

	while (srun_flag) {
		gui_handle_pending_ui_ops();
		idx = cnt % 10;
		if (idx == 0)
			gui_update_log_data();
		else if (idx == 2)
			gui_update_latency_data();
		else if (idx == 4)
			home_status_tick();
		else if (idx == 6)
			cpu_load_ui_render();

		gui_rf_loop();

		cnt = (cnt + 1) % 10;
		usleep(TIME_5MS_US);
	}

	pthread_mutex_lock(&s_gui_lifecycle_mutex);
	s_gui_thread_started = 0;
	pthread_mutex_unlock(&s_gui_lifecycle_mutex);

	return NULL;
}

int notify_update_gui_latency(uint8_t am_idx, rt_latency_shm_stat *p)
{
	core_session_t *ps = get_core_session(am_idx);

	pthread_mutex_lock(&s_rt_latency_mutex);
	ps->rt_shm_stat = p;
	ps->rt_shm_flag = 1;
	pthread_mutex_unlock(&s_rt_latency_mutex);

	return 0;
}

static char sip_info[128];
int start_lvgl_gui(void)
{
	int ret = 0;

	pthread_mutex_lock(&s_gui_lifecycle_mutex);
	if (s_gui_thread_started || srun_flag) {
		pthread_mutex_unlock(&s_gui_lifecycle_mutex);
		return 0;
	}
	s_gui_thread_started = 1;
	srun_flag = 1;
	pthread_mutex_unlock(&s_gui_lifecycle_mutex);

	s_stop_refresh_pending[0] = 0;
	s_stop_refresh_pending[1] = 0;

	pthread_mutex_lock(&s_gui_init_mutex);
	s_gui_init_done = 0;
	s_gui_init_status = 0;
	pthread_mutex_unlock(&s_gui_init_mutex);

	ret = pthread_create(&slvgl_gui_thread, NULL, lvgl_gui_thread, NULL);
	if (ret) {
		printf("failed to create lvgl_gui_thread, ret:%d-%s\n", ret, strerror(-ret));
		pthread_mutex_lock(&s_gui_lifecycle_mutex);
		srun_flag = 0;
		s_gui_thread_started = 0;
		pthread_mutex_unlock(&s_gui_lifecycle_mutex);
		goto out_tag;
	}

	pthread_mutex_lock(&s_gui_init_mutex);
	while (!s_gui_init_done)
		pthread_cond_wait(&s_gui_init_cond, &s_gui_init_mutex);
	ret = s_gui_init_status;
	pthread_mutex_unlock(&s_gui_init_mutex);

	if (ret) {
		pthread_mutex_lock(&s_gui_lifecycle_mutex);
		srun_flag = 0;
		pthread_mutex_unlock(&s_gui_lifecycle_mutex);
		pthread_join(slvgl_gui_thread, NULL);
	}

out_tag:
	return ret;
}

int stop_lvgl_gui(void)
{
	int should_join = 0;

	pthread_mutex_lock(&s_gui_lifecycle_mutex);
	if (s_gui_thread_started) {
		srun_flag = 0;
		should_join = 1;
	}
	pthread_mutex_unlock(&s_gui_lifecycle_mutex);

	if (!should_join)
		return 0;

	pthread_join(slvgl_gui_thread, NULL);
	return 0;
}

/**
 * @brief Queries the operation status and updates boot type and flag if provided
 *
 * This function checks if an operation has been triggered, and if so, retrieves
 * the boot type and status flag. It also resets the operation status after reading.
 *
 * @param pboot_type Pointer to store the boot type
 * @param pflag Pointer to store the operation status flag
 * @return 1 if operation was triggered, 0 otherwise
 */
int query_operation_status(uint8_t *pboot_type, uint16_t *pflag)
{
	OperationInfo op = {0};
	uint8_t boot_type = 0;
	int ret = 0;

	if (copy_operation_info(&op) == 0 && op.trigger) {
		boot_type = op.appName + 1 + op.coreId * 4;
		if (pboot_type)
			*pboot_type = boot_type;
		if (pflag)
			*pflag = op.status;

		printf("%s() trigger:%d coreId:%d appName:%d status:%d\n", __func__, op.trigger, op.coreId, op.appName, op.status);
		update_operation_status(0);
		ret = 1;
	}

	return ret;
}




