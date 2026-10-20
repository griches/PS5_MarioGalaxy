#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the game's code from your own disc,
# for building the title: on the Mac (or any machine with Rust), before
# ps5/tools/vm.sh build.
#
#   ps5/tools/prepare-game.sh DISC_IMAGE [--content]
#
# Applies ps5/patches/nebula.patch to vendor/nebula (once), builds nebula-recomp,
# extracts the disc (USA RMGE01 revision 0: ISO, RVZ, WBFS...) into dump/disc and
# translates it into C++ in gamedata/generated. With --content it also makes
# gamedata/content and gamedata/RMGE01_boot_image.bin for console.sh game-files
# (the title makes the same files itself from a disc image in dump/ on the console).
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
disc=${1:?usage: prepare-game.sh DISC_IMAGE [--content]}
nebula="$root/vendor/nebula"

[[ -f $nebula/Cargo.toml ]] || git -C "$root" submodule update --init vendor/nebula
if git -C "$nebula" diff --quiet; then
    git -C "$nebula" apply "$root/ps5/patches/nebula.patch"
    echo "applied ps5/patches/nebula.patch to vendor/nebula"
fi
(cd "$nebula" && cargo build --release --locked -p nebula-recomp)
recomp="$nebula/target/release/nebula-recomp"

"$recomp" identify "$disc"
[[ -d $root/dump/disc ]] || "$recomp" extract "$disc" --output "$root/dump/disc" | grep -v '^PROGRESS'
mkdir -p "$root/gamedata"
rm -rf "$root/gamedata/generated"
"$recomp" generate "$root/dump/disc" --output "$root/gamedata/generated" | grep -v '^PROGRESS'
if [[ ${2:-} == --content ]]; then
    rm -rf "$root/gamedata/content"
    "$recomp" package-content "$root/dump/disc" --output "$root/gamedata/content" | grep -v '^PROGRESS'
    "$recomp" build-boot-image "$root/dump/disc" --output "$root/gamedata/RMGE01_boot_image.bin"
fi
echo "gamedata/generated is ready: ps5/tools/vm.sh build"
