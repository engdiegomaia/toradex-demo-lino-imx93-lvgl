#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
output_dir="${project_dir}/app/platform/linux/build-aarch64-docker"
image_name="lvgl-performance-monitor:aarch64-build"

mkdir -p "${output_dir}"

docker build --tag "${image_name}" --file "${project_dir}/docker/Dockerfile.aarch64-build" "${project_dir}/docker"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "${project_dir}:/src:ro" \
    --volume "${output_dir}:/out" \
    --env PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
    "${image_name}" \
    cmake -S /src/app/platform/linux -B /out -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=/src/docker/aarch64-toolchain.cmake \
        -DWAYLAND_PROTOCOLS_BASE=/usr/share/wayland-protocols \
        -DLV_BUILD_CONF_DIR=/src/app/platform/linux \
        -DCONFIG_LV_BUILD_DEMOS=OFF \
        -DCONFIG_LV_BUILD_EXAMPLES=OFF \
        -DCONFIG_LV_USE_THORVG_INTERNAL=OFF \
        -DCONFIG_GG_USE_G2D=OFF
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "${project_dir}:/src:ro" \
    --volume "${output_dir}:/out" \
    "${image_name}" \
    cmake --build /out --parallel
