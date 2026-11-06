/*
 * load_gen - controllable busy-loop load on Cortex-A55-0.
 *
 * A worker pinned to CPU 0 burns CPU for level% of every 10 ms period. Levels
 * cycle OFF -> 25 -> 50 -> 100 -> OFF so one button drives it. The duty-cycle
 * arithmetic is pure and host-tested.
 */
#ifndef LOAD_GEN_H
#define LOAD_GEN_H

#include <stdint.h>

#define LOAD_GEN_PERIOD_NS 10000000LL

int64_t load_gen_busy_ns(int level_pct, int64_t period_ns);
int load_gen_level_next(int level_pct);
const char *load_gen_level_name(int level_pct); /* <= 4 chars */

/* Thread control (idempotent). level 0 keeps the thread idle. */
int load_gen_start(void);
void load_gen_stop(void);
void load_gen_set_level(int level_pct);
int load_gen_get_level(void);

#endif
