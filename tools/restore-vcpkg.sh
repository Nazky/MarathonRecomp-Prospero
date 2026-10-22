#!/usr/bin/env bash
# Restore the optional desktop vcpkg checkout from its workspace archive.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ARCHIVE="$ROOT/thirdparty/vcpkg.snapshot.tar.xz"
DEST="$ROOT/thirdparty/vcpkg"

if [[ -f "$DEST/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "vcpkg is already restored at $DEST"
    exit 0
fi
if [[ ! -s "$ARCHIVE" ]]; then
    echo "ERROR: missing vcpkg snapshot archive: $ARCHIVE" >&2
    exit 2
fi
mkdir -p "$ROOT/thirdparty"
tar -xJf "$ARCHIVE" -C "$ROOT/thirdparty"
[[ -f "$DEST/scripts/buildsystems/vcpkg.cmake" ]] || {
    echo "ERROR: vcpkg archive did not contain scripts/buildsystems/vcpkg.cmake" >&2
    exit 2
}
echo "Restored vcpkg at $DEST"
