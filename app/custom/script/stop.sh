#!/bin/bash
# Copyright 2025 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
# accordance with the applicable license terms.
#
# Stop a firmware running on one of the auxiliary cores.
# See start.sh for the rationale behind the class-alias path discovery.

CORE=$1

log() { echo "[stop.sh] $*" >&2; }
log "invoked: CORE=$CORE cwd=$PWD (called by pid $PPID)"

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
    CMD_DIR=$(find_rproc "ca55" || find_rproc "cortex-a" || true)
else
    CMD_DIR=$(find_rproc "cm33" || find_rproc "imx-rproc" || find_rproc "cortex-m" || true)
fi

if [ -z "$CMD_DIR" ]; then
    # Nothing to stop — the target core has no remoteproc node.
    exit 0
fi

# Only issue the stop if the firmware is actually running; otherwise remoteproc
# returns EINVAL and spams the log with 'No such file or directory' from the
# echo redirection.
current=$(cat "$CMD_DIR/state" 2>/dev/null || echo "")
log "state before stop: $current (on $CMD_DIR)"
if [ "$current" = "running" ]; then
    echo stop > "$CMD_DIR/state" 2>/dev/null || true
    log "state after stop: $(cat "$CMD_DIR/state" 2>/dev/null)"
fi
