/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "ram_console.h"
#include "log_tools.h"
#include "rt_stats.h"
#include "mem_mgr.h"
#include "lvgl_gui.h"
#include "applog.h"
#include "log_view.h"


int reset_rt_stats(uint8_t am_idx)
{
	core_session_t *ps = get_core_session(am_idx);
	size_t shm_len = sizeof(rt_latency_shm_t);
	rt_latency_shm_t *pshm = ps->rt_shm;

	ps->boot_fail = 0;
	ps->boot_flag = 0;
	ps->boot_cnt = 0;
	ps->boot_sec = 0;

	if (pshm) {
		pshm->flag = 0;
		pshm->type = 0;
		pshm->rd_idx = 0;
		pshm->wr_idx = 0;
		wr_flush_cache((uint8_t*)pshm, shm_len);
	}

	if (ps->shm_virt) {
		mem_unmap(ps->shm_virt, ps->shm_len);
		ps->shm_virt = NULL;
		ps->shm_len = 0;
		ps->rt_shm = NULL;
	}

	return 0;
}

int set_boot_flag(uint8_t am_idx, int flag)
{
	core_session_t *ps = get_core_session(am_idx);

	if (!ps->boot_flag && flag) {
		ps->boot_flag = flag;
		ps->boot_fail = 0;
		log_view_m33_set_state("RUNNING");
		log_view_m33_set_event("handshake ok");
	}

	return 0;
}

int is_boot_failed(uint8_t am_idx)
{
	core_session_t *ps = get_core_session(am_idx);

	return ps->boot_fail;
}

/* Latency rows go to the container log at DEBUG; the screen shows them in the
 * M33 frame (log_view.h). */
static void stats_print(char *pn, rt_latency_shm_stat *s)
{
	char row[96];

	applog_format_latency(row, sizeof(row), pn, s->min, s->mean, s->max);
	applog(APPLOG_DEBUG, "m33", "%s", row);
}

static void update_tic_tac(uint8_t am_idx)
{
	static int tic = 1;

	(void)am_idx;
	log_view_m33_set_event(tic ? "tic" : "tac");
	tic = !tic;
}

static int update_shm_data(uint8_t am_idx, rt_latency_shm_t *pshm)
{
	core_session_t *ps = get_core_session(am_idx);
	size_t shm_len = sizeof(rt_latency_shm_t);
	uint8_t boot_type = ps->boot_type;
	uint8_t test_type = ps->test_type;
	uint8_t *p = NULL;

	p = (uint8_t*)pshm;
	rd_invalid_cache(p, shm_len);

	if (!pshm->flag)
		pshm->flag = 1;

	if (test_type)
		pshm->type = test_type;

	ps->boot_cnt++;
	if (!ps->boot_flag) {
		if (pshm->flag == 2)
		      set_boot_flag(am_idx, 1);
		else if (ps->boot_cnt > 3)
		      ps->boot_fail = 1;
	}

	if (ps->boot_fail) {
		log_view_m33_set_state("FAILED");
		log_view_m33_set_event("no handshake from M33");
	} else if (!ps->boot_flag) {
		log_view_m33_set_state("BOOTING");
	}

	if (get_debug_mode() & DEBUG_MODE(2))
		applog(APPLOG_DEBUG, "shm", "poll am=%d rd_idx=%u wr_idx=%u type=%u flag=%u fail=%d",
		       am_idx, pshm->rd_idx, pshm->wr_idx, pshm->type, pshm->flag, ps->boot_fail);

	/* Handshake fields once per second. The `goto ret_tag` below silently
	 * suppresses every latency print when rd_idx == wr_idx, so this DEBUG
	 * line (APPLOG_LEVEL=debug) is how to tell whether the M33 advances
	 * wr_idx. */
	applog(APPLOG_DEBUG, "shm", "handshake am=%u sec=%d flag=%u type=%u rd_idx=%u wr_idx=%u cpu_load=%.2f%%",
	       am_idx, ps->boot_sec, pshm->flag, pshm->type,
	       pshm->rd_idx, pshm->wr_idx, pshm->cpu_load);

	if (pshm->rd_idx == pshm->wr_idx)
		goto ret_tag;

	/* For M-core hello_world */
	if (BOOT_TYPE_HELLO(boot_type))
		update_tic_tac(am_idx);

	if (BOOT_TYPE_RTLAT(boot_type)) {
		if (pshm->wr_idx > 1)
			log_view_m33_set_load(pshm->cpu_load);

		stats_print("irq_delay", &pshm->irq_delay);
		stats_print("irq_sched", &pshm->irq_sched);
		log_view_m33_set_latency(&pshm->irq_delay, &pshm->irq_sched);
		log_view_m33_set_event("sampling");

		notify_update_gui_latency(am_idx, &pshm->irq_sched);
	}

	pshm->rd_idx++;

ret_tag:
	/* One complete frame per poll tick keeps every value in a fixed place. */
	if (am_idx == MC_IDX) {
		log_view_m33_set_uptime(ps->boot_sec);
		log_view_m33_publish();
	}
	wr_flush_cache(p, shm_len);
	return 0;
}

int show_stat_shm_data(uint8_t am_idx, uint64_t addr)
{
	core_session_t *ps = get_core_session(am_idx);
	size_t shm_len = sizeof(rt_latency_shm_t);
	rt_latency_shm_t *pshm = ps->rt_shm;

	if (!pshm) {
		pshm = (rt_latency_shm_t *)mem_map_page(addr, &shm_len, &ps->shm_virt);
		ps->shm_len = shm_len;
		ps->rt_shm = pshm;

		if (pshm) {
			/* Canary at a slot the handshake never touches. If Linux read-back
			 * differs from what we just wrote, either the mmap silently fell
			 * back to a copy-on-write page or the region isn't reaching DDR. */
			volatile uint32_t *canary = (volatile uint32_t *)((uint8_t *)pshm + 0x100);
			uint32_t before = *canary;
			*canary = 0xDEADBEEF;
			wr_flush_cache((uint8_t *)canary, sizeof(*canary));
			rd_invalid_cache((uint8_t *)canary, sizeof(*canary));
			uint32_t after = *canary;
			applog(after == 0xDEADBEEFu ? APPLOG_INFO : APPLOG_ERROR, "shm",
			       "canary addr=0x%lx before=0x%08x wrote=0xDEADBEEF read=0x%08x %s",
			       (unsigned long)addr + 0x100, before, after,
			       after == 0xDEADBEEFu ? "ok" : "MISMATCH");
		} else {
			applog(APPLOG_ERROR, "shm", "mem_map_page failed addr=0x%lx",
			       (unsigned long)addr);
			return -1;
		}

		pshm->rd_idx = 0;
		pshm->wr_idx = 0;
		wr_flush_cache((uint8_t*)pshm, shm_len);
	}

	ps->boot_sec++;
	if (ps->boot_type)
		update_shm_data(am_idx, pshm);

	return 0;
}
