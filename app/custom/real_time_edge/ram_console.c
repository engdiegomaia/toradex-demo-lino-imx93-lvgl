/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "ram_console.h"
#include "rt_stats.h"
#include "log_tools.h"
#include "mem_mgr.h"


int set_ram_console_addr(uint8_t am_idx, off_t addr)
{
	core_session_t *ps = get_core_session(am_idx);

	ps->ram_console_addr = addr;
	printf("%s() am_idx:%d addr:0x%lX\n", __func__, am_idx, addr);
	return 0;
}

int set_ram_shm_addr(uint8_t am_idx, off_t addr)
{
	core_session_t *ps = get_core_session(am_idx);

	ps->shm_addr = addr;
	printf("%s() am_idx:%d addr:0x%lX\n", __func__, am_idx, addr);
	return 0;
}

static void print_buf(uint8_t am_idx, uint32_t start_pos, uint32_t end_pos, void *buf_addr, bool print_blink, bool new_line)
{
	core_session_t *ps = get_core_session(am_idx);
	bool begin_dump;
	uint32_t i;
	char chc;
	char ch;

	if (new_line)
		begin_dump = false;
	else
		begin_dump = true;

	ps->shm_str_len = 0;
	for (i = start_pos; i < end_pos; i++) {
		ch = *((unsigned char *) (buf_addr + RAM_CONSOLE_HEAD_SIZE + i));
		if (!begin_dump) {
			if (ch == 0xa || ch == 0xd)
				begin_dump = true;
		} else {
			chc = ((isascii(ch)) && (isprint(ch) || ch == 0xa || ch == 0xd)) ? ch : '.';

			/* For A-core hello_world */
			if (BOOT_TYPE_HELLO(ps->boot_type)) {
				set_boot_flag(am_idx, 1);
				log_printf(am_idx, "%c", chc);
			} else if (get_debug_mode()) {
				printf("%c", chc);
			}

			if (!ps->shm_addr && (ps->shm_str_len < SIZE_OF(ps->shm_str)))
				ps->shm_str[ps->shm_str_len++] = ch;
		}
	}

	if (print_blink && get_debug_mode())
		printf("\033[5m%s\033[0m", " > ");
}

static void *ram_console_thread(void *p)
{
	uint32_t pos, real_pos, last_pos = 0, real_last_pos;
	struct ram_console_header *header;
	uint8_t am_idx = *(uint8_t *)p;
	core_session_t *ps = NULL;
	char *pkey = "shm_addr:";
	off_t ram_con_addr = 0;
	char *pstr = NULL;
	size_t buf_size;
	void *buf_addr;

	ps = get_core_session(am_idx);

	ram_con_addr = ps->ram_console_addr;
	printf("ram console addr is 0x%lX!\n", ram_con_addr);
	if (!ram_con_addr)
		goto err;

	/* Map RAM Console Header firstly */
	header = mem_map(RAM_CONSOLE_HEAD_SIZE, ram_con_addr);
	if (header == (void *) -1) {
		fprintf(stderr, "mapping header memory failed\r\n");
		goto err;
	}
	if (strcmp(header->flag_string, RAM_CONSOLE_HEAD_STR) != 0) {
		mem_unmap(header, RAM_CONSOLE_HEAD_SIZE);
		fprintf(stderr, "Invalid RAM Console header at 0x%lx\r\n", ram_con_addr);
		goto err;
	}
	buf_size = header->ram_console_buf_size;
	pos = header->pos;

	if (mem_unmap(header, RAM_CONSOLE_HEAD_SIZE) == -1) {
		fprintf(stderr, "unmapping header memory failed\r\n");
		goto err;
	}
	/* Re-map RAM Console Header and Buffer Memory */
	ps->ram_buf_size = buf_size + RAM_CONSOLE_HEAD_SIZE;
	buf_addr = mem_map(ps->ram_buf_size, ram_con_addr);
	if (buf_addr == (void *) -1) {
		fprintf(stderr, "mapping buffer memory failed\r\n");
		goto err;
	}
	ps->ram_buf_addr = buf_addr;

	real_pos = pos % buf_size;
	printf("RAM Console@0x%lx, buf_size:%ld\r\n", ram_con_addr, buf_size);

	if (pos > buf_size)
		print_buf(am_idx, real_pos + 1, buf_size, buf_addr, false, true);

	print_buf(am_idx, 0, real_pos, buf_addr, true, false);
	fflush(stdout);

	last_pos = pos;
	while (ps->ram_console_flag) {
		header = buf_addr;
		pos = header->pos;
		real_pos = pos % buf_size;
		real_last_pos = last_pos % buf_size;

		if (pos == last_pos)
			goto loop_tag;

		if (pos > last_pos) {
			if (pos - last_pos <= buf_size) {
				/* return 3 blank space which is used by blink display */
				if (get_debug_mode())
					printf("\b\b\b");
				if (real_pos > real_last_pos) {
					print_buf(am_idx, real_last_pos, real_pos, buf_addr, true, false);
				} else {
					print_buf(am_idx, real_last_pos, buf_size, buf_addr, false, false);
					print_buf(am_idx, 0, real_pos, buf_addr, true, false);
				}
			} else { /* dump whole buffer */
				print_buf(am_idx, real_pos, buf_size, buf_addr, false, true);
				print_buf(am_idx, 0, real_pos, buf_addr, true, false);
			}
		} else {
			/* Overflow, re-print whole buffer */
			print_buf(am_idx, real_pos, buf_size, buf_addr, false, true);
			print_buf(am_idx, 0, real_pos, buf_addr, true, false);
		}
		last_pos = pos;
		fflush(stdout);

		if (!ps->shm_addr) {
			if (ps->shm_str_len < 2)
				goto loop_tag;

			ps->shm_str[ps->shm_str_len - 1] = '\0';
			pstr = strstr(ps->shm_str, pkey);
			if (pstr) {
				ps->shm_addr = strtoul(pstr + strlen(pkey), NULL, 0);
				printf("Got am_idx:%d shm_addr:0x%lX\n", am_idx, ps->shm_addr);
			}
		}

loop_tag:
		sleep(1);
	};

	if (mem_unmap(buf_addr, ps->ram_buf_size) == -1)
		fprintf(stderr, "unmapping buffer memory failed\r\n");
	else
		ps->ram_buf_addr = NULL;

err:
	return NULL;
}

int start_ram_console_thread(uint8_t am_idx)
{
	static uint8_t sboot_idx[AM_MAX] = {AC_IDX, MC_IDX};
	core_session_t *ps = get_core_session(am_idx);
	int ret = 0;

	if (ps->ram_buf_addr) {
		mem_unmap(ps->ram_buf_addr, ps->ram_buf_size);
		ps->ram_buf_addr = NULL;
		ps->ram_buf_size = 0;
	}

	ps->ram_console_flag = 1;
	ret = pthread_create(&ps->ram_console_thread, NULL, ram_console_thread, &sboot_idx[am_idx]);
	if (ret) {
		printf("failed to create cyclic task, ret:%d-%s\n", ret, strerror(-ret));
		goto out_tag;
	}

out_tag:
	return ret;
}

int stop_ram_console_thread(uint8_t am_idx)
{
	core_session_t *ps = get_core_session(am_idx);

	if (!ps->ram_console_flag)
		return 0;

	ps->ram_console_flag = 0;
	pthread_join(ps->ram_console_thread, NULL);

	return 0;
}

int output_shm_data(void)
{
	core_session_t *ps = NULL;
	uint8_t am_idx = 0;

	for (am_idx = 0; am_idx < AM_MAX; am_idx++) {
		ps = get_core_session(am_idx);

		if (ps->shm_addr)
			show_stat_shm_data(am_idx, ps->shm_addr);
	}

	return 0;
}
