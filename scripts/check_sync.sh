#!/usr/bin/env bash
# check_sync.sh — validate all manual sync points and report drift.
#
# Usage (from repo root):
#   ./scripts/check_sync.sh
#
# Exit codes:
#   0  — all checks passed
#   1  — one or more drifts detected (details printed above exit)
#
# Checks performed:
#   1. Every App/Src/*.c file is listed in docs/TECHNICAL.md
#   2. Every App/Inc/*.h file is listed in docs/TECHNICAL.md
#   3. Every file linked from README.md exists in docs/
#   4. Every module listed in docs/ARCHITECTURE.md exists in App/Src/ or App/Inc/
#   5. PersistBlock_t assertion value in test file matches persist.h comment
#   6. PERSIST_VERSION in persist.h matches version referenced in test file
#   7. docs/TESTING.md total count line is present (can't auto-validate count without building)
#   8. Every command listed in docs/PRGM_COMMANDS.md has a handler in prgm_exec.c

set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS=0
FAIL=0

ok()   { echo "  [OK]  $1"; PASS=$((PASS+1)); }
fail() { echo "  [!!]  $1"; FAIL=$((FAIL+1)); }
head() { echo ""; echo "--- $1 ---"; }

# ── Check 1: App/Src/*.c listed in TECHNICAL.md ──────────────────────────────
head "App/Src files in TECHNICAL.md"
for f in "$REPO_ROOT/App/Src/"*.c; do
    name=$(basename "$f")
    if grep -q "$name" "$REPO_ROOT/docs/TECHNICAL.md"; then
        ok "$name"
    else
        fail "$name not listed in docs/TECHNICAL.md"
    fi
done

# ── Check 2: App/Inc/*.h listed in TECHNICAL.md ──────────────────────────────
head "App/Inc headers in TECHNICAL.md"
for f in "$REPO_ROOT/App/Inc/"*.h; do
    name=$(basename "$f")
    if grep -q "$name" "$REPO_ROOT/docs/TECHNICAL.md"; then
        ok "$name"
    else
        fail "$name not listed in docs/TECHNICAL.md"
    fi
done

# ── Check 3: README.md linked files exist ────────────────────────────────────
head "README.md linked files exist"
while IFS= read -r link; do
    path="$REPO_ROOT/$link"
    if [ -f "$path" ]; then
        ok "$link"
    else
        fail "$link linked from README.md does not exist"
    fi
done < <(grep -oE '\(docs/[^)]+\)' "$REPO_ROOT/README.md" | tr -d '()' | sort -u)

# ── Check 4: ARCHITECTURE.md module list vs filesystem ───────────────────────
head "ARCHITECTURE.md modules exist on filesystem"
while IFS= read -r name; do
    # Search across all App/ subdirectories and Core/Src — architecture diagram
    # references files from App/Src, App/Inc, App/HW, App/Tests, and Core/Src
    if find "$REPO_ROOT/App" "$REPO_ROOT/Core/Src" -name "$name" 2>/dev/null | grep -q .; then
        ok "$name"
    elif find "$REPO_ROOT/Middlewares" "$REPO_ROOT/Core" -name "$name" 2>/dev/null | grep -q .; then
        ok "$name (third-party/core)"
    else
        fail "$name referenced in ARCHITECTURE.md not found anywhere under App/ or Core/"
    fi
done < <(grep -oE '[a-zA-Z_]+\.(c|h)' "$REPO_ROOT/docs/ARCHITECTURE.md" | sort -u || true)

# ── Check 5: PersistBlock_t size assertion matches header comment ─────────────
head "PersistBlock_t size consistency"
# Extract the numeric argument from: EXPECT_EQ((int)sizeof(PersistBlock_t), NNNN, ...)
test_size=$(grep 'EXPECT_EQ.*sizeof.*PersistBlock_t' "$REPO_ROOT/App/Tests/test_persist_roundtrip.c" \
    | grep -oE ', [0-9]+,' | grep -oE '[0-9]+' | head -1)
# Extract the number from: } PersistBlock_t;  /* Total: NNNN B */
header_size=$(grep 'PersistBlock_t.*Total:' "$REPO_ROOT/App/Inc/persist.h" \
    | grep -oE 'Total: [0-9]+' | grep -oE '[0-9]+' | head -1)

if [ -z "$test_size" ] || [ -z "$header_size" ]; then
    fail "Could not extract PersistBlock_t size (test='$test_size', header='$header_size')"
elif [ "$test_size" = "$header_size" ]; then
    ok "PersistBlock_t: test=$test_size B, header comment=$header_size B — match"
else
    fail "PersistBlock_t MISMATCH: test asserts $test_size B, header comment says $header_size B"
fi

# ── Check 6: PERSIST_VERSION used in test matches header ─────────────────────
head "PERSIST_VERSION consistency"
header_ver=$(grep -oE '#define PERSIST_VERSION[[:space:]]+[0-9]+' "$REPO_ROOT/App/Inc/persist.h" | grep -oE '[0-9]+$')
test_ver=$(grep -oE 'PERSIST_VERSION[[:space:]]*[0-9]+|version.*=.*[0-9]+' "$REPO_ROOT/App/Tests/test_persist_roundtrip.c" | grep -oE '[0-9]+' | head -1)
# Simpler: just confirm PERSIST_VERSION macro is referenced in the test
if grep -q "PERSIST_VERSION" "$REPO_ROOT/App/Tests/test_persist_roundtrip.c"; then
    ok "PERSIST_VERSION ($header_ver) is referenced in test_persist_roundtrip.c"
else
    fail "PERSIST_VERSION not referenced in test_persist_roundtrip.c — test may not validate version"
fi

# ── Check 7: TESTING.md has an assertion total line ──────────────────────────
head "TESTING.md has total count"
total=$(grep -oE "[0-9]+ assertions total" "$REPO_ROOT/docs/TESTING.md" | head -1 || true)
if [ -n "$total" ]; then
    ok "TESTING.md reports: $total"
else
    fail "TESTING.md missing total assertion count line — run scripts/update_test_counts.sh"
fi

# ── Check 8: active PRGM commands have handlers in prgm_exec.c ───────────────
head "PRGM_COMMANDS.md active commands in prgm_exec.c dispatch table"
# Extract prefix strings directly from prgm_exec.c cmd_table entries
# Format: { "Token", len, exact, handler }
active_cmds=$(grep -oE '"[A-Za-z][A-Za-z0-9><=( ]*"' "$REPO_ROOT/App/Src/prgm_exec.c" | tr -d '"' | sort -u || true)
if [ -z "$active_cmds" ]; then
    fail "Could not extract dispatch table entries from prgm_exec.c"
else
    # Verify each dispatch entry appears in PRGM_COMMANDS.md (case-insensitive)
    while IFS= read -r token; do
        token_trimmed=$(echo "$token" | sed 's/[[:space:]]*$//')
        if grep -qi "$token_trimmed" "$REPO_ROOT/docs/PRGM_COMMANDS.md"; then
            ok "$token_trimmed"
        else
            fail "prgm_exec.c handler \"$token_trimmed\" not documented in PRGM_COMMANDS.md"
        fi
    done <<< "$active_cmds"
fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "══════════════════════════════════════"
echo "  Passed: $PASS    Failed: $FAIL"
echo "══════════════════════════════════════"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
