/*
 * log_frame - fixed-layout "frames" for the on-screen log terminals.
 *
 * Instead of scrolling lines, each terminal shows one frame that is redrawn
 * from scratch on every update. Labels and numeric columns never move; only
 * the values change. Text messages live in a fixed row (EVENT) and are
 * truncated to its width. Every frame starts with TERMINAL_BUF_CLEAR so the
 * terminal drops the previous frame.
 *
 * No LVGL dependency, so it is unit-tested on the host.
 */
#ifndef LOG_FRAME_H
#define LOG_FRAME_H

#include <stddef.h>
#include <stdint.h>

/* Widest frame line. The Home terminal wrapped at ~27 columns with the
 * 16 px font, so every line stays within this. */
#define LOG_FRAME_MAX_COLS 25
/* The event row is "> " plus the text. */
#define LOG_FRAME_EVENT_COLS (LOG_FRAME_MAX_COLS - 2)
#define LOG_FRAME_STATE_COLS 10

typedef struct {
	char state[LOG_FRAME_STATE_COLS + 1]; /* BOOTING, RUNNING, FAILED, ... */
	int uptime_s;
	int has_load;
	double load_pct;
	int has_latency;
	int32_t sched_min, sched_mean, sched_max; /* ns */
	int32_t delay_min, delay_mean, delay_max; /* ns */
	char event[64];
} log_frame_m33_t;

typedef struct {
	int total; /* percent */
	int app;
	int idle;
	int load_level;  /* load generator duty: 0 (OFF), 25, 50, 100 */
	int has_jitter;  /* Linux timer jitter measured this window */
	int32_t jit_mean, jit_max; /* ns */
} log_frame_a55_t;

typedef struct {
	char state[LOG_FRAME_STATE_COLS + 1];
	int uptime_s;
	int linked;   /* replies are flowing */
	int has_rtt;
	int32_t rtt_min, rtt_mean, rtt_max; /* ns */
	uint32_t sent, lost;
	char event[64];
} log_frame_rpmsg_t;

/* Render into out (NUL-terminated, truncated to cap - 1). Return the length,
 * or 0 for NULL/too-small arguments. */
size_t log_frame_render_m33(const log_frame_m33_t *f, char *out, size_t cap);
size_t log_frame_render_rpmsg(const log_frame_rpmsg_t *f, char *out, size_t cap);
size_t log_frame_render_a55(const log_frame_a55_t *f, char *out, size_t cap);

#endif /* LOG_FRAME_H */
