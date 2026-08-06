/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __NET_TOOLS_H__
#define __NET_TOOLS_H__

#include "basedef.h"


#define ETH_BUF_LEN (512)
#define ETH_IFN_LEN (64)

int get_inet_ip(char *pif_info, int buf_len);

#endif
