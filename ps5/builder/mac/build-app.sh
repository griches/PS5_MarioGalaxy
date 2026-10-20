#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the builder's Mac app, with the
# builder (build/builder-tools/mac) and the kit (build/builder-kit-mac) inside,
# signed with the Developer ID in MAC_SIGN_IDENTITY (default: the keychain's
# first one), zipped as dist/builder/SuperMarioGalaxy-PS5-Builder-VERSION-mac.zip.
# The app is made in a temporary folder: a folder under a File Provider (an
# iCloud-synced ~/Documents) gets Finder information that codesign refuses.
#
#   ps5/builder/mac/build-app.sh VERSION    (prints the app's path and the zip's)
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
version=${1:?usage: build-app.sh VERSION}
name="Super Mario Galaxy PS5 Builder"
stage="${TMPDIR:-/tmp}/galaxy-builder-app"
app="$stage/$name.app"
identity=${MAC_SIGN_IDENTITY:-$(security find-identity -v -p codesigning | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)}
[[ -n $identity ]] || { echo "build-app.sh: no Developer ID Application identity" >&2; exit 2; }
for input in "$root/build/builder-tools/mac/galaxy-builder" "$root/build/builder-kit-mac/flags/game.txt"; do
    [[ -e $input ]] || { echo "build-app.sh: missing $input" >&2; exit 2; }
done
rm -rf "$stage"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
swiftc -O -parse-as-library -target arm64-apple-macos13.0 "$root/ps5/builder/mac/BuilderApp.swift" \
    -o "$app/Contents/MacOS/SuperMarioGalaxyPS5Builder"
cp "$root/build/builder-tools/mac/galaxy-builder" "$app/Contents/MacOS/"
cp -R "$root/build/builder-kit-mac" "$app/Contents/Resources/kit"

# The icon: the port's own title art (ps5/art/icon0-source.png)
iconset=$(mktemp -d)/AppIcon.iconset
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
    sips -z $size $size "$root/ps5/art/icon0-source.png" --out "$iconset/icon_${size}x${size}.png" >/dev/null
    sips -z $((size * 2)) $((size * 2)) "$root/ps5/art/icon0-source.png" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/AppIcon.icns"

cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>$name</string>
    <key>CFBundleDisplayName</key><string>$name</string>
    <key>CFBundleIdentifier</key><string>mobi.bouncingball.galaxy-ps5-builder</string>
    <key>CFBundleExecutable</key><string>SuperMarioGalaxyPS5Builder</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$version</string>
    <key>CFBundleVersion</key><string>$version</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>LSArchitecturePriority</key><array><string>arm64</string></array>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSHumanReadableCopyright</key><string>GPL-3.0. Not affiliated with Nintendo.</string>
</dict>
</plist>
PLIST

# No extended attributes (codesign refuses Finder information and resource forks)
xattr -cr "$app"
# Sign the helpers first (every Mach-O the builder runs), then the app (its own
# executable with the bundle); hardened runtime.
while IFS= read -r -d '' binary; do
    if file "$binary" | grep -q "Mach-O"; then
        codesign --force --timestamp --options runtime -s "$identity" "$binary" 2>/dev/null
    fi
done < <(find "$app/Contents/Resources/kit/bin" "$app/Contents/MacOS/galaxy-builder" -type f -print0)
codesign --force --timestamp --options runtime -s "$identity" "$app"
codesign --verify --strict --deep "$app"
echo "$app (signed: $identity)"
zip="$root/dist/builder/SuperMarioGalaxy-PS5-Builder-$version-mac.zip"
mkdir -p "$(dirname "$zip")"
rm -f "$zip"
ditto -c -k --norsrc --noextattr --keepParent "$app" "$zip"
echo "$zip ($(du -h "$zip" | cut -f1))"
