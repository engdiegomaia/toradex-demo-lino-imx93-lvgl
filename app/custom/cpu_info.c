/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms.
*/

#include "cpu_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>

/* Cortex-M33 on the i.MX93 is fully described by the reference manual; the
 * Linux kernel has no visibility of it. These strings match the SoC spec. */
static const cortex_m_info_t s_cortex_m_defaults = {
    .frequency    = "250MHz",
    .system_cache = "16kB",
    .code_cache   = "16KB",
    .boot_rom     = "256KB",
    .tcm          = "256KB",
    .nvic         = "support",
    .architecture = "32-bit Arm\xC2\xAE" "v8-M",
};

static int read_first_line(const char *path, char *out, size_t out_len)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(out, (int)out_len, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' || out[len - 1] == ' ')) {
        out[--len] = '\0';
    }
    return 0;
}

static void format_frequency(unsigned long khz, char *out, size_t out_len)
{
    if (khz >= 1000UL * 1000UL) {
        double ghz = (double)khz / 1000000.0;
        snprintf(out, out_len, "%.2fGHz", ghz);
    } else if (khz > 0) {
        snprintf(out, out_len, "%luMHz", khz / 1000UL);
    } else {
        snprintf(out, out_len, "n/a");
    }
}

static void read_cache_size(int cpu_index, int cache_index, char *out, size_t out_len)
{
    char path[128];
    char buf[32] = {0};
    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cache/index%d/size",
             cpu_index, cache_index);
    if (read_first_line(path, buf, sizeof(buf)) == 0 && buf[0]) {
        snprintf(out, out_len, "%s", buf);
    } else {
        snprintf(out, out_len, "n/a");
    }
}

/* Pull the "Features" line for a given processor block in /proc/cpuinfo,
 * and derive an MPE/NEON summary. */
static void read_mpe_features(int cpu_index, char *out, size_t out_len)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(out, out_len, "n/a");
        return;
    }

    char line[512];
    int  current_processor = -1;
    int  found_asimd = 0;
    int  found_fp    = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) current_processor = atoi(colon + 1);
        } else if (current_processor == cpu_index &&
                   strncmp(line, "Features", 8) == 0) {
            if (strstr(line, "asimd")) found_asimd = 1;
            if (strstr(line, "fp"))    found_fp    = 1;
            break;
        }
    }
    fclose(f);

    if (found_asimd && found_fp) {
        snprintf(out, out_len, "NEON + FP (ASIMD)");
    } else if (found_fp) {
        snprintf(out, out_len, "FP (no NEON)");
    } else {
        snprintf(out, out_len, "n/a");
    }
}

static void read_architecture(char *out, size_t out_len)
{
    struct utsname u;
    if (uname(&u) == 0) {
        if (strcmp(u.machine, "aarch64") == 0) {
            snprintf(out, out_len, "64-bit Arm\xC2\xAE" "v8-A");
        } else {
            snprintf(out, out_len, "%s", u.machine);
        }
    } else {
        snprintf(out, out_len, "n/a");
    }
}

void cpu_info_refresh_frequency(int cpu_index, char *out, size_t out_len)
{
    char path[128];
    char buf[32] = {0};
    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
             cpu_index);

    if (read_first_line(path, buf, sizeof(buf)) == 0 && buf[0]) {
        format_frequency(strtoul(buf, NULL, 10), out, out_len);
        return;
    }

    /* Fallback: cpuinfo_cur_freq (root-only on some kernels). */
    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq",
             cpu_index);
    if (read_first_line(path, buf, sizeof(buf)) == 0 && buf[0]) {
        format_frequency(strtoul(buf, NULL, 10), out, out_len);
        return;
    }

    snprintf(out, out_len, "n/a");
}

void cpu_info_read_cortex_a(int cpu_index, cortex_a_info_t *out)
{
    if (!out) return;

    cpu_info_refresh_frequency(cpu_index, out->frequency, sizeof(out->frequency));
    read_cache_size(cpu_index, 0, out->l1i_cache, sizeof(out->l1i_cache));
    read_cache_size(cpu_index, 1, out->l1d_cache, sizeof(out->l1d_cache));
    read_cache_size(cpu_index, 2, out->l2_cache,  sizeof(out->l2_cache));
    read_cache_size(cpu_index, 3, out->l3_cache,  sizeof(out->l3_cache));
    read_mpe_features(cpu_index, out->mpe, sizeof(out->mpe));
    read_architecture(out->architecture, sizeof(out->architecture));
}

void cpu_info_read_cortex_m(cortex_m_info_t *out)
{
    if (!out) return;
    *out = s_cortex_m_defaults;
}
