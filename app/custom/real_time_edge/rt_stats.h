/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


#ifndef __RT_STAT_H__
#define __RT_STAT_H__

#include "basedef.h"


int reset_rt_stats(uint8_t am_idx);

int set_boot_flag(uint8_t am_idx, int flag);

int is_boot_failed(uint8_t am_idx);

int show_stat_shm_data(uint8_t am_idx, uint64_t addr);


#endif
