# Coupling Refactor Plan

> **Temporary working document.** When all tasks are complete, move the summary
> to a `docs/PROJECT_HISTORY.md` milestone entry and delete this file.

---

## Why this exists

An architectural audit identified several places where one module directly reads
or writes another module's internal state with no accessor layer between them.
The practical consequence: a change to any shared data structure (GraphState_t,
ProgramStore_t, ans, LVGL screen pointers) cascades to a dozen files with no
compile-time guide for which ones need updating.

The ten tasks below each close one coupling gap. Every task is self-contained and
can be completed in a single session. Tasks in Tier 1 (T1–T3) have the highest
impact and no dependencies. Tier 2 tasks (T4–T6) build on each other. Tier 3
tasks (T7–T9) are best done after their Tier 2 prerequisites.

---

## Dependency map

```
T1  T2  T3  T4  T8  T10     ← fully independent; do in any order
         |   |
         T5  T6  ← T5 before T6 is natural but not strictly required
              |
              T7  ← best after T5 and T6
T1+T2+T3 ──> T9  ← persist layer becomes clean only after raw-access is gone
```

---

## Standard post-task steps

Run these after **every** task before moving on. They are the same every time.

```
1.  ctest --test-dir build-tests -V
        → all suites must pass; zero new failures permitted.

2.  cmake --build build/Debug -j4
        → 0 errors, 0 new warnings. -Werror is active; a warning = a failure.

3.  ./scripts/check_sync.sh
        → if any new .c or .h file was added, update docs/TECHNICAL.md and
          docs/ARCHITECTURE.md so check_sync.sh passes.

4.  Complexity delta — rate the commit neutral / increase / decrease.
        → if increase: add a [complexity] item to CLAUDE.md "Next session
          priorities" before committing.

5.  Scorecard — decide if any dimension changed (API/header design, code
    organisation, function complexity, testing). If a rating changed:
        a. Update the table in CLAUDE.md "Quality Scorecard".
        b. Add a row to the Scorecard Change Log in CLAUDE.md.
        c. Add a Milestone Reviews entry to docs/PROJECT_HISTORY.md.

6.  Session log — add one bullet to the Session Log in docs/PROJECT_HISTORY.md.

7.  CLAUDE.md cleanup — if the completed task matches a backlog item in
    CLAUDE.md "Next session priorities", move it to docs/PROJECT_HISTORY.md
    "Resolved Items" and remove it from the backlog.

8.  Mark the task complete in this file (change [ ] to [x] in the index below).
```

---

## Task index

| ID  | Title                                    | Tier | Status |
|-----|------------------------------------------|------|--------|
| T1  | Graph state accessor layer               | 1    | [x]    |
| T2  | `g_prgm_store` accessor API              | 1    | [x]    |
| T3  | ANS getter/setter                        | 1    | [x]    |
| T4  | LVGL screen show/hide API                | 2    | [x]    |
| T5  | Mode transition via `Calc_SetMode()`     | 2    | [x]    |
| T6  | `menu_open`/`menu_close` module delegation | 2  | [x]    |
| T7  | `Execute_Token` dispatch table           | 3    | [x]    |
| T8  | Extract `calc_history.c`                 | 2    | [x]    |
| T9  | Move `BuildPersistBlock` to `persist.c`  | 3    | [x]    |
| T10 | `MenuState_t` retrofit remaining modules | 1    | [ ]    |

---

## T1 — Graph state accessor layer

**Problem.** `GraphState_t graph_state` (defined in `calculator_core.c`, declared
`extern` in `app_common.h:147`) is written directly by eleven modules with no
validation gate. Any change to the struct layout breaks all eleven. This is the
highest-impact coupling in the project.

**Writers today:** `graph_ui.c` (equations), `graph_ui_range.c` (window bounds),
`ui_param_yeq.c` (param pairs), `ui_mode.c` (param_mode, grid_on), `ui_yvars.c`
(enabled flags), `ui_graph_zoom.c` (zoom presets), `graph.c` (ZBox commit),
`calculator_core.c` (initialiser + persist load).

**Goal.** Add a thin API to `graph.h`/`graph.c` so every write goes through a
named function. Direct field writes (`graph_state.x_min = ...`) become
compile errors outside of `graph.c` once `graph_state` is no longer exported.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS, LVGL)
Task: Add a graph state accessor layer so external modules can no longer
directly write to GraphState_t fields.

Context
-------
GraphState_t is defined in App/Inc/app_common.h:119-144 and declared extern in
the same file at line 147. The global is currently defined in
App/Src/calculator_core.c around line 127. Eleven modules write to it directly
through calc_internal.h, which re-exports the extern.

Goal
----
1. Move the `graph_state` global definition from calculator_core.c into
   App/Src/graph.c (it is logically owned by the graph subsystem).

2. Add write accessor functions to App/Inc/graph.h and implement them in
   App/Src/graph.c. You need at minimum:

     void Graph_SetEquation(uint8_t idx, const char *eq);
     void Graph_SetEquationEnabled(uint8_t idx, bool enabled);
     void Graph_SetWindow(float xmin, float xmax, float ymin, float ymax,
                         float xscl, float yscl, float xres);
     void Graph_SetParamEquation(uint8_t idx, const char *x_eq, const char *y_eq);
     void Graph_SetParamEnabled(uint8_t idx, bool enabled);
     void Graph_SetParamWindow(float tmin, float tmax, float tstep);
     void Graph_SetParamMode(bool param);
     void Graph_SetGridOn(bool on);
     void Graph_SetActive(bool active);

   For read access, add:
     const GraphState_t *Graph_GetState(void);

   graph.c already reads graph_state for rendering — those reads can continue
   using the local definition now that it lives there.

3. Migrate every write site in the following files to use the new accessors.
   Migrate one file at a time, rebuilding and running ctest after each:
     a. graph_ui.c        — equations[], enabled[], active
     b. graph_ui_range.c  — xmin/xmax/ymin/ymax/xscl/yscl/xres, tmin/tmax/tstep
     c. ui_param_yeq.c    — param_x[], param_y[], param_enabled[]
     d. ui_mode.c         — param_mode, grid_on
     e. ui_yvars.c        — enabled[]
     f. ui_graph_zoom.c   — all zoom preset writes
     g. calculator_core.c — persist load (Calc_ApplyPersistBlock), initialiser

4. Remove `extern GraphState_t graph_state;` from app_common.h (line 147).
   The comment block at lines 80-112 that documents field ownership stays —
   update it to say "see Graph_GetState() in graph.h" for external reads.

5. Read sites (graph.c rendering, persist.c save, graph_draw.c) use
   `Graph_GetState()` or direct access if inside graph.c.

Constraints
-----------
- Do NOT change GraphState_t field names — that would break persist compatibility.
- Do NOT add validation logic inside accessors yet (that is a future task).
  Keep accessors as thin wrappers that assign and return.
- All writes must be under lvgl_lock() as before — the accessor functions do
  NOT acquire the lock; callers already hold it.
- Run `ctest --test-dir build-tests -V` after each file migration.
- Run `cmake --build build/Debug -j4` when all migrations are done.

This task resolves the "[refactor] graph_state accessor extraction" item in
CLAUDE.md "Next session priorities".
```

---

## T2 — `g_prgm_store` accessor API

**Problem.** `g_prgm_store` (a 19 KB struct) is declared `extern` in
`prgm_exec.h:120` and written directly by `ui_prgm.c` at more than eight call
sites. There is no bounds checking at the write point. Changing
`PRGM_NAME_LEN`, `PRGM_BODY_LEN`, or the slot count touches both files.

**Goal.** Add named write accessors to `prgm_exec.h`/`prgm_exec.c` and migrate
all direct struct writes in `ui_prgm.c` to use them. Then remove the
`extern ProgramStore_t g_prgm_store;` from `prgm_exec.h`.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS)
Task: Add accessor functions for g_prgm_store so ui_prgm.c can no longer
directly write into ProgramStore_t fields.

Context
-------
g_prgm_store is declared extern in App/Inc/prgm_exec.h:120 and defined in
App/Src/prgm_exec.c around line 120. App/Src/ui_prgm.c directly accesses
g_prgm_store.names[idx] and g_prgm_store.bodies[idx] at least 8 times
(lines ~258, 278, 304, 333, 588–596, 841, 851). These direct writes have no
bounds checking.

Goal
----
1. Add the following to App/Inc/prgm_exec.h (public API section) and implement
   in App/Src/prgm_exec.c:

     /* Read accessors */
     const char *Prgm_GetName(uint8_t slot);
     const char *Prgm_GetBody(uint8_t slot);
     bool        Prgm_IsSlotOccupied(uint8_t slot);

     /* Write accessors (with bounds checking) */
     void Prgm_SetName(uint8_t slot, const char *name);
     void Prgm_AppendLine(uint8_t slot, const char *line);
     void Prgm_SetBody(uint8_t slot, const char *body);
     void Prgm_ClearSlot(uint8_t slot);

   Prgm_SetName must clamp to PRGM_NAME_LEN.
   Prgm_AppendLine must check remaining space in PRGM_BODY_LEN.
   Prgm_SetBody must check length and null-terminate safely.

2. Migrate every direct write in App/Src/ui_prgm.c to use these accessors.
   Reads (display/list) can continue through Prgm_GetName/Prgm_GetBody.

3. ui_prgm_exec.c, ui_prgm_ctl.c, ui_prgm_io.c — audit for any direct accesses
   and migrate those too.

4. Once no call site writes g_prgm_store directly, remove the extern from
   prgm_exec.h. prgm_exec.c's own internal code (load, save, execute) can
   still access the struct directly since it is in the same translation unit.

Constraints
-----------
- prgm_exec.c already has Prgm_Init, Prgm_Save, Prgm_Load — follow that
  naming convention (Prgm_ prefix, PascalCase verb).
- Do not change ProgramStore_t field layout (FLASH compatibility).
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`
  when done.
```

---

## T3 — ANS getter/setter

**Problem.** `ans` (float) and `ans_is_matrix` (bool) are defined in
`calculator_core.c` and declared `extern` in `calc_internal.h:56-57`. Multiple
modules write these globals independently — `ui_input.c:143-144` writes both
during STO, `calculator_core.c` writes them in the evaluator result path,
and `prgm_exec.c` reads ans implicitly. If `ans_is_matrix` is set but `ans`
holds a stale scalar, silent wrong results occur.

**Goal.** Create a single write path that always sets both values atomically.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS)
Task: Replace direct extern writes to `ans` and `ans_is_matrix` with a
getter/setter API so the two values are always updated together.

Context
-------
`ans` (float) and `ans_is_matrix` (bool) are declared extern in
App/Inc/calc_internal.h lines 56-57 and defined in App/Src/calculator_core.c.
Multiple modules write them independently:
  - App/Src/ui_input.c:143-144 (STO path)
  - App/Src/calculator_core.c (main evaluator result path — search for
    `ans =` and `ans_is_matrix =`)

Goal
----
1. Add to App/Inc/calculator_core.h (the public header for calculator_core.c):

     void  Calc_SetAnsScalar(float value);
     void  Calc_SetAnsMatrix(void);   /* ans_is_matrix=true; ans value unused */
     float Calc_GetAns(void);
     bool  Calc_GetAnsIsMatrix(void);

2. In App/Src/calculator_core.c, implement the four functions as simple
   one-liners that read/write the static locals (change `ans` and
   `ans_is_matrix` from extern globals to static locals in calculator_core.c).

3. Remove `extern float ans;` and `extern bool ans_is_matrix;` from
   calc_internal.h. Add `#include "calculator_core.h"` wherever the includes
   are needed to get the accessors.

4. Migrate every write site:
   - App/Src/ui_input.c: replace `ans = result.value; ans_is_matrix = false;`
     with `Calc_SetAnsScalar(result.value);` etc.
   - App/Src/calculator_core.c: migrate all internal write sites to call the
     functions (or keep as direct writes since it is the owning translation unit).

5. Migrate read sites in persist, ui_vars, ui_stat to use Calc_GetAns() /
   Calc_GetAnsIsMatrix().

Constraints
-----------
- calc_engine.c already receives ans as a parameter to Evaluate() — that
  pattern is correct; do NOT change it. The getter just feeds the call site.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
```

---

## T4 — LVGL screen show/hide API

**Problem.** LVGL screen object pointers (`ui_matrix_screen`,
`ui_graph_yeq_screen`, `ui_stat_screen`, etc.) are declared `extern` in
`calc_internal.h:65-73` and in their owning modules' headers. The
`hide_all_screens()` function in `calculator_core.c` calls
`lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)` for every screen by name.
Adding or renaming a screen requires editing `calculator_core.c` and at least
two headers. Screen lifecycle is not encapsulated.

**Goal.** Each module exposes `Module_Show()` / `Module_Hide()` functions.
`calculator_core.c` calls these functions; it no longer holds extern pointers
to other modules' LVGL objects.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS, LVGL)
Task: Encapsulate LVGL screen pointers inside their owning modules and replace
extern pointer access with show/hide functions.

Context
-------
calc_internal.h lines 65-73 declare extern LVGL screen pointers that are
owned by other modules (graph_ui.c, graph_ui_range.c, ui_graph_zoom.c,
ui_param_yeq.c, ui_matrix.c). Additional screen pointers (ui_stat_screen,
ui_draw_screen, ui_vars_screen, ui_yvars_screen) appear in their module .h
files but are also referenced by calculator_core.c. `hide_all_screens()` in
calculator_core.c around line 1070 calls lv_obj_add_flag for every screen.

Goal
----
For each module that owns a screen, add two functions to its public header:

  void ModuleName_ShowScreen(void);
  void ModuleName_HideScreen(void);

Modules to cover (match existing header prefix convention):
  graph_ui.c/h        → Graph_ShowYeqScreen / Graph_HideYeqScreen
  graph_ui_range.c/h  → Graph_ShowRangeScreen / Graph_HideRangeScreen
                        Graph_ShowZoomFactorsScreen / Graph_HideZoomFactorsScreen
  ui_graph_zoom.c/h   → Zoom_ShowScreen / Zoom_HideScreen
  ui_param_yeq.c/h    → ParamYeq_ShowScreen / ParamYeq_HideScreen
  ui_matrix.c/h       → Matrix_ShowMenuScreen / Matrix_HideMenuScreen
                        Matrix_ShowEditScreen / Matrix_HideEditScreen
  ui_stat.c/h         → Stat_ShowMenuScreen / Stat_HideMenuScreen
                        Stat_ShowEditScreen / Stat_HideEditScreen
                        Stat_ShowResultsScreen / Stat_HideResultsScreen
  ui_draw.c/h         → Draw_ShowScreen / Draw_HideScreen
  ui_vars.c/h         → Vars_ShowScreen / Vars_HideScreen
  ui_yvars.c/h        → Yvars_ShowScreen / Yvars_HideScreen
  ui_math_menu.c/h    → already has math_menu_open/close — audit whether the
                        pointer is leaked; wrap if so.

Implementations are one-liners:
  void Matrix_HideMenuScreen(void) {
      lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
  }

After adding all show/hide functions:
1. Replace every direct `lv_obj_add_flag(ui_*_screen, ...)` call in
   calculator_core.c with the appropriate Hide function.
2. Remove the extern screen pointer declarations from calc_internal.h (lines
   65-73) once calculator_core.c no longer references them directly.
3. Make the screen pointer statics (remove extern from module .h files) if
   nothing outside that module still needs the pointer.

Constraints
-----------
- All LVGL calls must be inside lvgl_lock()/lvgl_unlock() as before. The
  show/hide functions themselves do NOT acquire the lock — caller holds it.
- Only add show/hide functions. Do not refactor screen creation or initialization
  in this task.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
```

---

## T5 — Mode transition via `Calc_SetMode()`

**Problem.** `current_mode` and `return_mode` are declared `extern` in
`calc_internal.h:52-53` and written raw (`current_mode = MODE_VARS_MENU;`) from
at least twenty sites across twelve files. There is no single place to add
invariant checks, logging, or transition guards. Every mode-related refactor
must grep for raw assignments.

**Goal.** Provide `Calc_SetMode()` and `Calc_SetReturnMode()` as the single
path for mode changes. Make `current_mode` a static local in
`calculator_core.c`.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS)
Task: Route all current_mode and return_mode writes through setter functions
so invariants can be added in one place in the future.

Context
-------
`current_mode` (CalcMode_t) and `return_mode` (CalcMode_t) are declared extern
in App/Inc/calc_internal.h lines 52-53 and written from ~20 sites in
~12 files. All sites do plain assignments: `current_mode = MODE_XYZ;`.

Goal
----
1. Add to App/Inc/calculator_core.h:

     void         Calc_SetMode(CalcMode_t mode);
     void         Calc_SetReturnMode(CalcMode_t mode);
     CalcMode_t   Calc_GetMode(void);
     CalcMode_t   Calc_GetReturnMode(void);

2. In App/Src/calculator_core.c, implement as simple assignments to the
   now-static `current_mode` and `return_mode` locals.

3. Remove `extern CalcMode_t current_mode;` and `extern CalcMode_t return_mode;`
   from calc_internal.h.

4. Add `#include "calculator_core.h"` to each module that currently uses those
   externs and migrate every write site. Read sites (`if (current_mode == X)`)
   become `if (Calc_GetMode() == X)`.

   Files to migrate (audit all; list is non-exhaustive):
     calculator_core.c, ui_input.c, ui_mode.c, ui_matrix.c, ui_stat.c,
     ui_draw.c, ui_vars.c, ui_yvars.c, ui_math_menu.c, ui_prgm.c,
     graph_ui.c, graph_ui_range.c, ui_graph_zoom.c, ui_param_yeq.c,
     prgm_exec.c.

Migrate one file at a time; rebuild and run ctest after each.

Constraints
-----------
- MODE_STO is a synthetic value (see CLAUDE.md gotcha #21) that is never set
  as current_mode. The cursor_render() call site in calculator_core.c uses
  `sto_pending ? MODE_STO : Calc_GetMode()` — keep this pattern.
- MODE_GRAPH_TRACE fallthrough (CLAUDE.md gotcha #12) must still work: after
  exiting trace mode the dispatcher continues to process the triggering key.
  This is unaffected by using Calc_SetMode() — just confirm.
- Do not add any business logic inside Calc_SetMode() in this task — keep it
  as a plain setter.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
```

---

## T6 — `menu_open`/`menu_close` module delegation

**Problem.** `menu_open()` in `calculator_core.c` (around line 960) contains a
`switch` that manually initialises `MenuState_t` fields, sets `current_mode`,
and shows a screen for each menu token — all in one place. `menu_close()`
(around line 1022) resets those same fields and hides every screen. Adding a
new menu requires editing both of these switch blocks in `calculator_core.c`,
plus the `hide_all_screens()` near line 1070. Three places, one file, no
compile-time enforcement.

**Prerequisite:** T4 (screen show/hide API) and T5 (Calc_SetMode) should be
done first so the delegation functions can use the cleaner APIs.

**Goal.** Each menu module exposes `Module_Open(CalcMode_t return_to)` and
`Module_Close(void) → CalcMode_t`. `menu_open`/`menu_close` in
`calculator_core.c` become thin dispatchers that call those functions.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS, LVGL)
Task: Extract menu initialisation and teardown from menu_open()/menu_close()
in calculator_core.c into per-module open/close functions.

Context
-------
menu_open() in App/Src/calculator_core.c (~line 960) contains a switch over
TOKEN_MATH, TOKEN_MATRX, TOKEN_PRGM, TOKEN_STAT, TOKEN_DRAW, TOKEN_VARS,
TOKEN_Y_VARS. For each token it: initialises a MenuState_t, calls
Calc_SetMode() (or sets current_mode raw today), and calls a module show
function. menu_close() (~line 1022) does the reverse.

Some modules (MATH, PRGM) already have their own open/close functions
(math_menu_open, prgm_menu_open). Extend that pattern to the remaining ones.

Goal
----
For each menu token that is handled inline in menu_open/menu_close, add to the
owning module:

  void Matrix_MenuOpen(CalcMode_t return_to);
  CalcMode_t Matrix_MenuClose(void);

  void Stat_MenuOpen(CalcMode_t return_to);
  CalcMode_t Stat_MenuClose(void);

  void Draw_MenuOpen(CalcMode_t return_to);
  CalcMode_t Draw_MenuClose(void);

  void Vars_MenuOpen(CalcMode_t return_to);
  CalcMode_t Vars_MenuClose(void);

  void Yvars_MenuOpen(CalcMode_t return_to);
  CalcMode_t Yvars_MenuClose(void);

Each Open function:
  - Initialises its MenuState_t (tab, cursor, scroll, return_mode = return_to)
  - Calls Calc_SetMode(MODE_XYZ)
  - Calls ModuleName_ShowScreen() (from T4)
  - Calls the module's display update function

Each Close function:
  - Resets its MenuState_t
  - Returns the saved return_mode (does NOT call Calc_SetMode itself —
    menu_close() in calculator_core.c sets the mode after calling this)

After all modules have Open/Close:
1. Replace the inline switch cases in menu_open() with single-line calls.
2. Replace the inline switch cases in menu_close() with single-line calls.
3. The lvgl_lock()/unlock() remains in menu_open()/menu_close() — the module
   functions do NOT acquire the lock.

Constraints
-----------
- Do not change the TOKEN_MATH / TOKEN_TEST path — math_menu_open/close
  already exist; just confirm they match the pattern.
- All MenuState_t fields must still be reset on close (prevents stale state
  on re-open).
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
```

---

## T7 — `Execute_Token` dispatch table

**Problem.** `Execute_Token()` in `calculator_core.c` (lines 1366–1396) is a
linear sequence of `if (current_mode == MODE_X) { handle_X(t); return; }`.
Adding a new mode requires inserting a new if-line here with no compile-time
enforcement that the handler was registered. The function will grow without
bound as features are added.

**Prerequisite:** T5 (Calc_SetMode/GetMode) should be done so the dispatch can
use `Calc_GetMode()`.

**Goal.** Replace the if-chain with a dispatch table so new modes are
self-registering.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS)
Task: Replace the linear if-chain in Execute_Token() with a static dispatch
table.

Context
-------
Execute_Token() in App/Src/calculator_core.c lines 1364-1396 routes token
processing via 30 sequential if-statements of the form:
  if (current_mode == MODE_X) { handle_X(t); return; }

Goal
----
1. Define a type:
     typedef bool (*ModeHandler_t)(Token_t);

2. Define a table:
     typedef struct { CalcMode_t mode; ModeHandler_t handler; } ModeEntry_t;
     static const ModeEntry_t k_mode_handlers[] = {
         { MODE_GRAPH_YEQ,         handle_yeq_mode },
         { MODE_GRAPH_RANGE,       handle_range_mode },
         ...
     };

   Include every mode that currently has an if-check EXCEPT:
     - MODE_PRGM_RUNNING (uses handle_prgm_running with no bool return — handle
       it as a special case before the table, same as today)
     - The two ALPHA_LOCK special cases (route_by_return_mode pattern) — keep
       as explicit cases before or after the table
     - The TOKEN_ON / TOKEN_QUIT / TOKEN_MODE pre-checks — keep before the table

3. Replace the if-chain with:
     for (size_t i = 0; i < ARRAY_SIZE(k_mode_handlers); i++) {
         if (Calc_GetMode() == k_mode_handlers[i].mode) {
             if (k_mode_handlers[i].handler(t)) return;
             break;
         }
     }
   (ARRAY_SIZE macro: `#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))`)

4. Add a _Static_assert at file scope:
     _Static_assert(ARRAY_SIZE(k_mode_handlers) == EXPECTED_COUNT,
                    "mode handler count mismatch — update k_mode_handlers");
   where EXPECTED_COUNT is the number of entries you put in the table. This
   ensures that removing or renaming a mode from CalcMode_t will either cause a
   linker error (missing handler) or trigger the assert.

Constraints
-----------
- Preserve all existing handler function signatures (bool return = "handled").
- The ALPHA_LOCK routing logic (lines 1386-1395) must work identically after
  the refactor — test it manually or add a test.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
- Rate complexity delta: this should be neutral or a decrease.
```

---

## T8 — Extract `calc_history.c`

**Problem.** `calculator_core.c` owns the history ring buffer (`history[]`,
`history_count`, `history_recall_offset`, matrix ring state) and six
history-related functions (`ui_update_history`, `handle_history_nav`,
`reset_matrix_scroll_focus`, `format_calc_result`, etc.) that have nothing to do
with token dispatch. These are about 150–200 lines that can move without
touching any other module.

**Goal.** Create `App/Src/calc_history.c` + `App/Inc/calc_history.h`. Move
history state and functions there. `calculator_core.c` calls the history API;
`calc_internal.h` shrinks by the six history externs.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS, LVGL)
Task: Extract history management from calculator_core.c into calc_history.c.

Context
-------
App/Inc/calc_internal.h lines 117-130 declare history state and functions:
  - HistoryEntry_t history[HISTORY_LINE_COUNT] (line 117)
  - history_count, history_recall_offset (lines 118-119)
  - matrix_scroll_focus, matrix_scroll_offset (lines 120-121)
  - extern ExprBuffer_t expr (line 123)
  - ui_update_history(), ui_refresh_display(), ui_output_row() (lines 125-127)
  - format_calc_result() (line 128)
  - handle_history_nav() (line 129)
  - reset_matrix_scroll_focus() (line 130)

These are defined in calculator_core.c. Find them there and move them.

Goal
----
1. Create App/Inc/calc_history.h with:
   - Include guard CALC_HISTORY_H
   - HistoryEntry_t typedef (move from calc_internal.h) or just #include
     calc_internal.h for the type if HistoryEntry_t is needed widely
   - Public API:
       void CalcHistory_Init(void);
       void CalcHistory_Commit(const char *expression, const char *result,
                               bool has_matrix, uint8_t ring_idx,
                               uint8_t ring_gen, uint8_t rows_cache);
       void CalcHistory_RecallUp(void);
       void CalcHistory_RecallDown(void);
       void CalcHistory_GetExprForRecall(char *buf, size_t len);
       int8_t CalcHistory_GetRecallOffset(void);
       void CalcHistory_ResetMatrixScroll(void);
       void CalcHistory_UpdateDisplay(void);   /* triggers LVGL label update */

2. Create App/Src/calc_history.c implementing the above. Move the static
   variables (`history`, `history_count`, `history_recall_offset`,
   `matrix_scroll_focus`, `matrix_scroll_offset`) from calculator_core.c.

3. Add calc_history.c to CMakeLists.txt source lists (both firmware and
   test targets if history has host-test coverage; check App/Tests/).

4. Remove the history externs from calc_internal.h (lines 117-130).
   Replace with `#include "calc_history.h"` where needed.

5. Update calls in calculator_core.c and ui_input.c to use the new API.

Constraints
-----------
- format_calc_result() is called from ui_input.c and calculator_core.c — it
  can move to calc_history.c or stay in calculator_core.c; choose the one that
  minimises new includes. Document the decision with a comment.
- After adding calc_history.c: run ./scripts/check_sync.sh — it will fail
  until you add the new file to docs/TECHNICAL.md and docs/ARCHITECTURE.md.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
```

---

## T9 — Move `BuildPersistBlock` / `ApplyPersistBlock` to `persist.c`

**Problem.** `Calc_BuildPersistBlock()` and `Calc_ApplyPersistBlock()` in
`calculator_core.c` (around lines 174–290) know the full internal layout of
every subsystem — graph_state, stat_data, calc_variables, matrices, ans, history.
Persist logic and calculator logic are the same translation unit. When
`PersistBlock_t` changes, the developer has to understand both at once.

**Prerequisite:** T1 (graph accessor), T2 (prgm accessor), T3 (ans getter/setter)
should be done first so the build/apply code calls accessors rather than touching
raw structs directly.

**Goal.** Move both functions into `persist.c` (they are already
persist-domain logic). `calculator_core.c` calls `Persist_BuildBlock()` /
`Persist_ApplyBlock()`.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS)
Task: Move Calc_BuildPersistBlock and Calc_ApplyPersistBlock from
calculator_core.c into persist.c, rename them to Persist_BuildBlock and
Persist_ApplyBlock.

Context
-------
App/Src/calculator_core.c contains Calc_BuildPersistBlock() and
Calc_ApplyPersistBlock() which fill/consume a PersistBlock_t. These functions
are pure persist logic: they read calculator state into a flat struct, or
apply a loaded struct back to live state. They belong in App/Src/persist.c
alongside Persist_Save() and Persist_Load().

Prerequisites: T1, T2, T3 should be complete so the functions use
Graph_GetState(), Prgm_GetName()/Prgm_GetBody(), Calc_GetAns() etc. rather
than direct extern access.

Goal
----
1. Move the full bodies of Calc_BuildPersistBlock and Calc_ApplyPersistBlock
   into App/Src/persist.c. Rename them:
     PersistBlock_t Persist_BuildBlock(void);
     void           Persist_ApplyBlock(const PersistBlock_t *block);

2. Declare them in App/Inc/persist.h.

3. In App/Src/calculator_core.c, replace calls to the old functions with calls
   to Persist_BuildBlock / Persist_ApplyBlock.

4. Remove the old Calc_BuildPersistBlock / Calc_ApplyPersistBlock from
   calculator_core.c and their declarations from calculator_core.h (if declared
   there) or calc_internal.h.

5. Add includes to persist.c for whatever module headers are needed to call
   the T1/T2/T3 accessors (graph.h, prgm_exec.h, calculator_core.h, etc.).

Constraints
-----------
- Do not change PersistBlock_t layout — FLASH compatibility.
- The persist roundtrip test (App/Tests/test_persist_roundtrip.c) must still
  pass — it tests Persist_Save/Load, not Build/Apply, so it should be
  unaffected.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
- Run ./scripts/check_sync.sh — no new files are added so it should pass.
```

---

## T10 — `MenuState_t` retrofit remaining modules

**Problem.** Several UI modules still use bespoke `cursor`/`scroll`/`tab`/
`return_mode` static variables rather than the `MenuState_t` struct introduced
in the INTERFACE_REFACTOR_PLAN. The already-retrofitted `ui_vars.c` is the
reference. `ui_stat.c`, `ui_math_menu.c`, `ui_matrix.c`, `ui_yvars.c`, and
`ui_draw.c` all have TODO migration notes but have not been updated.

**Goal.** Migrate each remaining module's navigation statics to `MenuState_t`
and replace manual UP/DOWN/tab logic with the `MenuState_Move*` helpers. Do
one module per session.

This task resolves the `[refactor] MenuState_t retrofit — remaining menus`
item in CLAUDE.md.

---

### Session prompt

```
Project: STM32F429-TI81-Calculator (C, FreeRTOS)
Task: Migrate one UI module's bespoke navigation statics to MenuState_t.
Pick one of: ui_stat.c, ui_math_menu.c, ui_matrix.c, ui_yvars.c, ui_draw.c.
(Do only one per session; run the full checklist before moving to the next.)

Context
-------
MenuState_t is defined in App/Inc/menu_state.h with fields:
  CalcMode_t return_mode, uint8_t tab, uint8_t item_cursor, uint8_t scroll.
Helper functions in App/Src/menu_state.c:
  MenuState_MoveUp, MenuState_MoveDown, MenuState_PrevTab, MenuState_NextTab,
  MenuState_DigitToIndex.

The already-retrofitted reference module is App/Src/ui_vars.c — read it
first to understand the expected pattern.

Goal
----
For the module you choose:
1. Find its bespoke cursor/scroll/tab/return_mode static variables.
2. Replace them with a single `static MenuState_t s_menu_state = {0};`
3. Replace manual UP/DOWN/tab/digit navigation logic with MenuState_Move*
   helpers.
4. Confirm the module's MenuState_t is used in menu_open/menu_close in
   calculator_core.c (or after T6, in the module's own Open/Close functions).
5. Run `ctest --test-dir build-tests -V` — the test_menu_state suite must pass.
6. Check if the module has a host test (App/Tests/). If navigation logic
   changed meaningfully, add or update tests.

Reference files
---------------
App/Inc/menu_state.h     — MenuState_t definition and helper declarations
App/Src/menu_state.c     — helper implementations
App/Src/ui_vars.c        — completed reference implementation
App/Tests/test_menu_state.c — existing navigation tests

Constraints
-----------
- Do one module per session. Do not batch multiple modules into one commit.
- After each module: run the full Standard Post-Task Steps from
  COUPLING_REFACTOR.md before starting the next module.
- Run `ctest --test-dir build-tests -V` and `cmake --build build/Debug -j4`.
```

---

## When all tasks are complete

1. Add a **Milestone Reviews** entry to `docs/PROJECT_HISTORY.md`:
   - Date, reviewer, dimensions changed (API/header design should reach A+,
     Code organisation should improve), summary of work done.

2. Update the **Quality Scorecard** in `CLAUDE.md` and add rows to the
   Scorecard Change Log for any dimensions that changed.

3. Update `docs/ARCHITECTURE.md` module dependency diagram to reflect the new
   clean boundaries.

4. Remove `COUPLING_REFACTOR.md` from the repo root and note its deletion in
   `docs/PROJECT_HISTORY.md`.
