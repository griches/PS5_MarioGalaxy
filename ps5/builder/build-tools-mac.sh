#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the builder's own tools for macOS
# (Apple silicon): nebula-recomp (vendor/nebula, nebula.patch applied),
# PS5_Vulkan's ps5-native-tool and the builder, into build/builder-tools/mac/.
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
out="$root/build/builder-tools/mac"
mkdir -p "$out"
export MACOSX_DEPLOYMENT_TARGET=13.0
(cd "$root/vendor/nebula" && cargo build --release --locked -p nebula-recomp)
cp "$root/vendor/nebula/target/release/nebula-recomp" "$out/"
native="$vulkan/tooling/native"
clang++ -std=c++20 -O2 -Wall -Wextra -mmacosx-version-min=13.0 "$native/native_app_builder.cpp" \
    "$native/self_container.cpp" "$native/elf_object.cpp" "$native/sce_module_writer.cpp" -lz -o "$out/ps5-native-tool"
clang++ -std=c++20 -O2 -Wall -Wextra -mmacosx-version-min=13.0 "$root/ps5/builder/builder.cpp" -o "$out/galaxy-builder"
for tool in nebula-recomp ps5-native-tool galaxy-builder; do
    echo "$tool: $(otool -L "$out/$tool" | tail -n +2 | awk '{print $1}' | tr '\n' ' ')"
done
