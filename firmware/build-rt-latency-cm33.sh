#!/usr/bin/env bash
#
# Rebuild rt_latency_cm33.elf with a shared-memory producer and the DDR window
# repointed for Verdin i.MX93. See README.md for the protocol and memory map.
#
# The upstream default (ORIGIN(m_data2) = 0xa0000000) lands in live Linux
# System RAM on this module. This script relocates it to the reserved
# 0xA5000000 carveout and adds the producer missing from upstream.
#
# Requires: west, cmake, ninja, arm-none-eabi-gcc (ARMGCC_DIR or one
# auto-detectable toolchain under ~/toolchains).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORKSPACE="${WORKSPACE:-${SCRIPT_DIR}/workspace}"
LINKER_PATCH="${SCRIPT_DIR}/patches/0001-cm33-shm-base-verdin-imx93.patch"
PRODUCER_PATCH="${SCRIPT_DIR}/patches/0002-cm33-rt-latency-shm-producer.patch"

# Expected shared-memory base. Must match FRDM_SHM_BASE in
# app/custom/real_time_edge/main.h and cm33-shm@ in the DT overlay.
EXPECTED_BASE="a5000000"

LD_REL="os/freertos/Core_M/boards/mcimx93evk/armgcc/MIMX9352_cm33_ram.ld"
APP_REL="apps/rt_latency/freertos/boards/mcimx93evk/cm33/armgcc"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

command -v west >/dev/null || die "west not found. Install the MCUXpresso SDK tooling first."
[ -f "${LINKER_PATCH}" ] || die "patch not found: ${LINKER_PATCH}"
[ -f "${PRODUCER_PATCH}" ] || die "patch not found: ${PRODUCER_PATCH}"

# --- resolve and validate the Arm toolchain ------------------------------
# The SDK derives its compiler paths from ARMGCC_DIR. If ARMGCC_DIR points
# somewhere that does not exist, the build does NOT fail early — it silently
# falls back to guessing "aarch64-none-elf-gcc", which is the 64-bit
# A-profile toolchain and cannot build Cortex-M33 code. Validate up front.
#
# Note the Arm tarball unpacks to a directory containing "Rel1" (capital R)
# while the compiler reports "rel1" — an easy path typo to make.
if [ -z "${ARMGCC_DIR:-}" ]; then
    # Auto-detect a single arm-none-eabi toolchain under ~/toolchains.
    mapfile -t _candidates < <(
        find "${HOME}/toolchains" -maxdepth 3 -type f -name arm-none-eabi-gcc \
             -printf '%h\n' 2>/dev/null | sed 's:/bin$::' | sort -u
    )
    case "${#_candidates[@]}" in
        0) die "ARMGCC_DIR is not set and no arm-none-eabi-gcc found under ~/toolchains.
       Install the Arm GNU toolchain, then: export ARMGCC_DIR=<toolchain-root>" ;;
        1) ARMGCC_DIR="${_candidates[0]}"
           echo "==> auto-detected ARMGCC_DIR=${ARMGCC_DIR}" ;;
        *) printf 'error: multiple toolchains found; set ARMGCC_DIR to one of:\n' >&2
           printf '       %s\n' "${_candidates[@]}" >&2
           exit 1 ;;
    esac
    export ARMGCC_DIR
fi

ARM_CC="${ARMGCC_DIR}/bin/arm-none-eabi-gcc"
[ -d "${ARMGCC_DIR}" ] || die "ARMGCC_DIR does not exist: ${ARMGCC_DIR}
       Check the path casing — the Arm tarball uses 'Rel1', not 'rel1'."
[ -x "${ARM_CC}" ] || die "no arm-none-eabi-gcc at ${ARM_CC}
       ARMGCC_DIR must be the toolchain ROOT (the dir containing bin/).
       A 64-bit aarch64-none-elf toolchain will NOT work — the M33 needs
       32-bit arm-none-eabi."
"${ARM_CC}" -mcpu=cortex-m33 -mthumb -x c -c /dev/null -o /dev/null 2>/dev/null \
    || die "${ARM_CC} cannot target cortex-m33."
echo "==> toolchain: $("${ARM_CC}" -dumpmachine) $("${ARM_CC}" -dumpversion)"

# The SDK's tool.yml validation imports jsonschema; without it the build
# falls back to default version checks and prints a confusing warning.
python3 -c 'import jsonschema' 2>/dev/null \
    || echo "warning: python module 'jsonschema' missing — SDK tool validation will be skipped." >&2

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

# --- apply the patches (idempotent) --------------------------------------
cd "${HMC}"
if grep -q "ORIGIN = 0x${EXPECTED_BASE}" "${LD_REL}"; then
    echo "==> linker script already repointed to 0x${EXPECTED_BASE}"
elif patch -p1 --dry-run --silent --forward --batch < "${LINKER_PATCH}"; then
    echo "==> applying ${LINKER_PATCH##*/}"
    patch -p1 --forward --batch < "${LINKER_PATCH}"
else
    die "patch does not apply to this upstream revision. Re-diff it against ${LD_REL}."
fi

grep -q "ORIGIN = 0x${EXPECTED_BASE}" "${LD_REL}" \
    || die "linker script does not contain 0x${EXPECTED_BASE} after patching."

PRODUCER_SRC="apps/rt_latency/freertos/boards/mcimx93evk/cm33/rt_latency_shm.c"
if [ -f "${PRODUCER_SRC}" ] && grep -q "CONFIG_RT_LATENCY_SHM_EXPORT" \
        apps/rt_latency/freertos/main.c; then
    echo "==> shared-memory producer patch already applied"
elif patch -p1 --dry-run --silent --forward --batch < "${PRODUCER_PATCH}"; then
    echo "==> applying ${PRODUCER_PATCH##*/}"
    patch -p1 --forward --batch < "${PRODUCER_PATCH}"
else
    die "producer patch does not apply to this upstream revision. Rebase ${PRODUCER_PATCH##*/}."
fi

[ -f "${PRODUCER_SRC}" ] \
    || die "shared-memory producer source is missing after patching."

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
# Match on the awk-parsed symbol value rather than a whole-line regex:
# readelf pads the index field ("   929: a5000000 ..."), so anchoring the
# pattern to the line start is brittle. Field 2 is the value, last field is
# the symbol name.
actual_base="$(readelf -sW "${ELF}" \
    | awk '$NF == "__noncachedata_start__" { print $2; exit }')"

if [ -z "${actual_base}" ]; then
    die "__noncachedata_start__ not found in ${ELF}. Refusing to install."
fi
if [ "${actual_base}" != "${EXPECTED_BASE}" ]; then
    readelf -sW "${ELF}" | grep -i noncachedata || true
    die "__noncachedata_start__ is at 0x${actual_base}, expected 0x${EXPECTED_BASE}.
       The linker patch did not take effect. Remove firmware/workspace and retry.
       Refusing to install."
fi
echo "    __noncachedata_start__ = 0x${actual_base} OK"

# The .ncache section itself must land there too, not just the symbol.
actual_sec="$(readelf -SW "${ELF}" \
    | awk '$2 == ".ncache" { print $4; exit }')"
if [ "${actual_sec}" != "${EXPECTED_BASE}" ]; then
    die ".ncache section is at 0x${actual_sec:-?}, expected 0x${EXPECTED_BASE}. Refusing to install."
fi
echo "    .ncache section        = 0x${actual_sec} OK"

# A relink without the producer recreates the original silent firmware, so
# refuse to install unless both the control task and payload publisher exist.
for required_symbol in shm_control_task rt_latency_shm_publish; do
    readelf -sW "${ELF}" | awk -v symbol="${required_symbol}" \
        '$NF == symbol { found = 1 } END { exit !found }' \
        || die "${required_symbol} is missing from ${ELF}. Refusing to install."
    echo "    ${required_symbol} present"
done

# --- install -------------------------------------------------------------
DEST="${REPO_ROOT}/app/custom/elf/rt_latency_cm33.elf"
if [ -f "${DEST}" ] && [ ! -f "${DEST}.evk-backup" ]; then
    cp -a "${DEST}" "${DEST}.evk-backup"
    echo "==> original ELF saved to ${DEST}.evk-backup"
fi
cp "${ELF}" "${DEST}"
echo "==> installed ${DEST}"
echo
echo "Next: sync to target, apply the DT overlay, reboot."
echo "See ${SCRIPT_DIR#"${REPO_ROOT}/"}/README.md"
