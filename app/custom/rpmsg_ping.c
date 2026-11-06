#define _GNU_SOURCE
#include "rpmsg_ping.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
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

rpmsg_ping_mode_t rpmsg_ping_mode_of(const char *devname)
{
	if (!devname)
		return RPMSG_PING_MODE_NONE;
	if (strstr(devname, RPMSG_PING_CHANNEL_FREERTOS))
		return RPMSG_PING_MODE_INCREMENT;
	if (strstr(devname, RPMSG_PING_CHANNEL_ZEPHYR))
		return RPMSG_PING_MODE_ECHO;
	return RPMSG_PING_MODE_NONE;
}

uint32_t rpmsg_ping_dst_of(const char *devname)
{
	const char *dot = devname ? strrchr(devname, '.') : NULL;

	return (dot && dot[1]) ? (uint32_t)strtoul(dot + 1, NULL, 10) : 0;
}

uint32_t rpmsg_ping_expected_reply(uint32_t sent, rpmsg_ping_mode_t mode)
{
	return mode == RPMSG_PING_MODE_INCREMENT ? (uint32_t)(sent + 1u) : sent;
}

int rpmsg_ping_virtio_of(const char *devname, char *out, int cap)
{
	int n = 0;

	if (!devname || !out || strncmp(devname, "virtio", 6) != 0)
		return 0;
	while (devname[6 + n] >= '0' && devname[6 + n] <= '9')
		n++;
	if (n == 0 || devname[6 + n] != '.' || 6 + n + 1 > cap)
		return 0;
	memcpy(out, devname, (size_t)(6 + n));
	out[6 + n] = '\0';
	return 1;
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
static int find_eptdev(const char *channel, char *sysdir, size_t cap)
{
	DIR *d = opendir("/sys/class/rpmsg");
	struct dirent *e;
	int found = -1, best = -1;

	if (!d)
		return -1;
	/* Stale endpoints survive a restart of this process, so take the newest
	 * (highest rpmsgN) one that carries our channel name. */
	while ((e = readdir(d))) {
		char p[320], name[64] = "";
		int idx;
		FILE *f;

		if (strncmp(e->d_name, "rpmsg", 5) != 0 ||
		    strncmp(e->d_name, "rpmsg_ctrl", 10) == 0)
			continue;
		snprintf(p, sizeof(p), "/sys/class/rpmsg/%s/name", e->d_name);
		f = fopen(p, "r");
		if (!f)
			continue;
		idx = atoi(e->d_name + 5);
		if (fgets(name, sizeof(name), f) &&
		    strncmp(name, channel, strlen(channel)) == 0 &&
		    idx > best) {
			snprintf(sysdir, cap, "/sys/class/rpmsg/%s", e->d_name);
			best = idx;
			found = 0;
		}
		fclose(f);
	}
	closedir(d);
	return found;
}

/* Device name of an announced channel we can ping, e.g.
 * "virtio1.rpmsg-openamp-demo-channel.-1.30". */
static int find_channel(char *out, size_t cap)
{
	DIR *d = opendir("/sys/bus/rpmsg/devices");
	struct dirent *e;
	int found = 0;

	if (!d)
		return 0;
	while (!found && (e = readdir(d)))
		if (rpmsg_ping_mode_of(e->d_name) != RPMSG_PING_MODE_NONE) {
			snprintf(out, cap, "%s", e->d_name);
			found = 1;
		}
	closedir(d);
	return found;
}

/* rpmsg_ctrlN whose parent device path runs through the given virtio bus. */
static int find_ctrl(const char *virtio, char *sysdir, size_t cap)
{
	DIR *d = opendir("/sys/class/rpmsg");
	struct dirent *e;
	int found = -1;
	char needle[32];

	if (!d)
		return -1;
	snprintf(needle, sizeof(needle), "/%s/", virtio);
	while (found < 0 && (e = readdir(d))) {
		char link[320], real[PATH_MAX];

		if (strncmp(e->d_name, "rpmsg_ctrl", 10) != 0)
			continue;
		snprintf(link, sizeof(link), "/sys/class/rpmsg/%s", e->d_name);
		if (realpath(link, real) && strstr(real, needle)) {
			snprintf(sysdir, cap, "%s", link);
			found = 0;
		}
	}
	closedir(d);
	return found;
}

static int open_endpoint(int *ctrl_fd_out, rpmsg_ping_mode_t *mode_out)
{
	struct rpmsg_endpoint_info info;
	char sysdir[128], node[192], virtio[24] = "", ctrl_dir[128], ctrl_node[192], devname[128] = "";
	const char *channel;
	rpmsg_ping_mode_t mode;
	int ctrl, fd, tries;

	/* Modules are only built, not loaded, on Torizon. Best effort. */
	(void)system("nsenter -t 1 -m -- modprobe rpmsg_char >/dev/null 2>&1; "
		     "nsenter -t 1 -m -- modprobe rpmsg_ctrl >/dev/null 2>&1");

	/* Wait for the M33 to announce the channel; this also tells us which of
	 * the two virtio buses (SRTM or user) the firmware is on. */
	for (tries = 0; tries < 100 && s_running; tries++) {
		if (find_channel(devname, sizeof(devname)) &&
		    rpmsg_ping_virtio_of(devname, virtio, sizeof(virtio)) &&
		    find_ctrl(virtio, ctrl_dir, sizeof(ctrl_dir)) == 0)
			break;
		virtio[0] = '\0';
		usleep(100000);
	}
	if (!virtio[0])
		return -1;
	snprintf(ctrl_node, sizeof(ctrl_node), "/dev/%s", strrchr(ctrl_dir, '/') + 1);
	if (ensure_node(ctrl_dir, ctrl_node) != 0 && access(ctrl_node, F_OK) != 0)
		return -1;
	ctrl = open(ctrl_node, O_RDWR);
	if (ctrl < 0)
		return -1;

	memset(&info, 0, sizeof(info));
	mode = rpmsg_ping_mode_of(devname);
	channel = mode == RPMSG_PING_MODE_ECHO ? RPMSG_PING_CHANNEL_ZEPHYR
					       : RPMSG_PING_CHANNEL_FREERTOS;
	snprintf(info.name, sizeof(info.name), "%s", channel);
	info.src = RPMSG_ADDR_ANY;
	info.dst = rpmsg_ping_dst_of(devname);
	if (ioctl(ctrl, RPMSG_CREATE_EPT_IOCTL, &info) < 0) {
		close(ctrl);
		return -1;
	}
	if (find_eptdev(channel, sysdir, sizeof(sysdir)) < 0) {
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
	*mode_out = mode;
	return fd;
}

static void *ping_main(void *arg)
{
	int ctrl = -1, fd = -1;
	rpmsg_ping_mode_t mode = RPMSG_PING_MODE_NONE;
	uint32_t seq = 0;
	uint8_t tx[RPMSG_PING_MSG_SIZE], rx[64];

	(void)arg;
	while (s_running && fd < 0) {
		fd = open_endpoint(&ctrl, &mode);
		if (fd < 0)
			sleep(1);
	}
	if (fd < 0)
		return NULL;
	applog(APPLOG_INFO, "rpmsg", "endpoint open, mode=%s",
	       mode == RPMSG_PING_MODE_ECHO ? "echo (Zephyr)" : "counter+1 (FreeRTOS)");

	/* The FreeRTOS example waits for a first message (hello) before it starts
	 * answering. The Zephyr one would echo it back and offset every reply. */
	if (mode == RPMSG_PING_MODE_INCREMENT)
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

		/* Drain everything queued: a stale or late answer must not shift
		 * the pairing of later pings. Only the reply to this seq counts. */
		if (poll(&pfd, 1, REPLY_TIMEOUT_MS) > 0) {
			int matched = 0;

			do {
				ssize_t n = read(fd, rx, sizeof(rx));
				uint32_t reply;

				if (n > 0 && !matched && rpmsg_ping_decode(rx, (int)n, &reply) &&
				    reply == rpmsg_ping_expected_reply(seq, mode)) {
					jitter_stats_add(&s_rtt, now_ns() - t0);
					pthread_mutex_lock(&s_totals_lock);
					rpmsg_ping_count_ok(&s_totals);
					pthread_mutex_unlock(&s_totals_lock);
					s_linked = 1;
					matched = 1;
				}
			} while (poll(&pfd, 1, 0) > 0);
		}
		seq++;
		usleep(PING_PERIOD_US);
	}
	s_linked = 0;
	/* The destroy ioctl belongs to the endpoint fd, not the control fd. */
	ioctl(fd, RPMSG_DESTROY_EPT_IOCTL);
	close(fd);
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
