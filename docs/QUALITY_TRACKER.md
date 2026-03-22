# Project Quality & Open-Source Readiness

**Purpose:** Permanent register for code quality reviews, CI, refactoring, testing, and contributor-docs work. This is the single source of truth for all P-numbered improvement items. Feature work, bug fixes, and session planning live in `CLAUDE.md` — not here. Update this file when a quality item is opened, progressed, or resolved.

**Last reviewed:** 2026-03-22 (Session 20: PRGM command reference `docs/PRGM_COMMANDS.md` created — all 14 CTL and 6 I/O commands documented with syntax, edge cases, and limits table)
**Reviewer:** Claude Code (claude-sonnet-4-6)

---

## Overall Assessment

> **Verdict: Strong personal/research project. Not yet production-ready.**
>
> Exceptional documentation and hardware-correctness. The expression evaluator, RTOS integration,
> and FLASH handling show genuine embedded expertise. The module extraction series is complete:
> `graph_ui.c`, `ui_matrix.c`, `ui_prgm.c`, `expr_util.c` extracted following a consistent pattern.
> P18 (Session 15) resolved all 10 CODE_REVIEW_PENDING items: PRGM execution moved to `prgm_exec.c`,
> all 7 over-100-line functions split, and 3 quick-win doc/comment items fixed. Function complexity
> dimension promoted C+ → B. `docs/CODE_REVIEW_PENDING.md` deleted (all items resolved).

**Estimated production readiness:** 92–94%
*(gain from P18 resolution: function complexity C+ → B; all CODE_REVIEW_PENDING items cleared)*

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
| `-Werror` enabled | ✅ Done — P6 resolved 2026-03-21 |
| PRGM backend hardware validation | 🔴 Open — P10 |
| **Code Quality** | |
| `const` on `TI81_LookupTable` | ✅ Done |
| Colour palette constants (`ui_palette.h`) | ✅ Done |
| `-Wall -Wextra` compiler warnings | ✅ Done |
| `expr_util.c` extraction + host tests | ✅ Done |
| Module extraction from `calculator_core.c` | ✅ Done — P2 resolved 2026-03-21 |
| Function complexity reduction | ✅ Done — P18 resolved 2026-03-22 |
| Scattered static state consolidation | 🟡 Partial — P3 (Phase 1+2 done; Phase 3 optional) |
| Float printf runtime guard | ✅ Done — P8 |
| **Hardware Onboarding** | |
| Bill of Materials | ✅ Done |
| STM32 GPIO wiring table | ✅ Done |
| Physical TI-81 ribbon ↔ wire mapping | 🟡 Partial — P7 (ribbon pad mapping unverified) |
| **Contributor Documentation** | |
| Architecture diagram | ✅ Done — P12 resolved 2026-03-21 |
| Testing guide (`docs/TESTING.md`) | ✅ Done — P13 resolved 2026-03-21 |
| PRGM completion roadmap | ✅ Done — P14 resolved 2026-03-22 |
| Expression pipeline walkthrough | ✅ Done — P15 resolved 2026-03-22 |
| FLASH sector map in onboarding docs | ✅ Done — P16 |
| Troubleshooting guide | ✅ Done — P17 resolved 2026-03-21 |
| Periodic code review checklist | ✅ Done — all 10 items resolved 2026-03-22; file deleted |

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
| Function complexity | B | 2026-03-22 |
| Magic numbers / constants | A- | 2026-03-21 |
| Testing | B+ | 2026-03-21 |

---

## Strengths (Do Not Regress)

### Documentation (A+)
`CLAUDE.md` is exceptional — feature status, architectural decisions, gotchas, known issues, PCB
notes, and next-session priorities all clearly maintained. `README.md`, `GETTING_STARTED.md`, and
`TECHNICAL.md` provide clean onboarding.

### API and header design (A)
- Include guards correctly named (`GRAPH_MODULE_H` vs `GRAPH_H` collision avoided by design)
- Extern declarations match implementations across all modules
- Module prefixes consistent: `Calc_*`, `Graph_*`, `Persist_*`, `Keypad_*`
- No circular dependencies; all 10 App headers fully declare their public APIs

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

### Open-source scaffolding (fully complete)
`CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, issue templates (bug + feature), PR template, README
badges — all in place. No further action needed.

---

## Prioritised Improvement Roadmap

### By Ease of Resolution

| Rank | Item | Effort estimate | Notes |
|---|---|---|---|
| 1 | P1 — Test suite to A rating | 4–8 hrs | Property-based tests are well-scoped; PRGM arm gated on P10 |
| 2 | P3 — Handler state params (Phase 3) | 8–16 hrs | Every handler signature changes; high regression risk |
| 3 | P10 — PRGM hardware validation | ~2–4 hrs | Implementation complete; execute 28-item test plan in `docs/prgm_manual_tests.md` |
| 4 | P7 — Physical wiring table | Indefinite | Requires donor board, multimeter, photography; cannot be done in software |

### By Impact of Resolution

| Rank | Item | Scorecard dimension(s) | Why |
|---|---|---|---|
| 1 | P10 — PRGM hardware validation | Testing; feature completeness | Largest remaining feature gap; implementation complete, awaiting hardware sign-off on `docs/prgm_manual_tests.md` |
| 2 | P1 — Test suite to A rating | Testing B+→A | Hardens expression engine edge cases via property-based tests |
| 3 | P3 — Handler state params (Phase 3) | Code organisation; enables unit testing | Handlers that accept `State_t *` become host-testable in isolation |
| 4 | P7 — Physical wiring table | Hardware replication | Relevant only to contributors replicating the physical build |

---

## Open Issues

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
| **B+** ✅ | Pure logic extracted and tested on host; `persist.c` round-trip; CI pipeline |
| **A** | + Property-based invariant tests (e.g. sin²+cos²=1 for 1,000 x values); PRGM either extracted+tested or all 28 manual tests in `prgm_manual_tests.md` signed off |

**Remaining gaps:** `Calc_FormatResult` scientific notation not checked byte-for-byte. PRGM execution backend is coupled to LVGL; needs extraction (see P18 item 4) before host testing is practical.

**Status:** Partially resolved (2026-03-21 — B+ score achieved)

---

### P3 — Scattered static state

**Rating impact:** Code organisation = B
**File:** `App/Src/calculator_core.c`

Phase 1+2 complete (2026-03-21): 10 named sub-structs introduced; ~40 flat statics consolidated. The P2 extraction moved the 6 graph-related structs to `graph_ui.c` as private statics.

| Struct | Instance | Location | Fields |
|---|---|---|---|
| `MathMenuState_t` | `s_math` | `calculator_core.c` | tab, item_cursor, scroll_offset, return_mode |
| `TestMenuState_t` | `s_test` | `calculator_core.c` | item_cursor, return_mode |
| `ModeScreenState_t` | `s_mode` | `calculator_core.c` | row_selected, cursor[8], committed[8] |
| `MatrixMenuState_t` | `s_matrix_menu` | `calculator_core.c` | tab, item_cursor, return_mode |

**Remaining work (Phase 3 — optional):** Modify the 4 remaining handlers (`handle_math_menu`, `handle_test_menu`, `handle_mode_screen`, matrix handlers) to accept `State_t *` parameters instead of module-level statics. Once handlers accept structs, they become host-testable without LVGL.

**Status:** Partially resolved (Phase 1+2 complete 2026-03-21; Phase 3 optional)

---

### P7 — Incomplete physical wiring table

**Rating impact:** Documentation = A+ (risk to contributor hardware replication)
**File:** `docs/GETTING_STARTED.md`

STM32 GPIO side is documented and cross-referenced against `keypad.h`. ✅

**Remaining:** The physical correspondence between numbered pads on the TI-81 PCB and logical A-line/B-line names has not been traced with a multimeter. The wiring table contains a prominent warning block and an invitation for community contributions.

**Status:** Partially resolved (STM32 GPIO side complete; ribbon pad mapping pending hardware access)

---

### P10 — PRGM system not hardware-validated

**Rating impact:** Testing = D
**Files:** `App/Src/ui_prgm.c` (PRGM menu and editor UI), `App/Src/prgm_exec.c` (execution interpreter), `docs/prgm_manual_tests.md`

All 6 PRGM modes are dispatched from `Execute_Token`. The full UI (EXEC/EDIT/ERASE tabs, 37 slots, name entry, line editor, CTL/I/O sub-menus) is implemented in `ui_prgm.c`. The execution interpreter (`prgm_execute_line`, `prgm_run_loop`, `prgm_run_start`) was moved to `prgm_exec.c` in Session 15 (P18). All implementation gaps (IS>(, DS<(, DispHome, DispGraph, Output(, Menu() were resolved in Session 19. A full command reference is in `docs/PRGM_COMMANDS.md`.

**Ready for hardware validation.** Execute the 28-item test plan in `docs/prgm_manual_tests.md`.

Pre-flight checklist:
1. `cmake --preset Debug && cmake --build build/Debug` — firmware builds with 0 errors.
2. All 301 host tests pass: `cmake -S App/Tests -B build/tests && cmake --build build/tests && ./build/tests/test_calc_engine && ./build/tests/test_expr_util && ./build/tests/test_persist_roundtrip`
3. Flash via OpenOCD and power-cycle (USB unplug/replug) before starting tests.

When all 28 tests pass, mark P10 resolved with the date and add a Review History row.

**Status:** Open — implementation complete; hardware validation pending

---

---

## Resolved Items

| Item | Resolution | Date |
|---|---|---|
| P17 — Troubleshooting guide | `docs/TROUBLESHOOTING.md` created | 2026-03-21 |
| P13 — Testing guide | `docs/TESTING.md` created | 2026-03-21 |
| P12 — Architecture diagram | Mermaid diagrams added to `docs/ARCHITECTURE.md` | 2026-03-21 |
| P6 — Enable `-Werror` | `-Werror` applied to `App/` sources via `set_source_files_properties` in `CMakeLists.txt` | 2026-03-21 |
| P2 — `calculator_core.c` too large | `graph_ui.c` (1,579 LOC) extracted. `calculator_core.c` reduced 5,820 → 1,989 LOC (−66%). Code organisation B → B+. | 2026-03-21 |
| P8 — Float printf runtime guard | Startup assertion in `App_DefaultTask_Run()`: halts with 10 Hz LED blink if `-u _printf_float` is missing | 2026-03-21 |
| P4 — Magic numbers: colours | `ui_palette.h` created with 14 named constants; inline hex literals replaced across 5 files | 2026-03-21 |
| P5 — Missing `const` on `TI81_LookupTable` | `const` added to `TI81_LookupTable` and `TI81_LookupTable_Size` | 2026-03-20 |
| P9 — Matrix determinant recursion | `mat_det` confirmed fully iterative (Gaussian elimination); tracker description was wrong; no guard needed | 2026-03-21 |
| P11 — Duplicate `#include` directives | Duplicate `<stdio.h>`, `<stdlib.h>`, `<string.h>` includes removed from `calculator_core.c` | 2026-03-21 |
| P16 — FLASH sector map in docs | Sector table (sectors 0–11) + `[!CAUTION]` block added to `docs/TECHNICAL.md` | 2026-03-21 |
| Open-source governance | `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, issue templates, PR template, README badges — all complete | 2026-03-21 |
| Header audit | All 10 App headers correctly declare public APIs; no circular dependencies; A-grade confirmed | 2026-03-21 |
| `st-flash` references removed | `GETTING_STARTED.md` and `TECHNICAL.md` use OpenOCD commands throughout | 2026-03-21 |
| Y= Toggle | Equation enable/disable toggle from Y= editor; persistence v4 | 2026-03-22 |
| Graph stability fixes | Startup white screen, coordinate overlap, and trace/graph transition freezes resolved | 2026-03-22 |
| P15 — Expression pipeline walkthrough | Worked example ("2 + sin(45)") added to `docs/TECHNICAL.md` | 2026-03-22 |
| Matrix history display | Column-aligned rows, horizontal scroll via LEFT/RIGHT, `<`/`>` clip indicators; `HistoryEntry_t` embeds `CalcMatrix_t` copy | 2026-03-22 |
| P18 — Function complexity reduction | All 10 CODE_REVIEW_PENDING items resolved: PRGM execution moved to `prgm_exec.c` (−545 L from `ui_prgm.c`); `ShuntingYard` split into 3 helpers; `handle_yeq_mode` split into navigation+insertion; `ui_init_graph_screens` split into 4 per-screen helpers; `commit_history_entry` extracted from `handle_history_nav`; `render_result_row` extracted from `ui_refresh_display`; `try_tokenize_number` split; 3 doc/comment quick-wins. Function complexity C+ → B. | 2026-03-22 |
| P14 — PRGM completion roadmap | `docs/PRGM_COMPLETION.md` created: 5-task roadmap (stale comments, IS>/DS<, DispHome/DispGraph, Output(, Menu() with effort estimates, implementation notes, and acceptance criteria tied to the 28-item test plan. | 2026-03-22 |

---

## Review History

| Date | Reviewer | Notes |
|---|---|---|
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | Initial review; P1–P10 logged; rating 60–70% |
| 2026-03-20 | Claude Code (claude-sonnet-4-6) | P2 partly resolved (3 modules extracted, 37% smaller); P5/P7 resolved; P6 partly resolved; rating 65–75% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P1: 103-test suite; Testing D→C; rating 70–80% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P1 B score: 153 tests, gcov 80.28%, CI host-tests job; Testing C→B; rating 75–85% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P1 B+ score: `expr_util.c` extracted (96 tests), persist round-trip (52 tests), HAL guards; Testing B→B+; rating 80–88% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Full quality + documentation pass; header audit A-grade; 7 onboarding gaps identified; P4 resolved |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | Full codebase re-review; P9 resolved (tracker error); P11 added and resolved; rating 82–88% |
| 2026-03-21 | Claude Code (claude-sonnet-4-6) | P3 Phase 1+2: 10 named sub-structs, ~40 statics consolidated; Code organisation B-→B; rating 83–89% |
| 2026-03-21 | Antigravity AI | Session 6: P2 (`graph_ui.c` extraction), P8 (float printf guard), P16 (FLASH sector map) resolved |
| 2026-03-21 | Antigravity AI | Session 7: Project update procedure integrated |
| 2026-03-21 | Antigravity AI | Session 8/9: Global Hard QUIT; IDE/Build fixes |
| 2026-03-21 | Antigravity AI | Session 10: P6, P12, P13, P17 resolved. `-Werror` enabled; Architecture/Testing/Troubleshooting docs created. Rating 88–92% |
| 2026-03-22 | Antigravity AI | Session 11: Y= toggle; persistence v4; stability fixes. Rating 90–92% |
| 2026-03-22 | Antigravity AI | Session 12: P15 resolved — expression pipeline worked example added to `TECHNICAL.md` |
| 2026-03-22 | Antigravity AI | Session 13: Matrix history display — column-aligned rows, horizontal scroll, `HistoryEntry_t` matrix copy. RAM at 82.44% |
| 2026-03-22 | Antigravity AI | Session 14: Periodic code review. P18 opened (function complexity). `docs/CODE_REVIEW_PENDING.md` created with 10 action items. No regressions detected. Rating 90–92% unchanged. |
| 2026-03-22 | Antigravity AI | Session 15: P18 resolved — all 10 CODE_REVIEW_PENDING items complete. PRGM execution moved to `prgm_exec.c`; 7 over-100-line functions split; 3 doc quick-wins. Function complexity C+ → B. 301/301 tests pass. Rating 92–94%. |
| 2026-03-22 | Claude Code (claude-sonnet-4-6) | Session 16: Graph render speed optimized — `MATH_VAR_X` token + `GraphEquation_t` postfix cache; `Graph_Render` parse cost 320× → 1× per equation per frame. Complexity delta: neutral. 301/301 tests pass. No P-items opened or closed. |
| 2026-03-22 | Claude Code (claude-sonnet-4-6) | Session 17: `HistoryEntry_t` matrix ring buffer refactor — replaced embedded `CalcMatrix_t` (148 B) with 3-byte ring reference; 8-slot ring stores last 8 matrix results with generation-based eviction. RAM: 82.44% → 81.82%. Complexity delta: decrease. 301/301 tests pass. No P-items opened or closed. P10 description updated (stale prgm_exec.c reference fixed). |
| 2026-03-22 | Claude Code (claude-sonnet-4-6) | P14 resolved: `docs/PRGM_COMPLETION.md` created. Audited actual executor state (If/While/For/Goto/Lbl/Disp/Input/Prompt/ClrHome/prgm call all implemented since Session 15). Identified 5 remaining gaps: stale warning comments, IS>/DS<, DispHome/DispGraph, Output(, Menu(. Each has implementation notes, effort estimate, and acceptance criteria. Rating unchanged at 92–94%. |
| 2026-03-22 | Claude Code (claude-sonnet-4-6) | Session 18: RAM audit (CLAUDE.md item 12). Root cause: LVGL heap (`work_mem_int`, 64 KB) + FreeRTOS heap (`ucHeap`, 64 KB) = 128 KB = 65% of internal RAM; SDRAM had 63.5 MB free. Fix: `SDRAM` linker region added to `STM32F429XX_FLASH.ld`; `.sdram (NOLOAD)` section; `LV_ATTRIBUTE_LARGE_RAM_ARRAY` redirects LVGL heap to SDRAM. Internal RAM: 81.82% → 48.49%. Complexity delta: neutral. 301/301 tests pass. No P-items opened or closed. |
| 2026-03-22 | Claude Code (claude-sonnet-4-6) | Session 19: PRGM system feature-complete (all 5 tasks from `docs/PRGM_COMPLETION.md`). Stale warning comments updated; `IS>(` and `DS<(` added to executor and CTL menu; `DispHome` and `DispGraph` added to executor and I/O menu; `Output(` implemented with `ui_output_row()` helper; `Menu(` fully implemented with LVGL overlay screen, cursor/scroll state, and UP/DOWN/1–9/ENTER/CLEAR key handling. PRGM ~50% → ~95%. Complexity delta: neutral. 301/301 tests pass. P10 remains open (hardware validation). |
| 2026-03-22 | Claude Code (claude-sonnet-4-6) | Session 20: `docs/PRGM_COMMANDS.md` created — complete PRGM command reference covering all 14 CTL commands (If/Then/Else/End/While/For/Goto/Lbl/Pause/Stop/Return/prgm/IS>/DS<), all 6 I/O commands (Input/Prompt/Disp/ClrHome/DispHome/DispGraph), Output(, Menu(, expr->VAR, and expression lines; each entry includes syntax, edge cases, and behaviour details. Complexity delta: neutral. 301/301 tests pass. No P-items opened or closed. |
