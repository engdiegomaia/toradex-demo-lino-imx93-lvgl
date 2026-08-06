#!/bin/sh
# Load the FreeRTOS rt_latency firmware into the Cortex-M33 via remoteproc.
#
# Torizon rootfs is read-only (OSTree), but the i.MX93 remoteproc driver uses
# request_firmware_direct() which only searches the kernel's built-in paths
# (chiefly /lib/firmware/*). Redirecting via
# /sys/module/firmware_class/parameters/path does NOT work for this call
# (observed on Torizon 7.7.0 kernel 6.6.142: "Direct firmware load ... failed
# with error -2" despite path being set).
#
# Workaround, per Toradex HMP docs
# (https://developer.toradex.com/software/hmp/hmp-nxp/how-to-load-firmwares-nxp/):
# bind-mount an overlay onto /lib/firmware in the host mount namespace via
# nsenter. This is volatile (lost on reboot) and does not modify OSTree.

set -eu

RPROC=/sys/class/remoteproc/remoteproc0
FW_NAME=rt_latency_cm33.elf
FW_SRC=/firmware
HOST_FW_DIR=/lib/firmware

if [ ! -d "$RPROC" ]; then
    echo "m33-loader: $RPROC missing. remoteproc driver not up?" >&2
    exit 1
fi

# Enter host mount namespace via nsenter (requires --pid=host + privileged).
# Fresh tmpfs on /lib/firmware, seeded with our firmware, plus symlink from
# our staged /firmware.
echo "m33-loader: bind-mounting firmware overlay onto host $HOST_FW_DIR"
nsenter -t 1 -m -- sh -c "
    mountpoint -q $HOST_FW_DIR && umount $HOST_FW_DIR 2>/dev/null || true
    mount -t tmpfs tmpfs $HOST_FW_DIR
" || {
    echo "m33-loader: failed to mount tmpfs on host $HOST_FW_DIR (need --pid=host + privileged)" >&2
    exit 1
}

# Copy firmware from our container into the freshly mounted tmpfs on the host.
# The container's $FW_SRC is not visible to the host mount ns; we go through
# /proc/self/root of the host-side sh, which sees the tmpfs, then copy via
# cat piped through nsenter.
cat "$FW_SRC/$FW_NAME" | nsenter -t 1 -m -- sh -c "cat > $HOST_FW_DIR/$FW_NAME"
nsenter -t 1 -m -- sh -c "ls -la $HOST_FW_DIR/"

current_state=$(cat "$RPROC/state")
echo "m33-loader: remoteproc0 state before = $current_state"

if [ "$current_state" = "running" ]; then
    echo "m33-loader: stopping current firmware"
    echo stop > "$RPROC/state"
    sleep 1
fi

echo "m33-loader: setting firmware = $FW_NAME"
printf '%s' "$FW_NAME" > "$RPROC/firmware"

echo "m33-loader: starting"
if ! echo start > "$RPROC/state" 2>/tmp/start.err; then
    echo "m33-loader: 'echo start' failed:" >&2
    cat /tmp/start.err >&2
    echo "--- last 30 dmesg lines ---" >&2
    dmesg 2>/dev/null | tail -30 >&2 || true
    exit 1
fi
sleep 1

final_state=$(cat "$RPROC/state")
echo "m33-loader: remoteproc0 state after  = $final_state"

if [ "$final_state" != "running" ]; then
    echo "m33-loader: FAILED to reach running state" >&2
    dmesg 2>/dev/null | tail -30 >&2 || true
    exit 1
fi

echo "m33-loader: done."
