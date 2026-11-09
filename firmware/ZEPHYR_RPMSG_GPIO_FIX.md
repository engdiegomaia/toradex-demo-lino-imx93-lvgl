# Zephyr RPMsg: preserve Linux GPIO ownership

## Problem and cause

On Lino i.MX93 with Verdin Adapter, Mallow carrier, the 10.1-inch DSI touch
panel and Torizon OS 7.7.0, starting item 4, **Zephyr: RPMsg round-trip**
(boot type 8), caused touch loss and CPU0 saturation within 5–45 seconds.
RPMsg replies and GUI heartbeats continued. Stopping M33 or restarting the
app and Weston did not restore normal operation; a reboot did.

The Zephyr SoC early-init hook unconditionally cleared `GPIO1..4->PCNS`, even
with `CONFIG_GPIO` disabled. This changes the security attributes of GPIO
banks shared with Linux. The NXP SDK header `MIMX9352_cm33.h`, field
`RGPIO_PCNS_NSE0`, defines zero as Secure-only access: Non-Secure accesses
read zero and writes are ignored. This firmware must preserve Linux's GPIO
configuration rather than take ownership of every pin.

Before the fix, `ilitek_touch_irq` remained at 26 despite repeated taps.
CPU0's idle counter also stopped advancing. The GPIO ownership violation
explains a mechanism for persistent touch loss. Its precise causal connection
to the increased rendering load has not been isolated.

Source examined: Zephyr 4.4.0, revision
`5cc0697a9246384a89ad69b04fbfde475f539821`,
`soc/nxp/imx/imx9/imx93/m33/soc.c:soc_early_init_hook`.
The SDK header is under the workspace's
`zsdk/modules/hal/nxp/mcux/mcux-sdk/devices/MIMX9352/` directory.

## Fix and scope

[Patch 0005](patches/0005-zephyr-imx93-preserve-linux-gpio-security.patch)
removes the four PCNS writes. It leaves the existing GPC sleep-hold operation
unchanged. No memory-map, clock, device-tree, OS or bootloader change is needed.

[The RPMsg build script](build-rpmsg-zephyr-cm33.sh) applies the patch
idempotently and refuses to install the ELF if `CONFIG_GPIO` is enabled.
Patch 0004 still provides the endless, silent echo responder.

This fix is for the GPIO-free M33 RPMsg application hosted by Linux.
It patches the shared Zephyr workspace's SoC source: reconsider ownership
before using that workspace for firmware that needs GPIO. Do not substitute
guessed PCNS values or reset all GPIO security registers from Linux.

## Build

From the repository root, with the existing West workspace, pyelftools and
Arm GNU 13.2.Rel1 toolchain available:

```sh
./firmware/build-rpmsg-zephyr-cm33.sh
```

The command installs the resulting ELF into
`app/custom/elf/zephyr_rpmsg_cm33.elf`. The existing development host used:

```sh
PYTHONPATH=/usr/lib/python3/dist-packages \
PATH="$HOME/.venv/mcux/bin:$PATH" \
./firmware/build-rpmsg-zephyr-cm33.sh
```

Host cross-build and firmware/security review passed. Disassembly confirmed
removal of all four GPIO stores. ELF text decreased from 56844 to 56816 bytes;
data remained 4544 bytes and BSS remained 17276 bytes. No revision was changed.

## Deployment and rollback

Back up the current target ELF before replacement. Hardware writes require
the per-command confirmation described in the project's AGENTS.md.

For a deployment that survives container recreation, rebuild the firmware
before syncing the project, then include it in the runtime image:

```sh
# Host, after the firmware build above:
SYNC_TARGET=torizon@<host> ./docker/sync-to-target.sh

# Target, from ~/lino-imx93-mallow-dsi-lvgl:
./docker/rebuild-app.sh
docker compose -f docker/docker-compose.yml up -d lvgl-app
sudo reboot
```

Use a clean boot after replacing the firmware: stopping the old M33 image
does not restore the GPIO security configuration it changed. Check the ELF
checksum in the running container before and after reboot.

The approved development-board test on 2026-11-09 instead copied the candidate
directly into the existing `lino-lvgl-app` container and rebooted. This survives
reboot but **does not survive container recreation from the old runtime image**.
The tracked host ELF was restored to the original after the build, so rebuild
the firmware before any subsequent project sync or image build.

Rollback restores the backed-up original ELF to the target and clean-boots
again. For an image deployment, rebuild the runtime image with that original
ELF and recreate the app container before rebooting. The original test ELF is
also backed up on the development host at
`/tmp/lino-zephyr-rpmsg-original.elf`; this temporary file is not durable storage.

## Initial hardware validation — 2026-11-09

Validated on the Lino i.MX93 development board described above, kernel
`6.6.142-7.7.0`. No simulator validation was performed.

| Check | Result |
|---|---|
| Candidate SHA-256, verified before/after reboot | `e7b927dd2a55463cdb84985e029bddda20654c23769101d1bf50b8a9fde93c2d` |
| Original ELF SHA-256 | `104e9ca8f28251fc0d8573977f7020c5b0cd763d7683cac804bf1cc4e39d7abc` |
| Startup | All four containers up; Weston, display-init and m33-loader healthy |
| Candidate execution | Item 4 started at 18:40:17 UTC; boot type 8 and echo endpoint logged |
| Touch | User confirmed navigation worked after starting item 4 |
| Touch interrupt count | Advanced from 0 after boot to 40 after selection/start and navigation |
| CPU0 idle | Approximately 77% and 81% in successive 10-second windows |
| GUI heartbeat | Continued through 18:41:31 UTC, beyond the old failure window |

CPU idle was calculated from `/proc/stat` counter deltas. Before the fix,
the saturated board had zero advancing CPU0 idle ticks. Initial testing
supports the fix; it does not establish long-term stability or explain the
rendering-load mechanism independently of the GPIO failure.

For follow-up validation, keep LOADGEN OFF, run item 4 for at least ten minutes,
tap through screens, and confirm touch IRQs, RPMsg counters and heartbeats
continue advancing. Check multiple 10-second CPU-idle windows, repeat START/STOP,
and compare FreeRTOS RPMsg under the same conditions. These extended checks
were not completed in the initial deployment test.

The detailed investigation remains in `docs/A55_0_SATURATION_DEBUG.md` locally;
that file is ignored by Git in this checkout. This guide preserves the fix
and validation record in the versioned firmware documentation.
