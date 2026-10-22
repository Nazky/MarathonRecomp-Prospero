#!/usr/bin/env bash
# Generate MarathonRecomp's PPC and Xenos shader sources from the matching
# Sonic the Hedgehog (2006) retail inputs. This script does not download game data.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PRIVATE_DIR="$ROOT/MarathonRecompLib/private"
TOOLS_BIN="$ROOT/tools/bin"
CONFIG="$ROOT/MarathonRecompLib/config/Marathon.toml"
SOURCE_ROOT="${1:-${MARATHON_GAME_DIR:-}}"

if [[ -z "$SOURCE_ROOT" ]]; then
    for candidate in "$ROOT/ressources" "$ROOT/resources" "$ROOT/game_assets"; do
        if [[ -d "$candidate/game" ]]; then
            SOURCE_ROOT="$candidate"
            break
        fi
    done
fi
GAME_FILES_ROOT=""
if [[ -n "$SOURCE_ROOT" ]]; then
    if [[ -d "$SOURCE_ROOT/game" ]]; then
        GAME_FILES_ROOT="$SOURCE_ROOT/game"
    else
        GAME_FILES_ROOT="$SOURCE_ROOT"
    fi
fi

mkdir -p "$PRIVATE_DIR" "$ROOT/MarathonRecompLib/ppc" "$ROOT/MarathonRecompLib/shader"

find_case_insensitive() {
    local dir="$1" name="$2"
    [[ -d "$dir" ]] || return 1
    find "$dir" -maxdepth 5 -type f -iname "$name" -print -quit 2>/dev/null
}

if [[ -n "$GAME_FILES_ROOT" && -d "$GAME_FILES_ROOT" ]]; then
    echo "==> [recomp-xex] Reading private game inputs from: $SOURCE_ROOT"
    for name in default.xex shader.arc shader_lt.arc; do
        input=$(find_case_insensitive "$GAME_FILES_ROOT" "$name" || true)
        if [[ -n "$input" && -f "$input" ]]; then
            destination="$PRIVATE_DIR/$name"
            if [[ ! -f "$destination" || "$input" -nt "$destination" ]]; then
                echo "    Copying $input -> $destination"
                cp -f "$input" "$destination"
                case "$name" in
                    default.xex) rm -f "$PRIVATE_DIR/.recomp_ppc_done" ;;
                    *.arc) rm -f "$PRIVATE_DIR/.recomp_shader_done" ;;
                esac
            fi
        fi
    done
fi

missing=()
for name in default.xex shader.arc shader_lt.arc; do
    [[ -s "$PRIVATE_DIR/$name" ]] || missing+=("$name")
done
if (( ${#missing[@]} > 0 )); then
    printf 'ERROR: missing MarathonRecomp private input(s): %s\n' "${missing[*]}" >&2
    echo "Place the matching Sonic 2006 default.xex, game/shader.arc, and game/shader_lt.arc under" >&2
    echo "MarathonRecompLib/private/ or under <game-root>/game/ (set MARATHON_GAME_DIR)." >&2
    exit 2
fi

bash "$ROOT/tools/build-host-tools.sh"

if [[ -n "${DXC_ROOT:-}" && -f "$DXC_ROOT/lib/x64/libdxcompiler.so" ]]; then
    DXC_LIB_DIR="$DXC_ROOT/lib/x64"
elif [[ -f "$ROOT/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
    DXC_LIB_DIR="$ROOT/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64"
elif [[ -f "$ROOT/.deps/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
    DXC_LIB_DIR="$ROOT/.deps/dxc-bin/lib/x64"
elif [[ -f "/opt/ps5-cache/refs/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
    DXC_LIB_DIR="/opt/ps5-cache/refs/dxc-bin/lib/x64"
else
    DXC_LIB_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/MarathonRecomp/dxc-bin/lib/x64"
fi
export LD_LIBRARY_PATH="$DXC_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

ppc_stamp="$PRIVATE_DIR/.recomp_ppc_done"
if [[ ! -f "$ppc_stamp" || "$PRIVATE_DIR/default.xex" -nt "$ppc_stamp" || \
      "$TOOLS_BIN/XenonRecomp" -nt "$ppc_stamp" || \
      "$CONFIG" -nt "$ppc_stamp" || \
      "$ROOT/tools/XenonRecomp/XenonUtils/ppc_context.h" -nt "$ppc_stamp" || \
      ! -s "$ROOT/MarathonRecompLib/ppc/ppc_recomp.0.cpp" || \
      ! -s "$ROOT/MarathonRecompLib/ppc/ppc_func_mapping.cpp" ]]; then
    echo "==> [recomp-xex] Running XenonRecomp on the supplied Sonic 2006 XEX..."
    "$TOOLS_BIN/XenonRecomp" "$CONFIG" "$ROOT/tools/XenonRecomp/XenonUtils/ppc_context.h"
    touch "$ppc_stamp"
else
    echo "==> [recomp-xex] Marathon PPC sources are up to date."
fi

shader_dir="$PRIVATE_DIR/shader"
shader_stamp="$PRIVATE_DIR/.recomp_shader_done"
if [[ ! -f "$shader_stamp" || "$PRIVATE_DIR/shader.arc" -nt "$shader_stamp" || \
      "$PRIVATE_DIR/shader_lt.arc" -nt "$shader_stamp" || \
      ! -s "$ROOT/MarathonRecompLib/shader/shader_cache.cpp" ]]; then
    echo "==> [recomp-xex] Extracting the two shader archives..."
    rm -rf "$shader_dir"
    mkdir -p "$shader_dir"
    "$TOOLS_BIN/u8extract" "$PRIVATE_DIR/shader.arc" "$shader_dir"
    "$TOOLS_BIN/u8extract" "$PRIVATE_DIR/shader_lt.arc" "$shader_dir"
    echo "==> [recomp-xex] Running XenosRecomp to generate shader_cache.cpp..."
    "$TOOLS_BIN/XenosRecomp" \
        "$shader_dir" \
        "$ROOT/MarathonRecompLib/shader/shader_cache.cpp" \
        "$ROOT/tools/XenosRecomp/XenosRecomp/shader_common.h"
    touch "$shader_stamp"
else
    echo "==> [recomp-xex] Marathon shader cache is up to date."
fi

echo "==> [recomp-xex] Marathon PPC and shader-source generation complete."
