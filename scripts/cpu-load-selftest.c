/*
 * Copyright 2026 Toradex
 *
 * Host-side self-test for the pure arithmetic in app/custom/cpu_load.c.
 * The percentage maths is the only non-trivial logic in the A55-0 utilisation
 * path, and it is the part that cannot be eyeballed on the panel, so it gets a
 * deterministic check that runs without a target.
 *
 *   cc -o /tmp/cpu-load-selftest scripts/cpu-load-selftest.c \
 *      app/custom/cpu_load.c -I app/custom -lpthread && /tmp/cpu-load-selftest
 */

#include "cpu_load.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int s_failures;

static void check_int(const char *what, int got, int want)
{
    if (got != want) {
        printf("  FAIL %-34s got %d, want %d\n", what, got, want);
        s_failures++;
    } else {
        printf("  ok   %-34s %d\n", what, got);
    }
}

static cpu_load_raw_t raw(unsigned long long busy, unsigned long long total,
                          unsigned long long proc, int valid)
{
    cpu_load_raw_t r;

    memset(&r, 0, sizeof(r));
    r.cpu_busy  = busy;
    r.cpu_total = total;
    r.proc_time = proc;
    r.valid     = valid;
    return r;
}

static void case_typical(void)
{
    cpu_load_raw_t prev = raw(100, 1000, 10, 1);
    cpu_load_raw_t cur  = raw(150, 1100, 20, 1);
    cpu_load_t     out;

    /* d_busy 50 / d_total 100 = 50%; d_proc 10 / 100 = 10% */
    puts("typical: half-loaded CPU, app owns a fifth of that");
    cpu_load_compute(&prev, &cur, &out);
    check_int("valid", out.valid, 1);
    check_int("total", out.total, 50);
    check_int("app",   out.app,   10);
    check_int("idle",  out.idle,  50);
}

static void case_fully_idle(void)
{
    cpu_load_raw_t prev = raw(100, 1000, 10, 1);
    cpu_load_raw_t cur  = raw(100, 1100, 10, 1);
    cpu_load_t     out;

    puts("fully idle: no busy or process ticks accrued");
    cpu_load_compute(&prev, &cur, &out);
    check_int("valid", out.valid, 1);
    check_int("total", out.total, 0);
    check_int("app",   out.app,   0);
    check_int("idle",  out.idle,  100);
}

static void case_saturated(void)
{
    cpu_load_raw_t prev = raw(0,   0,   0,   1);
    cpu_load_raw_t cur  = raw(100, 100, 200, 1);
    cpu_load_t     out;

    /* Two cores' worth of process time against one core's window: the app
     * figure must saturate rather than report 200%. */
    puts("saturated: process ran on both cores, app must clamp");
    cpu_load_compute(&prev, &cur, &out);
    check_int("valid", out.valid, 1);
    check_int("total", out.total, 100);
    check_int("app",   out.app,   100);
    check_int("idle",  out.idle,  0);
}

static void case_rounding(void)
{
    cpu_load_raw_t prev = raw(0, 0, 0, 1);
    cpu_load_raw_t cur  = raw(1, 3, 2, 1);
    cpu_load_t     out;

    /* 1/3 -> 33 (33.3 rounds down), 2/3 -> 67 (66.6 rounds up). */
    puts("rounding: thirds must round to nearest, not truncate");
    cpu_load_compute(&prev, &cur, &out);
    check_int("total", out.total, 33);
    check_int("app",   out.app,   67);
    check_int("idle",  out.idle,  67);
}

static void case_stalled_counters(void)
{
    cpu_load_raw_t prev = raw(100, 1000, 10, 1);
    cpu_load_raw_t same = raw(100, 1000, 10, 1);
    cpu_load_t     out;

    puts("stalled: cpu_total did not advance");
    cpu_load_compute(&prev, &same, &out);
    check_int("valid", out.valid, 0);
}

static void case_invalid_inputs(void)
{
    cpu_load_raw_t prev = raw(100, 1000, 10, 0);   /* not valid */
    cpu_load_raw_t cur  = raw(150, 1100, 20, 1);
    cpu_load_t     out;

    puts("invalid: unprimed baseline yields no sample");
    cpu_load_compute(&prev, &cur, &out);
    check_int("valid", out.valid, 0);

    cpu_load_compute(NULL, &cur, &out);
    check_int("valid (NULL prev)", out.valid, 0);
}

static void case_counter_rollback(void)
{
    cpu_load_raw_t prev = raw(100, 1000, 50, 1);
    cpu_load_raw_t cur  = raw(90,  1100, 40, 1);   /* busy/proc went backwards */
    cpu_load_t     out;

    puts("rollback: backwards busy/proc deltas floor at zero");
    cpu_load_compute(&prev, &cur, &out);
    check_int("valid", out.valid, 1);
    check_int("total", out.total, 0);
    check_int("app",   out.app,   0);
}

/* Live smoke test: the parsers must actually cope with this machine's /proc. */
static void case_live_proc(void)
{
    cpu_load_raw_t r;
    cpu_load_t     out;
    struct timespec deadline;
    volatile unsigned long spin = 0;
    int i;

    puts("live: /proc parsing on this host");

    check_int("read_raw returns 0", cpu_load_read_raw(CPU_LOAD_A55_0_INDEX, &r), 0);
    check_int("raw valid", r.valid, 1);
    check_int("cpu_total advanced past zero", r.cpu_total > 0, 1);
    check_int("cpu_total >= cpu_busy", r.cpu_total >= r.cpu_busy, 1);

    /* Prime, burn ~200 ms of user time, then sample: both the CPU and the
     * process counters must have moved. */
    cpu_load_sample();

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_nsec += 200L * 1000L * 1000L;
    if (deadline.tv_nsec >= 1000L * 1000L * 1000L) {
        deadline.tv_nsec -= 1000L * 1000L * 1000L;
        deadline.tv_sec++;
    }
    for (;;) {
        struct timespec now;

        for (i = 0; i < 100000; i++)
            spin += (unsigned long)i;

        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            break;
    }

    check_int("sample produced", cpu_load_sample(), 1);
    check_int("peek filled", cpu_load_peek(&out), 1);
    check_int("busy loop shows load", out.total > 0, 1);
    check_int("busy loop attributed to app", out.app > 0, 1);
    check_int("total + idle == 100", out.total + out.idle, 100);

    check_int("take consumes the sample", cpu_load_take(&out), 1);
    check_int("take is one-shot", cpu_load_take(&out), 0);
}

static void case_history_order(void)
{
    cpu_load_t hist[CPU_LOAD_HISTORY];
    int n;

    /* case_live_proc() has already pushed samples into the ring. */
    puts("history: ring returns at most a chartful, oldest first");
    n = cpu_load_history(hist, CPU_LOAD_HISTORY);
    check_int("count within bounds", n >= 1 && n <= CPU_LOAD_HISTORY, 1);
    check_int("entries are valid samples", hist[0].valid, 1);
    check_int("truncation honours max", cpu_load_history(hist, 1), 1);
    check_int("zero max returns nothing", cpu_load_history(hist, 0), 0);
}

int main(void)
{
    case_typical();
    case_fully_idle();
    case_saturated();
    case_rounding();
    case_stalled_counters();
    case_invalid_inputs();
    case_counter_rollback();
    case_live_proc();
    case_history_order();

    if (s_failures) {
        printf("\n%d check(s) FAILED\n", s_failures);
        return 1;
    }

    puts("\nall checks passed");
    return 0;
}
