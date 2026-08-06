#!/usr/bin/env bash
# Run on the target after 'git pull' to rebuild the LVGL app image without
# leaving behind dangling <none> images.
#
# What this does:
#   1. docker compose build lvgl-app       — rebuilds the image with layer cache
#   2. docker image prune -f               — drops the now-orphaned previous
#      lino-imx93-lvgl-app:latest image (which gets retagged to <none>:<none>
#      as soon as the new build finishes)
#
# Usage (from repo root):
#   ./docker/rebuild-app.sh
#
# Or, if you want a full clean rebuild:
#   ./docker/rebuild-app.sh --no-cache

set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
compose_file="${project_dir}/docker/docker-compose.yml"

echo "[rebuild-app] Building lvgl-app image..."
docker compose -f "${compose_file}" build "$@" lvgl-app

echo "[rebuild-app] Pruning dangling <none> images..."
docker image prune -f

echo "[rebuild-app] Done. Remaining images:"
docker images
