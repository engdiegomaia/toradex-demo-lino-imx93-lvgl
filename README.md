# Lino i.MX93 Real-Time HMI Demo

An internally developed LVGL industrial HMI demo for the Toradex Verdin i.MX93
on the Lino + Verdin Adapter + Mallow carrier platform. It combines a Wayland
UI, Weston, and a Cortex-M33 real-time latency benchmark with shared-memory
telemetry.

> **Component licensing:** This is a Toradex-developed project, not a
> redistribution of the NXP Demo Gallery project as a whole. Some files in the
> application carry NXP proprietary notices or were generated with NXP GUI
> Guider. Those specific materials retain their applicable terms; the NXP
> agreement is included at [`app/licenses/LICENSE.txt`](app/licenses/LICENSE.txt).
> Other project-authored code and third-party components have their own
> provenance and terms. No single license is asserted for the whole repository.

> **Project status:** This is a hardware-specific demo and integration
> reference, not a production-ready control system. The Cortex-M33 FreeRTOS
> latency path has been validated on the target. Other firmware entries may
> have different support status; see the firmware notes before selecting one.

## What it contains

- LVGL and GUI Guider application sources under `app/`.
- Torizon OS container and Weston configuration under `docker/`.
- Device-tree overlay sources under `device-tree/`.
- Cortex-M33 firmware patches and build instructions under `firmware/`.
- Bring-up and integration investigation notes under `docs/`.

The UI runs as a Wayland client. Weston provides the display compositor. The
application loads M33 firmware through Linux remoteproc and reads benchmark
telemetry from shared memory. Rebuilt FreeRTOS firmware uses the reserved
`0xA5000000` region; legacy prebuilt images may use `0xA4000000`.

## Hardware and software

The validated setup uses a Toradex Verdin i.MX93 module, Lino SoM, Verdin
Adapter, Mallow carrier, and Toradex 10.1-inch DSI capacitive touch display.
The deployment notes refer to Torizon OS 7.7.0. Other combinations may require
different device-tree overlays and container image tags.

Building and deployment require a Linux host with Docker, Docker Compose,
`rsync`, and the cross-build toolchain image dependencies. Firmware rebuilds
also require the MCUXpresso SDK West workspace and `arm-none-eabi-gcc`.

## Build and deploy

Build the aarch64 application image inputs on the host:

```sh
./docker/build-aarch64.sh
```

Build and start the container stack on a configured target:

```sh
docker compose -f docker/docker-compose.yml build
docker compose -f docker/docker-compose.yml up -d
```

### Security and device access

The Compose stack is isolated from IP networking; Weston and the application
communicate over the shared Wayland socket. The `display-init` helper drops all
Linux capabilities and only needs the host sysfs bind attribute plus read-only
visibility of `/dev/dri/card0`. The M33 loader and LVGL application still run
privileged. They enter the host mount
namespace to stage firmware and write remoteproc sysfs. The LVGL application
also offlines a CPU through sysfs and reads telemetry through `/dev/mem`. Treat
those images and this source tree as host-control software: run only reviewed
builds on a trusted target, and do not enable a remote Docker API for the device.

Keep deployment credentials in local `.env` files or environment variables;
`.env` files are ignored by Git. Commit only sanitized examples such as
`.env.example`.

The target requires the appropriate display and shared-memory device-tree
overlays. See [firmware build and deployment](firmware/README.md) and the
[display bring-up notes](docs/DISPLAY_BRINGUP_DEBUG.md) before applying overlays.
The helper scripts accept target host configuration through their arguments or
environment variables; review them before using them on a device.

For firmware build details, see [firmware/README.md](firmware/README.md). The
focused integration notes are in [`docs/`](docs/).

## Repository layout

```text
app/          LVGL application, generated UI, custom logic, and vendor sources
assets/       Runtime assets
device-tree/  Device-tree overlay source and deployment helper
docker/       Container definitions and host/target helper scripts
docs/         Display and M33 integration notes
firmware/     Firmware patches and reproducible build helper
scripts/      Diagnostics and small host-side utilities
```

## Licensing

This internally developed project combines Toradex-authored code with files
that retain NXP notices, vendored software, prebuilt firmware, and branded
assets. The NXP agreement applies to NXP Licensed Software as defined in that
agreement; it does not establish a license for every project-authored or
third-party file. The agreement describes conditions for source distribution
of its Licensed Software through GitHub, and terms for reports and derivative
works. Follow the notices attached to each component. The accompanying NXP
software content register is [`app/SCR.txt`](app/SCR.txt).

Vendored LVGL notices are under [`app/platform/lvgl/`](app/platform/lvgl/).
Prebuilt firmware, fonts, and branded assets have separate provenance and
redistribution terms. No repository-wide license is declared; do not assume
that one component's license covers the entire repository.

## Contributions

Please keep generated GUI Guider output changes reproducible and document any
manual edits required after regeneration. Avoid committing local build output,
target-specific credentials, or diagnostic bundles.
