#!/usr/bin/env bash
#
# Rebuild rt_latency_cm33.elf with the shared-memory window repointed for
# Verdin i.MX93. See README.md for why this is necessary.
#
# The upstream default (ORIGIN(m_data2) = 0xa0000000) lands in live Linux
# System RAM on this module. This script applies a one-line linker patch
# moving it to the reserved 0xA5000000 carveout.
#
# Requires: west, cmake, ninja, arm-none-eabi-gcc (ARMGCC_DIR set).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORKSPACE="${WORKSPACE:-${SCRIPT_DIR}/workspace}"
PATCH="${SCRIPT_DIR}/patches/0001-cm33-shm-base-verdin-imx93.patch"

# Expected shared-memory base. Must match FRDM_SHM_BASE in
# app/custom/real_time_edge/main.h and cm33-shm@ in the DT overlay.
EXPECTED_BASE="a5000000"

LD_REL="os/freertos/Core_M/boards/mcimx93evk/armgcc/MIMX9352_cm33_ram.ld"
APP_REL="apps/rt_latency/freertos/boards/mcimx93evk/cm33/armgcc"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

command -v west >/dev/null || die "west not found. Install the MCUXpresso SDK tooling first."
[ -n "${ARMGCC_DIR:-}" ] || die "ARMGCC_DIR is not set — the SDK build needs it to locate arm-none-eabi-gcc."
[ -f "${PATCH}" ] || die "patch not found: ${PATCH}"

# --- fetch upstream -------------------------------------------------------
if [ ! -d "${WORKSPACE}/heterogeneous-multicore" ]; then
    echo "==> initialising west workspace in ${WORKSPACE}"
    mkdir -p "${WORKSPACE}"
    git -C "${WORKSPACE}" clone \
        https://github.com/nxp-real-time-edge-sw/heterogeneous-multicore.git
    ( cd "${WORKSPACE}/heterogeneous-multicore" \
      && west init -l . \
      && west update )
else
    echo "==> reusing existing workspace ${WORKSPACE}"
fi

HMC="${WORKSPACE}/heterogeneous-multicore"
[ -f "${HMC}/${LD_REL}" ] || die "linker script missing at ${HMC}/${LD_REL} — upstream layout changed."

# --- apply the patch (idempotent) ----------------------------------------
cd "${HMC}"
if grep -q "ORIGIN = 0x${EXPECTED_BASE}" "${LD_REL}"; then
    echo "==> linker script already repointed to 0x${EXPECTED_BASE}"
elif patch -p1 --dry-run --silent < "${PATCH}"; then
    echo "==> applying ${PATCH##*/}"
    patch -p1 < "${PATCH}"
else
    die "patch does not apply to this upstream revision. Re-diff it against ${LD_REL}."
fi

grep -q "ORIGIN = 0x${EXPECTED_BASE}" "${LD_REL}" \
    || die "linker script does not contain 0x${EXPECTED_BASE} after patching."

# --- build ---------------------------------------------------------------
# Invocation copied from upstream apps/.../cm33/armgcc/build_release.sh
echo "==> building rt_latency CM33 (release)"
cd "${HMC}/${APP_REL}"
rm -rf release
west sdk_build -p always ../../../../ -b mcimx93evk --config release \
     -Dcore_id=cm33 -d release

ELF="$(find "${HMC}/${APP_REL}/release" -name '*.elf' -print -quit)"
[ -n "${ELF}" ] || die "build produced no .elf under ${HMC}/${APP_REL}/release"

# --- verify the ELF actually moved --------------------------------------
echo "==> verifying shared-memory base in $(basename "${ELF}")"
if ! readelf -sW "${ELF}" | grep -qi "^ *[0-9]*: *${EXPECTED_BASE} .*__noncachedata_start__"; then
    readelf -sW "${ELF}" | grep -i noncachedata || true
    die "__noncachedata_start__ is not at 0x${EXPECTED_BASE}. Refusing to install."
fi
echo "    __noncachedata_start__ = 0x${EXPECTED_BASE} OK"

# --- install -------------------------------------------------------------
DEST="${REPO_ROOT}/app/custom/elf/rt_latency_cm33.elf"
if [ -f "${DEST}" ]; then
    cp -a "${DEST}" "${DEST}.evk-backup"
    echo "==> previous ELF saved to ${DEST}.evk-backup"
fi
cp "${ELF}" "${DEST}"
echo "==> installed ${DEST}"
echo
echo "Next: sync to target, apply the DT overlay, reboot."
echo "See ${SCRIPT_DIR#"${REPO_ROOT}/"}/README.md"
