# P28 Cursor Refactor — Manual Verification Tests

Temporary checklist. Delete after all tests pass.

**Purpose:** Verify that replacing `cursor_place()` with `cursor_render()` (8 commits, 2026-04-01) produced zero visible behaviour change across all 7 cursor-update functions.

**Pre-flight:** Build 0 errors ✓ — flash and power-cycle before testing.

---

## Main expression cursor

These are the highest-priority tests — the main cursor is the only one that uses the `MODE_STO` synthesis path.

| # | Action | Expected |
|---|---|---|
| 1 | Power on, wait for home screen | Cursor blinks grey block on expression row |
| 2 | Press `2ND` | Cursor turns amber with `^` inside |
| 3 | Press any function key (e.g. `SIN`) | Cursor resets to grey block |
| 4 | Press `ALPHA` | Cursor turns green with `A` inside |
| 5 | Press any letter key (e.g. `A`) | Letter inserted, cursor resets to grey |
| 6 | Press `STO→` | Cursor turns green with `A` inside (STO-pending state) |
| 7 | While STO-pending, press any letter | Stores to that variable, cursor resets to grey block |
| 8 | Press `2ND+INS` | Cursor changes to underscore style |
| 9 | Press `2ND+INS` again | Cursor returns to full-height block |

---

## Y= editor

| # | Action | Expected |
|---|---|---|
| 10 | Press `Y=` | Cursor blinks on first equation field |
| 11 | Press `2ND` | Cursor turns amber with `^` |
| 12 | Press any key to resolve 2ND | Cursor resets |
| 13 | Press `2ND+INS` | Cursor changes to underscore style |
| 14 | Press `2ND+INS` again | Cursor returns to block |

---

## RANGE editor

| # | Action | Expected |
|---|---|---|
| 15 | Press `RANGE` | Cursor blinks in Xmin field |
| 16 | Press `2ND+INS` | Cursor changes to underscore style |
| 17 | Press `2ND+INS` again | Cursor returns to block |

---

## ZOOM FACTORS editor

| # | Action | Expected |
|---|---|---|
| 18 | Press `ZOOM` → select Set Factors | Cursor blinks in XFact field |
| 19 | Press `2ND+INS` | Cursor changes to underscore style |
| 20 | Press `2ND+INS` again | Cursor returns to block |

---

## Matrix editor — **insert mode must not affect cursor shape**

This is a critical check: `insert=false` was a deliberate decision for the matrix editor (no `TOKEN_INS` handler exists in `handle_matrix_edit`).

| # | Action | Expected |
|---|---|---|
| 21 | Press `MATRX` → EDIT → select a matrix → navigate to a cell | Cursor is a full-height grey block |
| 22 | Press `2ND+INS` | Cursor shape does **not** change (remains full-height block) |
| 23 | Press `2ND` | Cursor turns amber with `^` |
| 24 | Press `ALPHA` | Cursor turns green with `A` |

---

## PRGM name entry — **insert mode must not affect cursor shape**

Also `insert=false` by design — name-entry is always alpha-layer with no insert toggle.

| # | Action | Expected |
|---|---|---|
| 25 | Press `PRGM` → NEW | Cursor blinks in name field; full-height block |
| 26 | Press `2ND+INS` | Cursor shape does **not** change (remains full-height block) |

---

## PRGM line editor

| # | Action | Expected |
|---|---|---|
| 27 | Press `PRGM` → EDIT → select a program | Cursor blinks on program line |
| 28 | Press `2ND+INS` | Cursor changes to underscore style |
| 29 | Press `2ND+INS` again | Cursor returns to block |

---

## Sign-off

All 29 tests pass → delete this file and add a row to `docs/PROJECT_HISTORY.md`:

```
| 2026-04-02 | P28 hardware verified — cursor_render() behaviour confirmed correct across all 7 editors. |
```
