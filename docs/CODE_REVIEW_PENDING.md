# Code Review — Action Items (Temporary)
> Delete this file when all items are checked off.
> Generated 2026-03-22. Reviewer: Claude Code (claude-sonnet-4-6).

---

## Quick wins (docs/config — no logic risk)

- [ ] **1. README.md Status section — reflect Session 20 PRGM completion** — The
  featured-news paragraph opens with "Y= Toggle is 100% complete" (Session 11 news).
  PRGM reaching ~95% in Sessions 19–20 (command reference, all CTL/I/O commands, `Menu(`
  overlay) is a bigger milestone and should be the lead. Rewrite the three status
  paragraphs to lead with PRGM reaching feature-complete (pending hardware validation),
  then MATRIX at ~95%, then note STAT is absent. File: `README.md` (Status section).

- [ ] **2. ARCHITECTURE.md Directory Map — expand App/Src listing** — The current
  Directory Map shows only `prgm_exec.c` as an example source, with `Src/` described
  generically. Add a line-by-line listing of all 10 `App/Src/*.c` files with one-line
  purpose descriptions, mirroring the CLAUDE.md architecture table. This makes the
  module map immediately useful for new contributors. File: `docs/ARCHITECTURE.md`
  (Directory Map section).

---

## Code refactoring (mechanical extractions — no logic changes)

- [ ] **3. Extract `handle_range_mode` + `handle_zoom_factors_mode` shared field-editor
  helper** — Both functions are ~165 lines and implement the identical pattern: map
  UP/DOWN to field selection, map digit/DEL to buffer editing, commit on ENTER/UP/DOWN,
  track cursor position. Extract a static helper
  `field_editor_handle(token, buf, len, cursor, fields, count) → bool` that encodes this
  logic once. Both handlers shrink to ~40 lines of setup + one call. No logic change.
  Files: `App/Src/graph_ui.c` (`handle_range_mode` lines ~1125–1295,
  `handle_zoom_factors_mode` lines ~1360–1521).

- [ ] **4. Split `handle_normal_mode` into focused sub-handlers** — At 131 lines it
  exceeds the 100-line guideline. The large switch over `Token_t` has three natural
  clusters: ENTER/CLEAR/ANS recall (evaluation flow), STO/operator/function insertion
  (expression building), and navigation (UP/DOWN history). Extract each cluster as a
  dedicated static function. `handle_normal_mode` reduces to ~30 lines of delegation.
  No logic change. File: `App/Src/calculator_core.c` (`handle_normal_mode`).

- [ ] **5. Split `handle_yeq_navigation` into cursor-move vs. row-switch helpers** — At
  129 lines it mixes three concerns: cursor LEFT/RIGHT byte stepping, UP/DOWN row
  switching, and MATH/TEST menu re-entry. Extract `yeq_cursor_move(dir)` (~30 lines)
  and `yeq_row_switch(dir)` (~25 lines) as static helpers. No logic change.
  File: `App/Src/graph_ui.c` (`handle_yeq_navigation`).

- [ ] **6. Replace `try_tokenize_identifier` character switch with a string dispatch
  table** — At 154 lines the function is a large `switch(first_char)` over every
  possible function name prefix (a–z, A–Z). Replace with a static `const char* name[]`
  / `Token_t tok[]` parallel array and a linear scan (or `bsearch` for alphabetical
  order). Function shrinks from 154 lines to ~40 lines. No behaviour change (same token
  outputs for same inputs). File: `App/Src/calc_engine.c:172–325`.

- [x] **7. Extract `prgm_execute_line` command handlers + dispatch table** — ✅ Resolved
  2026-03-22 (Session 22). 22 static `cmd_*` handlers extracted (3–50 lines each);
  `parse_incdec_args` shared helper; `CmdEntry_t` dispatch table; `prgm_execute_line`
  body ~30 lines. P19 resolved. 301/301 tests pass.

---

## Future P-items to add to QUALITY_TRACKER (not immediate)

- **Program execution host tests** — `prgm_execute_line` has zero unit tests despite
  implementing complex control flow (If/Then/Else/End, While, For, Goto/Lbl, call stack
  depth 4). Recommend ~80 tests covering: If/Else/End branching, While loop exit
  condition, For loop step and bounds, nested Goto/Lbl, subroutine call/return depth,
  IS>/DS< threshold behavior, Disp/Input/Prompt output. Requires item 7 above to be
  complete first (pure command handlers are testable without LVGL). File:
  `App/Tests/test_prgm_exec.c` (new).

- **UI token dispatch tests** — `handle_normal_mode` and the graph mode handlers in
  `graph_ui.c` have zero tests. If extracted as pure functions (token in → mode out,
  no LVGL side effects), ~50 tests could verify mode transitions, history navigation,
  and expression edit correctness. Requires items 4 and 5 above as prerequisites.

---

## Cross-reference verification (no action required — all clean)

- `app_common.h` CalcMode_t (21 values) — matches CLAUDE.md "Input Mode System" ✅
- `app_common.h` GraphState_t (11 fields) — matches CLAUDE.md "Graphing System → State" ✅
- Test counts 153/96/52/121 (422 total) — consistent across CLAUDE.md, GETTING_STARTED.md ✅
- ARCHITECTURE.md memory layout (~49% RAM) — consistent with CLAUDE.md (48.49%) ✅
- `prgm_exec.c` listed in ARCHITECTURE.md module diagram — present ✅
- All `docs/` files linked from README.md exist on disk ✅
- `App/Tests/test_persist_roundtrip.c` PersistBlock_t size assertion — should be
  verified against `App/Inc/persist.h` comment before next PersistBlock_t change ✅
