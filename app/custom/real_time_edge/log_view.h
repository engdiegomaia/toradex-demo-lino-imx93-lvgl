/*
 * log_view - live state behind the on-screen log frames (see log_frame.h).
 *
 * Producers update fields and call publish; one complete frame is pushed
 * through the GUI log ring and replaces the previous frame in the terminal.
 * Call from the main loop thread only (no locking).
 */
#ifndef LOG_VIEW_H
#define LOG_VIEW_H

#include "basedef.h"
#include "linux_jitter.h"

/* Clear the Cortex-M33 frame to the given state/event (no data yet). */
void log_view_m33_reset(const char *state, const char *event);
void log_view_m33_set_state(const char *state);
void log_view_m33_set_event(const char *event);
void log_view_m33_set_uptime(int seconds);
void log_view_m33_set_load(double load_pct);
void log_view_m33_set_latency(const rt_latency_shm_stat *delay,
			      const rt_latency_shm_stat *sched);
void log_view_m33_publish(void);

/* RPMsg round-trip frame on the Cortex-M33 terminal. Publishes immediately.
 * rtt may be NULL when no reply arrived in this window (last values are kept). */
void log_view_rpmsg_publish(const char *state, int uptime_s, int linked,
			    const jitter_snapshot_t *rtt, uint32_t sent,
			    uint32_t lost, const char *event);

/* Cortex-A55-0 utilisation frame, in percent. Publishes immediately. */
void log_view_a55_publish(int total, int app, int idle);
/* Remember the last load-generator level and Linux jitter window; they are
 * shown on the A55 frame on the next publish. has_jitter == 0 keeps the last
 * jitter values. */
void log_view_a55_set_loadgen(int level_pct);
void log_view_a55_set_jitter(int32_t mean_ns, int32_t max_ns);

#endif /* LOG_VIEW_H */
