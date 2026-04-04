#!/usr/bin/env bash
# list_modules.sh — list App/Src/*.c and App/Inc/*.h files with their
# first scope-comment line, formatted as a markdown table row.
#
# Usage (from repo root):
#   ./scripts/list_modules.sh
#
# Output is a ready-to-paste markdown table for docs/TECHNICAL.md
# Project Structure section. Compare against the existing listing to
# find additions or deletions.

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

extract_desc() {
    local file="$1"
    # Look for @brief tag first
    desc=$(grep -m1 "@brief" "$file" 2>/dev/null | sed 's/.*@brief[[:space:]]*//' | sed 's/^[[:space:]]*//')
    if [ -z "$desc" ]; then
        # Fall back to first non-empty comment line after the opening /**
        desc=$(awk '/\/\*\*/{found=1; next} found && /\*[[:space:]]+[A-Z]/{gsub(/^[[:space:]]*\*[[:space:]]*/,""); print; exit}' "$file" 2>/dev/null)
    fi
    echo "${desc:-(no description)}"
}

echo "### App/Src/ — $(ls "$REPO_ROOT/App/Src/"*.c 2>/dev/null | wc -l | tr -d ' ') files"
echo ""
echo "| File | Responsibility |"
echo "|---|---|"
for f in "$REPO_ROOT/App/Src/"*.c; do
    name=$(basename "$f")
    desc=$(extract_desc "$f")
    printf "| \`%s\` | %s |\n" "$name" "$desc"
done

echo ""
echo "### App/Inc/ — $(ls "$REPO_ROOT/App/Inc/"*.h 2>/dev/null | wc -l | tr -d ' ') headers"
echo ""
echo "| File | Responsibility |"
echo "|---|---|"
for f in "$REPO_ROOT/App/Inc/"*.h; do
    name=$(basename "$f")
    desc=$(extract_desc "$f")
    printf "| \`%s\` | %s |\n" "$name" "$desc"
done
