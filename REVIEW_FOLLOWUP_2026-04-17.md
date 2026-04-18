# Code Review Follow-Up — 2026-04-17

Temporary working document. Delete when all items are closed or migrated to PROJECT_HISTORY.md.

Each item has: context, exact files/lines, risk if left unfixed, and a ready-to-use prompt.
follow up item 6 from @REVIEW_FOLLOWUP_2026-04-17.md
---

## Item 1 — [refactor] Cursor overlay dispatch duplication + prgm accessor gap

**Priority:** High  
**Effort:** Small (~30 lines changed)  
**Complexity ceiling impact:** None (refactor, zero logic change)

**Context:**  
`cursor_timer_cb` (calculator_core.c:702) and `ui_update_status_bar` (calculator_core.c:729) contain
byte-for-byte identical 6-branch if-else chains that decide which overlay screen's cursor to blink.
Every new overlay screen must be added in both places — a classic divergence trap.

The two PRGM branches inside those chains also break the encapsulation rule from COUPLING_REFACTOR T4:
```c
// All other screens use proper accessors:
else if (Graph_IsYeqScreenVisible())   yeq_cursor_update();

// But PRGM leaks its screen pointer into calculator_core.c:
else if (ui_prgm_editor_screen != NULL && !lv_obj_has_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN))
    prgm_editor_cursor_update();
```
`ui_prgm_editor_screen` and `ui_prgm_new_screen` are declared extern in `calc_internal.h` — the very
extern pattern that T4 eliminated for all other screens. If `ui_prgm.c` is ever restructured,
`calculator_core.c` silently depends on those internals.

**Fix:**
1. Add `Prgm_IsEditorScreenVisible()` and `Prgm_IsNewScreenVisible()` to `ui_prgm.c` / `ui_prgm.h`
   (same one-liner pattern as `Graph_IsYeqScreenVisible()` in `graph_ui.c:91`).
2. Remove the two `ui_prgm_*` externs from wherever they are declared in `calc_internal.h`.
3. Extract the 6-branch chain into a `static void update_overlay_cursor(void)` helper in
   `calculator_core.c`, called from both `cursor_timer_cb` and `ui_update_status_bar`.

**Files:** `App/Src/calculator_core.c:709–744`, `App/Src/ui_prgm.c`, `App/Inc/ui_prgm.h`, `App/Inc/calc_internal.h`

**Prompt to use:**
> "Follow up item 1 from REVIEW_FOLLOWUP_2026-04-17.md — refactor the cursor overlay dispatch
> duplication in calculator_core.c. Add Prgm_IsEditorScreenVisible / Prgm_IsNewScreenVisible
> accessors to ui_prgm, remove the prgm screen externs from calc_internal.h, and extract the
> shared 6-branch chain into update_overlay_cursor()."

---

## Item 2 — [refactor] COUPLING_REFACTOR T11 — stat_data / stat_results accessor gap

**Status: RESOLVED 2026-04-17** — See `docs/PROJECT_HISTORY.md` Resolved Items for details.

---

## Item 3 — [complexity] ui_refresh_display inner loop restructure

**Status: RESOLVED 2026-04-17** — See `docs/PROJECT_HISTORY.md` Resolved Items for details.

---

## Item 4 — [refactor] prgm_token_to_str lookup table

**Status: RESOLVED 2026-04-17** — Tokens are not contiguous across the enum (scattered across
numeric-input, operators, trig, log, powers, matrix, punctuation groups), so an offset array was
not viable. Replaced the 35-case switch with a `static const struct { Token_t tok; const char
*str; }` array (`k_prgm_tok_strs[]`) and a single-loop function body. Zero behaviour change;
build clean, no new warnings.

---

## Item 5 — [refactor] Process_Hardware_Key direct mode mutation

**Priority:** Medium  
**Effort:** Small (~20 lines changed)  
**Complexity ceiling impact:** None

**Context:**  
COUPLING_REFACTOR T5 made `current_mode` and `return_mode` `static` in `calculator_core.c` and
added `Calc_GetMode()` / `Calc_SetMode()` / `Calc_GetReturnMode()` / `Calc_SetReturnMode()`
accessors so other modules cannot directly write these fields. The purpose: prevent cross-task
data races and make the access boundary explicit.

`Process_Hardware_Key()` (calculator_core.c:1276) runs on keypadTask. Because it lives in the
same translation unit as the static variables, it still uses direct assignment:
```c
current_mode = return_mode;   // T5 intended this to be Calc_SetMode(Calc_GetReturnMode())
return_mode  = MODE_NORMAL;
```
This is not a runtime bug today (keypadTask only writes these in atomic-looking patterns), but it:
1. Bypasses any future validation/hook added to the setter
2. Is inconsistent with the accessor discipline enforced everywhere else
3. Would become a race condition if `current_mode` were ever read on another task path

**Fix:** Replace all direct `current_mode = ...` and `return_mode = ...` assignments in
`Process_Hardware_Key` with `Calc_SetMode(...)` / `Calc_SetReturnMode(...)` calls, and
`current_mode` reads with `Calc_GetMode()`.

**Files:** `App/Src/calculator_core.c:1276–1374`

**Prompt to use:**
> "Follow up item 5 from REVIEW_FOLLOWUP_2026-04-17.md — in Process_Hardware_Key() in
> calculator_core.c, replace all direct reads and writes of current_mode and return_mode with
> the Calc_GetMode / Calc_SetMode / Calc_GetReturnMode / Calc_SetReturnMode accessor calls
> added in COUPLING_REFACTOR T5. Zero behaviour change."

---

## Item 6 — [testing] ui_vars.c y-statistics reimplementation

**Priority:** Medium  
**Effort:** Medium (~40 lines new code, new tests)  
**Complexity ceiling impact:** None ([testing] items don't count)

**Context:**  
`ui_vars.c:112–136` re-derives y-statistics on the fly from raw `stat_data` arrays:

```c
static float vars_sum_y(void) {
    float s = 0.0f;
    for (int i = 0; i < (int)stat_data.list_len; i++) s += stat_data.list_y[i];
    return s;
}
static float vars_sum_y2(void) { ... }  // sum of squares
static float vars_sum_xy(void) { ... }  // cross product
static float vars_mean_y(void) { ... }  // mean
static float vars_sx_y(void)   { ... }  // sample std dev
```

`calc_stat.c` already implements the canonical x-stat versions (`CalcStat_Compute1Var`). The y-stat
functions above are *not tested* — they live only in `ui_vars.c` and are exercised only via the
hardware VARS menu. If a precision fix (e.g. Kahan summation, different Bessel correction) were
applied to `calc_stat.c`, the VARS menu would show different numbers for x vs. y without any
compile-time warning.

**Fix:**
1. Add `CalcStat_SumY()`, `CalcStat_SumY2()`, `CalcStat_SumXY()`, `CalcStat_MeanY()`,
   `CalcStat_SxY()` to `calc_stat.c` / `calc_stat.h`, using the same pattern as the existing
   1-var functions.
2. In `ui_vars.c`, delete the five `vars_*` static helpers and replace their call sites with the
   new `CalcStat_*` calls (passing `Stat_GetData()` — see Item 2).
3. Add assertions in `test_stat.c` covering the new y-stat functions with a known dataset.

**Files:** `App/Src/ui_vars.c:112–136`, `App/Src/calc_stat.c`, `App/Inc/calc_stat.h`,
`App/Tests/test_stat.c`

**Note:** Depends on Item 2 (Stat_GetData accessor) if you want to decouple ui_vars.c from
raw stat_data. Can be done independently by keeping stat_data access as-is in the helper functions
if Item 2 hasn't landed yet.

**Prompt to use:**
> "Follow up item 6 from REVIEW_FOLLOWUP_2026-04-17.md — add CalcStat_SumY, CalcStat_SumY2,
> CalcStat_SumXY, CalcStat_MeanY, CalcStat_SxY to calc_stat.c/h; replace the five vars_*
> static helpers in ui_vars.c with calls to the new functions; add host test assertions to
> test_stat.c covering the new functions with a known dataset."

---

## Item 7 — [complexity] graph.c three functions over 80 lines

**Status: RESOLVED 2026-04-17** — See `docs/PROJECT_HISTORY.md` Resolved Items for details.

---

## Item 8 — [complexity] eval_matrix_func 126-line / 29-case dispatch

**Priority:** Low (calc_engine well-isolated, already tested)  
**Effort:** Medium (~60 lines reorganised)  
**Status: RESOLVED 2026-04-18** — 8 if-branches extracted into `eval_mat_transpose`,
`eval_mat_det`, `eval_mat_rowswap`, `eval_mat_rowplus`, `eval_mat_mrow`, `eval_mat_mrowplus`,
`eval_mat_round`, `eval_mat_arith`. `eval_matrix_func` reduced to an 8-line dispatch.
All 10 host test suites pass. Complexity delta: decrease.

**Complexity ceiling impact:** Would close a [complexity] slot if P30 or P29 are closed first

**Context:**  
`eval_matrix_func()` in `calc_engine.c:835` is the longest single function in the codebase at
~126 lines. It contains a 29-case switch where each case independently implements a matrix
operation (det, inv, dim, fill, transpose, ref, rref, etc.):

```c
static CalcResult_t eval_matrix_func(Token_t func, CalcMatrix_t *m, ...) {
    switch (func) {
        case TOKEN_MAT_DET:   // ~8 lines of det logic
        case TOKEN_MAT_INV:   // ~12 lines of inv logic
        case TOKEN_MAT_TRANS: // ~10 lines of transpose logic
        // ... 26 more ...
    }
}
```

Each case is independent — no shared local variables between cases (or only trivial ones like
`result`). The fix is mechanical: pull each case into `static CalcResult_t eval_mat_det(...)` etc.
and replace the case body with a call. The switch becomes a 29-line dispatch table.

This is low priority because `calc_engine.c` has good test coverage and the function, while long,
has a clear single responsibility.

**Prerequisite:** Complexity ceiling must be below 3 before starting this.

**Files:** `App/Src/calc_engine.c:835`

**Prompt to use:**
> "Follow up item 8 from REVIEW_FOLLOWUP_2026-04-17.md — extract each case of the 29-case switch
> in eval_matrix_func (calc_engine.c:835) into a named static helper (eval_mat_det, eval_mat_inv,
> etc.), reducing the switch to a dispatch table of calls. Zero behaviour change; verify
> test_calc_engine still passes. Check complexity debt ceiling is below 3 before starting."

---

## Complexity Debt Ceiling Status

As of this review, 3 items are open — the ceiling is hit:

| # | Item | File | Status |
|---|---|---|---|
| 1 | P29 DRAW complexity follow-up | `graph.c`, `calculator_core.c` | Active |
| 2 | P30 STAT complexity follow-up | `ui_stat.c` | Backlog |
| 3 | ui_refresh_display inner loop | `calculator_core.c:586` | **RESOLVED 2026-04-17** |

Ceiling is now at 2 open items — one slot is free. The quickest remaining win is P29 item (2): move
`try_execute_draw_command` from `calculator_core.c` into `ui_draw_exec.c` — estimated ~30 lines
moved, closes P29 and frees another slot.

---

*Delete this file after all items are resolved or promoted to PROJECT_HISTORY.md.*
