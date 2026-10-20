#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the title's RADV archive: PS5_Vulkan's
# pinned Mesa revision with ps5/patches/radv.patch (the Eternal Darkness port's:
# 120 Hz output kept on a TV that runs it as VRR, for frames in between, and its
# HDR10 formats, unused here), built the way PS5_Vulkan's
# tools/build-radv.sh builds its release archive. Prints the archive's path;
# build.sh links it in place of PS5_Vulkan's (RADV_ARCHIVE overrides both).
#
#   PS5_VULKAN_DIR   the PS5_Vulkan checkout (its pin, cross file, SDK and the
#                    host OpenCL tools its own RADV build made)
#   PS5_MESA_FORK    the Mesa fork (default ../PS5_Mesa beside PS5_Vulkan)
#   PS5_RADV_DIR     where to build (default ~/ps5/build/PS5_MarioGalaxy-radv)
#
# A full build takes a while; it runs again only when the pin or the patch changes.
#
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
mesa_fork=${PS5_MESA_FORK:-$vulkan/../PS5_Mesa}
revision=$(sed -n 's/^mesa_revision=//p' "$vulkan/tools/build-radv.sh")
patch="$ps5/patches/radv.patch"
out=${PS5_RADV_DIR:-$HOME/ps5/build/PS5_MarioGalaxy-radv}
sdk="$vulkan/.deps/native/ps5-payload-sdk"
archive="$out/lib/libvulkan_radeon.ps5.a"
stamp="$revision $(sha256sum "$patch" | cut -d' ' -f1) $(cat "$sdk/.ps5-sdk-revision")"
if [[ -f $archive && $(cat "$out/.stamp" 2>/dev/null) == "$stamp" ]]; then
    echo "$archive"
    exit 0
fi
meson=${MESON:-$(command -v meson || echo "$HOME/.local/bin/meson")}
ninja=${NINJA:-$(command -v ninja || echo "$HOME/.local/bin/ninja")}
clc_bin="$vulkan/.deps/work/radv-clc-bin"
[[ -n $revision ]] || { echo "no mesa_revision in $vulkan/tools/build-radv.sh" >&2; exit 2; }
[[ -x $clc_bin/mesa_clc ]] || { echo "run PS5_Vulkan's tools/build-radv.sh release first (host OpenCL tools)" >&2; exit 2; }

src="$out/src"
build="$out/build"
echo "==> [radv] RADV at ${revision:0:12} with ps5/patches/radv.patch (a full build: several minutes)" >&2
rm -rf "$src" "$archive" "$out/.stamp"
mkdir -p "$src"
git -C "$mesa_fork" archive "$revision" | tar -x -C "$src"
patch -d "$src" -p1 --quiet < "$patch"
export PATH="$clc_bin:$PATH"
constants="$out/cross-constants.ini"
printf "[constants]\nsdk = '%s'\n" "$sdk" > "$constants"
# PS5_Vulkan's release options (tools/build-radv.sh).
options=(-Dvulkan-drivers=amd -Dgallium-drivers= -Dplatforms= -Dradv-winsys=ps5
    -Dllvm=disabled -Damd-use-llvm=false -Dvideo-codecs=
    -Dbuildtype=debugoptimized -Db_ndebug=true
    -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled
    -Dvalgrind=disabled -Dlibunwind=disabled -Dzstd=disabled -Dzlib=enabled --force-fallback-for=zlib
    -Dexpat=disabled -Dxmlconfig=disabled -Dshader-cache=enabled -Dbuild-tests=false -Dvulkan-layers= -Dtools=
    -Dmesa-clc=system)
wipe=()
[[ -f $build/build.ninja ]] && wipe=(--wipe)
"$meson" setup "${wipe[@]}" "$build" "$src" \
    --cross-file "$constants" --cross-file "$vulkan/tooling/radv/ps5-cross.ini" \
    "${options[@]}" -Dradv-build-id="$revision" > "$out/setup.log" 2>&1 ||
    { tail -20 "$out/setup.log" >&2; exit 1; }
"$ninja" -C "$build" src/amd/vulkan/libvulkan_radeon.a > "$out/build.log" 2>&1 ||
    { grep -E "error|FAILED" "$out/build.log" | head -20 >&2; exit 1; }
mkdir -p "$out/lib"
cp "$build/src/amd/vulkan/libvulkan_radeon.a" "$archive"
# link-title.sh names the revision from here, as for PS5_Vulkan's archive.
printf 'RADV for Super Mario Galaxy: PS5_Mesa with ps5/patches/radv.patch\nrevision: %s\n' "$revision" \
    > "$out/PROVENANCE.txt"
printf '%s\n' "$stamp" > "$out/.stamp"
echo "$archive"
