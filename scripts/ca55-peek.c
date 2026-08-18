// SPDX-License-Identifier: GPL-2.0
/*
 * Dump physical memory through /dev/mem, for inspecting Cortex-A55 AMP state.
 *
 * Why not dd or busybox devmem: read() on /dev/mem is refused for these
 * addresses (CONFIG_STRICT_DEVMEM=y), and returns 0 bytes. mmap() of the same
 * range is permitted, which is the path the LVGL app already uses for the M33
 * shared memory. So this reads via mmap only.
 *
 * Must run inside the privileged app container: that is where /dev/mem plus
 * CAP_SYS_RAWIO are available. The Torizon host OS refuses the same access.
 *
 *   ca55-peek <phys-addr> [length]     hex + ASCII
 *   ca55-peek <phys-addr> ram_console  decode a struct ram_console_header
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define RAM_CONSOLE_HEAD_SIZE 64

static void *map_phys(int fd, unsigned long addr, size_t len, size_t *delta)
{
	long page = sysconf(_SC_PAGESIZE);
	unsigned long base = addr & ~(unsigned long)(page - 1);
	size_t span = len + (addr - base);
	void *p;

	span = ((span + page - 1) / page) * page;
	p = mmap(NULL, span, PROT_READ, MAP_SHARED, fd, (off_t)base);
	if (p == MAP_FAILED)
		return NULL;
	*delta = addr - base;
	return p;
}

static void dump(const unsigned char *p, unsigned long addr, size_t len)
{
	size_t i, j;

	for (i = 0; i < len; i += 16) {
		printf("%08lx  ", addr + i);
		for (j = 0; j < 16; j++)
			printf(j < len - i ? "%02x " : "   ", p[i + j]);
		printf(" |");
		for (j = 0; j < 16 && j < len - i; j++)
			putchar(isprint(p[i + j]) ? p[i + j] : '.');
		printf("|\n");
	}
}

int main(int argc, char **argv)
{
	unsigned long addr;
	size_t len = 256, delta = 0;
	int console = 0, nonzero = 0;
	unsigned char *p;
	size_t i;
	int fd;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <phys-addr> [length|ram_console]\n", argv[0]);
		return 2;
	}
	addr = strtoul(argv[1], NULL, 0);
	if (argc > 2) {
		if (!strcmp(argv[2], "ram_console")) {
			console = 1;
			len = RAM_CONSOLE_HEAD_SIZE + 1024;
		} else {
			len = strtoul(argv[2], NULL, 0);
		}
	}

	fd = open("/dev/mem", O_RDONLY | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem");
		return 1;
	}

	p = map_phys(fd, addr, len, &delta);
	if (!p) {
		perror("mmap");
		close(fd);
		return 1;
	}
	p += delta;

	for (i = 0; i < len; i++)
		if (p[i]) {
			nonzero = 1;
			break;
		}

	printf("phys 0x%08lx, %zu bytes, %s\n", addr, len,
	       nonzero ? "NON-ZERO" : "all zero");

	if (console) {
		uint64_t buf_addr;
		uint64_t buf_size;
		uint32_t pos;
		char flag[13];

		memcpy(flag, p, 12);
		flag[12] = '\0';
		memcpy(&buf_addr, p + 16, 8);
		memcpy(&buf_size, p + 24, 8);
		memcpy(&pos, p + 32, 4);
		printf("flag_string : \"%s\"\n", flag);
		printf("buf_addr    : 0x%llx\n", (unsigned long long)buf_addr);
		printf("buf_size    : %llu\n", (unsigned long long)buf_size);
		printf("pos         : %u\n", pos);
		printf("--- payload after header ---\n");
		dump(p + RAM_CONSOLE_HEAD_SIZE, addr + RAM_CONSOLE_HEAD_SIZE,
		     len - RAM_CONSOLE_HEAD_SIZE > 512 ? 512 : len - RAM_CONSOLE_HEAD_SIZE);
	} else {
		dump(p, addr, len);
	}

	close(fd);
	return 0;
}
