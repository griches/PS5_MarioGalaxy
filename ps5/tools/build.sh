#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - build the title into dist/<TITLE_ID>/.
#
#   ps5/tools/vm.sh build [ninja arguments]
#
# Configures ps5/CMakeLists.txt with the payload SDK's toolchain (once) and
# builds it. The game's modules are C++ that nebula-recomp generated from the
# player's disc into gamedata/generated (ps5/tools/prepare-game.sh, on the Mac).
#
#   PS5_BUILD_DIR    the build tree (vm.sh puts it on the Linux VM's own disk)
#   PS5_VULKAN_DIR   the PS5_Vulkan checkout (its RADV archive, link recipe, packaging)
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail

ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
sdk=$(bash "$ps5/tools/setup-sdk.sh")
generated="$root/gamedata/generated"
[[ -f $generated/game/module.cpp ]] || { echo "no generated game code in $generated (run ps5/tools/prepare-game.sh)" >&2; exit 2; }
# The title's own RADV (120 Hz on a VRR TV); link-title.sh links RADV_ARCHIVE.
export RADV_ARCHIVE=${RADV_ARCHIVE:-$(PS5_VULKAN_DIR="$vulkan" bash "$ps5/tools/setup-radv.sh")}
build=${PS5_BUILD_DIR:-$root/build/game}
ninja=$(command -v ninja || echo "$HOME/.local/bin/ninja")
export PS5_CLANG=${PS5_CLANG:-$(command -v clang || true)}

vkd3d_host=$(bash "$ps5/tools/setup-vkd3d.sh")
if [[ ! -f $build/build.ninja ]] || ! grep -q VKD3D_HOST_DIR "$build/CMakeCache.txt"; then
    mkdir -p "$build"
    cmake -S "$ps5" -B "$build" -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" \
        -DCMAKE_TOOLCHAIN_FILE="$sdk/toolchain/prospero.cmake" \
        -DPS5_PORT_DIR="$root" -DPS5_VULKAN_DIR="$vulkan" -DPS5_GENERATED_DIR="$generated" \
        -DVKD3D_HOST_DIR="$vkd3d_host" -DPS5_VULKAN_HEADERS="${PS5_MESA_DIR:-$HOME/ps5/PS5_Mesa}/include" \
        -DPS5_SPIRV_HEADERS="${PS5_SPIRV_HEADERS:-$HOME/ps5/dxc/src/external/SPIRV-Headers/include}" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=OFF > "$build/configure.log" 2>&1 \
        || { tail -60 "$build/configure.log" >&2; exit 1; }
fi
"$ninja" -C "$build" "${@:-SuperMarioGalaxy}"
