# Maintenance Standards

**Purpose:** Governs how to keep the codebase from accumulating complexity and documentation drift. Defines what to update after each change, which numbers must stay in sync, and what quality baselines to protect. All open work items live in `CLAUDE.md` "Next session priorities" — this file defines process only.

---

## Document Map

| File | What goes stale |
|---|---|
| `CLAUDE.md` | Feature %, priority list, overall quality rating, scorecard dimension ratings |
| `docs/PROJECT_HISTORY.md` | Session log, completed features, resolved items, milestone reviews — update after every session |
| `README.md` | Feature status narrative (prose descriptions per area) |
| `docs/GETTING_STARTED.md` | OpenOCD path version, toolchain PATH entries (test counts: links to `docs/TESTING.md`) |
| `docs/TECHNICAL.md` | Supported functions table, Input Modes table, memory layout %, FLASH sector contents, font regeneration commands |
| `docs/ARCHITECTURE.md` | Module dependency diagram, file tree listing (memory layout: links to TECHNICAL.md — no numeric mirror) |
| `docs/MENU_SPECS.md` | Menu layouts, navigation rules, implementation status — single source of truth for all menus |
| `docs/PRGM_COMMANDS.md` | PRGM command set; update when commands are added, removed, or renamed |
| `docs/prgm_manual_tests.md` | PRGM hardware test plan; update test IDs when command set changes |
| `docs/TESTING.md` | Host test executables and coverage targets |
| `docs/POWER_MANAGEMENT.md` | Power management design and Stop mode implementation notes |
| `docs/DISPLAY_STABILITY.md` | Display stability and pixel-clock artifact notes |
| `docs/TROUBLESHOOTING.md` | Troubleshooting steps for common build, flash, and runtime issues |
| `docs/PCB_DESIGN.md` | Custom PCB design notes (paused) |
| `App/Tests/test_persist_roundtrip.c` | Hardcoded `PersistBlock_t` size assertion — must match `persist.h` |
| `scripts/check_sync.sh` | Hard-coded file paths and grep patterns — update when doc structure changes |
| `scripts/update_test_counts.sh` | Suite executable names — update when a new test executable is added |
| This file | Scorecard grading criteria (Rises when / Falls when) — ratings live in `CLAUDE.md` |

---

## Quality Scorecard

Grading criteria only. Current ratings and the snapshot date live in `CLAUDE.md` "Quality Scorecard". When a dimension's rating changes: update `CLAUDE.md`, then add a Milestone Reviews entry to `docs/PROJECT_HISTORY.md`.

**Scorecard Change Log** — `CLAUDE.md` must maintain a compact change log immediately after the scorecard table in the format below. This makes trends visible; a single snapshot date hides whether a B is stable or recently degraded from A-.

```markdown
### Scorecard Change Log
| Date | Dimension | Old | New | Trigger |
|---|---|---|---|---|
| 2026-01-01 | Testing | B+ | A | P1 property tests added |
```

When a rating changes: add a row here, update the table above, and add a Milestone Reviews entry to `docs/PROJECT_HISTORY.md`.

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

**Documentation:** `docs/PROJECT_HISTORY.md` is the canonical session log and history archive. All doc files must stay in sync with code; artifact-based updates happen per-commit, session log and number sync happen once per session at close (see Update Rules).

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

### Complexity Debt Ceiling

**Hard limit: 3 open `[complexity]` items at any time.**

Before starting any feature work (non-`[bug]`, non-`[docs]`), count the open `[complexity]` items in `CLAUDE.md` "Next session priorities". If the count is ≥ 3:

1. Report the count to the user and list the open items by name.
2. Do not begin the feature.
3. Propose resolving one `[complexity]` item first.

This limit applies regardless of how urgent the feature seems. `[hardware]` validation items and `[testing]` items do not count toward this limit.

---

## Breaking Change Protocol

A **breaking change** is any modification that could cause existing saved state (flash persist block) or stored user programs to misbehave after a firmware update — without any visible error to the user.

### Persist layout changes (`PersistBlock_t`)
1. Increment `PERSIST_VERSION` in `persist.h`
2. Add a migration case to the version-upgrade switch in `Persist_Load()`
3. Update the hardcoded size assertion in `test_persist_roundtrip.c` (search `EXPECT_EQ((int)sizeof`)
4. Document the migration in `docs/TECHNICAL.md` Persist Versions table

### `CalcMode_t` value removal
- Do not remove a mode value if it may be stored in `PersistBlock_t`
- If removal is required: add a migration in `Persist_Load()` to remap the old value to a valid replacement; remove the old value only in a subsequent session after the migration has shipped

### PRGM command removal or token change
- Any command token that may exist in stored user programs must be handled for at least one firmware version after removal — map to `Disp "REMOVED"` or a no-op, not a crash
- Update `docs/PRGM_COMMANDS.md` with a "Removed in vN" row rather than deleting the entry — the row is evidence of migration intent

### Public API changes (header-level)
- Renaming a public function used in test stubs (`App/Tests/`) requires updating the stub in the same commit as the rename
- Do not change a function's parameter types or return type without checking all call sites across `App/Src/`, `App/Tests/`, and `Core/`

---

## Definition of Done

An item in "Next session priorities" is **done** only when all applicable checkboxes below are met. Do not remove an item from "Next session priorities" until DoD is satisfied.

- [ ] **Host tests pass** — `ctest --test-dir build-tests` exits 0 with no skipped suites
- [ ] **Hardware validation complete** — or explicitly waived by appending `(hardware waiver: <reason>, <date>)` inline to the item's entry in "Next session priorities"
- [ ] **Complexity delta rated** — rated neutral / increase / decrease; if `increase`, a `[complexity]` follow-up item exists in "Next session priorities"
- [ ] **Session log entry added** — entry in `docs/PROJECT_HISTORY.md` Session Log at the appropriate tier (Minor or Major — see Entry Tiers in Update Rules)
- [ ] **Artifact triggers actioned** — every applicable row in the Artifact-Based Update Triggers table has been acted on; if no rows fired, this is automatically satisfied

### Hardware-Gated Items

If an item is blocked on **hardware access only** and all other DoD checkboxes pass, it may be moved from **Active** to **Backlog** in `CLAUDE.md` with a `[hardware]` tag. Hardware-gated items:
- Are **not** considered done
- Do **not** count against the 3-item complexity debt ceiling
- Must not be silently dropped — they stay in the backlog until hardware-validated and closed with a `docs/PROJECT_HISTORY.md` Resolved Items row

---

## Update Rules

Rate complexity delta before every commit.

### Artifact-Based Update Triggers

These triggers fire regardless of change size. If the artifact changed, the update is **mandatory** — no size-based exception applies. Check this table before every commit.

| If this changed | Always do this | Also verify |
|---|---|---|
| `App/Inc/persist.h` — `PersistBlock_t` layout | Update size assertion in `test_persist_roundtrip.c` | `docs/TECHNICAL.md` persist layout |
| `App/Inc/app_common.h` — `CalcMode_t` values | Update `docs/TECHNICAL.md` Input Modes table | — (no CLAUDE.md block exists; read `app_common.h` directly for current values) |
| `App/Inc/app_common.h` — `GraphState_t` fields | Update `docs/TECHNICAL.md` Graphing → State section | — (no CLAUDE.md block exists; read `app_common.h` directly for current values) |
| Any `App/Src/*.c` file added or removed | Update `docs/TECHNICAL.md` Project Structure listing | `docs/ARCHITECTURE.md` file tree |
| Any `App/Tests/` test added or removed | Re-run `cmake --build build-tests`; update `docs/TESTING.md` assertion counts (it is the canonical source — run `scripts/update_test_counts.sh`) | Scorecard Testing row if rating changed |
| `docs/PRGM_COMMANDS.md` touched | Verify it matches `prgm_exec.c` dispatch table exactly | `docs/prgm_manual_tests.md` test IDs |
| Any new public function or header added | Confirm module prefix convention (`Calc_*`, `Graph_*`, etc.) | `docs/ARCHITECTURE.md` module diagram if boundary changed |
| New `App/Src/ui_*.c` module with symbols referenced in `calculator_core.c` | Add a stub section to `App/Tests/calculator_core_test_stubs.h` (typedef + extern + static inline no-ops) and concrete definitions in `App/Tests/test_normal_mode.c` | Rebuild `build-tests` — confirm no undeclared identifier errors before committing |

### After Every Commit

- **Complexity delta** — rate neutral / increase / decrease; record it as a `Complexity: <rating>` footer line in the commit message (e.g. `Complexity: neutral`); if increase, also add a `[complexity]` item to `CLAUDE.md` "Next session priorities"
- **Completed item** — if the commit resolved a priority item, remove it from `CLAUDE.md` "Next session priorities" and update `Est. Done` % if applicable
- **Test count changed** — update `docs/TESTING.md` assertion counts (run `scripts/update_test_counts.sh`); update the scorecard Testing row if the rating changed
- **RAM or FLASH size changed** — update `docs/TECHNICAL.md` Memory Layout section (lines 686–688)

### Once Per Session (at close)

Run these once when closing a session — not after each individual commit. A five-commit session gets one session log entry, not five.

- **Session log** — add one entry to `docs/PROJECT_HISTORY.md` "Session Log" (see Entry Tiers below)
- **CLAUDE.md sync** — verify Feature Completion %, Est. Done values, and open priorities reflect the full session's work
- **Public numbers** — if test counts, feature %, or memory % changed during the session, sync them across all docs (see Numbers to Keep in Sync)

### After a Small Change (single file, < ~100 lines, no new public API)

- **Gotcha check** — does the change create or resolve a gotcha in `CLAUDE.md` "Common Gotchas"?
- **Feature status** — if the change advances a feature, update `Est. Done` % in `CLAUDE.md`
- **Bug fix test** — a bug fix without a new or updated test is incomplete

### After a Medium Change (multiple files, new logic, new API surface)

**Check the Artifact-Based Update Triggers table first** — CalcMode_t, GraphState_t, PersistBlock_t, PRGM commands, and test count changes are all covered there. The items below are the only Medium Change requirements not already in that table:

- **New math function** — update `docs/TECHNICAL.md` Supported Functions table and `CLAUDE.md` Feature Completion Status
- **Gotcha created or resolved** — update `CLAUDE.md` "Common Gotchas" numbered list

### After a Large Change (new module, feature complete, major refactor)

Triggers: module extraction, feature shipped, spec alignment, doc restructure.

- **`docs/ARCHITECTURE.md`** — add/update the module in the Mermaid diagram and file tree
- **`docs/TECHNICAL.md` Project Structure listing** — update `App/Src/` and `App/Inc/` entries
- **`docs/GETTING_STARTED.md`** — verify build commands, OpenOCD paths, and test section are accurate
- **`docs/TESTING.md`** — update if new test executables or coverage targets added (**canonical source for all test counts** — other docs link here)
- **`README.md`** — update feature % if completion changed significantly (> ~5%)
- **`CLAUDE.md` Feature Completion Status** — update the `~72%` header if overall completion changed
- **`docs/PROJECT_HISTORY.md`** — add a session log entry and update Completed Features if applicable
- **Scorecard above** — review all 10 dimensions; update any that changed
- **Document Map above** — add any new doc file

**Verification — New Contributor Smoke Test:** Open a fresh terminal with no project-specific PATH configuration. Execute each numbered step in `docs/GETTING_STARTED.md` verbatim. If any step fails, requires knowledge not stated in the doc, or produces output that contradicts the doc text, update the doc before closing the session. Common failure points: OpenOCD version path, CMake minimum version, ARM toolchain PATH entry, test executable names.

### After a Quality Item is Resolved

- **`CLAUDE.md` "Next session priorities"** — remove the resolved item
- **`CLAUDE.md` "Project Quality"** — update overall rating % if it changed
- **Scorecard above** — update the affected dimension rating if it changed
- **`docs/PROJECT_HISTORY.md`** — add a row to Resolved Items; add a Milestone Reviews entry if a rating changed or a major milestone was reached

### PROJECT_HISTORY.md Entry Tiers

Two tiers control how much to write per session. Choose based on impact, not commit count.

**Minor** — routine work: test additions, small refactors, doc fixes, config changes, gotcha resolutions. Write **one summary bullet** for the whole session, regardless of how many commits it contains. Format: `Session YYYY-MM-DD — brief description (N commits).`

**Major** — feature shipped, module added or extracted, scorecard rating changed, significant bug fixed, breaking change made. Write a **dedicated paragraph** entry. Always add a Completed Features or Resolved Items row in addition to the session log entry.

When uncertain: if someone reading the history 6 months from now would want a paragraph rather than a bullet, it is Major.

---

## Numbers to Keep in Sync

| Number | Canonical source | Also in |
|---|---|---|
| Test counts (per-executable) | `docs/TESTING.md` (canonical) | — (`docs/GETTING_STARTED.md` links here; no hardcoded mirror) |
| Overall test total | `docs/TESTING.md` | — |
| Feature completion % | `CLAUDE.md` Feature table | `README.md` Status narrative (prose, not a mirrored table) |
| Overall quality rating | `CLAUDE.md` "Project Quality" | — |
| Memory layout % used | `docs/TECHNICAL.md` lines 686–688 | — (`docs/ARCHITECTURE.md` links here; no numeric mirror) |
| `PersistBlock_t` size (bytes) | `App/Inc/persist.h` comment | `test_persist_roundtrip.c`, `docs/TECHNICAL.md` |
| `PERSIST_VERSION` | `App/Inc/persist.h` | Migration case in `Persist_Load()`, `docs/TECHNICAL.md` Persist Versions table |
| `CalcMode_t` values | `App/Inc/app_common.h` | `docs/TECHNICAL.md` "Input Modes" |
| `GraphState_t` fields | `App/Inc/app_common.h` | `docs/TECHNICAL.md` "Graphing → State" |

---

## File Structure Maintenance

**When a file is added** (`App/Src/*.c`, `App/Inc/*.h`, or a new doc):
- Add to `docs/TECHNICAL.md` Project Structure listing with a one-liner: `filename.c — <module responsibility>`
- Add to `docs/ARCHITECTURE.md` directory map with the same one-liner
- Confirm module prefix convention (`Calc_*`, `Graph_*`, etc.) if a new public API is introduced
- If a doc file, add it to the Document Map table above

**When a file is removed:**
- Remove from both locations above
- Search `CLAUDE.md`, `README.md`, `docs/GETTING_STARTED.md`, `docs/ARCHITECTURE.md` for stale references

**When a file's responsibility changes:**
- Update the one-liner in both locations above
- Update the Mermaid diagram in `docs/ARCHITECTURE.md` if dependency structure changed
- Add a session log entry in `docs/PROJECT_HISTORY.md` explaining the change
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
| New architectural boundary or module | `docs/ARCHITECTURE.md` diagram + `docs/TECHNICAL.md` Project Structure listing |
| Expression pipeline stage changed | `docs/TECHNICAL.md` expression pipeline section |
| PRGM command added, removed, or changed | `docs/PRGM_COMMANDS.md` (canonical) |
| New gotcha or known issue | `CLAUDE.md` "Common Gotchas" numbered list |
| Memory region added or resized | `CLAUDE.md` "Critical Build Settings → SDRAM layout" + linker script + `docs/TECHNICAL.md` |
| New FLASH sector assigned | `docs/TECHNICAL.md` FLASH sector map + `CLAUDE.md` gotcha #15 |

---

## Staleness Hotspots

The Full Update Checklist (below) is the authoritative procedure. This section highlights the highest-risk sync points for quick reference — each item maps to a checklist step. Check these first after any significant session.

Items 1–3 (`PersistBlock_t`, `CalcMode_t`, `GraphState_t`) are covered by the Artifact-Based Update Triggers table in Update Rules — that table is the authoritative procedure for those. The items below are the remaining high-risk sync points not covered by the artifact table:

1. **Test counts** — drift every time a test is added. Always copy from `cmake --build build-tests` output; never estimate.

2. **Feature completion %** — `CLAUDE.md`'s feature table and `README.md`'s status table are independent text blocks describing the same truth.

3. **`CLAUDE.md` Architecture file list** — the `App/Src/` table is manually maintained; easy to forget when adding a new extracted module.

4. **Memory layout %** — in both `TECHNICAL.md` (canonical) and `ARCHITECTURE.md` (mirror); neither is computed automatically.

---

## Full Update Checklist

Run after any significant session as a completeness check.

Steps marked **[AI]** can be executed entirely by an AI session. Steps marked **[Human]** require hardware access or manual judgment and must be handed off explicitly — they are not skippable by an AI.

### Step 1 — Run host tests first

**[AI]**
```bash
cmake -S App/Tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests
```

All suites must exit 0. See [docs/TESTING.md](TESTING.md) for the authoritative suite list. Fix any failure before proceeding.

### Step 2 — Identify what changed

**[AI]**
```bash
git log --oneline -15          # review recent commits; identify the session boundary
git diff <first-session-hash>^..HEAD --stat   # stat of all changes in this session
```

For each commit note: new `CalcMode_t` values? New `GraphState_t` fields? `PersistBlock_t` change? New `App/Src/` files? Test count change? Any artifact triggers (see table above) fired? Any `Complexity: increase` footers?

### Step 3 — Verify `CLAUDE.md` is current

**[AI]**
- [ ] Feature Completion Status % reflects what shipped
- [ ] Feature table `Est. Done` values are correct
- [ ] All completed priority items removed from "Next session priorities" (DoD satisfied)
- [ ] Complexity increases have a `[complexity]` follow-up item
- [ ] Open `[complexity]` items ≤ 3 (debt ceiling); if exceeded, report to user before proceeding
- [ ] TECHNICAL.md Project Structure listing covers every `App/Src/` file
- [ ] `CalcMode_t` block matches `App/Inc/app_common.h` exactly
- [ ] `GraphState_t` block matches `App/Inc/app_common.h` exactly
- [ ] `docs/TESTING.md` assertion counts match Step 1 output (run `scripts/check_sync.sh` to verify all sync points at once)
- [ ] Scorecard Change Log row added if any rating changed this session

### Step 4 — Update `PROJECT_HISTORY.md` (single history source)

**[AI]** — Apply the Entry Tiers from the Update Rules section: one entry per session, not one per commit. Choose Minor (summary bullet) or Major (dedicated paragraph) based on impact.
- [ ] Session Log — one entry for the session (Minor: one-line summary; Major: short paragraph)
- [ ] Completed Features — add a row if a feature shipped (Major tier only)
- [ ] Resolved Items — add a row if a significant quality item was resolved (Major tier only)
- [ ] Milestone Reviews — add an entry if a rating changed or a major milestone was reached (Major tier only)

### Step 5 — Sync the public numbers

**[AI]** — Run `scripts/check_sync.sh` first; it validates most of these automatically. Address any reported drift before proceeding.
- [ ] `scripts/check_sync.sh` exits 0 (or all reported drifts are explained and resolved)
- [ ] Feature completion % in `CLAUDE.md` narrative matches `README.md` Status section
- [ ] Memory layout % in `docs/TECHNICAL.md` lines 686–688 reflects current build (check after any file size change)

### Step 6 — Verify cross-references

**[AI]** — Run only if Step 2 shows any file added, removed, or moved (`git diff --stat` includes a line with `=>` or shows a new/deleted filename). Skip entirely if no filesystem changes occurred this session.
- [ ] Every file linked from `README.md` exists in `docs/`
- [ ] Every module listed in `ARCHITECTURE.md` exists in `App/Src/` or `App/Inc/`
- [ ] Every file path in `PROJECT_HISTORY.md` Resolved Items still exists

### Step 7 — Hardware-gated items

**[Human]** — AI must list these explicitly at session close; do not mark them complete.
- [ ] All open `[hardware]` items in "Next session priorities" reviewed; any newly unblocked items promoted to Active
- [ ] Flash firmware and run any manual test plans for items under hardware validation

---

## Periodic Code Review

**Auto-trigger:** Run a Periodic Code Review at the start of any session where the following command reports ≥ 3 files changed by ≥ 50 lines since the last review entry in `docs/PROJECT_HISTORY.md`:

```bash
git log --oneline --all | grep "periodic.*review\|code.*review" | head -1
# find the hash above, then:
git diff <hash>..HEAD --stat | awk -F'[|+]' 'NF>2 && $3+0 >= 50' | wc -l
```

Also run manually after any major feature, before adding a new module, or when the codebase feels like it has grown faster than it has been simplified.

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
- `[complexity]` — a function or file that has grown past the complexity thresholds and needs extraction or simplification
- `[hardware]` — a validation item that requires physical hardware access to close

Order by effort: `[docs]` items first (minutes each), then `[refactor]` items by ascending line count.

Items not yet ready to act on (prerequisite incomplete, overlapping scope, high regression risk) — add them anyway with a **Prerequisite:** or **Coordinate with:** note inline so the context is not lost.

If the review surfaces a non-obvious structural insight for future sessions, save it as a project memory.

---

## Git Workflow

### Commit Message Convention

This project uses Conventional Commits. Prefix every commit message with a type:

| Type | Use for |
|---|---|
| `feat:` | New calculator behaviour, UI feature, new math function |
| `fix:` | Bug fix — incorrect behaviour, crash, display glitch |
| `refactor:` | Code restructuring with no behaviour change |
| `test:` | New or updated host tests |
| `docs:` | Documentation-only changes |
| `chore:` | Build config, toolchain, CMakeLists, .gitignore |
| `perf:` | Performance improvement |

Always include a `Complexity:` footer on every commit:

```
feat: add LinReg to STAT CALC menu

Complexity: increase
```

```
refactor: extract param_yeq helpers into ui_param_yeq.c

Complexity: decrease
```

The `Complexity:` footer is the persistent record of the delta rating — future sessions can run `git log --grep="Complexity: increase"` to audit debt accumulation.

### Staging and Committing

Stage specific files by name rather than `git add -A` to avoid accidentally committing build artefacts or sensitive files.

```bash
# Stage only the files that changed — never git add -A
git add App/Src/<changed>.c App/Inc/<changed>.h CLAUDE.md
git commit -m "type: description

Complexity: neutral|increase|decrease"
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

---

## Updating This Document

This document may be updated when:
- A new recurring type of change arises that is not covered by existing rules
- A rule proves unworkable in practice — document the failure case in `docs/PROJECT_HISTORY.md` before changing the rule
- A new module or subsystem type is added that needs its own artifact trigger row
- A scorecard dimension's rise/fall criteria no longer match how the codebase actually evolves

**Process:**
1. Propose the change to the user before editing this file
2. Add a note to `docs/PROJECT_HISTORY.md` Session Log when a significant rule changes
3. If a scorecard dimension's rise/fall criteria change, immediately re-evaluate the current rating against the new criteria and update `CLAUDE.md`

**Do not add rules to solve one-time problems.** If a rule would only have applied to a single past incident, capture the incident in `docs/PROJECT_HISTORY.md` instead. Rules that exist for one past event become noise that dilutes the signal of rules that apply broadly.

**Removing rules:** A rule may be removed if it has not fired in 10+ sessions and the scenario it guards against no longer exists in the codebase. Document the removal reason in `docs/PROJECT_HISTORY.md`.
