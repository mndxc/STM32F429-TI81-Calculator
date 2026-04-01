# Maintenance Standards

**Purpose:** Governs how to keep the codebase from accumulating complexity and documentation drift. Defines what to update after each change, which numbers must stay in sync, and what quality baselines to protect. All open work items live in `CLAUDE.md` "Next session priorities" — this file defines process only.

---

## Document Map

| File | What goes stale |
|---|---|
| `CLAUDE.md` | Feature %, priority list, `CalcMode_t` block, `GraphState_t` block, test counts, overall quality rating, scorecard dimension ratings |
| `docs/PROJECT_HISTORY.md` | Session log, completed features, resolved items, milestone reviews — update after every session |
| `README.md` | Feature status table (% complete, notes) |
| `docs/GETTING_STARTED.md` | Test counts, OpenOCD path version, toolchain PATH entries |
| `docs/TECHNICAL.md` | Supported functions table, Input Modes table, memory layout %, FLASH sector contents, font regeneration commands |
| `docs/ARCHITECTURE.md` | Module dependency diagram, file tree listing, memory layout % (mirrors TECHNICAL.md) |
| `docs/MENU_SPECS.md` | Menu layouts, navigation rules, implementation status — single source of truth for all menus |
| `docs/PRGM_COMMANDS.md` | PRGM command set; update when commands are added, removed, or renamed |
| `docs/prgm_manual_tests.md` | PRGM hardware test plan; update test IDs when command set changes |
| `docs/TESTING.md` | Host test executables and coverage targets |
| `docs/POWER_MANAGEMENT.md` | Power management design and Stop mode implementation notes |
| `docs/DISPLAY_STABILITY.md` | Display stability and pixel-clock artifact notes |
| `docs/TROUBLESHOOTING.md` | Troubleshooting steps for common build, flash, and runtime issues |
| `docs/PCB_DESIGN.md` | Custom PCB design notes (paused) |
| `App/Tests/test_persist_roundtrip.c` | Hardcoded `PersistBlock_t` size assertion — must match `persist.h` |
| This file | Scorecard grading criteria (Rises when / Falls when) — ratings live in `CLAUDE.md` |

---

## Quality Scorecard

Grading criteria only. Current ratings and the snapshot date live in `CLAUDE.md` "Quality Scorecard". When a dimension's rating changes: update `CLAUDE.md`, then add a Milestone Reviews entry to `docs/PROJECT_HISTORY.md`.

| Dimension | Rises when | Falls when |
|---|---|---|
| Documentation | All doc files stay in sync; new gotchas added promptly | Docs drift from code; session log falls behind |
| API / header design | Module boundaries clean; no new circular deps | Headers gain impl details; prefixes inconsistent |
| Memory safety & FLASH handling | New FLASH ops in `.RamFunc`; persist fields validated | Raw FLASH writes outside `persist.c`; alignment ignored |
| RTOS integration | All new LVGL calls inside mutex | LVGL call outside mutex; deadlock in timer callback |
| Error handling | New error paths return `CalcError_t` | Silent failures; out-of-bounds without check |
| Naming conventions | New names follow `Module_VerbNoun()` pattern | New inconsistent prefixes or abbreviations |
| Code organisation | Modules extracted cleanly; files stay under ~500 lines | Files grow past 500 lines without extraction plan |
| Function complexity | Functions stay under 100 lines | New over-100-line functions without follow-up plan |
| Magic numbers / constants | New colours/limits go to named constants in `ui_palette.h` | Inline hex literals or magic numbers in new code |
| Testing | New property tests; coverage increases | Test count drops; new logic with no host test |

---

## Do Not Regress

Standing rules that apply regardless of current rating. These define the floor — any new code that violates them is a defect, not a style preference.

**Documentation:** `docs/PROJECT_HISTORY.md` is the canonical session log and history archive. All doc files must stay in sync with code after every change.

**API and header design:** Module prefixes (`Calc_*`, `Graph_*`, `Persist_*`, `Keypad_*`) are consistent. No circular dependencies. Do not mix implementation details into public headers.

**Embedded-specific correctness:** FLASH erase routines in `.RamFunc`. `_Static_assert` on `PersistBlock_t` alignment. `volatile` on `g_sleeping`. ISR-safe queue use (`xQueueSendFromISR`). Checksums + magic + version on every persist block.

**RTOS integration:** Mutex guards all LVGL calls. `cursor_timer_cb` deadlock case documented and avoided. Never add LVGL calls outside `lvgl_lock()` / `lvgl_unlock()`.

**Error handling:** `CalcResult_t` carries `CalcError_t` + message + matrix flag. Bounds checking on token count, stack depth, and matrix dimensions. New error paths must return `CalcError_t`.

---

## Complexity Management

**The codebase must not grow in complexity faster than it is simplified.** Hard constraint, not a guideline.

Before closing a session or presenting a commit as complete, rate the complexity delta — one of:
- `neutral` — no net change in cognitive load (pure refactor, test addition, config change)
- `increase` — new logic, new state, new abstractions, or a file grew significantly
- `decrease` — logic removed, files split, dead code deleted, abstractions simplified

If `increase`: immediately add one or more `[complexity]` items to `CLAUDE.md` "Next session priorities" describing how to pay down the debt. Each item must name the file, function, and technique. Do not leave an `increase` commit without a follow-up plan.

### What counts as a complexity increase

- A file grows by more than ~100 lines without a corresponding extraction or removal elsewhere
- A new module is added without a clear, bounded responsibility
- New global or shared state is introduced
- A function exceeds ~80–100 lines
- A new conditional branch is added to an already-large switch or if-chain
- A workaround or special-case is added rather than fixing the underlying model

### What counts as paying down complexity

- Extracting a cohesive group of functions into a new module (following the `ui_matrix.c` / `expr_util.c` pattern)
- Replacing magic numbers or colours with named constants
- Reducing a large switch to a dispatch table or handler chain
- Deleting dead code or unused state
- Splitting a multi-responsibility function into focused helpers

---

## Update Rules

Rate complexity delta before every commit.

### After Every Commit

- **Session log** — add a bullet to `docs/PROJECT_HISTORY.md` "Session Log" section
- **Complexity delta** — rate neutral / increase / decrease; if increase, add a `[complexity]` item to `CLAUDE.md` "Next session priorities"
- **Completed item** — if the commit resolved a priority item, remove it from `CLAUDE.md` "Next session priorities" and update `Est. Done` % if applicable
- **Test count changed** — update `CLAUDE.md` "Host Tests" and `docs/GETTING_STARTED.md` "Host Tests" in the same commit; update the scorecard Testing row if the rating changed
- **RAM or FLASH size changed** — update `CLAUDE.md` "Memory regions" percentages

### After a Small Change (single file, < ~100 lines, no new public API)

- **Gotcha check** — does the change create or resolve a gotcha in `CLAUDE.md` "Common Gotchas"?
- **Feature status** — if the change advances a feature, update `Est. Done` % in `CLAUDE.md`
- **Bug fix test** — a bug fix without a new or updated test is incomplete

### After a Medium Change (multiple files, new logic, new API surface)

Triggers: new `CalcMode_t` value, new `GraphState_t` field, new math function, `PersistBlock_t` layout change, test count change, PRGM command added/removed.

- **New `CalcMode_t` value** — update `docs/TECHNICAL.md` "Input Modes" table in the same commit; must match `App/Inc/app_common.h` exactly
- **New `GraphState_t` field** — update `docs/TECHNICAL.md` "Graphing → State" struct to match `App/Inc/app_common.h`
- **`PersistBlock_t` layout change** — update the hardcoded size literal in `test_persist_roundtrip.c` (search `EXPECT_EQ((int)sizeof(PersistBlock_t)`) to match `persist.h`; run host tests before committing
- **New math function** — update `docs/TECHNICAL.md` Supported Functions table and `CLAUDE.md` Feature Completion Status
- **PRGM command set changed** — update `docs/PRGM_COMMANDS.md` to match the `prgm_exec.c` dispatch table
- **Gotcha created or resolved** — update `CLAUDE.md` "Common Gotchas" numbered list

### After a Large Change (new module, feature complete, major refactor)

Triggers: module extraction, feature shipped, spec alignment, doc restructure.

- **`docs/ARCHITECTURE.md`** — add/update the module in the Mermaid diagram and file tree
- **`CLAUDE.md` Architecture → File structure** — update `App/Src/` and `App/Inc/` tables
- **`docs/GETTING_STARTED.md`** — verify build commands, OpenOCD paths, and test section are accurate
- **`docs/TESTING.md`** — update if new test executables or coverage targets added
- **`README.md`** — update feature % if completion changed significantly (> ~5%)
- **`CLAUDE.md` Feature Completion Status** — update the `~72%` header if overall completion changed
- **`docs/PROJECT_HISTORY.md`** — add a session log entry and update Completed Features if applicable
- **Scorecard above** — review all 10 dimensions; update any that changed
- **Document Map above** — add any new doc file

**Verification:** Could a new contributor build, flash, and run tests following only the docs, with no other guidance? If no, update the relevant doc before closing.

### After a Quality Item is Resolved

- **`CLAUDE.md` "Next session priorities"** — remove the resolved item
- **`CLAUDE.md` "Project Quality"** — update overall rating % if it changed
- **Scorecard above** — update the affected dimension rating if it changed
- **`docs/PROJECT_HISTORY.md`** — add a row to Resolved Items; add a Milestone Reviews entry if a rating changed or a major milestone was reached

---

## Numbers to Keep in Sync

| Number | Canonical source | Also in |
|---|---|---|
| Test counts (per-executable) | `cmake --build build/tests` output | `CLAUDE.md` "Host Tests", `docs/GETTING_STARTED.md` |
| Overall test total | same | same |
| Feature completion % | `CLAUDE.md` Feature table | `README.md` Status table |
| Overall quality rating | `CLAUDE.md` "Project Quality" | — |
| Memory layout % used | `docs/TECHNICAL.md` | `docs/ARCHITECTURE.md` (mirror) |
| `PersistBlock_t` size (bytes) | `App/Inc/persist.h` comment | `test_persist_roundtrip.c`, `docs/TECHNICAL.md`, `CLAUDE.md` |
| `CalcMode_t` values | `App/Inc/app_common.h` | `docs/TECHNICAL.md` "Input Modes" |
| `GraphState_t` fields | `App/Inc/app_common.h` | `docs/TECHNICAL.md` "Graphing → State" |

---

## File Structure Maintenance

**When a file is added** (`App/Src/*.c`, `App/Inc/*.h`, or a new doc):
- Add to `CLAUDE.md` "Architecture → File structure" with a one-liner: `filename.c — <module responsibility>`
- Add to `docs/ARCHITECTURE.md` directory map with the same one-liner
- Confirm module prefix convention (`Calc_*`, `Graph_*`, etc.) if a new public API is introduced
- If a doc file, add it to the Document Map table above

**When a file is removed:**
- Remove from both locations above
- Search `CLAUDE.md`, `README.md`, `docs/GETTING_STARTED.md`, `docs/ARCHITECTURE.md` for stale references

**When a file's responsibility changes:**
- Update the one-liner in both locations above
- Update the Mermaid diagram in `docs/ARCHITECTURE.md` if dependency structure changed
- Add a session log entry in `CLAUDE.md` explaining the change
- Update the scope comment at the top of the file (the `calc_internal.h` scope comment pattern)

**When a test executable is added:**
- Update `App/Tests/CMakeLists.txt`
- Update `CLAUDE.md` "Host Tests" section and `docs/GETTING_STARTED.md` host tests section
- Update the scorecard Testing row if the rating changes

---

## Onboarding Document Lookup

| Change type | Document(s) to update |
|---|---|
| New peripheral or GPIO pin | `docs/GETTING_STARTED.md` hardware wiring table + `keypad.h` constant |
| New build step or toolchain version | `docs/GETTING_STARTED.md` build section + `CLAUDE.md` "Build & Flash" |
| New architectural boundary or module | `docs/ARCHITECTURE.md` diagram + `CLAUDE.md` "Architecture → File structure" |
| Expression pipeline stage changed | `docs/TECHNICAL.md` expression pipeline section |
| PRGM command added, removed, or changed | `docs/PRGM_COMMANDS.md` (canonical) |
| New gotcha or known issue | `CLAUDE.md` "Common Gotchas" numbered list |
| Memory region added or resized | `CLAUDE.md` "Critical Build Settings → SDRAM layout" + linker script + `docs/TECHNICAL.md` |
| New FLASH sector assigned | `docs/TECHNICAL.md` FLASH sector map + `CLAUDE.md` gotcha #15 |

---

## Staleness Hotspots

Check these first after any significant session — they are the highest-risk sync points.

1. **`PersistBlock_t` size assertion** — `test_persist_roundtrip.c` has a hardcoded byte count that must match `persist.h`. Any layout change silently breaks CI if not updated. The only hotspot that causes an active test failure, not just doc drift.

2. **`CalcMode_t` enum — 3-way sync** — `app_common.h` is canonical. Any new mode must be added to `CLAUDE.md` "Input Mode System" AND `TECHNICAL.md` "Input Modes" in the same commit.

3. **`GraphState_t` struct** — `app_common.h` is canonical. Any new field must be reflected in `CLAUDE.md` "Graphing System → State".

4. **Test counts** — drift every time a test is added. Always copy from `cmake --build build/tests` output; never estimate.

5. **Feature completion %** — `CLAUDE.md`'s feature table and `README.md`'s status table are independent text blocks describing the same truth.

6. **`CLAUDE.md` Architecture file list** — the `App/Src/` table is manually maintained; easy to forget when adding a new extracted module.

7. **Memory layout %** — in both `TECHNICAL.md` (canonical) and `ARCHITECTURE.md` (mirror); neither is computed automatically.

---

## Full Update Checklist

Run after any significant session as a completeness check.

### Step 1 — Run host tests first

```bash
cmake -S App/Tests -B build/tests && cmake --build build/tests
./build/tests/test_calc_engine
./build/tests/test_expr_util
./build/tests/test_persist_roundtrip
./build/tests/test_prgm_exec
```

All four must exit 0. Copy exact test counts from this output — never estimate. Fix any failure before proceeding.

### Step 2 — Identify what changed

```bash
git log --oneline $(git log --format="%H" -- CLAUDE.md | sed -n '2p')..HEAD
git diff HEAD~N HEAD --stat   # replace N with the commit count above
```

For each commit note: new `CalcMode_t` values? New `GraphState_t` fields? `PersistBlock_t` change? New `App/Src/` files? Test count change?

### Step 3 — Verify `CLAUDE.md` is current
- [ ] Feature Completion Status % reflects what shipped
- [ ] Feature table `Est. Done` values are correct
- [ ] All completed priority items removed from "Next session priorities"
- [ ] Complexity increases have a `[complexity]` follow-up item
- [ ] Architecture → File structure table lists every `App/Src/` file
- [ ] `CalcMode_t` block matches `App/Inc/app_common.h` exactly
- [ ] `GraphState_t` block matches `App/Inc/app_common.h` exactly
- [ ] "Host Tests" counts match Step 1 output

### Step 4 — Update `PROJECT_HISTORY.md` (single history source)
- [ ] Session Log — add a bullet for the session just completed
- [ ] Completed Features — add a row if a feature shipped
- [ ] Resolved Items — add a row if a significant quality item was resolved
- [ ] Milestone Reviews — add an entry if a rating changed or a major milestone was reached
- [ ] Scorecard above — update a dimension rating only if it changed

### Step 5 — Sync the public numbers
- [ ] Test counts match across `CLAUDE.md` and `GETTING_STARTED.md`
- [ ] Feature completion % matches across `CLAUDE.md` and `README.md`
- [ ] Memory layout % matches across `TECHNICAL.md` and `ARCHITECTURE.md`

### Step 6 — Verify cross-references
- [ ] Every file linked from `README.md` exists in `docs/`
- [ ] Every module listed in `ARCHITECTURE.md` exists in `App/Src/` or `App/Inc/`
- [ ] Every file path in `PROJECT_HISTORY.md` Resolved Items still exists

---

## Periodic Code Review

Use at natural milestones — after a major feature, before a new module, or when the codebase feels like it has grown faster than it has been simplified.

### Phase 1 — Structural scan (delegate to an Explore agent)

> For each `App/Src/*.c` file report: total line count, function names and approximate line counts, functions exceeding 80–100 lines, and obvious code smells (magic numbers, duplicated patterns, large switch statements). Also report: redundant headers, test coverage gaps, docs folder contents. Return a ranked hotspot table.

### Phase 2 — Direct reads of key files

1. `App/Inc/calc_internal.h` — duplicate constants, over-broad state exposure?
2. `App/Inc/app_common.h` — correct scope?
3. `docs/ARCHITECTURE.md` — does it match the current file tree?
4. `docs/GETTING_STARTED.md` — build commands, paths, and test counts still accurate?
5. `README.md` — does the status section reflect current feature state?
6. Top ~80 lines of the largest `App/Src/*.c` files — constant blocks, include lists, file-level comments.

### Phase 3 — Write items directly into `CLAUDE.md` "Next session priorities"

Do not create a separate file. Append each actionable item to `CLAUDE.md` "Next session priorities" using the standard tagged format:

```
**[tag] Title** — one sentence describing the issue and the fix. Zero logic change. Files: `...`.
```

Tag mapping for review items:
- `[docs]` — stale docstrings, file-tree listing gaps, cross-reference drift (no logic risk)
- `[refactor]` — function extraction, named constants, dispatch tables (mechanical, no logic change)
- `[testing]` — coverage gaps surfaced by the review
- `[bug]` — incorrect behaviour surfaced by the review

Order by effort: `[docs]` items first (minutes each), then `[refactor]` items by ascending line count.

Items not yet ready to act on (prerequisite incomplete, overlapping scope, high regression risk) — add them anyway with a **Prerequisite:** or **Coordinate with:** note inline so the context is not lost.

If the review surfaces a non-obvious structural insight for future sessions, save it as a project memory.

---

## Git Workflow

Stage specific files by name rather than `git add -A` to avoid accidentally committing build artefacts or sensitive files.

```bash
git add App/Src/calculator_core.c App/Src/app_init.c App/Inc/app_common.h \
        App/Src/graph.c App/Inc/graph.h \
        App/HW/Keypad/keypad.c App/HW/Keypad/keypad.h \
        Core/Inc/FreeRTOSConfig.h CLAUDE.md
git commit -m "description"
git push
```

Commit when a feature works end to end or before starting a risky change. Do not commit half-finished work that does not build.

### .gitignore key exclusions

```
build/
LVGL/
Middlewares/Third_Party/lvgl/tests/
Middlewares/Third_Party/lvgl/docs/
Middlewares/Third_Party/lvgl/examples/
Middlewares/Third_Party/lvgl/demos/
Drivers/STM32F4xx_HAL_Driver/
Drivers/CMSIS/
Middlewares/Third_Party/FreeRTOS/
Middlewares/ST/
```
