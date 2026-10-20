#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - vkd3d (Wine's Direct3D 12 on Vulkan),
# which runs Nebula's Direct3D 12 renderer unchanged on the console's RADV.
#
#   ps5/tools/setup-vkd3d.sh            (in the VM; build.sh runs it)
#
# Fetches the release into .deps/vkd3d with ps5/patches/vkd3d.patch applied,
# then builds it once for the VM itself (~/ps5/build/vkd3d-host): that build
# makes the parser sources (bison/flex) and the version header the console
# build compiles, and its vkd3d-compiler turns Nebula's presentation shaders
# into bytecode at build time (ps5/tools/post-shaders.sh), as fxc does on Windows.
# Prints the host build's source folder.
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
version=2.1
deps=$root/.deps
src=$deps/vkd3d
host=${VKD3D_HOST_DIR:-$HOME/ps5/build/vkd3d-host}
patch=$ps5/patches/vkd3d.patch
stamp="$version $(sha256sum "$patch" | cut -c1-16)"

if [[ $(cat "$src/.ps5-stamp" 2>/dev/null) != "$stamp" ]]; then
    mkdir -p "$deps/dl"
    tarball=$deps/dl/vkd3d-$version.tar.xz
    [[ -f $tarball ]] || curl -fsSL -o "$tarball" "https://dl.winehq.org/vkd3d/source/vkd3d-$version.tar.xz"
    rm -rf "$src" "$deps/vkd3d-$version"
    tar -xf "$tarball" -C "$deps"
    mv "$deps/vkd3d-$version" "$src"
    patch -d "$src" -p1 --quiet < "$patch"
    echo "$stamp" > "$src/.ps5-stamp"
    rm -rf "$host"
fi

if [[ $(cat "$host/.ps5-stamp" 2>/dev/null) != "$stamp" ]]; then
    rm -rf "$host" && mkdir -p "$host"
    cp -a "$src" "$host/src"
    (
        cd "$host/src"
        ./configure CC=gcc CPPFLAGS="-I$HOME/ps5/PS5_Mesa/include -I$HOME/ps5/dxc/src/external/SPIRV-Headers/include" \
            --disable-tests --disable-doxygen-doc --without-ncurses --without-xcb > ../configure.log 2>&1 \
            || { tail -30 ../configure.log >&2; exit 1; }
        make -j"$(nproc)" > ../make.log 2>&1 || { tail -30 ../make.log >&2; exit 1; }
    )
    echo "$stamp" > "$host/.ps5-stamp"
fi
echo "$host/src"
