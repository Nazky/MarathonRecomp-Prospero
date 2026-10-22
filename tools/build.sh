#!/usr/bin/env bash
# MarathonRecomp - Native PS5 build and homebrew staging script.
# Uses the PS5 Payload SDK, RADV (Mesa), PacBrew, and Plume.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TITLE_ID="${TITLE_ID:-PPSA99903}"
if [[ ! "$TITLE_ID" =~ ^PPS[AB][0-9]{5}$ ]]; then
    echo "ERROR: set TITLE_ID to an assigned/fake PS5 title ID (PPSA/PPSB plus five digits)." >&2
    exit 2
fi
CONCEPT_ID="${TITLE_ID:4:5}"
APP_TITLE="${APP_TITLE:-Sonic the Hedgehog (2006) Recompiled}"

# Convert retained sce_sys source files and validate package-ready assets early.
bash "$ROOT/tools/prepare-assets.sh"

GAME_INPUT_ROOT="${MARATHON_GAME_DIR:-}"
if [[ -z "$GAME_INPUT_ROOT" ]]; then
    for candidate in "$ROOT/ressources" "$ROOT/resources" "$ROOT/game_assets"; do
        if [[ -d "$candidate/game" ]]; then
            GAME_INPUT_ROOT="$candidate"
            break
        fi
    done
elif [[ "$GAME_INPUT_ROOT" != /* ]]; then
    GAME_INPUT_ROOT="$ROOT/$GAME_INPUT_ROOT"
fi

find_game_input() {
    local name="$1" search_root
    [[ -n "$GAME_INPUT_ROOT" ]] || return 1
    if [[ -d "$GAME_INPUT_ROOT/game" ]]; then
        search_root="$GAME_INPUT_ROOT/game"
    else
        search_root="$GAME_INPUT_ROOT"
    fi
    [[ -d "$search_root" ]] || return 1
    find "$search_root" -maxdepth 5 -type f -iname "$name" -print -quit 2>/dev/null
}

for name in default.xex shader.arc shader_lt.arc; do
    if [[ ! -s "$ROOT/MarathonRecompLib/private/$name" ]]; then
        game_input="$(find_game_input "$name" || true)"
        if [[ -z "$game_input" || ! -s "$game_input" ]]; then
            echo "ERROR: missing required private game input: $name" >&2
            echo "Provide it in MarathonRecompLib/private/ or under <game-root>/game (shader archives may be in xenon/archives)." >&2
            echo "Set MARATHON_GAME_DIR to the folder containing game/, or directly to that game folder." >&2
            exit 2
        fi
    fi
done
PLUME_ROOT="${PLUME_ROOT:-$ROOT/thirdparty/plume}"

# Keep large PS5 SDK/ports/driver downloads outside the checked-out workspace.
if [[ -n "${PS5_DEPS_ROOT:-}" ]]; then
    DEPS_ROOT="$PS5_DEPS_ROOT"
elif [[ -n "${MARATHON_DEPS_ROOT:-}" ]]; then
    DEPS_ROOT="$MARATHON_DEPS_ROOT"
elif [[ -d "$ROOT/.deps/native/ps5-payload-sdk" ]]; then
    DEPS_ROOT="$ROOT/.deps" # legacy cache from earlier builds
elif [[ -d "/opt/ps5-cache/refs/PS5_Vulkan/.deps/native/ps5-payload-sdk" ]]; then
    DEPS_ROOT="/opt/ps5-cache/refs/PS5_Vulkan/.deps"
else
    DEPS_ROOT="${XDG_CACHE_HOME:-$HOME/.cache}/MarathonRecomp/deps"
fi
export PS5_DEPS_ROOT="$DEPS_ROOT"

SDK_ROOT="${PS5_PAYLOAD_SDK:-$DEPS_ROOT/native/ps5-payload-sdk}"
ZLIB_ROOT="$DEPS_ROOT/native/zlib/root"
RADV_ARCHIVE_OVERRIDE="${RADV_ARCHIVE:-}"
if [[ -n "$RADV_ARCHIVE_OVERRIDE" ]]; then
    RADV_ARCHIVE="$RADV_ARCHIVE_OVERRIDE"
elif [[ -f "$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a" ]]; then
    RADV_ARCHIVE="$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a"
else
    RADV_ARCHIVE="$DEPS_ROOT/native/radv/lib/libvulkan_radeon.ps5.a"
fi
PACBREW_SYSROOT="${PACBREW_SYSROOT:-$DEPS_ROOT/pacbrew/v0.40.2/sysroot/user/homebrew}"

if [[ ! -x "$SDK_ROOT/bin/prospero-lld" || ! -f "$RADV_ARCHIVE" || ! -d "$PACBREW_SYSROOT" ]]; then
    echo "==> [deps] Bootstrapping PS5 SDK, RADV, and PacBrew dependencies into $DEPS_ROOT..."
    bash "$ROOT/tools/setup-native-dependencies.sh"
    bash "$ROOT/tools/build-radv.sh" release
    bash "$ROOT/tools/setup-pacbrew-dependencies.sh" --all >/dev/null

    # build-radv.sh release installs under radv-release; refresh the path chosen
    # before bootstrapping so a missing debug-path fallback isn't linked.
    if [[ -z "$RADV_ARCHIVE_OVERRIDE" ]]; then
        if [[ -f "$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a" ]]; then
            RADV_ARCHIVE="$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a"
        elif [[ -f "$DEPS_ROOT/native/radv/lib/libvulkan_radeon.ps5.a" ]]; then
            RADV_ARCHIVE="$DEPS_ROOT/native/radv/lib/libvulkan_radeon.ps5.a"
        fi
    fi
fi

if [[ ! -f "$RADV_ARCHIVE" ]]; then
    echo "ERROR: RADV archive is missing: $RADV_ARCHIVE" >&2
    echo "Run tools/build-radv.sh release or set RADV_ARCHIVE to an existing PS5 RADV archive." >&2
    exit 2
fi

PACBREW_ROOT="${PACBREW_ROOT:-${PACBREW_SYSROOT%/user/homebrew}}"
export PKG_CONFIG_SYSROOT_DIR="$PACBREW_ROOT"
export PKG_CONFIG_LIBDIR="$PACBREW_SYSROOT/lib/pkgconfig:$PACBREW_SYSROOT/libdata/pkgconfig"
export PKG_CONFIG_PATH="$PACBREW_SYSROOT/libdata/pkgconfig"
export PKG_CONFIG_DIR=
if ! command -v pkg-config >/dev/null || ! pkg-config --exists libavcodec; then
    echo "ERROR: PacBrew's PS5 FFmpeg pkg-config metadata (libavcodec) is missing." >&2
    echo "Run tools/setup-pacbrew-dependencies.sh --all and check PACBREW_SYSROOT." >&2
    exit 2
fi
read -r -a FFMPEG_CFLAGS <<<"$(pkg-config --static --cflags libavcodec)"
read -r -a FFMPEG_LIBS <<<"$(pkg-config --static --libs libavcodec)"

# PacBrew's FFmpeg 7.0.1 omits Marathon's custom XMA raw-frame decoder.
# Build that decoder with the same Prospero toolchain and use its headers and
# libavcodec, while continuing to use PacBrew's matching libavutil libraries.
FFMPEG_XMA_DEFAULT_ROOT="$DEPS_ROOT/native/ffmpeg-xmaframes"
case "$DEPS_ROOT/" in
    "$ROOT/"*) FFMPEG_XMA_DEFAULT_ROOT="${XDG_CACHE_HOME:-$HOME/.cache}/MarathonRecomp/deps/native/ffmpeg-xmaframes" ;;
esac
FFMPEG_XMA_ROOT="${MARATHON_XMA_FFMPEG_ROOT:-$FFMPEG_XMA_DEFAULT_ROOT}"
if [[ "$FFMPEG_XMA_ROOT" != /* ]]; then
    FFMPEG_XMA_ROOT="$ROOT/$FFMPEG_XMA_ROOT"
fi
MARATHON_XMA_FFMPEG_ROOT="$FFMPEG_XMA_ROOT" \
    bash "$ROOT/tools/build-ffmpeg-xmaframes.sh" "$SDK_ROOT" "$DEPS_ROOT"
FFMPEG_XMA_SOURCE="$FFMPEG_XMA_ROOT/src/ffmpeg-7.0.1"
FFMPEG_XMA_AVCODEC="$FFMPEG_XMA_SOURCE/libavcodec/libavcodec.a"
if [[ ! -s "$FFMPEG_XMA_AVCODEC" ]]; then
    echo "ERROR: patched XMAFrames libavcodec was not produced." >&2
    exit 2
fi

FFMPEG_RELINKED_LIBS=()
ffmpeg_codec_replaced=0
ffmpeg_pthread_moved=0
for ffmpeg_lib in "${FFMPEG_LIBS[@]}"; do
    case "$ffmpeg_lib" in
        -lavcodec|-l:libavcodec.a)
            FFMPEG_RELINKED_LIBS+=("$FFMPEG_XMA_AVCODEC")
            ffmpeg_codec_replaced=1
            ;;
        -lz|-l:libz.a)
            # RADV is linked --whole-archive and already contains its zlib
            # implementation. Avoid pulling a second static zlib from PacBrew.
            ;;
        -pthread)
            # The PS5 build invokes lld directly, which does not accept the
            # compiler-driver-only -pthread option. Keep it on C++ compile flags.
            ffmpeg_pthread_in_cflags=0
            for ffmpeg_cflag in "${FFMPEG_CFLAGS[@]}"; do
                if [[ "$ffmpeg_cflag" == "-pthread" ]]; then
                    ffmpeg_pthread_in_cflags=1
                    break
                fi
            done
            if (( ! ffmpeg_pthread_in_cflags && ! ffmpeg_pthread_moved )); then
                FFMPEG_CFLAGS+=("-pthread")
                ffmpeg_pthread_moved=1
            fi
            ;;
        -Wl,*)
            IFS=',' read -r -a ffmpeg_wl_args <<<"${ffmpeg_lib#-Wl,}"
            FFMPEG_RELINKED_LIBS+=("${ffmpeg_wl_args[@]}")
            ;;
        *)
            FFMPEG_RELINKED_LIBS+=("$ffmpeg_lib")
            ;;
    esac
done
if (( ! ffmpeg_codec_replaced )); then
    echo "ERROR: unexpected PacBrew FFmpeg link flags; could not replace -lavcodec." >&2
    printf '  %s\n' "${FFMPEG_LIBS[@]}" >&2
    exit 2
fi
FFMPEG_LIBS=("${FFMPEG_RELINKED_LIBS[@]}")

echo "==> [ffmpeg-xma] Linking the patched XMAFRAMES decoder."

MODULE_SDK=0x02000009
COMPANION_SDK=0x08050001
FSELF_MAGIC=0x1D3D154F

BUILD_DIR="$ROOT/build"
OBJ_DIR="$BUILD_DIR/obj"
STUB_DIR="$BUILD_DIR/stubs"
DIST_DIR="$ROOT/dist"
PKG_DIR="$ROOT/pkg"
NATIVE_DIR="$ROOT/tooling/native"
NATIVE_TOOL="${NATIVE_TOOL:-$BUILD_DIR/host/ps5-native-tool}"
JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"

mkdir -p "$BUILD_DIR/host" "$OBJ_DIR" "$STUB_DIR" "$DIST_DIR" "$PKG_DIR"

HOST_CXX="${CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
chmod +x "$ROOT/tools/bin/"* 2>/dev/null || true
if [[ ! -x "$NATIVE_TOOL" ]]; then
    if [[ -x "$ROOT/tools/bin/ps5-native-tool" ]]; then
        cp -f "$ROOT/tools/bin/ps5-native-tool" "$NATIVE_TOOL"
        chmod +x "$NATIVE_TOOL"
    else
        ZLIB_ARCHIVE="$(find "$ZLIB_ROOT" -type f -name libz.a -print -quit)"
        "$HOST_CXX" -std=c++20 -O2 -Wall -Wextra \
            -I "$ZLIB_ROOT/usr/include" \
            "$NATIVE_DIR/native_app_builder.cpp" \
            "$NATIVE_DIR/self_container.cpp" \
            "$NATIVE_DIR/elf_object.cpp" \
            "$NATIVE_DIR/sce_module_writer.cpp" \
            "$ZLIB_ARCHIVE" \
            -o "$NATIVE_TOOL"
    fi
fi

# Generate Marathon-specific PPC and shader sources from the supplied retail inputs.
if [[ -n "$GAME_INPUT_ROOT" ]]; then
    MARATHON_GAME_DIR="$GAME_INPUT_ROOT" bash "$ROOT/tools/recomp-xex.sh" "$GAME_INPUT_ROOT"
else
    bash "$ROOT/tools/recomp-xex.sh"
fi

# Generate SPIR-V C headers required by Marathon's PS5 Vulkan backend.
bash "$ROOT/tools/compile-shaders.sh"

# Generate embedded BIN2C resources into $BUILD_DIR/res if not yet generated.
bash "$ROOT/tools/generate-resources.sh" "$BUILD_DIR"

# The desktop CMake configure normally emits these ignored version sources.
BUILD_TYPE="${BUILD_TYPE:-Release}" bash "$ROOT/tools/generate-version.sh"

cc() { PS5_PAYLOAD_SDK="$SDK_ROOT" sh "$ROOT/tooling/prospero-clang18" "$@"; }

COMMON_INCLUDES=(
    -I"$BUILD_DIR"
    -I"$ROOT/MarathonRecomp"
    -I"$ROOT/MarathonRecomp/api"
    -I"$ROOT/MarathonRecompLib"
    -I"$ROOT/thirdparty/concurrentqueue"
    -I"$ROOT/thirdparty/ddspp"
    -I"$ROOT/thirdparty/imgui"
    -I"$ROOT/thirdparty/PS5VK-Overlay/include"
    -I"$ROOT/thirdparty/implot"
    -I"$ROOT/thirdparty/json/include"
    -I"$ROOT/thirdparty/magic_enum/include"
    -I"$ROOT/thirdparty/o1heap"
    -I"$ROOT/thirdparty/stb"
    -I"$ROOT/tools/XenonRecomp/thirdparty/tomlplusplus/include"
    -I"$ROOT/thirdparty/unordered_dense/include"
    -I"$ROOT/tools/bc_diff"
    -I"$ROOT/tools/XenonRecomp/XenonUtils"
    -I"$ROOT/tools/XenonRecomp/thirdparty/simde"
    -I"$ROOT/tools/XenonRecomp/thirdparty/fmt/include"
    -I"$ROOT/tools/XenonRecomp/thirdparty/xxHash"
    -I"$ROOT/tools/XenonRecomp/thirdparty/libmspack/libmspack/mspack"
    -I"$ROOT/tools/XenonRecomp/thirdparty/tiny-AES-c"
    -I"$ROOT/tools/XenonRecomp/thirdparty/TinySHA1"
    -I"$ROOT/tools/XenonRecomp/thirdparty/disasm"
    -I"$ROOT/tools/XenosRecomp/thirdparty/smol-v/source"
    -I"$FFMPEG_XMA_SOURCE"
    -I"$PLUME_ROOT"
    -I"$PLUME_ROOT/contrib/Vulkan-Headers/include"
    -I"$PLUME_ROOT/contrib/volk"
    -I"$PLUME_ROOT/contrib/VulkanMemoryAllocator/include"
    -I"$PLUME_ROOT/contrib/SPIRV-Reflect"
    -I"$PACBREW_SYSROOT/include"
    -I"$PACBREW_SYSROOT/include/SDL2"
)

COMMON_DEFINES=(
    -D__PROSPERO__
    -DUSE_PTHREAD
    -DVK_NO_PROTOTYPES
    -DSDL_MAIN_HANDLED
    -DNOMINMAX
    -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    -D_CRT_SECURE_NO_WARNINGS
    -DNDEBUG
)

COMMON_WARN_FLAGS=(
    -fno-strict-aliasing
    -ffp-model=strict
    -Wno-unknown-warning-option
    -Wno-nontrivial-memcall
    -Wno-missing-template-arg-list-after-template-kw
    -Wno-enum-constexpr-conversion
    -Wno-switch
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-void-pointer-to-int-cast
    -Wno-int-to-void-pointer-cast
    -Wno-invalid-offsetof
    -Wno-null-arithmetic
    -Wno-null-conversion
    -Wno-tautological-undefined-compare
    -Wno-deprecated-declarations
    -Wno-format
)

CFLAGS=(-std=c11 -O2 -ffunction-sections -fdata-sections "${COMMON_DEFINES[@]}" "${COMMON_INCLUDES[@]}")
CXXFLAGS=(-std=c++20 -O2 -fexceptions -funwind-tables -ffunction-sections -fdata-sections "${COMMON_DEFINES[@]}" "${COMMON_WARN_FLAGS[@]}" "${COMMON_INCLUDES[@]}" "${FFMPEG_CFLAGS[@]}")
if [[ "${PS5_GPU_DIAGNOSTICS:-0}" == "1" ]]; then
    CXXFLAGS+=(-DMARATHON_PS5_GPU_DIAGNOSTICS=1)
    echo "==> [build] High-volume PS5 GPU diagnostics enabled."
fi
XMA_PATCH_HASH="$(sha256sum "$ROOT/tools/patches/ffmpeg-xmaframes-7.0.1.patch" | awk '{print $1}')"
IMGUI_SOURCE_HASH="missing"
if [[ -f "$ROOT/thirdparty/imgui/imgui.cpp" && -f "$ROOT/thirdparty/imgui/imgui.h" ]]; then
    IMGUI_SOURCE_HASH="$(
        sha256sum \
            "$ROOT"/thirdparty/imgui/imgui*.cpp \
            "$ROOT"/thirdparty/imgui/imgui*.h \
            "$ROOT"/thirdparty/imgui/imconfig.h \
            "$ROOT"/thirdparty/imgui/imstb*.h \
            "$ROOT"/thirdparty/imgui/backends/imgui_impl_sdl2.cpp \
            "$ROOT"/thirdparty/imgui/backends/imgui_impl_sdl2.h \
            | sha256sum | awk '{print $1}'
    )"
fi
REGISTRY_HEADER_HASH="$(sha256sum "$ROOT/MarathonRecomp/os/registry.h" | awk '{print $1}')"

# Invalidate cached objects/PCHs when target flags, SDK, Clang, FFmpeg/ImGui
# sources, or PS5 compatibility headers change.
TOOLCHAIN_COMPILER="${PS5_CLANG:-$(command -v clang || command -v clang-18 || true)}"
TOOLCHAIN_VERSION=""
if [[ -n "$TOOLCHAIN_COMPILER" ]]; then
    TOOLCHAIN_VERSION="$("$TOOLCHAIN_COMPILER" --version 2>/dev/null | head -n 1 || true)"
fi
BUILD_CONFIG_HASH="$(
    {
        printf '%s\0' "$SDK_ROOT" "$TOOLCHAIN_COMPILER" "$TOOLCHAIN_VERSION" "$FFMPEG_XMA_SOURCE" "$FFMPEG_XMA_AVCODEC" "$XMA_PATCH_HASH" "$IMGUI_SOURCE_HASH" "$REGISTRY_HEADER_HASH" "${CFLAGS[@]}" "${CXXFLAGS[@]}" "${FFMPEG_LIBS[@]}"
        cksum "$ROOT/tooling/prospero-clang18"
    } | cksum | awk '{print $1 ":" $2}'
)"
BUILD_CONFIG_STAMP="$BUILD_DIR/.ps5-build-config"
previous_build_config=""
if [[ -f "$BUILD_CONFIG_STAMP" ]]; then
    IFS= read -r previous_build_config < "$BUILD_CONFIG_STAMP"
fi
if [[ "$previous_build_config" != "$BUILD_CONFIG_HASH" ]]; then
    echo "==> [build] PS5 compiler configuration changed; invalidating cached objects/PCHs."
    rm -f "$OBJ_DIR"/*.o "$BUILD_DIR"/*.pch
    printf '%s\n' "$BUILD_CONFIG_HASH" > "$BUILD_CONFIG_STAMP"
fi

obj_path_for() {
    local src="$1"
    local rel="${src#$ROOT/}"
    rel="${rel//\//__}"
    printf '%s/%s.o' "$OBJ_DIR" "$rel"
}

OBJECTS=()
QUEUE_C=()
QUEUE_CXX_NOPCH=()
QUEUE_CXX_PPCPCH=()
QUEUE_CXX_STDPCH=()

queue_c() {
    local src="$1"
    local obj
    obj="$(obj_path_for "$src")"
    OBJECTS+=("$obj")
    if [[ "${CLEAN_BUILD:-0}" == "1" || ! -f "$obj" || "$src" -nt "$obj" ]]; then
        QUEUE_C+=("$src")
    fi
}

queue_cxx() {
    local src="$1"
    local pch_mode="${2:-none}"
    local obj needs_rebuild=0 pch_header="" shader_header overlay_header
    obj="$(obj_path_for "$src")"
    OBJECTS+=("$obj")

    if [[ "${CLEAN_BUILD:-0}" == "1" || ! -f "$obj" || "$src" -nt "$obj" ]]; then
        needs_rebuild=1
    fi
    case "$pch_mode" in
        ppc) pch_header="$ROOT/MarathonRecompLib/ppc/ppc_recomp_shared.h" ;;
        std) pch_header="$ROOT/MarathonRecomp/stdafx.h" ;;
    esac
    if [[ -f "$obj" && -n "$pch_header" && "$pch_header" -nt "$obj" ]]; then
        needs_rebuild=1
    fi
    if [[ -f "$obj" && "$src" == "$ROOT/MarathonRecomp/gpu/video.cpp" ]]; then
        for shader_header in "$ROOT"/MarathonRecomp/gpu/shader/hlsl/*.spirv.h; do
            if [[ -f "$shader_header" && "$shader_header" -nt "$obj" ]]; then
                needs_rebuild=1
                break
            fi
        done
        for overlay_header in "$ROOT"/thirdparty/PS5VK-Overlay/include/*.h; do
            if [[ -f "$overlay_header" && "$overlay_header" -nt "$obj" ]]; then
                needs_rebuild=1
                break
            fi
        done
    elif [[ -f "$obj" && "$src" == "$ROOT/thirdparty/PS5VK-Overlay/src/system_info_overlay.cpp" ]]; then
        for overlay_header in "$ROOT"/thirdparty/PS5VK-Overlay/include/*.h; do
            if [[ -f "$overlay_header" && "$overlay_header" -nt "$obj" ]]; then
                needs_rebuild=1
                break
            fi
        done
    fi

    if (( needs_rebuild )); then
        case "$pch_mode" in
            ppc) QUEUE_CXX_PPCPCH+=("$src") ;;
            std) QUEUE_CXX_STDPCH+=("$src") ;;
            *)   QUEUE_CXX_NOPCH+=("$src") ;;
        esac
    fi
}

run_parallel_compile() {
    local active=0
    for cmd_idx in "$@"; do
        eval "$cmd_idx" &
        active=$((active + 1))
        if (( active >= JOBS )); then
            wait -n
            active=$((active - 1))
        fi
    done
    wait
}

echo "==> [1/7] Compiling PS5 CRT & AGC stubs..."
cc -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$NATIVE_DIR/app_crt.cpp" -o "$OBJ_DIR/app_crt.o"
OBJECTS+=("$OBJ_DIR/app_crt.o")

stub() {
    local library=$1 source=$2
    cc -std=c11 -O2 -fPIC -c "$ROOT/$source" -o "$OBJ_DIR/${library}_stub.o"
    "$SDK_ROOT/bin/prospero-lld" --shared -soname "${library}.prx" \
        -o "$STUB_DIR/${library}.so" "$OBJ_DIR/${library}_stub.o"
}
stub libSceAgc vendor/ps5/sdk/stubs/agc_canary_link_stub.c
stub libSceAgcDriver vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c

echo "==> [2/7] Queueing Plume, XenonUtils & thirdparty sources..."
queue_cxx "$PLUME_ROOT/plume_vulkan.cpp" none
queue_c "$ROOT/thirdparty/o1heap/o1heap.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/xxHash/xxhash.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/disasm/disasm.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/disasm/ppc-dis.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/tiny-AES-c/aes.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/libmspack/libmspack/mspack/lzxd.c"
# PacBrew's zstd archive carries weak trace-hook references; define the
# optional hooks locally so the PS5 import converter doesn't seek SDK exports.
queue_c "$NATIVE_DIR/zstd_trace_stubs.c"
queue_cxx "$ROOT/tools/XenonRecomp/thirdparty/fmt/src/format.cc" none
queue_cxx "$ROOT/tools/XenonRecomp/thirdparty/fmt/src/os.cc" none
queue_cxx "$ROOT/thirdparty/PS5VK-Overlay/src/system_info_overlay.cpp" none

for src in "$ROOT"/tools/XenonRecomp/XenonUtils/*.cpp; do
    queue_cxx "$src" none
done

IMGUI_REQUIRED_SRCS=(
    "$ROOT/thirdparty/imgui/backends/imgui_impl_sdl2.cpp"
    "$ROOT/thirdparty/imgui/imgui.cpp"
    "$ROOT/thirdparty/imgui/imgui_demo.cpp"
    "$ROOT/thirdparty/imgui/imgui_draw.cpp"
    "$ROOT/thirdparty/imgui/imgui_tables.cpp"
    "$ROOT/thirdparty/imgui/imgui_widgets.cpp"
)
for src in "${IMGUI_REQUIRED_SRCS[@]}"; do
    if [[ ! -f "$src" ]]; then
        echo "ERROR: missing Dear ImGui submodule source: $src" >&2
        echo "Run: git submodule update --init --recursive thirdparty/imgui" >&2
        exit 2
    fi
    queue_cxx "$src" none
done

for src in \
    "$ROOT/thirdparty/implot/implot.cpp" \
    "$ROOT/thirdparty/implot/implot_demo.cpp" \
    "$ROOT/thirdparty/implot/implot_items.cpp" \
    "$ROOT/tools/XenosRecomp/thirdparty/smol-v/source/smolv.cpp"; do
    [[ -f "$src" ]] && queue_cxx "$src" none
done

echo "==> [3/7] Queueing MarathonRecompLib (PPC + Xenos Shader Cache)..."
for src in "$ROOT"/MarathonRecompLib/ppc/*.cpp; do
    queue_cxx "$src" ppc
done
for src in "$ROOT"/MarathonRecompLib/shader/*.cpp; do
    queue_cxx "$src" ppc
done

echo "==> [4/7] Queueing MarathonRecomp embedded BIN2C resources..."
while IFS= read -r -d '' cfile; do
    queue_c "$cfile"
done < <(find "$BUILD_DIR/res" -name "*.c" -print0 | sort -z)

echo "==> [5/7] Queueing MarathonRecomp C++ sources..."
if [[ ! -f "$ROOT/MarathonRecomp/ps5/elevation.cpp" ]]; then
    echo "ERROR: missing MarathonRecomp/ps5/elevation.cpp; this PS5 build needs the filesystem access policy hook." >&2
    exit 2
fi
MARATHON_SRCS=(
    "$ROOT/MarathonRecomp/app.cpp"
    "$ROOT/MarathonRecomp/exports.cpp"
    "$ROOT/MarathonRecomp/main.cpp"
    "$ROOT/MarathonRecomp/misc_impl.cpp"
    "$ROOT/MarathonRecomp/preload_executable.cpp"
    "$ROOT/MarathonRecomp/sdl_listener.cpp"
    "$ROOT/MarathonRecomp/stdafx.cpp"
    "$ROOT/MarathonRecomp/version.cpp"
    "$ROOT/MarathonRecomp/kernel/imports.cpp"
    "$ROOT/MarathonRecomp/kernel/xdm.cpp"
    "$ROOT/MarathonRecomp/kernel/heap.cpp"
    "$ROOT/MarathonRecomp/kernel/memory.cpp"
    "$ROOT/MarathonRecomp/kernel/xam.cpp"
    "$ROOT/MarathonRecomp/kernel/io/file_system.cpp"
    "$ROOT/MarathonRecomp/locale/achievement_locale.cpp"
    "$ROOT/MarathonRecomp/locale/config_locale.cpp"
    "$ROOT/MarathonRecomp/locale/locale.cpp"
    "$ROOT/MarathonRecomp/os/linux/logger_linux.cpp"
    "$ROOT/MarathonRecomp/os/linux/media_linux.cpp"
    "$ROOT/MarathonRecomp/os/linux/process_linux.cpp"
    "$ROOT/MarathonRecomp/os/linux/user_linux.cpp"
    "$ROOT/MarathonRecomp/os/linux/version_linux.cpp"
    # PS5-specific facilities are explicit in this custom build.
    "$ROOT/MarathonRecomp/ps5/elevation.cpp"
    "$ROOT/MarathonRecomp/cpu/guest_thread.cpp"
    "$ROOT/MarathonRecomp/gpu/video.cpp"
    "$ROOT/MarathonRecomp/gpu/imgui/imgui_common.cpp"
    "$ROOT/MarathonRecomp/gpu/imgui/imgui_font_builder.cpp"
    "$ROOT/MarathonRecomp/gpu/imgui/imgui_snapshot.cpp"
    "$ROOT/MarathonRecomp/apu/audio.cpp"
    "$ROOT/MarathonRecomp/apu/xma_decoder.cpp"
    "$ROOT/MarathonRecomp/apu/embedded_player.cpp"
    "$ROOT/MarathonRecomp/apu/driver/sdl2_driver.cpp"
    "$ROOT/MarathonRecomp/hid/hid.cpp"
    "$ROOT/MarathonRecomp/hid/driver/sdl_hid.cpp"
    "$ROOT/MarathonRecomp/patches/aspect_ratio_patches.cpp"
    "$ROOT/MarathonRecomp/patches/audio_patches.cpp"
    "$ROOT/MarathonRecomp/patches/camera_patches.cpp"
    "$ROOT/MarathonRecomp/patches/fps_patches.cpp"
    "$ROOT/MarathonRecomp/patches/frontend_listener.cpp"
    "$ROOT/MarathonRecomp/patches/input_patches.cpp"
    "$ROOT/MarathonRecomp/patches/loading_patches.cpp"
    "$ROOT/MarathonRecomp/patches/MainMenuTask_patches.cpp"
    "$ROOT/MarathonRecomp/patches/misc_patches.cpp"
    "$ROOT/MarathonRecomp/patches/pause_patches.cpp"
    "$ROOT/MarathonRecomp/patches/player_patches.cpp"
    "$ROOT/MarathonRecomp/patches/SaveDataTask_patches.cpp"
    "$ROOT/MarathonRecomp/patches/text_patches.cpp"
    "$ROOT/MarathonRecomp/patches/TitleTask_patches.cpp"
    "$ROOT/MarathonRecomp/patches/video_patches.cpp"
    "$ROOT/MarathonRecomp/ui/achievement_menu.cpp"
    "$ROOT/MarathonRecomp/ui/achievement_overlay.cpp"
    "$ROOT/MarathonRecomp/ui/black_bar.cpp"
    "$ROOT/MarathonRecomp/ui/button_window.cpp"
    "$ROOT/MarathonRecomp/ui/common_menu.cpp"
    "$ROOT/MarathonRecomp/ui/fader.cpp"
    "$ROOT/MarathonRecomp/ui/game_window.cpp"
    "$ROOT/MarathonRecomp/ui/imgui_utils.cpp"
    "$ROOT/MarathonRecomp/ui/installer_wizard.cpp"
    "$ROOT/MarathonRecomp/ui/message_window.cpp"
    "$ROOT/MarathonRecomp/ui/mods_menu.cpp"
    "$ROOT/MarathonRecomp/ui/options_menu.cpp"
    "$ROOT/MarathonRecomp/install/installer.cpp"
    "$ROOT/MarathonRecomp/install/iso_file_system.cpp"
    "$ROOT/MarathonRecomp/install/update_checker.cpp"
    "$ROOT/MarathonRecomp/install/xcontent_file_system.cpp"
    "$ROOT/MarathonRecomp/install/hashes/game.cpp"
    "$ROOT/MarathonRecomp/install/hashes/episode_sonic.cpp"
    "$ROOT/MarathonRecomp/install/hashes/episode_shadow.cpp"
    "$ROOT/MarathonRecomp/install/hashes/episode_silver.cpp"
    "$ROOT/MarathonRecomp/install/hashes/episode_amigo.cpp"
    "$ROOT/MarathonRecomp/install/hashes/mission_sonic.cpp"
    "$ROOT/MarathonRecomp/install/hashes/mission_shadow.cpp"
    "$ROOT/MarathonRecomp/install/hashes/mission_silver.cpp"
    "$ROOT/MarathonRecomp/user/achievement_data.cpp"
    "$ROOT/MarathonRecomp/user/achievement_manager.cpp"
    "$ROOT/MarathonRecomp/user/config.cpp"
    "$ROOT/MarathonRecomp/user/registry.cpp"
    "$ROOT/MarathonRecomp/user/paths.cpp"
    "$ROOT/MarathonRecomp/utils/bit_stream.cpp"
    "$ROOT/MarathonRecomp/utils/ring_buffer.cpp"
    "$ROOT/MarathonRecomp/mod/custom_filesystem.cpp"
    "$ROOT/MarathonRecomp/mod/mod_loader.cpp"
)
for src in "${MARATHON_SRCS[@]}"; do
    if [[ ! -f "$src" ]]; then
        echo "ERROR: required MarathonRecomp source is missing: $src" >&2
        exit 2
    fi
    queue_cxx "$src" std
done

echo "==> [6/7] Compiling queued translation units in parallel (-j $JOBS)..."
if (( ${#QUEUE_C[@]} > 0 )); then
    CMDS=()
    for src in "${QUEUE_C[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" ]] && continue
        CMDS+=("cc \"\${CFLAGS[@]}\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

if (( ${#QUEUE_CXX_NOPCH[@]} > 0 )); then
    CMDS=()
    for src in "${QUEUE_CXX_NOPCH[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" ]] && continue
        CMDS+=("cc \"\${CXXFLAGS[@]}\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

if (( ${#QUEUE_CXX_PPCPCH[@]} > 0 )); then
    PPC_PCH="$BUILD_DIR/ppc_recomp_shared.h.pch"
    if [[ "${CLEAN_BUILD:-0}" == "1" || ! -f "$PPC_PCH" || "$ROOT/MarathonRecompLib/ppc/ppc_recomp_shared.h" -nt "$PPC_PCH" ]]; then
        cc "${CXXFLAGS[@]}" -x c++-header "$ROOT/MarathonRecompLib/ppc/ppc_recomp_shared.h" -o "$PPC_PCH"
    fi
    CMDS=()
    for src in "${QUEUE_CXX_PPCPCH[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" && "$obj" -nt "$PPC_PCH" ]] && continue
        CMDS+=("cc \"\${CXXFLAGS[@]}\" -include-pch \"$PPC_PCH\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

if (( ${#QUEUE_CXX_STDPCH[@]} > 0 )); then
    STD_PCH="$BUILD_DIR/stdafx.h.pch"
    if [[ "${CLEAN_BUILD:-0}" == "1" || ! -f "$STD_PCH" || "$ROOT/MarathonRecomp/stdafx.h" -nt "$STD_PCH" ]]; then
        cc "${CXXFLAGS[@]}" -x c++-header "$ROOT/MarathonRecomp/stdafx.h" -o "$STD_PCH"
    fi
    CMDS=()
    for src in "${QUEUE_CXX_STDPCH[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" && "$obj" -nt "$STD_PCH" ]] && continue
        CMDS+=("cc \"\${CXXFLAGS[@]}\" -include-pch \"$STD_PCH\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

echo "==> [7/7] Linking PS5 PIE ELF with RADV (libvulkan_radeon.ps5.a) + PacBrew (SDL2, SDL2_mixer, vorbis, ogg, zstd)..."
source "$ROOT/tools/radv-link.sh"
radv_link_recipe "$ROOT" "$SDK_ROOT" "$RADV_ARCHIVE" || exit 2

"$SDK_ROOT/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --Map="$BUILD_DIR/llvm-pie.map" \
    --version-script "$NATIVE_DIR/app-symbols.map" --exclude-libs=ALL \
    -e _start -o "$BUILD_DIR/llvm-pie.elf" \
    "${OBJECTS[@]}" \
    --start-group \
    "$PACBREW_SYSROOT/lib/libSDL2_mixer.a" \
    "$PACBREW_SYSROOT/lib/libvorbisfile.a" \
    "$PACBREW_SYSROOT/lib/libvorbis.a" \
    "$PACBREW_SYSROOT/lib/libogg.a" \
    "$PACBREW_SYSROOT/lib/libSDL2.a" \
    "$PACBREW_SYSROOT/lib/libiconv.a" \
    "$PACBREW_SYSROOT/lib/libzstd.a" \
    "${FFMPEG_LIBS[@]}" \
    --end-group \
    "$STUB_DIR/libSceAgc.so" "$STUB_DIR/libSceAgcDriver.so" \
    "${radv_link_inputs[@]}" \
    --as-needed "$SDK_ROOT"/target/lib/*.so

echo "==> [package] Converting and signing PS5 eboot.bin ($TITLE_ID)..."
"$NATIVE_TOOL" link --in "$BUILD_DIR/llvm-pie.elf" --out "$BUILD_DIR/eboot.elf" \
    --stub-dir "$SDK_ROOT/target/lib" --stub "$STUB_DIR/libSceAgc.so" \
    --stub "$STUB_DIR/libSceAgcDriver.so" --module-sdk "$MODULE_SDK" \
    --companion-sdk "$COMPANION_SDK" --file-name eboot.elf

APP_DIR="$DIST_DIR/$TITLE_ID"
rm -rf -- "$APP_DIR"
mkdir -p "$APP_DIR/sce_sys" "$APP_DIR/sce_module"
"$NATIVE_TOOL" self --sign --in "$BUILD_DIR/eboot.elf" --out "$APP_DIR/eboot.bin" --magic "$FSELF_MAGIC"
python3 - "$TITLE_ID" "$CONCEPT_ID" "$APP_TITLE" "$APP_DIR/sce_sys/param.json" <<'PY'
import json, sys

title_id, concept_id, title, output = sys.argv[1:]
param = {
    "ageLevel": {"default": 0},
    "applicationCategoryType": 0,
    "applicationDrmType": "free",
    "attribute": 0,
    "attribute2": 0,
    "attribute3": 0,
    "conceptId": concept_id,
    "contentBadgeType": 1,
    "contentId": f"UP9000-{title_id}_00-MARATHONRECOMP",
    "contentVersion": "01.000.000",
    "downloadDataSize": 256,
    "gameIntent": {"permittedIntents": [{"intentType": "launchActivity"}]},
    "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": title}},
    "masterVersion": "01.00",
    "pubtools": {"toolVersion": "2.00"},
    "requiredSystemSoftwareVersion": "0x0000000000000000",
    "sdkVersion": "0x0000000000000000",
    "titleId": title_id,
    "versionFileUri": "",
}
with open(output, "w", encoding="utf-8") as stream:
    json.dump(param, stream, indent=2)
    stream.write("\n")
PY
for asset in icon0.png pic0.dds pic1.dds snd0.at9; do
    if [[ -s "$ROOT/sce_sys/$asset" ]]; then
        cp -f "$ROOT/sce_sys/$asset" "$APP_DIR/sce_sys/$asset"
    fi
done
# Validate the files actually staged for packaging; .source.* files remain in
# the project sce_sys/ but are intentionally not copied into the PS5 app.
bash "$ROOT/tools/validate-assets.sh" "$APP_DIR/sce_sys"

[[ -f "$ROOT/runtime/libc.prx" ]] || bash "$ROOT/tools/rebuild-libc.sh"
cp "$ROOT/runtime/libc.prx" "$APP_DIR/sce_module/libc.prx"

# Include the supplied game tree in the installed app's read-only /app0/ressources path.
if [[ -n "$GAME_INPUT_ROOT" && -d "$GAME_INPUT_ROOT" ]]; then
    mkdir -p "$APP_DIR/ressources"
    if [[ -d "$GAME_INPUT_ROOT/game" ]]; then
        echo "==> [package] Staging $GAME_INPUT_ROOT into $APP_DIR/ressources/..."
        cp -a "$GAME_INPUT_ROOT/." "$APP_DIR/ressources/"
    else
        echo "==> [package] Staging game folder $GAME_INPUT_ROOT into $APP_DIR/ressources/game/..."
        mkdir -p "$APP_DIR/ressources/game"
        cp -a "$GAME_INPUT_ROOT/." "$APP_DIR/ressources/game/"
    fi
fi

"$NATIVE_TOOL" self --inspect --file "$APP_DIR/sce_module/libc.prx"
"$NATIVE_TOOL" self --inspect --file "$APP_DIR/eboot.bin"

rm -rf -- "$PKG_DIR/$TITLE_ID"
mkdir -p "$PKG_DIR"
cp -al "$APP_DIR" "$PKG_DIR/$TITLE_ID" 2>/dev/null || cp -a "$APP_DIR" "$PKG_DIR/$TITLE_ID"
printf 'Build complete:\n  dist: %s (%s bytes)\n  pkg:  %s\n' \
    "$APP_DIR" "$(stat -c %s "$APP_DIR/eboot.bin")" "$PKG_DIR/$TITLE_ID"

