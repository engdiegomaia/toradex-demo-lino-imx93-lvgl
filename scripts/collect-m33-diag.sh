#!/bin/bash
# Collect Verdin i.MX93 Cortex-M33 remoteproc + shared-memory diagnostic evidence
# into a single tarball. Run on the target as root (or via sudo). Nothing here
# writes to hardware — read-only probes only.
#
# Output: /tmp/m33-diag-<timestamp>.tar.gz
#
# Every command's stderr is preserved, never discarded. Empty output alone must
# not be interpreted as "the region is empty" — the *.err files decide.

set -u

TS=$(date +%Y%m%d-%H%M%S)
OUT="/tmp/m33-diag-$TS"
mkdir -p "$OUT"

section() { echo; echo "=========================================================="; echo " $*"; echo "=========================================================="; }

run() {
    # run <label> <cmd...>
    label="$1"; shift
    section "$label"
    echo "\$ $*"
    "$@" >"$OUT/$label.out" 2>"$OUT/$label.err"
    rc=$?
    echo "[exit $rc]"
    [ -s "$OUT/$label.err" ] && { echo "-- stderr --"; cat "$OUT/$label.err"; }
    cat "$OUT/$label.out"
}

runsh() {
    # runsh <label> <shell-string>
    label="$1"; shift
    section "$label"
    echo "\$ $*"
    bash -c "$*" >"$OUT/$label.out" 2>"$OUT/$label.err"
    rc=$?
    echo "[exit $rc]"
    [ -s "$OUT/$label.err" ] && { echo "-- stderr --"; cat "$OUT/$label.err"; }
    cat "$OUT/$label.out"
}

{
    section "collection metadata"
    date -u
    id
    uname -a

    run os-release           cat /etc/os-release
    run ostree-status        ostree admin status
    run dt-model             cat /proc/device-tree/model
    run cmdline              cat /proc/cmdline
    run iomem                cat /proc/iomem
    runsh reserved-memory-ls 'ls -la /proc/device-tree/reserved-memory/ 2>&1'
    runsh dmesg-reserved     'dmesg | grep -iE "reserved.?mem|no-map|carveout"'
    runsh dmesg-rproc        'dmesg | grep -iE "remoteproc|rproc|rsrc|rsc|vring|virtio|rpmsg|imx-rproc|imx-dsp"'
    runsh overlays           'ls -la /proc/device-tree/chosen/overlays/ 2>&1; echo "---"; for f in /proc/device-tree/chosen/overlays/*; do [ -f "$f" ] || continue; echo "== $f =="; od -An -tx1 "$f" | head -2; done'
    runsh rpmsg-bus          'ls -la /sys/bus/rpmsg/devices/ 2>&1'
    runsh remoteproc         'for rp in /sys/class/remoteproc/remoteproc*; do echo "== $rp =="; for f in name state firmware; do echo "-- $f --"; cat "$rp/$f" 2>&1; done; done'
    runsh mu-nodes           'find /proc/device-tree -name "mu@*" -o -name "mailbox@*" 2>/dev/null | while read n; do echo "== $n =="; ls "$n"; done'

    section "running device tree dump (dtc -I fs -O dts)"
    if command -v dtc >/dev/null 2>&1; then
        dtc -I fs -O dts /proc/device-tree > "$OUT/running-dt.dts" 2>"$OUT/running-dt.err"
        echo "wrote $OUT/running-dt.dts ($(wc -l <"$OUT/running-dt.dts") lines)"
        [ -s "$OUT/running-dt.err" ] && { echo "-- stderr --"; cat "$OUT/running-dt.err"; }
        echo "-- reserved-memory extract --"
        awk '/reserved-memory {/,/^\t};/' "$OUT/running-dt.dts" | tee "$OUT/reserved-memory.dts"
        echo "-- cm33 / remoteproc extract --"
        awk '/(cm33|remoteproc|imx-rproc|rpmsg)/,/^\t};/' "$OUT/running-dt.dts" | head -200 | tee "$OUT/cm33-node.dts"
    else
        echo "dtc not installed — install device-tree-compiler or export via /sys/firmware/fdt"
        cp /sys/firmware/fdt "$OUT/fdt.bin" 2>>"$OUT/running-dt.err" || echo "no /sys/firmware/fdt either"
    fi

    runsh devmem-probe 'if command -v busybox >/dev/null 2>&1; then for a in 0xA4000000 0xA4000004 0xA4008000 0xA4010000 0xA4020000; do echo "-- $a --"; busybox devmem "$a" 32 2>&1; done; else echo "busybox not present on host — see container-devmem below"; fi'

    runsh mmap-probe 'cat >/tmp/rd.c <<CEOF
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr,"usage: %s <hex-addr> [words]\n",argv[0]); return 2; }
    off_t addr = strtoull(argv[1],0,16);
    int words = argc>2 ? atoi(argv[2]) : 8;
    int fd = open("/dev/mem", O_RDONLY|O_SYNC);
    if (fd<0) { fprintf(stderr,"open /dev/mem: %s\n",strerror(errno)); return 1; }
    long ps = sysconf(_SC_PAGESIZE);
    off_t base = addr & ~(ps-1);
    off_t off  = addr - base;
    void *m = mmap(0, ps, PROT_READ, MAP_SHARED, fd, base);
    if (m==MAP_FAILED) { fprintf(stderr,"mmap %lx: %s\n",(unsigned long)base,strerror(errno)); return 1; }
    volatile uint32_t *p = (uint32_t*)((char*)m+off);
    for (int i=0;i<words;i++) printf("%08lx: %08x\n",(unsigned long)(addr+i*4),p[i]);
    munmap(m,ps); close(fd); return 0;
}
CEOF
if command -v gcc >/dev/null 2>&1; then
    gcc -O2 -o /tmp/rd /tmp/rd.c 2>&1 && for a in 0xA4000000 0xA4008000 0xA4010000 0xA4020000; do echo "-- $a --"; /tmp/rd "$a" 8 2>&1; done
else
    echo "no gcc on host — copy /tmp/rd.c into the lvgl-app container and build there"
fi'

    runsh container-devmem 'if command -v docker >/dev/null 2>&1; then docker exec lino-lvgl-app sh -c "command -v busybox >/dev/null && for a in 0xA4000000 0xA4008000 0xA4010000 0xA4020000; do echo -- \$a --; busybox devmem \$a 32 2>&1; done" 2>&1; else echo "no docker CLI on host"; fi'

    runsh maps-of-lvgl-app 'if command -v docker >/dev/null 2>&1; then pid=$(docker inspect --format "{{.State.Pid}}" lino-lvgl-app 2>/dev/null); [ -n "$pid" ] && cat /proc/$pid/maps 2>&1 | head -60 || echo "no lvgl-app pid"; fi'

    runsh firmware-elf-headers 'for f in /var/rootdirs/home/torizon/*/elf/*.elf /home/torizon/*/elf/*.elf; do [ -f "$f" ] || continue; echo "== $f =="; readelf -l "$f" 2>&1 | head -30; done'

    runsh mem-total          'cat /proc/meminfo | head -6'
    runsh mount-firmware     'mount | grep -E "firmware|tmpfs" | head -20'
    runsh docker-ps          'docker ps 2>&1'
    runsh docker-logs-lvgl   'docker logs --tail 60 lino-lvgl-app 2>&1'

    section "END"
    date -u
} | tee "$OUT/collect.log"

tar -C /tmp -czf "/tmp/m33-diag-$TS.tar.gz" "m33-diag-$TS"
echo
echo "==> archive: /tmp/m33-diag-$TS.tar.gz"
echo "==> transfer with: scp torizon@<target>:/tmp/m33-diag-$TS.tar.gz ."
