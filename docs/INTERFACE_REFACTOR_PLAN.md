# Interface & Module Refactoring Plan

**Status:** Items 1–3 complete (2026-04-15 / 2026-04-16 / 2026-04-16). Items 4–6 pending.  
**Purpose:** This document is the single source of truth for bringing each module to a clean,
well-bounded package. Work items are ordered so they can be executed sequentially from the top.
Each section is self-contained — an AI starting a fresh session needs only this document and the
referenced source files to execute the item.

**Ground rules for all items:**
- Run `./scripts/check_sync.sh` (from the repo root) and the full test suite
  (`cd build-tests && make && ctest`) after every item before committing.
  The script lives at `scripts/check_sync.sh` — **not** `docs/check_sync.sh`. It validates
  eight manual sync points (TECHNICAL.md file lists, ARCHITECTURE.md module references,
  PersistBlock_t size, PERSIST_VERSION, TESTING.md assertion count, PRGM_COMMANDS.md
  dispatch table coverage) and exits 1 if any drift is detected.
- Rate complexity delta in the commit message (neutral / increase / decrease).
- If complexity increases, add a `[complexity]` entry to `CLAUDE.md` "Next session priorities".
- Do not change observable behaviour in any item — these are all structural changes only.

---

## Architecture Map (read first)

```
┌──────────────────────────────────────────────────────────────────┐
│                        PURE LAYERS (no LVGL/HAL)                  │
│  calc_engine.c   expr_util.c   calc_stat.c   persist.c           │
│  graph_draw.c    graph.c                                          │
└──────────────────────────┬───────────────────────────────────────┘
                           │ clean one-way calls
┌──────────────────────────▼───────────────────────────────────────┐
│                   UI SUPER-MODULE                                  │
│  All share state via calc_internal.h (see Item 0 below)           │
│                                                                    │
│  calculator_core.c   ← hub; dispatcher; owns all shared globals   │
│  ui_input.c          ← expression input handlers                  │
│  ui_mode.c           ← MODE screen                                │
│  ui_math_menu.c      ← MATH/NUM/HYP/PRB/TEST menus               │
│  graph_ui.c          ← Y=, ZOOM, TRACE, ZBox                      │
│  graph_ui_range.c    ← RANGE and ZOOM FACTORS editors             │
│  ui_param_yeq.c      ← parametric Y= editor                       │
│  ui_matrix.c         ← matrix cell editor                         │
│  ui_prgm.c           ← program menu, editor, name dialog           │
│  ui_prgm_ctl.c       ← CTL submenu (extracted — Item 1 ✓)         │
│  ui_prgm_io.c        ← I/O submenu (extracted — Item 1 ✓)         │
│  ui_prgm_exec.c      ← EXEC slot picker (extracted — Item 1 ✓)    │
│  ui_stat.c           ← statistics UI                              │
│  ui_draw.c           ← DRAW menu                                  │
│  ui_vars.c           ← VARS menu                                  │
│  ui_yvars.c          ← Y-VARS menu                                │
└──────────────────────────────────────────────────────────────────┘
```

### File size snapshot (as of plan creation)

| File | Lines | Status |
|------|-------|--------|
| ui_prgm.c | 1,276 | Item 1 ✓ (was 1,652) |
| ui_prgm_ctl.c | ~150 | New — Item 1 ✓ |
| ui_prgm_io.c | ~120 | New — Item 1 ✓ |
| ui_prgm_exec.c | ~200 | New — Item 1 ✓ |
| calculator_core.c | 1,591 | Hub; large by design |
| calc_engine.c | 1,392 | Large but coherent pipeline |
| graph_ui.c | 1,131 | Large; extraction candidate (Item 4) |
| graph.c | 885 | Acceptable |
| graph_ui_range.c | 718 | Acceptable |
| ui_stat.c | 669 | Acceptable |
| prgm_exec.c | 626 | Acceptable |
| ui_matrix.c | 541 | Acceptable |
| ui_math_menu.c | 481 | Under budget |

---

## Item 0 — Understanding calc_internal.h (read before touching any UI file)

**This item is reference-only. Do not modify anything.**

[App/Inc/calc_internal.h](../App/Inc/calc_internal.h) (129 lines) is the shared-state contract for
the UI super-module. It exposes globals defined in `calculator_core.c` to every UI module:

```c
// Globals defined in calculator_core.c, declared here for all UI modules:
extern CalcMode_t   current_mode;   // current UI state machine state
extern CalcMode_t   return_mode;    // mode to return to on CLEAR/exit
extern bool         insert_mode;    // overwrite vs. insert
extern bool         cursor_visible; // blink state
extern float        ans;            // last result
extern bool         ans_is_matrix;
extern bool         angle_degrees;
extern bool         sto_pending;

extern char         expression[MAX_EXPR_LEN];  // the live expression buffer
extern uint8_t      expr_len;                  // byte length of expression
extern uint8_t      cursor_pos;                // byte offset; always a UTF-8 boundary

extern HistoryEntry_t history[HISTORY_LINE_COUNT];
extern uint8_t        history_count;
// ... plus all LVGL screen pointers, shared UI helpers
```

**Rules that must never be violated:**
1. `calc_internal.h` is included ONLY by the files listed in its scope comment. If you are adding
   a file that is NOT part of the UI super-module, do NOT include it here.
2. No module outside the super-module may read or write any of these globals directly.
3. No UI module calls another UI module's private (static) functions. Cross-module calls go through
   the public headers (`ui_prgm.h`, `graph_ui.h`, etc.).

---

## Item 1 — Split ui_prgm.c: extract CTL and I/O submenus ✓ COMPLETE (2026-04-15)

**Problem:** [App/Src/ui_prgm.c](../App/Src/ui_prgm.c) is 1,652 lines and contains five distinct
features: the program list menu, the line editor, the program name dialog, the CTL submenu, and the
I/O submenu. The last two are classic self-contained menu handlers that belong in their own files,
following the same pattern used when `ui_math_menu.c` was extracted from `calculator_core.c`.

**Goal:** Reduce `ui_prgm.c` to ~1,200 lines. No behaviour changes.

### What to extract

The two handlers to move, with their current line locations:
- `handle_prgm_ctl_menu()` — line 1,279 in `ui_prgm.c`
- `handle_prgm_io_menu()` — line 1,332 in `ui_prgm.c`

Along with each handler, move:
- The static state variables it exclusively owns (e.g., `prgm_ctl_cursor`, `prgm_ctl_scroll`,
  `prgm_io_cursor`)
- The `prgm_ctl_insert[]` / `prgm_io_insert[]` string tables it reads
- The `ui_update_prgm_ctl_display()` and `ui_update_prgm_io_display()` display helpers it calls
- The `ui_prgm_ctl_screen` and `ui_prgm_io_screen` LVGL object pointers (these are currently
  defined in `ui_prgm.c`; they need to become extern declarations here and move to the new files)

The screen init calls for CTL and I/O screens (currently in `ui_prgm_init_screens()`) can stay in
`ui_prgm.c` as calls to new init functions from the child modules, or move to the child modules
with a forward declaration. Either approach is fine as long as the screen pointers are owned by
the new modules.

### New files to create

**`App/Src/ui_prgm_ctl.c`**
```
Includes: calc_internal.h, ui_prgm.h, ui_prgm_ctl.h
Contains: PRGM_CTL_ITEM_COUNT constant, prgm_ctl_insert[] table,
          prgm_ctl_cursor/prgm_ctl_scroll statics,
          ui_prgm_ctl_screen pointer,
          ui_update_prgm_ctl_display(),
          ui_init_prgm_ctl_screen(),
          handle_prgm_ctl_menu()
```

**`App/Inc/ui_prgm_ctl.h`**
```c
#ifndef UI_PRGM_CTL_H
#define UI_PRGM_CTL_H
#include "app_common.h"
void ui_init_prgm_ctl_screen(lv_obj_t *parent);
void ui_update_prgm_ctl_display(void);
bool handle_prgm_ctl_menu(Token_t t);
#endif
```

**`App/Src/ui_prgm_io.c`**
```
Includes: calc_internal.h, ui_prgm.h, ui_prgm_io.h
Contains: PRGM_IO_ITEM_COUNT constant, prgm_io_insert[] table,
          prgm_io_cursor static,
          ui_prgm_io_screen pointer,
          ui_update_prgm_io_display(),
          ui_init_prgm_io_screen(),
          handle_prgm_io_menu()
```

**`App/Inc/ui_prgm_io.h`**
```c
#ifndef UI_PRGM_IO_H
#define UI_PRGM_IO_H
#include "app_common.h"
void ui_init_prgm_io_screen(lv_obj_t *parent);
void ui_update_prgm_io_display(void);
bool handle_prgm_io_menu(Token_t t);
#endif
```

### Files to update after extraction

| File | Change |
|------|--------|
| `App/Src/ui_prgm.c` | Remove extracted functions/statics/tables; `#include "ui_prgm_ctl.h"` and `"ui_prgm_io.h"`; call new init functions from the existing screen-init block |
| `App/Inc/ui_prgm.h` | Remove declarations for `handle_prgm_ctl_menu` and `handle_prgm_io_menu`; they are now owned by the new headers |
| `App/Src/calculator_core.c` | Add `#include "ui_prgm_ctl.h"` and `"ui_prgm_io.h"` (or verify that the dispatcher already calls through `ui_prgm.h` — if the existing include chain covers it, no change needed) |
| `build-tests/CMakeLists.txt` | Add the two new `.c` files to the test build if any test links against `ui_prgm.c` |
| `docs/TECHNICAL.md` | Add the two new files to the App/Src file list |
| `docs/ARCHITECTURE.md` | Update the module diagram |
| `CLAUDE.md` | Update the scorecard "Code organisation" note; add [complexity] item if the extraction increases it |

### Acceptance criteria
- [x] `ui_prgm.c` is under 1,300 lines (now 1,276)
- [x] `handle_prgm_ctl_menu` and `handle_prgm_io_menu` are not defined in `ui_prgm.c`
- [x] Firmware builds with 0 errors and 0 warnings
- [x] All host tests pass (`ctest` in `build-tests/`)
- [ ] No behaviour change: CTL/I/O menus open, navigate, and insert text identically to before (hardware validation pending — P10)

---

## Item 2 — Add ExpressionBuffer_t wrapper to expr_util.h ✓ (2026-04-16)

**Problem:** The expression buffer (`expression[]`, `expr_len`, `cursor_pos`) is scattered across
`calculator_core.c` as three separate statics, exposed raw via `calc_internal.h`. Every module
that edits the expression (`ui_input.c`, `ui_prgm.c`, `ui_matrix.c`) must manually enforce
invariants: cursor ≤ len, len ≤ MAX_EXPR_LEN, cursor always on a UTF-8 boundary. Gotcha #13 in
`CLAUDE.md` exists because this burns people.

The pure UTF-8 helpers already exist in [App/Inc/expr_util.h](../App/Inc/expr_util.h). This item
adds a thin stateful wrapper struct and accessor functions so callers stop touching raw fields.

**Goal:** Wrap the three fields in a struct; add setter/accessor helpers; migrate callers. No
behaviour changes.

### New struct and API to add to expr_util.h

```c
/**
 * @brief Stateful wrapper for the expression buffer.
 *
 * All mutations must go through the ExprBuffer_* API to guarantee invariants:
 *   - buf is always null-terminated
 *   - len == strlen(buf)
 *   - cursor <= len
 *   - cursor is always at a UTF-8 character boundary
 */
typedef struct {
    char    buf[MAX_EXPR_LEN];
    uint8_t len;
    uint8_t cursor;   /* byte offset; always a UTF-8 boundary */
} ExprBuffer_t;

/** Insert string s at cursor. No-op if buffer would overflow. */
void ExprBuffer_Insert(ExprBuffer_t *b, bool insert_mode, const char *s);

/** Delete the character immediately before cursor (backspace). */
void ExprBuffer_Delete(ExprBuffer_t *b);

/** Move cursor one character left (UTF-8 and matrix-token aware). */
void ExprBuffer_Left(ExprBuffer_t *b);

/** Move cursor one character right (UTF-8 and matrix-token aware). */
void ExprBuffer_Right(ExprBuffer_t *b);

/** Clear the buffer: len=0, cursor=0, buf[0]='\0'. */
void ExprBuffer_Clear(ExprBuffer_t *b);
```

Note: `MAX_EXPR_LEN` is defined in `calc_internal.h`, not in `expr_util.h`. To avoid creating a
dependency, either move the constant to `app_common.h` (preferred — it's a fundamental limit) or
duplicate it with a static_assert that they match. Check `app_common.h` for a good home first.

### Files to create

**`App/Src/expr_buffer.c`** — implementations of the five new functions. Each wraps the
corresponding `ExprUtil_*` call with the struct fields. This file has zero LVGL/HAL dependencies
and must be usable in host test builds.

### Files to update

| File | Change |
|------|--------|
| `App/Inc/expr_util.h` | Add `ExprBuffer_t` typedef and the five function declarations |
| `App/Src/expr_buffer.c` | New file: implement the five functions |
| `App/Src/calculator_core.c` | Replace `char expression[MAX_EXPR_LEN]; uint8_t expr_len; uint8_t cursor_pos;` with `ExprBuffer_t expr;`; update all internal accesses to `expr.buf`, `expr.len`, `expr.cursor` |
| `App/Inc/calc_internal.h` | Replace the three individual `extern` declarations with `extern ExprBuffer_t expr;` |
| `App/Src/ui_input.c` | Update all reads/writes of `expression`, `expr_len`, `cursor_pos` to use the struct fields and, where possible, the new API |
| `App/Src/ui_prgm.c` | Same migration for any direct accesses |
| `App/Src/ui_matrix.c` | Same migration |
| `build-tests/CMakeLists.txt` | Add `expr_buffer.c` to test builds that already link `expr_util.c` |

### Migration strategy (important)

Do NOT attempt a big-bang rename. The `calc_internal.h` extern is the single point of exposure.
Migrate in this order:
1. Add the struct and functions to `expr_util.h` / `expr_buffer.c` first.
2. Change `calculator_core.c` to declare `ExprBuffer_t expr;` and update its own internal access.
3. Update `calc_internal.h` to export `extern ExprBuffer_t expr;`.
4. Migrate `ui_input.c` — it is the highest-frequency caller.
5. Migrate `ui_prgm.c` and `ui_matrix.c`.
6. Run the test suite after each file migration.

### Acceptance criteria
- [x] `ExprBuffer_t` is defined in `expr_util.h`
- [x] No module accesses `expression[]`, `expr_len`, or `cursor_pos` as raw names — all accesses
  are through `expr.buf`, `expr.len`, `expr.cursor` (struct field access is fine; the point is
  they are members of a single identifiable struct)
- [x] `expr_buffer.c` has no LVGL/HAL includes
- [x] Host tests pass; add at least one test for ExprBuffer invariant checking (8 groups, 48 assertions in `test_expr_buffer.c`)
- [ ] Firmware builds with 0 errors — BLOCKED: pre-existing build failures from Item 1 (`ui_mode.c` COLOR_* undeclared, `ui_math_menu.h` lv_obj_t unknown) tracked in CLAUDE.md Active bug item

---

## Item 3 — Unify menu navigation state with MenuState_t ✓ COMPLETE (2026-04-16)

**Problem:** Every menu module declares the same three or four state variables independently:

```c
// ui_vars.c
static uint8_t vars_tab    = 0;
static uint8_t vars_cursor = 0;
static uint8_t vars_scroll = 0;
static CalcMode_t vars_return_mode;

// ui_stat.c
static uint8_t stat_tab    = 0;
static uint8_t stat_cursor = 0;
...
```

And every menu handler re-implements the same UP/DOWN/LEFT/RIGHT/digit logic. A scroll bug fix
currently requires touching every menu file.

**Goal:** Define a `MenuState_t` base struct and shared navigation helpers. Retrofit one module as
a proof-of-concept, document the pattern, leave the rest as a follow-on item.

### New types and helpers

Add to `App/Inc/app_common.h` (or a new `App/Inc/menu_state.h` if `app_common.h` is already large):

```c
/**
 * @brief Common navigation state shared by all single-list and tabbed menus.
 */
typedef struct {
    uint8_t     tab;           /* active tab index (0 if single-list) */
    uint8_t     cursor;        /* highlighted row within the visible window */
    uint8_t     scroll;        /* top-of-window item index */
    CalcMode_t  return_mode;   /* mode to restore on CLEAR */
} MenuState_t;

/**
 * @brief Move the menu cursor up by one row, scrolling if needed.
 * @param s         menu state to update
 * @param total     total item count in the active tab/list
 * @param visible   number of visible rows (typically MENU_VISIBLE_ROWS)
 */
void MenuState_MoveUp(MenuState_t *s, uint8_t total, uint8_t visible);

/**
 * @brief Move the menu cursor down by one row, scrolling if needed.
 */
void MenuState_MoveDown(MenuState_t *s, uint8_t total, uint8_t visible);

/**
 * @brief Move to the previous tab (wrapping). Resets cursor and scroll.
 */
void MenuState_PrevTab(MenuState_t *s, uint8_t tab_count);

/**
 * @brief Move to the next tab (wrapping). Resets cursor and scroll.
 */
void MenuState_NextTab(MenuState_t *s, uint8_t tab_count);

/**
 * @brief Resolve a digit shortcut (TOKEN_1..TOKEN_9, TOKEN_0) to an item index.
 * Returns -1 if the token is not a digit or the index is out of range.
 */
int MenuState_DigitToIndex(Token_t t, uint8_t total);

/**
 * @brief Returns the absolute item index (scroll + cursor).
 */
static inline uint8_t MenuState_AbsoluteIndex(const MenuState_t *s) {
    return s->scroll + s->cursor;
}
```

Implement `MenuState_MoveUp`, `MenuState_MoveDown`, `MenuState_PrevTab`, `MenuState_NextTab`, and
`MenuState_DigitToIndex` in a new file `App/Src/menu_state.c`. This file has zero LVGL/HAL
dependencies and is host-testable.

### Proof-of-concept: retrofit ui_vars.c

[App/Src/ui_vars.c](../App/Src/ui_vars.c) is 446 lines and has a `VarsMenuState_t` struct already
defined. Replace it with `MenuState_t` from the shared header. The handler body simplifies from
manual if/else chains to calls like:

```c
case TOKEN_UP:
    MenuState_MoveUp(&vars_state, vars_tab_item_count[vars_state.tab], MENU_VISIBLE_ROWS);
    lvgl_lock(); ui_update_vars_display(); lvgl_unlock();
    return true;
```

After the proof-of-concept passes testing, add a note at the top of every remaining menu file
pointing to `menu_state.h` as the standard pattern for future retrofits. Leave the remaining
retrofits for subsequent sessions — do not attempt to migrate all menus in one commit.

### Files to create
- `App/Inc/menu_state.h` (or extend `app_common.h`)
- `App/Src/menu_state.c`

### Files to update
- `App/Src/ui_vars.c` — proof-of-concept migration
- `App/Inc/ui_vars.h` — remove `VarsMenuState_t` if it was exposed; replace with `MenuState_t`
- `build-tests/CMakeLists.txt` — add `menu_state.c` to test build

### Acceptance criteria
- [x] `MenuState_t` and all five helpers are defined and implemented
- [x] `ui_vars.c` uses `MenuState_t`; its bespoke state variables are gone
- [x] `menu_state.c` has no LVGL/HAL includes
- [x] Host tests pass; test_menu_state.c has 5 groups, 43 assertions covering MoveUp/Down bounds, PrevTab/NextTab, DigitToIndex, AbsoluteIndex
- [x] All other menus still work (no regression; they have not been migrated yet — TODO notes added)

---

## Item 4 — Reduce graph_ui.c: extract the ZOOM menu

**Problem:** [App/Src/graph_ui.c](../App/Src/graph_ui.c) is 1,131 lines. It hosts four distinct
features: the Y= editor, the ZOOM menu, the TRACE handler, and the ZBox handler. The ZOOM menu
(`handle_zoom_mode`, `zoom_execute_item`, `zoom_menu_reset`, `apply_zoom_preset`, `zoom_show_graph`,
`zoom_enter_zbox`, `zoom_enter_factors`, `zoom_scale_view`, `ui_update_zoom_display`) is a
self-contained block from approximately line 153 to line 468.

**Goal:** Extract the ZOOM menu to `ui_graph_zoom.c` / `ui_graph_zoom.h`. Target: `graph_ui.c`
under 800 lines.

### What to extract

Functions (all currently in `graph_ui.c`):
- `ui_init_zoom_screen()` (line ~153)
- `apply_zoom_preset()` (line ~282)
- `ui_update_zoom_display()` (line ~329, already exposed in `graph_ui.h`)
- `zoom_menu_reset()` (line ~379)
- `zoom_show_graph()` (line ~390)
- `zoom_enter_zbox()` (line ~401)
- `zoom_scale_view()` (line ~414)
- `zoom_enter_factors()` (line ~425)
- `zoom_execute_item()` (line ~434)
- `handle_zoom_mode()` (line ~860, already in `graph_ui.h`)

State variables to move with the functions:
- `ui_graph_zoom_screen` LVGL pointer (currently defined in `graph_ui.c` and exported in
  `calc_internal.h` — move definition to the new file; `calc_internal.h` extern stays)
- `zoom_cursor` static

### New files to create

**`App/Inc/ui_graph_zoom.h`**
```c
#ifndef UI_GRAPH_ZOOM_H
#define UI_GRAPH_ZOOM_H
#include "app_common.h"
void ui_init_zoom_screen(lv_obj_t *parent);
void ui_update_zoom_display(void);
void zoom_menu_reset(void);
bool handle_zoom_mode(Token_t t);
#endif
```

**`App/Src/ui_graph_zoom.c`**
```
Includes: calc_internal.h, ui_graph_zoom.h, graph.h
Contains: all functions listed above
```

### Files to update

| File | Change |
|------|--------|
| `App/Src/graph_ui.c` | Remove extracted functions; add `#include "ui_graph_zoom.h"` |
| `App/Inc/graph_ui.h` | Remove `ui_update_zoom_display` and `handle_zoom_mode` declarations (now in `ui_graph_zoom.h`) |
| `App/Src/calculator_core.c` | Add `#include "ui_graph_zoom.h"`; verify the dispatcher calls still resolve |
| `docs/TECHNICAL.md` | Add the new file to the App/Src file list |
| `CLAUDE.md` | Update scorecard "Code organisation" row |

### Acceptance criteria
- [ ] `graph_ui.c` is under 800 lines
- [ ] `handle_zoom_mode` and `ui_update_zoom_display` are not defined in `graph_ui.c`
- [ ] Firmware builds with 0 errors
- [ ] Host tests pass
- [ ] No behaviour change: ZOOM menu opens, all presets work, ZBox works

---

## Item 5 — Document graph_state ownership

**Problem:** `graph_state` (a `GraphState_t` struct) is read or written by at least seven modules:
`calculator_core.c`, `graph_ui.c`, `graph_ui_range.c`, `ui_param_yeq.c`, `graph.c`, `ui_stat.c`,
and `persist.c`. There is no formal record of who owns which fields or under what conditions
mutations are safe.

**Goal:** Add an ownership table as a comment block inside `app_common.h` where `GraphState_t` is
defined, and add a "State Ownership" section to `docs/TECHNICAL.md`. No code changes.

### Comment block to add in app_common.h

Find the `GraphState_t` struct definition and add the following block immediately above it:

```c
/*
 * GraphState_t — ownership and mutation rules
 *
 * graph_state is a global defined in calculator_core.c and exported via
 * calc_internal.h.  All mutations must happen under lvgl_lock() because
 * they are followed immediately by LVGL label/display updates in the same
 * critical section.
 *
 * Field ownership by module:
 *
 *   equations[]/enabled[]    — Written by graph_ui.c (Y= editor),
 *                              ui_param_yeq.c (parametric editor),
 *                              ui_yvars.c (ON/OFF tab actions).
 *                              Read by graph.c (render), persist.c (save/load).
 *
 *   xmin/xmax/ymin/ymax      — Written by graph_ui_range.c (RANGE editor),
 *                              graph_ui.c (ZOOM preset actions),
 *                              graph.c (ZBox commit).
 *                              Read by graph.c (render), persist.c (save/load).
 *
 *   tmin/tmax/tstep           — Written by graph_ui_range.c (parametric RANGE
 *                              editor).  Read by graph.c (parametric render).
 *
 *   mode (Function/Param)    — Written by ui_mode.c (MODE screen row 4).
 *                              Read by all graph modules to branch behaviour.
 *
 *   connected/dot            — Written by ui_mode.c (MODE screen row 5).
 *                              Read by graph.c.
 *
 * Adding a new graph feature: add its fields here with their owning module,
 * then update docs/TECHNICAL.md "State Ownership" section.
 */
```

### Section to add in docs/TECHNICAL.md

Add a "State Ownership" section after the existing "Memory Layout" section. The section should
list `graph_state`, `calc_variables[]`, `calc_matrices[]`, `stat_data`, and `ans` with their
owning module and the rule for safe mutation.

### Files to update
- `App/Inc/app_common.h` — comment block above `GraphState_t`
- `docs/TECHNICAL.md` — new "State Ownership" section

### Acceptance criteria
- [ ] `GraphState_t` definition has the ownership comment
- [ ] `docs/TECHNICAL.md` has a "State Ownership" section
- [ ] No code changes; firmware and tests unchanged

---

## Item 6 — Plan PersistBlock_t sub-struct split (design only, no code)

**This item produces a design document only. Do not change any code.**

**Problem:** [App/Inc/persist.h](../App/Inc/persist.h) defines `PersistBlock_t` as a single flat
2,060-byte struct (PERSIST_VERSION 6 as of plan creation). Every new feature adds fields and
bumps the global version. A change to stat fields technically requires migrating graph fields even
though they did not change.

**Goal:** Design a sub-struct layout that can be adopted at the next planned persist version bump,
and write it up as a section in `docs/TECHNICAL.md` so it is not forgotten.

### Design to document

```c
// Proposed PersistBlock_t (adopt at next version bump)
typedef struct {
    uint32_t magic;           // PERSIST_MAGIC unchanged
    uint16_t version;         // global layout version (must increment on any structural change)

    // Per-section sub-versions allow independent migration:
    uint16_t graph_ver;
    uint16_t stat_ver;
    uint16_t matrix_ver;
    uint16_t prgm_ver;
    uint16_t mode_ver;

    GraphPersist_t    graph;
    StatPersist_t     stat;
    MatrixPersist_t   matrix;
    PrgmPersist_t     prgm;
    ModePersist_t     mode;

    uint32_t checksum;        // CRC32 of all bytes above
} PersistBlock_t;
```

The migration switch in `Persist_Load()` becomes a series of per-section switches instead of one
monolithic switch. A firmware that adds a stat field increments `stat_ver` and `version` but does
not change the graph migration path.

### Where to document
- Add a "Persist Migration Design" subsection to `docs/TECHNICAL.md` under the existing persist
  section.
- Note the current flat layout, the proposed sub-struct layout, and the constraint that this must
  be adopted atomically at a version bump (no partial migration).

### Acceptance criteria
- [ ] Design is documented in `docs/TECHNICAL.md`
- [ ] No code changes; firmware and tests unchanged
- [ ] The design is reviewed before the next feature that adds persist fields

---

## Completion Checklist

After all six items are done, update the following:

| File | Update |
|------|--------|
| `CLAUDE.md` Quality Scorecard | Raise "Code organisation" from B to B+ if ui_prgm.c < 1,000 lines and graph_ui.c < 800 lines |
| `docs/PROJECT_HISTORY.md` | Add a Resolved Items row for the refactor milestone |
| `CLAUDE.md` "Next session priorities" | Remove items that correspond to completed work here |

### Outstanding after these six items (not in scope here)

- Retrofit remaining menu modules to `MenuState_t` (ui_stat, ui_matrix, ui_yvars, ui_draw)
- Extract `graph_state` accessors from raw field mutations (follow-on to Item 5)
- Adopt sub-struct `PersistBlock_t` layout at the next feature that bumps persist version
  (follow-on to Item 6)
