# Project History

The single history archive for this project. Add a Session Log entry after every session. Add Completed Features / Resolved Items rows when work ships. Add a Milestone Reviews entry only when a rating changes or a major milestone is reached.

---

## Session Log

- 2026-03-20: PRGM UI polish, colour palette extraction (`ui_palette.h`), PRGM module extraction to `ui_prgm.c`
- 2026-03-21: `expr_util.c` extraction (9 pure functions), 301-test host suite, persist round-trip tests, HAL guards in `persist.c`, full quality review pass
- 2026-03-21 (Session 6): `graph_ui.c` extraction (P2), float printf runtime guard (P8), FLASH sector map docs (P16)
- 2026-03-21 (Session 7): Integrate project update procedure (workflow, documentation, guidelines)
- 2026-03-21 (Session 8): Implement Global Hard QUIT (2nd+CLEAR) navigation
- 2026-03-21 (Session 9): IDE/Build fixes (IntelliSense header fix, recursive include resolve, debug config fix, CMake build fix)
- 2026-03-22 (Session 10): P6, P12, P13, P17 resolved (Sweet Spot items). -Werror enabled for App; Architecture/Testing/Troubleshooting docs created.
- 2026-03-22 (Session 11): Implement Y= equation enable/disable toggle functionality. Update graph renderer and persistence (v4). Fixed a startup crash and multiple trace/graph transition freezes in `graph.c` and `graph_ui.c` (guards for zero-scale ticks, loop clamping for singularities, and LVGL mutex synchronization).
- 2026-03-22 (Session 12): [P15] Expression pipeline documented in `TECHNICAL.md` with "2 + sin(45)" worked example. `MAINTENANCE_STANDARDS.md` updated.
- 2026-03-22 (Session 13): Matrix history display refactored — column-aligned rows, horizontal scroll via LEFT/RIGHT when expression is empty, `<`/`>` clip indicators. `HistoryEntry_t` now embeds `CalcMatrix_t` copy. Build at 82.44% RAM.
- 2026-03-22 (Session 15): P18 resolved — all 10 CODE_REVIEW_PENDING items complete. PRGM execution logic moved from `ui_prgm.c` to `prgm_exec.c` (−545 L / +540 L). Over-100-line functions split: `ShuntingYard` (3 helpers), `handle_yeq_mode` (navigation+insertion), `ui_init_graph_screens` (4 per-screen), `handle_history_nav` (`commit_history_entry`), `ui_refresh_display` (`render_result_row`), `try_tokenize_number` (2 helpers). Docs: README links, ARCHITECTURE diagram, `calc_internal.h` scope comment. `CODE_REVIEW_PENDING.md` deleted. Function complexity C+ → B. 301/301 tests pass. **Complexity delta: `decrease`**.
- 2026-03-22 (Session 16): Graph render speed — added `MATH_VAR_X` token and `GraphEquation_t` postfix cache API to `calc_engine`. `Graph_Render` now runs Tokenize+ShuntingYard once per equation per render (on equation change only) and calls `Calc_EvalGraphEquation` per pixel column. Parse cost 320× → 1× per equation per frame. 301/301 tests pass. **Complexity delta: `neutral`**.
- 2026-03-22 (Session 17): `HistoryEntry_t` matrix ring buffer refactor — replaced embedded `CalcMatrix_t` (148 B) with 3-byte ring reference. 8-slot `matrix_ring[]` stores last 8 matrix results; generation counter detects eviction and falls back to pre-formatted `result` string. RAM: 82.44% → 81.82%. 301/301 tests pass. **Complexity delta: `decrease`**.
- 2026-03-22 (Session 18): RAM audit (P12) — root cause: LVGL heap + FreeRTOS heap = 128 KB = 65% of 192 KB internal RAM; SDRAM had 63.5 MB free. Fix: `SDRAM` region added to linker script, `.sdram (NOLOAD)` section, `LV_ATTRIBUTE_LARGE_RAM_ARRAY` redirects LVGL heap to SDRAM. RAM: 81.82% → 48.49%. 301/301 tests pass. **Complexity delta: `neutral`**.
- 2026-03-22 (Session 19): PRGM system feature-complete — `IS>(` and `DS<(` implemented; `DispHome`, `DispGraph`, `Output(`, `Menu(` implemented. PRGM: ~50% → ~95% (hardware validation pending, P10). 301/301 tests pass. **Complexity delta: `neutral`**.
- 2026-03-22 (Session 20): PRGM command reference created — `docs/PRGM_COMMANDS.md`. `docs/PRGM_COMPLETION.md` deleted. 301/301 tests pass. **Complexity delta: `neutral`**.
- 2026-03-22 (Session 21): Periodic code review — structural scan + Phase 2 direct reads. P19/P20 opened. VARS/Y-VARS menu specs added to CLAUDE.md. **Complexity delta: `neutral`**.
- 2026-03-22 (Session 22): P19 resolved — `prgm_execute_line` dispatch table refactor. 22 static `cmd_*` handler functions extracted; dispatch table replaces 495-line if/else chain; body reduced to ~30 lines. Zero logic changes. 301/301 tests pass. **Complexity delta: `decrease`**.
- 2026-03-22 (Session 23): P20 resolved — program execution host test suite. `#ifndef HOST_TEST` guards added throughout `prgm_exec.c`/`.h`; 121 tests / 14 groups. Bug fixed: `prgm_run_loop` Stop/Return/Goto-abort path did not reset `current_mode`. Testing B+ → A-. 422/422 tests pass. **Complexity delta: `neutral`**.
- 2026-03-25 (Session 27): PRGM manual test plan rewrite — comprehensive 50-test plan targeting Session 26 regressions. Test count: 40 → 50. **Complexity delta: `neutral`**.
- 2026-03-25 (Session 26): PRGM hardware test fixes — all 5 groups (A–E) executed. CTL menu → 8 items; I/O menu → 5 items; removed Then/Else/While/For/Return/Prompt/Output(/Menu( per spec; EXEC tab execution model implemented. Test suite: 378/378 pass (−44 tests for removed commands). Firmware: 48.45% RAM, 36.28% FLASH. **Complexity delta: `decrease`**.
- 2026-03-26 (Session 28): `QUALITY_TRACKER.md` renamed to `MAINTENANCE_STANDARDS.md`; restructured as process document; history moved to `PROJECT_HISTORY.md`; session log consolidated from `CLAUDE.md`. **Complexity delta: `neutral`**.
- 2026-03-27 (Session 29): PRGM hardware test fix groups F1–F10 — 9 failing tests addressed. F1: CLEAR aborts infinite loop (`prgm_request_abort()` called from keypadTask + `osDelay(0)` per line in `prgm_run_loop`). F2: `DispGraph` deadlock fixed (`osDelay(20)` before `Graph_Render` per gotcha #16). F3: EXEC tab added to in-editor CTL/IO sub-menu — slot picker screen, 3-way tab navigation, subroutine insertion (`prgmNAME`); new `MODE_PRGM_EXEC_MENU`. F4: TEST/MATH menus now accessible from program editor; insert redirected via `prgm_editor_menu_insert()`. F5: Name-entry DEL re-engages ALPHA; ALPHA_LOCK routing fixed. F6/F7: ALPHA+letter and digit slot shortcuts work in all 3 PRGM tabs. F8: INS toggles insert mode in editor; underscore cursor shape for insert. F9: `Input` prompt always shows `?` only. F10: Arrow navigation in name-entry (`prgm_new_name_cursor`); LEFT/RIGHT move in field; DOWN opens editor body; UP at editor line 0 returns to name entry. 378/378 tests pass. Firmware: 48.48% RAM, 36.45% FLASH. **Complexity delta: `increase`** (`ui_prgm.c` grew ~200 lines: new EXEC sub-menu screen + handler + cursor state; see complexity item in CLAUDE.md).
- 2026-03-31 (Session 30): Cursor insert+modifier fix — `LV_OBJ_FLAG_OVERFLOW_VISIBLE` on cursor box; `in_insert` condition simplified (removed `current_mode` guards) so mode indicator (^/A) renders above the underline cursor in insert+2ND/ALPHA. `handle_yeq_insertion` in `graph_ui.c` refactored to use `ExprUtil_InsertChar`/`InsertStr` (−20 lines duplicate logic). `test_prgm_exec` expanded by 18 tests (empty body, slot lookup, 2-level nesting): 378 → 396 total. P26/P27/P28 refactor items added to CLAUDE.md. Full doc-sync pass: test counts corrected in `GETTING_STARTED.md`, `README.md`, `TECHNICAL.md`. **Complexity delta: `decrease`** (graph_ui.c deduplication + calculator_core.c simplification).
- 2026-04-01: P25 resolved — `docs/PRGM_COMMANDS.md` rewritten to match post-Session-26 8-CTL/5-IO spec. Removed Then/Else/While/For(/Return/Prompt/Output(/Menu( entries; updated If (single-line only), Goto/Lbl (single-char constraint), Input (shows `?`), End (no-op note); documented EXEC tab `prgm<name>` insertion model; removed stale Limits row. **Complexity delta: `neutral`**.
- 2026-04-01: `handle_prgm_editor` split — extracted `prgm_editor_handle_nav`, `prgm_editor_handle_del_clear`, `prgm_editor_handle_insert` as three focused static helpers in `ui_prgm.c`. Main dispatcher reduced from 152 → 40 lines. Zero logic change. **Complexity delta: `neutral`**.
- 2026-04-02: `handle_normal_mode` host test suite — 104 tests / 10 groups covering all 8 static sub-handlers. `calculator_core_test_stubs.h` (comprehensive LVGL/RTOS/graph_ui/ui_matrix/ui_prgm stub layer), minimal HOST_TEST guards in `calculator_core.c` (include block, `sto_pending` visibility, `lvgl_lock` bodies). Bug fixed: `commit_history_entry` self-copy when re-evaluating only history slot (pointer equality guard). Suite 412 → 516 total. **Complexity delta: `neutral`**.
- 2026-04-02: Item 18 display-string replacements — Y= row labels → Y₁–Y₄ (`graph_ui.c:154`); TOKEN_X_INV display-only mappings → ⁻¹ U+E001 (`graph_ui.c`, `ui_prgm.c`). Expression buffer `^-1` kept as intentional deviation (Option B). VARS/Y-VARS strings deferred to when those menus are implemented. **Complexity delta: `neutral`**.
- 2026-04-02: `handle_prgm_menu` split — extracted `handle_erase_confirm`, `enter_exec_tab`, `enter_edit_tab`, `enter_erase_tab` as focused static helpers in `ui_prgm.c`. Dispatcher ~120 → ~50 lines. Zero logic change. `graph_ui.c` pre-existing implicit-fallthrough `-Werror` fixed (`__attribute__((fallthrough))`). **Complexity delta: `neutral`**.
- 2026-04-02: P3 Phase 3 complete — `handle_mode_screen`, `handle_math_menu`, `handle_test_menu`, `handle_matrix_menu` now accept explicit state struct pointers (`ModeScreenState_t *`, `MathMenuState_t *`, `TestMenuState_t *`, `MatrixMenuState_t *`) instead of reading module-level statics. `MatrixMenuState_t` moved from `calculator_core.c` to `ui_matrix.h` (now public extern `matrix_menu_state`); the three former `matrix_tab`/`matrix_item_cursor`/`matrix_return_mode` statics replaced by the consolidated struct. History buffer reduced to TI-81 spec: `HISTORY_LINE_COUNT` 32 → 1, `MATRIX_RING_COUNT` 8 → 1 (~7 KB RAM recovered). Removes the two "Deliberate Deviations" for UP/DOWN multi-step history scroll and `2nd+ENTRY` offset behaviour. Zero logic change — circular buffer math and `handle_history_nav` work correctly at size 1. **Complexity delta: `decrease`** (RAM reduction, state consolidation, handler testability).
- 2026-04-02: P34 parametric graphing complete — `MATH_VAR_T` token + `Calc_PrepareParamEquation`/`Calc_EvalParamEquation` API in `calc_engine`; `GRAPH_NUM_PARAM 3` pairs + `t_min/max/step` + `param_mode` in `GraphState_t`; `PersistBlock_t` v5 (864 → 1264 bytes); `Graph_RenderParametric` + `Graph_InvalidateCache` in `graph.c`; `ui_param_yeq_screen` + `handle_param_yeq_mode` + 9-field parametric RANGE layout in `graph_ui.c`; MODE row 4 handler, `TOKEN_X_T` mode-sensitive insert, `MODE_GRAPH_PARAM_YEQ` dispatch in `calculator_core.c`; 28-assertion `test_param.c` host test suite. 516 → 449 total (prior count was assertions, now tracked per-suite). **Complexity delta: `increase`** (new mode, new screen, tokenizer/evaluator param flags, renderer branch; see P35 complexity item in CLAUDE.md).
- 2026-04-03: P30 STAT menu complete — `StatData_t`/`StatResults_t` + `STAT_MAX_POINTS 99` in `app_common.h`; `MODE_STAT_MENU/EDIT/RESULTS` in `CalcMode_t`; `calc_stat.h`/`calc_stat.c` (1-Var, LinReg, LnReg, ExpReg, PwrReg, SortX, SortY, Clear); `PersistBlock_t` v6 (1264 → 2060 bytes, +stat list 796 B); `ui_stat.h`/`ui_stat.c` (~470 lines, three screens + handlers); `Graph_DrawScatter`/`Graph_DrawXYLine`/`Graph_DrawHistogram` in `graph.c`; `TOKEN_STAT` dispatch + persist + screen init in `calculator_core.c`; 39-assertion `test_stat.c` host test suite; 7 test executables, 488 total assertions. **Complexity delta: `increase`** (new module, 3 modes, 3 screens, 3 graph renderers, +796 B persist; see P30 complexity item in CLAUDE.md).

---

## Completed Features

| Feature | Log date | Notes |
|---|---|---|
| **Build** | — | Successful (fixed implicit fallthrough in `graph_ui.c`; startup crash and trace freeze in `graph.c`) |
| **Flash** | — | Successful using OpenOCD to STM32F429I-DISC1 |
| ZBox rubber-band zoom | 2026-03-20 | Final validation on hardware pending |
| UTF-8 cursor navigation | 2026-03-21 | 12 test groups; `expr_util.c` extracted |
| Persist checksums/HAL | 2026-03-21 | FLASH sector 7 guard added; versioned header |
| Graph UI extraction | 2026-03-21 | Removed 1.5k LOC from `calculator_core.c` (P2) |
| Project Update Procedure | 2026-03-21 | Canonical sync rules + workflow automated |
| Global Hard QUIT | 2026-03-22 | 2nd+CLEAR hard exit to main screen implemented |
| Y= Toggle | 2026-03-22 | Equation enable/disable toggle from Y= editor implemented |

---

## Resolved Items

| Item | Summary | Date |
|---|---|---|
| P5 | `const` on `TI81_LookupTable` | 2026-03-20 |
| P4 | `ui_palette.h` — 14 named colour constants; inline hex literals replaced | 2026-03-21 |
| P9 | `mat_det` confirmed iterative; tracker entry was incorrect | 2026-03-21 |
| P11 | Duplicate `#include` directives removed from `calculator_core.c` | 2026-03-21 |
| P6 | `-Werror` enabled for App sources | 2026-03-21 |
| P8 | Float printf runtime guard in `App_DefaultTask_Run` | 2026-03-21 |
| P2 | `graph_ui.c` extracted — `calculator_core.c` −66% | 2026-03-21 |
| P16 | FLASH sector map added to `docs/TECHNICAL.md` | 2026-03-21 |
| P12 | Architecture diagram — Mermaid added to `docs/ARCHITECTURE.md` | 2026-03-21 |
| P13 | `docs/TESTING.md` created | 2026-03-21 |
| P17 | `docs/TROUBLESHOOTING.md` created | 2026-03-21 |
| Open-source governance | `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, issue/PR templates, README badges | 2026-03-21 |
| P15 | Expression pipeline walkthrough added to `docs/TECHNICAL.md` | 2026-03-22 |
| P18 | All 10 `CODE_REVIEW_PENDING` items resolved — function complexity C+ → B | 2026-03-22 |
| P19 | `prgm_execute_line` dispatch table — 22 handlers, body 495 → ~30 lines | 2026-03-22 |
| P20 | prgm exec host suite — 121 tests, `prgm_run_loop` bug fixed; Testing B+ → A- | 2026-03-22 |
| P21–P24 | Promoted from retired `CODE_REVIEW_PENDING.md` to CLAUDE.md next priorities | 2026-03-25 |
| P25 | `docs/PRGM_COMMANDS.md` rewritten — 8-CTL/5-IO spec, removed 6 unsupported commands | 2026-04-01 |
| `handle_prgm_editor` split | Extracted 3 static helpers; dispatcher 152 → 40 lines; `ui_prgm.c` | 2026-04-01 |
| `handle_normal_mode` tests | 104-test suite; `calculator_core_test_stubs.h`; HOST_TEST guards; self-copy bug fixed | 2026-04-02 |
| P3 Phase 3 | State params for 4 handlers; `MatrixMenuState_t` public; history buffer 32→1; ~7 KB RAM freed | 2026-04-02 |
| History spec alignment | `HISTORY_LINE_COUNT` 32→1, `MATRIX_RING_COUNT` 8→1; UP/DOWN scroll and `2nd+ENTRY` deviation removed | 2026-04-02 |
| Item 18 | Y= row labels → Y₁–Y₄; TOKEN_X_INV display-only → ⁻¹ U+E001; `^-1` expression insertion kept (Option B); VARS/Y-VARS strings deferred | 2026-04-02 |
| `handle_prgm_menu` split | Extracted `handle_erase_confirm`, `enter_exec_tab`, `enter_edit_tab`, `enter_erase_tab`; dispatcher ~120 → ~50 lines; zero logic change; `graph_ui.c` implicit-fallthrough fixed | 2026-04-02 |
| P34 | Parametric graphing — `MATH_VAR_T`, `Calc_PrepareParamEquation`/`Calc_EvalParamEquation`, `GraphState_t` param fields, persist v5, `Graph_RenderParametric`/`Graph_InvalidateCache`, param Y= screen + 9-field RANGE + `handle_param_yeq_mode`, `TOKEN_X_T` + MODE row 4 + `MODE_GRAPH_PARAM_YEQ` dispatch; 28-test `test_param.c`; hardware validation pending (P35h) | 2026-04-02 |
| P30 | STAT menu — `StatData_t`/`StatResults_t` in `app_common.h`; `calc_stat.c` (1-Var, LinReg, LnReg, ExpReg, PwrReg, SortX, SortY, Clear); persist v6 (+796 B stat list); `ui_stat.c` (3 screens: tab-menu, DATA editor, results); `Graph_DrawScatter`/`XYLine`/`Histogram` in `graph.c`; `TOKEN_STAT` dispatch in `calculator_core.c`; 39-test `test_stat.c`; hardware validation pending (P30h) | 2026-04-03 |

---

## Milestone Reviews

| Date | Notes |
|---|---|
| 2026-03-20 | Initial review; P1–P10 logged; rating 60–70% |
| 2026-03-21 | Testing D → C → B → B+ across multiple sessions. Full quality pass; header audit A-grade. Rating 80–88%. |
| 2026-03-21 | Sessions 6–10: P2/P6/P8/P12/P13/P16/P17 resolved. `-Werror` enabled; Architecture/Testing/Troubleshooting docs created. Rating 88–92%. |
| 2026-03-22 | Sessions 11–13: Y= toggle, persist v4, matrix history display. RAM 82.44%. |
| 2026-03-22 | Session 15: P18 — function complexity C+ → B. Rating 92–94%. |
| 2026-03-22 | Sessions 16–18: Graph postfix cache; matrix ring buffer; LVGL heap → SDRAM. RAM 48.49%. |
| 2026-03-22 | Sessions 19–23: PRGM feature-complete (~95%); dispatch table (P19); 121-test exec suite (P20). Testing B+ → A-. Rating 93–95%. |
| 2026-03-25 | Sessions 26–28: PRGM spec alignment −44 tests (suite 378/378); manual test plan 40 → 50; `CODE_REVIEW_PENDING.md` retired; P21–P24 promoted. |
| 2026-03-26 | `QUALITY_TRACKER.md` renamed to `MAINTENANCE_STANDARDS.md`; restructured as process document; history moved here. |
| 2026-04-01 | P28 complete: `cursor_place()` → `cursor_render()`. All 7 cursor-update functions now pass explicit `visible`/`mode`/`insert` params; no global state read. `MODE_STO` synthetic enum added. API/header design A- → A. Rating 90–92%. Complexity delta: **decrease**. |
| 2026-04-01 | Refactor+docs batch (session 30): P21–P27 static helper extractions across graph_ui.c/calculator_core.c/app_init.c; MAINTENANCE_STANDARDS.md/TECHNICAL.md stale-reference cleanup; P1 property-based tests (sin²+cos²=1 ×1000, Calc_FormatResult sci-notation boundaries). Testing A- → A. Suite 412/412. Rating 91–93%. |
| 2026-04-02 | `handle_normal_mode` host tests: 104 tests / 10 groups; calculator_core_test_stubs.h stub layer; self-copy bug fixed in commit_history_entry. Suite 412 → 516. Testing remains A. Rating 91–93%. |
| 2026-04-02 | P3 Phase 3 complete: state params for 4 handlers; `MatrixMenuState_t` public; history buffer 32→1; ~7 KB RAM freed; TI-81 history deviation resolved. Complexity delta: **decrease**. Rating 91–93%. |
