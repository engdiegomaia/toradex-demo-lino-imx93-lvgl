# Rebuilding `rt_latency_cm33.elf` for Verdin i.MX93

The `rt_latency_cm33.elf` shipped in `app/custom/elf/` is the **NXP EVK
build** and does not work on this module. It links its non-cached shared
memory to `0xA0000000`, which on Verdin i.MX93 is live Linux kernel memory
(`/proc/iomem`: `98000000-a3ffffff : System RAM`).

This directory holds the one-line patch that repoints it, plus a build
script. See [`../docs/M33_HANDSHAKE_DEBUG.md`](../docs/M33_HANDSHAKE_DEBUG.md)
for the full root-cause analysis.

## The three addresses that must agree

| Where | Setting | Value |
|---|---|---|
| Linux app | `FRDM_SHM_BASE` in `app/custom/real_time_edge/main.h` | `0xA5000000` |
| Device tree | `cm33-shm@a5000000` in `device-tree/overlays/lino-imx93_cm33-shm_overlay.dts` | `0xA5000000`, 16 MB |
| Firmware | `ORIGIN(m_data2)` in `MIMX9352_cm33_ram.ld` | `0xA5000000`, 16 MB |

The first two are already committed. **The third requires rebuilding the
firmware — that is what this directory is for.** Until it is done the
handshake still fails with `wr_idx:0` and `Boot failed`.

## Why 0xA5000000

- Upstream uses `0xA0000000` (16 MB) for `m_data2` on `mcimx93evk`. That is
  System RAM here.
- `0xA4000000` — the demo's original `FRDM_SHM_BASE` — is `vdev0vring0`,
  owned by the RPMsg/virtio transport.
- `0xA5000000` sits inside `a4120000-ffffffff : System RAM`, clear of the
  M33 carveout that ends at `0xA4120000`, and is 16 MB-aligned, which the
  ARMv8-M MPU wants for a region of that size.

## Build

Requires the MCUXpresso SDK West workspace and `arm-none-eabi-gcc`.

```bash
./build-rt-latency-cm33.sh
```

The script fetches upstream `heterogeneous-multicore`, applies
`patches/0001-cm33-shm-base-verdin-imx93.patch`, builds the CM33 release
target, and copies the ELF to `app/custom/elf/rt_latency_cm33.elf`.

## Verifying the result

Before deploying, confirm the new ELF actually moved:

```bash
readelf -sW app/custom/elf/rt_latency_cm33.elf | grep noncachedata
# expect: a5000000 ... __noncachedata_start__
readelf -S app/custom/elf/rt_latency_cm33.elf | grep ncache
# expect: .ncache PROGBITS a5000000
```

Both must read `a5000000`. If they still say `a0000000`, the patch did not
apply and the build used a cached linker script.

## Deploying

The overlay needs TorizonCore Builder and a reboot:

```bash
# Compile the overlay
dtc -@ -I dts -O dtb \
  -o lino-imx93_cm33-shm_overlay.dtbo \
  ../device-tree/overlays/lino-imx93_cm33-shm_overlay.dts
```

Then apply per the
[Toradex Device Tree Overlays guide](https://developer.toradex.com/torizon/os-customization/use-cases/device-tree-overlays-on-torizon/).
After reboot, confirm the carveout is reserved and out of System RAM:

```bash
grep -E "a5000000|System RAM" /proc/iomem
ls /proc/device-tree/reserved-memory/ | grep cm33
```

`0xA5000000` must NOT appear inside a `System RAM` span.
