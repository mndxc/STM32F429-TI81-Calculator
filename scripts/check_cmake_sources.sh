#!/usr/bin/env bash
# Verify every App/Src/*.c file is listed in the root CMakeLists.txt.
# Run locally or in CI before the firmware build to catch missing entries early.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CMAKELISTS="$ROOT/CMakeLists.txt"
SRCDIR="$ROOT/App/Src"

missing=()
for f in "$SRCDIR"/*.c; do
    base=$(basename "$f")
    if ! grep -qF "App/Src/$base" "$CMAKELISTS"; then
        missing+=("$base")
    fi
done

if [ ${#missing[@]} -gt 0 ]; then
    echo "ERROR: App/Src source files not listed in CMakeLists.txt:"
    for m in "${missing[@]}"; do
        echo "  $m"
    done
    exit 1
fi

echo "OK: all $(ls "$SRCDIR"/*.c | wc -l | tr -d ' ') App/Src/*.c files are listed in CMakeLists.txt"
