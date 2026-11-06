#define _GNU_SOURCE
#include "rpmsg_ping.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

#include "applog.h"

#define NSEC_PER_SEC 1000000000LL
#define PING_PERIOD_US 10000
#define REPLY_TIMEOUT_MS 200

int rpmsg_ping_encode(uint32_t seq, uint8_t *buf, int cap)
{
	if (!buf || cap < RPMSG_PING_MSG_SIZE)
		return 0;
	buf[0] = (uint8_t)seq;
	buf[1] = (uint8_t)(seq >> 8);
	buf[2] = (uint8_t)(seq >> 16);
	buf[3] = (uint8_t)(seq >> 24);
	return RPMSG_PING_MSG_SIZE;
}

int rpmsg_ping_decode(const uint8_t *buf, int len, uint32_t *seq)
{
	if (!buf || !seq || len < RPMSG_PING_MSG_SIZE)
		return 0;
	*seq = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
	       ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
	return 1;
}

int rpmsg_ping_reply_matches(uint32_t sent, uint32_t reply)
{
	return reply == (uint32_t)(sent + 1u);
}

void rpmsg_ping_count_sent(rpmsg_ping_counters_t *c)
{
	if (c)
		c->sent++;
}

void rpmsg_ping_count_ok(rpmsg_ping_counters_t *c)
{
	if (c)
		c->ok++;
}

uint32_t rpmsg_ping_lost(const rpmsg_ping_counters_t *c)
{
	return (c && c->sent > c->ok) ? c->sent - c->ok : 0;
}

/* ------------------------------------------------------------ target ---- */

struct rpmsg_endpoint_info {
	char name[32];
	uint32_t src;
	uint32_t dst;
};
#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)
#define RPMSG_ADDR_ANY 0xFFFFFFFFu

static pthread_t s_thread;
static volatile int s_running, s_linked;
static int s_started;
static jitter_stats_t s_rtt;
static rpmsg_ping_counters_t s_totals;
static pthread_mutex_t s_totals_lock = PTHREAD_MUTEX_INITIALIZER;

static int64_t now_ns(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (int64_t)t.tv_sec * NSEC_PER_SEC + t.tv_nsec;
}

/* The container's /dev is a snapshot: nodes created after it started are
 * missing, so create the node from the sysfs "dev" major:minor. */
static int ensure_node(const char *sysdir, const char *node)
{
	char path[128], buf[32];
	unsigned int maj, min;
	FILE *f;

	if (access(node, F_OK) == 0)
		return 0;
	snprintf(path, sizeof(path), "%s/dev", sysdir);
	f = fopen(path, "r");
	if (!f)
		return -1;
	if (!fgets(buf, sizeof(buf), f) || sscanf(buf, "%u:%u", &maj, &min) != 2) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return mknod(node, S_IFCHR | 0600, makedev(maj, min));
}

/* Find the rpmsgN class entry created for our channel name. */
static int find_eptdev(char *sysdir, size_t cap)
{
	DIR *d = opendir("/sys/class/rpmsg");
	struct dirent *e;
	int found = -1;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char p[320], name[64] = "";
		FILE *f;

		if (strncmp(e->d_name, "rpmsg", 5) != 0 ||
		    strncmp(e->d_name, "rpmsg_ctrl", 10) == 0)
			continue;
		snprintf(p, sizeof(p), "/sys/class/rpmsg/%s/name", e->d_name);
		f = fopen(p, "r");
		if (!f)
			continue;
		if (fgets(name, sizeof(name), f) &&
		    strncmp(name, RPMSG_PING_CHANNEL, strlen(RPMSG_PING_CHANNEL)) == 0) {
			snprintf(sysdir, cap, "/sys/class/rpmsg/%s", e->d_name);
			found = 0;
		}
		fclose(f);
		if (found == 0)
			break;
	}
	closedir(d);
	return found;
}

static int open_endpoint(int *ctrl_fd_out)
{
	struct rpmsg_endpoint_info info;
	char sysdir[128], node[192];
	int ctrl, fd, tries;

	/* Modules are only built, not loaded, on Torizon. Best effort. */
	(void)system("nsenter -t 1 -m -- modprobe rpmsg_char >/dev/null 2>&1; "
		     "nsenter -t 1 -m -- modprobe rpmsg_ctrl >/dev/null 2>&1");

	for (tries = 0; tries < 100 && s_running; tries++) {
		if (ensure_node("/sys/class/rpmsg/rpmsg_ctrl0", "/dev/rpmsg_ctrl0") == 0)
			break;
		usleep(100000);
	}
	ctrl = open("/dev/rpmsg_ctrl0", O_RDWR);
	if (ctrl < 0)
		return -1;

	memset(&info, 0, sizeof(info));
	snprintf(info.name, sizeof(info.name), "%s", RPMSG_PING_CHANNEL);
	info.src = RPMSG_ADDR_ANY;
	info.dst = RPMSG_PING_REMOTE_ADDR;
	if (ioctl(ctrl, RPMSG_CREATE_EPT_IOCTL, &info) < 0) {
		close(ctrl);
		return -1;
	}
	if (find_eptdev(sysdir, sizeof(sysdir)) < 0) {
		close(ctrl);
		return -1;
	}
	snprintf(node, sizeof(node), "/dev/%s", strrchr(sysdir, '/') + 1);
	if (ensure_node(sysdir, node) != 0 && access(node, F_OK) != 0) {
		close(ctrl);
		return -1;
	}
	fd = open(node, O_RDWR);
	if (fd < 0) {
		close(ctrl);
		return -1;
	}
	*ctrl_fd_out = ctrl;
	return fd;
}

static void *ping_main(void *arg)
{
	int ctrl = -1, fd = -1;
	uint32_t seq = 0;
	uint8_t tx[RPMSG_PING_MSG_SIZE], rx[64];

	(void)arg;
	while (s_running && fd < 0) {
		fd = open_endpoint(&ctrl);
		if (fd < 0)
			sleep(1);
	}
	if (fd < 0)
		return NULL;
	applog(APPLOG_INFO, "rpmsg", "endpoint open, ping dst=%u", RPMSG_PING_REMOTE_ADDR);

	/* The M33 waits for a first message (hello) before it starts echoing. */
	(void)write(fd, "hello", 5);

	while (s_running) {
		struct pollfd pfd = {.fd = fd, .events = POLLIN};
		int64_t t0;

		rpmsg_ping_encode(seq, tx, sizeof(tx));
		t0 = now_ns();
		if (write(fd, tx, RPMSG_PING_MSG_SIZE) != RPMSG_PING_MSG_SIZE)
			break;
		pthread_mutex_lock(&s_totals_lock);
		rpmsg_ping_count_sent(&s_totals);
		pthread_mutex_unlock(&s_totals_lock);

		if (poll(&pfd, 1, REPLY_TIMEOUT_MS) > 0) {
			ssize_t n = read(fd, rx, sizeof(rx));
			uint32_t reply;

			if (n > 0 && rpmsg_ping_decode(rx, (int)n, &reply) &&
			    rpmsg_ping_reply_matches(seq, reply)) {
				jitter_stats_add(&s_rtt, now_ns() - t0);
				pthread_mutex_lock(&s_totals_lock);
				rpmsg_ping_count_ok(&s_totals);
				pthread_mutex_unlock(&s_totals_lock);
				s_linked = 1;
			}
		}
		seq++;
		usleep(PING_PERIOD_US);
	}
	s_linked = 0;
	close(fd);
	ioctl(ctrl, RPMSG_DESTROY_EPT_IOCTL);
	close(ctrl);
	return NULL;
}

int rpmsg_ping_start(void)
{
	if (s_started)
		return 0;
	jitter_stats_init(&s_rtt);
	memset(&s_totals, 0, sizeof(s_totals));
	s_running = 1;
	if (pthread_create(&s_thread, NULL, ping_main, NULL) != 0) {
		s_running = 0;
		return -1;
	}
	s_started = 1;
	return 0;
}

void rpmsg_ping_stop(void)
{
	if (!s_started)
		return;
	s_running = 0;
	pthread_join(s_thread, NULL);
	s_started = 0;
	s_linked = 0;
}

int rpmsg_ping_take(jitter_snapshot_t *rtt, rpmsg_ping_counters_t *totals)
{
	if (!s_started)
		return 0;
	if (totals) {
		pthread_mutex_lock(&s_totals_lock);
		*totals = s_totals;
		pthread_mutex_unlock(&s_totals_lock);
	}
	return rtt ? jitter_stats_snapshot(&s_rtt, rtt) : 0;
}

int rpmsg_ping_linked(void)
{
	return s_linked;
}
