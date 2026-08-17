# M33 rt_latency handshake debug — ROOT CAUSE FOUND 2026-08-17

> **2026-08-17 — Root cause identified: shared-memory address mismatch.**
> The Linux side polled `0xA4000000`, which the Verdin i.MX93 base device
> tree assigns to **`vdev0vring0`** — virtio/RPMsg vring memory, not a free
> carveout. The rt_latency demo does not use RPMsg; it maps a plain struct.
> Meanwhile the shipped `rt_latency_cm33.elf` links `__noncachedata_start__`
> to `0xA0000000`, which `/proc/iomem` shows as live **System RAM**
> (`98000000-a3ffffff`). The two sides never referenced the same memory, so
> the `flag=1 -> flag=2` handshake could not complete under any conditions.
>
> The D-cache hypothesis below is **falsified** — see "Corrections".
> Sections below this banner are preserved as the historical record.

## Corrections to earlier claims (2026-08-17)

**Item 4 of "confirmed as fact" was wrong.** It claimed the rt_latency ELF
references `0xA4000000` in its *literal pool*. It does contain the byte
pattern 9 times, but every occurrence is at file offset `0xa845`–`0xcdad`,
which falls inside **`.symtab`** (`0x00ac6c`–`0x1045c`). `.text` ends at
`0x90b0` and `.symtab` is not loaded onto the M33. These are debug symbol
values, not code literals. Verified with `readelf -S` + offset mapping.

**The D-cache hypothesis is falsified.** `readelf -sW` shows the firmware
has a dedicated non-cached section and calls `XCACHE_CleanCache`:

```
[13] .ncache   PROGBITS  a0000000  00a0fc  000000  W
1133: a0000000  0 NOTYPE GLOBAL __noncachedata_start__
1166: a0000000  0 NOTYPE GLOBAL __noncachedata_end__
1033: 0ffe58ad 24 FUNC   GLOBAL XCACHE_CleanCache
```

Coherency was handled correctly by the firmware author. The addresses
simply never matched.

**The devmem probe in move #3 never ran.** `devmem-probe.out` in the
2026-08-06 tarball is five lines of `devmem: applet not found`. It was
filed as evidence but produced none. busybox in that container has no
`devmem` applet.

**`.resource_table` is empty.** 16 bytes: `01 00 00 00` then zeros —
`ver=1, num=0`. Zero resource entries, so remoteproc is told nothing about
carveouts. `state: running` therefore means only "reset released"; it
carries no information about firmware memory setup.

## Ground truth from the running device tree

Decompiled from `m33-diag-20260806-234228/fdt.bin`:

```
vdev0vring0@a4000000   reg = <0x0 0xa4000000 0x0 0x8000>   no-map
vdev0vring1@a4008000   reg = <0x0 0xa4008000 0x0 0x8000>   no-map
vdev1vring0@a4010000   reg = <0x0 0xa4010000 0x0 0x8000>   no-map
vdev1vring1@a4018000   reg = <0x0 0xa4018000 0x0 0x8000>   no-map
vdevbuffer@a4020000    reg = <0x0 0xa4020000 0x0 0x100000> shared-dma-pool
rsc-table@2021e000     reg = <0x0 0x2021e000 0x0 0x1000>   no-map

remoteproc-cm33 {
    compatible = "fsl,imx93-cm33";
    memory-region = <0x09 0x0a 0x0b 0x0c 0x0d 0x0e>;  /* the 6 above */
};
```

The M33 carveout ends at `0xA4120000`. `/proc/iomem` confirms
`a4120000-ffffffff : System RAM`, so there was **no** free reserved region
for a raw shared struct — one has to be created.

## Upstream firmware source — located 2026-08-17

The firmware is NXP's `heterogeneous-multicore` (matches this repo's
`app/SCR.txt` and the `rt_latency_board.c` FILE symbol in the shipped ELF):

- Repo: https://github.com/nxp-real-time-edge-sw/heterogeneous-multicore
- App:  `apps/rt_latency/freertos/boards/mcimx93evk/cm33/`
- **Linker script (the relevant file):**
  `os/freertos/Core_M/boards/mcimx93evk/armgcc/MIMX9352_cm33_ram.ld:41`

```
m_data2  (RW) : ORIGIN = 0xa0000000, LENGTH = 0x01000000
```

`.ncache` and `.ncache.init` are both placed `> m_data2`, so that single
`ORIGIN` is what puts the shared struct at `0xA0000000`. Changing it moves
the shared memory — no source-code edit is required.

## The fix (2026-08-17)

Three addresses must agree. Chosen base: **`0xA5000000`, 16 MB** — inside
`a4120000-ffffffff : System RAM`, clear of the M33 carveout ending at
`0xA4120000`, and 16 MB-aligned for the ARMv8-M MPU. Length matches
upstream's `m_data2`.

1. **DONE — DT overlay** `device-tree/overlays/lino-imx93_cm33-shm_overlay.dts`
   reserves 16 MB at `0xA5000000` per the Toradex Cortex-M Shared Memory
   Guide. Compiles clean with `dtc -@`. Deliberately does *not* touch
   `remoteproc-cm33`'s `memory-region` — a dtbo fragment replaces rather
   than appends, which would drop the vdev phandles and break RPMsg.

2. **DONE — Linux side** `FRDM_SHM_BASE` in
   `app/custom/real_time_edge/main.h`: `0xA4000000` -> `0xA5000000`.
   Cross-build clean.

3. **READY TO RUN, NOT YET BUILT — firmware relink.**
   `firmware/patches/0001-cm33-shm-base-verdin-imx93.patch` changes
   `ORIGIN(m_data2)` to `0xa5000000`. Verified to apply clean against
   pristine upstream, and idempotent on re-run.
   `firmware/build-rt-latency-cm33.sh` fetches upstream, applies the patch,
   builds, **refuses to install unless `__noncachedata_start__` really
   reads `a5000000`**, and installs to `app/custom/elf/`.
   Needs `west` + `ARMGCC_DIR` (MCUXpresso SDK tooling), which is not
   present on this host — so the ELF in `app/custom/elf/` is **still the
   EVK build**.

**Until step 3 runs and the overlay is deployed, the handshake still fails
with `wr_idx:0` / `Boot failed`.** Steps 1 and 2 alone are not sufficient.

### Deployment order (all three needed)

```bash
# 1. HOST — rebuild firmware (needs MCUXpresso SDK + ARMGCC_DIR)
./firmware/build-rt-latency-cm33.sh

# 2. HOST — compile + apply overlay via TorizonCore Builder, then reboot
dtc -@ -I dts -O dtb -o lino-imx93_cm33-shm_overlay.dtbo \
    device-tree/overlays/lino-imx93_cm33-shm_overlay.dts

# 3. TARGET — after reboot, verify the carveout left System RAM
grep -E "a5000000|System RAM" /proc/iomem   # a5000000 must NOT be System RAM
ls /proc/device-tree/reserved-memory/ | grep cm33

# 4. HOST — ship app + new ELF
./docker/sync-to-target.sh
```

Expected on success: `[shm-diag]` shows `flag:2` and `wr_idx` incrementing,
and the BenchMarking charts populate.

## The failure (historical, as of 2026-08-06)

## The failure

- rt_latency_cm33.elf boots successfully via remoteproc. `dmesg` shows
  "remote processor imx-rproc is now up".
- The Linux side (LVGL app) writes `pshm->flag = 1` at 0xA4000000 and waits
  for the M33 to answer `2` within 3 polls. That answer never comes.
- Result: `Boot failed! reboot it! fail_cnt:N` fires, BenchMarking screen
  stays empty.

## What has been confirmed as fact (with commands)

1. **0xA4000000 is a valid, reserved carveout in the Toradex Verdin i.MX93
   base device tree.** `iomem.out` shows `a4000000-a411ffff : reserved`.
   `/proc/device-tree/reserved-memory/` lists vdev0vring0..vdev1vring1,
   vdevbuffer, rsc-table.  No DT overlay needed.

2. **Linux mmap of 0xA4000000 works and writes reach DDR.** After
   [shm-canary] was added, we see `read:0xDEADBEEF OK` on every first-map,
   and the diag print shows `flag:1 rd_idx:0 wr_idx:0` holding steady
   for the full 4 s handshake window. Linux writes stick.

3. **The M33 side never writes back.** `wr_idx` stays 0 for the entire
   handshake. If the firmware were writing to a cached alias only, we'd
   see values fluctuate; steady 0 rules out several races.

4. **The rt_latency ELF references 0xA4000000 in its literal pool.** See
   `firmware-shm-refs` in the latest tarball. Confirms the firmware was
   built against the same shm base the Linux side uses.

## Actively suspected root cause

M33 firmware writes to 0xA4000000 via its **cached** normal-memory alias
(default MPU attribute on Cortex-M33 for RAM regions unless
`ARM_MPU_SetMemAttr` marks it non-cacheable). Writes sit in D-cache;
Linux sees the old value. Symmetrically, the M33 reads the cached line
and never observes the `1` Linux wrote.

Confidence: **hypothesis, not fact**. Cannot prove without either the
firmware source or a JTAG dump.

Alternative (weaker) hypotheses still on the table:
- Firmware boots into a fault handler before reaching main(). No M33
  console has been captured yet — the LPUART node in the DT may be
  claimed by Linux. Without console, "remoteproc up" only means the
  reset was released.
- Firmware TYPE differs — Linux writes `pshm->type = test_type` (=1),
  firmware might expect a different code before it starts producing
  samples.

## What is NOT the root cause (falsified)

- **TRDC / ATF blocking access** — falsified. Previous session's
  conclusion was built on `dd 2>/dev/null` which hid a STRICT_DEVMEM
  refusal. New evidence (`iomem` + successful canary write) proves the
  region is a plain no-map carveout mmap-able from userspace.
- **Missing reserved-memory DT node** — falsified. Toradex ships them.
- **Stdio buffering hiding logs** — falsified. `setvbuf(_IONBF)`
  landed and `[shm-diag]` lines appear each second.
- **Stale binary in container** — falsified. `sync-to-target.sh` now
  auto-runs `build-aarch64.sh` when source is newer.

## Next moves, ordered by cost

### 1. Get an M33 console (highest information yield)

Identify the LPUART used by rt_latency_cm33 (`board.h` in the firmware
sources — not in this repo). Cross-check against `/proc/device-tree`
for a matching `lpuart@*` node claimed by Linux. If Linux claims it,
propose a DT overlay setting `status = "disabled"` on that node.

Without this, every next step is guessing.

### 2. Get the firmware source

The prebuilt ELFs live in `app/custom/elf/`. The source is in one of:
- NXP MCUXpresso SDK for i.MX93 — search for `rt_latency` project
- NXP `mcux-sdk` repo on GitHub
- Toradex Cortex-M example repo (they maintain forks)

Once we have `BOARD_ConfigMPU()`, the answer is one grep away:
- Search for `MPU_ATTR` / `ARM_MPU_SetMemAttr` on the region containing
  0xA4000000.
- If the region is marked cacheable, the fix is either
  `ARM_MPU_SetMemAttr(idx, ARM_MPU_ATTR(ARM_MPU_ATTR_NON_CACHEABLE, ...))`
  or explicit `SCB_CleanDCache_by_Addr` after every shared-mem write.

### 3. Independent devmem confirmation

With the app container stopped and M33 stopped, run a privileged debian
container and `busybox devmem 0xA4000000 32 0xDEADBEEF; busybox devmem
0xA4000000 32`. Then start the firmware (echo start > .../remoteproc0/state)
and poll `busybox devmem 0xA4000000 32` every second for 30 s. If the
value ever becomes `2` (or anything nonzero written by the firmware),
we know the firmware IS writing and only the Linux side was hiding it.

Documented in this repo under Phase 3 of the debug plan.

## Runbook — how to reproduce the current state cleanly

```bash
# HOST
./docker/sync-to-target.sh       # auto-builds if source is newer

# TARGET
ssh torizon@<target-host>.local
cd ~/lino-imx93-mallow-dsi-lvgl
./docker/rebuild-app.sh
docker compose -f docker/docker-compose.yml down --remove-orphans
docker compose -f docker/docker-compose.yml up -d

# Hard gate — new symbols must be present
docker exec lino-lvgl-app strings /app/gui_guider | grep -E 'shm-diag|shm-canary|auto-restart disabled'
# Expected 3 lines.

# Freeze state on first fail so devmem probes remain valid
docker exec lino-lvgl-app touch /tmp/rt_no_autorestart

# From the UI, tap Start on rt_latency. Then:
docker logs --since 30s lino-lvgl-app | grep -E 'shm-diag|shm-canary|Boot failed|holding state'
```

## Files touched to reach this state

- `app/custom/real_time_edge/rt_stats.c` — `[shm-diag]` per-second log,
  `[shm-canary]` first-map sanity check.
- `app/custom/real_time_edge/main.c` — `setvbuf(_IONBF)` for unbuffered
  stdio, `access("/tmp/rt_no_autorestart")` + `RT_NO_AUTORESTART` gate.
- `docker/sync-to-target.sh` — auto-invokes `build-aarch64.sh` when
  source is newer than the binary the Dockerfile copies.
- `docker/docker-compose.yml` — commented-out `RT_NO_AUTORESTART=1`.
- `scripts/collect-m33-diag.sh` — added `firmware-shm-refs` probe that
  greps the shipped ELFs for 0xA4000000 references, plus readelf/strings.

## Ground rules that stay in force

- Never conclude from a command whose stderr was discarded.
- Distinguish fact / inference / hypothesis in every claim.
- Verify against Toradex Developer Center, NXP RM, and upstream kernel
  before asserting binding, address, or driver behavior.
- One commit per logical change, conventional message.
- Order actions from lowest to highest risk. Mark the first that needs
  a reboot.
