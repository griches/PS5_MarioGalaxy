#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - launch the title on the console and follow its log.
#
#   ps5/tools/run.sh                 launch, and capture klog until the title ends
#   RUN_TIMEOUT=120 ps5/tools/run.sh give up after that many seconds
#
# PS5_Vulkan's run tool checks that the console runs nothing, launches the title
# through the control payload (ps5vkctl), captures klog until the title's
# "ends" line, a crash record or the title's exit, and symbolises a crash. The
# log is kept in klog/<run>/klog.log. 
#
# SPDX-License-Identifier: MIT

set -euo pipefail
ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$ps5/sce_sys/param.json")
# klog's prefix: the title's name, as a regular expression
prefix=$(python3 -c 'import json,re,sys; print(re.escape("[" + json.load(open(sys.argv[1]))["localizedParameters"]["en-US"]["titleName"] + "]"))' "$ps5/sce_sys/param.json")
run_dir="$root/klog/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run_dir"
exec python3 "$vulkan/tools/run-title.py" "$title_id" \
    --until "$prefix ends" --timeout "${RUN_TIMEOUT:-86400}" \
    --echo "$prefix|fatal|FAILED|Fatal" \
    --elf "$root/build/last-title.elf" --output "$run_dir/klog.log"
