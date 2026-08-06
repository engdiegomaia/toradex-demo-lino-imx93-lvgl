/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms.
*/

#ifndef __CPU_INFO_H_
#define __CPU_INFO_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define CPU_INFO_CELL_LEN 48

typedef struct {
    char frequency[CPU_INFO_CELL_LEN];
    char l1i_cache[CPU_INFO_CELL_LEN];
    char l1d_cache[CPU_INFO_CELL_LEN];
    char l2_cache[CPU_INFO_CELL_LEN];
    char l3_cache[CPU_INFO_CELL_LEN];
    char mpe[CPU_INFO_CELL_LEN];
    char architecture[CPU_INFO_CELL_LEN];
} cortex_a_info_t;

typedef struct {
    const char *frequency;
    const char *system_cache;
    const char *code_cache;
    const char *boot_rom;
    const char *tcm;
    const char *nvic;
    const char *architecture;
} cortex_m_info_t;

/* Cortex-A55: reads /proc/cpuinfo and /sys.
 * cpu_index is the logical CPU number to probe (e.g. 1 for the A55-1). */
void cpu_info_read_cortex_a(int cpu_index, cortex_a_info_t *out);

/* Cortex-M33 on the i.MX93 is not managed by Linux; return datasheet constants. */
void cpu_info_read_cortex_m(cortex_m_info_t *out);

/* Refresh only the frequency field (cpufreq can change at runtime). */
void cpu_info_refresh_frequency(int cpu_index, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
#endif
