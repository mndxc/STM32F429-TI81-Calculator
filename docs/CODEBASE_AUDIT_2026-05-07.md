# STM32F429-TI81-Calculator — Codebase Audit

**Generated:** 2026-05-07
**Auditor scope:** Static analysis of headers, source file preambles, and grep-based cross-cutting checks. Human review items are marked explicitly.
**Reference architecture:** Three-layer model per docs/ARCHITECTURE.md — Hardware Layer, Application Core (host-testable), UI Logic (embedded-only).

---

## A. Audit Dimensions and Pass/Fail Criteria

### A1. File-Level Documentation

| Grade | Criteria |
|-------|----------|
| PASS | `.c` file opens with a `/** @file ... @brief ... */` Doxygen block that names the module's single responsibility. Matching `.h` file has the same block. |
| FAIL | `.c` or `.h` file has no opening doc block, or the `@brief` is a grab-bag of unrelated concerns, or the `.c` brief contradicts the `.h` brief. |

### A2. Header Hygiene

| Grade | Criteria |
|-------|----------|
| PASS | Header includes only what its callers need. Include guard is `#ifndef MODULE_H` / `#define` / `#endif`. No implementation detail (static variables, static non-inline functions) leaks into the header. `static inline` helpers are acceptable in headers only when they are zero-overhead and have no mutable side effects. |
| FAIL | Header includes implementation headers that callers do not need. Header uses `#pragma once` inconsistently with the rest of the codebase. Static non-inline variables declared at file scope in a header (ODR violation risk). |

### A3. Interface Width

| Grade | Criteria |
|-------|----------|
| PASS | Every symbol used by only one translation unit is `static`. No raw `extern` variable declarations in `.c` files (all externally-shared globals go through their owning header). Public API width is proportional to the module's responsibility surface. |
| FAIL | Non-`static` globals in `.c` that are not declared in the owning `.h`. Raw `extern` function or variable declarations in `.c` files. Public API wider than the module's callers require. |

### A4. Layer Discipline

| Grade | Criteria |
|-------|----------|
| PASS | Application Core files contain zero direct LVGL calls and zero direct LVGL type references. UI Logic files call into Application Core only through declared header interfaces; they do not call each other laterally without going through a named interface. |
| FAIL | App Core source file contains `lv_label`, `lv_obj`, `lvgl_lock`, or `#include "lvgl.h"` outside a `HOST_TEST` guard. App Core source includes a UI Logic header in the non-`HOST_TEST` path. UI Logic files use raw `extern` to reach internal state of a sibling UI Logic module. |

### A5. Function Complexity

| Grade | Criteria |
|-------|----------|
| PASS | No function exceeds 60 lines. Cyclomatic depth does not exceed 4. Exception: well-documented, single-responsibility state-machine dispatchers may exceed line limits with a justifying comment. |
| FAIL | Function exceeds 60 lines without a documented exception rationale. Nesting depth reaches 5+. A single function conflates two or more separable responsibilities. |

### A6. Naming Convention

| Grade | Criteria |
|-------|----------|
| PASS | Public functions use the `Module_VerbNoun` pattern. Private helpers use `snake_case`. Enum values use a consistent prefix tied to their type (`MODE_*`, `MATH_*`, `CALC_ERR_*`, `TOKEN_*`). |
| FAIL | Mixed casing within a module's public API. Enum values with no type prefix. Abbreviations that collide across modules. |

### A7. Inline Debt

| Grade | Criteria |
|-------|----------|
| PASS | Zero `TODO`, `FIXME`, `HACK`, or `XXX` markers in source or headers. |
| FAIL | Any such marker exists. |

---

## B. Per-Module Audit Checklist

### Tier 1: Application Core (host-testable)

#### `calc_engine.c` / `calc_engine.h` — 1791 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both `.c` and `.h` have Doxygen blocks. `.c` describes the three-pass pipeline clearly. |
| Header hygiene | ✅ PASS | `calc_engine.h` uses `#ifndef` guard. Includes only `<stdint.h>` and `<stdbool.h>`. No LVGL dependency. |
| Interface width | ⚠️ REVIEW | Two `extern` variables (`calc_variables[27]`, `calc_matrices[4]`) are exported directly — assess whether all fields need to be globally mutable or whether an accessor seam would better control write access. |
| Layer discipline | ✅ PASS | Zero LVGL calls confirmed by grep. No UI Logic headers included. |
| Function complexity | ❌ FAIL | `EvaluateRPN_ex` (lines 1268–1400, 132 lines) dispatches 53 `MATH_*` token types with 7 top-level `if (tt == ...)` chains at nesting depth 4–5. Sub-dispatchers `eval_unary_func`, `eval_binary_op`, `eval_mat_arith`, `eval_matrix_func` are partially extracted but the top-level function still exceeds the 60-line limit. Confirmed pre-existing issue per CLAUDE.md. |
| Naming convention | ✅ PASS | `Calc_` prefix consistent. `MathTokenType_t` enum uses `MATH_` prefix. `CalcError_t` uses `CALC_ERR_`. |
| Inline debt | ✅ PASS | Zero markers found. |

**Human review:** Confirm that the `extern calc_variables` / `extern calc_matrices` exposure is intentional and does not require a write-notification pattern (e.g., for cache invalidation on write).

---

#### `calc_stat.c` / `calc_stat.h` — 300 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both files have Doxygen blocks. `.h` explicitly states "No LVGL, HAL, or RTOS dependencies." |
| Header hygiene | ✅ PASS | Includes only `app_common.h`. |
| Interface width | ✅ PASS | Small, focused API: `CalcStat_Compute1Var`, `CalcStat_ComputeLinReg`, `CalcStat_Sort`, `CalcStat_Clear`. |
| Layer discipline | ✅ PASS | Zero LVGL calls confirmed. |
| Function complexity | ⚠️ REVIEW | Not verified by line-level inspection. Confirm no individual stat function exceeds 60 lines. |
| Naming convention | ✅ PASS | `CalcStat_` prefix consistent. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `calc_history.c` / `calc_history.h` — 135 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both have Doxygen blocks. |
| Header hygiene | ❌ FAIL | `calc_history.h` declares `CalcHistory_UpdateDisplay()` but it is **defined** in `calculator_core.c`. Cross-module function ownership violation: any reader of `calc_history.h` who assumes the implementation is in `calc_history.c` will be wrong. Root cause: the function needs LVGL objects owned by `calculator_core.c`. Fix: provide it as a callback or move display objects to a dedicated display-layer module (see ARCH_OPPS item 4). |
| Interface width | ⚠️ REVIEW | `CalcHistory_UpdateDisplay()` is a UI concern implemented in `calculator_core.c`. Confirm whether it should be replaced by a callback. |
| Layer discipline | ✅ PASS | `calc_history.c` itself has no LVGL calls. The coupling is in `calculator_core.c`'s implementation of the declared function. |
| Function complexity | ✅ PASS | 135-line file; no individual function likely exceeds limits. |
| Naming convention | ✅ PASS | `CalcHistory_` prefix consistent. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `expr_util.c` / `expr_util.h` — 144 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both have Doxygen blocks. `.h` explicitly notes "no dependency on LVGL, FreeRTOS, or HAL." |
| Header hygiene | ✅ PASS | Clean. Self-contained with `<stdint.h>` and `<stdbool.h>`. Provides `MAX_EXPR_LEN` fallback for standalone use. |
| Interface width | ✅ PASS | Small, well-scoped API for UTF-8 expression buffer manipulation. |
| Layer discipline | ✅ PASS | Zero LVGL calls. |
| Function complexity | ✅ PASS | Small file; all functions expected to be short. |
| Naming convention | ✅ PASS | `ExprUtil_` and `ExprBuffer_` prefixes consistent. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `menu_state.c` / `menu_state.h` — 61 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both have Doxygen blocks. |
| Header hygiene | ⚠️ REVIEW | `menu_state.h` declares `MenuState_AbsoluteIndex` as `static inline` (acceptable). Confirm there are no other non-inline statics in this header. |
| Interface width | ✅ PASS | Minimal — `MenuState_t` type plus one inline helper. |
| Layer discipline | ✅ PASS | Zero LVGL calls confirmed. |
| Function complexity | ✅ PASS | Trivially small. |
| Naming convention | ✅ PASS | `MenuState_` prefix consistent. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `persist.c` / `persist.h` — 341 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both have Doxygen blocks. `persist.h` documents the sub-struct versioning model clearly. |
| Header hygiene | ✅ PASS | `persist.h` includes only `app_common.h` and `calc_engine.h`. LVGL dependency is guarded `#ifndef HOST_TEST` in the `.c` file. |
| Interface width | ✅ PASS | Appropriate: `Persist_BuildBlock`, `Persist_ApplyBlock`, `Persist_Save`, `Persist_Load`, `Persist_Reset`, `Persist_Validate`, `Persist_Checksum`. |
| Layer discipline | ⚠️ REVIEW | `persist.c` includes `calculator_core.h` (UI Logic layer) in non-HOST_TEST builds to access `Calc_GetAns`, `Calc_SetAnsScalar`, `Calc_SetAngleDegrees`. ANS and angle-mode state should be accessible through App Core headers (`calc_engine.h` or a dedicated state header). |
| Function complexity | ⚠️ REVIEW | `Persist_BuildBlock` and `Persist_ApplyBlock` are the largest functions. Confirm neither exceeds 60 lines at depth ≤ 4. |
| Naming convention | ✅ PASS | `Persist_` prefix consistent. Sub-struct types use `GraphPersist_t`, `StatPersist_t` pattern. |
| Inline debt | ✅ PASS | Zero markers found. |
| **CRITICAL DEFECT** | ❌ FAIL | **FLASH sector collision:** `persist.h:45,47` defines `PERSIST_FLASH_ADDR = 0x080E0000` / `PERSIST_SECTOR = FLASH_SECTOR_11`. `prgm_exec.h:47,49` defines identical values. Both erase the full 128 KB sector before writing. A `Persist_Save()` call destroys all program storage; a `Prgm_Save()` call destroys all persist state. Combined data ≈ 21 KB, fits within 128 KB. Fix: sub-sector-offset layout or consecutive sectors. |

---

#### `prgm_exec.c` / `prgm_exec.h` — 907 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both files have Doxygen blocks. `.h` documents the FLASH layout, slot scheme, and the `PrgmOutput_t` callback seam. |
| Header hygiene | ✅ PASS | `prgm_exec.h` guards its HAL include properly. Exports a clean `PrgmOutput_t` callback interface for I/O decoupling. |
| Interface width | ⚠️ REVIEW | `prgm_exec.c` line 146 uses `extern ProgramStore_t g_prgm_store` inside `#ifdef HOST_TEST`. Acceptable for test access, but the raw `extern` in a `.c` file is a pattern to flag. Some public functions break the `Module_VerbNoun` capitalization convention (see naming below). |
| Layer discipline | ❌ FAIL | `prgm_exec.c` includes `calculator_core.h` (UI Logic layer) in non-HOST_TEST builds. Calls `format_calc_result()` (UI formatting function defined in `calculator_core.c`) and `Calc_GetExpr()` → `ExprBuffer_Clear()` (expression buffer state owned by the UI layer). **File references:** `prgm_exec.c:19–21`, `prgm_exec.c:474`, `prgm_exec.c:503`. |
| Function complexity | ⚠️ REVIEW | `prgm_execute_line` and `prgm_run_loop` are the largest functions. Verify no individual command handler exceeds 60 lines. |
| Naming convention | ❌ FAIL | Public functions `prgm_run_start`, `prgm_run_loop`, `prgm_lookup_slot`, `prgm_request_abort`, `prgm_is_waiting_input`, `prgm_get_input_var`, `prgm_clear_input_wait`, `prgm_cmd_table_validate` use `snake_case` in the public header instead of `Module_VerbNoun`. |
| Inline debt | ✅ PASS | Zero markers found. |
| **CRITICAL DEFECT** | ❌ FAIL | (Same FLASH sector collision as persist.h.) Also: `prgm_exec.h:15–17` comment says "Sector 10: 0x080C0000 — calculator variables" — **stale** since commit `cf931ab` moved persist to sector 11. |

---

### Tier 2: UI Logic (embedded-only)

#### `calculator_core.c` / `calculator_core.h` — 1713 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ❌ FAIL | `.c` `@brief` says "Calculator logic, UI management, and FreeRTOS task implementation" — grab-bag description confirming the four-responsibility problem: (1) LVGL UI object creation, (2) expression editing, (3) history management, (4) mode routing. The `.h` brief is narrow and correct but does not match the sprawling scope of the `.c` file. |
| Header hygiene | ✅ PASS | `calculator_core.h` includes only `<stdbool.h>`, `app_common.h`, `expr_util.h`, and `calc_engine.h`. No LVGL dependency in the header. |
| Interface width | ⚠️ REVIEW | `calculator_core.h` exposes `format_calc_result()`, `ui_refresh_display()`, `ui_output_row()`, `handle_history_nav()` as "Super-module internal display/nav functions." This is lateral coupling between UI modules through the calculator_core header rather than through each module's own declared interface. |
| Layer discipline | ⚠️ REVIEW | `calculator_core.c` includes 17 UI module headers in the non-HOST_TEST path. `CalcHistory_UpdateDisplay()` is defined here (declared in `calc_history.h`) — ownership belongs in the display layer. |
| Function complexity | ❌ FAIL | Confirmed pre-existing issue. `calculator_core.c` carries four independent responsibilities without an extracted interface for each. |
| Naming convention | ✅ PASS | Public API uses `Calc_` prefix. Internal helpers are `snake_case`. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `graph_ui.c` / `graph_ui.h` — 1403 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | `.c` has a Doxygen block. |
| Header hygiene | ✅ PASS | `graph_ui.h` is narrowly scoped. |
| Interface width | ⚠️ REVIEW | `graph_ui.c` owns both the Y= editor state machine and six live graph-canvas mode handlers. These six canvas-mode handlers share file space with the Y= editor but have no shared mutable state with it. |
| Layer discipline | ✅ PASS | All LVGL calls are in the UI Logic layer as expected. |
| Function complexity | ❌ FAIL | `handle_trace_mode` spans lines 1100–1252 (152 lines, nesting depth 6). Confirmed per CLAUDE.md. Handles: (a) LEFT/RIGHT step with wrapping, (b) parametric vs. function step size selection, (c) equation switching, (d) re-render, (e) TRACE→GRAPH transition, (f) TRACE→YEQ transition. Each of (a)–(f) could be extracted as a named helper. |
| Naming convention | ✅ PASS | `Graph_*` and `handle_*` patterns consistent within module. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `graph.c` / `graph.h` — 1336 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both have Doxygen blocks. |
| Header hygiene | ❌ FAIL | `graph.h` includes `lvgl.h` at line 10 (with a HOST_TEST guard that stubs `lv_obj_t`). Any module including `graph.h` for `GraphState_t` transitively pulls in the LVGL header chain in embedded builds. `GraphState_t` is defined in `app_common.h`, so modules that only need the state type can use `app_common.h` instead. `graph.h` also exports `Graph_Init(lv_obj_t *parent)` which genuinely needs the LVGL type, so the include cannot be fully removed — this is a design tension. Separating the render API from the state accessor API would allow a narrower interface. |
| Interface width | ✅ PASS | 40 public function declarations — large but justified by the breadth of graph functionality. |
| Layer discipline | ✅ PASS | Render-layer module; LVGL dependency is expected. |
| Function complexity | ⚠️ REVIEW | `Graph_Render` and `Graph_RenderParametric` are the main candidates. Verify neither exceeds 60 lines after the coordinate transform deduplication via `graph_coord.h`. |
| Naming convention | ✅ PASS | `Graph_` prefix consistent. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `graph_ui_range.c` / `graph_ui_range.h` — 743 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | `.c` has a clear Doxygen block. Documents the `FieldEditor_t` generic infrastructure. |
| Header hygiene | ✅ PASS | Clean includes. |
| Interface width | ✅ PASS | Appropriately scoped to RANGE and ZOOM FACTORS screen management. |
| Layer discipline | ✅ PASS | UI Logic layer; LVGL dependency expected. |
| Function complexity | ⚠️ REVIEW | 743 lines handling two editors. Verify `handle_range_mode` and `handle_zoom_factors_mode` individually stay within 60 lines and depth ≤ 4. The shared `FieldEditor_t` / `field_editor_handle()` pattern is a positive structural signal. |
| Naming convention | ✅ PASS | |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `ui_prgm.c` / `ui_prgm.h` — 973 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | `.c` has a clear Doxygen block naming three sub-responsibilities (slot browser, name-entry, output adapter). |
| Header hygiene | ❌ FAIL | `ui_prgm.h` includes `lvgl.h` (line 4). `prgm_exec.c` includes `ui_prgm.h` (line 20) in the non-HOST_TEST path — this is the mechanism by which `prgm_exec.c` acquires a transitive LVGL dependency. The declarations that `prgm_exec.c` needs from `ui_prgm.h` (`Prgm_GetLine`, `Prgm_GetNumLines`, `prgm_parse_from_store`) are LVGL-free and could be split into a narrower `prgm_store_access.h`. |
| Interface width | ⚠️ REVIEW | `ui_prgm.h` exports `prgm_flatten_to_store()` as "a thin wrapper — kept for sub-menu files that have not yet been migrated." Migration to `prgm_editor.h` should be completed. |
| Layer discipline | ✅ PASS | UI Logic layer; expected. |
| Function complexity | ⚠️ REVIEW | Verify `handle_prgm_menu`, `handle_prgm_new_name`, and `handle_prgm_running` stay within 60 lines and depth ≤ 4. |
| Naming convention | ⚠️ REVIEW | Mixed: some public functions use `prgm_` lowercase prefix (`prgm_slot_id_str`, `prgm_reset_state`) while others use `Prgm_` capitalized. Verify whether lowercase variants were intended as internal but accidentally exported. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `ui_matrix.c` / `ui_matrix.h` — 579 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ❌ FAIL | `ui_matrix.c` has no file-level Doxygen block. Starts directly with `#include "ui_matrix.h"`. |
| Header hygiene | ⚠️ REVIEW | `ui_matrix.h` exports `extern MenuState_t matrix_menu_state`. Check whether this extern is needed by `calculator_core.c` or can be replaced with an accessor function. |
| Interface width | ❌ FAIL | **Most severe interface-width violation in the codebase.** `ui_matrix.c` lines 13–24 declare 9 non-`static` global variables (`ui_matrix_screen`, `ui_matrix_edit_screen`, `matrix_menu_state`, `matrix_edit_idx`, `matrix_edit_cursor`, `matrix_edit_scroll`, `matrix_edit_dim_field`, `matrix_edit_buf`, `matrix_edit_len`, `matrix_edit_val_cursor`) without `static`. Only `matrix_menu_state` has a matching `extern` in `ui_matrix.h`; the remaining 8 are exported by linkage without any declaration in the public header. Additionally, `ui_matrix.c` lines 49–51 contain raw `extern` declarations for `menu_insert_text` and `tab_move` — two functions defined in `calculator_core.c` but not declared in any header. |
| Layer discipline | ✅ PASS | UI Logic layer; LVGL dependency expected. |
| Function complexity | ⚠️ REVIEW | Check `handle_matrix_menu` and `handle_matrix_edit` for line length and depth. |
| Naming convention | ✅ PASS | `Matrix_` prefix on public API; `matrix_edit_*` for internal state. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `prgm_editor.c` / `prgm_editor.h` — 538 lines

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Both have Doxygen blocks. `.c` documents the callback-based decoupling from `ui_prgm.c`. |
| Header hygiene | ✅ PASS | Clean. Dependency graph is documented in the `.c` file header. Acyclic dependency confirmed. |
| Interface width | ✅ PASS | Clean public API: `PrgmEditor_InitScreen`, `PrgmEditor_Open`, `PrgmEditor_Close`, `PrgmEditor_HandleToken`, `PrgmEditor_InsertStr`, `PrgmEditor_MenuInsert`, `PrgmEditor_CursorUpdate`, `PrgmEditor_GetScreen`. Callbacks for cross-module coordination are properly injected. |
| Layer discipline | ✅ PASS | UI Logic layer; expected. |
| Function complexity | ⚠️ REVIEW | Verify `PrgmEditor_HandleToken` dispatches cleanly. |
| Naming convention | ✅ PASS | `PrgmEditor_` prefix consistent and capitalized correctly. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### `app_common.h` — catch-all header risk assessment

| Dimension | Status | Notes |
|-----------|--------|-------|
| File-level doc | ✅ PASS | Doxygen block present. Notes "keeps dependencies minimal." |
| Header hygiene | ⚠️ REVIEW | Only 5 `#define` constants (low risk). However, `app_common.h` defines `GraphState_t` (280+ byte struct) and `StatData_t`/`StatResults_t`. These belong to their respective subsystems. Risk grows as codebase grows. Currently manageable. |
| Interface width | ⚠️ REVIEW | Exports `Execute_Token` and `Process_Hardware_Key` — the top-level entry points — plus FreeRTOS queue and mutex handles. The `extern osMessageQId keypadQueueHandle` and `extern SemaphoreHandle_t xLVGL_Mutex` are necessary shared handles. Risk: low today, but every addition propagates everywhere. |
| Inline debt | ✅ PASS | Zero markers found. |

---

#### Remaining UI Logic modules (brief checklist)

| Module | Lines | Doc | Hygiene | Interface | Layer | Complexity | Naming | Debt | Priority |
|--------|-------|-----|---------|-----------|-------|------------|--------|------|----------|
| `graph_draw.c/.h` | 141 | ✅ | ✅ | ✅ | ✅ | ✅ PASS | ✅ | ✅ | Low |
| `ui_graph_zoom.c/.h` | 311 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_mode.c/.h` | 224 | ✅ | ⚠️ `extern ModeScreenState_t s_mode` exposed to `persist.c`; mutable state through extern | ⚠️ `s_mode` should be accessed through an accessor | ✅ | ✅ | ✅ | ✅ | Medium |
| `ui_input.c/.h` | 273 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW (touches expr state, STO synthesis) | ✅ | ✅ | Medium |
| `ui_math_menu.c/.h` | 477 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_stat.c/.h` | 369 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_stat_edit.c/.h` | 476 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_vars.c/.h` | 422 | ✅ | ⚠️ `extern MenuState_t vars_menu_state` | ⚠️ Same extern-state pattern as ui_matrix | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_yvars.c/.h` | 407 | ✅ | ⚠️ `extern MenuState_t yvars_menu_state` | ⚠️ Same pattern | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_draw.c/.h` | 418 | ✅ | ⚠️ `extern MenuState_t draw_menu_state` | ⚠️ Same pattern | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_sto.c/.h` | 387 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_error.c/.h` | 140 | ✅ | ⚠️ `extern lv_obj_t *ui_error_screen` | ⚠️ Screen pointer exported; should be hidden | ✅ | ✅ | ✅ | ✅ | Low |
| `ui_reset.c/.h` | 173 | ✅ | ⚠️ `extern lv_obj_t *ui_reset_screen` | ⚠️ Same pattern | ✅ | ✅ | ✅ | ✅ | Low |
| `ui_param_yeq.c/.h` | 328 | ✅ | ✅ | ✅ | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_prgm_ctl.c/.h` | 79 | ✅ | ⚠️ `extern lv_obj_t *ui_prgm_ctl_screen` | ⚠️ Same pattern | ✅ | ✅ | ✅ | ✅ | Low |
| `ui_prgm_io.c/.h` | 75 | ✅ | ⚠️ `extern lv_obj_t *ui_prgm_io_screen` | ⚠️ Same pattern | ✅ | ✅ | ✅ | ✅ | Low |
| `ui_prgm_exec.c/.h` | 94 | ✅ | ⚠️ `extern lv_obj_t *ui_prgm_exec_screen` | ⚠️ Same pattern | ✅ | ✅ | ✅ | ✅ | Low |
| `ui_prgm_mode.c/.h` | 128 | ✅ | ⚠️ `extern lv_obj_t *ui_prgm_mode_num_screen`, `ui_prgm_mode_gph_screen` | ⚠️ Same pattern | ✅ | ✅ | ✅ | ✅ | Low |
| `app_init.c/.h` | 379 | ✅ | ⚠️ `extern volatile bool g_sleeping` in `.h` | ⚠️ Volatile state exposed; should have accessor | ✅ | ⚠️ REVIEW | ✅ | ✅ | Low |
| `ui_menu_screen.c/.h` | 135 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | Low |

**Pervasive pattern noted:** Seven UI sub-module headers export mutable `MenuState_t` state directly via `extern`. Four PRGM sub-module headers and two error/reset module headers export LVGL screen object pointers directly via `extern lv_obj_t *`. A `Show/Hide/IsVisible` accessor pattern — already established in newer modules like `prgm_editor.c` and `graph_ui.c` — should replace the raw externs systematically.

---

#### Newly-created headers (structural audit)

| Header | Include guard | Statics | Notes |
|--------|--------------|---------|-------|
| `graph_coord.h` | `#pragma once` (inconsistent with rest of codebase) | `static inline` only (acceptable) | Functional coordinate transforms; zero side effects. |
| `calc_mode_topology.h` | `#pragma once` (inconsistent) | `static inline` only (acceptable) | The `from` parameter is `(void)from` — from→to pair validation promised in the function doc is not yet implemented. |

---

## C. Priority Ranking

| Rank | Module | Lines | Confirmed Issues | Rationale |
|------|--------|-------|-----------------|-----------|
| 1 | `persist.c` / `persist.h` | 341 | **CRITICAL** FLASH sector collision with prgm_exec | Data-loss bug: `Persist_Save()` erases all program storage. Must fix before next hardware test session. |
| 2 | `prgm_exec.c` / `prgm_exec.h` | 907 | **CRITICAL** same sector; naming convention; layer violation | Shares the collision defect. Also has upward App Core → UI Logic dependency and stale FLASH map comment. |
| 3 | `calculator_core.c` / `.h` | 1713 | Function complexity (4 responsibilities), `CalcHistory_UpdateDisplay` ownership split | Largest coordination hub. Four-responsibility problem makes every change risky. ARCH_OPPS item 4 directly addresses this. |
| 4 | `calc_engine.c` / `calc_engine.h` | 1791 | `EvaluateRPN_ex` complexity (132 lines, if-chain dispatch, depth 5) | Largest file. Token dispatch partially extracted but top-level function still needs sub-dispatch extraction. |
| 5 | `graph_ui.c` / `.h` | 1403 | `handle_trace_mode` complexity (152 lines, depth 6) | Y= editor and 6 canvas-mode handlers in one file with no modular boundary. |
| 6 | `ui_matrix.c` / `.h` | 579 | Missing file doc, 9 non-`static` globals, 2 raw `extern` function declarations in `.c` | Despite mid-size, has worst encapsulation posture: un-`static` globals and raw `extern` function declarations in the `.c` file. |
| 7 | `ui_prgm.c` / `ui_prgm.h` | 973 | Header pulls LVGL into prgm_exec, naming inconsistency, transitional wrapper debt | After prgm_editor extraction the responsibility split is good, but header still causes LVGL pollution into prgm_exec. |
| 8 | `calc_history.c` / `.h` | 135 | `CalcHistory_UpdateDisplay` ownership split | Small file but the cross-module function ownership will worsen if ARCH_OPPS item 4 is not implemented. |
| 9 | `ui_mode.h` (+ `persist.c`) | 224 | `extern ModeScreenState_t s_mode` exported for persist; persist.c → calculator_core.h coupling | Soft violations, low risk of defect, but should be cleaned up before module list grows. |
| 10 | Pervasive `extern lv_obj_t *` pattern | — | 6 sub-module headers expose screen pointers | Systematic but low-risk. Replace with Show/Hide/IsVisible accessors. |

---

## D. Findings Summary

All confirmed defects, with file:line references. Suitable as seed items for CLAUDE.md `[refactor]` and `[docs]` backlog (already added to CLAUDE.md "Next session priorities" as of 2026-05-07).

### Critical (data-loss risk)

**FLASH sector collision between persist and prgm_exec**
- `persist.h:45,47` — `PERSIST_FLASH_ADDR = 0x080E0000`, `PERSIST_SECTOR = FLASH_SECTOR_11`
- `prgm_exec.h:47,49` — `PRGM_FLASH_ADDR = 0x080E0000`, `PRGM_SECTOR = FLASH_SECTOR_11`
- `persist.c:71` — erase uses `PERSIST_SECTOR` (= sector 11, full 128 KB erase)
- `prgm_exec.c:72` — erase uses `PRGM_SECTOR` (= sector 11, full 128 KB erase)
- Both modules erase the full 128 KB sector before writing. Introduced in commit `cf931ab` ("persist→SECTOR_11"). Combined data size ≈ 21,780 bytes — fits within 128 KB. Fix: sub-sector-offset layout or consecutive whole sectors.

**Stale FLASH map comment in prgm_exec.h**
- `prgm_exec.h:15–17` — comment says "Sector 10: 0x080C0000 — calculator variables / graph / matrices" but sector 10 is now occupied by firmware. Predates commit `cf931ab`.
- `STM32F429XX_FLASH.ld:61` — linker comment says "Sector 11 reserved for the persist block" but does not mention program storage.

### High (architecture violations)

**`prgm_exec.c` upward dependency on UI Logic layer**
- `prgm_exec.c:19–21` (non-HOST_TEST includes): `calculator_core.h`, `ui_prgm.h`, `graph.h`
- `prgm_exec.c:474` — calls `format_calc_result()` (defined in `calculator_core.c`, declared in `calculator_core.h`)
- `prgm_exec.c:503` — calls `ExprBuffer_Clear(Calc_GetExpr())` where `Calc_GetExpr()` is in `calculator_core.h`
- Fix: move `format_calc_result` to `calc_engine.c`/`calc_engine.h` and expose `Calc_ClearExpr()` as an App Core API.

**`CalcHistory_UpdateDisplay` implementation split**
- `calc_history.h:74` — declares `CalcHistory_UpdateDisplay()`
- `calculator_core.c:834` — implements it (needs LVGL objects owned there)
- `calc_history.c:8` — explicitly notes "intentionally NOT defined here"
- Root cause: display objects needed by the function are private to `calculator_core.c`. ARCH_OPPS item 4 (ExprEditor extraction) would enable `CalcHistory_UpdateDisplay` to move back to `calc_history.c` via a registered callback.

**`calc_engine.c:EvaluateRPN_ex` complexity**
- Lines 1268–1400 (132 lines), 7 top-level `if (tt == ...)` guards before sub-dispatch. Nesting reaches depth 5. Sub-dispatch to a table-driven handler indexed by `MathTokenType_t` would reduce the top-level function to a loop + dispatch call.

**`graph_ui.c:handle_trace_mode` complexity**
- Lines 1100–1252 (152 lines, depth 6). Six separable sub-concerns. Extract: `trace_step_left`, `trace_step_right`, `trace_step_size`, `trace_switch_equation`, `trace_to_graph`, `trace_to_yeq`.

### Medium (encapsulation and interface violations)

**`ui_matrix.c` encapsulation failure**
- Lines 13–24: 9 non-`static` global variables with no matching `extern` declarations in `ui_matrix.h` — exported by linkage, accessible without a declared contract.
- Lines 49–51: raw `extern void menu_insert_text(...)` and `extern void tab_move(...)` declarations for functions defined in `calculator_core.c`.
- Line 1: no file-level Doxygen block.

**Pervasive `extern lv_obj_t *` screen pointer pattern**
- `ui_prgm_ctl.h:22`, `ui_prgm_mode.h:19–20`, `ui_prgm_exec.h:24`, `ui_prgm_io.h:22`, `ui_reset.h:19`, `ui_error.h:25`
- Screen pointers exported as mutable `extern lv_obj_t *`. Replace with `Module_Show()`, `Module_Hide()`, `Module_IsVisible()` accessors following the pattern in `graph_ui.c`.

**Pervasive `extern MenuState_t` pattern**
- `ui_matrix.h:12`, `ui_draw.h:24`, `ui_mode.h:39`, `ui_vars.h:31`, `ui_yvars.h:27`, `ui_stat.h:20`
- Mutable `MenuState_t` state exposed by raw `extern`. Callers should use `Module_GetCursor()` / `Module_MoveCursor()` accessors.

**`calculator_core.c` `@brief` does not reflect module scope**
- `calculator_core.c:2–8` — `@brief` says "Calculator logic, UI management, and FreeRTOS task implementation." Should state the coordinator role and enumerate planned extraction targets.

**Missing file-level documentation**
- `ui_matrix.c:1–3` — no `@file`/`@brief` block.
- `calc_mode_topology.h:1` — no `@file`/`@brief` block.

### Low (style and consistency)

**Naming convention inconsistency in prgm_exec.h**
- Public functions `prgm_run_start`, `prgm_run_loop`, `prgm_lookup_slot`, `prgm_request_abort`, `prgm_is_waiting_input`, `prgm_get_input_var`, `prgm_clear_input_wait` use lowercase `snake_case` rather than `Module_VerbNoun`. Should be renamed `Prgm_RunStart`, `Prgm_RunLoop`, etc.

**Include guard style inconsistency**
- `graph_coord.h` and `calc_mode_topology.h` use `#pragma once`; all other headers use `#ifndef MODULE_H` / `#endif`. Choose one convention and apply consistently.

**`calc_mode_topology.h` — `from` parameter is unused**
- `calc_mode_topology.h:14` — `CalcMode_IsValidTransition(CalcMode_t from, CalcMode_t to)` immediately casts `from` to `(void)from`. Either implement the from-to pair table or remove the parameter.

---

## Critical Files for Resolving Top Findings

| File | Issue |
|------|-------|
| `App/Inc/persist.h` | FLASH sector collision (critical) |
| `App/Inc/prgm_exec.h` | FLASH sector collision + stale comment + naming convention |
| `App/Src/persist.c` | Erase routine needs sub-sector awareness |
| `App/Src/prgm_exec.c` | Erase routine + upward UI Logic dependency |
| `App/Src/ui_matrix.c` | Missing doc, 9 un-`static` globals, raw `extern` function decls |
| `App/Inc/ui_matrix.h` | Add proper `extern` declarations or add accessors |
| `App/Src/calculator_core.c` | Update `@brief`, plan extraction of 4 responsibilities |

---

## E. Actionable Work Items

Each item is self-contained and can be started independently (prerequisites noted where they exist). Items are ordered by priority: critical first, then high architecture violations, then medium encapsulation issues, then low style items. The verification step for every item is: `cd build-tests && cmake --build . && ctest --output-on-failure` (the host test suite). Build the embedded target after any change to a non-`HOST_TEST`-guarded path.

**Progress check: 2026-05-14.** Status legend: ✅ Done · ⚠️ Partial (interim steps done, full fix deferred) · ❌ Open.

---

### E1. ✅ [CRITICAL] Fix FLASH sector collision between persist and prgm_exec

**Why it matters:** `Persist_Save()` and `Prgm_Save()` both call `HAL_FLASHEx_Erase` on `FLASH_SECTOR_11`. Either call silently destroys the other module's data. This is a data-loss bug that will manifest on the first hardware session that exercises both saves.

**Chosen fix:** Move program storage to `FLASH_SECTOR_12` (Bank 2, 0x08100000, 128 KB). Persist stays at sector 11. STM32F429ZIT6 has 2 MB; Bank 2 sectors 12–23 are unused. This avoids any sub-region erase complexity (STM32F4 can only erase full sectors) and leaves both modules simple.

**Files to change:**
- `App/Inc/prgm_exec.h`
- `App/Src/prgm_exec.c` (no logic change — just picks up the new constants)
- `STM32F429XX_FLASH.ld` (update linker comment)

**Steps:**

1. In `App/Inc/prgm_exec.h`, lines 15–17 and 47–49, make these changes:
   - Replace the stale comment block (lines 15–17) with the current sector layout:
     ```c
     *   Sector 10: 0x080C0000 — occupied by firmware (as of 2026-04-28, ~820 KB)
     *   Sector 11: 0x080E0000 — persist block (persist.h / persist.c)
     *   Sector 12: 0x08100000 — program storage (this module, Bank 2)
     ```
   - Change `PRGM_FLASH_ADDR` from `0x080E0000U` to `0x08100000U`.
   - Change `PRGM_SECTOR` from `FLASH_SECTOR_11` to `FLASH_SECTOR_12`.
   - Update the inline comment on `PRGM_FLASH_ADDR` from `"Sector 11, 128 KB"` to `"Sector 12 (Bank 2), 128 KB"`.

2. In `STM32F429XX_FLASH.ld`, line 61, update the comment from:
   ```
   * Sector 11 (0x080E0000-0x080FFFFF) is reserved for the persist block.
   ```
   to:
   ```
   * Sector 11 (0x080E0000-0x080FFFFF) is reserved for the persist block.
   * Sector 12 (0x08100000-0x0811FFFF) is reserved for program storage.
   ```

3. `prgm_exec.c` requires no logic changes — `persist_erase_sector()` and `prgm_erase_sector()` already read the `#define` constants.

**Verify:**
- Build embedded target: `cmake --build build/Debug` (confirms `FLASH_SECTOR_12` is a valid HAL constant).
- Run host test suite: `cd build-tests && cmake --build . && ctest --output-on-failure` (confirms no regression in `test_prgm_exec` and `test_persist_roundtrip`).
- On hardware: flash, `Persist_Save()` then `Prgm_Save()` then power-cycle — confirm both survive. This is the only full verification.

**Complexity delta:** Neutral (constant rename only).

---

### E2. ✅ [CRITICAL + LOW] Fix stale FLASH map comment in prgm_exec.h

**Note:** This item is resolved as a by-product of E1 (step 1 above rewrites the comment block). If E1 is done, skip E2. If for some reason E1 is deferred, apply just the comment fix here as a standalone change.

**Files to change:** `App/Inc/prgm_exec.h` lines 15–17.

**Steps:** Replace the Sector 10 comment with the current layout as described in E1 step 1 above.

**Complexity delta:** Neutral.

---

### E3. ❌ [HIGH] Remove prgm_exec.c's upward dependency on the UI Logic layer

**Why it matters:** `prgm_exec.c` is classified as Application Core (host-testable) but in non-`HOST_TEST` builds it includes `calculator_core.h` (UI Logic) and calls `format_calc_result()` (defined in `calculator_core.c`) and `Calc_GetExpr()` + `ExprBuffer_Clear()` (UI-layer state). This creates an upward dependency that prevents `prgm_exec.c` from being tested or reused without dragging in the entire UI layer.

**Specific violations (confirmed by grep):**
- `prgm_exec.c:19` — `#include "calculator_core.h"` (non-HOST_TEST)
- `prgm_exec.c:474` — `format_calc_result(&r, disp_buf, MAX_RESULT_LEN)` (defined in `calculator_core.c:453`)
- `prgm_exec.c:503` and `prgm_exec.c:859` — `ExprBuffer_Clear(Calc_GetExpr())` (both `Calc_GetExpr()` and the `ExprBuffer_Clear` call reach into UI-layer state)

**Files to change:**
- `App/Src/calc_engine.c` — add `format_calc_result` implementation
- `App/Inc/calc_engine.h` — add `format_calc_result` declaration
- `App/Src/calculator_core.c` — remove the now-duplicated definition (keep the local call sites using the new `calc_engine.h` version)
- `App/Inc/calculator_core.h` — remove the `format_calc_result` declaration (line 91)
- `App/Inc/calc_engine.h` — add `Calc_ClearExpr()` declaration
- `App/Src/calculator_core.c` — add `Calc_ClearExpr()` definition (one-liner calling `ExprBuffer_Clear(Calc_GetExpr())`)
- `App/Src/prgm_exec.c` — remove `#include "calculator_core.h"` from the non-HOST_TEST block; replace `format_calc_result` call with the `calc_engine.h` version; replace `ExprBuffer_Clear(Calc_GetExpr())` with `Calc_ClearExpr()`
- `App/Inc/ui_prgm.h` — still included by prgm_exec.c (line 20) for `Prgm_GetLine` / `Prgm_GetNumLines` / `prgm_parse_from_store`; see E10 for the follow-on split

**Steps:**

1. In `App/Src/calc_engine.c`, append (near the bottom, before `#endif /* HOST_TEST */` if applicable):
   ```c
   void format_calc_result(const CalcResult_t *r, char *buf, int buf_size) { ... }
   ```
   Copy the implementation verbatim from `calculator_core.c:453`. It uses only `<stdio.h>`, `<string.h>`, and `CalcResult_t` — all available in `calc_engine.h`'s scope.

2. In `App/Inc/calc_engine.h`, add the declaration (after the existing Calc_* declarations):
   ```c
   void format_calc_result(const CalcResult_t *r, char *buf, int buf_size);
   ```

3. In `App/Inc/calc_engine.h`, add:
   ```c
   void Calc_ClearExpr(void);
   ```

4. In `App/Src/calculator_core.c`, add the implementation:
   ```c
   void Calc_ClearExpr(void) { ExprBuffer_Clear(Calc_GetExpr()); }
   ```

5. In `App/Src/calculator_core.c:453`, delete the `format_calc_result` definition (it now lives in `calc_engine.c`). The three call sites in `calculator_core.c` already include `calc_engine.h` transitively, so they will resolve correctly.

6. In `App/Inc/calculator_core.h:91`, remove the `format_calc_result` declaration.

7. In `App/Src/prgm_exec.c`, inside the `#ifndef HOST_TEST` include block (lines 19–21):
   - Remove `#include "calculator_core.h"`.
   - Add `#include "calc_engine.h"` if not already included unconditionally.
   - Replace `format_calc_result(&r, disp_buf, MAX_RESULT_LEN)` with the same call (now resolves from `calc_engine.h`).
   - Replace `ExprBuffer_Clear(Calc_GetExpr())` (both occurrences, lines ~503 and ~859) with `Calc_ClearExpr()`.

8. Confirm `graph.h` (line 21 of prgm_exec.c) is still needed for `Graph_Set*` calls guarded by `#ifndef HOST_TEST`. If so, retain that include. The goal is only to remove `calculator_core.h`.

**Verify:** Host test suite passes. Embedded build passes. Search for remaining uses of `calculator_core.h` in `prgm_exec.c` to confirm it is gone.

**Complexity delta:** Neutral (pure extraction, no logic change).

---

### E4. ❌ [HIGH] Remove persist.c's upward dependency on the UI Logic layer

**Why it matters:** `persist.c:16` includes `calculator_core.h` (UI Logic) in non-HOST_TEST builds to access `Calc_GetAns()`, `Calc_SetAnsScalar()`, and `Calc_SetAngleDegrees()`. These are App Core state accessors that belong in `calc_engine.h`.

**Prerequisite:** E3 (adds `Calc_ClearExpr` as a model for this pattern).

**Files to change:**
- `App/Inc/calc_engine.h` — add `Calc_GetAns`, `Calc_GetAnsIsMatrix`, `Calc_SetAnsScalar`, `Calc_SetAnsMatrix`, `Calc_GetAngleDegrees`, `Calc_SetAngleDegrees` declarations (check whether any already exist)
- `App/Src/calculator_core.c` — these functions likely already exist; confirm they are declared somewhere reachable without `calculator_core.h`
- `App/Src/persist.c` — remove `#include "calculator_core.h"`; add `#include "calc_engine.h"` if needed

**Steps:**

1. `grep -rn "Calc_GetAns\|Calc_SetAnsScalar\|Calc_SetAngleDegrees" App/Inc/calc_engine.h` — check which of these are already declared in `calc_engine.h`. Add any that are missing.

2. Confirm the implementations exist in a file that is compiled in all configurations (not just non-HOST_TEST). They likely live in `calculator_core.c` or `calc_engine.c`.

3. In `persist.c:16`, change `#include "calculator_core.h"` to `#include "calc_engine.h"`. Build — fix any undeclared symbol errors by adding declarations to `calc_engine.h`.

**Verify:** Host test suite passes (`test_persist_roundtrip`). Embedded build passes.

**Complexity delta:** Neutral.

---

### E5. ❌ [HIGH] Verify and resolve function complexity in persist.c (Persist_BuildBlock / Persist_ApplyBlock)

**Why it matters:** Both `Persist_BuildBlock` (lines 167–235, ~68 lines) and `Persist_ApplyBlock` (lines 236–305, ~69 lines) exceed the 60-line limit. The audit marked these ⚠️ REVIEW.

**Files to change:** `App/Src/persist.c`

**Steps:**

1. Read `Persist_BuildBlock` and map its logical sections. Typical candidates: graph state serialisation, stat state serialisation, Y= state serialisation, mode state serialisation. Each is a natural `static` helper:
   ```c
   static void build_graph_state(PersistBlock_t *out);
   static void build_stat_state(PersistBlock_t *out);
   static void build_mode_state(PersistBlock_t *out);
   ```

2. Repeat for `Persist_ApplyBlock` with `apply_graph_state`, `apply_stat_state`, `apply_mode_state`.

3. Extract each helper, moving the relevant lines out of the top-level function. The top-level function becomes a short orchestrator calling the helpers in order.

4. Verify each helper is ≤ 60 lines and nesting depth ≤ 4.

**Verify:** `test_persist_roundtrip` passes (property-based, catches serialise/deserialise mismatches).

**Complexity delta:** Decrease.

---

### E6. ❌ [HIGH] Extract sub-dispatch helpers from EvaluateRPN_ex in calc_engine.c

**Context from audit:** `EvaluateRPN_ex` (lines 1268–1400, 132 lines) has 7 top-level `if (tt == ...)` chains at nesting depth 4–5 dispatching 53 `MATH_*` token types. Sub-dispatchers `eval_unary_func`, `eval_binary_op`, `eval_mat_arith`, `eval_matrix_func` are partially extracted but the top-level function still exceeds limits.

**Files to change:** `App/Src/calc_engine.c`

**Steps:**

1. Read `EvaluateRPN_ex` in full and list every top-level `if (tt == ...)` group. Identify groups that share a theme (e.g., all trig functions, all matrix operations, all comparison operators).

2. Create a static dispatcher per theme:
   ```c
   static CalcError_t rpn_eval_trig(MathTokenType_t tt, double *stack, int *sp);
   static CalcError_t rpn_eval_matrix(MathTokenType_t tt, double *stack, int *sp, ...);
   static CalcError_t rpn_eval_comparison(MathTokenType_t tt, double *stack, int *sp);
   ```
   The existing `eval_unary_func`, `eval_binary_op`, etc. already establish this pattern — extend it rather than invent a new one.

3. The goal: `EvaluateRPN_ex` becomes a loop + a switch/if-chain that calls the themed dispatchers. Each dispatcher handles ≤ 20 token types. Top-level function drops below 60 lines.

4. Zero behaviour change — the `test_calc_engine` and `test_parse_eval` host tests are the regression harness.

**Verify:** `test_calc_engine` + `test_parse_eval` pass. Run `test_expr_util` and `test_error_codes` as a sanity check.

**Complexity delta:** Decrease.

---

### E7. ❌ [HIGH] Extract cursor-mode state machines from graph_ui.c

**Context from audit:** `handle_trace_mode` (lines 1100–1252, 152 lines, depth 6) handles six separable sub-concerns. The Y= editor and six canvas-mode handlers share one 1403-line file with no modular boundary.

**Files to change / create:**
- `App/Src/graph_ui.c` — remove extracted functions
- `App/Src/graph_ui_cursor.c` — new file (canvas-mode handlers: trace, free-cursor)
- `App/Inc/graph_ui_cursor.h` — new header (entry points called from `graph_ui.c`)

**Steps:**

1. Read `handle_trace_mode` and identify the six sub-concerns (per audit: LEFT/RIGHT step with wrapping, parametric vs. function step size, equation switching, re-render, TRACE→GRAPH transition, TRACE→YEQ transition).

2. Extract each as a named `static` helper in the new `graph_ui_cursor.c`:
   ```c
   static void trace_step_left(void);
   static void trace_step_right(void);
   static void trace_select_step_size(void);
   static void trace_switch_equation(int dir);
   static void trace_transition_to_graph(CalcMode_t *ret_mode);
   static void trace_transition_to_yeq(CalcMode_t *ret_mode);
   ```
   `handle_trace_mode` itself becomes the public entry point (≤ 30 lines) that delegates to these helpers.

3. Similarly examine `handle_free_cursor_mode` — extract any sub-concerns that exceed 20 lines.

4. Move both `handle_trace_mode` and `handle_free_cursor_mode` to `graph_ui_cursor.c`. Declare them in `graph_ui_cursor.h`. Include `graph_ui_cursor.h` from `graph_ui.c` and remove the now-moved function bodies.

5. Add the new source file to `App/CMakeLists.txt` (the App target sources list) and to `App/Tests/CMakeLists.txt` if a graph render test links `graph_ui.c`.

6. Add a `/** @file graph_ui_cursor.c @brief Canvas-mode state machines … */` Doxygen block at the top of the new file.

**Verify:** `test_graph_render` passes. Embedded build passes (check for undefined symbol errors from the new split).

**Complexity delta:** Decrease.

---

### E8. ⚠️ [MEDIUM] Fix CalcHistory_UpdateDisplay cross-module ownership

**Context from audit:** `CalcHistory_UpdateDisplay()` is declared in `calc_history.h:74` but implemented in `calculator_core.c:834`. The function needs LVGL objects private to `calculator_core.c`, so it cannot trivially move. The long-term fix is a registered callback (ARCH_OPPS item 4, ExprEditor extraction).

**Prerequisite:** This is partially blocked by the ARCH_OPPS item 4 (ExprEditor extraction from `calculator_core.c`). As an interim measure, document the situation more clearly.

**Interim steps (low effort, do now):**

1. In `App/Inc/calc_history.h`, revise the comment on line 7 from the current vague text to:
   ```c
   * CalcHistory_UpdateDisplay() is declared here but IMPLEMENTED in calculator_core.c
   * because it needs the LVGL label objects that are private to that file.
   * Long-term fix: register a display-update callback from calculator_core.c
   * during init (see ARCHITECTURE_OPPORTUNITIES.md item 4 — ExprEditor extraction).
   * Do not move this declaration to calculator_core.h — it would create a circular
   * include since calc_history.h is already included by calculator_core.c.
   ```

2. In `App/Src/calculator_core.c:834`, add a comment above the function:
   ```c
   /* Declared in calc_history.h — implemented here because lv_label objects are
      private to this file. See ARCHITECTURE_OPPORTUNITIES.md item 4. */
   ```

**Full fix (deferred — do after ARCH_OPPS item 4):** Once ExprEditor is extracted and display objects live in a dedicated module, `CalcHistory_UpdateDisplay` can be moved back to `calc_history.c` with access granted via a registered `void (*on_history_change)(void)` callback injected during init.

**Complexity delta:** Neutral (interim); Decrease (full fix).

---

### E9. ❌ [MEDIUM] Fix ui_matrix.c encapsulation failures

**Context from audit:** 9 non-`static` globals at lines 13–24 (exported by linkage with no matching `extern` in `ui_matrix.h`); 2 raw `extern` function declarations at lines 49–51 for `menu_insert_text` and `tab_move` defined in `calculator_core.c`; missing file-level Doxygen block.

**Confirmed current state (grep result):** All 9 variables at lines 26–39 are already `static`. The raw `extern` declarations for `menu_insert_text` and `tab_move` are at lines 49–50. No file-level Doxygen block.

**Files to change:** `App/Src/ui_matrix.c`, `App/Inc/calculator_core.h`

**Steps:**

1. Verify the globals: `grep -n "^lv_obj_t\|^MenuState_t\|^int\|^uint\|^bool\|^char" App/Src/ui_matrix.c | head -20`. If any lack `static`, add it. Based on the current grep they appear to already be `static` — confirm and mark this sub-step done.

2. For the raw `extern` declarations at lines 49–51:
   - Check whether `menu_insert_text` and `tab_move` are already declared in `calculator_core.h`.
   - If yes: replace the raw `extern` declarations in `ui_matrix.c` with `#include "calculator_core.h"`.
   - If no: add declarations to `calculator_core.h` under a clear section comment (`/* Internal UI helpers exposed to lateral sub-modules */`) and include `calculator_core.h` from `ui_matrix.c`.

3. Add a file-level Doxygen block at line 1 of `ui_matrix.c`:
   ```c
   /**
    * @file  ui_matrix.c
    * @brief Matrix editor UI — slot browser (MATRX tab) and cell editor (EDIT tab).
    *
    * Two sub-state machines: matrix_menu (browse/select) and matrix_edit (dimension
    * change, cell navigation, and value entry). Both run inside handle_matrix_menu()
    * and handle_matrix_edit() respectively.
    */
   ```

**Verify:** Embedded build passes (no new undefined symbols). Host test suite unaffected (ui_matrix.c is embedded-only).

**Complexity delta:** Neutral.

---

### E10. ❌ [MEDIUM] Eliminate LVGL transitive pollution from ui_prgm.h into prgm_exec.c

**Context from audit:** `ui_prgm.h:18` includes `lvgl.h`. `prgm_exec.c:20` includes `ui_prgm.h` in non-`HOST_TEST` builds. This gives `prgm_exec.c` a transitive LVGL dependency and prevents the declarations `prgm_exec.c` actually needs (`Prgm_GetLine`, `Prgm_GetNumLines`, `prgm_parse_from_store`) from being accessed without LVGL.

**Files to change / create:**
- `App/Inc/prgm_store_access.h` — new narrow header (LVGL-free)
- `App/Inc/ui_prgm.h` — include `prgm_store_access.h` instead of redeclaring
- `App/Src/prgm_exec.c` — replace `#include "ui_prgm.h"` with `#include "prgm_store_access.h"`

**Steps:**

1. Create `App/Inc/prgm_store_access.h`:
   ```c
   #ifndef PRGM_STORE_ACCESS_H
   #define PRGM_STORE_ACCESS_H
   /**
    * @file  prgm_store_access.h
    * @brief LVGL-free declarations for reading the program store.
    *        Safe to include from Application Core modules (no LVGL dependency).
    */
   #include <stdint.h>
   #include "prgm_exec.h"  /* ProgramStore_t */

   const char *Prgm_GetLine(uint8_t slot, uint16_t line);
   uint16_t    Prgm_GetNumLines(uint8_t slot);
   void        prgm_parse_from_store(uint8_t slot);
   #endif /* PRGM_STORE_ACCESS_H */
   ```

2. In `App/Inc/ui_prgm.h`, remove the duplicate declarations of `Prgm_GetLine`, `Prgm_GetNumLines`, `prgm_parse_from_store` and add `#include "prgm_store_access.h"` so existing includers of `ui_prgm.h` continue to resolve them.

3. In `App/Src/prgm_exec.c`, inside the `#ifndef HOST_TEST` block (line 20), replace `#include "ui_prgm.h"` with `#include "prgm_store_access.h"`.

4. Build embedded target. If any undefined symbols appear, move the missing declarations to `prgm_store_access.h`.

**Note on `prgm_flatten_to_store` migration (also flagged):** `ui_prgm.h:40` exports `prgm_flatten_to_store()` with a comment saying it is a thin wrapper kept for unmigrated sub-menu files. Once all callers have been moved to `prgm_editor.h`, remove the declaration from `ui_prgm.h`. `grep -rn "prgm_flatten_to_store" App/` to find remaining callers.

**Verify:** Embedded build passes. Host test suite passes. `grep -n "lvgl" App/Src/prgm_exec.c` should return zero results outside `#ifndef HOST_TEST` guards.

**Complexity delta:** Neutral.

---

### E11. ❌ [MEDIUM] Replace pervasive `extern lv_obj_t *` screen pointer pattern with Show/Hide accessors

**Affected files (confirmed by grep):**
- `App/Inc/ui_prgm_ctl.h:22` — `extern lv_obj_t *ui_prgm_ctl_screen`
- `App/Inc/ui_prgm_mode.h:19–20` — `extern lv_obj_t *ui_prgm_mode_num_screen`, `*ui_prgm_mode_gph_screen`
- `App/Inc/ui_prgm_exec.h:24` — `extern lv_obj_t *ui_prgm_exec_screen`
- `App/Inc/ui_prgm_io.h:22` — `extern lv_obj_t *ui_prgm_io_screen`
- `App/Inc/ui_reset.h:19` — `extern lv_obj_t *ui_reset_screen`
- `App/Inc/ui_error.h:25` — `extern lv_obj_t *ui_error_screen`

**Reference implementation:** `graph_ui.h` uses `Graph_ShowYeqScreen()` / `Graph_HideYeqScreen()`. Follow the same pattern.

**Steps (apply the same procedure to each module):**

For each module (e.g., `ui_error`):

1. In `App/Inc/ui_error.h`, remove `extern lv_obj_t *ui_error_screen;` and add:
   ```c
   void UiError_Show(void);
   void UiError_Hide(void);
   bool UiError_IsVisible(void);
   ```

2. In `App/Src/ui_error.c`, change `lv_obj_t *ui_error_screen;` to `static lv_obj_t *ui_error_screen;`.

3. Add the three accessor implementations:
   ```c
   void UiError_Show(void)    { lv_scr_load(ui_error_screen); }
   void UiError_Hide(void)    { /* hide logic — see how graph_ui.c hides YEQ */ }
   bool UiError_IsVisible(void) { return lv_scr_act() == ui_error_screen; }
   ```

4. `grep -rn "ui_error_screen" App/` — for each call site that directly sets or reads the pointer, replace with the accessor call.

5. Repeat for `ui_reset`, `ui_prgm_ctl`, `ui_prgm_exec`, `ui_prgm_io`, `ui_prgm_mode` (two screens — add `UiPrgmMode_ShowNum()`, `UiPrgmMode_ShowGph()`).

**Batch this work:** do all six modules in one commit to avoid a partial-state build break. The naming convention for new functions follows `Module_VerbNoun` with the module name derived from the file: `UiError_`, `UiReset_`, `UiPrgmCtl_`, `UiPrgmExec_`, `UiPrgmIo_`, `UiPrgmMode_`.

**Verify:** Embedded build passes. `grep -rn "extern lv_obj_t" App/Inc/` returns zero results when done.

**Complexity delta:** Neutral.

---

### E12. ❌ [MEDIUM] Replace pervasive `extern MenuState_t` pattern with cursor accessors

**Affected files (confirmed by grep):**
- `App/Inc/ui_matrix.h:12` — `extern MenuState_t matrix_menu_state`
- `App/Inc/ui_draw.h:24` — `extern MenuState_t draw_menu_state`
- `App/Inc/ui_vars.h:31` — `extern MenuState_t vars_menu_state`
- `App/Inc/ui_yvars.h:27` — `extern MenuState_t yvars_menu_state`
- `App/Inc/ui_stat.h:20` — `extern MenuState_t stat_menu_state`
- `App/Inc/ui_mode.h:39` — `extern MenuState_t s_mode` (also used by `persist.c`)

**Note on `ui_mode.h` / `persist.c` coupling:** `persist.c` accesses `s_mode` from `ui_mode.h` to read the mode committed state. E4 (removing `calculator_core.h` from `persist.c`) may expose this more directly. A `UiMode_GetCommittedMode(uint8_t index)` accessor would break this coupling.

**Steps (same procedure for each module):**

For each module (e.g., `ui_matrix`):

1. `grep -rn "matrix_menu_state" App/` — list every access site and note whether each is a read (cursor position) or write (state mutation).

2. For read-only callers: add `uint8_t Matrix_GetCursor(void)` to `ui_matrix.h` and implement it as `return matrix_menu_state.cursor;` in `ui_matrix.c`. Change `matrix_menu_state.cursor` accesses in callers to `Matrix_GetCursor()`.

3. For write callers (typically `calculator_core.c` resetting state on mode change): add `void Matrix_ResetState(void)` and implement it.

4. Once all callers go through accessors, remove the `extern MenuState_t matrix_menu_state;` from `ui_matrix.h` and make the variable `static` in `ui_matrix.c`.

5. Repeat for each module. `ui_mode.h`'s `s_mode` is the highest priority because `persist.c` reads it.

**Verify:** Embedded build passes. `grep -rn "extern MenuState_t" App/Inc/` returns zero results when done.

**Complexity delta:** Neutral.

---

### E13. ❌ [LOW] Rename prgm_exec.h public API to Module_VerbNoun convention

**Context from audit:** Eight public functions use `snake_case` (`prgm_run_start`, etc.) instead of the `Module_VerbNoun` convention used everywhere else.

**Rename map:**
| Old name | New name |
|----------|----------|
| `prgm_run_start` | `Prgm_RunStart` |
| `prgm_run_loop` | `Prgm_RunLoop` |
| `prgm_lookup_slot` | `Prgm_LookupSlot` |
| `prgm_request_abort` | `Prgm_RequestAbort` |
| `prgm_is_waiting_input` | `Prgm_IsWaitingInput` |
| `prgm_get_input_var` | `Prgm_GetInputVar` |
| `prgm_clear_input_wait` | `Prgm_ClearInputWait` |
| `prgm_cmd_table_validate` | `Prgm_CmdTableValidate` |

**Files to change:**
- `App/Inc/prgm_exec.h` — rename declarations
- `App/Src/prgm_exec.c` — rename definitions
- `App/Src/calculator_core.c` — update all call sites
- `App/Tests/test_prgm_cmd_table.c` — update `prgm_cmd_table_validate` → `Prgm_CmdTableValidate`
- `App/Tests/test_prgm_exec.c` — update any direct calls

**Steps:**

1. `grep -rn "prgm_run_start\|prgm_run_loop\|prgm_lookup_slot\|prgm_request_abort\|prgm_is_waiting_input\|prgm_get_input_var\|prgm_clear_input_wait\|prgm_cmd_table_validate" App/` — get the full call-site list.

2. Do a search-and-replace across all listed files. Use `sed -i` or editor bulk rename — these are exact string matches with no ambiguity.

3. Rebuild and fix any missed sites.

**Verify:** Host test suite passes (`test_prgm_exec`, `test_prgm_cmd_table`). Embedded build passes.

**Complexity delta:** Neutral.

---

### E14. ✅ [LOW] Standardise include guards: replace `#pragma once` with `#ifndef` guards

**Affected files:**
- `App/Inc/graph_coord.h:8` — uses `#pragma once`
- `App/Inc/calc_mode_topology.h:1` — uses `#pragma once`

**All other headers use `#ifndef MODULE_H` / `#define MODULE_H` / `#endif`.**

**Steps:**

1. In `App/Inc/graph_coord.h`, replace `#pragma once` (line 8) with:
   ```c
   #ifndef GRAPH_COORD_H
   #define GRAPH_COORD_H
   ```
   And add `#endif /* GRAPH_COORD_H */` at the end of the file.

2. In `App/Inc/calc_mode_topology.h`, replace `#pragma once` (line 1) with:
   ```c
   #ifndef CALC_MODE_TOPOLOGY_H
   #define CALC_MODE_TOPOLOGY_H
   ```
   And add `#endif /* CALC_MODE_TOPOLOGY_H */` at the end of the file.

**Verify:** Host test suite passes (`test_mode_topology`, `test_graph_render`). Embedded build passes.

**Complexity delta:** Neutral.

---

### E15. ✅ [LOW] Resolve calc_mode_topology.h's unused `from` parameter

**Context from audit:** `CalcMode_IsValidTransition(CalcMode_t from, CalcMode_t to)` immediately does `(void)from`. The doc comment says from→to pair validation is reserved for future use, but the parameter misleads callers into thinking it is validated.

**Decision required (pick one before implementing):**

- **Option A — Remove the parameter:** Change signature to `CalcMode_IsValidTransition(CalcMode_t to)`. Simpler, honest. If from→to validation is added later, the signature will need to change again. Update all call sites.
- **Option B — Implement the from→to table:** Define a small allowable-transitions table. The `CalcMode_t` topology is already documented in `calc_mode_topology.h`. This makes the guard actually useful. Adds ~20 lines. Update `test_mode_topology` to cover the new pairs.

**Recommendation:** Option B, because the topology guard was added specifically to catch invalid transitions (CLAUDE.md ARCHITECTURE_OPPORTUNITIES item 2). A guard that ignores `from` defeats the purpose.

**Steps for Option B:**

1. In `App/Inc/calc_mode_topology.h`, after the existing `CalcMode_t` enum or comment block, add a `static const` transition table or implement the check inline:
   ```c
   static inline bool CalcMode_IsValidTransition(CalcMode_t from, CalcMode_t to) {
       /* Any mode can return to NORMAL */
       if (to == MODE_NORMAL) return true;
       /* GRAPH sub-modes require entering from GRAPH or a sibling graph mode */
       if (to == MODE_GRAPH_TRACE || to == MODE_GRAPH_FREE_CURSOR)
           return (from == MODE_GRAPH || from == MODE_GRAPH_TRACE || from == MODE_GRAPH_FREE_CURSOR);
       /* ... add remaining valid pairs based on the topology comments already in the header */
       return false;
   }
   ```
   Base the pairs on the existing comments in `calc_mode_topology.h` and on `CalcMode_t` values used in `calculator_core.c`.

2. In `App/Tests/test_mode_topology.c`, add test cases that call `CalcMode_IsValidTransition(from, to)` with known-valid and known-invalid pairs and assert the results.

**Verify:** `test_mode_topology` passes with new from→to assertions.

**Complexity delta:** Neutral.

---

### E16. ❌ [LOW] Update calculator_core.c @brief to reflect coordinator role

**Context from audit:** `calculator_core.c:2–8` `@brief` says "Calculator logic, UI management, and FreeRTOS task implementation" — a grab-bag description that confirms the four-responsibility problem without acknowledging the planned extraction path.

**Files to change:** `App/Src/calculator_core.c` (lines 2–8 Doxygen block only).

**Steps:**

1. Replace the current `@brief` with:
   ```c
   /**
    * @file  calculator_core.c
    * @brief Central coordinator for expression editing, history display, and mode routing.
    *
    * This file currently carries four responsibilities that are planned for staged
    * extraction (see ARCHITECTURE_OPPORTUNITIES.md):
    *   1. LVGL UI object creation and layout (target: dedicated ui_expr.c)
    *   2. Expression buffer editing (target: ExprEditor module)
    *   3. History display management (target: calc_history.c via callback)
    *   4. Mode routing / token dispatch (stays here as coordinator)
    *
    * CalcHistory_UpdateDisplay() is implemented here (not in calc_history.c) because
    * it requires LVGL label objects that are private to this file. See E8 in the audit.
    */
   ```

**Verify:** Build only (documentation change).

**Complexity delta:** Neutral.

---

### E17. ❌ [LOW] Verify function complexity for ⚠️ REVIEW modules

The following modules were marked ⚠️ REVIEW for function complexity but not verified by the audit. Confirm each module has no function exceeding 60 lines at nesting depth > 4. If any does, open a new `[complexity]` item in CLAUDE.md.

**Quick script:** run this from the repo root to get function line counts per file:
```sh
for f in App/Src/graph_ui_range.c App/Src/ui_graph_zoom.c App/Src/ui_input.c \
          App/Src/ui_math_menu.c App/Src/ui_stat.c App/Src/ui_stat_edit.c \
          App/Src/ui_vars.c App/Src/ui_yvars.c App/Src/ui_param_yeq.c \
          App/Src/prgm_editor.c App/Src/graph.c; do
  echo "=== $f ==="; awk '/^\w.*\(/{fn=$0; start=NR} /^\}$/{if(start>0){len=NR-start; if(len>50) print len" lines: "fn; start=0}}' "$f"; done
```

Modules to check and their known-large functions:
- `graph_ui_range.c` — `handle_range_mode`, `handle_zoom_factors_mode`
- `ui_input.c` — STO synthesis + expr-state logic
- `graph.c` — `Graph_Render`, `Graph_RenderParametric` (confirm within limits after `graph_coord.h` deduplication)
- `prgm_editor.c` — `PrgmEditor_HandleToken`
- `ui_stat_edit.c`, `ui_math_menu.c`, `ui_vars.c`, `ui_yvars.c`, `ui_param_yeq.c` — general sweep

Open a `[complexity]` item in CLAUDE.md "Next session priorities" for any function found to exceed the limit.

**Complexity delta:** Not applicable (this is a verification step, not a change).

---

### E18. ❌ [LOW] Assess calc_engine.c extern variable exposure (calc_variables / calc_matrices)

**Context from audit (A3, Interface Width, ⚠️ REVIEW):** `calc_engine.h` exports `calc_variables[27]` and `calc_matrices[4]` as raw `extern` arrays. All fields are globally mutable with no write notification. The audit asks whether an accessor seam would better control write access (e.g., for cache invalidation on write).

**Steps:**

1. `grep -rn "calc_variables\[" App/Src App/Tests | grep -v "calc_engine"` — find all write sites outside `calc_engine.c`.

2. For each write site, determine whether it needs to trigger any side effect (e.g., clearing a cached evaluation result, marking Y= variables dirty). If yes, the write should go through a setter; if no, the raw extern is acceptable.

3. Document the conclusion as a comment in `calc_engine.h` near the `extern` declarations, e.g.:
   ```c
   /* Direct write is safe: no cached state depends on these arrays. Callers that
      store to calc_variables[] are responsible for calling Calc_ClearHistory() if
      the variable change should be reflected in the history display. */
   ```
   Or, if a setter is warranted, add `void Calc_SetVariable(uint8_t idx, double val)` and route writes through it.

**Verify:** No behaviour change if result is documentation only. If setters are added, run `test_calc_engine` and `test_parse_eval`.

**Complexity delta:** Neutral or slight decrease (setter hides mutation sites).

---

### Summary Table

_Status as of 2026-05-14: ✅ Done · ⚠️ Partial (interim comments added; full callback fix deferred) · ❌ Open_

| Item | Severity | Est. Effort | Prerequisites | Primary Verification | Status (2026-05-14) |
|------|----------|-------------|---------------|----------------------|---------------------|
| E1 — FLASH sector collision | Critical | 30 min | None | Hardware + host suite | ✅ Done (commit `8db9e83`) |
| E2 — Stale comment (covered by E1) | Critical→Low | 5 min | E1 | Build only | ✅ Done (by E1) |
| E3 — prgm_exec.c UI layer dep | High | 2–3 h | None | Host suite + build | ❌ Open |
| E4 — persist.c UI layer dep | High | 1 h | E3 (model) | `test_persist_roundtrip` | ❌ Open |
| E5 — persist.c function length | High | 1 h | E4 | `test_persist_roundtrip` | ❌ Open |
| E6 — EvaluateRPN_ex complexity | High | 3–4 h | None | `test_calc_engine`, `test_parse_eval` | ❌ Open |
| E7 — graph_ui.c cursor extraction | High | 3–4 h | None | `test_graph_render` | ❌ Open |
| E8 — CalcHistory_UpdateDisplay | Medium | 30 min interim / 4 h full | ARCH_OPPS item 4 for full fix | Build only (interim) | ⚠️ Partial (comments added in `calc_history.h:7` and `calculator_core.c:808`; callback fix deferred) |
| E9 — ui_matrix.c encapsulation | Medium | 1–2 h | None | Build | ✅ Done (9 globals marked `static`; redundant raw `extern` decls removed — already in `ui_shared.h`; file-level Doxygen block added) |
| E10 — ui_prgm.h LVGL pollution | Medium | 1–2 h | E3 (pattern) | Build | ❌ Open (`prgm_store_access.h` not created) |
| E11 — extern lv_obj_t * pattern | Medium | 3–4 h (6 modules) | None | Build | ❌ Open |
| E12 — extern MenuState_t pattern | Medium | 3–4 h (6 modules) | None | Build | ❌ Open |
| E13 — prgm_exec.h naming | Low | 30 min | None | Host suite + build | ❌ Open |
| E14 — #pragma once inconsistency | Low | 15 min | None | Host suite + build | ✅ Done (both headers now use `#ifndef` guards) |
| E15 — calc_mode_topology.h `from` | Low | 1–2 h | None | `test_mode_topology` | ✅ Done (Option A: parameter removed; signature is now `CalcMode_IsValidTransition(CalcMode_t to)`) |
| E16 — calculator_core.c @brief | Low | 10 min | None | Build only | ❌ Open (still says "Calculator logic, UI management, and FreeRTOS task implementation") |
| E17 — ⚠️ REVIEW sweep | Low | 30 min | None | Open new items if found | ❌ Open |
| E18 — calc_variables extern assess | Low | 30 min | None | `test_calc_engine` | ❌ Open |
