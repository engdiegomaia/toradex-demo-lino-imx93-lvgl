#!/usr/bin/env bash
# Sync the whole project tree (app/, docker/, assets/) from the host to the
# target after a cross-build. Replaces the ad-hoc `rsync app/` step, which
# left docker/ stale and made every recent Dockerfile/compose change invisible
# to the target.
#
# Usage:
#   ./docker/sync-to-target.sh [target-host]
#
# Default target is the Torizon mDNS name shown in the current session.

set -euo pipefail

target="${1:-torizon@<target-host>.local}"
project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
remote_dir="~/lino-imx93-mallow-dsi-lvgl"

echo "[sync] Target:      ${target}"
echo "[sync] Source root: ${project_dir}"
echo "[sync] Remote dir:  ${remote_dir}"

# The runtime Dockerfile COPY-s a pre-built binary; if we skip the cross-build,
# stale objects ship to the target and every downstream log is meaningless.
# Detect source-newer-than-binary and rebuild automatically. Set SKIP_BUILD=1
# to override (e.g. when only touching docker/ or scripts/).
binary="${project_dir}/app/platform/linux/build-aarch64-docker/gui_guider"
if [ "${SKIP_BUILD:-0}" != "1" ]; then
    needs_build=0
    if [ ! -f "${binary}" ]; then
        needs_build=1
    else
        newer=$(find "${project_dir}/app" -name '*.c' -newer "${binary}" -print -quit 2>/dev/null || true)
        if [ -n "${newer}" ]; then
            echo "[sync] Source newer than binary: ${newer}"
            needs_build=1
        fi
    fi
    if [ "${needs_build}" = "1" ]; then
        echo "[sync] Running cross-build before rsync..."
        "${project_dir}/docker/build-aarch64.sh"
    fi
fi

# Sync application code + cross-compiled binaries.
# --delete is safe under app/ because everything there is either committed
# source or generated build artifacts.
rsync -av --delete \
    "${project_dir}/app/" \
    "${target}:${remote_dir}/app/"

# Sync docker infrastructure (Dockerfiles, compose, weston config).
# Excludes the build helper's own working state.
rsync -av --delete \
    --exclude 'build-aarch64/' \
    --exclude '*.log' \
    "${project_dir}/docker/" \
    "${target}:${remote_dir}/docker/"

# Sync static assets referenced at runtime (fonts, images bundled with the UI).
if [ -d "${project_dir}/assets" ]; then
    rsync -av --delete \
        "${project_dir}/assets/" \
        "${target}:${remote_dir}/assets/"
fi

# Sync host-side diagnostic scripts (collect-m33-diag.sh etc).
if [ -d "${project_dir}/scripts" ]; then
    rsync -av --delete \
        "${project_dir}/scripts/" \
        "${target}:${remote_dir}/scripts/"
fi

echo "[sync] Done. Next on target:"
echo "  cd ${remote_dir}"
echo "  docker compose -f docker/docker-compose.yml down --remove-orphans"
echo "  ./docker/rebuild-app.sh"
echo "  docker compose -f docker/docker-compose.yml up"
