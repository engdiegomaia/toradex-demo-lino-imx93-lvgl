/*
 * Copyright 2026 Toradex
 *
 * Cortex-A55-0 utilisation UI. See cpu_load_ui.h for the contract.
 */

#include "cpu_load_ui.h"

#include "branding.h"
#include "cpu_info.h"
#include "cpu_load.h"

#include "lvgl.h"
#include "custom.h"
#include "events/gg_event.h"

/* Defined in custom.c; mirrors OperationInfo.coreId, which query_operation_status()
 * folds into boot_type as `appName + 1 + coreId * 4`. */
extern int cortexID;

/* coreId of the Cortex-M33 in that mapping. The A-core value (0) is no longer
 * selectable: its remoteproc demo has no UI, and start.sh exits 2 without a
 * ca55 node anyway. */
#define CORE_ID_M33 1

/* Palette lifted from setup_Home() / home_status.c so nothing here depends on
 * runtime widget-state introspection. */
#define COLOR_CAPTION   0x014B88
#define COLOR_VALUE     0x023c6c
#define COLOR_RUNNING   0x22C55E
#define COLOR_SELECTED  0x2563EB

/* The panel keeps the 479 x 493 slot at (288, 297) GUI-Guider gave it:
 * container_cortexA0 on its left is now the branding quadrant (branding.c) and
 * container_cortexM33 on its right starts at x=786. With no free column left,
 * the readouts go in a strip across the panel between the header and the log
 * terminal, and the terminal moves down to make room for it. */
#define READOUT_Y_CAPTION 170
#define READOUT_Y_VALUE   191
#define READOUT_X_TOTAL    12
#define READOUT_X_APP     168
#define READOUT_X_CLOCK   324

/* ta_coreA1_log was 309 px tall at y=167; keep its bottom edge at 476. */
#define LOG_Y 240
#define LOG_H 236

/* BenchMarking_A1 was a 0..6000 ns latency plot; it becomes 0..100 %.
 * 21 ticks over 0..100 puts a tick every 5, and a major every 4 ticks labels
 * 0/20/40/60/80/100. */
#define LOAD_AXIS_MAX          100
#define LOAD_SCALE_TICKS       21
#define LOAD_SCALE_MAJOR_EVERY 4

static lv_obj_t * s_val_total;
static lv_obj_t * s_val_app;
static lv_obj_t * s_val_clock;

static int s_home_ready;

/* Screens other than Home are built lazily. Remember which screen instance each
 * retarget was applied to, so a recreated screen is retargeted again instead of
 * being missed by a one-shot flag. */
static lv_obj_t * s_bench_applied_to;
static lv_obj_t * s_console_applied_to;
static lv_obj_t * s_cpuinfo_applied_to;

static lv_obj_t * make_label(lv_obj_t * parent, int x, int y, const lv_font_t * font,
                             uint32_t color, const char * text)
{
    lv_obj_t * label = lv_label_create(parent);

    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_set_align(label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_x(label, x);
    lv_obj_set_y(label, y);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);

    return label;
}

static void update_clock_label(void)
{
    char freq[CPU_INFO_CELL_LEN];

    if (!s_val_clock || !lv_obj_is_valid(s_val_clock))
        return;

    cpu_info_refresh_frequency(CPU_LOAD_A55_0_INDEX, freq, sizeof(freq));
    lv_label_set_text(s_val_clock, freq);
}

/* ------------------------------------------------------------------ Home ---- */

static void retire_a55_1_lane(gg_Home_t * home)
{
    /* Drop the A-core from the core selector: hidden checkbox, hidden duplicate
     * Cortex-A icon. Hiding does not renumber siblings, so radio_event_handler()
     * still maps the M33 checkbox (child index 1) to cortexID = 1. */
    lv_obj_add_flag(home->container_radio_cb_core_A1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(home->container_1_core_A1_icon, LV_OBJ_FLAG_HIDDEN);

    /* Both of these carry generated LV_EVENT_CLICKED handlers that set
     * cortexID = 0. The handlers are static in app/generated/, so the reachable
     * fix is to stop the click ever being delivered. Children keep their own
     * clickability, so the log terminal and the benchmarking button still
     * navigate. */
    lv_obj_remove_flag(home->container_cortexA1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(home->container_1_core_A1_icon, LV_OBJ_FLAG_CLICKABLE);

    cortexID = CORE_ID_M33;
    lv_obj_clear_state(home->container_radio_cb_core_A1, LV_STATE_CHECKED);
    lv_obj_add_state(home->container_radio_cb_core_m33, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(home->container_cortexM33, lv_color_hex(COLOR_SELECTED),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void build_a55_0_panel(gg_Home_t * home)
{
    lv_label_set_text(home->container_cortexA1_title_coreA1, "Cortex-A55-0");
    lv_label_set_text(home->container_cortexA1_coreA1_os, "Torizon OS");
    lv_label_set_text(home->container_cortexA1_coreA1_app, "Linux + LVGL HMI");

    /* A55-0 is the core we are running on, so its state is not a question. */
    lv_label_set_text(home->container_cortexA1_coreA1_status, "Running");
    lv_obj_set_style_text_color(home->container_cortexA1_coreA1_status,
                                lv_color_hex(COLOR_RUNNING), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(home->container_cortexA1_coreA1_icon,
                                   lv_color_hex(COLOR_RUNNING), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor_opa(home->container_cortexA1_coreA1_icon, 255, LV_PART_MAIN);
    lv_obj_set_style_image_recolor(home->container_cortexA1_coreA1_run,
                                   lv_color_hex(COLOR_RUNNING), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor_opa(home->container_cortexA1_coreA1_run, 255, LV_PART_MAIN);

    /* Deliberately NOT spinning the activity icon. A permanent rotation would
     * redraw a 60x64 image every frame on a GPU-less i.MX93 and inflate the very
     * baseline this screen exists to measure. The status label carries the
     * "running" signal instead. gg_pause_anim() is defensive, in case an earlier
     * state left an animation attached. */
    gg_pause_anim(home->container_cortexA1_coreA1_run, anim_rotation_cb);

    lv_obj_set_y(home->container_cortexA1_ta_coreA1_log, LOG_Y);
    lv_obj_set_height(home->container_cortexA1_ta_coreA1_log, LOG_H);

    make_label(home->container_cortexA1, READOUT_X_TOTAL, READOUT_Y_CAPTION,
               &lv_font_montserratMedium_15, COLOR_CAPTION, "Total load");
    s_val_total = make_label(home->container_cortexA1, READOUT_X_TOTAL, READOUT_Y_VALUE,
                             &lv_font_montserratMedium_30, COLOR_VALUE, "-- %");

    make_label(home->container_cortexA1, READOUT_X_APP, READOUT_Y_CAPTION,
               &lv_font_montserratMedium_15, COLOR_CAPTION, "This app (HMI)");
    s_val_app = make_label(home->container_cortexA1, READOUT_X_APP, READOUT_Y_VALUE,
                           &lv_font_montserratMedium_30, COLOR_VALUE, "-- %");

    make_label(home->container_cortexA1, READOUT_X_CLOCK, READOUT_Y_CAPTION,
               &lv_font_montserratMedium_15, COLOR_CAPTION, "Core clock");
    s_val_clock = make_label(home->container_cortexA1, READOUT_X_CLOCK, READOUT_Y_VALUE + 6,
                             &lv_font_montserratMedium_20, COLOR_VALUE, "n/a");

    update_clock_label();
}

void cpu_load_ui_init(gg_ui_t * ui)
{
    if (!ui || !ui->Home.screen)
        return;

    retire_a55_1_lane(&ui->Home);
    build_a55_0_panel(&ui->Home);
    branding_build_home(ui);

    s_home_ready = 1;
}

/* --------------------------------------------------------- BenchMarking ---- */

static void chart_push(lv_obj_t * chart, lv_chart_series_t * series, int value)
{
    if (!series)
        return;

    lv_chart_set_next_value(chart, series, value);
}

/* One sample across all three series. The chart is fed whether or not its screen
 * is active, so entering the screen shows current data rather than a window that
 * only starts filling on arrival. */
static void chart_push_sample(const cpu_load_t * load)
{
    gg_BenchMarking_A1_t * bm = &guider_ui.BenchMarking_A1;

    if (!bm->screen || !bm->chart_bm || !lv_obj_is_valid(bm->chart_bm))
        return;

    /* Series-to-statistic mapping follows the legend colours, which are fixed in
     * gg_BenchMarking_A1.c: ser_1 red, ser_2 green, ser_3 blue. */
    chart_push(bm->chart_bm, bm->chart_bm_ser_1, load->total);
    chart_push(bm->chart_bm, bm->chart_bm_ser_2, load->idle);
    chart_push(bm->chart_bm, bm->chart_bm_ser_3, load->app);

    lv_chart_refresh(bm->chart_bm);
}

/* gg_BenchMarking_A1.c seeds the three series with flat placeholder values
 * (2 / 0 / 1). Against a 0..100 axis those read as real near-idle measurements,
 * so blank them out and replay the ring instead: the plot then shows actual
 * history on first entry rather than a fiction or ten seconds of nothing. */
static void chart_backfill(void)
{
    gg_BenchMarking_A1_t * bm = &guider_ui.BenchMarking_A1;
    cpu_load_t history[CPU_LOAD_HISTORY];
    int n;
    int i;

    lv_chart_set_all_values(bm->chart_bm, bm->chart_bm_ser_1, LV_CHART_POINT_NONE);
    lv_chart_set_all_values(bm->chart_bm, bm->chart_bm_ser_2, LV_CHART_POINT_NONE);
    lv_chart_set_all_values(bm->chart_bm, bm->chart_bm_ser_3, LV_CHART_POINT_NONE);

    n = cpu_load_history(history, CPU_LOAD_HISTORY);
    for (i = 0; i < n; i++)
        chart_push_sample(&history[i]);
}

static void apply_benchmarking(void)
{
    gg_BenchMarking_A1_t * bm = &guider_ui.BenchMarking_A1;

    lv_label_set_text(bm->label_title, " CPU Utilisation");
    lv_label_set_text(bm->bm_core, "Cortex-A55-0");
    lv_label_set_text(bm->label_unit, "(%)");

    lv_chart_set_range(bm->chart_bm, LV_CHART_AXIS_PRIMARY_Y, 0, LOAD_AXIS_MAX);
    lv_chart_set_range(bm->chart_bm, LV_CHART_AXIS_SECONDARY_Y, 0, LOAD_AXIS_MAX);

    /* scale_x is the *vertical* value axis despite its name — it is created with
     * LV_SCALE_MODE_VERTICAL_LEFT in gg_BenchMarking_A1.c. scale_y is the
     * horizontal sample axis and keeps its 0..10 range. */
    lv_scale_set_range(bm->scale_x, 0, LOAD_AXIS_MAX);
    lv_scale_set_total_tick_count(bm->scale_x, LOAD_SCALE_TICKS);
    lv_scale_set_major_tick_every(bm->scale_x, LOAD_SCALE_MAJOR_EVERY);

    /* Legend text follows each line's colour, not the old statistic names:
     * line_Min is red like ser_1, line_Max green like ser_2, line_Avg blue like
     * ser_3. */
    lv_label_set_text(bm->label_min, "Total");
    lv_label_set_text(bm->label_max, "Idle");
    lv_label_set_text(bm->label_avg, "App");

    chart_backfill();
}

/* ------------------------------------------------- Console_A1 / CpuInfo ---- */

static void apply_console(void)
{
    lv_label_set_text(guider_ui.Console_A1.label_title, "Cortex-A55-0 Runtime Logs");
}

static void apply_cpuinfo(void)
{
    char freq[CPU_INFO_CELL_LEN];

    lv_label_set_text(guider_ui.CpuInfo.label_coreA1, "Cortex-A55-0");

    /* gg_CpuInfo.c fills the table from CPU_INFO_A_CORE_INDEX == 1. Cache and
     * architecture rows are identical across the cluster, but the frequency row
     * must come from cpu0 — cpu1 has no cpufreq node while it is offline. */
    cpu_info_refresh_frequency(CPU_LOAD_A55_0_INDEX, freq, sizeof(freq));
    lv_table_set_cell_value(guider_ui.CpuInfo.table_coreA1, 0, 1, freq);
}

/* ---------------------------------------------------------------- tick ---- */

void cpu_load_ui_render(void)
{
    cpu_load_t load;

    if (!s_home_ready)
        return;

    if (guider_ui.BenchMarking_A1.screen != s_bench_applied_to) {
        s_bench_applied_to = guider_ui.BenchMarking_A1.screen;
        if (s_bench_applied_to)
            apply_benchmarking();
    }

    if (guider_ui.Console_A1.screen != s_console_applied_to) {
        s_console_applied_to = guider_ui.Console_A1.screen;
        if (s_console_applied_to)
            apply_console();
    }

    if (guider_ui.CpuInfo.screen != s_cpuinfo_applied_to) {
        s_cpuinfo_applied_to = guider_ui.CpuInfo.screen;
        if (s_cpuinfo_applied_to)
            apply_cpuinfo();
    }

    if (!cpu_load_take(&load))
        return;

    if (s_val_total && lv_obj_is_valid(s_val_total))
        lv_label_set_text_fmt(s_val_total, "%d %%", load.total);

    if (s_val_app && lv_obj_is_valid(s_val_app))
        lv_label_set_text_fmt(s_val_app, "%d %%", load.app);

    update_clock_label();

    chart_push_sample(&load);
}
