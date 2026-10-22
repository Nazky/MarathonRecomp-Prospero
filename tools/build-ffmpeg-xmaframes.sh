#!/usr/bin/env bash
# Build FFmpeg 7.0.1's XMA raw-frame decoder for Prospero. PacBrew's stock
# libavcodec does not include Marathon's custom AV_CODEC_ID_XMAFRAMES codec.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
FFMPEG_VERSION="7.0.1"
FFMPEG_SHA256="bce9eeb0f17ef8982390b1f37711a61b4290dc8c2a0c1a37b5857e85bfb0e4ff"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"
PATCH_FILE="$ROOT/tools/patches/ffmpeg-xmaframes-7.0.1.patch"
SDK_ROOT="${1:-${PS5_PAYLOAD_SDK:-}}"
DEPS_ROOT="${2:-${PS5_DEPS_ROOT:-}}"

if [[ -z "$SDK_ROOT" || ! -f "$SDK_ROOT/toolchain/prospero.sh" ]]; then
    echo "ERROR: PS5_PAYLOAD_SDK must point to an SDK containing toolchain/prospero.sh." >&2
    exit 2
fi
if [[ -z "$DEPS_ROOT" ]]; then
    echo "ERROR: PS5_DEPS_ROOT must be set for the FFmpeg XMAFrames cache." >&2
    exit 2
fi
if [[ ! -s "$PATCH_FILE" ]]; then
    echo "ERROR: missing FFmpeg XMAFrames patch: $PATCH_FILE" >&2
    exit 2
fi

for command in make patch sha256sum tar wget cksum awk; do
    command -v "$command" >/dev/null || {
        echo "ERROR: missing required command for patched FFmpeg: $command" >&2
        exit 2
    }
done

FFMPEG_ROOT="${MARATHON_XMA_FFMPEG_ROOT:-$DEPS_ROOT/native/ffmpeg-xmaframes}"
DOWNLOAD_DIR="$FFMPEG_ROOT/downloads"
SOURCE_ROOT="$FFMPEG_ROOT/src"
SOURCE_DIR="$SOURCE_ROOT/ffmpeg-$FFMPEG_VERSION"
ARCHIVE="$DOWNLOAD_DIR/ffmpeg-$FFMPEG_VERSION.tar.xz"
LIBAVCODEC="$SOURCE_DIR/libavcodec/libavcodec.a"
STAMP="$FFMPEG_ROOT/.xmaframes-build-config"
JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"
PATCH_SHA256="$(sha256sum "$PATCH_FILE" | awk '{print $1}')"

mkdir -p "$DOWNLOAD_DIR" "$SOURCE_ROOT"

if [[ ! -f "$ARCHIVE" ]] || [[ "$(sha256sum "$ARCHIVE" | awk '{print $1}')" != "$FFMPEG_SHA256" ]]; then
    echo "==> [ffmpeg-xma] Downloading FFmpeg $FFMPEG_VERSION source..." >&2
    temporary_archive="$ARCHIVE.download"
    rm -f -- "$temporary_archive"
    wget -q "$FFMPEG_URL" -O "$temporary_archive"
    if [[ "$(sha256sum "$temporary_archive" | awk '{print $1}')" != "$FFMPEG_SHA256" ]]; then
        rm -f -- "$temporary_archive"
        echo "ERROR: FFmpeg $FFMPEG_VERSION source checksum mismatch." >&2
        exit 2
    fi
    mv "$temporary_archive" "$ARCHIVE"
fi

# PacBrew uses this same Prospero toolchain to build its FFmpeg 7.0.1 port.
export PS5_PAYLOAD_SDK="$SDK_ROOT"
set +u
source "$SDK_ROOT/toolchain/prospero.sh"
set -u

for tool in CC CXX AR RANLIB NM STRIP PKG_CONFIG; do
    if [[ -z "${!tool:-}" ]]; then
        echo "ERROR: Prospero toolchain did not set $tool." >&2
        exit 2
    fi
done

CONFIGURE_ARGS=(
    "--prefix=$FFMPEG_ROOT/prefix"
    --enable-cross-compile
    "--cross-prefix=$SDK_ROOT/bin/prospero-"
    --enable-static
    --disable-shared
    --enable-pic
    --arch=x86_64
    --target-os=freebsd
    --disable-everything
    --enable-decoder=xmaframes
    --disable-programs
    --disable-doc
    --disable-debug
    --disable-autodetect
    --disable-x86asm
    "--cc=$CC"
    "--cxx=$CXX"
    "--nm=$NM"
    "--strip=$STRIP"
    "--ar=$AR"
    "--ranlib=$RANLIB"
    "--pkg-config=$PKG_CONFIG"
)

BUILD_FINGERPRINT="$(
    {
        printf '%s\n' "$FFMPEG_VERSION" "$FFMPEG_SHA256" "$PATCH_SHA256" "$SDK_ROOT" \
            "$CC" "$CXX" "$AR" "$RANLIB" "$NM" "$STRIP" "$PKG_CONFIG" \
            "${CFLAGS:-}" "${CPPFLAGS:-}" "${LDFLAGS:-}" "${CONFIGURE_ARGS[@]}"
        cksum "$SDK_ROOT/toolchain/prospero.sh"
    } | cksum | awk '{print $1 ":" $2}'
)"

previous_fingerprint=""
if [[ -f "$STAMP" ]]; then
    IFS= read -r previous_fingerprint < "$STAMP"
fi
if [[ "${CLEAN_BUILD:-0}" == "1" || ! -s "$LIBAVCODEC" || "$previous_fingerprint" != "$BUILD_FINGERPRINT" ]]; then
    echo "==> [ffmpeg-xma] Building the patched XMAFrames decoder for Prospero (-j $JOBS)..." >&2
    rm -rf -- "$SOURCE_DIR"
    tar -xf "$ARCHIVE" -C "$SOURCE_ROOT"
    cd "$SOURCE_DIR"
    patch --silent -p1 < "$PATCH_FILE"

    if ! ./configure "${CONFIGURE_ARGS[@]}" > "$FFMPEG_ROOT/configure.log" 2>&1; then
        tail -n 100 "$FFMPEG_ROOT/configure.log" >&2 || true
        echo "ERROR: configuring patched FFmpeg failed; see $FFMPEG_ROOT/configure.log." >&2
        exit 2
    fi
    if ! make -j "$JOBS" libavcodec/libavcodec.a > "$FFMPEG_ROOT/build.log" 2>&1; then
        tail -n 120 "$FFMPEG_ROOT/build.log" >&2 || true
        echo "ERROR: building patched FFmpeg failed; see $FFMPEG_ROOT/build.log." >&2
        exit 2
    fi

    printf '%s\n' "$BUILD_FINGERPRINT" > "$STAMP"
else
    echo "==> [ffmpeg-xma] Using cached XMAFrames decoder." >&2
fi

[[ -s "$LIBAVCODEC" ]] || {
    echo "ERROR: patched FFmpeg did not produce $LIBAVCODEC." >&2
    exit 2
}
