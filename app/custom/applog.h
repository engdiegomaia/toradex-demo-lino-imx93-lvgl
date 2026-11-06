/*
 * applog - leveled, column-aligned logging for the HMI backend.
 *
 * One line per event:
 *
 *   14:21:07.412 INFO  shm     handshake ok flag=2 wr_idx=9
 *   14:21:08.413 WARN  m33     latency spike irq_sched max=48000 ns
 *
 * The threshold comes from the APPLOG_LEVEL environment variable
 * (debug|info|warn|error, default info). ANSI colour is used only when stdout
 * is a TTY and NO_COLOR is unset, so `docker logs` stays plain text.
 *
 * This file has no LVGL dependency so it can be unit-tested on the host.
 */
#ifndef APPLOG_H
#define APPLOG_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
	APPLOG_DEBUG = 0,
	APPLOG_INFO,
	APPLOG_WARN,
	APPLOG_ERROR
} applog_level_t;

typedef struct {
	int hour;
	int min;
	int sec;
	int msec;
} applog_stamp_t;

/* Log one event. printf-style; a trailing newline is added automatically. */
void applog(applog_level_t level, const char *component, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

/* Pure formatter: writes "<stamp> <LEVEL> <component> <msg>\n" into out.
 * Always NUL-terminates (when cap > 0) and keeps a trailing newline even if
 * the message is truncated. Returns the resulting string length. */
size_t applog_format(char *out, size_t cap, const applog_stamp_t *stamp,
		     applog_level_t level, const char *component,
		     const char *msg, int use_color);

/* Case-insensitive level name -> level; returns fallback for unknown input. */
applog_level_t applog_parse_level(const char *name, applog_level_t fallback);

/* Fixed-width latency row with explicit unit, e.g.
 *   "irq_sched  min   1200  mean   1800  max   4100 ns"
 * Negative inputs are clamped to 0. */
void applog_format_latency(char *out, size_t cap, const char *label,
			   int32_t min_ns, int32_t mean_ns, int32_t max_ns);

#endif /* APPLOG_H */
