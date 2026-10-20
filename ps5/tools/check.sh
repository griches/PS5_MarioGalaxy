#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - compile runtime sources for the console
# without linking, to see what the port still lacks.
#
#   ps5/tools/vm.sh check FILE...     (paths relative to vendor/nebula/runtime/src)
#
# SPDX-License-Identifier: GPL-3.0-only
set -uo pipefail
ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
sdk=$(bash "$ps5/tools/setup-sdk.sh")
nebula="$root/vendor/nebula"
flags=(--sysroot="$sdk/target" -std=c++20 -march=znver2 -fsyntax-only -ferror-limit=0
    -fexperimental-library -fdeclspec -Wno-unknown-attributes -Wno-ignored-attributes
    -DNOMINMAX -DWIN32_LEAN_AND_MEAN -DGALAXY_PS5=1 -DSOFTFLOAT_FAST_INT64=1 -include "$ps5/compat/include/msvc_crt.h"
    -I"$ps5/compat/include" -I"$nebula/runtime/include" -I"$ps5/src" -I"$ps5/compat/generated"
    -I"$nebula/third_party/berkeley-softfloat-3/source/include" -I"$nebula/runtime/softfloat")
status=0
for file in "$@"; do
    echo "== $file"
    case $file in
    ps5/*) source_file="$root/$file" ;;
    *) source_file="$nebula/runtime/src/$file" ;;
    esac
    "$sdk/bin/prospero-clang++" "${flags[@]}" -I"$ps5/compat/src" -c "$source_file" 2>&1 || status=1
done
exit $status
