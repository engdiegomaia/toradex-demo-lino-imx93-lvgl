#!/usr/bin/env bash
#
# Build the Cortex-M33 FreeRTOS RPMsg ping-pong firmware for Verdin i.MX93 from
# the MCUXpresso SDK example rpmsg_lite_pingpong_rtos_linux (remote), with the
# demo patch that makes it echo forever and stay silent on the console.
#
# The example uses the vring addresses (0xA4000000/0xA4010000) and the
# resource table that the module's base device tree already reserves, so no
# linker or device-tree change is needed. Requires: west, cmake, ninja and an
# arm-none-eabi toolchain (ARMGCC_DIR or one under ~/toolchains).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORKSPACE="${WORKSPACE:-${SCRIPT_DIR}/workspace}"
SDK="${WORKSPACE}/mcuxsdk/mcuxsdk"
PATCH="${SCRIPT_DIR}/patches/0003-cm33-rpmsg-pingpong-endless-silent.patch"
EXAMPLE="examples/multicore_examples/rpmsg_lite_pingpong_rtos_linux/remote"
SRC_REL="examples/multicore_examples/rpmsg_lite_pingpong_rtos/secondary/main.c"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build-rpmsg}"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

command -v west >/dev/null || die "west not found."
[ -d "${SDK}" ] || die "MCUXpresso SDK not found at ${SDK} (run build-rt-latency-cm33.sh first to set up the workspace)."
[ -f "${PATCH}" ] || die "patch not found: ${PATCH}"

if [ -z "${ARMGCC_DIR:-}" ]; then
    ARMGCC_DIR="$(find "${HOME}/toolchains" -maxdepth 3 -type f -name arm-none-eabi-gcc \
        -printf '%h\n' 2>/dev/null | sed 's:/bin$::' | sort -u | head -n1)"
    [ -n "${ARMGCC_DIR}" ] || die "set ARMGCC_DIR to the arm-none-eabi toolchain root."
    export ARMGCC_DIR
fi
[ -x "${ARMGCC_DIR}/bin/arm-none-eabi-gcc" ] || die "no arm-none-eabi-gcc under ${ARMGCC_DIR}"

# Apply the patch once (idempotent).
cd "${SDK}"
if grep -q "echo forever" "${SRC_REL}"; then
    echo "==> demo patch already applied"
elif patch -p1 --dry-run --silent --forward --batch < "${PATCH}"; then
    patch -p1 --forward --batch < "${PATCH}"
else
    die "patch does not apply to this SDK revision."
fi

west sdk_build -p always "${EXAMPLE}" -b mcimx93evk --config release \
     -Dcore_id=cm33 -d "${BUILD_DIR}"

ELF="$(find "${BUILD_DIR}" -maxdepth 1 -name '*.elf' -print -quit)"
[ -n "${ELF}" ] || die "build produced no .elf in ${BUILD_DIR}"

# remoteproc needs the resource table, and the app needs the NS announce.
"${ARMGCC_DIR}/bin/arm-none-eabi-readelf" -SW "${ELF}" | grep -q '\.resource_table' \
    || die ".resource_table missing from ${ELF}; refusing to install."

DEST="${REPO_ROOT}/app/custom/elf/rpmsg_pingpong_cm33.elf"
cp "${ELF}" "${DEST}"
echo "==> installed ${DEST}"
