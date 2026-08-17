# Building `rt_latency_cm33.elf` for Verdin i.MX93

The `rt_latency_cm33.elf` originally shipped in `app/custom/elf/` is the
upstream NXP console-only benchmark. It does not implement the
`rt_latency_shm_t` protocol used by this GUI. Its EVK linker script also
places `m_data2` at `0xA0000000`, which is live Linux kernel memory on this
module (`/proc/iomem`: `98000000-a3ffffff : System RAM`).

This directory carries two downstream patches and a reproducible build:

1. relocate `m_data2` to the reserved Verdin/Lino carveout;
2. add the missing M33 shared-memory producer.

See [`../docs/M33_HANDSHAKE_DEBUG.md`](../docs/M33_HANDSHAKE_DEBUG.md) for
the investigation history.

## The three addresses that must agree

| Where | Setting | Value |
|---|---|---|
| Linux app | `FRDM_SHM_BASE` in `app/custom/real_time_edge/main.h` | `0xA5000000` |
| Device tree | `cm33-shm@a5000000` in `device-tree/overlays/lino-imx93_cm33-shm_overlay.dts` | `0xA5000000`, 16 MB |
| Firmware | `__noncachedata_start__` from `ORIGIN(m_data2)` | `0xA5000000`, 16 MB |

The firmware uses that linker symbol as an explicit `volatile` pointer; it
does not rely on a C object being allocated in `.ncache`.

## Producer protocol

The implementation follows Toradex's Cortex-M shared-memory pattern: reserve
a dedicated DDR range with `no-map`, access the same physical address from
both cores, and keep it separate from remoteproc/RPMsg memory.

The existing NXP i.MX93 board setup already maps DDR as outer-shareable and
non-cacheable in the M33 MPU. The producer adds the safeguards needed by this
multi-field ABI:

- static size/offset assertions on both A55 and M33 builds;
- `volatile` shared accesses and ARM data-memory barriers;
- `flag=1` request and `flag=2` acknowledgement only after benchmark tasks
  start;
- payload-first, `wr_idx`-last publication;
- no overwrite while `rd_idx != wr_idx`.

It samples the upstream live latency accumulators once per second. The CM33
target does not enable NXP CPU-load accounting, so `cpu_load` is currently
reported as `0.0`.

Toradex warns that raw shared memory lacks the safety guarantees supplied by
RPMsg/RemoteProc. The barriers, ownership rule, and commit marker here make
the inherited ABI usable, but production hardening should also add an ABI
version/magic, integrity checking, and explicit recovery from a stalled peer.

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

The script fetches upstream `heterogeneous-multicore`, applies both patches,
builds the CM33 release target, verifies the address and producer symbols,
then copies the ELF to `app/custom/elf/rt_latency_cm33.elf`. The original ELF
is preserved once as `rt_latency_cm33.elf.evk-backup` (gitignored).

## Verifying the result

Before deploying, confirm the new ELF actually moved:

```bash
readelf -sW app/custom/elf/rt_latency_cm33.elf | grep noncachedata
# expect: a5000000 ... __noncachedata_start__
readelf -S app/custom/elf/rt_latency_cm33.elf | grep ncache
# expect: .ncache PROGBITS a5000000
readelf -sW app/custom/elf/rt_latency_cm33.elf \
  | grep -E 'shm_control_task|rt_latency_shm_publish'
# expect both producer symbols
```

The `.ncache` section may have size zero: the producer follows Toradex's
direct-address example and uses `__noncachedata_start__` as the pointer rather
than placing storage in the ELF. Its address and both producer symbols must
still be present.

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

### Current target status (2026-08-17)

The overlay is deployed on Torizon OS 7.7.0 build 40. The target reports
`a5000000-a5ffffff : reserved`, both the panel and SHM overlays applied
successfully, and the display/LVGL application still operate normally.

End-to-end validation is still pending: deploy the updated ELF and Linux
binary, rebuild `m33-loader` and `lvgl-app`, then confirm remoteproc is
`running`, the handshake reaches `flag=2`, `wr_idx` advances, and the latency
charts populate without `Boot failed` messages.
