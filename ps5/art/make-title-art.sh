#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - the title's art, from the pictures in this folder.
#
#   ps5/art/make-title-art.sh
#
#   icon0-source.png  ->  ps5/sce_sys/icon0.png   the home-screen tile, 512x512
#   pic-source.png    ->  ps5/sce_sys/pic0.dds    the background behind the title, 3840x2160
#                     ->  ps5/sce_sys/pic1.dds    the splash while it starts
#
# Both pictures are the port's own, made for it with ChatGPT in the look of the
# game's cover (a hero leaping through a starfield over a glowing planet rim,
# small grassy planetoids): an original young explorer in a red jacket and blue
# trousers reaching for a golden star. Nothing in them is from the game, and
# they carry no logo or name. The background keeps its subject in the lower
# right, clear of what the home screen covers (the top quarter: menu bar and
# tiles; the left third: title, Play Game, Friends Who Play).
# Needs Python with Pillow.
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
art=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ps5=$(dirname "$art")
python3 - "$art/icon0-source.png" "$ps5/sce_sys/icon0.png" <<'PY'
import sys
from PIL import Image
Image.open(sys.argv[1]).convert("RGB").resize((512, 512), Image.LANCZOS).save(sys.argv[2])
PY
python3 "$ps5/tools/png-to-title-dds.py" "$art/pic-source.png" "$ps5/sce_sys/pic0.dds"
cp "$ps5/sce_sys/pic0.dds" "$ps5/sce_sys/pic1.dds"
ls -l "$ps5/sce_sys"
