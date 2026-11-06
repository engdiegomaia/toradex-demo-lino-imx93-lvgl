#!/usr/bin/env bash
#
# Build the Cortex-M33 Zephyr RPMsg firmware for Verdin i.MX93 from the Zephyr
# sample samples/subsys/ipc/openamp_rsc_table (board imx93_evk/mimx9352/m33).
#
# The sample's overlay already places the shared memory at 0xA4000000 and the
# resource table at 0x2001E000, which the module's base device tree reserves,
# so no source change is needed. It announces "rpmsg-client-sample" and echoes
# the payload; the HMI measures the round trip over rpmsg_char.
#
# Requires: west, cmake, ninja, pyelftools, and an arm-none-eabi toolchain
# (used through ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb). pyelftools may be
# provided with PYTHONPATH when it is not installed in the west environment.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORKSPACE="${WORKSPACE:-${SCRIPT_DIR}/workspace}"
ZEPHYR="${WORKSPACE}/zsdk/zephyr"
SAMPLE="samples/subsys/ipc/openamp_rsc_table"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build-rpmsg-zephyr}"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

command -v west >/dev/null || die "west not found."
[ -d "${ZEPHYR}" ] || die "Zephyr tree not found at ${ZEPHYR}."
python3 -c 'import elftools' 2>/dev/null \
    || die "python module 'elftools' (pyelftools) missing; install it or set PYTHONPATH."

if [ -z "${GNUARMEMB_TOOLCHAIN_PATH:-}" ]; then
    GNUARMEMB_TOOLCHAIN_PATH="$(find "${HOME}/toolchains" -maxdepth 3 -type f -name arm-none-eabi-gcc \
        -printf '%h\n' 2>/dev/null | sed 's:/bin$::' | sort -u | head -n1)"
    [ -n "${GNUARMEMB_TOOLCHAIN_PATH}" ] || die "set GNUARMEMB_TOOLCHAIN_PATH to the toolchain root."
fi
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb GNUARMEMB_TOOLCHAIN_PATH

cd "${ZEPHYR}"
west build -p always -b imx93_evk/mimx9352/m33 "${SAMPLE}" -d "${BUILD_DIR}"

ELF="${BUILD_DIR}/zephyr/zephyr_openamp_rsc_table.elf"
[ -f "${ELF}" ] || die "build produced no ${ELF}"
"${GNUARMEMB_TOOLCHAIN_PATH}/bin/arm-none-eabi-readelf" -SW "${ELF}" | grep -q '\.resource_table' \
    || die ".resource_table missing; refusing to install."

cp "${ELF}" "${REPO_ROOT}/app/custom/elf/zephyr_rpmsg_cm33.elf"
echo "==> installed ${REPO_ROOT}/app/custom/elf/zephyr_rpmsg_cm33.elf"
