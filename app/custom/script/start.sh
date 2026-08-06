#!/bin/bash
# Copyright 2025 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
# accordance with the applicable license terms.
#
# Load a firmware ELF into one of the auxiliary cores via remoteproc.
#
# The original NXP EVK scripts hardcoded platform paths like
#   /sys/devices/platform/remoteproc-ca55-1/remoteproc/remoteproc0
#   /sys/devices/platform/remoteproc-cm33/remoteproc/remoteproc1
# which do not exist on the Toradex Torizon i.MX93 kernel. The Torizon device
# tree only exposes the M33 remoteproc, and it lives at the stable class-alias
# path /sys/class/remoteproc/remoteproc0. The A55 secondary-core remoteproc
# ("remoteproc-ca55-1") requires a DT overlay that carves the second A-core
# out of Linux; Torizon does not ship that overlay by default.
#
# We resolve the remoteproc node by matching `.../remoteproc*/name` so this
# script works regardless of enumeration order.

CORE=$1     # a = Cortex-A secondary, m = Cortex-M33
RTOS=$2     # z = Zephyr, r = FreeRTOS
TYPE=$3     # h = hello_world, r = rt_latency

ELF_DIR="$PWD/elf"

# Find the remoteproc sysfs node whose kernel name matches a substring.
# Prints the /sys/class/remoteproc/remoteprocN path on stdout, exits 1 if
# nothing matches.
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

if [ "$CORE" == "a" ]; then
    # Cortex-A secondary core. Torizon default DT does not enable this — the
    # A55 remoteproc requires an AMP overlay. Detect and fail cleanly.
    CMD_DIR=$(find_rproc "ca55" || find_rproc "cortex-a" || true)
    if [ -z "$CMD_DIR" ]; then
        echo "start.sh: Cortex-A secondary remoteproc not present. This device tree" >&2
        echo "start.sh: does not carve out A55 core 1 for AMP. Skipping." >&2
        exit 2
    fi
    if [ "$RTOS" == "z" ]; then
        if [ "$TYPE" == "h" ]; then
            FW="$ELF_DIR/hello_world_ca55_0xd0100000_zephyr.elf"
        else
            FW="$ELF_DIR/rt_latency_ca55_0xd0100000_zephyr.elf"
        fi
    else
        if [ "$TYPE" == "h" ]; then
            FW="$ELF_DIR/hello_world_ca55_0xd0fff000.elf"
        else
            FW="$ELF_DIR/rt_latency_ca55_0xd0fff000.elf"
        fi
    fi
else
    # Cortex-M33 — always present on i.MX93 SoC.
    CMD_DIR=$(find_rproc "cm33" || find_rproc "imx-rproc" || find_rproc "cortex-m" || true)
    if [ -z "$CMD_DIR" ]; then
        echo "start.sh: Cortex-M33 remoteproc not found under /sys/class/remoteproc/*" >&2
        exit 2
    fi
    if [ "$RTOS" == "z" ]; then
        if [ "$TYPE" == "h" ]; then
            FW="$ELF_DIR/hello_world_cm33_zephyr.elf"
        else
            FW="$ELF_DIR/rt_latency_cm33_zephyr.elf"
        fi
    else
        if [ "$TYPE" == "h" ]; then
            FW="$ELF_DIR/hello_world_cm33.elf"
        else
            FW="$ELF_DIR/rt_latency_cm33.elf"
        fi
    fi
fi

# The remoteproc `firmware` sysfs attribute takes a BASENAME that is looked
# up under /lib/firmware, not an absolute ELF path. m33-loader has already
# bind-mounted a tmpfs at /lib/firmware on the host and populated it with the
# rt_latency firmware; we do the same for whichever firmware the user picked.
FW_NAME=$(basename "$FW")

if [ ! -f "$FW" ]; then
    echo "start.sh: firmware not found: $FW" >&2
    exit 2
fi

# Push the requested ELF into the host's /lib/firmware tmpfs so the kernel
# request_firmware_direct() can find it. Requires --pid=host + privileged,
# same as m33-loader.
if command -v nsenter >/dev/null 2>&1; then
    cat "$FW" | nsenter -t 1 -m -- sh -c "cat > /lib/firmware/$FW_NAME" || {
        echo "start.sh: failed to stage $FW_NAME into host /lib/firmware" >&2
        exit 3
    }
fi

# Stop current firmware if it's still running — remoteproc rejects a firmware
# write while state=running.
current=$(cat "$CMD_DIR/state" 2>/dev/null || echo "")
if [ "$current" = "running" ]; then
    echo stop > "$CMD_DIR/state" 2>/dev/null || true
    sleep 1
fi

printf '%s' "$FW_NAME" > "$CMD_DIR/firmware"
echo start > "$CMD_DIR/state"
