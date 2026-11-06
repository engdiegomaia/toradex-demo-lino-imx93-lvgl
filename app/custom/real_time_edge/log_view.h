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

/* Clear the Cortex-M33 frame to the given state/event (no data yet). */
void log_view_m33_reset(const char *state, const char *event);
void log_view_m33_set_state(const char *state);
void log_view_m33_set_event(const char *event);
void log_view_m33_set_uptime(int seconds);
void log_view_m33_set_load(double load_pct);
void log_view_m33_set_latency(const rt_latency_shm_stat *delay,
			      const rt_latency_shm_stat *sched);
void log_view_m33_publish(void);

/* Cortex-A55-0 utilisation frame, in percent. Publishes immediately. */
void log_view_a55_publish(int total, int app, int idle);

#endif /* LOG_VIEW_H */
