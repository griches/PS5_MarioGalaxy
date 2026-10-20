#!/usr/bin/env bash
# Super Mario Galaxy for the PlayStation 5 - Nebula's presentation passes
# (post_process.hlsl with the vendored SMAA and FSR 1 sources) compiled to DXBC
# bytecode headers, as Nebula's CMake does with fxc, but with vkd3d's HLSL
# compiler (built for the VM by ps5/tools/vkd3d-host.sh).
#
#   post-shaders.sh NEBULA_DIR VKD3D_COMPILER OUT_DIR
#
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
nebula=$1 compiler=$2 out=$3
mkdir -p "$out"
post=$nebula/runtime/src/gx/post_process.hlsl
smaa=$nebula/third_party/smaa/SMAA.hlsl
ffx_a=$nebula/third_party/fidelityfx-fsr1/ffx_a.h
fsr1=$nebula/third_party/fidelityfx-fsr1/ffx_fsr1.h

# name pass preset library stage entry
shader() {
    local name=$1 pass=$2 preset=$3 library=$4 stage=$5 entry=$6
    local src=$out/$name.hlsl
    {
        echo "#define $pass 1"
        [[ $preset != - ]] && echo "#define $preset 1"
        echo "#define POST_SECTION 1"; cat "$post"; echo
        if [[ $library == smaa ]]; then cat "$smaa"; echo
        elif [[ $library == fsr ]]; then
            cat "$ffx_a"; echo; echo "#undef POST_SECTION"; echo "#define POST_SECTION 2"
            cat "$post"; echo; cat "$fsr1"; echo
        fi
        echo "#undef POST_SECTION"; echo "#define POST_SECTION 3"; cat "$post"; echo
    } > "$src.tmp"
    if [[ -f $out/$name.h ]] && cmp -s "$src.tmp" "$src"; then rm "$src.tmp"; return; fi
    mv "$src.tmp" "$src"
    "$compiler" -x hlsl -p "$stage" -b dxbc-tpf -e "$entry" -o "$out/$name.dxbc" "$src"
    python3 - "$out/$name.dxbc" "$out/$name.h" "g_$name" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
rows = [', '.join(f'0x{b:02x}' for b in data[i:i + 16]) for i in range(0, len(data), 16)]
with open(sys.argv[2], 'w') as f:
    f.write(f'const BYTE {sys.argv[3]}[] =\n{{\n    ' + ',\n    '.join(rows) + '\n};\n')
PY
}

shader post_fullscreen_vs PASS_RESAMPLE - none vs_5_0 VSMain
shader post_resample_ps PASS_RESAMPLE - none ps_5_0 PSMain
shader post_fxaa_ps PASS_FXAA - none ps_5_0 PSMain
shader post_fsr_easu_ps PASS_FSR_EASU - fsr ps_5_0 PSMain
shader post_fsr_rcas_ps PASS_FSR_RCAS - fsr ps_5_0 PSMain
shader post_smaa_edges_vs PASS_SMAA_EDGES SMAA_PRESET_HIGH smaa vs_5_0 VSMain
shader post_smaa_blend_vs PASS_SMAA_BLEND SMAA_PRESET_HIGH smaa vs_5_0 VSMain
shader post_smaa_blend_ps PASS_SMAA_BLEND SMAA_PRESET_HIGH smaa ps_5_0 PSMain
for preset in LOW MEDIUM HIGH ULTRA; do
    suffix=${preset,,}
    shader post_smaa_edges_ps_$suffix PASS_SMAA_EDGES SMAA_PRESET_$preset smaa ps_5_0 PSMain
    shader post_smaa_weights_vs_$suffix PASS_SMAA_WEIGHTS SMAA_PRESET_$preset smaa vs_5_0 VSMain
    shader post_smaa_weights_ps_$suffix PASS_SMAA_WEIGHTS SMAA_PRESET_$preset smaa ps_5_0 PSMain
done
