#!/bin/sh
# Recover the i.MX93 MediaMix block controller when its first boot-time probe
# times out before dependencies are ready. Without this controller bound,
# imx-drm never creates /dev/dri/card0 and Weston cannot start.

set -eu

DEVICE=4ac10000.system-controller
DEVICE_DIR=/sys/bus/platform/devices/$DEVICE
DRIVER_DIR=/sys/bus/platform/drivers/imx93-blk-ctrl
DRM_CARD=/dev/dri/card0
MAX_ATTEMPTS=60

attempt=1
while [ "$attempt" -le "$MAX_ATTEMPTS" ]; do
    if [ -c "$DRM_CARD" ]; then
        echo "display-init: $DRM_CARD is ready"
        exec sleep infinity
    fi

    if [ -d "$DEVICE_DIR" ] && [ ! -L "$DEVICE_DIR/driver" ] && [ -w "$DRIVER_DIR/bind" ]; then
        echo "display-init: binding $DEVICE (attempt $attempt/$MAX_ATTEMPTS)"
        if ! printf '%s' "$DEVICE" > "$DRIVER_DIR/bind" 2>/tmp/display-bind.err; then
            echo "display-init: bind not ready yet: $(cat /tmp/display-bind.err)" >&2
        fi
    else
        echo "display-init: waiting for DRM dependencies (attempt $attempt/$MAX_ATTEMPTS)"
    fi

    attempt=$((attempt + 1))
    sleep 1
done

echo "display-init: $DRM_CARD did not appear after $MAX_ATTEMPTS attempts" >&2
exit 1
