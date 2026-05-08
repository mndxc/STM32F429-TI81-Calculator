# Architecture Deepening Opportunities

Surfaced by architectural friction survey, 2026-05-07. These are refactors that turn
shallow or entangled modules into deep ones — improving locality, testability, and
long-term maintainability. Each item is self-contained and can be tackled independently.

Items are ordered from lowest to highest effort.

---

## 1. Graph coordinate transform — eliminate silent duplication

**Effort:** Low — 1–2 hours  
**Risk:** Very low — mechanical substitution, no behaviour change, build will catch mistakes

### Problem

`graph.c` defines four private `static` coordinate transform functions:

- `math_y_to_px()` — [graph.c:187](../App/Src/graph.c#L187)
- `math_x_to_px()` — [graph.c:194](../App/Src/graph.c#L194)
- `px_to_math_x()` — [graph.c:204](../App/Src/graph.c#L204)
- `px_to_math_y()` — [graph.c:214](../App/Src/graph.c#L214)

`graph_draw.c` independently re-implements two of them under different names:

- `draw_px_to_math_x()` — [graph_draw.c:42](../App/Src/graph_draw.c#L42)
- `draw_math_y_to_px()` — [graph_draw.c:50](../App/Src/graph_draw.c#L50)

`graph_draw.c` even acknowledges the duplication in a comment at line 37:
> "These mirror math_x_to_px / math_y_to_px / px_to_math_x in graph.c"

Any change to the coordinate math (e.g. a RANGE boundary condition fix, a rounding
change for off-by-one pixel bugs) requires two independent edits with no compile-time
guarantee they stay in sync.

### Solution

Create `App/Inc/graph_coord.h` containing the four formulas as `static inline` functions
taking a `const GraphState_t *s` parameter. Both `graph.c` and `graph_draw.c` include
this header and delete their private copies.

```c
// App/Inc/graph_coord.h
#pragma once
#include "graph.h"  // GraphState_t, GRAPH_W, GRAPH_H

static inline int32_t graph_coord_math_x_to_px(const GraphState_t *s, float x) {
    return (int32_t)((x - s->x_min) / (s->x_max - s->x_min) * GRAPH_W);
}
static inline int32_t graph_coord_math_y_to_px(const GraphState_t *s, float y) {
    return (int32_t)((s->y_max - y) / (s->y_max - s->y_min) * GRAPH_H);
}
static inline float graph_coord_px_to_math_x(const GraphState_t *s, int32_t px) {
    return s->x_min + (float)px / GRAPH_W * (s->x_max - s->x_min);
}
static inline float graph_coord_px_to_math_y(const GraphState_t *s, int32_t py) {
    return s->y_max - (float)py / GRAPH_H * (s->y_max - s->y_min);
}
```

Verify the exact formulas match the existing private statics before deleting them —
the private copies in `graph.c` are the authoritative reference since they have been
exercised by the graph render integration test.

In `graph.c`: add `#include "graph_coord.h"`, delete the four private statics, update
the ~20 call sites to pass `Graph_GetState()`. In `graph_draw.c`: same for the two
private functions. Then add 4 simple assertions in `test_graph_render.c` verifying
known pixel positions at fixed RANGE values.

### Impact

- One fix site for all coordinate math bugs going forward.
- The formulas become testable through the existing `test_graph_render.c` without
  requiring the full graph renderer to run.
- `static inline` means zero runtime cost — the compiler folds them identically to
  the current private statics.

---

## 2. CalcMode_t transitions — add an enforcement seam at `Calc_SetMode`

**Effort:** Low–Medium — 2–4 hours  
**Risk:** Low — `Calc_SetMode()` already exists; this adds a guard inside it

### Problem

`Calc_SetMode()` is a one-liner at [calculator_core.c:192](../App/Src/calculator_core.c#L192):

```c
void Calc_SetMode(CalcMode_t mode) { current_mode = mode; }
```

There are also 3 sites that bypass it with direct assignment (`current_mode = mode`
at lines 856, 859, 969, 1271, 1290, 1658). All 31 direct-assignment and wrapper call
sites accept any `CalcMode_t` value without checking whether the transition is legal.

`test_mode_topology.c` validates the legal transition graph at compile/test time, but
there is no runtime enforcement. An invalid transition (e.g. entering
`MODE_GRAPH_TRACE` without a valid Y= equation, or entering `MODE_PRGM_EDITOR` while
a program is already running) silently succeeds. The bug only surfaces later when the
mode's handler runs against an unexpected state.

### Solution

Add a transition guard inside `Calc_SetMode()`. The legal-transition table already
exists in `test_mode_topology.c`; extract it into `App/Inc/calc_mode_topology.h` (a
header-only table, no new `.c` file needed) so both the test and the runtime can share
it.

```c
// In App/Inc/calc_mode_topology.h
// Table of { from, to } legal pairs — shared by test_mode_topology.c and Calc_SetMode()

// In calculator_core.c Calc_SetMode():
void Calc_SetMode(CalcMode_t mode) {
#ifndef NDEBUG
    assert(CalcMode_IsValidTransition(current_mode, mode));
#endif
    current_mode = mode;
}
```

For the 3 sites that bypass `Calc_SetMode()` via direct assignment: audit each one.
Two legitimate exceptions exist:
- Line 1658 (`current_mode = mode` inside `Calc_SimulateToken`) is a deliberate
  internal restore-after-simulation — keep as-is with a comment.
- Lines 856/859 (YEQ toggle) and 969 (mode-screen result apply) — convert to
  `Calc_SetMode()` calls; they should pass topology validation.

### Impact

- Invalid mode transitions become assert-failures in debug builds, surfacing bugs at
  the point of the bad transition rather than 100ms later in the handler.
- The topology table gains a live execution counterpart — tests and runtime share one
  source of truth.
- Any future mode added to the enum must also be added to the topology table to
  compile cleanly.
- No performance impact in release builds (`NDEBUG` strips the assert).

---

## 3. PrgmEditor extraction — split `ui_prgm.c`'s two state machines ✓ COMPLETE 2026-05-07

**Effort:** Medium — 4–7 hours  
**Risk:** Medium — touches LVGL object ownership and several cross-call sites; careful
  header organisation avoids regressions

### Problem

`ui_prgm.c` (1399 lines) contains two completely independent state machines that share
a file only because both operate on program storage:

**Program list browser** (`handle_prgm_menu` at [ui_prgm.c:686](../App/Src/ui_prgm.c#L686), ~350 lines)
— browsing the 9 program slots, renaming, deleting.

**Program line editor** (`handle_prgm_editor` at [ui_prgm.c:1027](../App/Src/ui_prgm.c#L1027), with
supporting functions `prgm_editor_cursor_update` at line 391, `ui_update_prgm_editor_display`
at line 408, `prgm_editor_insert_str` at line 472, and several `static` helpers) — the
cursor-navigated line-by-line editor shown when a program slot is opened.

The editor's LVGL objects are: `ui_prgm_editor_screen`, `prgm_edit_title_lbl`,
`prgm_edit_line_labels[14]`, `prgm_edit_scroll_down`, `prgm_edit_scroll_up`
(declared at [ui_prgm.c:64](../App/Src/ui_prgm.c#L64)), plus the cursor objects created
in `cursor_box_create()` at line 240.

The editor has no host-testable seam. Any bug in line-by-line cursor navigation,
character insertion, or scroll-to-line logic requires embedded hardware to reproduce.

The list browser and editor share **no mutable state** — the only shared dependency is
the `prgm_exec.h` storage API (`Prgm_GetLine`, `Prgm_SetLine`, `Prgm_GetLineCount`,
etc.) which both legitimately read.

### Solution

Extract a new module `App/Src/prgm_editor.c` / `App/Inc/prgm_editor.h`.

**Move to `prgm_editor.c`:**
- All LVGL objects: `ui_prgm_editor_screen`, `prgm_edit_title_lbl`,
  `prgm_edit_line_labels`, scroll arrows, cursor objects
- State: `prgm_cursor_line`, `prgm_cursor_col`, `prgm_editor_from_new`,
  `prgm_editor_scroll_top`
- Functions: `ui_init_prgm_editor_screen`, `prgm_editor_cursor_update`,
  `ui_update_prgm_editor_display`, `prgm_editor_scroll_to_line`,
  `prgm_editor_insert_str`, `prgm_editor_menu_insert`, `handle_prgm_editor`,
  and all `static` helpers (`prgm_editor_handle_nav`, `prgm_editor_handle_del_clear`,
  `prgm_editor_handle_insert`)

**Public interface for `prgm_editor.h`:**
```c
void PrgmEditor_InitScreen(void);           // called from ui_init_prgm_screens
void PrgmEditor_Open(uint8_t slot, bool from_new);  // replaces prgm_parse_from_store + show
void PrgmEditor_Close(void);                // hides screen, flattens to store
bool PrgmEditor_HandleToken(Token_t t);     // replaces handle_prgm_editor in route table
void PrgmEditor_InsertStr(const char *s);   // replaces prgm_editor_insert_str (called by sub-menus)
void PrgmEditor_MenuInsert(const char *s);  // replaces prgm_editor_menu_insert
void PrgmEditor_CursorUpdate(void);         // replaces prgm_editor_cursor_update (called by ui_prgm.c)
lv_obj_t *PrgmEditor_GetScreen(void);       // for hide/show from prgm_submenu_return_to_editor
```

`ui_prgm.c` retains only the list browser and calls through this interface. The
`prgm_submenu_return_to_editor()` / `prgm_submenu_tab_switch()` helpers at lines
1077–1088 stay in `ui_prgm.c` since they call `Calc_SetMode()` and manipulate
screen visibility — they are coordinators between the list browser and editor, not
part of the editor itself.

**Test coverage:** under `HOST_TEST`, `PrgmEditor_HandleToken()` token dispatch
(navigation, insertion, deletion, sub-menu routing) becomes unit-testable without
LVGL. Add stubs for `lvgl_lock/unlock` and `lv_*` calls (pattern already established
in `test_normal_mode.c`).

### Impact

- `ui_prgm.c` drops from 1399 to roughly 700 lines.
- The editor's line navigation and character insertion logic becomes testable in
  host builds — the most defect-prone code in the program subsystem.
- `prgm_exec.c` (execution) + `prgm_editor.c` (editing) + `ui_prgm.c` (browsing)
  now map cleanly to three distinct responsibilities in the program subsystem.
- `prgm_editor.c` goes on the CMake host-test include list alongside `prgm_exec.c`.

---

## 4. ExprEditor module — extract the expression display state machine

**Effort:** High — 6–10 hours  
**Risk:** Medium-High — touches the hot input path; the `sto_pending` STO synthesis
  rule (gotcha #21) must be preserved exactly at every call site

### Problem

The expression buffer display state is managed inline throughout `calculator_core.c`
and `ui_input.c`. It comprises:

- `ExprBuffer_t expr` — the stateful buffer wrapper
- `sto_pending` — the STO-pending flag (declared at [calculator_core.c:154](../App/Src/calculator_core.c#L154))
- The three LVGL expression display objects (expression label, cursor box, cursor inner label)
- `cursor_render()` at [calculator_core.c:359](../App/Src/calculator_core.c#L359) — the
  function that synthesises `MODE_STO` from `sto_pending` before calling the render
- 19 call sites across `calculator_core.c` that directly read/write `sto_pending`,
  `expr`, or call `cursor_render()`

Any caller that needs to refresh the expression display must know three things that
belong to the display layer, not the calling logic:
1. The STO synthesis rule (pass `sto_pending ? MODE_STO : current_mode`)
2. The three LVGL object handles
3. Which LVGL lock pattern applies (timer callback vs. task context)

This is a violation of depth: the interface a caller must understand is nearly as
complex as the implementation.

### Solution

Extract `App/Src/expr_editor.c` / `App/Inc/expr_editor.h` as a module that owns
the expression display state. The public interface:

```c
// App/Inc/expr_editor.h

typedef struct {
    ExprBuffer_t  buf;
    bool          sto_pending;
    // LVGL handles are private — module-level statics in expr_editor.c
} ExprEditorState_t;

// Lifecycle
void ExprEditor_Init(lv_obj_t *parent);     // creates LVGL objects, stored as statics
void ExprEditor_Refresh(CalcMode_t mode);   // renders buf + cursor with STO synthesis

// Buffer mutations (each calls Refresh internally)
void ExprEditor_Insert(const char *s);
void ExprEditor_Delete(void);
void ExprEditor_Clear(void);
void ExprEditor_Left(void);
void ExprEditor_Right(void);
void ExprEditor_PrependAns(const char *ans_str);

// State accessors
bool        ExprEditor_GetStoPending(void);
void        ExprEditor_SetStoPending(bool v);
const char *ExprEditor_GetBuf(void);
int         ExprEditor_GetCursor(void);
int         ExprEditor_GetLen(void);
```

The STO synthesis rule moves inside `ExprEditor_Refresh()`:
```c
void ExprEditor_Refresh(CalcMode_t mode) {
    CalcMode_t display_mode = s_sto_pending ? MODE_STO : mode;
    lvgl_lock();
    lv_label_set_text(s_expr_label, s_expr.buf);
    cursor_render(s_cursor_box, s_cursor_inner, s_row_label,
                  s_expr.cursor, display_mode);
    lvgl_unlock();
}
```

Callers in `calculator_core.c` and `ui_input.c` replace all `ExprBuffer_*` +
`cursor_render()` call pairs with a single `ExprEditor_*` call.

**Migration steps:**
1. Create `expr_editor.c` and `expr_editor.h` with the above interface as stubs
   returning immediately (so it compiles).
2. Move `ExprBuffer_t expr` and `sto_pending` from `calculator_core.c` into
   `expr_editor.c` as module-level statics. Update `calculator_core.c` to call
   `ExprEditor_Get*` accessors.
3. Move the three LVGL object handles (`s_expr_label`, `s_cursor_box`, `s_cursor_inner`)
   into `expr_editor.c`.
4. Move `cursor_render()` into `expr_editor.c` (it is already logically owned by
   the display layer, not the routing layer).
5. Implement `ExprEditor_Refresh()` with STO synthesis.
6. Replace direct `ExprBuffer_*` calls in `ui_input.c` with `ExprEditor_*` calls.
7. Replace `cursor_render()` call sites in `calculator_core.c` with `ExprEditor_Refresh()`.
8. Delete the now-unused accessors `Calc_GetStoPending` / `Calc_SetStoPending` from
   `calculator_core.h`.

**Test coverage:** `ExprEditor_Insert`, `ExprEditor_Delete`, `Left`, `Right`, `Clear`,
`PrependAns` all operate on the `ExprBuffer_t` which has no LVGL dependency. Under
`HOST_TEST`, stub `ExprEditor_Refresh` as a no-op and test the buffer mutations
directly. The STO synthesis rule becomes a single test case: after `SetStoPending(true)`,
assert `Refresh()` passes `MODE_STO` to `cursor_render`.

### Impact

- Gotcha #21 (STO cursor synthesis) is enforced at exactly one code location instead
  of being a documented rule that every future call-site must remember.
- Any overlay editor (Y=, RANGE, matrix, PRGM) that incorrectly copies a
  `calculator_core.c` expression refresh call site will get a compile error rather
  than a silent wrong-mode cursor.
- The expression display state is testable in host builds independently of the routing
  table and mode dispatch logic.
- `calculator_core.c` loses its three LVGL object handles, stepping further toward the
  architectural goal of separating UI-object ownership from mode routing.

---

## Summary

| # | Item | Files involved | Effort | Risk |
|---|------|----------------|--------|------|
| 1 | Graph coordinate transform header | [graph.c](../App/Src/graph.c), [graph_draw.c](../App/Src/graph_draw.c), new `graph_coord.h` | 1–2 h | Very low |
| 2 | `CalcMode_t` transition enforcement | [calculator_core.c](../App/Src/calculator_core.c), new `calc_mode_topology.h` | 2–4 h | Low |
| 3 | `PrgmEditor` extraction ✓ | [ui_prgm.c](../App/Src/ui_prgm.c), [prgm_editor.c](../App/Src/prgm_editor.c), [prgm_editor.h](../App/Inc/prgm_editor.h) | 4–7 h | Medium |
| 4 | `ExprEditor` module | [calculator_core.c](../App/Src/calculator_core.c), [ui_input.c](../App/Src/ui_input.c), new `expr_editor.c` / `expr_editor.h` | 6–10 h | Medium-High |
