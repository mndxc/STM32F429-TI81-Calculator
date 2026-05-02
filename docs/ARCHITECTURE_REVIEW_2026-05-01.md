# Architecture Review — 2026-05-01

**Reviewer:** Claude Code (claude-sonnet-4-6)
**Scope:** Full codebase under `App/` — module coupling, interface depth, test isolation.
**Method:** Header analysis, include-graph traversal, deletion-test applied to every suspected shallow module.

> **For future AI sessions:** This document uses the vocabulary defined in `LANGUAGE.md` (if it exists) and the deepening-opportunity framing from the `/improve-codebase-architecture` skill. Treat it as a standing backlog, not a snapshot — tick items off as they are completed and note the date. The **To-Do Checklist** at the bottom is the actionable section; the review text above each opportunity gives the "why" so future sessions can make judgment calls.

---

## Codebase Health Summary

| Metric | Finding |
|---|---|
| Total app code | ~15,500 lines across `App/Src/` |
| Isolated, testable modules | calc_engine, expr_util, calc_stat, menu_state, persist, graph (render core) |
| Hard-to-test modules | calculator_core, graph_ui, ui_prgm, graph_ui_range, all ui_prgm_*.c |
| Most-included header | calc_internal.h — 11 translation units |
| Circular include groups | graph_ui.c ↔ graph_ui_range.c ↔ ui_graph_zoom.c |
| Copy-paste boilerplate | ui_prgm_ctl/io/exec/mode.c — ~80% identical structure |
| Shared mutable extern globals | 4 (insert_mode, cursor_visible, sto_pending, expr) |

**Overall:** Core math layer (calc_engine, calc_stat, expr_util, persist) is solid and well-isolated. Architectural friction is concentrated in the UI super-module that communicates through `calc_internal.h` externs, and in the graph editor's circular include chain. All five opportunities below are surgical — none requires a major rewrite.

---

## Opportunity 1 — Encapsulate expression editor shared mutable state

### Background

`calc_internal.h` is a convenience header that re-exports eight other headers and additionally declares four `extern` mutable globals:

```c
extern bool         insert_mode;      // expression buffer insert vs. overwrite
extern bool         cursor_visible;   // blinking cursor show/hide flag
extern bool         sto_pending;      // STO key pressed, waiting for destination
extern ExprBuffer_t expr;             // expression buffer (.buf, .len, .cursor)
```

These four variables are owned by `calculator_core.c` but are readable and writable by any of the 11 translation units that include `calc_internal.h`. There is no seam — every file is an implicit adapter on the same undifferentiated mutable state.

**Deletion test:** If you deleted `calculator_core.c` as the nominal owner of this state, complexity would not concentrate — it would spray across all 11 files, because each has latent write paths to these globals. That is the opposite of depth.

**Practical consequences:**
- A bug in cursor state or insert mode requires searching 11 files, not 1.
- Adding a new module that needs to read `sto_pending` requires including the entire calc_internal.h header bundle (and its 8 transitive includes).
- No module can be compiled in isolation without linking all 11 others; host tests for any UI module require stubbing the entire super-module.

### Proposed solution

Move all four globals to `static` in `calculator_core.c`. Add getter/setter pairs to `calculator_core.h`:

```c
bool  Calc_GetInsertMode(void);
void  Calc_SetInsertMode(bool v);
bool  Calc_GetCursorVisible(void);
void  Calc_SetCursorVisible(bool v);
bool  Calc_GetStoPending(void);
void  Calc_SetStoPending(bool v);
ExprBuffer_t *Calc_GetExpr(void);   // returns pointer; callers mutate through it
```

Each of the 11 modules drops its `extern` access and calls the setter/getter instead. `calc_internal.h` removes the four `extern` declarations (it can keep its header re-exports for now, or be removed entirely as a follow-up).

### Files involved

- [App/Inc/calc_internal.h](../App/Inc/calc_internal.h) — remove the four extern declarations
- [App/Src/calculator_core.c](../App/Src/calculator_core.c) — make globals static; add getter/setter bodies
- [App/Inc/calculator_core.h](../App/Inc/calculator_core.h) — add getter/setter declarations
- 11 callers (all include calc_internal.h today): graph_ui.c, graph_ui_range.c, ui_graph_zoom.c, ui_param_yeq.c, ui_matrix.c, ui_math_menu.c, ui_prgm.c, ui_prgm_ctl.c, ui_prgm_io.c, ui_input.c, ui_mode.c

### Benefits

- **Locality:** every write to `sto_pending` is traceable via `Calc_SetStoPending`; `grep` finds all sites immediately.
- **Testability:** UI modules can be compiled with a thin `calculator_core` stub that provides the getter/setter bodies — no need to link the full dispatcher.
- **Scorecard impact:** API / header design (currently A+) stays clean; Code organisation (currently B) improves as the super-module boundary sharpens.

### Gotcha

`expr` is an `ExprBuffer_t` struct, not a scalar. The getter must return a pointer so callers can still call `ExprBuffer_Insert(&expr, ...)`. Returning by value would break every mutation site. Annotate the pointer getter with a comment that callers must not cache the pointer across a `Calc_Reset()` call.

---

## Opportunity 2 — Break the graph editor's circular include chain

### Background

The three graph editor files form a mutual-dependency triangle:

```
graph_ui.c  ──includes──▶  graph_ui_range.h  ──includes──▶  graph_ui.h
     │                                                             │
     └──includes──▶  ui_graph_zoom.h  ──────────────────────────▶ ┘
```

All three also include `calc_internal.h`. The cursor state structs (`TraceState_t`, `FreeCursorState_t`, `ZBoxState_t`) are defined in `graph_ui.c` / `graph_ui.h` but needed by `graph_ui_range.c` and `ui_graph_zoom.c` — which is why the cycle exists.

`graph.c` already owns `GraphState_t` and exposes it via a const getter (`Graph_GetState()`). Moving the three cursor structs and their mutators to `graph.c`/`graph.h` would make all three graph editor files callers of `graph.h` instead of callers of each other — breaking the cycle entirely. The T2-C architecture review item moved the cursor *data* into `graph.c`; this opportunity finishes the job at the *include* level.

**Deletion test:** Delete `graph_ui_range.c`. Does the complexity concentrate? Yes — all range-editing logic has one home. The circular include means you can't even *compile* it without dragging in `graph_ui.c`, so the deletion test can't currently be applied cleanly.

### Proposed solution

1. Move `TraceState_t`, `FreeCursorState_t`, `ZBoxState_t` struct definitions from `graph_ui.h` into `graph.h`.
2. Add accessors in `graph.c`: `Graph_GetTraceState()`, `Graph_SetTraceState()`, etc. (or pointer getters, consistent with GraphState_t pattern).
3. Remove the `graph_ui.h` include from `graph_ui_range.c` and `ui_graph_zoom.c`; replace with `graph.h`.
4. Remove the `graph_ui_range.h` and `ui_graph_zoom.h` includes from `graph_ui.c`; replace with `graph.h`.
5. Verify no remaining circular includes using `gcc -MM` on each file.

### Files involved

- [App/Inc/graph.h](../App/Inc/graph.h) — add cursor struct definitions + accessor declarations
- [App/Src/graph.c](../App/Src/graph.c) — add accessor bodies; static storage for cursor state
- [App/Inc/graph_ui.h](../App/Inc/graph_ui.h) — remove cursor struct definitions (now in graph.h)
- [App/Src/graph_ui.c](../App/Src/graph_ui.c) — update includes
- [App/Src/graph_ui_range.c](../App/Src/graph_ui_range.c) — update includes
- [App/Src/ui_graph_zoom.c](../App/Src/ui_graph_zoom.c) — update includes

### Benefits

- **Testability:** `graph_ui_range.c` (range field editing) can be compiled and tested without linking `graph_ui.c` or `ui_graph_zoom.c`.
- **Locality:** All graph viewport + cursor state lives in `graph.c`. A bug in trace state has one home.
- **Enables backlog items:** The `Graph_HandleKey()` unified entry point (listed in CLAUDE.md Large section) becomes a clean single-file change once the cycle is broken — currently it would require reordering mutually dependent headers.

### Gotcha

`graph.h` already has an include-guard named `GRAPH_MODULE_H` (not `GRAPH_H`) specifically to avoid collision with the `GRAPH_H` height constant (see CLAUDE.md gotcha #9). Do not change the guard name. Verify the cursor struct type names don't conflict with any existing `graph.h` types before moving them.

---

## Opportunity 3 — Deepen PRGM submenu screens into a generic Menu Screen module

### Background

The four PRGM submenu files share ~80% of their implementation:

| File | Lines | Unique content |
|---|---|---|
| [App/Src/ui_prgm_ctl.c](../App/Src/ui_prgm_ctl.c) | ~280 | Label list: Lbl, Goto, If, IS>, DS<, Pause, End, Stop, prgm |
| [App/Src/ui_prgm_io.c](../App/Src/ui_prgm_io.c) | ~130 | Label list: Disp, Input, DispHome, DispGraph, ClrHome |
| [App/Src/ui_prgm_exec.c](../App/Src/ui_prgm_exec.c) | ~196 | Label list + subroutine slot picker |
| [App/Src/ui_prgm_mode.c](../App/Src/ui_prgm_mode.c) | ~281 | Label list across NUMBER/GRAPH tabs |

Every file contains: static tab/cursor/scroll state, static LVGL label arrays, a `ui_init_xxx_screen()` function, a `ui_update_xxx_display()` function, a `handle_xxx_menu(Token_t t)` dispatcher, and up/down/left/right navigation functions. The navigation logic is copy-pasted.

**Deletion test:** Delete `ui_prgm_ctl.c`. The CTL menu logic is gone — nothing else absorbs it. But the *navigation boilerplate* is still in the other three files unchanged. The boilerplate was not earning its keep — it was just being duplicated.

### Proposed solution

Extract a `MenuScreen_t` module — a table-driven menu screen driver — in a new `ui_menu_screen.c` / `ui_menu_screen.h`. The driver takes a descriptor:

```c
typedef struct {
    uint8_t        tab_count;
    const char   **labels;          // flat array; tab boundaries inferred from tab_count
    uint8_t        items_per_tab;
    void         (*on_select)(uint8_t tab, uint8_t item, void *ctx);
    void          *ctx;
} MenuScreenDesc_t;
```

It owns: LVGL label creation, cursor rendering, up/down/left/right token handling, scroll logic. Each of the four PRGM submenus becomes a ~40-line descriptor + `on_select` callback.

Note: `ui_prgm_exec.c` has a subroutine picker that lists stored programs — this is more complex than a static label list. It can use the same framework with a dynamically populated label array, or remain as a one-off if the complexity of fitting it into the framework outweighs the gain. Make this call during implementation.

### Files involved

- New: `App/Src/ui_menu_screen.c`, `App/Inc/ui_menu_screen.h`
- [App/Src/ui_prgm_ctl.c](../App/Src/ui_prgm_ctl.c) — rewrite to descriptor + callback (~40 lines)
- [App/Src/ui_prgm_io.c](../App/Src/ui_prgm_io.c) — rewrite to descriptor + callback (~30 lines)
- [App/Src/ui_prgm_exec.c](../App/Src/ui_prgm_exec.c) — rewrite or adapt
- [App/Src/ui_prgm_mode.c](../App/Src/ui_prgm_mode.c) — rewrite to descriptor + callback (~50 lines, two tabs)
- [App/Src/ui_prgm.c](../App/Src/ui_prgm.c) — update calls to `ui_init_xxx_screen()`
- [docs/TECHNICAL.md](TECHNICAL.md) — add `ui_menu_screen.c` to the project structure section

### Benefits

- **Leverage:** Adding a new submenu tab is a one-line label-array change, not 200 lines of boilerplate.
- **Locality:** Navigation bugs fix once, in `ui_menu_screen.c`, not four times.
- **Scorecard impact:** Code organisation (currently B) improves as `ui_prgm.c` (1301 lines) shrinks; Function complexity (currently B) improves as the per-screen dispatcher functions collapse.

### Gotcha

The existing `menu_state.c` / `MenuState_t` module already provides `MoveUp`, `MoveDown`, `PrevTab`, `NextTab` navigation helpers. Check whether `MenuScreen_t` should *own* a `MenuState_t` internally or just wrap one. Avoid duplicating menu navigation logic — the right answer is probably that `MenuScreen_t` owns a `MenuState_t` instance and delegates list-navigation to it.

---

## Opportunity 4 — Isolate the program executor from the UI layer

### Background

`prgm_exec.c` was architected with a `PrgmOutput_t` callback struct as a seam — test builds inject a test adapter; embedded builds inject a UI adapter. This is a good pattern. However, in non-HOST_TEST builds, `prgm_exec.c` includes `calc_internal.h` and calls `CalcHistory_Commit()`, `lvgl_lock()`, and display-update functions directly, bypassing its own seam.

This means the seam is hypothetical: there is one adapter (the test adapter) but the real embedded code path does not honour it. Per the "one adapter = hypothetical seam, two adapters = real seam" principle, this is not yet a real seam.

**Impact:** `prgm_exec.c` cannot be tested against the embedded display logic without linking LVGL and the full UI super-module. A bug in `Disp` output handling could be in the executor logic or in the UI adapter — you cannot tell without reading both.

**Relevant files:** The `#include "calc_internal.h"` in `prgm_exec.c` is gated on `#ifndef HOST_TEST`. In host builds it is absent; in embedded builds it pulls in the UI super-module.

### Proposed solution

Move all `lvgl_lock()`, `CalcHistory_Commit()`, and display-update calls out of `prgm_exec.c` into the embedded adapter implementation (which lives in or near `ui_prgm.c`). `prgm_exec.c` should call *only* through `PrgmOutput_t` function pointers in all build configurations. The `#ifndef HOST_TEST` guard around the `calc_internal.h` include should then be deletable.

The embedded adapter `PrgmOutput_t` instance (currently assembled in `ui_prgm.c`) becomes the single place where the executor's output is translated into LVGL calls — consistent with how the test adapter works.

### Files involved

- [App/Src/prgm_exec.c](../App/Src/prgm_exec.c) — remove `#ifndef HOST_TEST` calc_internal.h include; route all display/history calls through `PrgmOutput_t`
- [App/Src/ui_prgm.c](../App/Src/ui_prgm.c) — move the embedded adapter's LVGL/history calls here
- [App/Inc/prgm_exec.h](../App/Inc/prgm_exec.h) — verify `PrgmOutput_t` callback signatures cover all current direct-call sites
- [App/Tests/](../App/Tests/) — add or expand `test_prgm_exec.c` to cover the cases that previously required embedded stubs

### Benefits

- **Locality:** A bug in `Disp` output is either in `prgm_exec.c` (executor logic) or the adapter in `ui_prgm.c` (rendering). Never both.
- **Testability:** `prgm_exec.c` compiles and runs identically in host and embedded builds with no `#ifdef` guards.
- **Scorecard impact:** Testing (currently A) strengthens as prgm_exec coverage no longer requires embedded stubs; API / header design (currently A+) stays clean.

### Gotcha

Audit `prgm_exec.c` for every call site that is currently gated `#ifndef HOST_TEST` or that calls a UI function directly. Map each to the appropriate `PrgmOutput_t` callback before starting — there may be edge cases (e.g., `Pause` blocking on a semaphore) that need a new callback slot in `PrgmOutput_t` rather than routing through an existing one. Do not add `#ifdef` blocks to hide the issue; if a callback slot is missing, add it.

---

## Opportunity 5 — Name and deepen the mode registration concept in the dispatcher

### Background

`calculator_core.c` contains `k_route_table[]`, a statically compiled dispatch table with 30+ entries. Adding a new `CalcMode_t` requires changes in four places: the enum in `app_common.h`, a new predicate function in `calculator_core.c`, a new handler reference in `k_route_table[]`, and a new `#include` at the top of `calculator_core.c`. There is no named concept for "a mode's full registration."

The table is not shallow — it does real work — but its interface is implicit, and the four-file edit cost per mode is a concrete maintenance burden. The current approach also means `calculator_core.c` must import every handler's header, even though it only routes tokens and never calls handler internals.

**Note:** The test `test_mode_topology.c` already guards against missing coverage in the dispatch table. This is the right approach; the deepening here is about reducing the four-file edit cost, not about correctness.

### Proposed solution

Introduce a `ModeRegistration_t` struct:

```c
typedef bool (*ModeHandler_fn)(Token_t);
typedef bool (*ModePredicate_fn)(Token_t);

typedef struct {
    CalcMode_t       mode;
    ModePredicate_fn pred;
    ModeHandler_fn   handler;
} ModeRegistration_t;
```

`k_route_table[]` becomes an array of `ModeRegistration_t`. No behavioural change — just naming the concept. As a follow-up, each module can declare its own `ModeRegistration_t` entry as a file-level constant and `calculator_core.c` can aggregate them by including one header per module instead of one per handler function.

This is intentionally conservative: it names the concept and reduces the edit cost without introducing dynamic registration (which would add complexity and break `test_mode_topology.c`'s static analysis).

### Files involved

- [App/Src/calculator_core.c](../App/Src/calculator_core.c) — rename/restructure `k_route_table[]` to use `ModeRegistration_t`
- [App/Inc/app_common.h](../App/Inc/app_common.h) — add `ModeRegistration_t` typedef (or add to a new `calculator_core.h` section)
- [App/Tests/test_mode_topology.c](../App/Tests/test_mode_topology.c) — verify it still compiles and passes after the rename

### Benefits

- **Locality:** "What does mode X do?" is answerable by finding its `ModeRegistration_t` entry — one line, not three grep results across two files.
- **Leverage:** Adding a mode requires touching one struct literal, not four locations across two files.
- **Low risk:** Pure rename/restructure; no behavioural change; `test_mode_topology.c` catches regressions immediately.

### Gotcha

Some entries in `k_route_table[]` have `NULL` predicates (always-active modes). Ensure `ModeRegistration_t` and the dispatcher loop handle `pred == NULL` correctly (treat as always-match). Document this in a comment on the struct definition — it is a non-obvious invariant.

---

## To-Do Checklist

Items are ordered within each opportunity by dependency — prerequisites first. Tick items off as completed; note the date in brackets, e.g. `[done 2026-05-15]`.

---

### Opportunity 1 — Encapsulate expression editor shared mutable state

**Prerequisite reading:** CLAUDE.md gotcha #21 (cursor_render STO synthesis) — the `sto_pending` getter/setter must preserve the synthesis rule. CLAUDE.md gotcha #13 (UTF-8 cursor integrity) — `expr` cursor state must stay consistent through the getter.

- [ ] **1-A** In `calculator_core.c`, change `insert_mode`, `cursor_visible`, `sto_pending`, `expr` from `extern`-declared to `static`.
- [ ] **1-B** Add `Calc_GetInsertMode()`, `Calc_SetInsertMode()`, `Calc_GetCursorVisible()`, `Calc_SetCursorVisible()`, `Calc_GetStoPending()`, `Calc_SetStoPending()`, and `Calc_GetExpr()` (returns `ExprBuffer_t *`) to `calculator_core.h` and implement in `calculator_core.c`.
- [ ] **1-C** Remove the four `extern` declarations from `calc_internal.h`.
- [ ] **1-D** Update each of the 11 callers (graph_ui.c, graph_ui_range.c, ui_graph_zoom.c, ui_param_yeq.c, ui_matrix.c, ui_math_menu.c, ui_prgm.c, ui_prgm_ctl.c, ui_prgm_io.c, ui_input.c, ui_mode.c) — replace direct extern access with the new getter/setter calls.
- [ ] **1-E** Verify that `cursor_render()` callers still pass the `sto_pending ? MODE_STO : current_mode` synthesis (see CLAUDE.md gotcha #21) — the setter call site should not change this logic.
- [ ] **1-F** Build with `-Werror`; fix any "implicit extern" warnings that surface previously-hidden direct-access sites.
- [ ] **1-G** Run full host test suite (`ctest` in `App/Tests/build/`) — all 14 suites should pass unchanged.
- [ ] **1-H** (Follow-up, separate session) Assess whether `calc_internal.h` is still needed once the four externs are removed. If its remaining value is only re-exporting other headers, replace each consumer's `#include "calc_internal.h"` with targeted includes of what it actually needs, and delete `calc_internal.h`. This is a complexity decrease.

---

### Opportunity 2 — Break the graph editor's circular include chain

**Prerequisite reading:** CLAUDE.md gotcha #9 (`GRAPH_MODULE_H` guard name). CLAUDE.md backlog item "Graph_HandleKey() unified entry point" — this refactor is a prerequisite for that item and should be done first.

- [ ] **2-A** Identify the full set of types that need to move: `TraceState_t`, `FreeCursorState_t`, `ZBoxState_t` — confirm their current definition locations in `graph_ui.h` and that no other header defines them.
- [ ] **2-B** Move the three cursor struct *definitions* from `graph_ui.h` to `graph.h`. Keep the include guard as `GRAPH_MODULE_H`.
- [ ] **2-C** Add static storage for the three cursor state instances in `graph.c`. Add getter/setter functions to `graph.h` consistent with the `Graph_GetState()` pattern (e.g. `Graph_GetTraceState()`, `Graph_SetTraceState()`). Pointer getters are acceptable here since cursor state is mutated in place.
- [ ] **2-D** Remove `graph_ui.h` from the include list in `graph_ui_range.c` and `ui_graph_zoom.c`. Replace with `graph.h` if not already included.
- [ ] **2-E** Remove `graph_ui_range.h` and `ui_graph_zoom.h` from the include list in `graph_ui.c`. Replace with `graph.h`.
- [ ] **2-F** Run `gcc -MM App/Src/graph_ui.c`, `gcc -MM App/Src/graph_ui_range.c`, `gcc -MM App/Src/ui_graph_zoom.c` (with the correct include paths) and confirm no remaining include cycles.
- [ ] **2-G** Build with `-Werror`; run full host test suite.
- [ ] **2-H** (Unlock follow-up) With the circular include broken, the "Graph_HandleKey() unified entry point" item in CLAUDE.md Large section is now unblocked. Update the CLAUDE.md item to note this dependency is satisfied.

---

### Opportunity 3 — Deepen PRGM submenu screens into a generic Menu Screen module

**Prerequisite reading:** `menu_state.c` / `MenuState_t` API in `App/Inc/menu_state.h` — the new `MenuScreen_t` module should own a `MenuState_t` internally and delegate list-navigation to it, not duplicate it.

- [ ] **3-A** Audit all four PRGM submenu files and list every piece of non-label logic that is file-specific (e.g., `ui_prgm_exec.c`'s dynamic subroutine listing). Decide which items can use a generic descriptor and which need a custom path. Document the decision before writing any code.
- [ ] **3-B** Write `App/Inc/ui_menu_screen.h` — define `MenuScreenDesc_t` (tab count, label arrays, item counts per tab, `on_select` callback, context pointer) and `MenuScreen_t` (owns a `MenuState_t`; LVGL object handles).
- [ ] **3-C** Write `App/Src/ui_menu_screen.c` — implement `MenuScreen_Init()`, `MenuScreen_UpdateDisplay()`, `MenuScreen_HandleToken(Token_t t)` (covers up/down/left/right/enter/2nd+quit), `MenuScreen_Destroy()`. All LVGL calls inside this file must follow the `lvgl_lock()` / `lvgl_unlock()` rule.
- [ ] **3-D** Rewrite `ui_prgm_ctl.c` using `MenuScreen_t`. Target: ≤50 lines (descriptor + `on_select` callback that calls `Prgm_InsertToken()`).
- [ ] **3-E** Rewrite `ui_prgm_io.c` using `MenuScreen_t`. Target: ≤40 lines.
- [ ] **3-F** Rewrite `ui_prgm_mode.c` using `MenuScreen_t`. Two tabs (NUMBER, GRAPH). Target: ≤60 lines.
- [ ] **3-G** Adapt or rewrite `ui_prgm_exec.c` (per the decision from 3-A). If it uses `MenuScreen_t`, target ≤50 lines; if it remains custom, document why.
- [ ] **3-H** Update `ui_prgm.c` call sites to the new init/destroy/handle signatures.
- [ ] **3-I** Add `ui_menu_screen.c` to `App/Src/CMakeLists.txt` (or equivalent build file).
- [ ] **3-J** Add `ui_menu_screen.c` to the project structure table in `docs/TECHNICAL.md`.
- [ ] **3-K** Run full host test suite. If `MenuScreen_HandleToken` logic is non-trivial, add a test in `App/Tests/` that exercises navigation (up/down/tab/wrap-around) without linking LVGL.

---

### Opportunity 4 — Isolate the program executor from the UI layer

**Prerequisite reading:** `prgm_exec.h` — understand the current `PrgmOutput_t` struct fields. `docs/PRGM_COMMANDS.md` — each supported command should have a corresponding output path; verify all are routed through `PrgmOutput_t` after this change.

- [ ] **4-A** Audit `prgm_exec.c` for every call site inside `#ifndef HOST_TEST` guards or that calls a UI function directly. Produce a list: `{call site, function called, which PrgmOutput_t slot it should route through (or "new slot needed")}`.
- [ ] **4-B** For any direct calls that have no corresponding `PrgmOutput_t` slot (e.g., a `Pause` blocking path, a display-update call), add the required slot to `PrgmOutput_t` in `prgm_exec.h`. Keep each slot's signature minimal — pass only what the callback needs.
- [ ] **4-C** In `prgm_exec.c`, replace every direct UI call with the corresponding `PrgmOutput_t` slot call. Remove the `#ifndef HOST_TEST` guard around `#include "calc_internal.h"` — it should now be unused.
- [ ] **4-D** In `ui_prgm.c`, update the embedded `PrgmOutput_t` instance to include the implementations of any new slots added in 4-B. Each implementation may call `lvgl_lock()`, `CalcHistory_Commit()`, etc. freely — it is the adapter, not the executor.
- [ ] **4-E** In `App/Tests/test_prgm_exec.c` (or a new test file), add test cases that exercise the output paths that previously required embedded stubs. The test adapter `PrgmOutput_t` can record calls to a buffer and assert on them.
- [ ] **4-F** Confirm `prgm_exec.c` compiles in host build without any LVGL or calc_internal symbols.
- [ ] **4-G** Run the full host test suite. Run hardware validation P10 (PRGM execution) on device to confirm the embedded adapter still works end-to-end. See `docs/prgm_manual_tests.md`.

---

### Opportunity 5 — Name and deepen the mode registration concept in the dispatcher

**Prerequisite reading:** `test_mode_topology.c` — understand how it validates the dispatch table. The `ModeRegistration_t` struct must be compatible with whatever static analysis the topology test does.

- [x] **5-A** [done 2026-05-01] `ModeRegistration_t` (mode + pred + handler) defined file-locally in `calculator_core.c`. `pred == NULL` means "fires when current_mode == mode" (documented in struct comment).
- [x] **5-B** [done 2026-05-01] `k_route_table[]` rewritten as array of `ModeRegistration_t`. 27 `pred_mode_xxx`/`pred_prgm_running` one-liners eliminated; simple mode entries use `pred = NULL`.
- [x] **5-C** [done 2026-05-01] Dispatcher loop updated: `fires = e->pred ? e->pred(t) : (current_mode == e->mode)`.
- [x] **5-D** [done 2026-05-01] `test_mode_topology` passes unchanged.
- [x] **5-E** [done 2026-05-01] Full host test suite 14/14, 964 assertions.
- [ ] **5-F** (Optional follow-up) Assess whether each module should declare its own `ModeRegistration_t g_xxx_registration` constant in its `.c` file, and `calculator_core.c` aggregates them by including a thin per-module registration header. This would reduce the four-file edit cost to a one-file change. Only worth doing if a new mode is being added soon — do not restructure speculatively.

---

## Cross-cutting notes for all opportunities

- **Complexity delta:** Each opportunity is expected to be a complexity *decrease* (existing logic moved or consolidated; no new behaviour added). After each commit, rate neutral/decrease and update CLAUDE.md accordingly.
- **Build safety:** All changes must compile with `-Werror`. The STM32 embedded build and the host test build must both pass before closing any item.
- **Scorecard tracking:** Completing Opportunity 1 improves "API / header design" and "Code organisation". Completing Opportunity 2 improves "Code organisation". Completing Opportunity 3 improves "Code organisation" and "Function complexity". Completing Opportunity 4 strengthens "Testing". Completing Opportunity 5 is neutral or slight improvement to "Code organisation". Update `CLAUDE.md` scorecard after each opportunity lands.
- **Sync check:** Run `scripts/check_sync.sh` before every commit (see `feedback_build_command.md` memory).
- **Do not combine opportunities in one commit.** Each should land as an independent, reviewable change so regressions are easy to bisect.
