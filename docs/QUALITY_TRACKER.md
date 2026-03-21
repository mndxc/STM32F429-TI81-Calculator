# Project Quality Tracker

Tracks the results of periodic professional quality reviews and outstanding improvement items.
Update this file after each review pass or when an item is resolved.

**Last reviewed:** 2026-03-21 (second pass — static analysis of all App/ source files)
**Reviewer:** Claude Code (claude-sonnet-4-6) via full codebase static analysis

---

## Overall Assessment

> **Verdict: Strong personal/research project. Not yet production-ready.**
>
> Exceptional documentation and hardware-correctness. The expression evaluator, RTOS integration,
> and FLASH handling show genuine embedded expertise. The main gaps are architectural: one
> oversized file, no automated tests, and scattered static state.

**Estimate for production readiness:** 82–88% (up from 80–88%; gains from P9 resolution — `mat_det` confirmed iterative, not recursive)

---

## Scorecard

| Dimension | Rating | Last changed |
|---|---|---|
| Documentation | A+ | 2026-03-20 |
| API / header design | A | 2026-03-20 |
| Memory safety & FLASH handling | A | 2026-03-20 |
| RTOS integration | A | 2026-03-20 |
| Error handling | A- | 2026-03-20 |
| Naming conventions | B+ | 2026-03-20 |
| Code organisation | B- | 2026-03-20 |
| Function complexity | C+ | 2026-03-20 |
| Magic numbers / constants | A- | 2026-03-21 |
| Testing | B+ | 2026-03-21 |

---

## Strengths (do not regress)

### Documentation (A+)
`CLAUDE.md` is exceptional — feature status, architectural decisions, gotchas, known issues, PCB
notes, and next-session priorities all clearly maintained. Most professional firmware teams do not
write this well. `README.md`, `GETTING_STARTED.md`, and `TECHNICAL.md` provide clean onboarding.

### API and header design (A)
- Include guards correctly named (`GRAPH_MODULE_H` vs `GRAPH_H` collision avoided by design)
- Extern declarations match implementations across all modules
- Module prefixes consistent: `Calc_*`, `Graph_*`, `Persist_*`, `Keypad_*`
- No circular dependencies detected

### Embedded-specific correctness (A)
- FLASH erase placed in `.RamFunc`; LTDC/SDRAM disable ordered correctly before Stop mode
- `_Static_assert` used for `PersistBlock_t` word-alignment
- `volatile` correctly applied to `g_sleeping` ISR flag
- Checksums + magic number + version on the persist block
- ISR-safe queue use (`xQueueSendFromISR`)
- `prgm.c` correctly mirrors `persist.c` pattern: CCMRAM storage, `.RamFunc` erase/write, magic/version/checksum

### RTOS integration (A)
Mutex guards on all LVGL calls are consistent. The `cursor_timer_cb` deadlock case is explicitly
documented and avoided. Priority and ISR ceiling usage is correct.

### Error handling (A-)
`CalcResult_t` returns rich error context (`CalcError_t` enum + message string + matrix flag).
Bounds checking present throughout: token count, stack depth, matrix dimension limits.

---

## Issues and Improvement Items

Issues are ordered by priority. Mark resolved items with a date rather than deleting them —
the history is useful.

---

### P1 — No automated test suite
**Rating impact:** Testing = B+ (up from B, up from C, up from D)
**Files:** `App/Src/calc_engine.c`, `App/Src/expr_util.c`, `App/Src/persist.c`,
`App/Tests/test_calc_engine.c`, `App/Tests/test_expr_util.c`,
`App/Tests/test_persist_roundtrip.c`, `App/Tests/CMakeLists.txt`,
`.github/workflows/build.yml`

**Progress since last review (B+ score reached 2026-03-21):**
- **301 total tests across 3 executables** (up from 153 / 1 executable):
  - `test_calc_engine`: 153 tests / 20 groups (unchanged)
  - `test_expr_util`: 96 tests / 12 groups (new — see below)
  - `test_persist_roundtrip`: 52 tests / 5 groups (new — see below)
- **`expr_util.c` extracted** from `calculator_core.c`: 9 pure functions
  (`ExprUtil_Utf8CharSize`, `ExprUtil_Utf8ByteToGlyph`, `ExprUtil_MatrixTokenSizeBefore`,
  `ExprUtil_MatrixTokenSizeAt`, `ExprUtil_MoveCursorLeft`, `ExprUtil_MoveCursorRight`,
  `ExprUtil_InsertChar`, `ExprUtil_InsertStr`, `ExprUtil_DeleteAtCursor`,
  `ExprUtil_PrependAns`) with no LVGL/HAL/RTOS dependencies
- `calculator_core.c` static functions now delegate to `ExprUtil_*` (thin wrappers)
- **`persist.c` serialization round-trip test**: `Persist_Checksum` and `Persist_Validate`
  exposed as public API; `stm32f4xx_hal.h` and FLASH-touching code guarded with
  `#ifndef HOST_TEST`; 52 tests verify checksum stability, valid/invalid block detection,
  field readback, and struct size/alignment
- CI `host-tests` job updated to build and run all three executables; gcovr filter
  extended to cover `expr_util.c` and `persist.c`
- **Also discovered:** `PersistBlock_t` header comment said 856 B; actual size is 860 B
  (corrected in both `persist.h` and the round-trip test)

**Progress since last review (B score reached 2026-03-21):**
- 153 tests across 20 groups (up from 103 / 14 groups)
- New groups: matrix row ops (`rowSwap`, `row+`, `*row`, `*row+`), matrix subtraction and
  scalar scaling, ANS-as-matrix-reference (`ans_is_matrix=true`), element-wise `round([M],n)`,
  boundary cases (`det` non-square, factorial n=0/negative/large, dimension mismatches),
  tokenizer coverage (pi literal, UTF-8 π, `e` constant, implicit multiplication, T-after-paren,
  COMMA handler loop body, comma-outside-function syntax error)
- `gcov --coverage` instrumentation added to `App/Tests/CMakeLists.txt` via `-DCOVERAGE=ON`
- **Branch coverage: 80.28% of 644 branches taken** (crosses the 80% B-score threshold)
  Remaining 6 untaken branches are unreachable: 4 are `while(0)` loop-back branches from
  `RPNERR` macro expansion, 2 are token-overflow guards inside error paths
- **CI pipeline: `host-tests` job added to `.github/workflows/build.yml`** — builds tests
  with coverage, runs them, and prints gcov branch summary on every push/PR

**Build and run:**
```bash
cmake -S App/Tests -B build/tests && cmake --build build/tests
./build/tests/test_calc_engine   # exits 0 on full pass
```

**Improvement path to production (A) rating:**

| Target | What is required |
|--------|-----------------|
| **C** (current) | 103 tests, calc_engine happy paths and main errors |
| **B** | + gcov branch coverage >80% on `calc_engine.c`; remaining matrix row-ops and boundary tests; CI pipeline (GitHub Actions) |
| **B+** | + pure logic extracted from `calculator_core.c` and tested on host (UTF-8 cursor, expr manipulation, history); `persist.c` serialization round-trip |
| **A** | + property-based invariant tests (e.g. sin²+cos²=1 for 1000 x values); PRGM either extracted+tested or documented manual test protocol signed off |

**Remaining gaps within `calc_engine.c` (needed for B):**
- Matrix row operations: `rowSwap`, `row+`, `*row`, `*row+` — none tested
- `[A]-[B]`, `scalar*[A]`, `[A]*scalar` — matrix subtraction and scalar scaling
- `ans_is_matrix=true` path — `ANS` resolving as a matrix reference
- `round([A], n)` — element-wise matrix round
- Boundary conditions: expressions at `CALC_MAX_TOKENS-1` (63 tokens), stack depth near limit
- Factorial: n=0 and large n approaching overflow
- `det` on a non-square matrix → `CALC_ERR_DOMAIN`
- Branch coverage unknown — gcov measurement needed to find uncovered paths

**Gaps requiring extraction work (needed for B+):**
- `expr_insert_char`, `expr_insert_str`, `utf8_char_size`, `utf8_byte_to_glyph`, cursor
  movement logic in `calculator_core.c` are static functions with no HAL/LVGL dependency;
  could be moved to a pure utility file and tested on host
- `Calc_BuildPersistBlock` / `Calc_ApplyPersistBlock` in `calculator_core.c` / `persist.c`:
  round-trip test (build block → apply to fresh state → compare) validates save/restore
  correctness without touching FLASH

**Gaps deferred to hardware or PRGM extraction (needed for A):**
- PRGM execution backend (`prgm_execute_line`, `prgm_run_loop`) is coupled to LVGL display
  calls; needs extraction before host testing is practical. `prgm_manual_tests.md`
  contains a 28-item hardware test plan; a signed-off manual protocol is the interim path
- `Calc_FormatResult` scientific notation format not checked byte-for-byte (format differs
  between ARM nano.specs and host libc; only presence of 'e' is currently verified)

**Resolved:** Partially (2026-03-21 — B+ score achieved: 301 total tests across 3 executables, expr_util.c extracted with 96-test suite, persist round-trip 52-test suite, HAL guards added to persist)

---

### P2 — `calculator_core.c` too large (was 5,820 LOC; now 3,563 LOC — partially resolved)
**Rating impact:** Code organisation = B-, Function complexity = C+
**File:** `App/Src/calculator_core.c`

**Progress since initial review:** Significant extraction completed.
- `App/Src/ui_prgm.c` extracted (1,713 LOC) + `App/Inc/ui_prgm.h`
- `App/Src/ui_matrix.c` extracted (544 LOC) + `App/Inc/ui_matrix.h`
- `App/Src/prgm_exec.c` extracted (111 LOC) + `App/Inc/prgm_exec.h`
- `App/Inc/calc_internal.h` added (internal shared declarations)
- `calculator_core.c` reduced from 5,820 → 3,563 LOC (~39% reduction)

**Remaining work:** The file is still the dominant file. Large handlers still present:
- `handle_normal_mode` (287 lines, L3037) — largest handler; contains a big token dispatch switch
- `handle_yeq_mode` (207 lines, L2027)
- `handle_range_mode` (172 lines, L2234)
- `handle_zoom_factors_mode` (163 lines, L2470)
- `handle_zbox_mode` (94 lines, L2633)
- `handle_math_menu` (89 lines, L2850)
- `handle_trace_mode` (75 lines, L2727)
- `handle_zoom_mode` (64 lines, L2406)
- `handle_test_menu` (64 lines, L2939)
- `handle_mode_screen` (48 lines, L2802) — acceptable size

Industry standard: functions under 50–100 lines.

**Remaining recommendation:**
- Extract graph screen handlers (Y=, RANGE, ZOOM, ZBox, Trace) into `App/Src/graph_ui.c`
- Reduce remaining handlers to under 100 lines each

**Resolved:** Partially (2026-03-20 — ui_prgm, ui_matrix, prgm extracted; 2026-03-21 — further reduced 3,654 → 3,563)

---

### P3 — Scattered static state (~80 variables)
**Rating impact:** Code organisation = B-, Testing = D

~80 static variables in `calculator_core.c` — no central `CalcState_t` struct. State includes
expression buffer, cursor, history, mode, range fields, zoom factors, trace position, matrix
edit state, and PRGM executor state. This makes the code:
- Difficult to snapshot or serialize (the `PersistBlock_t` functions paper over this)
- Impossible to unit test in isolation
- Hard to reason about in GDB (requires many watchpoints)

**Recommendation:** Introduce a `CalcState_t` struct grouping logically related fields. Does not
need to happen all at once — group PRGM state first (already a natural boundary), then matrix
editor state, then expression/history state.

**Resolved:** —

---

### P4 — Magic numbers: colours and display dimensions
**Rating impact:** Magic numbers = B-
**Files:** `App/Src/calculator_core.c`, `App/Src/graph.c`, `App/Src/app_init.c`

Inline `lv_color_hex(0xFFFF00)`, `lv_color_hex(0xFFAA00)`, `lv_color_hex(0x888888)` etc.
appear throughout rather than being referenced by name. The scroll indicator X position was
hardcoded as `18`, silently changed to `4` in a later session — a named constant would have
made the intent and the change obvious.

Some constants exist (`COLOR_BG`, `COLOR_HISTORY_EXPR`) but coverage is inconsistent.

**Recommendation:** Add a palette header (`App/Inc/ui_palette.h`) defining all named colours
used across the project; replace inline hex literals.

**Resolved:** 2026-03-21 — `App/Inc/ui_palette.h` created with 14 named constants (COLOR_BLACK, COLOR_BG, COLOR_WHITE, COLOR_YELLOW, COLOR_AMBER, COLOR_GREY_LIGHT/MED/INACTIVE/DARK/TICK, COLOR_2ND, COLOR_ALPHA, COLOR_CURVE_Y1–Y4). All inline hex literals replaced in `calculator_core.c`, `graph.c`, `app_init.c`, `ui_prgm.c`, `ui_matrix.c`. Orphaned local `#define` block removed from `calculator_core.c`. One intentional exception: `0x00FF00` trace crosshair green in `graph.c` (single use, no semantic peer).

---

### P5 — Missing `const` on immutable data
**Rating impact:** Naming conventions = B+
**File:** `App/HW/Keypad/keypad_map.c`

`TI81_LookupTable` now correctly declared `const` — placed in `.rodata` (FLASH), saving RAM.
`TI81_LookupTable_Size` also marked `const`. A broader audit for other non-const read-only
data in the codebase is still warranted but the primary instance is resolved.

**Resolved:** 2026-03-20

---

### P6 — No `-Wall -Wextra` in build
**Rating impact:** Code organisation = B-
**File:** `CMakeLists.txt`

`-Wall -Wextra` is now present via `add_compile_options(-Wall -Wextra)` in `CMakeLists.txt`.
`-Werror` is not yet set — the next step is to resolve the current warning baseline and then
add `-Werror` to prevent regressions. Suppression for CubeMX/LVGL sources via a separate
`target_compile_options` block is still recommended for clean separation.

**Resolved:** Partially (2026-03-20 — -Wall -Wextra added; -Werror not yet set)

---

### P7 — Incomplete hardware wiring table in GETTING_STARTED.md
**Rating impact:** Documentation = A+ (risk to onboarding)
**File:** `docs/GETTING_STARTED.md`

**Partially resolved.** The STM32 GPIO side of the wiring table (A1–A7 = PE5/PE4/PE3/PE2/PB7/PB4/PB3,
B1–B8 = PG9/PD7/PC11/PC8/PC3/PA5/PG2/PG3, ON = PE6) is now documented and cross-referenced
against `keypad.h`.

**Remaining:** The physical correspondence between numbered pads on the TI-81 PCB and the
logical A-line/B-line names has not been manually traced and photographed. A new contributor
cannot replicate the physical wiring without this information or access to their own multimeter
and a donor board. Annotated photos are planned; the wiring table now contains a prominent
warning block calling this out and inviting community contributions.

**Resolved:** Partially (2026-03-20 — STM32 GPIO side complete; physical ribbon mapping pending)

---

### P8 — No runtime guard for float printf support
**Rating impact:** Low (build-time risk)
**File:** `CMakeLists.txt`, `App/Src/calc_engine.c`

50+ `snprintf` calls rely on `%.4g`/`%.6f`/`%.4e` formats working correctly. If `-u _printf_float`
is accidentally removed during a CMake refactor, all float output silently becomes empty strings —
no compiler or linker error is produced.

**Recommendation:** Add a startup assertion (or a boot-time test) that verifies
`snprintf(buf, 8, "%.2f", 1.5f)` produces `"1.50"`, and halt with an error LED pattern if not.

**Resolved:** —

---

### P9 — Matrix determinant recursion (TRACKER DESCRIPTION WAS INCORRECT — now resolved)
**Rating impact:** Low
**File:** `App/Src/calc_engine.c`

**Re-examined 2026-03-21:** The previous tracker entry said "the Gaussian-elimination `mat_det`
function recurses up to `size` levels." This was incorrect. `mat_det` (`calc_engine.c:549`) is
fully **iterative** — it uses standard nested `for` loops for Gaussian elimination with partial
pivoting, with no recursive calls whatsoever. The function cannot stack-overflow regardless of
matrix dimensions. No assertion or depth guard is needed.

**Resolved:** 2026-03-21 (tracker error corrected — implementation was always iterative)

---

### P10 — PRGM system not hardware-validated
**Rating impact:** Testing = D (existing item)
**Files:** `App/Src/calculator_core.c` (prgm_* functions), `prgm_manual_tests.md`

See `prgm_manual_tests.md` for the 28-test hardware validation plan. Until all 28 tests
pass, PRGM should be treated as non-functional (see also README.md and CLAUDE.md notices).

**Note (2026-03-21):** PRGM backend handler functions now exist in `calculator_core.c`:
`handle_prgm_running`, `handle_prgm_menu`, `handle_prgm_new_name`, `handle_prgm_editor`,
`handle_prgm_ctl_menu`, `handle_prgm_io_menu`. The 6 PRGM modes are all dispatched from
`Execute_Token`. However, hardware validation has not been performed; PRGM remains untested.

**Resolved:** —

---

### P11 — Duplicate `#include` directives in `calculator_core.c`
**Rating impact:** Naming conventions = B+ (minor cleanliness issue)
**File:** `App/Src/calculator_core.c` (lines 25–30)

`<stdio.h>`, `<stdlib.h>`, and `<string.h>` are each included **twice** in the same file:
```c
// Lines 25–27 (first occurrence)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Lines 28–30 (duplicate)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```
Include guards in the standard headers prevent double-definition errors, so there is no
functional impact. However, this is a maintenance smell — it likely originated from a
copy-paste during a merge or extraction session.

**Recommendation:** Delete lines 28–30.

**Resolved:** —

---

## Resolved Items

| Item | Resolution | Date |
|---|---|---|
| P1 — No automated test suite | 301-test host suite (3 executables); expr_util.c extracted; persist round-trip tests; HAL guards; Testing rating B+ | 2026-03-21 (partial — B+ score) |
| P4 — Magic numbers: colours | `ui_palette.h` created; 14 named constants; inline hex literals replaced across `calculator_core.c`, `graph.c`, `app_init.c`, `ui_prgm.c`, `ui_matrix.c`; one intentional exception (trace crosshair green in `graph.c`) | 2026-03-21 |
| P5 — Missing `const` on `TI81_LookupTable` | `const` added to `TI81_LookupTable` and `TI81_LookupTable_Size` in `keypad_map.c` | 2026-03-20 |
| P7 — Incomplete wiring table | STM32 GPIO side documented; physical TI-81 ribbon mapping pending annotated photos | 2026-03-20 (partial) |
| P9 — Matrix determinant recursion | Tracker description was incorrect — `mat_det` (L549) is iterative Gaussian elimination; no recursive calls exist; no depth guard needed | 2026-03-21 |

---

## Review History

| Date | Reviewer | Method | Notes |
|---|---|---|---|
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | Full static codebase analysis | Initial review; 10 issues logged |
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | Incremental codebase analysis | Update pass: P2 partially resolved (3 modules extracted, calculator_core.c 37% smaller); P5/P7 resolved; P6 partially resolved; overall rating bumped to 65–75% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Incremental codebase analysis | P1 partially resolved: 103-test host suite added for calc_engine; Testing D → C; overall rating bumped to 70–80% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Incremental codebase analysis | P1 B score: 153 tests (groups 15–20 added), gcov 80.28% branch coverage, CI host-tests job; Testing C → B; overall rating bumped to 75–85% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Incremental codebase analysis | P1 B+ score: expr_util.c extracted (9 pure functions, 96 tests/12 groups), persist round-trip suite (52 tests/5 groups), HOST_TEST HAL guards, PersistBlock_t size corrected 856→860 B; Testing B → B+; overall rating bumped to 80–88% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Full static quality review + documentation pass | All docs updated (CLAUDE.md, QUALITY_TRACKER.md, OPEN_SOURCE_RECOMMENDATIONS.md); header audit: A-grade (all 10 headers complete, no circular deps); 7 onboarding gaps identified; priorities 13–15 added; P4 fully resolved entry added to resolved items table |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Full static codebase re-review (all App/ source files) | P9 resolved (mat_det confirmed iterative, tracker description was wrong); P11 added (duplicate #includes in calculator_core.c L25–30); calculator_core.c LOC corrected 3,654→3,563; all handler sizes re-measured with exact line numbers; prgm.c confirmed correct .RamFunc + CCMRAM pattern; overall rating bumped to 82–88% |
