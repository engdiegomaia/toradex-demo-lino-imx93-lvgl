/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __LOG_TOOLS_H__
#define __LOG_TOOLS_H__

#include "basedef.h"


int log_printf(uint8_t am_idx, char *fmtarg, ...);

int pop_gui_buf(uint8_t am_idx, char *pbuf, int len);
int pop_gui_buf_try(uint8_t am_idx, char *pbuf, int len);

int push_oper_cmd(uint8_t am_idx, cmd_type_t *pop_cmd);

int pop_oper_cmd(uint8_t am_idx, cmd_type_t *pop_cmd);

int dump_oper_cmd(uint8_t am_idx, char *ptitle[TYPE_AM_MAX]);


#endif
