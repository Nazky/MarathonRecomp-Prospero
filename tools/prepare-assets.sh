#!/usr/bin/env bash
# MarathonRecomp-Prospero - Native Linux PS5 presentation asset converter.
#
# Normalizes the launcher icon and prepares retained presentation sources:
#   - icon0.png : 512x512 PNG
#   - pic0.dds  : 3840x2160 BC7_UNORM DX10 DDS (selection background)
#   - pic1.dds  : 3840x2160 BC7_UNORM DX10 DDS (launch splash screen)
#   - snd0.at9  : looped 48 kHz ATRAC9 RIFF/WAVE soundtrack
#
# Automatic mode (called by `make` and `tools/build.sh`):
#   Converts icon0.source.*, pic0.source.*, and pic1.source.* when present.
#   Audio is auto-selected from snd0.wav and supported snd0.source.* files.
#   If only one background source is present, it fills both presentation slots.
#   No placeholder assets are created when sources are absent.
#
# Explicit conversion mode:
#   bash tools/prepare-assets.sh [--icon PATH] [--background PATH]
#       [--selection-background PATH] [--launch-background PATH]
#       [--audio PATH] [--audio-duration SECONDS] [--force]

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SCE_SYS="$ROOT/sce_sys"
TOOLS_BIN="$ROOT/tools/bin"
CONVERTER="$TOOLS_BIN/png_to_bc7_dds"
AT9_ENCODER="$TOOLS_BIN/wav_to_at9"

ICON_IN=""
ICON_EXPLICIT=0
BG_IN=""
PIC0_IN=""
PIC1_IN=""
AUDIO_IN=""
AUDIO_DURATION="174"
AT9_TOOL="${PS4_AT9TOOL:-}"
# Recipe metadata only: the upstream encoder path applies no extra fixed gain.
AUDIO_GAIN_DB="0dB"
AUDIO_RECIPE_VERSION="8"
AUDIO_RECIPE_STAMP="$ROOT/build/asset-state/snd0.at9.recipe"
FORCE_SYNC=0

while (($# > 0)); do
    case "$1" in
        --icon)
            ICON_IN="${2:-}"; ICON_EXPLICIT=1; shift 2 ;;
        --background|--bg)
            BG_IN="${2:-}"; shift 2 ;;
        --selection-background|--pic0)
            PIC0_IN="${2:-}"; shift 2 ;;
        --launch-background|--splash|--pic1)
            PIC1_IN="${2:-}"; shift 2 ;;
        --audio|--music|--snd0)
            AUDIO_IN="${2:-}"; shift 2 ;;
        --audio-duration)
            AUDIO_DURATION="${2:-174}"; shift 2 ;;
        --at9-tool)
            AT9_TOOL="${2:-}"; shift 2 ;;
        --force|-f)
            FORCE_SYNC=1; shift ;;
        --help|-h)
            sed -n '2,20p' "${BASH_SOURCE[0]}"
            exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            exit 2 ;;
    esac
done

audio_recipe_for() {
    local src="$1"
    local source_hash converter_hash pipeline_hash
    source_hash="$(sha256sum "$src")"
    source_hash="${source_hash%% *}"
    converter_hash="$(sha256sum "$ROOT/tools/wav_to_at9/wav_to_at9.cpp")"
    converter_hash="${converter_hash%% *}"
    pipeline_hash="$(sha256sum "$ROOT/tools/prepare-assets.sh")"
    pipeline_hash="${pipeline_hash%% *}"
    printf 'version=%s;gain=%s;source=%s;converter=%s;pipeline=%s\n' \
        "$AUDIO_RECIPE_VERSION" "$AUDIO_GAIN_DB" "$source_hash" "$converter_hash" "$pipeline_hash"
}

audio_needs_sync() {
    local src="$1"
    local dst="$SCE_SYS/snd0.at9"
    local expected_recipe output_hash expected_stamp current_stamp

    if [[ "$FORCE_SYNC" == "1" || ! -f "$dst" || "$src" -nt "$dst" ]]; then
        return 0
    fi

    expected_recipe="$(audio_recipe_for "$src")"
    output_hash="$(sha256sum "$dst")"
    output_hash="${output_hash%% *}"
    expected_stamp="${expected_recipe};output=${output_hash}"
    [[ ! -f "$AUDIO_RECIPE_STAMP" ]] && return 0
    current_stamp="$(<"$AUDIO_RECIPE_STAMP")"
    [[ "$current_stamp" != "$expected_stamp" ]]
}

select_auto_audio_source() {
    # Prefer the newest explicitly named audio source. Retain the historical
    # snd0.wav path, and also recognize conventional snd0.source.* assets.
    local best="" candidate extension
    for candidate in "$SCE_SYS/snd0.wav" "$SCE_SYS"/snd0.source.*; do
        [[ -f "$candidate" ]] || continue
        extension="${candidate##*.}"
        extension="${extension,,}"
        case "$extension" in
            wav|mp3|ogg|flac|m4a|aac|opus|wma|at9) ;;
            *) continue ;;
        esac
        if [[ -z "$best" || "$candidate" -nt "$best" ]]; then
            best="$candidate"
        fi
    done
    if [[ -n "$best" ]]; then
        printf '%s' "$best"
    fi
}

select_auto_image_source() {
    local stem="$1" best="" candidate extension
    for candidate in "$SCE_SYS/${stem}.source."*; do
        [[ -f "$candidate" ]] || continue
        extension="${candidate##*.}"
        extension="${extension,,}"
        case "$extension" in
            png|jpg|jpeg|webp|bmp|tga) ;;
            *) continue ;;
        esac
        if [[ -z "$best" || "$candidate" -nt "$best" ]]; then
            best="$candidate"
        fi
    done
    if [[ -n "$best" ]]; then
        printf '%s' "$best"
    fi
}

select_auto_icon_source() {
    local best="$(select_auto_image_source icon0)" candidate
    for candidate in "$SCE_SYS"/icon0.jpg "$SCE_SYS"/icon0.jpeg \
                     "$SCE_SYS"/icon0.webp "$SCE_SYS"/icon0.bmp "$SCE_SYS"/icon0.tga; do
        [[ -f "$candidate" ]] || continue
        if [[ -z "$best" || "$candidate" -nt "$best" ]]; then
            best="$candidate"
        fi
    done
    if [[ -n "$best" ]]; then
        printf '%s' "$best"
    fi
}

image_needs_sync() {
    local src="$1"; shift
    local dst
    [[ "$FORCE_SYNC" == "1" ]] && return 0
    [[ ! -x "$CONVERTER" || "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" -nt "$CONVERTER" ]] && return 0
    for dst in "$@"; do
        [[ -s "$dst" ]] || return 0
        [[ "$src" -nt "$dst" || "$CONVERTER" -nt "$dst" || "$ROOT/tools/prepare-assets.sh" -nt "$dst" ]] && return 0
    done
    return 1
}

icon_is_valid_png() {
    local path="$1"
    python3 - "$path" <<'PY'
from pathlib import Path
import struct
import sys
try:
    data = Path(sys.argv[1]).read_bytes()
except OSError:
    raise SystemExit(1)
valid = (len(data) >= 24 and data[:8] == b"\x89PNG\r\n\x1a\n"
         and struct.unpack(">II", data[16:24]) == (512, 512))
raise SystemExit(0 if valid else 1)
PY
}

record_audio_recipe() {
    local src="$1"
    local recipe output_hash tmp
    recipe="$(audio_recipe_for "$src")"
    output_hash="$(sha256sum "$SCE_SYS/snd0.at9")"
    output_hash="${output_hash%% *}"
    recipe="${recipe};output=${output_hash}"
    mkdir -p "${AUDIO_RECIPE_STAMP%/*}"
    tmp="$(mktemp "${AUDIO_RECIPE_STAMP}.tmp.XXXXXX")"
    printf '%s' "$recipe" > "$tmp"
    mv -f "$tmp" "$AUDIO_RECIPE_STAMP"
}

ensure_converter() {
    if [[ ! -x "$CONVERTER" || "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" -nt "$CONVERTER" ]]; then
        mkdir -p "$TOOLS_BIN"
        local cxx="${HOST_CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
        echo "==> [assets] Building native Linux BC7 DDS converter ($CONVERTER)..." >&2
        "$cxx" -std=c++20 -O3 -fopenmp \
            -I"$ROOT/thirdparty/stb" \
            "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" \
            -o "$CONVERTER" 2>/dev/null || \
        "$cxx" -std=c++20 -O3 \
            -I"$ROOT/thirdparty/stb" \
            "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" \
            -o "$CONVERTER"
    fi
}

ensure_at9_encoder() {
    if [[ ! -x "$AT9_ENCODER" || "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" -nt "$AT9_ENCODER" ]]; then
        mkdir -p "$TOOLS_BIN"
        local cxx="${HOST_CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
        local cc="${HOST_CC:-$(command -v gcc || command -v clang-18 || command -v clang)}"
        local build_at9="$ROOT/build/host-at9"
        echo "==> [assets] Building native Linux ATRAC9 encoder ($AT9_ENCODER)..." >&2
        mkdir -p "$build_at9"
        for csrc in "$ROOT"/thirdparty/libatrac9/src/*.c; do
            "$cc" -O3 -c "$csrc" -o "$build_at9/$(basename "${csrc%.c}").o"
        done
        "$cxx" -std=c++20 -O3 \
            -I"$ROOT/thirdparty/stb" \
            -I"$ROOT/thirdparty/libatrac9/src" \
            "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" \
            "$build_at9"/*.o -lm \
            -o "$AT9_ENCODER"
        rm -rf "$build_at9"
    fi
}

encode_audio_to_at9() {
    local src="$1"
    local dst="$2"
    case "${src,,}" in
        *.at9)
            if [[ "$(realpath "$src")" != "$(realpath -m "$dst")" ]]; then
                echo "==> [assets] Installing pre-encoded ATRAC9 $src -> $dst..."
                cp -f "$src" "$dst"
            fi
            ;;
        *.wav|*.mp3|*.ogg|*.flac)
            if [[ -n "$AT9_TOOL" ]]; then
                command -v ffmpeg >/dev/null 2>&1 || { echo "Error: ffmpeg is required when using --at9-tool." >&2; exit 1; }
                local tmp_wav
                tmp_wav="$(mktemp --suffix=.wav)"
                # Keep external encoders at the same neutral 0 dB gain as the native path.
                ffmpeg -y -v error -i "$src" -vn -sn -dn -t "$AUDIO_DURATION" -ar 48000 -ac 2 -c:a pcm_s16le "$tmp_wav"
                echo "==> [assets] Encoding $src -> $dst via $AT9_TOOL..."
                if [[ "$AT9_TOOL" == *.exe ]] && command -v wine >/dev/null 2>&1 && ! grep -qi microsoft /proc/version 2>/dev/null; then
                    wine "$AT9_TOOL" -e -br 144 -wholeloop "$tmp_wav" "$dst"
                else
                    "$AT9_TOOL" -e -br 144 -wholeloop "$tmp_wav" "$dst"
                fi
                rm -f "$tmp_wav"
            else
                ensure_at9_encoder
                echo "==> [assets] Encoding $src -> $dst (48 kHz stereo looped ATRAC9, max ${AUDIO_DURATION}s)..."
                if ! "$AT9_ENCODER" "$src" "$dst" "$AUDIO_DURATION" 2>/dev/null; then
                    if command -v ffmpeg >/dev/null 2>&1; then
                        local tmp_wav
                        tmp_wav="$(mktemp --suffix=.wav)"
                        ffmpeg -y -v error -i "$src" -vn -sn -dn -t "$AUDIO_DURATION" -ar 48000 -ac 2 -c:a pcm_s16le "$tmp_wav"
                        "$AT9_ENCODER" "$tmp_wav" "$dst" "$AUDIO_DURATION"
                        rm -f "$tmp_wav"
                    else
                        "$AT9_ENCODER" "$src" "$dst" "$AUDIO_DURATION"
                    fi
                fi
            fi
            ;;
        *.m4a|*.aac|*.opus|*.wma)
            command -v ffmpeg >/dev/null 2>&1 || {
                echo "Error: Converting ${src} requires ffmpeg on PATH (or provide .wav, .mp3, .ogg, .flac, or .at9)." >&2
                exit 1
            }
            local tmp_wav
            tmp_wav="$(mktemp --suffix=.wav)"
            echo "==> [assets] Decoding $src to 48 kHz stereo PCM via ffmpeg..."
            ffmpeg -y -v error -i "$src" -vn -sn -dn -t "$AUDIO_DURATION" -ar 48000 -ac 2 -c:a pcm_s16le "$tmp_wav"
            if [[ -n "$AT9_TOOL" ]]; then
                echo "==> [assets] Encoding $src -> $dst via $AT9_TOOL..."
                "$AT9_TOOL" -e -br 144 -wholeloop "$tmp_wav" "$dst"
            else
                ensure_at9_encoder
                echo "==> [assets] Encoding $src -> $dst (48 kHz stereo looped ATRAC9)..."
                "$AT9_ENCODER" "$tmp_wav" "$dst" "$AUDIO_DURATION"
            fi
            rm -f "$tmp_wav"
            ;;
        *)
            echo "Unsupported audio extension for $src (expected .wav, .mp3, .ogg, .flac, .m4a, .aac, or .at9)" >&2
            exit 1
            ;;
    esac
    record_audio_recipe "$src"
}

mkdir -p "$SCE_SYS"

# Explicit CLI inputs override the conventional retained-source filenames.
if [[ -n "$BG_IN" ]]; then
    PIC0_IN="$BG_IN"
    PIC1_IN="$BG_IN"
fi
if [[ -z "$PIC0_IN" ]]; then
    PIC0_IN="$(select_auto_image_source pic0)"
fi
if [[ -z "$PIC1_IN" ]]; then
    PIC1_IN="$(select_auto_image_source pic1)"
fi
if [[ -z "$ICON_IN" ]]; then
    icon_source="$(select_auto_icon_source)"
    if [[ -s "$SCE_SYS/icon0.png" ]]; then
        # Preserve an existing valid package icon unless a retained source is
        # newer. If the package-path file is malformed/wrong-sized, normalize
        # that file itself so a user's replacement is not overwritten by an
        # older retained source image.
        if ! icon_is_valid_png "$SCE_SYS/icon0.png"; then
            ICON_IN="$SCE_SYS/icon0.png"
        elif [[ -n "$icon_source" && "$icon_source" -nt "$SCE_SYS/icon0.png" ]]; then
            ICON_IN="$icon_source"
        fi
    else
        ICON_IN="$icon_source"
    fi
fi
if [[ -z "$ICON_IN" && ! -s "$SCE_SYS/icon0.png" ]]; then
    echo "ERROR: no launcher icon source found. Add sce_sys/icon0.source.{png,jpg} (or pass --icon PATH), or provide a valid sce_sys/icon0.png." >&2
    exit 2
fi
# Only the conventional retained .source.* files are auto-discovered. Loose
# package assets and arbitrary pic0/pic1 files are never treated as sources.

# 1. Icon: retain the source and normalize package output to a real 512x512 PNG.
if [[ -n "$ICON_IN" ]]; then
    [[ -f "$ICON_IN" ]] || { echo "Icon file not found: $ICON_IN" >&2; exit 1; }
    ensure_converter
    if [[ "$ICON_EXPLICIT" == "1" ]] || image_needs_sync "$ICON_IN" "$SCE_SYS/icon0.png" || ! icon_is_valid_png "$SCE_SYS/icon0.png"; then
        echo "==> [assets] Converting/resizing $ICON_IN -> $SCE_SYS/icon0.png (512x512 PNG)..."
        icon_tmp_dir="$(mktemp -d "$SCE_SYS/.icon0.XXXXXX")"
        icon_tmp_png="$icon_tmp_dir/icon0.png"
        if ! "$CONVERTER" "$ICON_IN" "$icon_tmp_png" 512 512; then
            rm -rf "$icon_tmp_dir"
            echo "ERROR: failed to convert launcher icon: $ICON_IN" >&2
            exit 1
        fi
        if ! icon_is_valid_png "$icon_tmp_png"; then
            rm -rf "$icon_tmp_dir"
            echo "ERROR: icon converter did not produce a valid 512x512 PNG." >&2
            exit 1
        fi
        mv -f "$icon_tmp_png" "$SCE_SYS/icon0.png"
        rmdir "$icon_tmp_dir"
    fi
fi

convert_background() {
    local src="$1" dst="$2" label="$3"
    [[ -f "$src" ]] || { echo "$label background not found: $src" >&2; exit 1; }
    ensure_converter
    if image_needs_sync "$src" "$dst"; then
        echo "==> [assets] Encoding $src -> $dst (3840x2160 BC7_UNORM DX10 DDS)..."
        "$CONVERTER" "$src" "$dst"
    fi
}

# 2. Background/splash: convert explicit inputs or conventional .source.* files.
# With no source input, remove stale DDS files so builds do not package old
# backgrounds after their retained source images have been removed.
if [[ -z "$PIC0_IN" && -z "$PIC1_IN" ]]; then
    rm -f "$SCE_SYS/pic0.dds" "$SCE_SYS/pic1.dds"
fi
if [[ -n "$PIC0_IN" ]]; then
    convert_background "$PIC0_IN" "$SCE_SYS/pic0.dds" "Selection"
fi
if [[ -n "$PIC1_IN" ]]; then
    convert_background "$PIC1_IN" "$SCE_SYS/pic1.dds" "Launch"
fi

# A lone source image serves both PS5 presentation slots. Keep the two DDS
# outputs identical so a stale companion from an older build cannot survive.
if [[ -n "$PIC0_IN" && -z "$PIC1_IN" && -s "$SCE_SYS/pic0.dds" ]]; then
    if ! cmp -s "$SCE_SYS/pic0.dds" "$SCE_SYS/pic1.dds"; then
        cp -f "$SCE_SYS/pic0.dds" "$SCE_SYS/pic1.dds"
    fi
elif [[ -n "$PIC1_IN" && -z "$PIC0_IN" && -s "$SCE_SYS/pic1.dds" ]]; then
    if ! cmp -s "$SCE_SYS/pic1.dds" "$SCE_SYS/pic0.dds"; then
        cp -f "$SCE_SYS/pic1.dds" "$SCE_SYS/pic0.dds"
    fi
fi

# 4. Explicit --audio OR auto-sync snd0.wav / supported snd0.source.* -> snd0.at9
if [[ -n "$AUDIO_IN" ]]; then
    [[ -f "$AUDIO_IN" ]] || { echo "Audio file not found: $AUDIO_IN" >&2; exit 1; }
    encode_audio_to_at9 "$AUDIO_IN" "$SCE_SYS/snd0.at9"
else
    audio_source="$(select_auto_audio_source)"
    if [[ -n "$audio_source" ]]; then
        echo "==> [assets] Auto-selected audio source: $audio_source"
        if audio_needs_sync "$audio_source"; then
            encode_audio_to_at9 "$audio_source" "$SCE_SYS/snd0.at9"
        fi
    else
        # Do not leave a placeholder/stale package soundtrack behind.
        rm -f "$SCE_SYS/snd0.at9" "$AUDIO_RECIPE_STAMP"
    fi
fi

bash "$ROOT/tools/validate-assets.sh" "$SCE_SYS"
