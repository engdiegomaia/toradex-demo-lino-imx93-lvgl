/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __BASE_DEF_H__
#define __BASE_DEF_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <time.h> /* clock_gettime() */
#include <limits.h>
#include <stddef.h>

#include <netinet/in.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h> /* mlockall() */
#include <sys/stat.h>
#include <sys/prctl.h>


#define DEBUG_TEST

#define dc_civac(p) { asm volatile("dc civac, %0" : : "r"(p) : "memory"); }
#define dc_cvac(p) { asm volatile("dc cvac, %0" : : "r"(p) : "memory"); }
#define CACHE_LINE_SIZE (64)

#define SIZE_OF(a) (sizeof(a) / sizeof(a[0]))

#define RT_ASSERT(x) do { if (!(x)) printf("%s()-%d ASSERT ERROR!\n", __func__, __LINE__); } while(0)
#define RT_ASSERT_RET(x, y) do { if (!(x)) {printf("%s()-%d ASSERT ERROR!\n", __func__, __LINE__); return (y);} } while(0)

#ifndef bool
#define bool int
#define false (!!0)
#define true (!false)
#endif

#define TIME_5MS_US	(5000)  /* 5ms */
#define TIME_100MS_US	(100000)  /* 100ms */
#define TIME_1S_US	(1000000)  /* 1s */

struct _core_session_t;
struct _core_session_t *get_core_session(uint8_t am_idx);

int is_running(void);

uint8_t get_debug_mode(void);

char *get_boot_type_string(int *plen);

#define get_boot_type(i) (get_core_session(i)->boot_type)
#define get_test_type(i) (get_core_session(i)->test_type)

enum demo_type
{
	TYPE_AM_INV = 0,
	TYPE_A_RTOS_RTLAT = 1,
	TYPE_A_RTOS_HELLO = 2,
	TYPE_A_ZEPH_RTLAT = 3,
	TYPE_A_ZEPH_HELLO = 4,
	TYPE_M_RTOS_RTLAT = 5,
	TYPE_M_RTOS_HELLO = 6,
	TYPE_M_ZEPH_RTLAT = 7,
	TYPE_M_ZEPH_HELLO = 8,
	TYPE_AM_MAX,
};

typedef struct {
	int16_t *array;
	size_t length;
} gui_int_array;

typedef struct {
	uint32_t op_sec;
	uint8_t op_flag;
	uint8_t boot_type;
} cmd_type_t;

#define GUI_BUF_SIZE (2048)
#define LOG_BUF_SIZE (512)
#define MAX_CMD_NUM (64)

#define GUI_LATENCY_MAX (6000)
#define GUI_LATENCY_NUM (3)

#define BOOT_TYPE_ACORE(t) ((t) >= TYPE_A_RTOS_RTLAT) && ((t) <= TYPE_A_ZEPH_HELLO)
#define BOOT_TYPE_MCORE(t) ((t) >= TYPE_M_RTOS_RTLAT) && ((t) <= TYPE_M_ZEPH_HELLO)
#define BOOT_TYPE_RTLAT(t) ((t) & 0x01)
#define BOOT_TYPE_HELLO(t) (!BOOT_TYPE_RTLAT(t))

#define AC_IDX (0)
#define MC_IDX (1)
#define AM_MAX (2)
#define BOOT_TYPE_AM_IDX(t) ((t >= TYPE_M_RTOS_RTLAT) ? MC_IDX : AC_IDX)

#define CMD_START (1)
#define CMD_STOP (0)

#define DEBUG_MODE(t) (0x01 << (t))

typedef struct {
    int32_t min;
    int32_t max;
    int32_t mean;
    uint64_t rms;
    uint64_t var;

    int32_t abs_min;
    int32_t abs_max;
} rt_latency_shm_stat;

typedef struct {
	uint32_t flag;
	uint32_t type;

	uint32_t rd_idx;
	uint32_t wr_idx;

	rt_latency_shm_stat irq_delay;
	rt_latency_shm_stat irq_sched;
	double cpu_load;
} rt_latency_shm_t;

typedef struct _core_session_t
{
	uint8_t boot_type;
	uint8_t test_type;

	rt_latency_shm_t *rt_shm;
	void *shm_virt;
	size_t shm_len;
	off_t shm_addr;
	char shm_str[256];
	int shm_str_len;

	int boot_fail;
	int boot_flag;
	uint32_t boot_cnt;
	uint32_t boot_sec;

	pthread_t ram_console_thread;
	off_t ram_console_addr;
	int ram_console_flag;
	size_t ram_buf_size;
	void *ram_buf_addr;

	int16_t min_latency[GUI_LATENCY_NUM];
	int16_t max_latency[GUI_LATENCY_NUM];
	int16_t mean_latency[GUI_LATENCY_NUM];
	gui_int_array int_array[3];

	rt_latency_shm_stat *rt_shm_stat;
	int rt_shm_flag;

	char log_buf[LOG_BUF_SIZE];
	char gui_buf[GUI_BUF_SIZE];
	char gui_log[GUI_BUF_SIZE / 2];
	int gui_rd_idx;
	int gui_wr_idx;

	cmd_type_t cmd_list[MAX_CMD_NUM];
	int cmd_rd_idx;
	int cmd_wr_idx;
} core_session_t;


#endif
