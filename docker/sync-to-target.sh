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

echo "[sync] Done. Next on target:"
echo "  cd ${remote_dir}"
echo "  docker compose -f docker/docker-compose.yml down --remove-orphans"
echo "  ./docker/rebuild-app.sh"
echo "  docker compose -f docker/docker-compose.yml up"
