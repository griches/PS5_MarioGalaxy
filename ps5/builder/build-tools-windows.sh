#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the builder's tools for Windows, built
# on a Windows PC over SSH (key login) from the Mac: sends the sources (nebula-recomp's
# workspace with nebula.patch applied, ps5-native-tool, the builder, zlib), runs
# build-tools-windows.bat there and fetches the programs into build/builder-tools/windows/.
#
#   ps5/builder/build-tools-windows.sh USER@HOST [FOLDER]   (FOLDER on the PC, default
#                                                           C:/Users/<user>/src/PS5_MarioGalaxyBuilder)
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
pc=${1:?usage: build-tools-windows.sh USER@HOST [FOLDER]}
folder=${2:-C:/Users/${pc%@*}/src/PS5_MarioGalaxyBuilder}
zlib="$root/.deps/zlib-1.3.1.tar.gz"
[[ -f $zlib ]] || curl -sfL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz -o "$zlib"
echo "9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23  $zlib" | shasum -a 256 -c --quiet

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/nebula/third_party" "$stage/nebula/runtime" "$stage/native" "$stage/builder" "$stage/zlib"
(cd "$root/vendor/nebula" && COPYFILE_DISABLE=1 tar -cf - Cargo.toml Cargo.lock crates metadata \
    third_party/dolphin-free-dsp-rom runtime/include) | tar -xf - -C "$stage/nebula"
cp "$vulkan"/tooling/native/{native_app_builder.cpp,self_container.cpp,elf_object.cpp,sce_module_writer.cpp,*.hpp} "$stage/native/"
cp "$root/ps5/builder/builder.cpp" "$stage/builder/"
mkdir -p "$stage/app"
cp "$root/ps5/builder/windows/Builder.cs" "$stage/app/"
magick "$root/ps5/art/icon0-source.png" -define icon:auto-resize=256,128,64,48,32,16 "$stage/app/app.ico"
tar -xzf "$zlib" -C "$stage/zlib" --strip-components 1
sed $'s/$/\r/' "$root/ps5/builder/build-tools-windows.bat" > "$stage/build-tools-windows.bat"  # cmd wants CRLF
# keep the PC's cargo cache (nebula/target) between runs
ssh "$pc" "powershell -NoProfile -Command \"New-Item -ItemType Directory -Force -Path '$folder' | Out-Null\""
(cd "$stage" && COPYFILE_DISABLE=1 tar -cf - .) | ssh "$pc" "tar -xf - -C \"$folder\""
ssh "$pc" "cmd /c \"${folder//\//\\}\\build-tools-windows.bat\"" 2>&1 | grep -v "^ *Compiling \|^ *Downloaded \|^ *Downloading " | tail -20
mkdir -p "$root/build/builder-tools/windows"
for tool in nebula-recomp ps5-native-tool galaxy-builder "Super Mario Galaxy PS5 Builder"; do
    scp -q "$pc:$folder/out/${tool// /\\ }.exe" "$root/build/builder-tools/windows/"
done
ls -la "$root/build/builder-tools/windows"
