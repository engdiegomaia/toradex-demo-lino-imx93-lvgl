/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "log_tools.h"
#include <pthread.h>

static pthread_mutex_t s_gui_buf_mutex = PTHREAD_MUTEX_INITIALIZER;


static int push_gui_buf(uint8_t am_idx, char *pbuf, int len)
{
	core_session_t *ps = get_core_session(am_idx);
	int wr_idx = 0;
	int next_idx = 0;
	int i = 0;

	pthread_mutex_lock(&s_gui_buf_mutex);
	wr_idx = ps->gui_wr_idx;

	for (i = 0; i < len; i++) {
		next_idx = (wr_idx + 1) % GUI_BUF_SIZE;
		if (next_idx == ps->gui_rd_idx)
			break;

		ps->gui_buf[wr_idx] = pbuf[i];
		wr_idx = next_idx;
	}
	ps->gui_wr_idx = wr_idx;
	pthread_mutex_unlock(&s_gui_buf_mutex);

	return 0;
}

int pop_gui_buf(uint8_t am_idx, char *pbuf, int len)
{
	core_session_t *ps = get_core_session(am_idx);
	int rd_idx = 0;
	int rlen = 0;
	int i = 0;

	if (am_idx >= AM_MAX)
		return -1;

	rd_idx = ps->gui_rd_idx;

	for (i = 0; i < len; i++) {
		if (rd_idx == ps->gui_wr_idx)
			break;

		pbuf[i] = ps->gui_buf[rd_idx];
		rd_idx = (rd_idx + 1) % GUI_BUF_SIZE;
		rlen++;
	}
	ps->gui_rd_idx = rd_idx;

	return rlen;
}

int pop_gui_buf_try(uint8_t am_idx, char *pbuf, int len)
{
	core_session_t *ps = get_core_session(am_idx);
	int rd_idx = 0;
	int rlen = 0;
	int i = 0;

	if (am_idx >= AM_MAX)
		return -1;

	if (pthread_mutex_trylock(&s_gui_buf_mutex) != 0)
		return -1;

	rd_idx = ps->gui_rd_idx;

	for (i = 0; i < len; i++) {
		if (rd_idx == ps->gui_wr_idx)
			break;

		pbuf[i] = ps->gui_buf[rd_idx];
		rd_idx = (rd_idx + 1) % GUI_BUF_SIZE;
		rlen++;
	}
	ps->gui_rd_idx = rd_idx;
	pthread_mutex_unlock(&s_gui_buf_mutex);

	return rlen;
}

int log_printf(uint8_t am_idx, char *fmtarg, ...)
{
	core_session_t *ps = get_core_session(am_idx);
	va_list vl_args;
	int ret = 0;

	va_start(vl_args, fmtarg);
	ret = vsnprintf(ps->log_buf, LOG_BUF_SIZE, fmtarg, vl_args);
	va_end(vl_args);

	if (ret > 0)
		push_gui_buf(am_idx, ps->log_buf, ret);

	if (get_debug_mode())
		printf("%s", ps->log_buf);

	return ret;
}

int push_oper_cmd(uint8_t am_idx, cmd_type_t *pop_cmd)
{
	core_session_t *ps = get_core_session(am_idx);
	int wr_idx = ps->cmd_wr_idx;
	cmd_type_t *pcmd = NULL;
	int next_idx = 0;

	if (!pop_cmd)
		return -1;

	next_idx = (wr_idx + 1) % MAX_CMD_NUM;
	if (next_idx == ps->cmd_rd_idx)
	      return -1;

	pcmd = &ps->cmd_list[wr_idx];
	memcpy(pcmd, pop_cmd, sizeof(*pcmd));

	wr_idx = next_idx;
	ps->cmd_wr_idx = wr_idx;

	return 0;
}

int pop_oper_cmd(uint8_t am_idx, cmd_type_t *pop_cmd)
{
	core_session_t *ps = get_core_session(am_idx);
	cmd_type_t *pcmd = NULL;
	int rd_idx = 0;

	if (am_idx >= AM_MAX)
		return -1;

	if (!pop_cmd)
		return -2;

	rd_idx = ps->cmd_rd_idx;
	if (rd_idx == ps->cmd_wr_idx)
		return -3;

	pcmd = &ps->cmd_list[rd_idx];
	memcpy(pop_cmd, pcmd, sizeof(*pcmd));

	rd_idx = (rd_idx + 1) % MAX_CMD_NUM;
	ps->cmd_rd_idx = rd_idx;

	return 0;
}

int dump_oper_cmd(uint8_t am_idx, char *ptitle[TYPE_AM_MAX])
{
	core_session_t *ps = get_core_session(am_idx);
	char *pcmd_str[2] = {"Stop", "Start"};
	cmd_type_t *pcmd = NULL;
	int rd_idx = 0;
	int i = 0;

	if (am_idx >= AM_MAX)
		return -1;

	rd_idx = ps->cmd_rd_idx;
	do {
		if (rd_idx == ps->cmd_wr_idx)
			break;

		pcmd = &ps->cmd_list[rd_idx];
		printf("%s() i:%d op_flag:%d boot_type:%d >> sec:%d %s %s\n", __func__, i, pcmd->op_flag,\
			pcmd->boot_type, pcmd->op_sec, pcmd_str[pcmd->op_flag], ptitle[pcmd->boot_type]);

		rd_idx = (rd_idx + 1) % MAX_CMD_NUM;
	} while (i++ < MAX_CMD_NUM);

	return 0;
}
