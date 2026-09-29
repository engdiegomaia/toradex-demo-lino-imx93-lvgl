# M33 rt_latency handshake debug — integration fixes 2026-08-18

> **Current status.** The original FreeRTOS failure included a missing
> producer: the upstream/shipped CM33 latency benchmark only logged to UART
> and never implemented the `rt_latency_shm_t` ABI polled by Linux. A separate
> address error also had to be fixed: the EVK linker put `m_data2` in live
> Linux RAM on this module.
>
> The rebuilt producer and dedicated `0xA5000000` carveout are in place, but
> the first runtime test still stopped at `flag=1`. The follow-up found that
> the app incorrectly polled A500 for the prebuilt Zephyr binary (which embeds
> A400), and that Linux still owned peripherals used by both M33 latency
> firmwares. Address selection is now firmware-specific and the overlay hands
> LPUART2/TPM2–4 to the M33 while keeping the UART clock gate initialized. The
> revised overlay and application are deployed. A post-deployment boot exposed
> a separate MediaMix deferred-probe race; `display-init` now recovers DRM and
> the resident `m33-loader` starts remoteproc automatically after reboot.
> Infrastructure and handshake validation now pass. Direct diagnostic runs of
> both Benchmarking backends reached `flag=2`, advanced `wr_idx`, and delivered
> latency samples to Linux at their firmware-specific SHM addresses.
> Sections labeled historical below are retained as the investigation record.

## Corrections to earlier claims (2026-08-17)

**Item 4 of "confirmed as fact" was wrong.** It claimed the rt_latency ELF
references `0xA4000000` in its *literal pool*. It does contain the byte
pattern 9 times, but every occurrence is at file offset `0xa845`–`0xcdad`,
which falls inside **`.symtab`** (`0x00ac6c`–`0x1045c`). `.text` ends at
`0x90b0` and `.symtab` is not loaded onto the M33. These are debug symbol
values, not code literals. Verified with `readelf -S` + offset mapping.

**The D-cache hypothesis is falsified.** The mcimx93evk board initialization
maps the entire relevant DDR range as outer-shareable, non-cacheable. The
old ELF's `.ncache` section was zero-sized because it had no producer data:

```
[13] .ncache   PROGBITS  a0000000  00a0fc  000000  W
1133: a0000000  0 NOTYPE GLOBAL __noncachedata_start__
1166: a0000000  0 NOTYPE GLOBAL __noncachedata_end__
```

The new producer uses that non-cacheable mapping plus explicit memory
barriers. Cache maintenance was not the missing operation.

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

## CORRECTION 2026-08-17 (later) — the address mismatch is NOT the root cause

After rebuilding the firmware and inspecting upstream, the diagnosis above
is **incomplete and its conclusion is wrong**. Corrected finding:

**The shipped `rt_latency_cm33.elf` does not implement the shared-memory
protocol at all.**

Evidence:

1. Upstream `apps/rt_latency/` contains **no** `rt_latency_shm_t`, `wr_idx`,
   `rd_idx`, or flag handshake. `grep -rn "rt_latency_shm\|wr_idx"` over the
   whole `heterogeneous-multicore` tree returns nothing. It reports results
   via `log_info()` — i.e. the **UART console**.
2. The originally shipped `rt_latency_cm33.elf` has **0** symbols matching
   `shm|wr_idx|rd_idx`, and the same holds for the other **M33** ELFs in
   `app/custom/elf/`.

   Corrected 2026-08-18: an earlier revision of this document claimed the
   A55 ELFs were consumers too. That was wrong, and it was wrong for the
   same reason several other claims here were - it read `strings` and symbol
   names instead of disassembling. Both `rt_latency_ca55_*` images are
   complete **producers**: each has an `sp_shm` OBJECT in `.data`, carries
   the `"sec:%d shm_addr:0x%llx size:%d"` format string in a *loaded*
   section (`.text` for FreeRTOS, `rodata` for Zephyr), and in `print_stats`
   loads a shm pointer, increments `wr_idx` (`add w0, w0, #1`;
   `str w0, [x20, #12]`) and stores stats at offsets 32/48/64/96 - an exact
   match for the 104-byte `rt_latency_shm_t`. The A55 firmware therefore
   needs no rebuild; see [`CA55_AMP_SPIKE.md`](CA55_AMP_SPIKE.md).
3. `.ncache` is zero-length and `m_data2` reports 0.00% used in **both** the
   original EVK build and the relinked one — nothing is allocated there.
4. Neither ELF contains its shm base as an aligned literal in loaded code.

So `wr_idx` stays 0 because **no shipped firmware ever increments it**.
There is no producer. The Linux side in `rt_stats.c` polls a protocol that
the M33 binary does not speak.

### What this means for the address work

The FreeRTOS address mismatch documented below is **real** — upstream's
`0xA0000000` is System RAM here — but fixing it cannot make the handshake
work, because the old firmware never writes to either address. The dedicated
A500 overlay, firmware-specific Linux base, and linker relink are all still
required for the rebuilt producer. They are not sufficient by themselves.

### The actual gap at that point

This demo needed an M33 firmware that implements the `rt_latency_shm_t`
producer. The GUI-Guider demo was ported from a Real-Time Edge package whose
M33 binary had that protocol; the ELFs in `app/custom/elf/` do not. Options,
in order of cost at the time, were:

1. **Get the matching firmware** from the NXP Real-Time Edge release the
   GUI-Guider demo came from (not the `heterogeneous-multicore` upstream —
   that variant logs to UART instead).
2. **Write the producer** against `rt_latency_shm_t` in `basedef.h`:
   place the struct at `ORIGIN(m_data2)`, set `flag=2` on seeing `flag=1`,
   fill `irq_delay`/`irq_sched`, increment `wr_idx` once per second. The
   upstream `rt_latency` stats are already computed in
   `apps/rt_latency/common/rt_latency.c` — only the shm export is missing.
3. **Re-target the Linux side to the console path** — read latency from the
   M33 UART instead of shared memory, matching what upstream actually does.
   Needs the M33 console, which is the still-outstanding hardware blocker.

### Falsified by this correction

- "The two sides never referenced the same memory, so the handshake could
  not complete" — true about addresses, but the operative reason is that
  the firmware has no handshake code at all.
- The relink alone was described as sufficient once applied. It is not.

## Producer implemented 2026-08-17

The missing FreeRTOS CM33 producer is now carried as
`firmware/patches/0002-cm33-rt-latency-shm-producer.patch` and applied by
`firmware/build-rt-latency-cm33.sh` after the linker relocation.

The implementation was adapted to Toradex's documented raw shared-memory
model rather than inventing an RPMsg dependency: Linux reserves the DDR range
with `no-map`, both cores use the same physical address, and the firmware
accesses it through an explicit volatile pointer. NXP's mcimx93evk CM33 board
setup already configures `0x80000000-0xDfffffff` as outer-shareable,
non-cacheable DDR.

Additional protocol safety for this multi-field structure:

- compile-time ABI size and offset checks on both cores;
- data-memory barriers around handshake and publication;
- start test case `type` when Linux sets `flag=1`, then acknowledge with
  `flag=2` only after the benchmark tasks exist;
- copy a consistent latency snapshot under a short FreeRTOS critical section;
- write payload first and increment `wr_idx` last as the commit marker;
- preserve unread data while `rd_idx != wr_idx`.

The producer publishes the upstream latency accumulators once per second.
CPU-load tracking is not enabled in this upstream CM33 configuration, so that
field is explicitly `0.0`.

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

`.ncache` and `.ncache.init` are both placed `> m_data2`. The new producer
uses `__noncachedata_start__` as its explicit pointer, so changing `ORIGIN`
moves that pointer. The relocation alone never supplied the missing protocol.

## The fix (2026-08-17)

For the rebuilt FreeRTOS producer, three addresses must agree. Chosen base:
**`0xA5000000`, 16 MB** — inside
`a4120000-ffffffff : System RAM`, clear of the M33 carveout ending at
`0xA4120000`, and 16 MB-aligned for the ARMv8-M MPU. Length matches
upstream's `m_data2`.

1. **DONE — DT overlay** `device-tree/overlays/lino-imx93_cm33-shm_overlay.dts`
   reserves 16 MB at `0xA5000000` per the Toradex Cortex-M Shared Memory
   Guide. Compiles clean with `dtc -@`. Deliberately does *not* touch
   `remoteproc-cm33`'s `memory-region` — a dtbo fragment replaces rather
   than appends, which would drop the vdev phandles and break RPMsg.

2. **DONE — Linux side** `M33_RTLAT_SHM_BASE` in
   `app/custom/real_time_edge/main.h` is `0xA5000000`. The prebuilt Zephyr
   image was verified to contain a literal `0xA4000000`, so
   `M33_LEGACY_SHM_BASE` preserves that address for Zephyr/legacy images.
   Cross-build clean and its disassembly contains both branch-specific bases.

3. **DONE — firmware producer and relink.**
   `firmware/patches/0001-cm33-shm-base-verdin-imx93.patch` changes
   `ORIGIN(m_data2)` to `0xa5000000`; patch 0002 adds the producer. The
   build completed with Arm GCC 13.2.1 and installed the verified ELF to
   `app/custom/elf/rt_latency_cm33.elf`. The script refuses installation
   unless the base is `a5000000` and both producer symbols are present.

## Target validation status — 2026-08-18

Completed on the validation target with Torizon OS 7.7.0 build 40:

- TorizonCore Builder deployed both
  `lino-imx93_panel-cap-touch-10inch-dsi_overlay.dtbo` and
  `lino-imx93_cm33-shm_overlay.dtbo` successfully;
- `/proc/device-tree/reserved-memory/cm33-shm@a5000000` exists;
- `/proc/iomem` reports `a5000000-a5ffffff : reserved`, splitting System RAM
  into `a4120000-a4ffffff` and `a6000000-ffffffff` around the carveout;
- the 10.1-inch display and the online LVGL application remained functional
  after reboot.

The first runtime test of the rebuilt FreeRTOS image reached remoteproc
`running`, but remained at `flag=1`, `wr_idx=0`. The prebuilt Zephyr image did
the same while Linux was incorrectly polling A500 for every M33 image.

Further inspection found that the M33 firmware initializes LPUART2 and TPM
counters before acknowledging `flag=1`, while the earlier Linux DT still had
LPUART2 and TPM4 enabled. The deployed overlay now disables LPUART2 and
TPM2/3/4, and keeps the LPUART2 gate initialized as in NXP's original
multicore board DTS. The deployed Linux app now polls the address actually
embedded in each firmware.

After deployment, the kernel intermittently timed out the initial probe of
`4ac10000.system-controller`, leaving `/dev/dri/card0` absent. Manually
rebinding the `imx93-blk-ctrl` driver immediately restored imx-drm, LVDS-1 and
Weston. The Compose stack now runs `display-init` to retry that bind safely,
and keeps `m33-loader` resident so both helpers restart with Docker. A service
restart followed by a host reboot validated all four containers healthy,
`/dev/dri/card0` present, the MediaMix controller bound, and remoteproc
`running` with `rt_latency_cm33.elf`.

End-to-end diagnostic validation completed after the reboot:

- FreeRTOS M33: `[shm-config] addr:0xa5000000`, `flag:1` → `flag:2`, and
  `wr_idx:0` → `wr_idx:16`, with IRQ delay/scheduling statistics consumed;
- Zephyr M33: `[shm-config] addr:0xa4000000`, `flag:1` → `flag:2`, and
  `wr_idx:0` → `wr_idx:3`, with CPU-load and IRQ statistics consumed;
- neither run emitted `Boot failed`.

The UI dropdown also had only three entries in the wrong order, despite the
backend's fixed four-entry mapping. That made the visible FreeRTOS latency
choice launch `hello_world` and the visible Zephyr latency choice launch the
FreeRTOS benchmark. Both the GUI-Guider project and generated screen now use
the required order: FreeRTOS latency/application, then Zephyr
latency/application. The rebuilt target binary contains that exact order.

The overlay validation proves that Linux no longer owns the shared DDR range;
the direct backend runs additionally prove that both CM33 producers boot and
complete the end-to-end protocol.

### Deployment order (all three needed)

```bash
# 1. HOST — rebuild firmware when source/SDK inputs change
./firmware/build-rt-latency-cm33.sh

# 2. HOST — compile + apply overlay via TorizonCore Builder, then reboot.
#    This revision also disables Linux lpuart2 and tpm2/tpm3/tpm4.
dtc -@ -I dts -O dtb -o lino-imx93_cm33-shm_overlay.dtbo \
    device-tree/overlays/lino-imx93_cm33-shm_overlay.dts

# 3. TARGET — after reboot, verify the carveout left System RAM
grep -E "a5000000|System RAM" /proc/iomem   # a5000000 must NOT be System RAM
ls /proc/device-tree/reserved-memory/ | grep cm33

# 4. HOST — ship app + new ELF
SYNC_TARGET=torizon@<target-host> ./docker/sync-to-target.sh
```

Expected on success (observed on the target): `[shm-config]` shows
`0xa5000000` for FreeRTOS or `0xa4000000` for Zephyr, then `[shm-diag]` shows
`flag:2` and `wr_idx` incrementing, with no `Boot failed` messages, and the
BenchMarking charts populate.

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
SYNC_TARGET=torizon@<target-host> ./docker/sync-to-target.sh  # auto-builds if source is newer

# TARGET
ssh torizon@<target-host>
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
