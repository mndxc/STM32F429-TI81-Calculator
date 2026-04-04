#!/usr/bin/env bash
# update_test_counts.sh — rebuild host tests, report per-suite assertion counts,
# and print the exact lines to paste into docs/TESTING.md.
#
# Usage (from repo root):
#   ./scripts/update_test_counts.sh
#
# What it does:
#   1. Rebuilds build-tests/
#   2. Runs each test executable and captures its assertion count
#   3. Prints a ready-to-paste diff for docs/TESTING.md
#
# After running, paste the printed counts into docs/TESTING.md and commit.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-tests"

echo "==> Building host tests..."
cmake -S "$REPO_ROOT/App/Tests" -B "$BUILD_DIR" --log-level=ERROR 2>&1 | grep -v "^--"
cmake --build "$BUILD_DIR" -- -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

echo ""
echo "==> Running suites..."

declare -A COUNTS=(
    [test_calc_engine]="Expression evaluation"
    [test_expr_util]="Buffer & cursor logic"
    [test_persist_roundtrip]="Serialization"
    [test_prgm_exec]="PRGM executor"
    [test_normal_mode]="handle_normal_mode dispatch"
    [test_param]="Parametric eval"
    [test_stat]="Statistical calculations"
)

TOTAL=0
declare -A RESULTS

for suite in test_calc_engine test_expr_util test_persist_roundtrip test_prgm_exec test_normal_mode test_param test_stat; do
    exe="$BUILD_DIR/$suite"
    if [ ! -f "$exe" ]; then
        echo "  WARNING: $suite not found at $exe — skipping"
        RESULTS[$suite]=0
        continue
    fi
    # Run and capture; count PASS lines or final summary
    output=$("$exe" 2>&1 || true)
    # Try "X passed" summary first; fall back to counting PASS lines
    count=$(echo "$output" | grep -oE "[0-9]+ passed" | grep -oE "[0-9]+" | tail -1)
    if [ -z "$count" ]; then
        count=$(echo "$output" | grep -c "^PASS" || echo "0")
    fi
    RESULTS[$suite]=$count
    TOTAL=$((TOTAL + count))
    printf "  %-30s %3d assertions\n" "$suite" "$count"
done

echo ""
echo "  Total: $TOTAL assertions across ${#RESULTS[@]} suites"

echo ""
echo "==> Paste these lines into docs/TESTING.md:"
echo ""
echo "  ctest --test-dir build-tests   # runs all ${#RESULTS[@]} suites ($TOTAL assertions total)"
echo ""
for suite in test_calc_engine test_expr_util test_persist_roundtrip test_prgm_exec test_normal_mode test_param test_stat; do
    desc="${COUNTS[$suite]}"
    n="${RESULTS[$suite]:-0}"
    printf "  ./build-tests/%-30s  # %s (%d tests)\n" "$suite" "$desc" "$n"
done
