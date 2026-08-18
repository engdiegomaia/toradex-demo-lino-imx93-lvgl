/*
 * Copyright 2026 Toradex
 *
 * Cortex-A55-0 utilisation sampling for the HMI benchmarking screen.
 *
 * Deliberately free of LVGL and real_time_edge dependencies: this module only
 * reads /proc and does arithmetic, so it can be exercised by
 * scripts/cpu-load-selftest.c on the host. All widget work lives in
 * cpu_load_ui.c, which runs on the LVGL thread.
 */

#ifndef __CPU_LOAD_H_
#define __CPU_LOAD_H_
#ifdef __cplusplus
extern "C" {
#endif

/* Logical CPU that runs Linux and the LVGL HMI. */
#define CPU_LOAD_A55_0_INDEX 0

/* Matches lv_chart_set_point_count(chart_bm, 10) in gg_BenchMarking_A1.c, so a
 * full chart can be backfilled from history on screen entry. */
#define CPU_LOAD_HISTORY 10

/* Raw counters, all in USER_HZ ticks. */
typedef struct {
    unsigned long long cpu_busy;   /* user+nice+system+irq+softirq+steal */
    unsigned long long cpu_total;  /* cpu_busy + idle + iowait */
    unsigned long long proc_time;  /* utime + stime of this process */
    int valid;
} cpu_load_raw_t;

/* Derived percentages, each clamped to 0..100. */
typedef struct {
    int total;   /* all load on the sampled CPU */
    int app;     /* this process' share (see cpu_load_compute() caveat) */
    int idle;    /* 100 - total */
    int valid;
} cpu_load_t;

/* Pure: derive percentages from two snapshots. No I/O, so it is directly
 * testable. Yields valid == 0 unless both snapshots are valid and the CPU
 * counters actually advanced. */
void cpu_load_compute(const cpu_load_raw_t *prev, const cpu_load_raw_t *cur,
                      cpu_load_t *out);

/* Read the "cpuN" line of /proc/stat plus /proc/self/stat. Returns 0 on
 * success, -1 on failure (out->valid is cleared either way). */
int cpu_load_read_raw(int cpu_index, cpu_load_raw_t *out);

/* Producer, called once per second from main()'s loop. Returns 1 when a fresh
 * sample became available, 0 otherwise (the first call only primes the
 * baseline). Thread-safe. */
int cpu_load_sample(void);

/* Latest sample without clearing the pending flag. Returns 1 if *out was
 * filled. Thread-safe. */
int cpu_load_peek(cpu_load_t *out);

/* Latest sample, clearing the pending flag so the caller sees each sample once.
 * Returns 1 if *out was filled. Thread-safe. */
int cpu_load_take(cpu_load_t *out);

/* Copies up to `max` past samples, oldest first, into `out`. Returns the number
 * written. Thread-safe. */
int cpu_load_history(cpu_load_t *out, int max);

#ifdef __cplusplus
}
#endif
#endif
