#!/usr/bin/env bash
# Builds mesa the way weaselway ships it, without installing anything.
# See WEASELWAY.md. Runs itself inside `nix develop` if not already there.
#
#   ./weaselway-build.sh              configure on first run, then compile
#   RECONFIGURE=1 ./weaselway-build.sh  re-apply the flags below to an existing build dir
#   BUILDTYPE=debugoptimized ./weaselway-build.sh
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${IN_NIX_SHELL:-}" ]; then
    exec nix develop "${SOURCE_DIR}" -c "$0" "$@"
fi

BUILD_DIR="${BUILD_DIR:-${SOURCE_DIR}/build/nix}"
BUILDTYPE="${BUILDTYPE:-release}"

# Same flags as weaselway/dev/build-mesa.sh, minus prefix/libdir (no install).
MESON_FLAGS=(
    --buildtype="${BUILDTYPE}"
    -Dglvnd=enabled
    -Dplatforms=x11,wayland
    -Degl-native-platform=surfaceless
    -Dgallium-drivers=softpipe,d3d12
    -Dvulkan-drivers=swrast,microsoft-experimental
    -Dgallium-d3d12-graphics=enabled
    -Dgallium-d3d12-video=enabled
    -Dshader-cache=enabled
)

if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
    meson setup "${BUILD_DIR}" "${SOURCE_DIR}" "${MESON_FLAGS[@]}"
elif [ -n "${RECONFIGURE:-}" ]; then
    meson setup --reconfigure "${BUILD_DIR}" "${SOURCE_DIR}" "${MESON_FLAGS[@]}"
fi

meson compile -C "${BUILD_DIR}"
