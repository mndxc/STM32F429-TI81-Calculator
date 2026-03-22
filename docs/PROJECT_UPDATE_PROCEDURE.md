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
| `CLAUDE.md` | Session continuity | Feature completion %, priority list, session log, Architecture file list, CalcMode_t enum block, test counts in "Host Tests" section, overall rating % |
| `docs/QUALITY_TRACKER.md` | Quality register | P-item statuses, "At a Glance" table, scorecard ratings, test counts in P1 table, review history entries |
| `README.md` | Public overview | Feature status table rows (% complete, notes) |
| `docs/GETTING_STARTED.md` | Onboarding | Test counts (3 numbers + total), OpenOCD path version |
| `docs/TECHNICAL.md` | Reference | Supported functions table, Input Modes table, Memory layout %, FLASH sector contents |
| `docs/ARCHITECTURE.md` | Module orientation | Module dependency diagram, file tree listing, memory layout % |

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

### Tier 2 — After a feature is complete (new behaviour ships)

5. **README.md → Status table**: Update the % estimate and notes for that feature row.

6. **CLAUDE.md → Feature Completion Status header line**: Update the overall `~65%` percentage
   and the session log entry.

7. **QUALITY_TRACKER.md → Overall Assessment / Estimated production readiness**: Update if the
   feature completion changes the rating.

### Tier 3 — After a module extraction or test suite change

8. **ARCHITECTURE.md → Module dependency diagram**: Add the new module to the Mermaid diagram
   and the file tree listing.

9. **TECHNICAL.md → CalcMode_t table**: If a new mode was added (new `CalcMode_t` enum value),
   add its row.

10. **TECHNICAL.md → Supported functions table**: If a new math function was wired, add it.

11. **GETTING_STARTED.md → Host Tests section**: Update all three test counts
    (e.g. `153 tests` → new number) and the total (e.g. `301 total`).

12. **CLAUDE.md → "Host Tests" section**: Update the same three counts and total to match.

13. **QUALITY_TRACKER.md → P1 table**: Update the per-executable test counts and branch
    coverage % if changed.

### Tier 4 — After a quality/CI item resolved (P-item)

14. **QUALITY_TRACKER.md → "At a Glance" table**: Flip the row from 🔴/🟡 → ✅.

15. **QUALITY_TRACKER.md → Scorecard**: Update the affected dimension rating and date.

16. **QUALITY_TRACKER.md → Prioritised Improvement Roadmap**: Strike through the resolved item
    in both "By Ease" and "By Impact" tables.

17. **QUALITY_TRACKER.md → Open Issues**: Mark the issue `✅ Resolved (date)`.

18. **CLAUDE.md → "Project Quality" → Current overall rating**: Update the `83–89%` line.

---

## Full Project Update Checklist

Run this after any significant session (multi-commit or milestone). It is a completeness
verification, not a replacement for the tier rules above.

### Step 1 — Re-read the session diff
```bash
git log --oneline --since="session start date"
git diff HEAD~N HEAD --stat
```
List all files touched. This anchors the rest of the checklist.

### Step 2 — Verify CLAUDE.md is current
- [ ] "Feature Completion Status" header % reflects what shipped
- [ ] Every session is logged under "Sessions:" with the date
- [ ] Feature table rows have correct `Est. Done` values
- [ ] All completed priority items are gone from "Next session priorities"
- [ ] Any complexity increases have a `[complexity]` follow-up item
- [ ] "Architecture → File structure" table lists every App/ source file (run `ls App/Src/`)
- [ ] CalcMode_t block in "Input Mode System" matches `app_common.h` exactly
- [ ] Test counts in "Host Tests" section match the actual test executables

### Step 3 — Verify QUALITY_TRACKER.md is current
- [ ] "Last reviewed" date at the top is today
- [ ] "At a Glance" table status matches the actual open/closed P-items
- [ ] Scorecard ratings and dates are current
- [ ] All recently resolved P-items are in the "Resolved Items" table with dates
- [ ] "Review History" has a new entry for this session
- [ ] Both "By Ease" and "By Impact" roadmap tables are strikethrough-updated

### Step 4 — Sync the public numbers
These numbers appear in multiple files and must match exactly:

| Number | Canonical source | Also in |
|---|---|---|
| Test counts (per-executable) | CLAUDE.md "Host Tests" | GETTING_STARTED.md, QUALITY_TRACKER.md P1 |
| Overall test total | CLAUDE.md "Host Tests" | GETTING_STARTED.md, QUALITY_TRACKER.md P1 |
| Feature completion % | CLAUDE.md Feature table | README.md Status table |
| Overall quality rating | QUALITY_TRACKER.md Assessment | CLAUDE.md "Project Quality" |
| Memory layout % used | TECHNICAL.md | ARCHITECTURE.md |
| PersistBlock_t size (bytes) | persist.h comment | TECHNICAL.md, CLAUDE.md (matrix cell editor entry) |

For each row, check all copies and make them match the canonical source.

### Step 5 — Verify cross-references are not dangling
- [ ] Every file linked from README.md exists in `docs/`
- [ ] Every file referenced in TECHNICAL.md "Adding a New Math Function" exists
- [ ] Every module listed in ARCHITECTURE.md exists in `App/Src/` or `App/Inc/`
- [ ] Every P-item file path in QUALITY_TRACKER.md still exists (renamed files break these)

### Step 6 — Run host tests as the ground-truth oracle
```bash
cmake -S App/Tests -B build/tests && cmake --build build/tests
./build/tests/test_calc_engine
./build/tests/test_expr_util
./build/tests/test_persist_roundtrip
```
The output lines show the exact counts. Copy these into the docs — never estimate.

---

## Staleness Hotspots (highest risk, check first)

1. **Test counts** — three separate numbers appear in CLAUDE.md, GETTING_STARTED.md, and
   QUALITY_TRACKER.md P1. They drift every time a test is added.

2. **Feature completion %** — CLAUDE.md's feature table and README.md's status table are
   independent text blocks that describe the same truth.

3. **CLAUDE.md Architecture file list** — the `App/Src/` file table is manually maintained.
   Every new extraction adds a file but the table is easy to forget.

4. **QUALITY_TRACKER.md "At a Glance" table** — three status fields per item (At a Glance row,
   the P-item itself, the Roadmap tables). All three must be updated together.

5. **Memory layout % used** — appears in both TECHNICAL.md and ARCHITECTURE.md.
   Neither is computed automatically; both must be updated when the firmware grows significantly.

---

## Known Current Inconsistency (fix before next commit)

`docs/TECHNICAL.md` and `docs/ARCHITECTURE.md` both state memory layout percentages
(`~68% RAM used`, `~33% FLASH used`, `0% CCMRAM used`). These appear in both files but
are only mentioned in one place in CLAUDE.md (Critical Build Settings). Designate TECHNICAL.md
as canonical and note ARCHITECTURE.md as a mirror.

---

## How This Scales as the Project Grows

- **New doc files added** (`docs/TESTING.md`, `docs/TROUBLESHOOTING.md`, etc.): add them to the
  Document Map table above and identify what makes them stale.
- **New P-items** in QUALITY_TRACKER.md: always add a corresponding "At a Glance" row at creation.
- **New `CalcMode_t` values**: update TECHNICAL.md Input Modes table and CLAUDE.md's
  CalcMode_t block in the same commit — never separately.
- **New math functions**: update TECHNICAL.md Supported Functions table and CLAUDE.md
  Feature Completion Status in the same commit.
- The canonical sources defined in the "Sync the public numbers" table are fixed — if a number
  moves to a new canonical home, update this checklist.
