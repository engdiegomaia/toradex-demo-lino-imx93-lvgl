#!/bin/bash
# Copyright 2025 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
# accordance with the applicable license terms.
#
# Load a firmware ELF into one of the auxiliary cores via remoteproc.
#
# Torizon specifics:
# - remoteproc nodes are enumerated under /sys/class/remoteproc/remoteproc*.
#   Only the Cortex-M33 is exposed by default; the Cortex-A55 secondary-core
#   remoteproc requires an AMP device-tree overlay that Torizon does not ship.
# - The rootfs is read-only OSTree, and the kernel's request_firmware_direct()
#   only searches paths compiled into the kernel (chiefly /lib/firmware).
#   Workaround: bind-mount a tmpfs at /lib/firmware on the host and drop the
#   ELF there before writing the basename to sysfs.
#
# This script assumes it runs in a container with:
#   - pid: host          (so nsenter -t 1 -m enters the host mount namespace)
#   - privileged: true   (so mount(2) succeeds in the host ns)
#   - /sys mounted rw    (so remoteproc sysfs writes succeed)

CORE=$1     # a = Cortex-A secondary, m = Cortex-M33
RTOS=$2     # z = Zephyr, r = FreeRTOS
TYPE=$3     # h = hello_world (M33 FreeRTOS: RPMsg ping-pong), r = rt_latency

ELF_DIR="$PWD/elf"

log() { echo "[start.sh] $*" >&2; }
log "invoked: CORE=$CORE RTOS=$RTOS TYPE=$TYPE cwd=$PWD"

find_rproc() {
    match="$1"
    for rp in /sys/class/remoteproc/remoteproc*; do
        [ -d "$rp" ] || continue
        name=$(cat "$rp/name" 2>/dev/null || echo "")
        case "$name" in
            *${match}*) echo "$rp"; return 0 ;;
        esac
    done
    return 1
}

# Ensure /lib/firmware on the host is a tmpfs we can write into. Idempotent:
# if it is already tmpfs (m33-loader may have set it up), do nothing.
ensure_host_firmware_tmpfs() {
    fs=$(nsenter -t 1 -m -- stat -f -c %T /lib/firmware 2>/dev/null || echo "unknown")
    if [ "$fs" = "tmpfs" ]; then
        return 0
    fi
    nsenter -t 1 -m -- sh -c '
        mountpoint -q /lib/firmware && umount /lib/firmware 2>/dev/null || true
        mount -t tmpfs tmpfs /lib/firmware
    ' || return 1
}

# The Cortex-A55 secondary is a Linux CPU until we take it away. PSCI CPU_ON
# fails with -EINVAL (already on) while Linux still owns it, so the driver
# refuses to boot firmware unless the CPU is offline first. Offlining is itself
# a PSCI CPU_OFF issued by that CPU, which is the only way to park it cleanly.
#
# Override with CA55_CPU if a future overlay targets a different affinity than
# MPIDR 0x100.
CA55_CPU="${CA55_CPU:-1}"
CA55_CPU_ONLINE="/sys/devices/system/cpu/cpu${CA55_CPU}/online"

ca55_cpu_offline() {
    if [ ! -w "$CA55_CPU_ONLINE" ]; then
        log "FAIL: $CA55_CPU_ONLINE not writable (need privileged + rw /sys)"
        return 1
    fi
    if [ "$(cat "$CA55_CPU_ONLINE")" = "0" ]; then
        log "cpu$CA55_CPU already offline"
        return 0
    fi
    echo 0 > "$CA55_CPU_ONLINE" || return 1
    # Hotplug is synchronous, but read back rather than assume.
    if [ "$(cat "$CA55_CPU_ONLINE")" != "0" ]; then
        log "FAIL: cpu$CA55_CPU still online after hotplug write"
        return 1
    fi
    log "cpu$CA55_CPU offline; nproc now $(nproc)"
    return 0
}

if [ "$CORE" == "a" ]; then
    CMD_DIR=$(find_rproc "ca55" || find_rproc "cortex-a" || true)
    if [ -z "$CMD_DIR" ]; then
        echo "start.sh: Cortex-A secondary remoteproc not present. This device tree" >&2
        echo "start.sh: does not carve out A55 core 1 for AMP. Skipping." >&2
        exit 2
    fi
    if [ "$RTOS" == "z" ]; then
        if [ "$TYPE" == "h" ]; then FW="$ELF_DIR/hello_world_ca55_0xd0100000_zephyr.elf"
        else                        FW="$ELF_DIR/rt_latency_ca55_0xd0100000_zephyr.elf"; fi
    else
        if [ "$TYPE" == "h" ]; then FW="$ELF_DIR/hello_world_ca55_0xd0fff000.elf"
        else                        FW="$ELF_DIR/rt_latency_ca55_0xd0fff000.elf"; fi
    fi
else
    CMD_DIR=$(find_rproc "cm33" || find_rproc "imx-rproc" || find_rproc "cortex-m" || true)
    if [ -z "$CMD_DIR" ]; then
        echo "start.sh: Cortex-M33 remoteproc not found under /sys/class/remoteproc/*" >&2
        exit 2
    fi
    if [ "$RTOS" == "z" ]; then
        if [ "$TYPE" == "h" ]; then FW="$ELF_DIR/hello_world_cm33_zephyr.elf"
        else                        FW="$ELF_DIR/rt_latency_cm33_zephyr.elf"; fi
    else
        if [ "$TYPE" == "h" ]; then FW="$ELF_DIR/rpmsg_pingpong_cm33.elf"
        else                        FW="$ELF_DIR/rt_latency_cm33.elf"; fi
    fi
fi

if [ ! -f "$FW" ]; then
    echo "start.sh: firmware not found: $FW" >&2
    exit 2
fi

FW_NAME=$(basename "$FW")

if ! ensure_host_firmware_tmpfs; then
    log "FAIL: could not mount tmpfs on host /lib/firmware (need pid: host + privileged)"
    exit 3
fi
log "host /lib/firmware type: $(nsenter -t 1 -m -- stat -f -c %T /lib/firmware 2>/dev/null)"
log "host /lib/firmware ls before staging:"
nsenter -t 1 -m -- ls -la /lib/firmware >&2 2>/dev/null || true

# Push the requested ELF into the host's /lib/firmware tmpfs so
# request_firmware_direct() can find it.
if ! cat "$FW" | nsenter -t 1 -m -- sh -c "cat > /lib/firmware/$FW_NAME"; then
    log "FAIL staging $FW_NAME into host /lib/firmware"
    exit 3
fi
log "host /lib/firmware ls after staging:"
nsenter -t 1 -m -- ls -la /lib/firmware >&2 2>/dev/null || true

# Stop current firmware if it's running — remoteproc rejects a firmware write
# while state=running.
current=$(cat "$CMD_DIR/state" 2>/dev/null || echo "")
log "current state before stop: $current"
if [ "$current" = "running" ]; then
    echo stop > "$CMD_DIR/state" 2>/dev/null || true
    for _ in 1 2 3 4 5; do
        s=$(cat "$CMD_DIR/state" 2>/dev/null || echo "")
        [ "$s" != "running" ] && break
        sleep 0.2
    done
    log "state after stop wait: $(cat "$CMD_DIR/state" 2>/dev/null)"
fi

log "remoteproc node: $CMD_DIR firmware=$FW_NAME"
log "state before write: $(cat "$CMD_DIR/state" 2>/dev/null)"

if [ "$CORE" == "a" ]; then
    if ! ca55_cpu_offline; then
        log "FAIL: could not release cpu$CA55_CPU from Linux"
        exit 3
    fi
fi

if ! printf '%s' "$FW_NAME" > "$CMD_DIR/firmware" 2>/tmp/fw.err; then
    log "FAIL writing firmware: $(cat /tmp/fw.err 2>/dev/null)"
    exit 3
fi
log "firmware attr now: $(cat "$CMD_DIR/firmware" 2>/dev/null)"

if ! echo start > "$CMD_DIR/state" 2>/tmp/start.err; then
    log "FAIL writing state=start: $(cat /tmp/start.err 2>/dev/null)"
    log "last dmesg:"
    nsenter -t 1 -m -- dmesg 2>/dev/null | tail -20 >&2 || true
    exit 3
fi

sleep 1
final=$(cat "$CMD_DIR/state" 2>/dev/null)
log "state after start: $final"
if [ "$final" != "running" ]; then
    log "remoteproc did NOT reach running state. Last dmesg:"
    nsenter -t 1 -m -- dmesg 2>/dev/null | tail -30 >&2 || true
    exit 3
fi
