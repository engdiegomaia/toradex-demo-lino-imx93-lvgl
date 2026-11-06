#include "log_view.h"

#include <string.h>

#include "log_frame.h"
#include "log_tools.h"

static log_frame_m33_t s_m33;

static void copy_field(char *dst, size_t cap, const char *src)
{
	if (!src)
		src = "";
	strncpy(dst, src, cap - 1);
	dst[cap - 1] = '\0';
}

void log_view_m33_reset(const char *state, const char *event)
{
	memset(&s_m33, 0, sizeof(s_m33));
	copy_field(s_m33.state, sizeof(s_m33.state), state);
	copy_field(s_m33.event, sizeof(s_m33.event), event);
}

void log_view_m33_set_state(const char *state)
{
	copy_field(s_m33.state, sizeof(s_m33.state), state);
}

void log_view_m33_set_event(const char *event)
{
	copy_field(s_m33.event, sizeof(s_m33.event), event);
}

void log_view_m33_set_uptime(int seconds)
{
	s_m33.uptime_s = seconds;
}

void log_view_m33_set_load(double load_pct)
{
	s_m33.has_load = 1;
	s_m33.load_pct = load_pct;
}

void log_view_m33_set_latency(const rt_latency_shm_stat *delay,
			      const rt_latency_shm_stat *sched)
{
	if (!delay || !sched)
		return;

	s_m33.has_latency = 1;
	s_m33.delay_min = delay->min;
	s_m33.delay_mean = delay->mean;
	s_m33.delay_max = delay->max;
	s_m33.sched_min = sched->min;
	s_m33.sched_mean = sched->mean;
	s_m33.sched_max = sched->max;
}

void log_view_m33_publish(void)
{
	char frame[LOG_BUF_SIZE];

	if (log_frame_render_m33(&s_m33, frame, sizeof(frame)))
		log_printf(MC_IDX, "%s", frame);
}

void log_view_a55_publish(int total, int app, int idle)
{
	log_frame_a55_t f = {total, app, idle};
	char frame[LOG_BUF_SIZE];

	if (log_frame_render_a55(&f, frame, sizeof(frame)))
		log_printf(AC_IDX, "%s", frame);
}
