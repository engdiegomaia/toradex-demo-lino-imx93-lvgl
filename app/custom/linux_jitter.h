/*
 * linux_jitter - wake-up lateness of a Linux timer thread on A55-0.
 *
 * A thread sleeps until absolute 1 ms deadlines (CLOCK_MONOTONIC) and records
 * how late it woke. Under CPU load this is what a Linux task experiences, to
 * be shown next to the Cortex-M33 interrupt latency. Stats are a pure,
 * host-testable core; the thread is a thin wrapper.
 */
#ifndef LINUX_JITTER_H
#define LINUX_JITTER_H

#include <stdint.h>
#include <pthread.h>

typedef struct {
	pthread_mutex_t lock;
	int64_t min_ns, max_ns, sum_ns;
	int count;
} jitter_stats_t;

typedef struct {
	int32_t min_ns, mean_ns, max_ns;
	int count;
} jitter_snapshot_t;

void jitter_stats_init(jitter_stats_t *s);
/* Negative lateness is clamped to 0. NULL-safe. */
void jitter_stats_add(jitter_stats_t *s, int64_t lateness_ns);
/* Returns 1 and fills *out if any sample arrived; always resets the window. */
int jitter_stats_snapshot(jitter_stats_t *s, jitter_snapshot_t *out);
/* max(0, actual - deadline) */
int64_t jitter_lateness_ns(int64_t deadline_ns, int64_t actual_ns);

/* Deadline after `prev_deadline_ns`. Normally prev + period. When the thread
 * is more than JITTER_RESYNC_NS late it skips ahead to now + period, so an
 * overloaded system does not make the probe spin to catch up. */
#define JITTER_RESYNC_NS 20000000LL
int64_t jitter_next_deadline_ns(int64_t prev_deadline_ns, int64_t now_ns,
				int64_t period_ns);

/* Start/stop the measuring thread (idempotent). Samples go to the global stats. */
int linux_jitter_start(void);
void linux_jitter_stop(void);
/* Window since the last call; 1 if valid. */
int linux_jitter_take(jitter_snapshot_t *out);

#endif
