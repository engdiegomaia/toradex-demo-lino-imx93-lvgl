#include "log_frame.h"

#include <stdio.h>
#include <string.h>

#include "terminal_buf.h"

#define FRAME_MAX 512
#define RULE_COLS 33
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
			    int32_t a, int32_t b, int32_t c)
{
	if (valid)
		snprintf(dst, cap, "%-9s  %6d  %6d  %6d", label, (int)clamp_ns(a),
			 (int)clamp_ns(b), (int)clamp_ns(c));
	else
		snprintf(dst, cap, "%-9s  %6s  %6s  %6s", label, "-", "-", "-");
}

size_t log_frame_render_m33(const log_frame_m33_t *f, char *out, size_t cap)
{
	char tmp[FRAME_MAX];
	char load[32], sched[48], delay[48], rule[RULE_COLS + 1];

	if (out && cap > 0)
		out[0] = '\0';
	if (!f || !out || cap < 2)
		return 0;

	memset(rule, '-', RULE_COLS);
	rule[RULE_COLS] = '\0';

	if (f->has_load)
		snprintf(load, sizeof(load), "%6.2f", f->load_pct);
	else
		snprintf(load, sizeof(load), "%6s", "-");

	put_latency_row(sched, sizeof(sched), "irq_sched", f->has_latency,
			f->sched_min, f->sched_mean, f->sched_max);
	put_latency_row(delay, sizeof(delay), "irq_delay", f->has_latency,
			f->delay_min, f->delay_mean, f->delay_max);

	snprintf(tmp, sizeof(tmp),
		 "%c"
		 "STATE    %-*.*s  t=%04ds\n"
		 "LOAD     %s %%\n"
		 "%s\n"
		 "%-9s  %6s  %6s  %6s\n"
		 "%s\n"
		 "%s\n"
		 "EVENT    %.*s",
		 TERMINAL_BUF_CLEAR,
		 LOG_FRAME_STATE_COLS, LOG_FRAME_STATE_COLS, f->state,
		 clamp_int(f->uptime_s, 0, UPTIME_MAX), load, rule, "LAT (ns)",
		 "min", "mean", "max", sched, delay, LOG_FRAME_EVENT_COLS,
		 f->event);

	return finish(tmp, out, cap);
}

size_t log_frame_render_a55(const log_frame_a55_t *f, char *out, size_t cap)
{
	char tmp[FRAME_MAX];

	if (out && cap > 0)
		out[0] = '\0';
	if (!f || !out || cap < 2)
		return 0;

	snprintf(tmp, sizeof(tmp),
		 "%c"
		 "CORE     Cortex-A55-0\n"
		 "TOTAL    %3d %%\n"
		 "APP      %3d %%\n"
		 "IDLE     %3d %%",
		 TERMINAL_BUF_CLEAR, clamp_int(f->total, 0, 100),
		 clamp_int(f->app, 0, 100), clamp_int(f->idle, 0, 100));

	return finish(tmp, out, cap);
}
