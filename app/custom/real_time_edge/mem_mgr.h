/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __MEM_MGR_H__
#define __MEM_MGR_H__

#include "basedef.h"


#define MM_PAGE_SIZE (4096)

#define MAX_MEM_MAP_NUM (256)
#define INV_MEM_FD (-1)

int rd_invalid_cache(uint8_t *pbuf, int len);
int wr_flush_cache(uint8_t *pbuf, int len);

int init_mem_mgr(void);
int exit_mem_mgr(void);

void *mem_map(size_t length, off_t offset);
int mem_unmap(void *addr, size_t length);

void *mem_map_page(off_t pa, size_t *len, void **pva);

#endif
