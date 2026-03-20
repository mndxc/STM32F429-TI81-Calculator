# Project Quality Tracker

Tracks the results of periodic professional quality reviews and outstanding improvement items.
Update this file after each review pass or when an item is resolved.

**Last reviewed:** 2026-03-20
**Reviewer:** Claude Code (claude-sonnet-4-6) via full codebase static analysis

---

## Overall Assessment

> **Verdict: Strong personal/research project. Not yet production-ready.**
>
> Exceptional documentation and hardware-correctness. The expression evaluator, RTOS integration,
> and FLASH handling show genuine embedded expertise. The main gaps are architectural: one
> oversized file, no automated tests, and scattered static state.

**Estimate for production readiness:** 60–70%

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
| Magic numbers / constants | B- | 2026-03-20 |
| Testing | D | 2026-03-20 |

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
**Rating impact:** Testing = D
**Files:** `App/Src/calc_engine.c` (1,135 LOC), `App/Src/calculator_core.c`

The expression parser pipeline (tokenizer → shunting-yard → RPN evaluator) has no automated
coverage. Edge cases — `2^-3`, nested parentheses, matrix dimension bounds, UTF-8 multi-byte
tokens — can only be found by flashing and testing manually. PRGM is an entire unvalidated
subsystem on `main`.

**Recommendation:** `calc_engine.c` has no LVGL or HAL dependencies; its three public functions
(`Tokenize`, `ShuntingYard`, `EvaluateRPN`) can be tested on a host machine with a plain C test
runner (e.g. Unity or a simple `assert`-based harness). This is the single highest-ROI improvement.

**Resolved:** —

---

### P2 — `calculator_core.c` too large (5,820 LOC)
**Rating impact:** Code organisation = B-, Function complexity = C+
**File:** `App/Src/calculator_core.c`

Largest file by a 5:1 ratio over the next largest (`calc_engine.c` at 1,135). The
`Execute_Token` refactor in session 3 split function *names* but did not reduce function size —
several handlers remain 200–316 lines (`prgm_execute_line`: 316, `handle_normal_mode`: 292,
`handle_yeq_mode`: 204, `handle_matrix_edit`: 192).

Industry standard: functions under 50–100 lines.

**Recommendation:**
- Extract PRGM subsystem into `App/Src/prgm.c` / `App/Inc/prgm.h`
- Extract matrix editor into `App/Src/matrix_editor.c`
- Extract graph screen handlers (Y=, RANGE, ZOOM) into `App/Src/graph_ui.c`
- Reduce remaining handlers to under 100 lines each

**Resolved:** —

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

**Resolved:** —

---

### P5 — Missing `const` on immutable data
**Rating impact:** Naming conventions = B+
**File:** `App/Drivers/Keypad/keypad_map.c`

```c
// Current (missing const):
KeyDefinition_t TI81_LookupTable[] = { ... };

// Should be:
const KeyDefinition_t TI81_LookupTable[] = { ... };
```

The lookup table is never modified. Marking it `const` places it in `.rodata` (FLASH), saves RAM,
and lets the compiler catch accidental writes. Other instances of non-const read-only data likely
exist; a full audit is warranted.

**Resolved:** —

---

### P6 — No `-Wall -Wextra` in build
**Rating impact:** Code organisation = B-
**File:** `CMakeLists.txt`

No warnings-as-errors flag. The `.clangd` config suppresses `unused-includes` and
`unknown_typename` (acceptable for CubeMX/LVGL generated code compatibility) but means app-code
warnings can hide behind those suppressions.

**Recommendation:** Add `-Wall -Wextra` to the app-code compile target in `CMakeLists.txt`.
Suppress only for LVGL/CubeMX sources using a separate `target_compile_options` block.
Do not add `-Werror` until the baseline warning count is zero.

**Resolved:** —

---

### P7 — Incomplete hardware wiring table in GETTING_STARTED.md
**Rating impact:** Documentation = A+ (risk to onboarding)
**File:** `docs/GETTING_STARTED.md`

The wiring section contains a placeholder:
> `(follow-up: fill in complete wiring table)`

The GPIO pin mapping table for the TI-81 ribbon connector is the first thing a new contributor
needs to reproduce the hardware. All authoritative pin constants are in `keypad.h`.

**Recommendation:** Fill the wiring table from `keypad.h` pin definitions (`KEYPAD_A1_PORT/PIN`
through `KEYPAD_B8_PORT/PIN` and `KEYPAD_ON_PORT/PIN`).

**Resolved:** —

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

### P9 — Matrix determinant recursion has no depth guard
**Rating impact:** Low (bounded by CALC_MATRIX_MAX_DIM = 6)
**File:** `App/Src/calc_engine.c`

The Gaussian-elimination `mat_det` function recurses up to `size` levels. At the current maximum
of 6×6 this is safe (depth ≤ 6). But there is no `assert(depth < CALC_MATRIX_MAX_DIM)` to
catch a regression if the max dimension is ever increased without auditing the recursion.

**Recommendation:** Add a compile-time or runtime depth assertion inside `mat_det`.

**Resolved:** —

---

### P10 — PRGM system not hardware-validated
**Rating impact:** Testing = D (existing item)
**Files:** `App/Src/calculator_core.c` (prgm_* functions), `TEMP-prgm_manual_tests.md`

See `TEMP-prgm_manual_tests.md` for the 28-test hardware validation plan. Until all 28 tests
pass, PRGM should be treated as non-functional (see also README.md and CLAUDE.md notices).

**Resolved:** —

---

## Resolved Items

*(none yet)*

---

## Review History

| Date | Reviewer | Method | Notes |
|---|---|---|---|
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | Full static codebase analysis | Initial review; 10 issues logged |
