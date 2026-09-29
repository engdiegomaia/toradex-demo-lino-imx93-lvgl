# Development Guide

## Build and deploy

Build the aarch64 application inputs on a Linux host:

```sh
./docker/build-aarch64.sh
```

Build and start the container stack on a configured target:

```sh
docker compose -f docker/docker-compose.yml build
docker compose -f docker/docker-compose.yml up -d
```

The target needs the appropriate display and shared-memory device-tree overlays.
See [firmware build and deployment](../firmware/README.md) and the [display
bring-up notes](DISPLAY_BRINGUP_DEBUG.md) before applying overlays. Review
helper scripts before using them on a device.

## Application structure

- `app/generated/` contains GUI Guider output. Keep the matching GUI Guider
  project changes with generated UI changes.
- `app/custom/` contains project integration logic, including M33 firmware
  loading, shared-memory telemetry, CPU utilization sampling, and branding.
- `app/platform/linux/` contains the Linux LVGL port; `app/platform/lvgl/`
  contains vendored LVGL.
- `docker/` contains container definitions, Weston configuration, and deployment
  helpers.
- `device-tree/` contains overlay sources and deployment helpers.
- `firmware/` contains firmware build instructions and patches.

The Linux build recursively includes C files under `app/custom/` and
`app/generated/`. New files are picked up at the next CMake configure.

## GUI Guider integration notes

Two generated files contain required integration edits:

- `app/generated/gg_utils.c` includes `screen_geometry.h` and calls
  `screen_geometry_apply()` in `setup_ui()` and
  `gg_load_screen_animation()`. Reapply these edits after regenerating the UI.
- `app/generated/screens/gg_CpuInfo.c` populates CPU tables through
  `cpu_info_read_cortex_a()` and `cpu_info_read_cortex_m()` instead of generated
  literal values.

Prefer runtime overrides in `app/custom/` for other project-specific UI changes
so they survive GUI Guider regeneration.

## M33 telemetry and firmware

The LVGL application is a Wayland client running alongside Weston. The
application loads firmware through Linux remoteproc and reads benchmark data
from shared memory. Rebuilt FreeRTOS firmware uses reserved address
`0xA5000000`; legacy prebuilt images may use `0xA4000000`.

The FreeRTOS M33 latency path has been validated on the target. The prebuilt
Zephyr latency images do not publish the shared-memory ring expected by the UI,
and A55 secondary firmware is not available through the target's configured
remoteproc interface. Consult [M33 handshake diagnostics](M33_HANDSHAKE_DEBUG.md)
for supporting investigation notes before selecting firmware.

## A55-0 utilization

The UI's A1 lane reports utilization for Cortex-A55-0, which runs Linux and the
HMI. `app/custom/cpu_load.c` samples `/proc/stat` and process statistics;
`cpu_load_ui.c` updates the display from the LVGL thread. The process value is
percentage of one core and can exceed the true A55-0 share when the process is
not confined to CPU0.

## Branding assets

GUI Guider owns `app/generated/assets/images/`. Project-created LVGL images
belong under `app/custom/assets/`. The conversion utility is
`scripts/png-to-lvgl-image.py`; intermediate source images and conversion inputs
are maintained under `app/resources/image/` where applicable.
