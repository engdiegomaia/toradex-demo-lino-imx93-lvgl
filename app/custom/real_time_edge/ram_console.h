/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __RAM_CONSOLE_H__
#define __RAM_CONSOLE_H__

#include "basedef.h"


#define RAM_CONSOLE_HEAD_STR ("RAM_CONSOLE")
#define RAM_CONSOLE_HEAD_SIZE (64)

struct ram_console_header {
    char flag_string[12];
    char *ram_console_buf_addr;
    size_t ram_console_buf_size;
    uint32_t pos;
};

int set_ram_console_addr(uint8_t am_idx, off_t addr);
int set_ram_shm_addr(uint8_t am_idx, off_t addr);

int start_ram_console_thread(uint8_t am_idx);
int stop_ram_console_thread(uint8_t am_idx);

int output_shm_data();


#endif
