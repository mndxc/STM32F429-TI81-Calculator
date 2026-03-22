# Project Quality & Open-Source Readiness

**Purpose:** Permanent register for code quality reviews, CI, refactoring, testing, and contributor-docs work. This is the single source of truth for all P-numbered improvement items. Feature work, bug fixes, and session planning live in `CLAUDE.md` — not here. Update this file when a quality item is opened, progressed, or resolved.

**Last reviewed:** 2026-03-21 (P2 resolved post-review; Session 8/9 updates)
**Reviewer:** Claude Code (claude-sonnet-4-6) via full codebase static analysis

---

## Overall Assessment

> **Verdict: Strong personal/research project. Not yet production-ready.**
>
> Exceptional documentation and hardware-correctness. The expression evaluator, RTOS integration,
> and FLASH handling show genuine embedded expertise. The module extraction series is now
> complete: `graph_ui.c`, `ui_matrix.c`, `ui_prgm.c`, `expr_util.c` have all been extracted
> following a consistent pattern. `calculator_core.c` is no longer the dominant file (1,989 LOC,
> down from 5,820). The remaining structural gap is that the file still owns 8 concern areas
> and individual handlers remain long. Function complexity (C+) is the weakest rated dimension
> and is not addressable by file reorganisation alone.

**Estimated production readiness:** 85–90%
*(up from 83–89%; gain from P2 resolution — Code organisation B→B+)*

---

## At a Glance

| Area | Status |
|---|---|
| **Community & Governance** | |
| `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` | ✅ Done |
| Issue templates (bug report, feature request) | ✅ Done |
| PR template | ✅ Done |
| README badges (Build, License, PRs Welcome) | ✅ Done |
| **CI & Testing** | |
| Firmware CI build on push/PR | ✅ Done |
| Host test suite (301 tests, 3 executables, 80.28% branch coverage) | ✅ Done — B+ rating; A requires property-based tests (P1) |
| `-Werror` enabled | 🔴 Open — P6 |
| PRGM backend hardware validation | 🔴 Open — P10 |
| **Code Quality** | |
| `const` on `TI81_LookupTable` | ✅ Done |
| Colour palette constants (`ui_palette.h`) | ✅ Done |
| `-Wall -Wextra` compiler warnings | ✅ Done |
| `expr_util.c` extraction + host tests | ✅ Done |
| Module extraction from `calculator_core.c` | ✅ Done — P2 resolved 2026-03-21 (`graph_ui.c` extracted; file at 1,989 LOC) |
| Scattered static state consolidation | 🟡 Partial — P3 (Phase 1+2 done; Phase 3 optional) |
| Float printf runtime guard | ✅ Done — P8 |
| **Hardware Onboarding** | |
| Bill of Materials | ✅ Done |
| STM32 GPIO wiring table | ✅ Done |
| Physical TI-81 ribbon ↔ wire mapping | 🟡 Partial — P7 (ribbon pad mapping unverified) |
| **Contributor Documentation** | |
| Architecture diagram | 🔴 Open — P12 |
| Testing guide (`docs/TESTING.md`) | 🔴 Open — P13 |
| PRGM completion roadmap | 🔴 Open — P14 |
| Expression pipeline walkthrough | 🔴 Open — P15 |
| FLASH sector map in onboarding docs | ✅ Done — P16 |
| Troubleshooting guide | 🔴 Open — P17 |

---

## Quality Scorecard

| Dimension | Rating | Last changed |
|---|---|---|
| Documentation | A+ | 2026-03-20 |
| API / header design | A | 2026-03-20 |
| Memory safety & FLASH handling | A | 2026-03-20 |
| RTOS integration | A | 2026-03-20 |
| Error handling | A- | 2026-03-20 |
| Naming conventions | B+ | 2026-03-20 |
| Code organisation | B+ | 2026-03-21 |
| Function complexity | C+ | 2026-03-20 |
| Magic numbers / constants | A- | 2026-03-21 |
| Testing | B+ | 2026-03-21 |

---

## Strengths (Do Not Regress)

### Documentation (A+)
`CLAUDE.md` is exceptional — feature status, architectural decisions, gotchas, known issues, PCB
notes, and next-session priorities all clearly maintained. Most professional firmware teams do not
write this well. `README.md`, `GETTING_STARTED.md`, and `TECHNICAL.md` provide clean onboarding.

### API and header design (A)
- Include guards correctly named (`GRAPH_MODULE_H` vs `GRAPH_H` collision avoided by design)
- Extern declarations match implementations across all modules
- Module prefixes consistent: `Calc_*`, `Graph_*`, `Persist_*`, `Keypad_*`
- No circular dependencies detected; all 10 App headers fully declare their public APIs

### Embedded-specific correctness (A)
- FLASH erase placed in `.RamFunc`; LTDC/SDRAM disable ordered correctly before Stop mode
- `_Static_assert` used for `PersistBlock_t` word-alignment
- `volatile` correctly applied to `g_sleeping` ISR flag
- Checksums + magic number + version on the persist block
- ISR-safe queue use (`xQueueSendFromISR`)
- `prgm_exec.c` correctly mirrors `persist.c` pattern: CCMRAM storage, `.RamFunc` erase/write, magic/version/checksum

### RTOS integration (A)
Mutex guards on all LVGL calls are consistent. The `cursor_timer_cb` deadlock case is explicitly
documented and avoided. Priority and ISR ceiling usage is correct.

### Error handling (A-)
`CalcResult_t` returns rich error context (`CalcError_t` enum + message string + matrix flag).
Bounds checking present throughout: token count, stack depth, matrix dimension limits.

### Open-source scaffolding (fully complete)
All standard community scaffolding is in place. No further action needed in this category.
- **`CONTRIBUTING.md`** — PR flow, code style, naming conventions, and development guidelines
- **`CODE_OF_CONDUCT.md`** — standard community conduct document
- **Issue templates** — `bug_report.md` and `feature_request.md` prompt for hardware revision, reproduction steps, and expected vs actual behaviour
- **PR template** — `.github/pull_request_template.md` in place
- **README badges** — Build status, License, and "PRs Welcome" badges at the top of `README.md`

---

## Prioritised Improvement Roadmap

Two reference views over the open issues. Use these to decide what to tackle next.

### By Ease of Resolution

| Rank | Item | Effort estimate | Notes |
|---|---|---|---|
| 1 | ~~P8 — Float printf guard~~ | ~~30 min~~ | ✅ Resolved 2026-03-21 |
| 2 | ~~P16 — FLASH sector map in docs~~ | ~~30 min~~ | ✅ Resolved 2026-03-21 |
| 3 | P17 — Troubleshooting guide | 1–2 hrs | Reorganise existing gotchas from `CLAUDE.md`; no new content needed |
| 4 | P13 — Testing guide | 1–2 hrs | Material already exists in `CLAUDE.md` and test file headers |
| 5 | P12 — Architecture diagram | 2–3 hrs | One Mermaid diagram; no code changes |
| 6 | P15 — Expression pipeline walkthrough | 2–3 hrs | Worked example in `TECHNICAL.md`; requires careful reading of `calc_engine.c` |
| 7 | P14 — PRGM completion roadmap | 2–4 hrs | Map backend gaps against 28-item test plan; planning doc only |
| 8 | P6 — Enable `-Werror` | 2–8 hrs | Config change; unknown current warning count is the risk variable |
| 9 | P1 — Test suite to A rating | 4–8 hrs | Property-based tests are well-scoped; PRGM arm blocked until backend complete |
| 10 | ~~P2 — Extract `graph_ui.c`~~ | ~~4–8 hrs~~ | ✅ Resolved 2026-03-21 |
| 11 | P3 — Handler state params (Phase 3) | 8–16 hrs | Every handler signature changes; high regression risk |
| 12 | P10 — PRGM hardware validation | Weeks | Blocked on completing the disconnected backend first |
| 13 | P7 — Physical wiring table | Indefinite | Requires donor board, multimeter, photography; cannot be done in software |

### By Impact of Resolution

| Rank | Item | Scorecard dimension(s) | Why |
|---|---|---|---|
| 1 | ~~P2 — Extract `graph_ui.c`~~ | ~~Code organisation B→B+~~ | ✅ Resolved 2026-03-21. Code organisation moved to B+. Function complexity unchanged at C+ (file reorganisation does not decompose long handlers). |
| 2 | P10 — PRGM hardware validation | Testing; feature completeness | Largest remaining feature gap; validating the backend is the single biggest milestone |
| 3 | P6 — Enable `-Werror` | CI quality gate | Structural gate: prevents all future warning regressions; protection value exceeds point gain |
| 4 | P1 — Test suite to A rating | Testing B+→A | Moves the only B+ to A; property-based tests harden expression engine edge cases |
| 5 | P3 — Handler state params (Phase 3) | Code organisation; enables unit testing | Once handlers accept `State_t *`, they become host-testable in isolation — unlocks a new coverage tier |
| 6 | P12 — Architecture diagram | Open-source onboarding | First thing a new contributor looks for; dramatically lowers the barrier to understanding RTOS/module structure |
| 7 | P14 — PRGM completion roadmap | Contributor enablement | Without a concrete task list, community effort cannot be directed at the largest incomplete feature |
| 8 | P13 — Testing guide | Onboarding; test suite growth | Without it, contributors can read tests but cannot add them confidently |
| 9 | P15 — Expression pipeline walkthrough | Contributor safety | Most algorithmically dense file; undocumented behaviour here is the most likely source of subtle community PR bugs |
| 10 | ~~P16 — FLASH sector map in docs~~ | ~~Hardware safety~~ | ✅ Resolved 2026-03-21 |
| 11 | P17 — Troubleshooting guide | First-contact friction | Reduces the most common new-builder failures; improves open-source engagement |
| 12 | ~~P8 — Float printf guard~~ | ~~Silent failure prevention~~ | ✅ Resolved 2026-03-21 |
| 13 | P7 — Physical wiring table | Hardware replication | Relevant only to contributors replicating the physical build; STM32 GPIO side already documented |

**Sweet spot** — highest impact for least effort: **P6, P12, P13, P17** — documentation and config items that close most open-source onboarding gaps and add a permanent CI quality gate. (P8, P16 resolved 2026-03-21.)

---

## Open Issues

Issues are ordered by priority. Mark resolved items with a date rather than deleting them —
the history is useful.

---

### P1 — Automated test suite: target A rating

**Rating impact:** Testing = B+ (up from D → C → B → B+)
**Files:** `App/Src/calc_engine.c`, `App/Src/expr_util.c`, `App/Src/persist.c`,
`App/Tests/test_calc_engine.c`, `App/Tests/test_expr_util.c`,
`App/Tests/test_persist_roundtrip.c`, `App/Tests/CMakeLists.txt`,
`.github/workflows/build.yml`

**Current state (B+ as of 2026-03-21):**

| Executable | Tests | What it covers |
|---|---|---|
| `test_calc_engine` | 153 / 20 groups | Tokenizer, shunting-yard, RPN evaluator, matrix ops, boundary cases; **80.28% branch coverage** on `calc_engine.c` |
| `test_expr_util` | 96 / 12 groups | UTF-8 cursor movement, insert/delete/overwrite, matrix token atomicity |
| `test_persist_roundtrip` | 52 / 5 groups | `PersistBlock_t` checksum stability, valid/invalid block detection, field round-trip, struct size/alignment |

All three executables run on the build host with no embedded dependencies. CI runs them on every push/PR with gcov branch coverage summary.

**Build and run:**
```bash
cmake -S App/Tests -B build/tests && cmake --build build/tests
./build/tests/test_calc_engine        # 153 tests
./build/tests/test_expr_util          # 96 tests
./build/tests/test_persist_roundtrip  # 52 tests
```

**Improvement path to A rating:**

| Target | Requirements |
|---|---|
| **B+** ✅ | Pure logic extracted from `calculator_core.c` and tested on host; `persist.c` round-trip; CI pipeline |
| **A** | + Property-based invariant tests (e.g. sin²+cos²=1 for 1,000 x values); PRGM either extracted+tested or all 28 manual tests in `prgm_manual_tests.md` signed off |

**Remaining gaps:** `Calc_FormatResult` scientific notation not checked byte-for-byte (format differs between ARM nano.specs and host libc). PRGM execution backend is coupled to LVGL display calls; needs extraction before host testing is practical.

**Status:** Partially resolved (2026-03-21 — B+ score achieved)

---

### P2 — `calculator_core.c` too large

**Rating impact:** Code organisation B → **B+** ✅
**File:** `App/Src/calculator_core.c`

**Resolved 2026-03-21** — `graph_ui.c` extracted. `calculator_core.c` is no longer the dominant file in the project.

**Full extraction history:**

| Module extracted | LOC added | `calculator_core.c` after |
|---|---|---|
| `App/Src/ui_matrix.c` + `ui_matrix.h` | 544 | ~4,600 |
| `App/Src/ui_prgm.c` + `ui_prgm.h` | 1,713 | ~3,900 |
| `App/Src/prgm_exec.c` + `prgm_exec.h` | 111 | ~3,800 |
| `App/Src/expr_util.c` + `expr_util.h` | 144 (9 pure functions) | ~3,603 |
| **`App/Src/graph_ui.c` + `graph_ui.h`** | **1,579** | **1,989** |

Original: 5,820 LOC. Final: 1,989 LOC. **−66% over the full extraction series.**

**What `graph_ui.c` contains:**
- All 6 graph handler functions: `handle_yeq_mode`, `handle_range_mode`, `handle_zoom_mode`, `handle_zoom_factors_mode`, `handle_zbox_mode`, `handle_trace_mode`
- `nav_to` (graph screen navigation dispatcher)
- `ui_init_graph_screens` (LVGL object creation for Y=, RANGE, ZOOM, ZOOM FACTORS)
- ~20 private static helpers (cursor update, field reset/load/commit, highlight, zoom execute, etc.)
- All 6 graph-state structs (`YeqEditorState_t`, `RangeEditorState_t`, `ZBoxState_t`, etc.) — private to the module
- Persist accessors (`graph_ui_get_zoom_x_fact`, `graph_ui_set_zoom_facts`) so `Calc_BuildPersistBlock` in `calculator_core.c` does not need to reach into `s_zf` directly

**Architectural compromises made during extraction (document for future refactors):**

1. **`range_cursor_update` exported** — `cursor_timer_cb` in `calculator_core.c` must call it to blink the RANGE field cursor. It is now public API in `graph_ui.h`. A cleaner long-term design would be a single `graph_ui_blink_cursor()` dispatch that `cursor_timer_cb` calls opaquely, eliminating three separate calls (`yeq_cursor_update`, `range_cursor_update`, `zoom_factors_cursor_update`) and the coupling they represent.

2. **`zoom_menu_reset()` folded into `nav_to(MODE_GRAPH_ZOOM)`** — Previously called explicitly before every `nav_to(MODE_GRAPH_ZOOM)` in `calculator_core.c`. Moved inside the `nav_to` switch case to avoid exporting a private helper. `nav_to` now has a mild implicit side effect beyond pure screen switching. Acceptable; documented here.

3. **`menu_open` and `ui_update_status_bar` made non-static** — These functions stay in `calculator_core.c` but are called by moved handlers in `graph_ui.c`. They are now declared in `calc_internal.h`. Net effect: two more functions added to the shared-state bridge surface. Not a problem in practice.

**Remaining concern — `calculator_core.c` still owns 8 concern areas:**
The file is no longer oversized, but it still contains: display/history rendering, MATH menu, TEST menu, MATRIX menu, MODE screen, PRGM dispatcher, persist integration, and the main expression handler. These are cohesive as a "calculator core" but the MATH/TEST/MODE handlers (~400 LOC combined) could be extracted following the established pattern if Code organisation is targeted for an A rating in a future pass.

**Current largest files (post-extraction):**

| File | LOC | Note |
|---|---|---|
| `App/Src/ui_prgm.c` | 1,713 | Now the largest module; PRGM UI + executor |
| `App/Src/calculator_core.c` | 1,989 | Still 8 concern areas but no longer dominant |
| `App/Src/graph_ui.c` | 1,579 | New; well-scoped to graph editor screens |
| `App/Src/calc_engine.c` | 1,138 | Dense but single-responsibility (expression pipeline) |

**Extraction pattern (established and validated):**
1. Move state structs and their LVGL widget handles to the new `.c` file as private statics
2. Export 4 screen pointers as non-static globals (needed by `hide_all_screens()` via `calc_internal.h`)
3. Export handler functions, init function, and any helpers called from `calculator_core.c` via the new `.h`
4. Declare cross-module functions used by the new module in `calc_internal.h`
5. Add the new source to `CMakeLists.txt`; run firmware build + 301 host tests to verify

**Status:** ✅ Resolved (2026-03-21 — `graph_ui.c` extracted; Code organisation B → B+; 301/301 tests pass)

---

### P3 — Scattered static state

**Rating impact:** Code organisation = B
**File:** `App/Src/calculator_core.c`

~80 static variables with no central `CalcState_t` struct. State spans expression buffers, cursor, history, mode, range fields, zoom factors, trace position, matrix edit, and PRGM executor. This makes the code difficult to snapshot, impossible to unit-test handlers in isolation, and hard to reason about in GDB.

**Progress (Phase 1+2 complete — 2026-03-21; Phase 1+2 partially migrated by P2 — 2026-03-21):**
Ten named sub-structs introduced; ~40 flat statics consolidated. Zero logic changes.

| Struct | Instance | Current location | Fields grouped |
|---|---|---|---|
| `RangeEditorState_t` | `s_range` | `graph_ui.c` (private) | field, buf[16], len, cursor |
| `TraceState_t` | `s_trace` | `graph_ui.c` (private) | x, eq_idx |
| `ZBoxState_t` | `s_zbox` | `graph_ui.c` (private) | px, py, px1, py1, corner1_set |
| `ZoomFactorsState_t` | `s_zf` | `graph_ui.c` (private) | x_fact, y_fact, field, buf[16], len, cursor |
| `ZoomMenuState_t` | `s_zoom` | `graph_ui.c` (private) | scroll_offset, item_cursor |
| `YeqEditorState_t` | `s_yeq` | `graph_ui.c` (private) | selected, cursor_pos |
| `MathMenuState_t` | `s_math` | `calculator_core.c` (private) | tab, item_cursor, scroll_offset, return_mode |
| `TestMenuState_t` | `s_test` | `calculator_core.c` (private) | item_cursor, return_mode |
| `ModeScreenState_t` | `s_mode` | `calculator_core.c` (private) | row_selected, cursor[8], committed[8] |
| `MatrixMenuState_t` | `s_matrix_menu` | `calculator_core.c` (private) | tab, item_cursor, return_mode |

The P2 extraction moved the 6 graph-related structs into `graph_ui.c` as truly private statics — they are no longer reachable outside the module. This is a stronger encapsulation than the original P3 goal and supersedes Phase 3 for those structs.

LVGL widget handle arrays and const data tables intentionally left flat (stable after init; never mutated at runtime).

**Remaining work (Phase 3 — optional, enables unit testing of remaining handlers):**
Modify the 4 remaining handlers in `calculator_core.c` (`handle_math_menu`, `handle_test_menu`, `handle_mode_screen`, and the matrix handlers) to accept `State_t *` parameters instead of reading module-level statics. Once `handle_math_menu` takes a `MathMenuState_t *`, it can be called from a host test with a constructed struct — no LVGL needed.

**Status:** Partially resolved (2026-03-21 — Phase 1+2 complete; 6 graph structs further improved to fully private by P2 extraction)

---

### P6 — `-Werror` not set

**Rating impact:** Code organisation = B-
**File:** `CMakeLists.txt`

`-Wall -Wextra` is present via `add_compile_options(-Wall -Wextra)` in `CMakeLists.txt`. `-Werror` is not yet set — the next step is to resolve the current warning baseline and add it to prevent regressions.

**Also pending:** `.github/workflows/build.yml` does not pass `-DCMAKE_BUILD_TYPE=Debug` to CMake. This works but diverges from the local developer build.

**Recommendation:**
1. Run the build with `-Werror` locally and fix any warnings that surface
2. Add `-Werror` in `CMakeLists.txt` (App sources only; suppress for CubeMX/LVGL sources via `target_compile_options`)
3. Add `-DCMAKE_BUILD_TYPE=Debug` to the CI configure step

**Status:** Partially resolved (2026-03-20 — `-Wall -Wextra` added; `-Werror` not yet set)

---

### P7 — Incomplete physical wiring table

**Rating impact:** Documentation = A+ (risk to contributor onboarding)
**File:** `docs/GETTING_STARTED.md`

The STM32 GPIO side of the wiring table (A1–A7 = PE5/PE4/PE3/PE2/PB7/PB4/PB3; B1–B8 = PG9/PD7/PC11/PC8/PC3/PA5/PG2/PG3; ON = PE6) is documented and cross-referenced against `keypad.h`. ✅

**Remaining:** The physical correspondence between numbered pads on the TI-81 PCB and the logical A-line/B-line names has not been manually traced and photographed. A new contributor cannot replicate the physical wiring without a multimeter and a donor board. Annotated photos are planned; the wiring table currently contains a prominent warning block and an invitation for community contributions.

**Status:** Partially resolved (2026-03-20 — STM32 GPIO side complete; ribbon pad mapping pending)

---

### P8 — No runtime guard for float printf

**Rating impact:** Low (silent build-time risk)
**Files:** `CMakeLists.txt`, `App/Src/app_init.c`

50+ `snprintf` calls rely on `%.4g`/`%.6f`/`%.4e` format specifiers. If `-u _printf_float` is accidentally removed during a CMake refactor, all float output silently becomes empty strings — no compiler or linker error is produced.

**Status:** ✅ Resolved (2026-03-21 — startup assertion added in `App_DefaultTask_Run()` after LVGL init; formats `1.5f` with `%.2f` and halts with 10 Hz heartbeat LED blink if the result is not `"1.5"`)

---

### P10 — PRGM system not hardware-validated

**Rating impact:** Testing = D
**Files:** `App/Src/calculator_core.c` (prgm_* handlers), `prgm_manual_tests.md`

The PRGM handler functions exist in `calculator_core.c` (`handle_prgm_running`, `handle_prgm_menu`, `handle_prgm_new_name`, `handle_prgm_editor`, `handle_prgm_ctl_menu`, `handle_prgm_io_menu`) and all 6 PRGM modes are dispatched from `Execute_Token`. No hardware validation has been performed; PRGM is untested end-to-end.

See `prgm_manual_tests.md` for the 28-item hardware test plan. Until all 28 tests pass, PRGM should be treated as non-functional (see also notices in `README.md` and `CLAUDE.md`).

**Status:** Open

---

### P12 — No architecture diagram

**Priority:** High (contributor onboarding)
**Files:** `README.md` or new `docs/ARCHITECTURE.md`

No visual diagram shows module dependencies, data flow, or RTOS task boundaries. New contributors must infer all relationships from include files and prose.

**Recommendation:** A Mermaid diagram covering:
- The three-task RTOS structure (KeypadTask → CalcCoreTask → DefaultTask)
- Module hierarchy (`keypad.c` → `calculator_core.c` → `calc_engine.c` / `graph.c` / `persist.c` / `expr_util.c`)
- Which modules are host-testable vs embedded-only

Suitable for embedding in `README.md` or a standalone `docs/ARCHITECTURE.md`.

**Status:** Open

---

### P13 — No contributor testing guide

**Priority:** High (contributor onboarding)
**Files:** New `docs/TESTING.md`

`CLAUDE.md` documents how to build and run the tests but not what each suite covers or how to add new tests. A contributor wanting to write tests has no starting point.

**Recommendation:** `docs/TESTING.md` should explain:
- What each executable tests (calc_engine: expression pipeline; expr_util: cursor/UTF-8/buffer manipulation; persist: serialization/checksum)
- How to add a test (copy a `static void test_*()` function, use `EXPECT_*` macros, register from `main()`)
- Coverage expectations (>80% branch coverage on `calc_engine.c` required; run with `-DCOVERAGE=ON`)
- Link to `prgm_manual_tests.md` for the PRGM hardware test protocol

**Status:** Open

---

### P14 — No PRGM completion roadmap

**Priority:** High (contributor onboarding)
**Files:** New `docs/PRGM_COMPLETION.md`

`ui_prgm.h` warns that the backend is incomplete but lists no concrete tasks. A contributor wanting to complete PRGM has no clear starting point.

**Recommendation:** `docs/PRGM_COMPLETION.md` listing 5–10 specific tasks (tokenization bridge, `prgm_flatten_to_store`, I/O execution loop, `Goto`/`Lbl` lookup table, `Menu(` support), effort estimates, and acceptance criteria tied to the 28-item manual test plan in `prgm_manual_tests.md`.

**Status:** Open

---

### P15 — Expression pipeline undocumented

**Priority:** Medium (contributor onboarding)
**File:** `docs/TECHNICAL.md`

`calc_engine.h` sketches the three-stage pipeline (tokenize → shunting-yard → RPN eval) but provides no worked example. Modifying `calc_engine.c` safely requires understanding the token list format, operator precedence handling, and RPN stack behaviour.

**Recommendation:** Add a section to `TECHNICAL.md` walking through `"2 + sin(45)"` step by step: raw string → token list → postfix token list → RPN stack trace → `CalcResult_t`.

**Status:** Open

---

### P16 — FLASH sector map not in onboarding docs

**Status:** ✅ Resolved (2026-03-21 — sector table + caution block added to `docs/TECHNICAL.md` Memory Layout section)

---

### P17 — No troubleshooting guide

**Priority:** Low (first-contact friction)
**Files:** New `docs/TROUBLESHOOTING.md` or appendix to `docs/GETTING_STARTED.md`

Common pitfalls are documented in `CLAUDE.md` as gotchas but not in `GETTING_STARTED.md` where a first-time builder would look.

**Recommendation:** Cover the most frequent failures:
- Silent float printf (missing `-u _printf_float`)
- White screen after flashing (power cycle required)
- GDB connection during Stop mode
- OpenOCD not finding the board

**Status:** Open

---

## Resolved Items

| Item | Resolution | Date |
|---|---|---|
| P2 — `calculator_core.c` too large | `graph_ui.c` (1,579 LOC) + `graph_ui.h` extracted. All 6 graph handler functions, `nav_to`, `ui_init_graph_screens`, ~20 private helpers, and 6 state structs moved. `calculator_core.c` reduced from 3,603 → 1,989 LOC (−45%); from 5,820 → 1,989 overall (−66%). Code organisation B → B+. Three minor architectural compromises documented in P2 issue entry above. 301/301 host tests pass. | 2026-03-21 |
| P8 — Float printf runtime guard | Startup assertion in `App_DefaultTask_Run()`: `snprintf(buf, 8, "%.2f", 1.5f)` — halts with 10 Hz heartbeat LED blink if `-u _printf_float` is missing | 2026-03-21 |
| P4 — Magic numbers: colours | `ui_palette.h` created with 14 named constants; inline hex literals replaced across `calculator_core.c`, `graph.c`, `app_init.c`, `ui_prgm.c`, `ui_matrix.c`; one intentional exception (trace crosshair green in `graph.c`) | 2026-03-21 |
| P5 — Missing `const` on `TI81_LookupTable` | `const` added to `TI81_LookupTable` and `TI81_LookupTable_Size` in `keypad_map.c` | 2026-03-20 |
| P9 — Matrix determinant recursion (tracker error) | `mat_det` (L549) confirmed fully iterative — Gaussian elimination with partial pivoting, no recursive calls; tracker description was wrong; no guard needed | 2026-03-21 |
| P11 — Duplicate `#include` directives | Lines 28–30 in `calculator_core.c` deleted; `<stdio.h>`, `<stdlib.h>`, `<string.h>` each now included once | 2026-03-21 |
| P16 — FLASH sector map in docs | Sector table (sectors 0–11 with addresses, sizes, contents) + `[!CAUTION]` block warning about FLASH_SECTOR_7 collision added to `docs/TECHNICAL.md` Memory Layout section | 2026-03-21 |
| Open-source governance | `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, issue templates (bug + feature), PR template, README badges — all complete | 2026-03-21 |
| Header audit | All 10 App headers correctly declare their public APIs; include guards correct; no circular dependencies; module prefixes consistent — A-grade confirmed | 2026-03-21 |
| `st-flash` references removed | `GETTING_STARTED.md` and `TECHNICAL.md` now use OpenOCD commands throughout | 2026-03-21 |

---

## Review History

| Date | Reviewer | Notes |
|---|---|---|
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | Initial review; P1–P10 logged; rating 60–70% |
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | P2 partly resolved (3 modules extracted, 37% smaller); P5/P7 resolved; P6 partly resolved; rating 65–75% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P1: 103-test suite; Testing D→C; rating 70–80% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P1 B score: 153 tests, gcov 80.28%, CI host-tests job; Testing C→B; rating 75–85% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P1 B+ score: `expr_util.c` extracted (96 tests/12 groups), persist round-trip (52 tests/5 groups), HAL guards, `PersistBlock_t` size corrected 856→860 B; Testing B→B+; rating 80–88% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Full quality + documentation pass; header audit A-grade; 7 onboarding gaps identified; P4 resolved |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Full codebase re-review; P9 resolved (tracker error); P11 added and resolved; handler sizes re-measured; rating 82–88% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Maintenance pass: P11 resolved; `GETTING_STARTED.md`/`TECHNICAL.md` corrected; `st-flash` references removed |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P8 resolved: float printf startup assertion added in `App_DefaultTask_Run()`; 10 Hz heartbeat fault indicator |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P3 Phase 1+2: 10 named sub-structs, ~40 statics consolidated; Code organisation B-→B; rating 83–89% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Document consolidation: `OPEN_SOURCE_RECOMMENDATIONS.md` merged into this file; P12–P17 added for onboarding gaps; governance added to Resolved Items |
| 2026-03-21 | Antigravity AI | Session 6: `graph_ui.c` extraction (P2), float printf startup guard (P8), FLASH sector map docs (P16) resolved |
| 2026-03-21 | Antigravity AI | Session 7: Project update procedure integrated (workflow, documents, guidelines) |
| 2026-03-21 | Antigravity AI | Session 8/9: Global Hard QUIT navigation implemented; IDE/Build fixes (IntelliSense header fix, recursive include resolve, debug config fix, CMake build fix) |
