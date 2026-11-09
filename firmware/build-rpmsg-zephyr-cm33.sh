#!/usr/bin/env bash
#
# Build the Cortex-M33 Zephyr RPMsg firmware for Verdin i.MX93 from the Zephyr
# sample samples/subsys/ipc/openamp_rsc_table (board imx93_evk/mimx9352/m33).
#
# The sample's overlay already places the shared memory at 0xA4000000 and the
# resource table at 0x2001E000, which the module's base device tree reserves,
# so the echo loop and Linux GPIO ownership are patched (0004/0005): it announces
# "rpmsg-client-sample" and echoes the payload forever; the HMI measures the round trip over rpmsg_char.
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

PATCH="${SCRIPT_DIR}/patches/0004-zephyr-rpmsg-client-endless-silent.patch"
SRC_REL="${SAMPLE}/src/main_remote.c"
[ -f "${PATCH}" ] || die "patch not found: ${PATCH}"

cd "${ZEPHYR}"
# Apply the demo patch once (idempotent): the upstream sample answers only 100
# messages and then destroys its endpoint.
if grep -q "echo forever" "${SRC_REL}"; then
    echo "==> demo patch already applied"
elif patch -p1 --dry-run --silent --forward --batch < "${PATCH}"; then
    patch -p1 --forward --batch < "${PATCH}"
else
    die "patch does not apply to this Zephyr revision."
fi

GPIO_PATCH="${SCRIPT_DIR}/patches/0005-zephyr-imx93-preserve-linux-gpio-security.patch"
[ -f "${GPIO_PATCH}" ] || die "patch not found: ${GPIO_PATCH}"
if patch -p1 --dry-run --silent --forward --batch < "${GPIO_PATCH}"; then
    patch -p1 --forward --batch < "${GPIO_PATCH}"
elif patch -p1 --dry-run --silent --reverse --batch < "${GPIO_PATCH}"; then
    echo "==> GPIO ownership patch already applied"
else
    die "GPIO ownership patch does not apply to this Zephyr revision."
fi

west build -p always -b imx93_evk/mimx9352/m33 "${SAMPLE}" -d "${BUILD_DIR}"

# This firmware leaves all GPIO banks owned by Linux. Refuse a configuration
# that would initialize GPIO drivers under that ownership contract.
grep -qx '# CONFIG_GPIO is not set' "${BUILD_DIR}/zephyr/.config" \
    || die "RPMsg firmware must leave CONFIG_GPIO disabled for Linux ownership."

ELF="${BUILD_DIR}/zephyr/zephyr_openamp_rsc_table.elf"
[ -f "${ELF}" ] || die "build produced no ${ELF}"
"${GNUARMEMB_TOOLCHAIN_PATH}/bin/arm-none-eabi-readelf" -SW "${ELF}" | grep -q '\.resource_table' \
    || die ".resource_table missing; refusing to install."

cp "${ELF}" "${REPO_ROOT}/app/custom/elf/zephyr_rpmsg_cm33.elf"
echo "==> installed ${REPO_ROOT}/app/custom/elf/zephyr_rpmsg_cm33.elf"
