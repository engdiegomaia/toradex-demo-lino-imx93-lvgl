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
TYPE=$3     # h = hello_world, r = rt_latency

ELF_DIR="$PWD/elf"

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
        if [ "$TYPE" == "h" ]; then FW="$ELF_DIR/hello_world_cm33.elf"
        else                        FW="$ELF_DIR/rt_latency_cm33.elf"; fi
    fi
fi

if [ ! -f "$FW" ]; then
    echo "start.sh: firmware not found: $FW" >&2
    exit 2
fi

FW_NAME=$(basename "$FW")

if ! ensure_host_firmware_tmpfs; then
    echo "start.sh: failed to mount tmpfs on host /lib/firmware — is lvgl-app running with pid: host + privileged?" >&2
    exit 3
fi

# Push the requested ELF into the host's /lib/firmware tmpfs so
# request_firmware_direct() can find it.
if ! cat "$FW" | nsenter -t 1 -m -- sh -c "cat > /lib/firmware/$FW_NAME"; then
    echo "start.sh: failed to stage $FW_NAME into host /lib/firmware" >&2
    exit 3
fi

# Stop current firmware if it's running — remoteproc rejects a firmware write
# while state=running.
current=$(cat "$CMD_DIR/state" 2>/dev/null || echo "")
if [ "$current" = "running" ]; then
    echo stop > "$CMD_DIR/state" 2>/dev/null || true
    # Wait for state to settle. remoteproc stop is async — writing 'start'
    # too early races with the tear-down and yields EBUSY.
    for _ in 1 2 3 4 5; do
        s=$(cat "$CMD_DIR/state" 2>/dev/null || echo "")
        [ "$s" != "running" ] && break
        sleep 0.2
    done
fi

printf '%s' "$FW_NAME" > "$CMD_DIR/firmware"
echo start > "$CMD_DIR/state"
