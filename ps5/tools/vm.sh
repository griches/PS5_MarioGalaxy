#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - run a build tool inside the Linux VM, from macOS.
#
#   ps5/tools/vm.sh build            ps5/tools/build.sh (the game)
#   ps5/tools/vm.sh deploy           ps5/tools/deploy.sh (FTP to the console)
#   ps5/tools/vm.sh run              ps5/tools/run.sh (launch and follow klog)
#   ps5/tools/vm.sh package-release VERSION   ps5/tools/package-release.sh (the release archive)
#
# The stack (PS5_Vulkan with its RADV archive, PS5_Mesa, PS5_PayloadSDK) lives in
# the Lima VM "ps5" under ~/ps5, built with LLVM 22 (~/ps5/env.sh); this folder is
# mounted there at the same path. The game's build tree goes on the VM's own disk
# (~/ps5/build/PS5_MarioGalaxy-game): the shared folder is several times slower for
# three thousand objects. The console's address is PS5_HOST in the VM's
# ~/ps5/PS5_Vulkan/.env.
#
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
tool=${1:?usage: vm.sh build|deploy|run|package-release [arguments]}
shift
exec limactl shell ps5 -- bash -lc '. ~/ps5/env.sh; export PS5_VULKAN_DIR=~/ps5/PS5_Vulkan PS5_PAYLOAD_SDK_FORK=~/ps5/PS5_PayloadSDK PS5_BUILD_DIR=${PS5_BUILD_DIR:-$HOME/ps5/build/PS5_MarioGalaxy-game}; cd "$1"; tool=$2; shift 2; exec bash "ps5/tools/$tool.sh" "$@"' _ "$root" "$tool" "$@"
