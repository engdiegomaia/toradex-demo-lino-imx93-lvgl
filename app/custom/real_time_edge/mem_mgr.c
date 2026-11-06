/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "mem_mgr.h"
#include "applog.h"


static int smem_fd = INV_MEM_FD;

static void *smm_ptr[MAX_MEM_MAP_NUM];
static size_t smm_len[MAX_MEM_MAP_NUM];
static int smm_cnt;

int rd_invalid_cache(uint8_t *pbuf, int len)
{
	uint64_t mask = ~(CACHE_LINE_SIZE - 1);
	uint8_t *p = NULL;
	int i = 0;

	p = (uint8_t*)((uint64_t)pbuf & mask);

	if (p != pbuf)
		len += pbuf - p;

	for (i = 0; i < len; i += CACHE_LINE_SIZE)
		dc_civac(p + i);

	return 0;
}

int wr_flush_cache(uint8_t *pbuf, int len)
{
	uint64_t mask = ~(CACHE_LINE_SIZE - 1);
	uint8_t *p = NULL;
	int i = 0;

	p = (uint8_t*)((uint64_t)pbuf & mask);

	if (p != pbuf)
		len += pbuf - p;

	for (i = 0; i < len; i += CACHE_LINE_SIZE)
		dc_cvac(p + i);

	return 0;
}

static int log_mem_map(void *p, size_t len, int add)
{
	int found = 0;
	int i = 0;

	if (!p)
		return -1;

	if (add) {
		for (i = 0; i < smm_cnt; i++) {
			if (smm_ptr[i] == NULL) {
				smm_ptr[i] = p;
				smm_len[i] = len;
				found = 1;
				break;
			}
		}

		if (!found) {
			smm_ptr[smm_cnt] = p;
			smm_len[smm_cnt] = len;
			smm_cnt++;
		}
	} else {
		for (i = 0; i < smm_cnt; i++) {
			if ((smm_ptr[i] == p) && (smm_len[i] == len)) {
				smm_ptr[i] = NULL;
				smm_len[i] = 0;
			}
		}
	}

	return 0;
}

int init_mem_mgr(void)
{
	int i = 0;

	if (smem_fd != INV_MEM_FD)
		return smem_fd;

	smem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (smem_fd == INV_MEM_FD)
		applog(APPLOG_ERROR, "mem", "cannot open /dev/mem (needs CAP_SYS_RAWIO)");

	smm_cnt = 0;
	for (i = 0; i < MAX_MEM_MAP_NUM; i++) {
		smm_ptr[i] = NULL;
		smm_len[i] = 0;
	}

	return 0;
}

int exit_mem_mgr(void)
{
	int i = 0;

	for (i = 0; i < smm_cnt; i++) {
		if (smm_ptr[i] && smm_len[i]) {
			munmap(smm_ptr[i], smm_len[i]);

			smm_ptr[i] = NULL;
			smm_len[i] = 0;
		}
	}
	smm_cnt = 0;

	if (smem_fd != INV_MEM_FD)
		close(smem_fd);
	smem_fd = INV_MEM_FD;

	return 0;
}

void *mem_map(size_t length, off_t offset)
{
	int fd = smem_fd;
	void *p = NULL;

	if (fd == INV_MEM_FD) {
		init_mem_mgr();
		fd = smem_fd;
	}

	p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (p != MAP_FAILED)
		log_mem_map(p, length, 1);

	return p;
}

int mem_unmap(void *addr, size_t length)
{
	log_mem_map(addr, length, 0);

	return munmap(addr, length);
}

void *mem_map_page(off_t pa, size_t *len, void **pva)
{
	off_t mask = ~(MM_PAGE_SIZE - 1);
	size_t length = MM_PAGE_SIZE;
	off_t pa_offset = 0;
	off_t pa_diff = 0;
	void *virt = NULL;

	if (len)
		length = *len;

	pa_offset = pa & mask;
	if (pa_offset != pa)
		pa_diff = pa - pa_offset;

	length += pa_diff;
	if (len)
		*len = length;

	virt = mem_map(length, pa_offset);
	if (virt == MAP_FAILED) {
		applog(APPLOG_ERROR, "mem", "mmap failed virt=%p pa=0x%lX pa_offset=0x%lX", virt, pa, pa_offset);
		return NULL;
	}

	if (pva)
		*pva = virt;

	return (virt + pa_diff);
}
