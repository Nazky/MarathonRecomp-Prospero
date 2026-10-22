#!/usr/bin/env bash
# Generate the ignored version.cpp/version.h files for the manual PS5 build.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}" python3 "$ROOT/tools/generate-version.py"
