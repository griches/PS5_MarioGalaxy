#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - deploy and run from macOS.
#
#   ps5/tools/console.sh deploy              dist/<TITLE_ID>/ to the console (FTP)
#   ps5/tools/console.sh game-files          gamedata/content and the boot image to the title folder
#   ps5/tools/console.sh textures            the HD pack's DDS files (gamedata/textures, Dolphin's
#                                            RMG/ layout) to textures/ (files already there at the
#                                            same size are skipped)
#   ps5/tools/console.sh run [SECONDS]       launch it and follow klog (default 120 s)
#   ps5/tools/console.sh env [KEY=VALUE...]  write the title's env.txt (no arguments: remove it)
#   ps5/tools/console.sh quit                close the title
#
# Uses the Mac's PS5_Vulkan checkout (../PS5_Vulkan, PS5_HOST in its .env) and
# the console's ftpsrv (2121), klogsrv (3232) and PS5_Vulkan's ps5vkctl (9111).
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$ps5/sce_sys/param.json")
host=${PS5_HOST:-$(sed -n 's/^PS5_HOST=//p' "$vulkan/.env")}
ftp="ftp://$host:2121/data/homebrew/$title_id"

case ${1:?usage: console.sh deploy|game-files|textures|run|env|quit} in
deploy)
    # The art always goes: every 3840x2160 pic0/pic1.dds has the same size, and
    # the tool skips a file whose size matches. The home screen shows
    # /user/appmeta/<TITLE_ID>, copied at registration, so that one is refreshed too.
    art=(sce_sys/icon0.png sce_sys/pic0.dds sce_sys/pic1.dds)
    always=(--always eboot.bin)
    for file in "${art[@]}"; do always+=(--always "$file"); done
    python3 "$vulkan/tools/deploy-title-folder.py" "${always[@]}" "$root/dist/$title_id"
    for file in "${art[@]}"; do
        curl -s -T "$root/dist/$title_id/$file" "ftp://$host:2121/user/appmeta/$title_id/${file#sce_sys/}" 2>/dev/null ||
            true
    done
    ;;
game-files)
    content="$root/gamedata/content"
    curl -s --ftp-create-dirs -T "$root/gamedata/RMGE01_boot_image.bin" "$ftp/RMGE01_boot_image.bin"
    (cd "$content" && find . -type f ! -name game.pak | sed 's|^\./||') | while read -r file; do
        curl -s --ftp-create-dirs -T "$content/$file" "$ftp/content/$file"
    done
    curl --ftp-create-dirs -T "$content/game.pak" "$ftp/content/game.pak.part"
    curl -s -Q "RNFR /data/homebrew/$title_id/content/game.pak.part" \
        -Q "RNTO /data/homebrew/$title_id/content/game.pak" "ftp://$host:2121/" > /dev/null
    echo "content/ and RMGE01_boot_image.bin sent"
    ;;
run)
    seconds=${2:-30}
    run_dir="$root/klog/$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$run_dir"
    python3 "$vulkan/tools/run-title.py" "$title_id" \
        --until 'exit status|fatal signal|runtime returned|\[memory\] guard|killing coredump|APP_SUSPENDING|Sending signal' --timeout "$seconds" \
        --echo '^\[Super Mario Galaxy\]|fatal|Sending signal|coredump' \
        --elf "$root/build/last-title.elf" --output "$run_dir/klog.log" || true
    curl -s -o "$run_dir/stderr.log" "$ftp/userdata/stderr.log" || true
    echo "klog: $run_dir/klog.log"
    echo "stderr: $run_dir/stderr.log ($(wc -l < "$run_dir/stderr.log" 2>/dev/null || echo 0) lines)"
    ;;
env)
    shift
    if [[ $# -eq 0 ]]; then
        curl -s -Q "DELE /data/homebrew/$title_id/env.txt" "ftp://$host:2121/" > /dev/null || true
        echo "env.txt removed"
    else
        printf '%s\n' "$@" | curl -s -T - "$ftp/env.txt"
        printf 'env.txt: %s\n' "$@"
    fi
    ;;
textures)
    put() {
        local file=$1 rel=${1#"$root/gamedata/textures/"}
        [[ $rel == */RMG/* ]] && rel=RMG/${rel#*/RMG/}
        local url="$ftp/textures/${rel// /%20}" size have
        size=$(stat -f %z "$file")
        have=$(curl -s -I "$url" | tr -d '\r' | awk '/Content-Length/ {print $2}')
        [[ $have == "$size" ]] && return
        for _ in 1 2 3; do curl -s --ftp-create-dirs -T "$file" "$url" && { echo "  $rel"; return; }; done
        echo "FAILED $rel" >&2
    }
    export -f put; export root ftp
    find "$root/gamedata/textures" -name '*.dds' -print0 | xargs -0 -n1 -P3 bash -c 'put "$0"'
    ;;
quit)
    printf 'kill %s\n' "$title_id" | nc -w 5 "$host" 9111
    ;;
*)
    echo "usage: console.sh deploy|game-files|textures|run|env|quit" >&2
    exit 2
    ;;
esac
