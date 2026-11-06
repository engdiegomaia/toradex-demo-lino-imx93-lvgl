/*
 * rpmsg_ping - Linux side of the A55 <-> Cortex-M33 RPMsg round-trip test.
 *
 * The M33 firmware (NXP rpmsg_lite_pingpong_rtos_linux, patched to echo
 * forever) announces the channel "rpmsg-openamp-demo-channel" at endpoint 30
 * and replies to every 4-byte counter with counter + 1. This module sends the
 * counter through an rpmsg_char endpoint, times the reply and keeps the
 * round-trip statistics (reusing jitter_stats_t). The message codec and the
 * counters are pure and host-tested; the thread needs the target.
 */
#ifndef RPMSG_PING_H
#define RPMSG_PING_H

#include <stdint.h>
#include "linux_jitter.h"

#define RPMSG_PING_MSG_SIZE 4
#define RPMSG_PING_CHANNEL "rpmsg-openamp-demo-channel"
#define RPMSG_PING_REMOTE_ADDR 30u

typedef struct {
	uint32_t sent;
	uint32_t ok;
} rpmsg_ping_counters_t;

/* Little-endian codec. Return the byte count / 1 on success, 0 on bad args. */
int rpmsg_ping_encode(uint32_t seq, uint8_t *buf, int cap);
int rpmsg_ping_decode(const uint8_t *buf, int len, uint32_t *seq);
/* The M33 answers with sent + 1 (wrapping). */
int rpmsg_ping_reply_matches(uint32_t sent, uint32_t reply);

void rpmsg_ping_count_sent(rpmsg_ping_counters_t *c);
void rpmsg_ping_count_ok(rpmsg_ping_counters_t *c);
uint32_t rpmsg_ping_lost(const rpmsg_ping_counters_t *c);

/* Thread control (idempotent). Needs a privileged container with pid: host. */
int rpmsg_ping_start(void);
void rpmsg_ping_stop(void);
/* Round-trip window since the last call (ns); 1 if any reply arrived. */
int rpmsg_ping_take(jitter_snapshot_t *rtt, rpmsg_ping_counters_t *totals);
/* 1 once the endpoint is open and replies are flowing. */
int rpmsg_ping_linked(void);

#endif
