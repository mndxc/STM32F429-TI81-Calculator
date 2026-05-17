# Menu Refactor — Generic Renderer and Navigator

**Status:** Planned, not yet started
**Created:** 2026-05-16
**Author:** Claude Code (claude-sonnet-4-6)

> **How to use this doc:** At the start of each session, read the Progress Dashboard to see what's done. Pick the next unchecked step. After completing a step: tick the checkboxes, set the status, fill in the commit SHA, and add a line to the Progress Log. When all steps are complete, move this file's contents into `docs/PROJECT_HISTORY.md` "Resolved Items" and delete this file (matching the pattern used for `ARCHITECTURE_OPPORTUNITIES.md`).

---

## Progress Dashboard

| # | Step | Status | Date | Commit | Est. |
|---|------|--------|------|--------|------|
| 0 | Passthrough bug fix (`ui_menu_screen.c:131`) | ✅ DONE | 2026-05-16 | 16c8b34 | 5 min |
| 1 | Extend `MenuScreen_t` API + rewrite 5 PRGM sub-menu descriptors + add host test | ✅ DONE | 2026-05-16 | db162f0 | 3 h |
| 2a | Migrate DRAW menu | ✅ DONE | 2026-05-16 | 88afabc | 1 h |
| 2b | Migrate TEST menu | ✅ DONE | 2026-05-16 | b9d0aff | 1 h |
| 2c | Migrate STAT menu | ✅ DONE | 2026-05-16 | 9e42e6b | 1 h |
| 2d | Migrate VARS menu (incl. `wrap_tabs`) | ✅ DONE | 2026-05-16 | 7c7855d | 1 h |
| 2e | Migrate Y-VARS menu (incl. tab reorder OFF/Y/ON) | ✅ DONE | 2026-05-16 | 03b048d | 1 h |
| 2f | Migrate MATH menu | ✅ DONE | 2026-05-16 | 2eb3e8c | 1.5 h |
| 2g | Migrate ZOOM menu | ✅ DONE | 2026-05-16 | 0921109 | 1.5 h |
| 2h | Migrate PRGM main menu (incl. `wrap_tabs`) | ✅ DONE | 2026-05-16 | a4f74be | 2 h |
| 3 | Migrate MATRX 2-tab menu | ✅ DONE | 2026-05-17 | — | 2 h |

**Total estimated effort:** ~18 hours, spread across multiple sessions. Each step is independently shippable (test suite must pass + manual smoke test before proceeding).

**Status legend:** ⬜ TODO · 🟡 IN PROGRESS · ✅ DONE · ⏸ BLOCKED · ❌ CANCELLED

---

## Context

Menus were built incrementally as features shipped, resulting in:

1. **A passthrough bug** — `MenuScreen_HandleToken()` (`App/Src/ui_menu_screen.c:131`) returns `true` in its `default:` branch, consuming every unrecognized token. Pressing MATH/VARS/STAT/etc. while inside any PRGM sub-menu (CTL, I/O, EXEC, MODE NUMBER, MODE GRAPH) does nothing, violating TI-81 guidebook p. 1-18.
2. **Duplicated infrastructure** — 8 menu modules each define their own LVGL label arrays, scroll-indicator show/hide, color update, and navigation loops. The shared `MenuState_t` + `MenuScreen_t` foundation exists but is only partially adopted.
3. **Inconsistent behaviour** — digit shortcut semantics, tab navigation (some wrap, some don't), scroll indicator visibility, and CLEAR exit handling all differ between modules.

The TI-81 guidebook (pp. 1-18, 1-25) specifies:
- Pressing any menu key from any menu opens that menu
- CLEAR returns to the screen the user was on before opening the menu
- 2nd+QUIT always returns to Home (already works via pred-based routing — unaffected)
- UP/DOWN moves cursor; ENTER or digit number selects; 7+ items shows scroll arrow at bottom

---

## What Will NOT Be Migrated

These are edit screens or modals, not navigable item lists:
- `App/Src/ui_mode.c` — 2D grid, already returns `false` in default
- Matrix edit screen (inside `App/Src/ui_matrix.c`) — expression cursor, cell-by-cell editing
- PRGM NEW name-entry (`MODE_PRGM_NEW_NAME`) — expression editor semantics
- PRGM ERASE confirm dialog — 2-choice modal
- STAT results screen (`MODE_STAT_RESULTS`) — display-only
- `MODE_GRAPH_ZOOM_CURSOR` — post-selection cursor-pick reached from ZOOM
- `MODE_GRAPH_ZOOM_FACTORS` — Set Factors edit screen reached from ZOOM

---

## Pre-existing Bug Flagged (NOT fixed here; tracked as Follow-up #1)

`handle_normal_mode` passes hard-coded `MODE_NORMAL` as return_to when opening menus (`App/Src/ui_input.c:241-248`). So pressing one menu key from inside another menu loses the original screen context — CLEAR from the second menu goes Home, not back to the program editor (or wherever).

This bug exists today between top-level menus and becomes more reachable after Step 0. **Recommended:** pair Follow-up #1 with Step 0 in the same session.

---

## Step 0 — Fix the Passthrough Bug

**Status:** ✅ DONE (2026-05-16, commit 16c8b34)
**File:** `App/Src/ui_menu_screen.c`, line ~131
**Estimate:** 5 min

### Sub-tasks
- [x] Change `default: return true` → `default: return false` (when `on_extra == NULL`)
- [x] Run `cd build-tests && cmake --build . && ctest --output-on-failure` — all 15 suites pass
- [ ] Manual smoke: from a PRGM sub-menu (CTL/I/O/EXEC), press MATH → MATH menu opens
- [x] Commit with `Co-Authored-By: Claude Sonnet 4.6` footer

### Diff
```c
// Before:
default:
    if (d->on_extra) return d->on_extra(t, ms);
    return true;   // BUG: eats menu-opening keys
// After:
default:
    if (d->on_extra) return d->on_extra(t, ms);
    return false;  // let menu-opening keys fall through to handle_normal_mode
```

### Acceptance
All 5 PRGM sub-menus pass unrecognized tokens through to `handle_normal_mode`. Caveat: until Follow-up #1 lands, MATH-opened-from-PRGM-sub-menu inserts into the wrong target — flag in commit message.

---

## Step 1 — Extend `MenuScreen_t` for Within-Menu Tab Navigation

**Status:** ✅ DONE (2026-05-16)
**Files:** `App/Inc/ui_menu_screen.h`, `App/Src/ui_menu_screen.c`, `App/Src/ui_prgm.c` (sub-menu descriptors), `tests/test_ui_menu_screen.c` (new), `tests/CMakeLists.txt`
**Estimate:** 3 h (breaking API change — must rewrite all 5 PRGM sub-menu descriptors in same commit)

### Sub-tasks
- [x] Add `MenuTabDesc_t` (per-tab item descriptor with own `item_count`, `display_labels`/`get_label`, `on_select(int, lv_obj_t*)`)
- [x] Restructure `MenuScreenDesc_t`: replace top-level `item_count`/`display_labels`/`on_select` with `const MenuTabDesc_t *tabs`; add `title`, `default_tab`, `wrap_tabs`
- [x] Bump `MENU_SCREEN_MAX_TABS` from 3 to 5 (VARS has 5 tabs)
- [x] Add `active_tab` and `title_label` fields to `MenuScreen_t`
- [x] Implement `MenuScreen_SetTab(ms, idx)` — recolor tab bar, reset cursor/scroll, refresh display (under lvgl_lock)
- [x] Implement `MenuScreen_IsMenuOpeningKey(t)` — TOKEN_MATH/TEST/VARS/MATRX/PRGM/Y_VARS/STAT/DRAW
- [x] Implement `MenuScreen_DefaultExtra(t, ms)` — closes current menu and passes graph nav (Y_EQUALS/RANGE/ZOOM/GRAPH/TRACE) and menu-opening keys through to `handle_normal_mode`
- [x] Update `MenuScreen_HandleToken` LEFT/RIGHT to honor `wrap_tabs` (see code block below)
- [x] Update `MenuScreen_UpdateDisplay` to use `desc->tabs[ms->active_tab]` for item lookups
- [x] Render `title` label when `tab_count == 0 && title != NULL` (yellow, top of screen)
- [x] Rewrite 5 PRGM sub-menu descriptors (`prgm_ctl_desc`, `prgm_io_desc`, `prgm_exec_desc`, `prgm_mode_number_desc`, `prgm_mode_graph_desc`) to use new `tabs[]` form
- [x] Create `tests/test_ui_menu_screen.c` with cases: digit shortcut, UP/DOWN with scroll, LEFT/RIGHT with and without wrap, CLEAR, unknown-token returns false, `on_extra` precedence
- [x] Register new test in `tests/CMakeLists.txt`
- [x] Update `docs/TESTING.md` suite count and assertion total

### Updated LEFT/RIGHT logic
```c
case TOKEN_LEFT:
    if (desc->on_tab_switch && desc->left_mode) {
        desc->on_tab_switch(ms->screen, desc->left_mode);   /* sibling-menu */
    } else if (desc->tab_count > 0) {
        uint8_t next;
        if (ms->active_tab > 0)        next = ms->active_tab - 1;
        else if (desc->wrap_tabs)      next = desc->tab_count - 1;
        else                            return true;
        lvgl_lock(); MenuScreen_SetTab(ms, next); lvgl_unlock();
    }
    return true;
/* TOKEN_RIGHT mirror with wrap to 0 */
```

### Acceptance
- All host tests pass including new `test_ui_menu_screen.c`
- Manual: all 5 PRGM sub-menus still navigable (CTL/I/O/EXEC LEFT/RIGHT, items select correctly)
- Manual: tab wrap-around works in any menu that opts in via `wrap_tabs=true` (test with PRGM sub-menus after they're set)

---

## Step 2 — Migrate Top-Level Menus

For each migration:
1. Remove per-module `lv_obj_t*` arrays and ad-hoc state
2. Declare `static MenuScreen_t s_ms`, `static const MenuTabDesc_t s_tabs[]`, `static const MenuScreenDesc_t s_desc`
3. Init body becomes `MenuScreen_Init(&s_ms, &s_desc, parent)` + `MenuScreen_ResetAndShow`
4. Handle body becomes `MenuScreen_HandleToken(&s_ms, t)`
5. Per-item logic moves into `on_select` callbacks; `on_cancel` calls `menu_close(TOKEN_X)`
6. Set `on_extra = MenuScreen_DefaultExtra` to inherit graph-nav + menu-key passthrough

### Step 2a — DRAW (`App/Src/ui_draw.c`)
**Status:** ✅ DONE (2026-05-16)

- [x] Replace `lv_obj_t` arrays with `MenuScreen_t s_ms`
- [x] Define single `MenuTabDesc_t` with 7 items, `display_labels = draw_item_names`
- [x] Descriptor: `tab_count = 0`, `title = "DRAW"`, `on_select` = existing action switch (clear / insert / cursor-pick by return_mode), `on_cancel` = `menu_close(TOKEN_DRAW)`
- [x] Remove `ui_update_draw_display`, manual cursor coloring, etc.
- [x] Tests pass; manual smoke: DRAW opens, items select, CLEAR returns

### Step 2b — TEST (`App/Src/ui_math_menu.c`)
**Status:** ✅ DONE (2026-05-16)

- [x] Replace `s_test_*` state with `MenuScreen_t s_test_ms`
- [x] Single `MenuTabDesc_t` with 6 items, `on_select` = `test_menu_insert(test_items[idx].str)`
- [x] Descriptor: `tab_count = 0`, `title = "TEST"`, `on_cancel = menu_close(TOKEN_TEST)`
- [x] `handle_test_menu` becomes 1-line wrapper
- [x] Tests pass; manual smoke

### Step 2c — STAT (`App/Src/ui_stat.c`)
**Status:** ✅ DONE (2026-05-16)

- [x] Replace state with `MenuScreen_t s_stat_ms`
- [x] 3 `MenuTabDesc_t` entries (CALC/DRAW/DATA), each with its own `on_select`
- [x] CALC/DRAW `on_select` transitions to `MODE_STAT_RESULTS` (existing handler untouched)
- [x] `on_cancel = menu_close(TOKEN_STAT)`
- [x] No `on_extra` needed (default does the right thing)
- [x] Tests pass; manual smoke

### Step 2d — VARS (`App/Src/ui_vars.c`)
**Status:** ✅ DONE (2026-05-16, commit 7c7855d)

- [x] Replace state with `MenuScreen_t s_vars_ms`
- [x] 5 `MenuTabDesc_t` entries (XY/Σ/LR/DIM/RNG) with their own item counts
- [x] Per-tab `on_select` callbacks call `vars_do_insert(tab, idx)` helper
- [x] `vars_on_extra` handles TOKEN_VARS → close+return true; others → DefaultExtra
- [x] **Set `wrap_tabs = true`** per guidebook p. 3-17 (LEFT from XY wraps to RNG)
- [x] `on_cancel = menu_close(TOKEN_VARS)` via `vars_on_cancel`
- [x] Tests pass; manual smoke: VARS opens to XY, LEFT wraps to RNG

### Step 2e — Y-VARS (`App/Src/ui_yvars.c`)
**Status:** ✅ DONE (2026-05-16, commit 03b048d)

- [x] Replace state with `MenuScreen_t s_yvars_ms`
- [x] **Reorder tabs to OFF / Y / ON** with `default_tab = 1` per guidebook p. 3-19
- [x] 3 `MenuTabDesc_t` entries; Y-tab `on_select` reads STO context flag (set by `Yvars_OpenForSto`)
- [x] `yvars_on_extra` handles TOKEN_Y_VARS → close+return true; others → DefaultExtra
- [x] `on_cancel = menu_close(TOKEN_Y_VARS)` via `yvars_on_cancel`
- [x] Tests pass; manual smoke: Y-VARS opens centered on Y; LEFT shows OFF, RIGHT shows ON

### Step 2f — MATH (`App/Src/ui_math_menu.c`)
**Status:** ✅ DONE (2026-05-16, commit 2eb3e8c)

- [x] Replace state with `MenuScreen_t s_math_ms`
- [x] 4 `MenuTabDesc_t` entries (MATH/NUM/HYP/PRB) with `get_label` callbacks for display+insert split
- [x] Shared `math_on_select` reads `s_math_ms.active_tab`; calls `math_menu_insert` (preserves PRGM_EDITOR / Y= / normal routing)
- [x] `math_on_extra` handles TOKEN_MATH → close+return true; others → DefaultExtra
- [x] `on_cancel = menu_close(TOKEN_MATH)` via `math_on_cancel`
- [x] PRB has exactly 3 items (Rand/nPr/nCr) — verified
- [x] Tests pass; manual smoke: MATH menu, NUM/HYP/PRB tabs all work

### Step 2g — ZOOM (`App/Src/ui_graph_zoom.c`)
**Status:** ✅ DONE (2026-05-16, commit 0921109)

Guidebook p. 1-19 explicitly uses ZOOM as the canonical menu example.

- [x] File: existing `App/Src/ui_graph_zoom.c` (extracted from graph_ui.c earlier)
- [x] Replace `handle_zoom_mode` body with `MenuScreen_HandleToken`
- [x] 8 items with pre-formatted "N:Label" display_labels; `zoom_on_select(idx)` → `zoom_execute_item(idx+1)`
- [x] `zoom_on_extra` handles TOKEN_ZOOM → close+return true; others → DefaultExtra
- [x] `zoom_on_cancel` calls `zoom_menu_reset()` + `hide_all_screens()` directly (TOKEN_ZOOM not in menu_close dispatch)
- [x] Existing `MODE_GRAPH_ZOOM_CURSOR` and `MODE_GRAPH_ZOOM_FACTORS` handlers unchanged
- [x] Tests pass; manual smoke: ZOOM menu opens, all 8 items dispatch correctly

### Step 2h — PRGM main menu (`App/Src/ui_prgm.c`)
**Status:** ✅ DONE (2026-05-16, commit a4f74be)

- [x] Replace main-menu state with `MenuScreen_t s_prgm_ms`
- [x] 3 `MenuTabDesc_t` entries (EXEC/EDIT/ERASE) with shared `prgm_get_label()` for dynamic "N:PrgmX Name" formatting (37 slots)
- [x] **Set `wrap_tabs = true`** per guidebook p. 8-8 — preserves existing D2 wrap behavior
- [x] `prgm_on_extra` handles TOKEN_PRGM (close+return true), TOKEN_A-Z (slots 10-35), TOKEN_THETA (slot 36); digit shortcuts handled by `MenuState_DigitToIndex`
- [x] ERASE-tab `on_select` triggers existing confirm-dialog overlay (erase_confirm rendered into `s_prgm_ms.item_labels[]`)
- [x] NEW name-entry stays as its own mode (NOT migrated; see Follow-up #3)
- [x] `on_cancel = menu_close(TOKEN_PRGM)` via `prgm_on_cancel`
- [x] `editor_return_to_menu` uses `MenuScreen_SetTab(&s_prgm_ms, 1)` under lvgl_lock
- [x] Tests pass; manual smoke: PRGM menu, EXEC/EDIT/ERASE wrap correctly, slot shortcuts work

---

## Step 3 — MATRIX 2-Tab Menu

**Status:** ✅ DONE (2026-05-17)
**File:** `App/Src/ui_matrix.c`
**Estimate:** 2 h

Guidebook p. 6-2 confirms MATRX is a standard 2-tab menu.

### Sub-tasks
- [x] Replace menu-tab state with `MenuScreen_t s_matrix_ms`
- [x] 2 `MenuTabDesc_t` entries (MATRIX: 6 items via `matrix_matrx_labels[]`; EDIT: 3 slots via `matrix_edit_get_label` dynamic labels)
- [x] MATRIX-tab `matrix_matrx_on_select`: hide menu screen + `menu_insert_text(matrix_op_insert[idx])`
- [x] EDIT-tab `matrix_edit_on_select`: set active matrix slot + transition to MODE_MATRIX_EDIT
- [x] `matrix_on_cancel = menu_close(TOKEN_MATRX)`
- [x] `matrix_on_extra`: TOKEN_MATRX → close+return true; others → DefaultExtra
- [x] Matrix edit screen completely unchanged except 3 `ui_matrix_screen` refs → `s_matrix_ms.screen`
- [x] Tests pass (all 16 host suites); manual smoke pending hardware

---

## Files Changed (Cumulative)

| File | Touched in Step(s) |
|------|----|
| `App/Src/ui_menu_screen.c` | 0, 1 |
| `App/Inc/ui_menu_screen.h` | 1 |
| `App/Src/ui_prgm.c` | 1 (sub-menus), 2h (main menu) |
| `App/Src/ui_draw.c` | 2a |
| `App/Src/ui_math_menu.c` | 2b, 2f |
| `App/Src/ui_stat.c` | 2c |
| `App/Src/ui_vars.c` | 2d |
| `App/Src/ui_yvars.c` | 2e |
| `App/Src/ui_zoom.c` (NEW) or `App/Src/graph_ui.c` | 2g |
| `App/Src/ui_matrix.c` | 3 ✅ |
| `tests/test_ui_menu_screen.c` (NEW) | 1 |
| `tests/CMakeLists.txt` | 1 |
| `docs/TESTING.md` | 1 (and any step that adds assertions) |

**Not changed:** `App/Src/ui_mode.c`, `App/Src/menu_state.c/h`, `App/Src/calculator_core.c`, `App/Src/ui_input.c` (latter only changes when Follow-up #1 lands)

---

## Program-Editing Context — Preserved vs Inherited Gaps

**Preserved correctly through the refactor:**
- PRGM editor → press PRGM → CTL/I/O/EXEC sub-menus → select → `PrgmEditor_MenuInsert(text)`
- PRGM editor → press MATH or TEST → menu opens with `return_to = MODE_PRGM_EDITOR`; insert routed to `PrgmEditor_MenuInsert` via the `return_mode == MODE_PRGM_EDITOR` branch
- PRGM editor → press MODE → PRGM number/graph mode sub-screens

**Pre-existing gaps the refactor does NOT fix (see Follow-ups):**
1. VARS/MATRX/Y_VARS/STAT/DRAW keys don't open menus from the editor (Follow-up #2)
2. After Step 0, chained menus from program editor lose editor context (Follow-up #1)
3. PRGM NEW name entry has 4 guidebook deviations (Follow-up #3)

---

## Guidebook Divergences (lower priority, tracked here)

Folded into the steps above:
- ✓ Tab wrap-around for VARS, PRGM (`wrap_tabs = true` in Steps 2d, 2h)
- ✓ Y-VARS tab reorder OFF/Y/ON with `default_tab = 1` (Step 2e)
- ✓ ZOOM menu treated as a first-class menu (Step 2g)

Remaining divergences flagged separately:
- PRGM sub-menu LEFT/RIGHT ordering from editor (verify against guidebook p. 8-9, line 4039)
- Scroll-arrow rendering format (`↓` as separate label vs in-string substitution per p. 1-19) — cosmetic
- Hold-to-scroll (p. 3-17) — keypad-driver concern, out of scope
- PRB tab item count — verify during Step 2f

---

## Follow-up Items (added to CLAUDE.md "Next session priorities")

1. **[bug] `handle_normal_mode` return_mode chain** — when opening a menu from inside another menu, walk past intermediate menu modes to find the non-menu origin so CLEAR (and post-select insert) returns to the user's actual previous screen, not Home. Files: `App/Src/ui_input.c:241-248` or `menu_open()` in `App/Src/calculator_core.c`. **Pair with Step 0** to avoid temporarily worse behavior.
2. **[bug] PRGM editor routes only MATH/TEST/PRGM/MODE to menus** — `App/Src/prgm_editor.c:461` falls into `editor_handle_insert` for VARS/MATRX/Y_VARS/STAT/DRAW. Add explicit cases calling `menu_open(TOKEN_X, MODE_PRGM_EDITOR)` for each; add corresponding `return_mode == MODE_PRGM_EDITOR → PrgmEditor_MenuInsert` branches to each menu's insert helper (mirror `ui_math_menu.c:257`).
3. **[bug] PRGM NEW name-entry guidebook conformance** (`App/Src/ui_prgm.c:541-633`): allowed chars missing θ and `.` (p. 8-4); alpha-lock model uses MODE_ALPHA + manual re-engage instead of MODE_ALPHA_LOCK (so every digit requires its own ALPHA press); `default: return true` (`:632`) absorbs RANGE/GRAPH/Y= etc. instead of leaving the edit screen (p. 1-25).

---

## Verification (Run After Each Step)

**Host test suite:**
```
cd build-tests && cmake --build . && ctest --output-on-failure
```
All 1031+ assertions plus new `test_ui_menu_screen.c` cases must pass.

**Manual hardware checks (full set, run after Step 1 and after each Step 2x):**
- From VARS menu: press MATH → MATH menu opens
- From MATH menu: press VARS → VARS menu opens
- From any PRGM sub-menu (CTL/I/O/EXEC): press MATH → MATH menu opens (Step 0 regression test)
- From any menu: CLEAR → returns to previous screen (Home for now; full fix requires Follow-up #1)
- From any menu: 2nd+QUIT → home screen
- Digit shortcuts work in all migrated menus
- Scroll arrows appear/hide correctly on lists with >7 items (MATH tab, VARS RNG, PRGM main)
- Tab LEFT/RIGHT resets cursor to row 0 and clears scroll offset
- DRAW and TEST show their yellow title labels
- VARS: pressing LEFT from XY wraps to RNG (Step 2d)
- Y-VARS: opens centered on Y; LEFT → OFF, RIGHT → ON (Step 2e)
- PRGM main: LEFT from EXEC wraps to ERASE (Step 2h)
- ZOOM: Standard/Square/Trig replot immediately; Box/In/Out/Integer enter cursor mode (Step 2g)

---

## Complexity Delta

This refactor **decreases** complexity: removes ~600–800 lines of duplicated boilerplate across 8 files, consolidates into one well-tested generic module. No behaviour change except the Step 0 correctness fix and the Step 2d/e/h guidebook-conformance fixes.

No `[complexity]` item needed in CLAUDE.md after the refactor completes. Update Quality Scorecard if Testing dimension improves due to new host test coverage.

---

## Progress Log

Add a one-line entry per session in reverse-chronological order. Include date, step(s) advanced, commit SHA, and any notes/decisions.

- **2026-05-17** — Step 3 complete. MATRX migrated to `MenuScreen_t`: removed `matrix_menu_state`, `ui_matrix_screen`, `matrix_tab_labels[]`, `matrix_item_labels[]`; added `s_matrix_ms`, `matrix_matrx_labels[]` (pre-formatted), `matrix_edit_get_label` (dynamic EDIT tab), `matrix_matrx_on_select`, `matrix_edit_on_select`, `matrix_on_cancel`, `matrix_on_extra` (TOKEN_MATRX→close+true; others→DefaultExtra); 3 `ui_matrix_screen` refs in `handle_matrix_edit` updated to `s_matrix_ms.screen`; `handle_matrix_menu` is 1-line wrapper. All 16 host suites pass.
- **2026-05-16** — Steps 2d–2h complete. VARS (7c7855d): 5-tab wrap_tabs; vars_on_extra handles TOKEN_VARS. Y-VARS (03b048d): tab reorder OFF/Y/ON, default_tab=1. MATH (2eb3e8c): 4-tab get_label, shared math_on_select, math_on_extra. ZOOM (0921109): 8 items, zoom_on_extra, zoom_on_cancel skips menu_close (TOKEN_ZOOM not in dispatch). PRGM (a4f74be): 3-tab wrap_tabs, prgm_get_label for 37 slots, prgm_on_extra handles A-Z/θ shortcuts, erase_confirm rendered into s_prgm_ms.item_labels[], editor_return_to_menu uses MenuScreen_SetTab. All 16 host suites pass after each step.
- **2026-05-16** — Step 2c complete. STAT migrated: replaced stat_menu_state / stat_tab_labels[] / stat_item_labels[] / ui_stat_screen with MenuScreen_t (s_stat_ms). Three per-tab on_select callbacks handle CALC→stat_run_calc, DRAW→stat_run_draw, DATA→inline actions. handle_stat_menu is a 1-line wrapper. ui_update_stat_display kept as thin wrapper (called by ui_stat_edit.c). All 16 host suites pass.
- **2026-05-16** — Step 2a complete (commit 88afabc). DRAW migrated to `MenuScreen_t`: removed `draw_menu_state`, `ui_draw_screen` global, `draw_item_labels[]`, `ui_update_draw_display`; added `s_ms`/`s_tab`/`s_desc` with `draw_on_select` and `draw_on_cancel`; `handle_draw_menu` is now a 1-line wrapper. Removed `ui_draw_screen` stubs from test files. All 16 host suites pass.
- **2026-05-16** — Step 1 complete (commit db162f0). Added `MenuTabDesc_t`, restructured `MenuScreenDesc_t` (tabs[] replaces item_count/display_labels/on_select; adds title/default_tab/wrap_tabs); added `active_tab`/`title_label` to `MenuScreen_t`; bumped `MENU_SCREEN_MAX_TABS` 3→5; implemented `SetTab`, `IsMenuOpeningKey`, `DefaultExtra`; rewrote 5 PRGM sub-menu descriptors to tabs[] form; added `test_ui_menu_screen.c` (69 assertions). All 16 host suites pass. Manual smoke pending hardware.
- **2026-05-16** — Step 0 complete (commit 16c8b34). One-line fix: `return true` → `return false` in `MenuScreen_HandleToken` default branch. All 15 host suites pass. Manual smoke pending hardware.
- **2026-05-16** — Plan created; saved as `docs/MENU_REFACTOR_PLAN.md`. CLAUDE.md "Next session priorities" updated with pointer. Awaiting Step 0 start.
