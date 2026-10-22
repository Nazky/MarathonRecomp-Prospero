#!/usr/bin/env bash
# Generate MarathonRecomp's embedded BIN2C resources using its CMake manifest.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
OUT_RES_DIR="$BUILD_DIR/res"
TOOLS_BIN="$ROOT/tools/bin"
FILE_TO_C="$TOOLS_BIN/file_to_c"

mkdir -p "$TOOLS_BIN"
if [[ ! -x "$FILE_TO_C" ]]; then
    bash "$ROOT/tools/build-host-tools.sh" >/dev/null
fi

RES_SRC_DIR="${MARATHON_RECOMP_RESOURCES:-}"
if [[ -z "$RES_SRC_DIR" ]]; then
    for candidate in \
        "$ROOT/MarathonRecompResources" \
        "$ROOT/.deps/MarathonRecompResources" \
        "/opt/ps5-cache/refs/MarathonRecompResources"; do
        if [[ -d "$candidate/images" && -d "$candidate/sounds" ]]; then
            RES_SRC_DIR="$candidate"
            break
        fi
    done
fi
if [[ -z "$RES_SRC_DIR" || ! -d "$RES_SRC_DIR" ]]; then
    echo "ERROR: MarathonRecompResources was not found or its submodule is empty." >&2
    echo "Clone with --recurse-submodules, run git submodule update --init --recursive, or set MARATHON_RECOMP_RESOURCES." >&2
    exit 2
fi

CMAKE_FILE="$ROOT/MarathonRecomp/CMakeLists.txt"
echo "==> [resources] Generating MarathonRecomp embedded resources into $OUT_RES_DIR..."
python3 - "$CMAKE_FILE" "$RES_SRC_DIR" "$OUT_RES_DIR" "$FILE_TO_C" <<'PY'
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

cmake_file, resource_root, output_root, file_to_c = sys.argv[1:]
with open(cmake_file, "r", encoding="utf-8") as source:
    content = source.read()

pattern = re.compile(
    r'BIN2C\(\s*TARGET_OBJ\s+\S+\s+SOURCE_FILE\s+'
    r'"\$\{RESOURCES_SOURCE_PATH\}/([^"]+)"\s+'
    r'DEST_FILE\s+"\$\{RESOURCES_OUTPUT_PATH\}/([^"]+)"\s+'
    r'ARRAY_NAME\s+"([^"]+)"'
    r'(?:\s+COMPRESSION_TYPE\s+"([^"]+)")?\s*\)'
)

entries = pattern.findall(content)
if not entries:
    raise SystemExit("no MarathonRecomp BIN2C resources were found in its CMakeLists.txt")

tasks = []
for source_relative, output_relative, array_name, compression in entries:
    source_path = os.path.join(resource_root, source_relative)
    output_base = os.path.join(output_root, output_relative)
    if not os.path.isfile(source_path):
        raise SystemExit(f"missing embedded resource: {source_path}")
    output_c, output_h = output_base + ".c", output_base + ".h"
    if (os.path.isfile(output_c) and os.path.isfile(output_h) and
            os.path.getmtime(output_c) >= os.path.getmtime(source_path) and
            os.path.getmtime(output_h) >= os.path.getmtime(source_path)):
        continue
    os.makedirs(os.path.dirname(output_base), exist_ok=True)
    tasks.append((file_to_c, source_path, array_name, compression or "none",
                  output_c, output_h))

if not tasks:
    print(f"    All {len(entries)} embedded resources are up to date.")
    raise SystemExit(0)

def generate(task):
    subprocess.run(list(task), check=True)

with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as executor:
    list(executor.map(generate, tasks))

print(f"    Generated {len(tasks)} resource pairs (.c + .h).")
PY
