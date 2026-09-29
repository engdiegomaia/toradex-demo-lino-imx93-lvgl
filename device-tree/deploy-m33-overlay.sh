#!/usr/bin/env bash
set -euo pipefail

# This script is meant to live in the self-contained TorizonCore Builder
# workspace alongside storage/, device-trees/, the base DTB and the overlay.
workspace=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

overlay_src="lino-imx93_cm33-shm_overlay.dts"
overlay_dtbo="lino-imx93_cm33-shm_overlay.dtbo"
base_dtb="imx93-lino-verdin-dev.dtb"
include_dir="device-trees"
storage_dir="${workspace}/storage"
builder_image="${TCB_IMAGE:-torizon/torizoncore-builder:3}"
branch="${TCB_BRANCH:-lino-imx93-shm-v2}"
remote_host="${1:-${TCB_REMOTE_HOST:-}}"
remote_user="${TCB_REMOTE_USER:-torizon}"

if [[ -z "${remote_host}" ]]; then
	echo "Set TCB_REMOTE_HOST or pass the target host as the first argument" >&2
	exit 2
fi

cd "${workspace}"

for required in "${overlay_src}" "${base_dtb}" "${include_dir}" "${storage_dir}"; do
	if [[ ! -e "${required}" ]]; then
		echo "Missing Builder input: ${workspace}/${required}" >&2
		exit 1
	fi
done

if ! command -v docker >/dev/null 2>&1; then
	echo "docker is required" >&2
	exit 1
fi

# Bind the workspace's existing storage/ explicitly. The standard tcb setup
# otherwise mounts Docker's unrelated global volume named "storage".
tcb_cmd()
{
	docker run --rm \
		--volume /deploy \
		--volume "${workspace}:/workdir" \
		--volume "${storage_dir}:/storage" \
		--volume /var/run/docker.sock:/var/run/docker.sock \
		--network host \
		"${builder_image}" "$@"
}

echo "[m33-overlay] Validating source with the host DTC"
dtc -@ -I dts -O dtb -o "${overlay_dtbo}" "${overlay_src}"

echo "[m33-overlay] Existing overlays"
status=$(tcb_cmd dto status 2>&1)
printf '%s\n' "${status}"

# Updating an overlay with the same output name requires removing its old
# compiled instance first. Keep the panel overlay and every other overlay.
if grep -Fq "lino-imx93_cm33-shm_overlay" <<<"${status}"; then
	echo "[m33-overlay] Removing the previous CM33 overlay"
	tcb_cmd dto remove "${overlay_dtbo}"
fi

echo "[m33-overlay] Applying the revised CM33 overlay"
tcb_cmd dto apply \
	--include-dir "${include_dir}" \
	--device-tree "${base_dtb}" \
	"${overlay_src}"

echo "[m33-overlay] Final overlays"
status=$(tcb_cmd dto status 2>&1)
printf '%s\n' "${status}"
grep -Fq "lino-imx93_panel-cap-touch-10inch-dsi_overlay" <<<"${status}"
grep -Fq "lino-imx93_cm33-shm_overlay" <<<"${status}"

echo "[m33-overlay] Creating OSTree branch ${branch}"
tcb_cmd union --subject "Lino i.MX93 M33 SHM and peripheral handoff v2" "${branch}"

deploy_args=(
	deploy
	--remote-host "${remote_host}"
	--remote-username "${remote_user}"
	--reboot
)
if [[ -n "${TCB_REMOTE_PASSWORD:-}" ]]; then
	deploy_args+=(--remote-password "${TCB_REMOTE_PASSWORD}")
fi
deploy_args+=("${branch}")

echo "[m33-overlay] Deploying ${branch} to ${remote_user}@${remote_host}"
tcb_cmd "${deploy_args[@]}"
