#include "applog.h"

#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define APPLOG_LINE_MAX 256
#define APPLOG_ANSI_RESET "\033[0m"

static const char *const s_level_name[] = {"DEBUG", "INFO", "WARN", "ERROR"};
static const char *const s_level_color[] = {"\033[90m", "\033[32m",
					    "\033[33m", "\033[31m"};

static pthread_once_t s_once = PTHREAD_ONCE_INIT;
static applog_level_t s_threshold = APPLOG_INFO;
static int s_use_color;

static int level_in_range(applog_level_t level)
{
	return level >= APPLOG_DEBUG && level <= APPLOG_ERROR;
}

applog_level_t applog_parse_level(const char *name, applog_level_t fallback)
{
	size_t i;

	if (!name)
		return fallback;

	for (i = 0; i < sizeof(s_level_name) / sizeof(s_level_name[0]); i++) {
		if (strcasecmp(name, s_level_name[i]) == 0)
			return (applog_level_t)i;
	}
	return fallback;
}

size_t applog_format(char *out, size_t cap, const applog_stamp_t *stamp,
		     applog_level_t level, const char *component,
		     const char *msg, int use_color)
{
	char level_field[32];
	int n;

	if (!out || cap < 2) {
		if (out && cap == 1)
			out[0] = '\0';
		return 0;
	}

	if (!level_in_range(level))
		level = APPLOG_ERROR;
	if (!component)
		component = "-";
	if (!msg)
		msg = "";

	if (use_color)
		snprintf(level_field, sizeof(level_field), "%s%-5s%s",
			 s_level_color[level], s_level_name[level],
			 APPLOG_ANSI_RESET);
	else
		snprintf(level_field, sizeof(level_field), "%-5s",
			 s_level_name[level]);

	n = snprintf(out, cap, "%02d:%02d:%02d.%03d %s %-7s %s",
		     stamp->hour, stamp->min, stamp->sec, stamp->msec,
		     level_field, component, msg);
	if (n < 0) {
		out[0] = '\0';
		return 0;
	}

	/* Reserve room for the newline; truncated messages keep it. */
	if ((size_t)n > cap - 2)
		n = (int)(cap - 2);
	out[n] = '\n';
	out[n + 1] = '\0';
	return (size_t)n + 1;
}

void applog_format_latency(char *out, size_t cap, const char *label,
			   int32_t min_ns, int32_t mean_ns, int32_t max_ns)
{
	if (min_ns < 0)
		min_ns = 0;
	if (mean_ns < 0)
		mean_ns = 0;
	if (max_ns < 0)
		max_ns = 0;

	snprintf(out, cap, "%-9s  min %6d  mean %6d  max %6d ns",
		 label ? label : "-", (int)min_ns, (int)mean_ns, (int)max_ns);
}

static void applog_init(void)
{
	s_threshold = applog_parse_level(getenv("APPLOG_LEVEL"), APPLOG_INFO);
	s_use_color = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
}

void applog(applog_level_t level, const char *component, const char *fmt, ...)
{
	char msg[APPLOG_LINE_MAX];
	char line[APPLOG_LINE_MAX + 64];
	struct timespec ts;
	struct tm tm_now;
	applog_stamp_t stamp;
	va_list args;

	pthread_once(&s_once, applog_init);
	if (level < s_threshold)
		return;

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm_now);
	stamp.hour = tm_now.tm_hour;
	stamp.min = tm_now.tm_min;
	stamp.sec = tm_now.tm_sec;
	stamp.msec = (int)(ts.tv_nsec / 1000000L);

	applog_format(line, sizeof(line), &stamp, level, component, msg,
		      s_use_color);

	/* One write per line keeps lines intact across threads, and the flush
	 * keeps `docker logs` current when stdout is a pipe. */
	fputs(line, stdout);
	fflush(stdout);
}
