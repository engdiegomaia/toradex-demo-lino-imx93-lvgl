# M33 rt_latency handshake debug — state as of 2026-08-06

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
