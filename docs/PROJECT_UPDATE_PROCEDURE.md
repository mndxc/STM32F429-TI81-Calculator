# Project Update Procedure — STM32F429 TI-81 Calculator

## Context

The project has two authoritative "source of truth" files (CLAUDE.md, QUALITY_TRACKER.md) and
several public-facing docs (README, GETTING_STARTED, TECHNICAL, ARCHITECTURE) that contain
progress-sensitive numbers and statuses. When work is done, these files drift out of sync. This
plan defines a tiered, ordered procedure — lightweight after small commits, thorough after
milestones — so no area is left dangling.

---

## Document Map

| File | Owner | What goes stale |
|---|---|---|
| `CLAUDE.md` | Session continuity | Feature completion %, priority list, session log, Architecture file list, CalcMode_t enum block, GraphState_t struct block, test counts in "Host Tests" section, overall rating % |
| `docs/QUALITY_TRACKER.md` | Quality register | P-item statuses, "At a Glance" table, scorecard ratings, test counts in P1 table, review history entries |
| `README.md` | Public overview | Feature status table rows (% complete, notes) |
| `docs/GETTING_STARTED.md` | Onboarding | Test counts (3 numbers + total), OpenOCD path version |
| `docs/TECHNICAL.md` | Reference | Supported functions table, Input Modes table, Memory layout %, FLASH sector contents |
| `docs/ARCHITECTURE.md` | Module orientation | Module dependency diagram, file tree listing, memory layout % (mirror of TECHNICAL.md — TECHNICAL.md is canonical) |
| `App/Tests/test_persist_roundtrip.c` | Test oracle | Hardcoded `PersistBlock_t` size assertion — must match `persist.h` |

---

## Tiered Update Rules

### Tier 1 — After every commit (always)

These are mandatory, per the complexity management rule already in CLAUDE.md.

1. **CLAUDE.md → "Completed features"**: Move the completed item from "Next session priorities"
   (or the feature table) into the correct place in the completed log or feature status table.
   Update the `Est. Done` percentage if applicable.

2. **CLAUDE.md → "Next session priorities"**: If the commit introduced a complexity increase,
   add a `[complexity]` follow-up item. If it resolved a priority item, remove it.

3. **CLAUDE.md → complexity impact review**: State what changed, rate delta (neutral/increase/decrease).

4. **QUALITY_TRACKER.md**: If a P-item was part of the commit, mark it resolved with the date,
   add a row to the "Resolved Items" table, and add a line to "Review History".

5. **QUALITY_TRACKER.md → Review History**: Add an entry for this session regardless of whether a
   P-item was formally resolved. Every session should be traceable. Format:
   `| date | Antigravity AI | Session N: <one-line summary of what was done> |`

### Tier 2 — After a feature is complete (new behaviour ships)

6. **README.md → Status table**: Update the % estimate and notes for that feature row.

7. **CLAUDE.md → Feature Completion Status header line**: Update the overall `~65%` percentage
   and the session log entry.

8. **QUALITY_TRACKER.md → Overall Assessment / Estimated production readiness**: Update if the
   feature completion changes the rating.

9. **If a new `CalcMode_t` value was added**: Update CLAUDE.md "Input Mode System" block AND
   TECHNICAL.md "Input Modes" table in the same commit — never separately. Both must match
   `App/Inc/app_common.h` exactly.

10. **If a `GraphState_t` field was added or removed**: Update CLAUDE.md "Graphing System → State"
    block to match `App/Inc/app_common.h`.

11. **If `PersistBlock_t` layout changed**: Update the hardcoded size literal in
    `App/Tests/test_persist_roundtrip.c` (search for `EXPECT_EQ((int)sizeof(PersistBlock_t)`) to
    match the comment in `App/Inc/persist.h`. Run host tests before committing to confirm.

### Tier 3 — After a module extraction or test suite change

12. **ARCHITECTURE.md → Module dependency diagram**: Add the new module to the Mermaid diagram
    and the file tree listing.

13. **TECHNICAL.md → Supported functions table**: If a new math function was wired, add it.

14. **GETTING_STARTED.md → Host Tests section**: Update all four test counts
    (e.g. `153 tests` → new number) and the total (e.g. `422 total`).

15. **CLAUDE.md → "Host Tests" section**: Update the same four counts and total to match.

16. **QUALITY_TRACKER.md → P1 table**: Update the per-executable test counts and branch
    coverage % if changed.

### Tier 4 — After a quality/CI item resolved (P-item)

17. **QUALITY_TRACKER.md → "At a Glance" table**: Flip the row from 🔴/🟡 → ✅.

18. **QUALITY_TRACKER.md → Scorecard**: Update the affected dimension rating and date.

19. **QUALITY_TRACKER.md → Prioritised Improvement Roadmap**: Strike through the resolved item
    in both "By Ease" and "By Impact" tables.

20. **QUALITY_TRACKER.md → Open Issues**: Mark the issue `✅ Resolved (date)`.

21. **CLAUDE.md → "Project Quality" → Current overall rating**: Update the `83–89%` line.

---

## Full Project Update Checklist

Run this after any significant session (multi-commit or milestone). It is a completeness
verification, not a replacement for the tier rules above.

### Step 1 — Run host tests as the ground-truth oracle

Do this **first**. A failing test immediately identifies concrete mismatches (wrong size
literals, broken logic) and tells you exactly what to fix before any doc work begins.

```bash
cmake -S App/Tests -B build/tests && cmake --build build/tests
./build/tests/test_calc_engine
./build/tests/test_expr_util
./build/tests/test_persist_roundtrip
./build/tests/test_prgm_exec
```

All four must exit `0` (full pass). The output lines show the exact test counts — copy these
into the docs. Never estimate. If a test fails, fix it before proceeding.

### Step 2 — Capture the session anchor

Before reading any docs, explicitly list what changed this session. This anchors the rest of
the checklist — you can quickly scan each item and ask "does this change affect X?".

```bash
# Commits since the last time CLAUDE.md was touched (last sync point)
git log --oneline $(git log --format="%H" -- CLAUDE.md | sed -n '2p')..HEAD
git diff HEAD~N HEAD --stat   # replace N with commit count above
```

For each commit, note:
- Were any new `CalcMode_t` values added? → affects Steps 4, 6 (Tier 2 item 9)
- Were any `GraphState_t` fields added? → affects Step 4 (Tier 2 item 10)
- Did `PersistBlock_t` layout change? → affects Step 1 assertion, Step 6 sync table
- Were any new App/Src/ files added? → affects Step 4 architecture file list
- Were any test counts changed? → affects Steps 4, 5, 6

### Step 3 — Verify CLAUDE.md is current
- [ ] "Feature Completion Status" header % reflects what shipped
- [ ] Every session is logged under "Sessions:" with the date
- [ ] Feature table rows have correct `Est. Done` values
- [ ] All completed priority items are gone from "Next session priorities"
- [ ] Any complexity increases have a `[complexity]` follow-up item
- [ ] "Architecture → File structure" table lists every App/ source file (run `ls App/Src/`)
- [ ] `CalcMode_t` block in "Input Mode System" matches `App/Inc/app_common.h` exactly
- [ ] `GraphState_t` block in "Graphing System → State" matches `App/Inc/app_common.h` exactly
- [ ] Test counts in "Host Tests" section match Step 1 output

### Step 4 — Verify QUALITY_TRACKER.md is current
- [ ] "Last reviewed" date at the top is today
- [ ] "At a Glance" table status matches the actual open/closed P-items
- [ ] Scorecard ratings and dates are current
- [ ] All recently resolved P-items are in the "Resolved Items" table with dates
- [ ] "Review History" has a new entry for this session (Tier 1 item 5)
- [ ] Both "By Ease" and "By Impact" roadmap tables are strikethrough-updated

### Step 5 — Sync the public numbers

These numbers appear in multiple files and must match exactly:

| Number | Canonical source | Also in |
|---|---|---|
| Test counts (per-executable) | Step 1 output | CLAUDE.md "Host Tests", GETTING_STARTED.md, QUALITY_TRACKER.md P1 |
| Overall test total | Step 1 output | CLAUDE.md "Host Tests", GETTING_STARTED.md, QUALITY_TRACKER.md P1 |
| Feature completion % | CLAUDE.md Feature table | README.md Status table |
| Overall quality rating | QUALITY_TRACKER.md Assessment | CLAUDE.md "Project Quality" |
| Memory layout % used | TECHNICAL.md | ARCHITECTURE.md (mirror) |
| `PersistBlock_t` size (bytes) | `App/Inc/persist.h` comment | `App/Tests/test_persist_roundtrip.c`, TECHNICAL.md sector map, CLAUDE.md |
| `CalcMode_t` values | `App/Inc/app_common.h` | CLAUDE.md "Input Mode System", TECHNICAL.md "Input Modes" |
| `GraphState_t` fields | `App/Inc/app_common.h` | CLAUDE.md "Graphing System → State" |

For each row, check all copies and make them match the canonical source.

### Step 6 — Verify cross-references are not dangling
- [ ] Every file linked from README.md exists in `docs/`
- [ ] Every file referenced in ARCHITECTURE.md "Adding a New Math Function" exists
- [ ] Every module listed in ARCHITECTURE.md exists in `App/Src/` or `App/Inc/`
- [ ] Every P-item file path in QUALITY_TRACKER.md still exists (renamed files break these)

---

## Staleness Hotspots (highest risk, check first)

1. **`PersistBlock_t` size assertion in test** — `App/Tests/test_persist_roundtrip.c` contains a
   hardcoded byte count that must match `App/Inc/persist.h`. Any layout change (new field,
   padding adjustment) silently breaks CI if the assertion is not updated. This is the only
   hotspot that causes an **active test failure**, not just a doc drift.

2. **`CalcMode_t` enum — 3-way sync** — `App/Inc/app_common.h` is canonical. Any new mode must
   be added to CLAUDE.md "Input Mode System" block AND TECHNICAL.md "Input Modes" table in
   the same commit. The two doc copies are independent text blocks with no mechanical link.

3. **`GraphState_t` struct** — `App/Inc/app_common.h` is canonical. Any new field must be
   reflected in the CLAUDE.md "Graphing System → State" struct block.

4. **Test counts** — three separate numbers appear in CLAUDE.md, GETTING_STARTED.md, and
   QUALITY_TRACKER.md P1. They drift every time a test is added. Always copy from Step 1 output.

5. **Feature completion %** — CLAUDE.md's feature table and README.md's status table are
   independent text blocks that describe the same truth.

6. **CLAUDE.md Architecture file list** — the `App/Src/` file table is manually maintained.
   Every new extraction adds a file but the table is easy to forget.

7. **QUALITY_TRACKER.md "At a Glance" table** — three status fields per item (At a Glance row,
   the P-item itself, the Roadmap tables). All three must be updated together.

8. **Memory layout % used** — appears in both TECHNICAL.md (canonical) and ARCHITECTURE.md
   (mirror). Neither is computed automatically; both must be updated when the firmware grows
   significantly.

---

## Periodic Code Review Procedure

Use this when you want a structured review of the whole codebase for simplification,
refactoring, file structure, and documentation quality. Run it at natural milestones
(after a major feature lands, before starting a new module, or when the codebase feels
like it has grown faster than it has been simplified).

The output is a `docs/CODE_REVIEW_PENDING.md` checklist — a flat, actionable list of
items ordered by ease × impact. Delete it when all items are checked off.

---

### Phase 1 — Broad structural scan (delegate to an agent)

Launch an Explore agent with this prompt (adapt the file list to the current `App/Src/`):

> Perform a thorough exploration of this STM32 TI-81 Calculator codebase at `<repo root>`.
> For each `App/Src/*.c` file report: total line count, top-level function names and
> approximate line counts, any functions exceeding 80–100 lines, and any obvious code
> smells (magic numbers, duplicated patterns, large switch statements).
> Also report: header files that look redundant or could be consolidated; test structure
> and coverage gaps; and docs folder contents.
> Return a structured report I can use to make specific refactoring recommendations.

This produces a ranked hotspot table (functions by line count, files by grade) without
consuming the main context window on raw file reads.

---

### Phase 2 — Direct reads of key files

After the agent report, read these files directly to ground the findings:

1. `App/Inc/calc_internal.h` — shared internal header; check for duplicate constants and
   over-broad exposure of state.
2. `App/Inc/app_common.h` — shared public header; check for correct scope.
3. `docs/ARCHITECTURE.md` — does it match the current file tree?
4. `docs/GETTING_STARTED.md` — are build commands, paths, and test counts still accurate?
5. `README.md` — does the status section reflect current feature state?
6. The top ~80 lines of the largest `App/Src/*.c` files to verify constant blocks,
   include lists, and file-level doc comments.

---

### Phase 3 — Synthesize into a report

Organize findings into four sections, in this order:

1. **Simplification & Refactoring** — functions over 100 lines, duplicated patterns,
   magic numbers not yet in a named constant, constants defined in multiple places.
   For each item: name the function, state the line count, and describe the mechanical
   extraction (no logic change required).

2. **File Structure** — header scope problems, asymmetric directory layout, files that
   should be merged or split, subdirectories that need explanation in ARCHITECTURE.md.

3. **Documentation** — stale paths or version numbers, missing onboarding steps,
   sections duplicated across files that have drifted, overly long files that serve
   two different audiences.

4. **Onboarding** — answer "can a new contributor run something in under 5 minutes?".
   Check: is there a hardware-free quickstart? Are build commands copy-paste safe?
   Is CONTRIBUTING.md complete and linked from README?

---

### Phase 4 — Write `docs/CODE_REVIEW_PENDING.md`

Create the checklist file with this structure:

```markdown
# Code Review — Action Items (Temporary)
> Delete this file when all items are checked off.
> Generated <date>.

## Quick wins (docs/config — no logic risk)
- [ ] **N. Title** — one sentence. Files: `...`

## Code refactoring (mechanical extractions — no logic changes)
- [ ] **N. Title** — one sentence. Files: `...`

## Future P-items to add to QUALITY_TRACKER (not immediate)
- **Title** — one sentence description.
```

Order items within each section by effort (smallest first). Include file paths on every
item so there is no ambiguity about where to make the change.

---

### Phase 5 — Save a project memory (optional)

If the review surfaces a non-obvious structural insight that should inform future sessions
(e.g. a recurring duplication pattern, a header boundary that keeps being violated), save
it as a `project` type memory in the project memory store. Do not save the full review —
only what is surprising or non-obvious from reading the code.

---

## How This Scales as the Project Grows

- **New doc files added** (`docs/TESTING.md`, `docs/TROUBLESHOOTING.md`, etc.): add them to the
  Document Map table above and identify what makes them stale.
- **New P-items** in QUALITY_TRACKER.md: always add a corresponding "At a Glance" row at creation.
- **New math functions**: update TECHNICAL.md Supported Functions table and CLAUDE.md
  Feature Completion Status in the same commit.
- The canonical sources defined in the "Sync the public numbers" table are fixed — if a number
  moves to a new canonical home, update this checklist.
