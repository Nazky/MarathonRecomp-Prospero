#!/usr/bin/env bash
# Compile Marathon's renderer HLSL into embedded SPIR-V C headers for the PS5 build.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
HLSL_DIR="$ROOT/MarathonRecomp/gpu/shader/hlsl"

DXC_DIR="${DXC_ROOT:-}"
if [[ -z "$DXC_DIR" ]]; then
    if [[ -f "$ROOT/tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc-linux" && -x "$ROOT/tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc-linux" ]]; then
        DXC_DIR="$ROOT/tools/XenosRecomp/thirdparty/dxc-bin"
    elif [[ -f "$ROOT/.deps/dxc-bin/bin/x64/dxc-linux" && -x "$ROOT/.deps/dxc-bin/bin/x64/dxc-linux" ]]; then
        DXC_DIR="$ROOT/.deps/dxc-bin"
    elif [[ -f "/opt/ps5-cache/refs/dxc-bin/bin/x64/dxc-linux" && -x "/opt/ps5-cache/refs/dxc-bin/bin/x64/dxc-linux" ]]; then
        DXC_DIR="/opt/ps5-cache/refs/dxc-bin"
    else
        DXC_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/MarathonRecomp/dxc-bin"
    fi
fi

SHADER_NAMES=(
    blend_color_alpha_ps conditional_survey_ps
    copy_vs copy_color_ps copy_depth_ps csd_filter_ps csd_no_tex_vs csd_vs
    enhanced_burnout_blur_vs enhanced_burnout_blur_ps movie_ps
    gaussian_blur_3x3 gaussian_blur_5x5 gaussian_blur_7x7 gaussian_blur_9x9
    gamma_correction_ps imgui_ps imgui_vs
    resolve_msaa_color_2x resolve_msaa_color_4x resolve_msaa_color_8x
    resolve_msaa_depth_2x resolve_msaa_depth_4x resolve_msaa_depth_8x
)
VERTEX_SHADERS=(copy_vs csd_no_tex_vs csd_vs enhanced_burnout_blur_vs imgui_vs)

needs_compile=0
for name in "${SHADER_NAMES[@]}"; do
    source="$HLSL_DIR/$name.hlsl"
    output="$source.spirv.h"
    if [[ ! -s "$source" ]]; then
        echo "ERROR: missing Marathon HLSL shader: $source" >&2
        exit 2
    fi
    if [[ ! -s "$output" || "$source" -nt "$output" ]]; then
        needs_compile=1
    fi
done

if (( needs_compile == 0 )); then
    echo "==> [shaders] Marathon SPIR-V headers are up to date."
    exit 0
fi

DXC_BIN=""
if [[ -n "${DXC:-}" ]]; then
    if [[ -f "$DXC" && -x "$DXC" ]]; then
        DXC_BIN="$DXC"
    else
        DXC_BIN="$(command -v "$DXC" 2>/dev/null || true)"
    fi
fi
if [[ -z "$DXC_BIN" && -f "$DXC_DIR/bin/x64/dxc-linux" && -x "$DXC_DIR/bin/x64/dxc-linux" ]]; then
    DXC_BIN="$DXC_DIR/bin/x64/dxc-linux"
fi
if [[ -z "$DXC_BIN" ]]; then
    DXC_BIN="$(command -v dxc 2>/dev/null || true)"
fi
if [[ -z "$DXC_BIN" || ! -f "$DXC_BIN" || ! -x "$DXC_BIN" ]]; then
    echo "ERROR: DXC compiler not found. Set DXC to the executable or DXC_ROOT to the pinned dxc-bin directory." >&2
    exit 2
fi

if [[ "$DXC_BIN" == "$DXC_DIR/"* && -d "$DXC_DIR/lib/x64" ]]; then
    export LD_LIBRARY_PATH="$DXC_DIR/lib/x64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

compile_shader() {
    local name="$1" target="$2" vertex="$3"
    local source="$HLSL_DIR/$name.hlsl"
    local output="$source.spirv.h"

    if [[ -s "$output" && ! "$source" -nt "$output" ]]; then
        return
    fi

    local args=(
        -T "$target"
        -HV 2021
        -all-resources-bound
        -spirv
        -fvk-use-dx-layout
    )
    if [[ "$vertex" == "1" ]]; then
        args+=(-fvk-invert-y)
    fi
    args+=(
        -DMARATHON_RECOMP
        -I "$HLSL_DIR"
        -E shaderMain
        -Fh "$output"
        "$source"
        -Vn "g_${name}_spirv"
    )

    echo "    DXC $name (${target})"
    "$DXC_BIN" "${args[@]}"
}

for name in "${SHADER_NAMES[@]}"; do
    target=ps_6_0
    vertex=0
    for vertex_name in "${VERTEX_SHADERS[@]}"; do
        if [[ "$name" == "$vertex_name" ]]; then
            target=vs_6_0
            vertex=1
            break
        fi
    done
    compile_shader "$name" "$target" "$vertex"
done

echo "==> [shaders] Generated Marathon SPIR-V headers."
