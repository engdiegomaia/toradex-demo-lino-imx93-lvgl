#include "log_frame.h"

#include <stdio.h>
#include <string.h>

#include "load_gen.h"
#include "terminal_buf.h"

#define FRAME_MAX 512
#define VALUE_MAX 999999
#define UPTIME_MAX 9999

static int clamp_int(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static int32_t clamp_ns(int32_t v)
{
	return v < 0 ? 0 : (v > VALUE_MAX ? VALUE_MAX : v);
}

static size_t finish(const char *tmp, char *out, size_t cap)
{
	size_t n = strlen(tmp);

	if (n >= cap)
		n = cap - 1;
	memcpy(out, tmp, n);
	out[n] = '\0';
	return n;
}

static void put_latency_row(char *dst, size_t cap, const char *label, int valid,
			    int32_t sched, int32_t delay)
{
	if (valid)
		snprintf(dst, cap, "%-9s  %6d  %6d", label, (int)clamp_ns(sched),
			 (int)clamp_ns(delay));
	else
		snprintf(dst, cap, "%-9s  %6s  %6s", label, "-", "-");
}

size_t log_frame_render_m33(const log_frame_m33_t *f, char *out, size_t cap)
{
	char tmp[FRAME_MAX];
	char load[32], rule[LOG_FRAME_MAX_COLS + 1];
	char min_row[48], mean_row[48], max_row[48];

	if (out && cap > 0)
		out[0] = '\0';
	if (!f || !out || cap < 2)
		return 0;

	memset(rule, '-', LOG_FRAME_MAX_COLS);
	rule[LOG_FRAME_MAX_COLS] = '\0';

	if (f->has_load)
		snprintf(load, sizeof(load), "%6.2f", f->load_pct);
	else
		snprintf(load, sizeof(load), "%6s", "-");

	/* Statistics down the rows, source (sched/delay) across the columns:
	 * two value columns instead of three keeps the frame within 25 cols. */
	put_latency_row(min_row, sizeof(min_row), "min", f->has_latency,
			f->sched_min, f->delay_min);
	put_latency_row(mean_row, sizeof(mean_row), "mean", f->has_latency,
			f->sched_mean, f->delay_mean);
	put_latency_row(max_row, sizeof(max_row), "max", f->has_latency,
			f->sched_max, f->delay_max);

	snprintf(tmp, sizeof(tmp),
		 "%c"
		 "STATE    %.*s\n"
		 "UPTIME   %04d s\n"
		 "LOAD     %s %%\n"
		 "%s\n"
		 "%-9s  %6s  %6s\n"
		 "%s\n"
		 "%s\n"
		 "%s\n"
		 "%s\n"
		 "> %.*s",
		 TERMINAL_BUF_CLEAR, LOG_FRAME_STATE_COLS, f->state,
		 clamp_int(f->uptime_s, 0, UPTIME_MAX), load, rule, "LAT (ns)",
		 "sched", "delay", min_row, mean_row, max_row, rule,
		 LOG_FRAME_EVENT_COLS, f->event);

	return finish(tmp, out, cap);
}

size_t log_frame_render_a55(const log_frame_a55_t *f, char *out, size_t cap)
{
	char tmp[FRAME_MAX];
	char gen[8], jmean[16], jmax[16];

	if (out && cap > 0)
		out[0] = '\0';
	if (!f || !out || cap < 2)
		return 0;

	snprintf(gen, sizeof(gen), "%s", load_gen_level_name(f->load_level));
	if (f->has_jitter) {
		snprintf(jmean, sizeof(jmean), "%6d", (int)clamp_ns(f->jit_mean));
		snprintf(jmax, sizeof(jmax), "%6d", (int)clamp_ns(f->jit_max));
	} else {
		snprintf(jmean, sizeof(jmean), "%6s", "-");
		snprintf(jmax, sizeof(jmax), "%6s", "-");
	}

	snprintf(tmp, sizeof(tmp),
		 "%c"
		 "CORE     Cortex-A55-0\n"
		 "TOTAL    %3d %%\n"
		 "APP      %3d %%\n"
		 "IDLE     %3d %%\n"
		 "LOADGEN  %s\n"
		 "JIT mean %s ns\n"
		 "JIT max  %s ns",
		 TERMINAL_BUF_CLEAR, clamp_int(f->total, 0, 100),
		 clamp_int(f->app, 0, 100), clamp_int(f->idle, 0, 100), gen,
		 jmean, jmax);

	return finish(tmp, out, cap);
}

/* RTT is shown in microseconds: round trips run from ~50 us to several ms,
 * which saturates a nanosecond column. */
static int ns_to_us(int32_t ns)
{
	int64_t us = ((int64_t)(ns < 0 ? 0 : ns) + 500) / 1000;

	return us > VALUE_MAX ? VALUE_MAX : (int)us;
}

static void put_rtt_row(char *dst, size_t cap, const char *label, int valid,
			int32_t v)
{
	if (valid)
		snprintf(dst, cap, "%-6s %8d", label, ns_to_us(v));
	else
		snprintf(dst, cap, "%-6s %8s", label, "--");
}

size_t log_frame_render_rpmsg(const log_frame_rpmsg_t *f, char *out, size_t cap)
{
	char tmp[FRAME_MAX], rule[LOG_FRAME_MAX_COLS + 1];
	char r_min[32], r_mean[32], r_max[32];
	unsigned sent, lost;

	if (out && cap > 0)
		out[0] = '\0';
	if (!f || !out || cap < 2)
		return 0;

	memset(rule, '-', LOG_FRAME_MAX_COLS);
	rule[LOG_FRAME_MAX_COLS] = '\0';
	put_rtt_row(r_min, sizeof(r_min), "min", f->has_rtt, f->rtt_min);
	put_rtt_row(r_mean, sizeof(r_mean), "mean", f->has_rtt, f->rtt_mean);
	put_rtt_row(r_max, sizeof(r_max), "max", f->has_rtt, f->rtt_max);
	sent = f->sent > 999999u ? 999999u : f->sent;
	lost = f->lost > 99999u ? 99999u : f->lost;

	snprintf(tmp, sizeof(tmp),
		 "%c"
		 "STATE    %.*s\n"
		 "UPTIME   %04d s\n"
		 "LINK     %s\n"
		 "%s\n"
		 "RTT (us)\n"
		 "%s\n"
		 "%s\n"
		 "%s\n"
		 "%s\n"
		 "SENT %06u  LOST %05u\n"
		 "> %.*s",
		 TERMINAL_BUF_CLEAR, LOG_FRAME_STATE_COLS, f->state,
		 clamp_int(f->uptime_s, 0, UPTIME_MAX), f->linked ? "UP" : "WAIT",
		 rule, r_min, r_mean, r_max, rule, sent, lost,
		 LOG_FRAME_EVENT_COLS, f->event);

	return finish(tmp, out, cap);
}
