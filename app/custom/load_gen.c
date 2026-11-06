#define _GNU_SOURCE
#include "load_gen.h"

#include <pthread.h>
#include <sched.h>
#include <time.h>

#define NSEC_PER_SEC 1000000000LL

static const int s_levels[] = {0, 25, 50, 100};
#define N_LEVELS ((int)(sizeof(s_levels) / sizeof(s_levels[0])))

int64_t load_gen_busy_ns(int level_pct, int64_t period_ns)
{
	if (level_pct <= 0)
		return 0;
	if (level_pct >= 100)
		return period_ns;
	return period_ns * level_pct / 100;
}

int load_gen_level_next(int level_pct)
{
	int i;

	for (i = 0; i < N_LEVELS; i++)
		if (s_levels[i] == level_pct)
			return s_levels[(i + 1) % N_LEVELS];
	return 0;
}

const char *load_gen_level_name(int level_pct)
{
	switch (level_pct) {
	case 25:  return "25%";
	case 50:  return "50%";
	case 100: return "100%";
	default:  return "OFF";
	}
}

static pthread_t s_thread;
static volatile int s_running, s_level;
static int s_started;

static int64_t now_ns(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (int64_t)t.tv_sec * NSEC_PER_SEC + t.tv_nsec;
}

static void *load_main(void *arg)
{
	cpu_set_t set;

	(void)arg;
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

	while (s_running) {
		int64_t busy = load_gen_busy_ns(s_level, LOAD_GEN_PERIOD_NS);
		int64_t start = now_ns();
		struct timespec rest = {0, (long)(LOAD_GEN_PERIOD_NS - busy)};

		while (now_ns() - start < busy)
			; /* burn */
		if (rest.tv_nsec > 0)
			nanosleep(&rest, NULL);
	}
	return NULL;
}

int load_gen_start(void)
{
	if (s_started)
		return 0;
	s_running = 1;
	if (pthread_create(&s_thread, NULL, load_main, NULL) != 0) {
		s_running = 0;
		return -1;
	}
	s_started = 1;
	return 0;
}

void load_gen_stop(void)
{
	if (!s_started)
		return;
	s_running = 0;
	pthread_join(s_thread, NULL);
	s_started = 0;
	s_level = 0;
}

void load_gen_set_level(int level_pct) { s_level = level_pct; }
int load_gen_get_level(void) { return s_level; }
