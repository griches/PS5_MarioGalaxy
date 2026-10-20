#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the builder for Windows as a release
# zip: the app, galaxy-builder.exe and the kit in one folder.
#
#   ps5/builder/package-windows.sh VERSION
#       -> dist/builder/SuperMarioGalaxy-PS5-Builder-VERSION-windows.zip
#
# Needs build/builder-tools/windows (build-tools-windows.sh) and
# build/builder-kit-windows (make-kit.sh windows).
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
version=${1:?usage: package-windows.sh VERSION}
name="Super Mario Galaxy PS5 Builder"
tools="$root/build/builder-tools/windows"
kit="$root/build/builder-kit-windows"
for input in "$tools/$name.exe" "$tools/galaxy-builder.exe" "$kit/flags/game.txt" "$kit/bin/clang.exe"; do
    [[ -e $input ]] || { echo "package-windows.sh: missing $input" >&2; exit 2; }
done
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
folder="$stage/$name"
mkdir -p "$folder"
cp "$tools/$name.exe" "$tools/galaxy-builder.exe" "$folder/"
cp -R "$kit" "$folder/kit"
sed $'s/$/\r/' > "$folder/README.txt" <<EOF
Super Mario Galaxy PS5 Builder $version

Makes the PlayStation 5 title PPSA99315 from your own Super Mario Galaxy disc
(USA, RMGE01, revision 0): .iso, .wbfs, .rvz, .wia, .ciso or .gcz.

1. Open "$name.exe". If SmartScreen says it doesn't know the app, choose
   More info, then Run anyway.
2. Drop your disc image onto the window and click Build (about 12 GB free
   needed; it takes a few minutes).
3. Copy the PPSA99315 folder it makes to /data/homebrew/ on your PS5 over FTP.

Keep this folder together: the app needs galaxy-builder.exe and kit\ beside it.
The title you build contains code translated from your disc: it is for your
own console, please don't share it.

https://github.com/griches/PS5_MarioGalaxy
Not affiliated with or endorsed by Nintendo. Licences: kit\title\licenses\.
EOF
zip="$root/dist/builder/SuperMarioGalaxy-PS5-Builder-$version-windows.zip"
mkdir -p "$(dirname "$zip")"
rm -f "$zip"
(cd "$stage" && COPYFILE_DISABLE=1 zip -q -r -X -9 "$zip" "$name")
echo "$zip ($(du -h "$zip" | cut -f1))"
