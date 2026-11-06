#include "linux_jitter.h"

#include <string.h>
#include <time.h>

#define JITTER_PERIOD_NS 1000000LL
#define NSEC_PER_SEC 1000000000LL

void jitter_stats_init(jitter_stats_t *s)
{
	if (!s)
		return;
	memset(s, 0, sizeof(*s));
	pthread_mutex_init(&s->lock, NULL);
}

int64_t jitter_lateness_ns(int64_t deadline_ns, int64_t actual_ns)
{
	return actual_ns > deadline_ns ? actual_ns - deadline_ns : 0;
}

void jitter_stats_add(jitter_stats_t *s, int64_t lateness_ns)
{
	if (!s)
		return;
	if (lateness_ns < 0)
		lateness_ns = 0;
	pthread_mutex_lock(&s->lock);
	if (s->count == 0 || lateness_ns < s->min_ns)
		s->min_ns = lateness_ns;
	if (s->count == 0 || lateness_ns > s->max_ns)
		s->max_ns = lateness_ns;
	s->sum_ns += lateness_ns;
	s->count++;
	pthread_mutex_unlock(&s->lock);
}

int jitter_stats_snapshot(jitter_stats_t *s, jitter_snapshot_t *out)
{
	int ok = 0;

	if (!s || !out)
		return 0;
	pthread_mutex_lock(&s->lock);
	if (s->count > 0) {
		out->min_ns = (int32_t)s->min_ns;
		out->max_ns = (int32_t)s->max_ns;
		out->mean_ns = (int32_t)(s->sum_ns / s->count);
		out->count = s->count;
		ok = 1;
	}
	s->min_ns = s->max_ns = s->sum_ns = 0;
	s->count = 0;
	pthread_mutex_unlock(&s->lock);
	return ok;
}

int64_t jitter_next_deadline_ns(int64_t prev_deadline_ns, int64_t now_ns,
				int64_t period_ns)
{
	int64_t next = prev_deadline_ns + period_ns;

	if (now_ns - next > JITTER_RESYNC_NS)
		return now_ns + period_ns;
	return next;
}

static jitter_stats_t s_stats;
static pthread_t s_thread;
static volatile int s_running;
static int s_started;

static int64_t to_ns(const struct timespec *t)
{
	return (int64_t)t->tv_sec * NSEC_PER_SEC + t->tv_nsec;
}

static void *jitter_main(void *arg)
{
	struct timespec next, now;
	int64_t deadline;

	(void)arg;
	clock_gettime(CLOCK_MONOTONIC, &now);
	deadline = to_ns(&now);
	while (s_running) {
		deadline = jitter_next_deadline_ns(deadline, to_ns(&now), JITTER_PERIOD_NS);
		next.tv_sec = deadline / NSEC_PER_SEC;
		next.tv_nsec = deadline % NSEC_PER_SEC;
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
		clock_gettime(CLOCK_MONOTONIC, &now);
		jitter_stats_add(&s_stats, jitter_lateness_ns(deadline, to_ns(&now)));
	}
	return NULL;
}

int linux_jitter_start(void)
{
	if (s_started)
		return 0;
	jitter_stats_init(&s_stats);
	s_running = 1;
	if (pthread_create(&s_thread, NULL, jitter_main, NULL) != 0) {
		s_running = 0;
		return -1;
	}
	s_started = 1;
	return 0;
}

void linux_jitter_stop(void)
{
	if (!s_started)
		return;
	s_running = 0;
	pthread_join(s_thread, NULL);
	s_started = 0;
}

int linux_jitter_take(jitter_snapshot_t *out)
{
	return s_started ? jitter_stats_snapshot(&s_stats, out) : 0;
}
