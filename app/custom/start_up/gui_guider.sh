#!/bin/sh
# Liubin.gong@nxp.com

cmd=$1

init() {
        echo "gui_guider backend program"
        export XDG_RUNTIME_DIR=/run/user/0
        export WAYLAND_DISPLAY=wayland-0
}

wait_ready() {
        while [ ! -S /run/user/0/wayland-0 ]; do
                sleep 1
        done

        until grep -q "Goodix Capacitive TouchScreen" /proc/bus/input/devices; do
                sleep 1
        done

        sleep 2
}

start() {
        wait_ready
        exec /root/demo/gui_guider
}

stop() {
        pkill -9 gui_guider >/dev/null 2>&1
}

case "$cmd" in
        "")
                init
                start
                ;;
        "init")
                init
                ;;
        "start")
                init
                start
                ;;
        "restart")
                stop
                sleep 1
                init
                start
                ;;
        "stop")
                stop
                ;;
        *)
                printf 'Usage: %s {start|stop|restart}\n' "$0"
                exit 1
                ;;
esac