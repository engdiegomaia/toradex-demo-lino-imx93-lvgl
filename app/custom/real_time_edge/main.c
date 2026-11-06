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
#include "lvgl_gui.h"
#include "rt_stats.h"
#include "mem_mgr.h"
#include "main.h"
#include "cpu_load.h"
#include "log_view.h"
#include "boot_names.h"
#include "applog.h"
#include "linux_jitter.h"
#include "load_gen.h"
#include "rpmsg_ping.h"


static uint32_t scur_sec = 0;
static uint8_t sdebug_mode;
static int srun_flag = 1;

static char sprep_cmd[] = {"uname -a"};
static char spost_cmd[] = {""};

static char sboot_type_str[256];
static int sboot_type_len;

static core_session_t score_session[AM_MAX];

core_session_t *get_core_session(uint8_t am_idx)
{
	RT_ASSERT(am_idx < AM_MAX);

	am_idx %= AM_MAX;
	return score_session + am_idx;
}

int is_running(void)
{
	return srun_flag;
}

uint8_t get_debug_mode(void)
{
	return sdebug_mode;
}

char *get_boot_type_string(int *plen)
{
	if (plen)
		*plen = sboot_type_len;

	return sboot_type_str;
}

static int run_system_cmd(char *pcmd)
{
	int ret = 0;

	if (strlen(pcmd) > 0)
		ret = system(pcmd);

	printf("\n");
	return ret;
}

static char *sboot_type_title[TYPE_AM_MAX] = {
	"Invalid Boot Type!",
	"FreeRTOS rt_latency on Cortex-A",
	"FreeRTOS hello_world on Cortex-A",
	"Zephyr rt_latency on Cortex-A",
	"Zephyr hello_world on Cortex-A",
	"FreeRTOS rt_latency on Cortex-M",
	"FreeRTOS hello_world on Cortex-M",
	"Zephyr rt_latency on Cortex-M",
	"Zephyr hello_world on Cortex-M",
};

static void print_usage(char *pname)
{
	printf("\tGUI Guider -- backend program for imx93 evk platform\n\n");

	printf("Usage:\n %s -h / -a / -t / -d\n", pname);
	printf("\t-h: print usage\n");
	printf("\t-a: ram console buffer address\n");
	printf("\t-t: the type of test case\n");
	printf("\t-d: enable debug mode\n");
	printf("\n");
}

static int parse_input_arg(int argc, char **argv)
{
	uint8_t test_type = 0;
	off_t ram_addr = 0;
	int ret = 0;
	int opt = 0;

	while ((opt = getopt(argc,argv, "ha:t:d:")) != -1) {
		switch(opt) {
		case 'a':
			ram_addr = strtoul(optarg, NULL, 0);
			set_ram_console_addr(AC_IDX, ram_addr);
			set_ram_console_addr(MC_IDX, ram_addr);
			break;
		case 't':
			test_type = strtoul(optarg, NULL, 0);
			get_test_type(AC_IDX) = test_type;
			get_test_type(MC_IDX) = test_type;
			break;
		case 'd':
			sdebug_mode = strtoul(optarg, NULL, 0);
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			ret = -1;
			break;
		}
	}

	return ret;
}

static void signal_handler(int sig)
{
	srun_flag = 0;
}

#define STRCAT(x, y) strncat(x, y, sizeof(x) - strlen(x) - 1)
static int init_boot_type(void)
{
	static const uint8_t app_slots[] = {TYPE_A_RTOS_RTLAT, TYPE_A_RTOS_HELLO,
					       TYPE_A_ZEPH_RTLAT, TYPE_A_ZEPH_HELLO};
	char str[64];
	size_t i;

	/* One dropdown row per application, in boot-type order; the selected row
	 * index is what selects the application, so the order must not change. */
	for (i = 0; i < sizeof(app_slots) / sizeof(app_slots[0]); i++) {
		snprintf(str, sizeof(str), "%s%zu. %s", i ? "\n" : "", i + 1,
			 boot_type_menu_name(app_slots[i]));
		STRCAT(sboot_type_str, str);
	}

	sboot_type_len = strlen(sboot_type_str);
	return sboot_type_len;
}

static uint8_t input_boot_type(void)
{
	uint8_t boot_type = 0;
	uint8_t test_type = 0;
	uint32_t len = 0;
	char input[8];
	char *pr;

	printf("\n");
	for (boot_type = TYPE_A_RTOS_RTLAT; boot_type <= TYPE_M_ZEPH_HELLO; boot_type++)
		printf("%d: %s\n", boot_type, sboot_type_title[boot_type]);

	printf("\nInput boot type:");
	while (srun_flag) {
		input[0] = '0';
		pr = fgets(input, 8, stdin);
		if (!pr)
			continue;

		len = strlen(input);
		if (len == 0)
			continue;

		if ((input[0] == '\r') || (input[0] == '\n'))
			continue;

		boot_type = input[0] - '0';
		test_type = 1;
		if ((len > 2) && (input[1] == '-')) {
			test_type = input[2] - '0';
			if (test_type > 6) {
				printf("Invalid test type:%d\n", test_type);
				continue;
			}
		}

		if (boot_type < TYPE_AM_MAX) {
			get_test_type(AC_IDX) = test_type;
			get_test_type(MC_IDX) = test_type;
			printf("%s() boot_type:%d test_type:%d\n", __func__, boot_type, test_type);
			break;
		} else {
			printf("Invalid boot type:%d\n", boot_type);
		}
	}

	if (!srun_flag)
		boot_type = 0;

	return boot_type;
}

static int show_title_to_console(uint8_t boot_type)
{
	uint8_t am_idx = BOOT_TYPE_AM_IDX(boot_type);

	if (am_idx == MC_IDX) {
		log_view_m33_reset("BOOTING", boot_type_app_name(boot_type));
		log_view_m33_publish();
	}

	return 0;
}

static int stop_multi_core_prog(uint8_t boot_type, int op_flag)
{
	uint8_t am_idx = BOOT_TYPE_AM_IDX(boot_type);
	uint8_t else_type = 0;
	int both_flag = 0;
	cmd_type_t op_cmd;

	switch (boot_type) {
	case TYPE_A_RTOS_RTLAT:
	case TYPE_A_ZEPH_RTLAT:
	case TYPE_M_RTOS_RTLAT:
	case TYPE_M_ZEPH_RTLAT:
		both_flag = 1;
		break;

	case TYPE_A_RTOS_HELLO:
	case TYPE_A_ZEPH_HELLO:
	case TYPE_M_RTOS_HELLO:
	case TYPE_M_ZEPH_HELLO:
		break;

	default:
		printf("%s() invalid boot type %d!\n", __func__, boot_type);
		return -1;
	}

	reset_rt_stats(am_idx);  /* Reset important data */

	if (am_idx == MC_IDX) {
		log_view_m33_reset("STOPPED", "stopped");
		log_view_m33_publish();
	}

	if (op_flag == CMD_STOP) {
		op_cmd.op_sec = scur_sec;
		op_cmd.op_flag = op_flag;
		op_cmd.boot_type = boot_type;
		push_oper_cmd(am_idx, &op_cmd);
	}

	if (boot_type == TYPE_M_RTOS_HELLO)
		rpmsg_ping_stop();

	if (BOOT_TYPE_ACORE(boot_type)) {
		stop_ram_console_thread(am_idx);
		run_system_cmd("./stop.sh a");
	} else {
		run_system_cmd("./stop.sh m");
	}

	if (both_flag && op_flag) {
		else_type = get_boot_type(AM_MAX - 1 - am_idx);

		if (!else_type || BOOT_TYPE_RTLAT(else_type)) {
			set_ram_shm_addr(AC_IDX, 0);
			set_ram_shm_addr(MC_IDX, 0);

			if (am_idx == MC_IDX) {
				stop_ram_console_thread(AC_IDX);
				request_stop_refresh_anim(0);
				run_system_cmd("./stop.sh a");
			} else {
				request_stop_refresh_anim(1);
				run_system_cmd("./stop.sh m");
			}
		} else {
			set_ram_shm_addr(am_idx, 0);
		}
	} else {
		set_ram_shm_addr(am_idx, 0);
	}

	printf("%s() boot_type:%d both_flag:%d else_type:%d\n", __func__, boot_type, both_flag, else_type);

	return 0;
}

/*
 * For A-core program, should set ram_console address,
 * frdm_servo will get ram_shm address from the log of ram_console.
 *
 * For M-core program, should set ram_shm address directly.
 *
 * show_stat_shm_data will be called only when ram_shm address is set.
 * */
static int s_rpmsg_start_sec;

static int start_multi_core_prog(uint8_t boot_type, int reboot)
{
	uint8_t am_idx = BOOT_TYPE_AM_IDX(boot_type);
	off_t shm_addr = M33_LEGACY_SHM_BASE;
	core_session_t *ps = NULL;
	off_t ram_addr = 0;
	int rpmsg_mode = 0;
	cmd_type_t op_cmd;

	ps = get_core_session(am_idx);

	stop_multi_core_prog(boot_type, CMD_START);

	switch (boot_type) {
	case TYPE_A_RTOS_RTLAT:
		run_system_cmd("./start.sh a r r");
		ram_addr = FRTOS_RAM_ADDR;
		break;

	case TYPE_A_RTOS_HELLO:
		run_system_cmd("./start.sh a r h");
		ram_addr = FRTOS_RAM_ADDR;
		break;

	case TYPE_A_ZEPH_RTLAT:
		run_system_cmd("./start.sh a z r");
		ram_addr = ZEPHY_RAM_ADDR;
		break;

	case TYPE_A_ZEPH_HELLO:
		run_system_cmd("./start.sh a z h");
		ram_addr = ZEPHY_RAM_ADDR;
		break;

	case TYPE_M_RTOS_RTLAT:
		/* This is the rebuilt producer linked against the dedicated SHM. */
		shm_addr = M33_RTLAT_SHM_BASE;
		run_system_cmd("./start.sh m r r");
		break;

	case TYPE_M_RTOS_HELLO:
		/* RPMsg round-trip firmware: no shared-memory producer, so the SHM
		 * handshake poll must stay off (it would report a boot failure). */
		run_system_cmd("./start.sh m r h");
		s_rpmsg_start_sec = scur_sec;
		if (rpmsg_ping_start())
			applog(APPLOG_WARN, "rpmsg", "ping thread not started");
		rpmsg_mode = 1;
		break;

	case TYPE_M_ZEPH_RTLAT:
		run_system_cmd("./start.sh m z r");
		break;

	case TYPE_M_ZEPH_HELLO:
		run_system_cmd("./start.sh m z h");
		break;

	default:
		printf("%s() invalid boot type %d!\n", __func__, boot_type);
		return -1;
	}

	if (!reboot) {
		show_title_to_console(boot_type);

		op_cmd.op_sec = scur_sec;
		op_cmd.op_flag = CMD_START;
		op_cmd.boot_type = boot_type;
		push_oper_cmd(am_idx, &op_cmd);
	}

	ps->boot_type = boot_type;
	applog(APPLOG_INFO, "app", "start am=%d boot_type=%d test_type=%d", am_idx, boot_type, ps->test_type);

	if (ram_addr) {
		set_ram_shm_addr(am_idx, 0);
		set_ram_console_addr(am_idx, ram_addr);
		start_ram_console_thread(am_idx);
	} else if (rpmsg_mode) {
		set_ram_shm_addr(am_idx, 0);
		applog(APPLOG_INFO, "rpmsg", "round-trip test started");
	} else {
		applog(APPLOG_INFO, "shm", "config boot_type=%d addr=0x%llx", boot_type,
		       (unsigned long long)shm_addr);
		set_ram_shm_addr(am_idx, shm_addr);
	}

	return 0;
}

/*
 * Wait for the compositor socket before touching LVGL.
 *
 * lv_wayland_window_create() connects once and does not retry, so if Weston
 * has not created its socket yet the app blocks forever with no display and
 * no diagnostic -- the container reports "Up" while only the startup banner
 * ever reaches the log.
 *
 * This is not hypothetical. When the i.MX93 MediaMix probe times out
 * ("imx93-blk-ctrl ... probe ... failed with error -110") the display-init
 * helper has to rebind the controller, which pushes Weston's socket past this
 * container's start. Compose gating on weston being "healthy" does not help:
 * the healthcheck passes before the socket is usable.
 *
 * On timeout we continue rather than exit. Exiting would just restart into
 * the same race, and continuing preserves the previous behaviour -- but now
 * the reason is in the log instead of being invisible.
 */
static int wait_for_wayland_display(int timeout_sec)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	const char *display = getenv("WAYLAND_DISPLAY");
	char path[512];
	int waited = 0;

	if (!runtime_dir || !display) {
		applog(APPLOG_WARN, "wayland", "XDG_RUNTIME_DIR or WAYLAND_DISPLAY unset; not waiting");
		return 0;
	}

	snprintf(path, sizeof(path), "%s/%s", runtime_dir, display);

	while (access(path, F_OK) != 0) {
		if (waited >= timeout_sec) {
			applog(APPLOG_ERROR, "wayland", "%s absent after %ds; starting anyway",
			       path, waited);
			return -1;
		}
		if (!waited)
			applog(APPLOG_INFO, "wayland", "waiting for %s", path);
		sleep(1);
		waited++;
	}

	applog(APPLOG_INFO, "wayland", "%s ready after %ds", path, waited);
	return 0;
}

int main(int argc, char **argv)
{
	core_session_t *ps = NULL;
	uint16_t start_flag = 0;
	uint8_t boot_type = 0;
	uint8_t am_idx = 0;
	int fail_cnt = 0;
	int run_flag = 0;
	int ret = 0;

	/* stdout is block-buffered when captured by docker logs; force unbuffered
	 * so diagnostic prints in the polling loop reach the log immediately. */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	for (am_idx = 0; am_idx < AM_MAX; am_idx++)
		get_test_type(am_idx) = 1;

	ret = parse_input_arg(argc, argv);
	if (ret)
		return ret;

	run_system_cmd(sprep_cmd);
	init_mem_mgr();

	init_boot_type();

	/* Weston may still be starting; see wait_for_wayland_display(). */
	wait_for_wayland_display(60);
	start_lvgl_gui();
	if (linux_jitter_start() || load_gen_start())
		applog(APPLOG_WARN, "app", "jitter/load threads not started");

	while (is_running()) {
		run_flag = 0;

		if (sdebug_mode & DEBUG_MODE(7)) {
			if (!(scur_sec % 35)) {
				boot_type = input_boot_type();
				if (boot_type)
					run_flag = 1;
				else
					break;
			}
		} else {
			ret = query_operation_status(&boot_type, &start_flag);
			if (ret) {
				if (start_flag)
					run_flag = 1;
				else
					stop_multi_core_prog(boot_type, CMD_STOP);
			}
		}

		if (run_flag) {
			fail_cnt = 0;
			start_multi_core_prog(boot_type, 0);
		}

		am_idx = BOOT_TYPE_AM_IDX(boot_type);
		if (is_boot_failed(am_idx) && (fail_cnt < 3)) {
			fail_cnt++;
			if (boot_type == TYPE_M_RTOS_RTLAT) {
				applog(APPLOG_ERROR, "m33", "boot failed; restarting (attempt %d/3)", fail_cnt);
				{
					char ev[32];

					snprintf(ev, sizeof(ev), "boot failed, retry %d/3", fail_cnt);
					log_view_m33_set_state("FAILED");
					log_view_m33_set_event(ev);
					log_view_m33_publish();
				}
				/* Filesystem gate is more robust than env: env vars only
				 * reach PID 1 via docker compose recreate, but a file at
				 * /tmp/rt_no_autorestart can be toggled with `docker exec
				 * touch` mid-run. Also survives the docker env quirks that
				 * bit us with RT_NO_AUTORESTART=1. */
				if (access("/tmp/rt_no_autorestart", F_OK) == 0 ||
				    getenv("RT_NO_AUTORESTART")) {
					applog(APPLOG_WARN, "m33", "auto-restart disabled; holding state (no reboot)");
					continue;
				}
				start_multi_core_prog(boot_type, 1);
			}
		}

		if (!(scur_sec % 10))
			applog(APPLOG_INFO, "app", "heartbeat uptime=%ds", scur_sec);

		output_shm_data();  /* Show latency data in share memory */

		if (get_boot_type(MC_IDX) == TYPE_M_RTOS_HELLO) {
			jitter_snapshot_t rtt;
			rpmsg_ping_counters_t tot = {0, 0};
			int got = rpmsg_ping_take(&rtt, &tot);

			if (got)
				applog(APPLOG_DEBUG, "rpmsg", "rtt n=%d min=%d mean=%d max=%d ns sent=%u ok=%u",
				       rtt.count, rtt.min_ns, rtt.mean_ns, rtt.max_ns,
				       (unsigned)tot.sent, (unsigned)tot.ok);

			log_view_rpmsg_publish("RUNNING", scur_sec - s_rpmsg_start_sec,
					       rpmsg_ping_linked(), got ? &rtt : NULL,
					       tot.sent, rpmsg_ping_lost(&tot),
					       rpmsg_ping_linked() ? "ping-pong ok" : "waiting for M33");
		}

		/* Cortex-A55-0 utilisation. Sampling only reads /proc, so it stays on this
		 * thread; cpu_load_ui_render() does the widget work on the LVGL thread.
		 * The console line reuses the A-core GUI log ring, which the Home terminal
		 * and Console_A1 already display. */
		if (cpu_load_sample()) {
			cpu_load_t load;
			jitter_snapshot_t jit;

			log_view_a55_set_loadgen(load_gen_get_level());
			if (linux_jitter_take(&jit))
				log_view_a55_set_jitter(jit.mean_ns, jit.max_ns);

			if (cpu_load_peek(&load))
				log_view_a55_publish(load.total, load.app, load.idle);
		}
#ifdef DEBUG_TEST
		if (!(scur_sec % 10)) {
			if (get_debug_mode() & DEBUG_MODE(3))
				dump_oper_cmd(am_idx, sboot_type_title);
		}
#endif
		scur_sec++;
		sleep(1);
	}

	for (am_idx = 0; am_idx < AM_MAX; am_idx++) {
		ps = get_core_session(am_idx);

		if (ps->boot_type)
			stop_multi_core_prog(ps->boot_type, CMD_STOP);
	}
	stop_ram_console_thread(AC_IDX);

	load_gen_stop();
	linux_jitter_stop();
	stop_lvgl_gui();
	exit_mem_mgr();

	run_system_cmd(spost_cmd);

	return 0;
}
