#!/usr/bin/env bash
# MarathonRecomp - Build host-side recompilation and resource tools.
# Fetch the pinned Linux x64 DXC headers, runtime, and compiler CLI into the external cache.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_BIN="$ROOT/tools/bin"
XENON_DIR="$ROOT/tools/XenonRecomp"
XENOS_DIR="$ROOT/tools/XenosRecomp"
DXC_COMMIT="737ac9f51c3d06f0cd1ee7e14dc065bafd52310d"
if [[ -z "${HOST_CXX:-${CXX:-}}" ]]; then
    HAVE_CLANGXX=0
    for clang_major in {30..18}; do
        if command -v "clang++-$clang_major" >/dev/null; then HAVE_CLANGXX=1; break; fi
    done
    command -v clang++ >/dev/null && HAVE_CLANGXX=1
    if (( HAVE_CLANGXX == 0 )); then
        echo "ERROR: XenonRecomp requires Clang++; install clang or set HOST_CXX/CXX." >&2
        exit 2
    fi
fi
DXC_DIR="${DXC_ROOT:-}"
if [[ -z "$DXC_DIR" ]]; then
    if [[ -f "$XENOS_DIR/thirdparty/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
        DXC_DIR="$XENOS_DIR/thirdparty/dxc-bin"
    elif [[ -f "$ROOT/.deps/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
        DXC_DIR="$ROOT/.deps/dxc-bin"
    elif [[ -f "/opt/ps5-cache/refs/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
        DXC_DIR="/opt/ps5-cache/refs/dxc-bin"
    else
        DXC_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/MarathonRecomp/dxc-bin"
    fi
fi

SYSTEM_DXC=""
if [[ -n "${DXC:-}" ]]; then
    if [[ -f "$DXC" && -x "$DXC" ]]; then
        SYSTEM_DXC="$DXC"
    else
        SYSTEM_DXC="$(command -v "$DXC" 2>/dev/null || true)"
    fi
else
    SYSTEM_DXC="$(command -v dxc 2>/dev/null || true)"
fi
NEED_DXC_FETCH=0
[[ -f "$DXC_DIR/lib/x64/libdxcompiler.so" ]] || NEED_DXC_FETCH=1
[[ -d "$DXC_DIR/inc" ]] || NEED_DXC_FETCH=1
if [[ -z "$SYSTEM_DXC" && ( ! -f "$DXC_DIR/bin/x64/dxc-linux" || ! -x "$DXC_DIR/bin/x64/dxc-linux" ) ]]; then
    NEED_DXC_FETCH=1
fi

if (( NEED_DXC_FETCH )); then
    command -v git >/dev/null || { echo "missing required command: git (to fetch DXC)" >&2; exit 2; }
    echo "==> [host-tools] Fetching pinned Linux x64 DXC runtime/compiler into $DXC_DIR..." >&2
    mkdir -p "$(dirname "$DXC_DIR")"
    DXC_TMP="$(mktemp -d /tmp/marathon-dxc.XXXXXX)"
    trap 'rm -rf -- "$DXC_TMP"' EXIT
    git clone --filter=blob:none --no-checkout --depth 1 \
        https://github.com/renderbag/dxc-bin.git "$DXC_TMP/repo" >&2
    git -C "$DXC_TMP/repo" fetch --filter=blob:none --depth=1 origin "$DXC_COMMIT" >&2
    git -C "$DXC_TMP/repo" sparse-checkout init --no-cone
    printf '/bin/x64/\n/inc/\n/lib/x64/\n' > "$DXC_TMP/sparse-checkout"
    git -C "$DXC_TMP/repo" sparse-checkout set --no-cone --stdin < "$DXC_TMP/sparse-checkout"
    git -C "$DXC_TMP/repo" checkout --detach FETCH_HEAD >&2
    mkdir -p "$DXC_DIR/bin" "$DXC_DIR/lib"
    cp -a "$DXC_TMP/repo/inc" "$DXC_DIR/"
    cp -a "$DXC_TMP/repo/lib/x64" "$DXC_DIR/lib/"
    cp -a "$DXC_TMP/repo/bin/x64" "$DXC_DIR/bin/"
    chmod +x "$DXC_DIR/bin/x64/dxc-linux"
    rm -rf -- "$DXC_TMP"
    trap - EXIT
fi

mkdir -p "$TOOLS_BIN"
chmod +x "$TOOLS_BIN/"* 2>/dev/null || true
# Match UnleashedRecomp's default for the small tools: GCC with its paired
# libstdc++. Clang 18 cannot consume some GCC 16 libstdc++ headers, so probe
# the selected recompiler compiler and use libc++ or a newer Clang when needed.
HOST_HELPER_CXX="${HOST_HELPER_CXX:-$(command -v g++ || command -v c++ || command -v clang++-18 || command -v clang++)}"
CC="${HOST_CC:-${CC:-$(command -v clang-18 || command -v clang || command -v gcc || command -v cc)}}"
RECOMP_CXX_REQUESTED="${HOST_CXX:-${CXX:-}}"
RECOMP_CXX=""
RECOMP_STDLIB_FLAGS=()
RECOMP_STDLIB_LINK_FLAGS=()
CXX_CANDIDATES=()
if [[ -n "$RECOMP_CXX_REQUESTED" ]]; then
    CXX_CANDIDATES+=("$RECOMP_CXX_REQUESTED")
fi
for clang_major in {30..18}; do
    candidate="$(command -v "clang++-$clang_major" 2>/dev/null || true)"
    if [[ -n "$candidate" ]]; then CXX_CANDIDATES+=("$candidate"); fi
done
candidate="$(command -v clang++ 2>/dev/null || true)"
if [[ -n "$candidate" ]]; then CXX_CANDIDATES+=("$candidate"); fi

CXX_PROBE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/marathon-cxx-probe.XXXXXX")"
CXX_PROBE_SOURCE="$CXX_PROBE_DIR/probe.cpp"
cat > "$CXX_PROBE_SOURCE" <<'CPP'
#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
int main() {
    std::vector<unsigned char> bytes;
    bytes.resize(2);
    std::string text;
    text += "probe";
    std::atomic<unsigned> value{1};
    return static_cast<int>(bytes.front()) + static_cast<int>(value.load()) +
           static_cast<int>(text.size());
}
CPP
probe_recompiler_cxx() {
    local compiler="$1"
    shift
    "$compiler" "$@" -std=c++20 "$CXX_PROBE_SOURCE" -o "$CXX_PROBE_DIR/probe" >/dev/null 2>&1
}
for candidate in "${CXX_CANDIDATES[@]}"; do
    if ! "$candidate" --version 2>&1 | grep -qi 'clang'; then
        continue
    fi
    if probe_recompiler_cxx "$candidate"; then
        RECOMP_CXX="$candidate"
        break
    elif probe_recompiler_cxx "$candidate" -stdlib=libc++; then
        RECOMP_CXX="$candidate"
        RECOMP_STDLIB_FLAGS=(-stdlib=libc++)
        break
    elif "$candidate" -std=c++20 -stdlib=libc++ "$CXX_PROBE_SOURCE" \
        -o "$CXX_PROBE_DIR/probe" -lc++abi >/dev/null 2>&1; then
        RECOMP_CXX="$candidate"
        RECOMP_STDLIB_FLAGS=(-stdlib=libc++)
        RECOMP_STDLIB_LINK_FLAGS=(-lc++abi)
        break
    fi
done
rm -rf -- "$CXX_PROBE_DIR"
if [[ -z "$RECOMP_CXX" ]]; then
    echo "ERROR: no compatible Clang++ standard library found." >&2
    echo "Clang 18 is incompatible with GCC 16 libstdc++ (including its __builtin_popcountg requirement); use Clang 19+ or install libc++/libc++abi development files, then set HOST_CXX." >&2
    exit 2
fi
if (( ${#RECOMP_STDLIB_FLAGS[@]} > 0 )); then
    if (( ${#RECOMP_STDLIB_LINK_FLAGS[@]} > 0 )); then
        echo "==> [host-tools] Helper C++: $HOST_HELPER_CXX; recompiler C++: $RECOMP_CXX -stdlib=libc++ -lc++abi" >&2
    else
        echo "==> [host-tools] Helper C++: $HOST_HELPER_CXX; recompiler C++: $RECOMP_CXX -stdlib=libc++" >&2
    fi
else
    echo "==> [host-tools] Helper C++: $HOST_HELPER_CXX; recompiler C++: $RECOMP_CXX (system standard library)" >&2
fi

for required in \
    "$XENON_DIR/XenonRecomp/main.cpp" \
    "$XENON_DIR/XenonRecomp/pch.h" \
    "$XENON_DIR/XenonUtils/ppc_context.h" \
    "$XENOS_DIR/XenosRecomp/main.cpp" \
    "$XENOS_DIR/XenosRecomp/shader_common.h"; do
    if [[ ! -s "$required" ]]; then
        echo "ERROR: required host-tool source is missing: $required" >&2
        echo "Clone MarathonRecomp with --recurse-submodules, or initialize its XenonRecomp/XenosRecomp submodules." >&2
        exit 2
    fi
done

needs_rebuild() {
    local binary="$1" marker="$2" path
    shift 2
    [[ ! -x "$binary" || ! -f "$marker" ]] && return 0
    for path in "$@"; do
        [[ -e "$path" ]] || continue
        if [[ -d "$path" ]]; then
            [[ -n "$(find "$path" -type f -newer "$marker" -print -quit 2>/dev/null)" ]] && return 0
        elif [[ "$path" -nt "$marker" ]]; then
            return 0
        fi
    done
    return 1
}

if [[ ! -x "$TOOLS_BIN/x_decompress" ]]; then
    echo "==> [host-tools] Building x_decompress..." >&2
    BUILD_HOST="$ROOT/build/host-xdec"
    mkdir -p "$BUILD_HOST"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" \
        -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/lzxd.c" -o "$BUILD_HOST/lzxd.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" \
        -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/system.c" -o "$BUILD_HOST/system.o"
    "$HOST_HELPER_CXX" -std=c++20 -O2 \
        -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" \
        "$ROOT/tools/x_decompress/x_decompress.cpp" \
        "$BUILD_HOST/lzxd.o" "$BUILD_HOST/system.o" \
        -o "$TOOLS_BIN/x_decompress"
    rm -rf "$BUILD_HOST"
fi

if [[ ! -x "$TOOLS_BIN/file_to_c" ]]; then
    echo "==> [host-tools] Building file_to_c..." >&2
    "$HOST_HELPER_CXX" -std=c++20 -O2 \
        -I"$XENOS_DIR/thirdparty/zstd/lib" \
        "$ROOT/tools/file_to_c/file_to_c.cpp" \
        -lzstd \
        -o "$TOOLS_BIN/file_to_c"
fi

if [[ ! -x "$TOOLS_BIN/u8extract" || "$ROOT/tools/u8extract/u8extract.cpp" -nt "$TOOLS_BIN/u8extract" ]]; then
    echo "==> [host-tools] Building u8extract..." >&2
    "$HOST_HELPER_CXX" -std=c++17 -O2 \
        "$ROOT/tools/u8extract/u8extract.cpp" \
        -lz \
        -o "$TOOLS_BIN/u8extract"
fi

XENON_MARKER="$TOOLS_BIN/.xenonrecomp-marathon"
if needs_rebuild "$TOOLS_BIN/XenonRecomp" "$XENON_MARKER" \
    "$XENON_DIR/XenonRecomp" "$XENON_DIR/XenonAnalyse" "$XENON_DIR/XenonUtils" \
    "$XENON_DIR/thirdparty/disasm" "$XENON_DIR/thirdparty/fmt" \
    "$XENON_DIR/thirdparty/tomlplusplus" "$XENON_DIR/thirdparty/xxHash" \
    "$XENON_DIR/thirdparty/libmspack" "$XENON_DIR/thirdparty/simde" \
    "$XENON_DIR/thirdparty/tiny-AES-c" "$XENON_DIR/thirdparty/TinySHA1"; then
    echo "==> [host-tools] Building XenonRecomp..." >&2
    BUILD_HOST="$ROOT/build/host-xenon"
    mkdir -p "$BUILD_HOST"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/disasm" -c "$XENON_DIR/thirdparty/disasm/disasm.c" -o "$BUILD_HOST/disasm.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/disasm" -c "$XENON_DIR/thirdparty/disasm/ppc-dis.c" -o "$BUILD_HOST/ppc-dis.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/tiny-AES-c" -c "$XENON_DIR/thirdparty/tiny-AES-c/aes.c" -o "$BUILD_HOST/aes.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/lzxd.c" -o "$BUILD_HOST/lzxd.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/system.c" -o "$BUILD_HOST/system.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/xxHash" -c "$XENON_DIR/thirdparty/xxHash/xxhash.c" -o "$BUILD_HOST/xxhash.o"
    XENON_CXXFLAGS=(
        -std=c++20 -O2
        "${RECOMP_STDLIB_FLAGS[@]}"
        -Wno-switch -Wno-unused-variable -Wno-pointer-arith -fms-extensions
        -DXENON_RECOMP_USE_ALIAS -D_CRT_SECURE_NO_WARNINGS
        -include "$XENON_DIR/XenonRecomp/pch.h"
        -I"$XENON_DIR/XenonRecomp"
        -I"$XENON_DIR/XenonAnalyse"
        -I"$XENON_DIR/XenonUtils"
        -I"$XENON_DIR/thirdparty/disasm"
        -I"$XENON_DIR/thirdparty/fmt/include"
        -I"$XENON_DIR/thirdparty/tomlplusplus/include"
        -I"$XENON_DIR/thirdparty/xxHash"
        -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack"
        -I"$XENON_DIR/thirdparty/tiny-AES-c"
        -I"$XENON_DIR/thirdparty/TinySHA1"
        -I"$XENON_DIR/thirdparty/simde"
    )
    idx=0
    for cpp_src in \
        "$XENON_DIR"/XenonRecomp/*.cpp \
        "$XENON_DIR/XenonAnalyse/function.cpp" \
        "$XENON_DIR"/XenonUtils/*.cpp \
        "$XENON_DIR/thirdparty/fmt/src/format.cc" \
        "$XENON_DIR/thirdparty/fmt/src/os.cc"
    do
        idx=$((idx + 1))
        TMPDIR="$BUILD_HOST" "$RECOMP_CXX" "${XENON_CXXFLAGS[@]}" -c "$cpp_src" -o "$BUILD_HOST/cpp_${idx}.o"
    done
    TMPDIR="$BUILD_HOST" "$RECOMP_CXX" "${RECOMP_STDLIB_FLAGS[@]}" -std=c++20 -O2 \
        "$BUILD_HOST"/*.o -lpthread "${RECOMP_STDLIB_LINK_FLAGS[@]}" -o "$TOOLS_BIN/XenonRecomp"
    rm -rf "$BUILD_HOST"
    touch "$XENON_MARKER"
fi

XENOS_MARKER="$TOOLS_BIN/.xenosrecomp-marathon"
if needs_rebuild "$TOOLS_BIN/XenosRecomp" "$XENOS_MARKER" \
    "$XENOS_DIR/XenosRecomp" "$XENOS_DIR/thirdparty/fmt" \
    "$XENOS_DIR/thirdparty/smol-v/source" "$XENOS_DIR/thirdparty/xxHash"; then
    echo "==> [host-tools] Building XenosRecomp (with -DMARATHON_RECOMP)..." >&2
    BUILD_HOST="$ROOT/build/host-xenos"
    mkdir -p "$BUILD_HOST"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/xxHash" -c "$XENON_DIR/thirdparty/xxHash/xxhash.c" -o "$BUILD_HOST/xxhash.o"
    XENOS_CXXFLAGS=(
        -std=c++20 -O2
        "${RECOMP_STDLIB_FLAGS[@]}"
        -Wno-switch -Wno-unused-variable -Wno-pointer-arith -fms-extensions
        -DMARATHON_RECOMP
        -include "$XENOS_DIR/XenosRecomp/pch.h"
        -I"$XENOS_DIR/XenosRecomp"
        -I"$XENOS_DIR/thirdparty/smol-v/source"
        -I"$XENOS_DIR/thirdparty/zstd/lib"
        -I"$XENOS_DIR/thirdparty/dxc-bin/inc"
        -I"$DXC_DIR/inc"
        -I"$XENON_DIR/thirdparty/fmt/include"
        -I"$XENON_DIR/thirdparty/xxHash"
    )
    idx=0
    for cpp_src in \
        "$XENOS_DIR/XenosRecomp/main.cpp" \
        "$XENOS_DIR/XenosRecomp/dxc_compiler.cpp" \
        "$XENOS_DIR/XenosRecomp/shader_recompiler.cpp" \
        "$XENOS_DIR/thirdparty/smol-v/source/smolv.cpp" \
        "$XENON_DIR/thirdparty/fmt/src/format.cc" \
        "$XENON_DIR/thirdparty/fmt/src/os.cc"
    do
        idx=$((idx + 1))
        TMPDIR="$BUILD_HOST" "$RECOMP_CXX" "${XENOS_CXXFLAGS[@]}" -c "$cpp_src" -o "$BUILD_HOST/cpp_${idx}.o"
    done
    TMPDIR="$BUILD_HOST" "$RECOMP_CXX" "${RECOMP_STDLIB_FLAGS[@]}" -std=c++20 -O2 \
        "$BUILD_HOST"/*.o \
        -L"$DXC_DIR/lib/x64" \
        -Wl,-rpath,"$DXC_DIR/lib/x64:\$ORIGIN/../XenosRecomp/thirdparty/dxc-bin/lib/x64" \
        -ldxcompiler -lzstd -lpthread "${RECOMP_STDLIB_LINK_FLAGS[@]}" \
        -o "$TOOLS_BIN/XenosRecomp"
    rm -rf "$BUILD_HOST"
    touch "$XENOS_MARKER"
fi

echo "DXC_LIB_DIR=$DXC_DIR/lib/x64"
