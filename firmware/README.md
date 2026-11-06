# Building `rt_latency_cm33.elf` for Verdin i.MX93

The `rt_latency_cm33.elf` originally shipped in `app/custom/elf/` is the
upstream NXP console-only benchmark. It does not implement the
`rt_latency_shm_t` protocol used by this GUI. Its EVK linker script also
places `m_data2` at `0xA0000000`, which is live Linux kernel memory on this
module (`/proc/iomem`: `98000000-a3ffffff : System RAM`).

This directory carries two downstream patches and a reproducible build:

1. relocate `m_data2` to the reserved Verdin/Lino carveout;
2. add the missing M33 shared-memory producer.


## Shared-memory addresses

| Where | Setting | Value |
|---|---|---|
| Linux app, rebuilt FreeRTOS | `M33_RTLAT_SHM_BASE` in `app/custom/real_time_edge/main.h` | `0xA5000000` |
| Device tree | `cm33-shm@a5000000` in `device-tree/overlays/lino-imx93_cm33-shm_overlay.dts` | `0xA5000000`, 16 MB |
| Firmware | `__noncachedata_start__` from `ORIGIN(m_data2)` | `0xA5000000`, 16 MB |
| Linux app, prebuilt Zephyr/legacy images | `M33_LEGACY_SHM_BASE` | `0xA4000000` |

The firmware uses that linker symbol as an explicit `volatile` pointer; it
does not rely on a C object being allocated in `.ncache`.

The prebuilt Zephyr image contains a literal `0xA4000000` and its source/build
inputs are not present in this repository, so the Linux app must select the
address per firmware. Its resource table advertises no virtio device; the
base-DT vring carveout can therefore carry the legacy raw protocol while that
image is running. The rebuilt FreeRTOS producer stays in the dedicated A500
carveout.

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
- `0xA4000000` — the demo's original shared address — is retained only for
  the prebuilt, non-virtio Zephyr/legacy images.
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

The overlay needs TorizonCore Builder and a reboot. It reserves A500 and
disables Linux ownership of LPUART2 and TPM2/3/4, which are initialized by the
M33 latency firmwares. It also keeps the LPUART2 clock gate initialized, as in
the original NXP multicore board DTS:

```bash
# Compile the overlay
dtc -@ -I dts -O dtb \
  -o lino-imx93_cm33-shm_overlay.dtbo \
  ../device-tree/overlays/lino-imx93_cm33-shm_overlay.dts
```

Then apply per the
[Toradex Device Tree Overlays guide](https://developer.toradex.com/torizon/os-customization/use-cases/device-tree-overlays-on-torizon/).
Use TorizonCore Builder (`dto apply`, `union`, `deploy`) from a workspace
that contains the base DTB and this overlay DTS.

After reboot, confirm the carveout is reserved and out of System RAM:

```bash
grep -E "a5000000|System RAM" /proc/iomem
ls /proc/device-tree/reserved-memory/ | grep cm33
```

`0xA5000000` must NOT appear inside a `System RAM` span.

### Current target status (2026-08-18)

The overlay is deployed on Torizon OS 7.7.0 build 40. The target reports
`a5000000-a5ffffff : reserved`, both the panel and SHM overlays applied
successfully, and the display/LVGL application still operate normally.

The updated ELF booted and remoteproc reached `running`. An initial test still
remained at `flag=1`, `wr_idx=0`; inspection then found one address error and
one resource-ownership violation:

- the prebuilt Zephyr image still accesses `0xA4000000`, while the Linux app
  had been changed globally to A500;
- Linux still owned LPUART2/TPM4 (and the timer handoff was incomplete), even
  though both M33 latency firmwares initialize those peripherals before
  acknowledging the start request.

The deployed app now selects A500 only for rebuilt FreeRTOS and A400 for
prebuilt Zephyr/legacy images. The deployed overlay disables LPUART2 and
TPM2/3/4 on Linux. Direct backend validation after reboot produced:

- FreeRTOS: `flag:1` → `flag:2`, `wr_idx:0` → `wr_idx:16`, with IRQ latency
  statistics consumed from A500;
- Zephyr: `flag:1` → `flag:2`, `wr_idx:0` → `wr_idx:3`, with CPU/IRQ statistics
  consumed from A400;
- no `Boot failed` message in either run.

The Compose stack also recovers the Torizon 7.7 MediaMix deferred-probe race
and reloads the M33 firmware automatically. A host reboot confirmed DRM,
Weston, remoteproc and the LVGL application return without manual commands.
