#!/usr/bin/env bash
# PS5 Vulkan Template - upload dist/<TITLE_ID>/ to the console.
#
#   ps5/tools/deploy.sh [--all]
#
# PS5_Vulkan's deploy tool uploads what changed (eboot.bin always), over the
# console's FTP server, with the address from PS5_Vulkan's .env.
#
# Copyright (C) 2026 Mihawk
# SPDX-License-Identifier: MIT
set -euo pipefail
ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$ps5/sce_sys/param.json")
[[ -d $root/dist/$title_id ]] || { echo "no dist/$title_id: run ps5/tools/build.sh first" >&2; exit 2; }
# The art and tile music always go: every 3840x2160 pic0/pic1.dds has the same
# size, which the tool would take for unchanged, and so does every 30 s snd0.at9.
# The home screen shows the copy in /user/appmeta/<TITLE_ID> (ps5/tools/console.sh
# deploy refreshes that too).
# res/ (12 MB) always goes in full too: its images change without changing size.
always=(--always eboot.bin --always sce_sys/icon0.png --always sce_sys/pic0.dds --always sce_sys/pic1.dds
        --always sce_sys/snd0.at9)
while IFS= read -r file; do always+=(--always "${file#"$root/dist/$title_id/"}"); done \
    < <(find "$root/dist/$title_id/res" -type f)
exec python3 "$vulkan/tools/deploy-title-folder.py" "${always[@]}" "$root/dist/$title_id" "$@"
