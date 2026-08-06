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

/* i.MX93 A55 cluster spec (Reference Manual, Cortex-A55 TRM). Used when the
 * kernel does not expose cache/cpufreq sysfs — the case on Torizon 6.6, where
 * both /sys/.../cpuN/cache/ and .../cpufreq/ come up empty. */
#define IMX93_A55_MAX_FREQ_STR "1.70GHz"
#define IMX93_A55_L1I_STR      "32KB"
#define IMX93_A55_L1D_STR      "32KB"
#define IMX93_A55_L2_STR       "128KB (per core)"
#define IMX93_A55_L3_STR       "not present"

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

static void read_cache_size(int cpu_index, int cache_index, const char *fallback,
                            char *out, size_t out_len)
{
    char path[128];
    char buf[32] = {0};
    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cache/index%d/size",
             cpu_index, cache_index);
    if (read_first_line(path, buf, sizeof(buf)) == 0 && buf[0]) {
        snprintf(out, out_len, "%s", buf);
    } else if (fallback) {
        snprintf(out, out_len, "%s", fallback);
    } else {
        snprintf(out, out_len, "n/a");
    }
}

/* Parse /proc/cpuinfo for the block belonging to `cpu_index`. Returns 1 if
 * anything was extracted (features / cpu_part), 0 otherwise. */
static int parse_cpuinfo_block(int cpu_index, char *features_out, size_t features_len,
                               unsigned int *cpu_part_out)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return 0;

    char line[512];
    int  current = -1;
    int  in_target = 0;
    int  got_features = 0;
    int  got_part = 0;

    if (features_out && features_len) features_out[0] = '\0';
    if (cpu_part_out) *cpu_part_out = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) current = atoi(colon + 1);
            in_target = (current == cpu_index);
            continue;
        }
        if (!in_target) continue;

        if (features_out && !got_features && strncmp(line, "Features", 8) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                const char *p = colon + 1;
                while (*p == ' ' || *p == '\t') p++;
                snprintf(features_out, features_len, "%s", p);
                size_t l = strlen(features_out);
                while (l > 0 && (features_out[l - 1] == '\n' || features_out[l - 1] == '\r'))
                    features_out[--l] = '\0';
                got_features = 1;
            }
        } else if (cpu_part_out && !got_part && strncmp(line, "CPU part", 8) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                *cpu_part_out = (unsigned int)strtoul(colon + 1, NULL, 0);
                got_part = 1;
            }
        }
    }
    fclose(f);
    return (got_features || got_part) ? 1 : 0;
}

/* Derive MPE/NEON summary from the Features string in /proc/cpuinfo. */
static void derive_mpe(const char *features, char *out, size_t out_len)
{
    if (!features || !features[0]) {
        snprintf(out, out_len, "n/a");
        return;
    }
    int has_asimd = (strstr(features, "asimd") != NULL);
    int has_fp    = (strstr(features, "fp")    != NULL);
    int has_fphp  = (strstr(features, "fphp")  != NULL);

    if (has_asimd && has_fphp) {
        snprintf(out, out_len, "NEON + FP16 (ASIMD)");
    } else if (has_asimd) {
        snprintf(out, out_len, "NEON + FP (ASIMD)");
    } else if (has_fp) {
        snprintf(out, out_len, "FP (no NEON)");
    } else {
        snprintf(out, out_len, "n/a");
    }
}

static void read_architecture(unsigned int cpu_part, char *out, size_t out_len)
{
    /* Cortex-A55 (0xd05) is Armv8.2-A. Detect explicitly so the string is
     * accurate on i.MX93 even when uname just says "aarch64". */
    if (cpu_part == 0xd05) {
        snprintf(out, out_len, "64-bit Arm\xC2\xAE" "v8.2-A");
        return;
    }
    struct utsname u;
    if (uname(&u) == 0 && strcmp(u.machine, "aarch64") == 0) {
        snprintf(out, out_len, "64-bit Arm\xC2\xAE" "v8-A");
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

    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq",
             cpu_index);
    if (read_first_line(path, buf, sizeof(buf)) == 0 && buf[0]) {
        format_frequency(strtoul(buf, NULL, 10), out, out_len);
        return;
    }

    /* Torizon 6.6 on i.MX93 does not expose cpufreq at all — fall back to the
     * fixed max clock from the reference manual, prefixed with '~' to signal
     * that this is a spec value, not a live measurement. */
    snprintf(out, out_len, "~%s", IMX93_A55_MAX_FREQ_STR);
}

void cpu_info_read_cortex_a(int cpu_index, cortex_a_info_t *out)
{
    if (!out) return;

    char features[256] = {0};
    unsigned int cpu_part = 0;
    parse_cpuinfo_block(cpu_index, features, sizeof(features), &cpu_part);

    cpu_info_refresh_frequency(cpu_index, out->frequency, sizeof(out->frequency));
    read_cache_size(cpu_index, 0, IMX93_A55_L1I_STR, out->l1i_cache, sizeof(out->l1i_cache));
    read_cache_size(cpu_index, 1, IMX93_A55_L1D_STR, out->l1d_cache, sizeof(out->l1d_cache));
    read_cache_size(cpu_index, 2, IMX93_A55_L2_STR,  out->l2_cache,  sizeof(out->l2_cache));
    read_cache_size(cpu_index, 3, IMX93_A55_L3_STR,  out->l3_cache,  sizeof(out->l3_cache));
    derive_mpe(features, out->mpe, sizeof(out->mpe));
    read_architecture(cpu_part, out->architecture, sizeof(out->architecture));
}

void cpu_info_read_cortex_m(cortex_m_info_t *out)
{
    if (!out) return;
    *out = s_cortex_m_defaults;
}
