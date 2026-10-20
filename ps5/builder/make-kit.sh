#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the builder's kit: everything the
# builder (ps5/builder/builder.cpp) needs on the player's computer except their
# disc. Run on the Mac after the platform's tools are built (ps5/builder/README.md).
#
#   ps5/builder/make-kit.sh mac|windows KIT_FOLDER
#
# The kit, of which nothing comes from the game:
#   bin/                 clang, ld.lld, llvm-ar (LLVM 22.1.8, .deps/llvm/<platform>),
#                        nebula-recomp, ps5-native-tool (build/builder-tools/<platform>)
#   lib/clang/22/include clang's own headers
#   sdk/target/include   the PS5 payload SDK's headers
#   include/             the headers the translated code includes (compat layer, Nebula)
#   flags/               game.txt, home.txt, dsp.txt: the build's compile flags, from
#                        its own commands ({KIT} the kit, {GEN} nebula-recomp's output)
#   link/                the title's link, recorded by link-title.sh (PS5_KIT_DIR)
#   title/               sce_sys/, sce_module/ and licenses/ of dist/PPSA99315
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
platform=${1:?usage: make-kit.sh mac|windows KIT_FOLDER}
kit=${2:?usage: make-kit.sh mac|windows KIT_FOLDER}
case $platform in
mac) llvm="$root/.deps/llvm/mac" exe="" ;;
windows) llvm="$root/.deps/llvm/win" exe=".exe" ;;
*) echo "make-kit.sh: mac or windows" >&2; exit 2 ;;
esac
tools="$root/build/builder-tools/$platform"
sdk="$root/.deps/native/ps5-payload-sdk"
for file in "$root/.deps/llvm/LICENSE.TXT" "$llvm/bin/llvm-ar$exe" "$tools/nebula-recomp$exe" "$tools/ps5-native-tool$exe" "$root/dist/PPSA99315/eboot.bin"; do
    [[ -e $file ]] || { echo "make-kit.sh: missing $file" >&2; exit 2; }
done

# The link record and the compile commands come from one build of this tree.
PS5_KIT_DIR="$root/build/kit" "$root/ps5/tools/vm.sh" build > "$root/build/kit-build.log" 2>&1 ||
    { tail -20 "$root/build/kit-build.log" >&2; exit 1; }
commands="$root/build/kit-commands.txt"
limactl shell ps5 -- bash -lc 'cd ~/ps5/build/PS5_MarioGalaxy-game && for t in game_module home_module dsp_module; do
        echo "#$t"; ninja -t commands $t | grep -E "functions_0000\.cpp\.o |home_button_native\.cpp\.o |rmge01_dsp\.cpp\.o " | grep -v " qc " | head -1
    done' > "$commands"

rm -rf "$kit"
mkdir -p "$kit/bin" "$kit/lib/clang" "$kit/sdk/target" "$kit/include/compat" "$kit/include/src" "$kit/flags" "$kit/title"

# Tools
if [[ $platform == mac ]]; then
    cp "$llvm/bin/clang-22" "$kit/bin/clang"
    cp "$llvm/bin/lld" "$kit/bin/ld.lld"
else
    cp "$llvm/bin/clang.exe" "$kit/bin/clang.exe"
    cp "$llvm/bin/lld.exe" "$kit/bin/ld.lld.exe"
fi
cp "$llvm/bin/llvm-ar$exe" "$tools/nebula-recomp$exe" "$tools/ps5-native-tool$exe" "$kit/bin/"
cp -R "$llvm/lib/clang/22" "$kit/lib/clang/22"

# Headers
cp -R "$sdk/target/include" "$kit/sdk/target/include"
cp -R "$root/ps5/compat/include" "$kit/include/compat/include"
cp -R "$root/ps5/compat/generated" "$kit/include/compat/generated"
cp -R "$root/vendor/nebula/runtime/include" "$kit/include/runtime"
cp "$root"/ps5/src/*.h "$kit/include/src/"
cp -R "$root/vendor/nebula/third_party/berkeley-softfloat-3/source/include" "$kit/include/softfloat"

# Flags: the build's command for one file of each module, its paths made the kit's,
# behind what the SDK's prospero-clang++ wrapper adds around a compile.
python3 - "$commands" "$kit/flags" "$root" "$sdk" <<'EOF'
import shlex, sys
commands, out, root, sdk = sys.argv[1:]
paths = [
    (f"{root}/ps5/compat/include", "{KIT}/include/compat/include"),
    (f"{root}/ps5/compat/generated", "{KIT}/include/compat/generated"),
    (f"{root}/vendor/nebula/runtime/include", "{KIT}/include/runtime"),
    (f"{root}/vendor/nebula/third_party/berkeley-softfloat-3/source/include", "{KIT}/include/softfloat"),
    (f"{root}/ps5/src", "{KIT}/include/src"),
    (f"{root}/gamedata/generated", "{GEN}"),
    (f"{sdk}/toolchain/../target", "{KIT}/sdk/target"),
]
before = ["--start-no-unused-arguments", "-target", "x86_64-sie-ps5", "-fvisibility-nodllstorageclass=default",
          "-isysroot", "{KIT}/sdk", "-stdlib++-isystem", "{KIT}/sdk/target/include/c++/v1",
          "-fno-stack-protector", "-fno-plt", "-femulated-tls", "-fdenormal-fp-math=ieee", "-frtti", "-fexceptions",
          "--end-no-unused-arguments"]
after = ["--start-no-unused-arguments", "--sysroot", "{KIT}/sdk", "--end-no-unused-arguments"]
names = {"game_module": "game", "home_module": "home", "dsp_module": "dsp"}
target = None
for line in open(commands):
    line = line.strip()
    if line.startswith("#"):
        target = line[1:]
        continue
    if not line:
        continue
    args = shlex.split(line)
    args = args[args.index(next(a for a in args if a.endswith("prospero-clang++"))) + 1:]
    flags, skip = [], 0
    for i, a in enumerate(args):
        if skip:
            skip -= 1
            continue
        if a in ("-MD",):
            continue
        if a in ("-MT", "-MF", "-o", "-c"):
            skip = 1
            continue
        if a == "-Winvalid-pch":
            continue
        if a == "-Xclang" and i + 1 < len(args) and args[i + 1] in ("-include-pch", "-include"):
            skip = 3  # -Xclang -include-pch -Xclang FILE: the builder makes its own
            continue
        for old, new in paths:
            a = a.replace(old, new)
        if root in a or "/home/" in a:
            sys.exit(f"make-kit.sh: a path the kit does not have in {target}'s flags: {a}")
        flags.append(a)
    open(f"{out}/{names[target]}.txt", "w").write("\n".join(before + flags + after) + "\n")
    print(f"flags/{names[target]}.txt: {len(flags)} flags")
EOF

# The link
cp -R "$root/build/kit/link" "$kit/link"
rm -f "$kit/link/files"/*/'.DS_Store'

# The title's own files (the eboot is the player's)
for item in sce_sys sce_module licenses; do
    cp -R "$root/dist/PPSA99315/$item" "$kit/title/$item"
done
cp "$root/.deps/llvm/LICENSE.TXT" "$kit/title/licenses/LLVM.txt"
find "$kit" -name '.DS_Store' -delete
echo "$kit ($(du -sh "$kit" | cut -f1))"
