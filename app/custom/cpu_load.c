/*
 * Copyright 2026 Toradex
 *
 * Cortex-A55-0 utilisation sampling. See cpu_load.h for the contract.
 */

#include "cpu_load.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static cpu_load_raw_t  s_prev;
static cpu_load_t      s_latest;
static int             s_pending;
static cpu_load_t      s_ring[CPU_LOAD_HISTORY];
static int             s_ring_count;
static int             s_ring_head;   /* next slot to write */

/* Rounded percentage, saturated at 100. */
static int pct(unsigned long long num, unsigned long long den)
{
    unsigned long long v;

    if (!den)
        return 0;

    v = (num * 100ULL + den / 2ULL) / den;
    return (v > 100ULL) ? 100 : (int)v;
}

void cpu_load_compute(const cpu_load_raw_t *prev, const cpu_load_raw_t *cur,
                      cpu_load_t *out)
{
    unsigned long long d_total, d_busy, d_proc;

    if (!out)
        return;

    out->total = 0;
    out->app   = 0;
    out->idle  = 100;
    out->valid = 0;

    if (!prev || !cur || !prev->valid || !cur->valid)
        return;

    /* Counters are monotonic; equal or going backwards means the snapshots are
     * unusable (first call, or a CPU that was hotplugged in between). */
    if (cur->cpu_total <= prev->cpu_total)
        return;

    d_total = cur->cpu_total - prev->cpu_total;
    d_busy  = (cur->cpu_busy  >= prev->cpu_busy)  ? cur->cpu_busy  - prev->cpu_busy  : 0;
    d_proc  = (cur->proc_time >= prev->proc_time) ? cur->proc_time - prev->proc_time : 0;

    out->total = pct(d_busy, d_total);

    /* Caveat: /proc/self/stat accounts this process across *all* CPUs, so `app`
     * is really "CPU time as a percentage of one core". That equals the share of
     * A55-0 exactly while the app is confined to CPU0 — true whenever CPU1 is
     * offline or handed to firmware — and overstates it when both cores run
     * Linux, hence the saturation in pct(). Per-CPU attribution for a process
     * is not exposed by /proc at all, so this is the measurement ceiling. */
    out->app = pct(d_proc, d_total);

    out->idle  = 100 - out->total;
    out->valid = 1;
}

/* Parse the per-CPU "cpuN" line of /proc/stat. Never the aggregate "cpu" line:
 * that one is cluster-wide and would silently change meaning when CPU1 goes
 * offline (its "cpu1" line simply disappears from the file). */
static int read_cpu_line(int cpu_index, cpu_load_raw_t *out)
{
    char   key[16];
    char   line[512];
    size_t key_len;
    FILE  *f;
    int    found = 0;

    snprintf(key, sizeof(key), "cpu%d ", cpu_index);
    key_len = strlen(key);

    f = fopen("/proc/stat", "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

        if (strncmp(line, key, key_len) != 0)
            continue;

        /* steal and the guest fields are absent on older kernels; default the
         * whole tail to 0 and accept any line that carries at least
         * user/nice/system/idle. */
        user = nice = system = idle = iowait = irq = softirq = steal = 0;
        if (sscanf(line + key_len, "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 4)
            break;

        out->cpu_busy  = user + nice + system + irq + softirq + steal;
        out->cpu_total = out->cpu_busy + idle + iowait;
        found = 1;
        break;
    }

    fclose(f);
    return found ? 0 : -1;
}

static int read_self_time(cpu_load_raw_t *out)
{
    char               buf[1024];
    FILE              *f;
    size_t             n;
    char              *p;
    unsigned long long utime = 0;
    unsigned long long stime = 0;
    int                i;

    f = fopen("/proc/self/stat", "r");
    if (!f)
        return -1;

    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    if (n == 0)
        return -1;
    buf[n] = '\0';

    /* The comm field (2) is parenthesised and may itself contain spaces and
     * parentheses, so anchor on the last ')' instead of counting tokens from
     * the start. After it, token 1 is field 3 (state), so utime (field 14) is
     * token 12 and stime (field 15) is token 13. */
    p = strrchr(buf, ')');
    if (!p)
        return -1;
    p++;

    for (i = 1; i <= 13; i++) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            return -1;

        if (i == 12)
            utime = strtoull(p, NULL, 10);
        else if (i == 13)
            stime = strtoull(p, NULL, 10);

        while (*p && *p != ' ')
            p++;
    }

    out->proc_time = utime + stime;
    return 0;
}

int cpu_load_read_raw(int cpu_index, cpu_load_raw_t *out)
{
    if (!out)
        return -1;

    memset(out, 0, sizeof(*out));

    if (read_cpu_line(cpu_index, out) != 0)
        return -1;

    if (read_self_time(out) != 0)
        return -1;

    out->valid = 1;
    return 0;
}

int cpu_load_sample(void)
{
    cpu_load_raw_t cur;
    cpu_load_t     sample;
    int            fresh = 0;

    if (cpu_load_read_raw(CPU_LOAD_A55_0_INDEX, &cur) != 0)
        return 0;

    pthread_mutex_lock(&s_mutex);

    /* s_prev starts zeroed, so the first call only primes the baseline. */
    cpu_load_compute(&s_prev, &cur, &sample);
    s_prev = cur;

    if (sample.valid) {
        s_latest  = sample;
        s_pending = 1;

        s_ring[s_ring_head] = sample;
        s_ring_head = (s_ring_head + 1) % CPU_LOAD_HISTORY;
        if (s_ring_count < CPU_LOAD_HISTORY)
            s_ring_count++;

        fresh = 1;
    }

    pthread_mutex_unlock(&s_mutex);
    return fresh;
}

int cpu_load_peek(cpu_load_t *out)
{
    int ret = 0;

    if (!out)
        return 0;

    pthread_mutex_lock(&s_mutex);
    if (s_latest.valid) {
        *out = s_latest;
        ret = 1;
    }
    pthread_mutex_unlock(&s_mutex);

    return ret;
}

int cpu_load_take(cpu_load_t *out)
{
    int ret = 0;

    if (!out)
        return 0;

    pthread_mutex_lock(&s_mutex);
    if (s_pending && s_latest.valid) {
        *out = s_latest;
        s_pending = 0;
        ret = 1;
    }
    pthread_mutex_unlock(&s_mutex);

    return ret;
}

int cpu_load_history(cpu_load_t *out, int max)
{
    int n, i, start;

    if (!out || max <= 0)
        return 0;

    pthread_mutex_lock(&s_mutex);

    n = (s_ring_count < max) ? s_ring_count : max;
    /* s_ring_head is the next write slot, so the n newest entries start n
     * places behind it. Copy them oldest first. */
    start = (s_ring_head - n + CPU_LOAD_HISTORY) % CPU_LOAD_HISTORY;
    for (i = 0; i < n; i++)
        out[i] = s_ring[(start + i) % CPU_LOAD_HISTORY];

    pthread_mutex_unlock(&s_mutex);

    return n;
}
