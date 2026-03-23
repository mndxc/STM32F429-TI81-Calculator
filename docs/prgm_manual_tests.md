# PRGM System — Manual Test Plan

Hardware: STM32F429I-DISC1. Run after flashing the latest build.
Pass criterion listed for each test. Mark ✅ / ❌ / ⚠️.

Last updated: 2026-03-22 (reflects sessions 2–23: full executor, dispatch-table refactor, all I/O commands)

---

## Menu & Navigation

### T01 — Open PRGM menu
**Steps:** From calculator home, press PRGM.
**Expected:** PRGM menu opens showing `EXEC  EDIT  ERASE` tab bar. EXEC tab highlighted **yellow**. All 37 program slots listed in format `1:Prgm1`, `2:Prgm2`, … `9:Prgm9`, `0:Prgm0`, `A:PrgmA`, … `θ:Prgmθ`. Programs that have been given a user name show it in a second column: `1:Prgm1  MYNAME`.

### T02 — Tab navigation
**Steps:** With PRGM menu open, press RIGHT twice.
**Expected:** Active tab advances: EXEC → EDIT → ERASE. Pressing LEFT returns. Tab highlight (yellow) moves. EXEC and EDIT both show all 37 slots in the same `N:PrgmN` format; ERASE shows only slots that have a user name or body.

### T03 — Scroll indicators on EXEC/EDIT
**Steps:** Open PRGM menu on EXEC or EDIT tab. Press DOWN repeatedly past the 7th visible slot.
**Expected:** `↓` amber indicator appears at the bottom of the list when more items exist below. `↑` amber indicator appears at the top once scrolled past the first slot. Indicators disappear when scrolled to the respective boundary.

### T04 — Close PRGM menu
**Steps:** With PRGM menu open, press CLEAR.
**Expected:** PRGM menu closes, calculator home screen returns. Expression buffer unchanged.

### T05 — Open PRGM menu from MATH menu
**Steps:** Open MATH menu (MATH key), then press PRGM.
**Expected:** MATH menu closes, PRGM menu opens on EXEC tab. No display corruption.

---

## Name Entry & Program Creation

### T06 — Create new program via EDIT tab (with name)
**Steps:** PRGM → RIGHT (EDIT tab) → navigate to an empty slot (e.g., `1:Prgm1`) → ENTER.
**Expected:** Name-entry screen appears showing `Prgm1:` with cursor. Alpha mode auto-engaged (green cursor with `A` inside).
Type `TEST` (T, E, S, T on ALPHA layer). Then ENTER.
**Expected:** `Prgm1:TEST` updates as each letter is typed. On ENTER, editor opens with title `Prgm1  TEST` and one blank `:` line with cursor.

### T07 — Create new program — skip name (name is optional)
**Steps:** PRGM → RIGHT (EDIT tab) → select an empty slot → ENTER → immediately press ENTER again (no name typed).
**Expected:** Editor opens with title `Prgm<N>` (no user name). The slot does NOT appear in the ERASE tab since it has no user name yet. Program body can be edited normally.

### T08 — Digits allowed in program name
**Steps:** PRGM → RIGHT (EDIT tab) → select an empty slot → ENTER → type `A1B2` (ALPHA+A, then 1, then ALPHA+B, then 2) → ENTER.
**Expected:** Each character appended: `A`, `1`, `B`, `2`. Digits `1` and `2` typed without requiring ALPHA. On ENTER, editor opens with title showing `A1B2` as the user name.

### T09 — Name entry DEL
**Steps:** From name-entry screen with letters typed, press DEL.
**Expected:** Last character removed. Cursor moves back. ALPHA re-engages after each letter so the next keypress continues as a letter by default.

### T10 — Cancel name entry
**Steps:** From name-entry screen, press CLEAR.
**Expected:** Returns to PRGM menu on EDIT tab. No name or body saved.

### T11 — Open existing program from EDIT tab
**Steps:** With at least one named program, open PRGM → EDIT tab → navigate to that slot (shows `N:PrgmN  NAME`) → ENTER.
**Expected:** Editor opens directly (no name-entry screen). Title shows `PrgmN  NAME`.

---

## Editor

### T12 — Editor cursor and character input
**Steps:** Open editor for any program slot. Type `A+1` (TOKEN_A in ALPHA layer, TOKEN_ADD, TOKEN_1).
**Expected:** Characters appear on the current (yellow-highlighted) line as `:A+1`. Cursor advances with each keypress.

### T13 — CTL sub-menu inserts keyword
**Steps:** In editor, press PRGM (opens CTL sub-menu). Navigate to `3:If ` and press ENTER (or press `3`).
**Expected:** CTL menu closes. Editor line now contains `:If ` with cursor after it.
Also verify: CTL menu has 14 items total (items 1–8 visible; items 9–14 require scrolling). Items 13 `IS>(` and 14 `DS<(` are visible after scrolling down past item 7.

### T14 — I/O sub-menu inserts keyword
**Steps:** In editor, press PRGM (CTL sub-menu opens). Press LEFT or RIGHT to switch to I/O tab. Navigate to `3:Disp ` and press ENTER (or press `3`).
**Expected:** I/O menu closes. Editor line contains `:Disp ` with cursor after it.
I/O menu has 6 items: `1:Input`, `2:Prompt`, `3:Disp`, `4:ClrHome`, `5:DispHome`, `6:DispGraph`. All 6 fit without scrolling.

### T15 — Multi-line editing and scroll
**Steps:** In editor, press ENTER six times to create 7+ lines. Keep pressing DOWN.
**Expected:** `↓` amber indicator appears on the last visible line when lines extend beyond 7 rows. `↑` appears when scrolled past row 0. Title stays fixed. Lines scroll correctly.

### T16 — Erase a program via ERASE tab
**Steps:** With at least one named program, go to PRGM → RIGHT RIGHT (ERASE tab) → select the program → ENTER.
**Expected:** Confirmation dialog appears showing `Prgm<N>  <NAME>` title, then `1:Do not erase` (yellow) and `2:Erase` (white). Press `2` or DOWN+ENTER to confirm. Program disappears from EXEC, EDIT (no longer shows user name), and ERASE lists.

---

## Persistence

### T17 — Programs survive 2nd+ON / power cycle
**Steps:** Create a program named `SAVE` in slot 1 with body `Disp "OK"`. Press 2nd+ON to save state. Power-cycle the board (USB unplug/replug). Open PRGM.
**Expected:** `1:Prgm1  SAVE` appears in EXEC and EDIT lists. Body intact when opened in editor.

---

## Executor — Basic Execution

### T18 — Run a Disp program
**Steps:** In EXEC tab, navigate to a slot containing:
```
Disp "HELLO"
```
Press ENTER.
**Expected:** PRGM menu closes, calculator home shows `HELLO` as a result row. Mode returns to normal.

### T19 — Run an empty slot (no-op)
**Steps:** In EXEC tab, navigate to a slot with no body (shows `N:PrgmN` with no user name column). Press ENTER.
**Expected:** PRGM menu closes, calculator returns to home. Nothing displayed (empty program = no output). No crash or lockup.

### T20 — Expression evaluation and ANS
**Steps:** Create and run program:
```
2+2
Disp ANS
```
**Expected:** `4` appears in history output. ANS is 4 after program completes.

### T21 — Input and variable store
**Steps:** Create and run program:
```
Input A
Disp A
```
When `A=?` prompt appears, type `7` and press ENTER.
**Expected:** `A=?` shown as expression row. After ENTER, `7` shown as result. `7` displayed by Disp. A=7 in variable store after.

### T22 — For( loop
**Steps:** Create and run program:
```
For(N,1,5,1)
Disp N
End
```
**Expected:** Five history rows appear showing `1`, `2`, `3`, `4`, `5` in order. Returns to normal mode.

### T23 — If/Then/Else/End
**Steps:** Create and run program:
```
Input A
If A>0
Then
Disp "POS"
Else
Disp "NEG"
End
```
Run twice: once enter `3`, once enter `-1`.
**Expected:** First run shows `POS`; second run shows `NEG`.

### T24 — CLEAR aborts execution
**Steps:** Create and run program:
```
Lbl A
Disp "X"
Goto A
```
While `X` rows are appearing, press CLEAR.
**Expected:** Execution stops immediately. Calculator returns to normal home screen. No lockup.

---

## Executor — Advanced Control Flow

### T25 — While loop
**Steps:** Create and run program:
```
1->I
While I<=3
Disp I
I+1->I
End
```
**Expected:** History shows `1`, `2`, `3` in order. Mode returns to normal after.

### T26 — For( with negative step (count-down)
**Steps:** Create and run program:
```
For(I,5,1,-1)
Disp I
End
```
**Expected:** History shows `5`, `4`, `3`, `2`, `1` in order.

### T27 — While with false initial condition (body never runs)
**Steps:** Create and run program:
```
While 0
Disp "NO"
End
Disp "DONE"
```
**Expected:** `NO` never appears. `DONE` appears once. No lockup.

### T28 — Goto/Lbl jump
**Steps:** Create and run program:
```
Goto END
Disp "SKIP"
Lbl END
Disp "DONE"
```
**Expected:** `SKIP` does not appear. `DONE` appears once.

### T29 — Pause halts and resumes on ENTER
**Steps:** Create and run program:
```
Disp "WAIT"
Pause
Disp "RESUMED"
```
**Expected:** `WAIT` appears. Execution halts. Pressing ENTER displays `RESUMED` and returns to normal mode.

### T30 — Stop terminates early
**Steps:** Create and run program:
```
Disp "A"
Stop
Disp "B"
```
**Expected:** `A` appears. `B` does not appear. Mode returns to normal after `Stop`.

### T31 — IS>( increment and skip
**Steps:** Create and run program:
```
1->I
IS>(I,2)
Disp "SKP"
Disp I
```
**Expected:** `I` starts at 1; IS>( increments to 2; 2 is NOT > 2, so `SKP` is NOT skipped and appears. `I` displays as `2`.
Run again but pre-set I=2: change `1->I` to `2->I`. Now IS>( increments I to 3; 3 > 2, so `SKP` is skipped. Only `I` (showing `3`) appears.

### T32 — DS<( decrement and skip
**Steps:** Create and run program:
```
3->I
DS<(I,3)
Disp "SKP"
Disp I
```
**Expected:** `I` starts at 3; DS<( decrements to 2; 2 < 3, so `SKP` IS skipped. Only `I` (showing `2`) appears.

### T33 — Subroutine call (prgm) and Return
**Steps:** Create program in slot 2 with body:
```
Disp "SUB"
Return
```
Create program in slot 1 with body:
```
Disp "MAIN"
prgm2
Disp "BACK"
```
Run slot 1.
**Expected:** History shows `MAIN`, then `SUB`, then `BACK` in order.

### T34 — Nested subroutine (2 levels deep)
**Steps:** Create slot 3:
```
Disp "DEEP"
```
Create slot 2:
```
prgm3
Disp "MID"
```
Create slot 1:
```
prgm2
Disp "TOP"
```
Run slot 1.
**Expected:** History shows `DEEP`, `MID`, `TOP` in order.

---

## Executor — I/O Commands

### T35 — Prompt command
**Steps:** Create and run program:
```
Prompt B
Disp B
```
When `B=?` appears, type `42` and ENTER.
**Expected:** Behavior identical to `Input B`. `B=?` prompt shown; `42` displayed; B=42 after.

### T36 — ClrHome clears history
**Steps:** Create and run program:
```
Disp "LINE1"
Disp "LINE2"
ClrHome
Disp "AFTER"
```
**Expected:** After `ClrHome`, `LINE1` and `LINE2` are cleared from the display. Only `AFTER` remains visible.

### T37 — DispGraph switches to graph view
**Steps:** Ensure at least one Y= equation is entered (e.g. `Y1=X`). Create and run program:
```
DispGraph
Pause
DispHome
```
**Expected:** Graph canvas appears after `DispGraph`. Pressing ENTER at `Pause` switches back to the calculator home screen via `DispHome`.

### T38 — Output( writes at position
**Steps:** Create and run program:
```
Output(1,1,"HELLO")
Output(4,5,"WORLD")
Pause
```
**Expected:** `HELLO` appears at the top-left of the display (row 1, col 1). `WORLD` appears further down (row 4, col 5). Pause holds the display until ENTER.

### T39 — Menu( interactive selection
**Steps:** Create and run program:
```
Menu("PICK","YES",YS,"NO",NS)
Lbl YS
Disp "YES"
Goto DONE
Lbl NS
Disp "NO"
Lbl DONE
Stop
```
Run twice: once select option 1 (`YES`), once select option 2 (`NO`).
**Expected:** First run: menu overlay shows title `PICK` with `1:YES` and `2:NO`; selecting 1 (UP/ENTER or key `1`) jumps to `Lbl YS` and displays `YES`. Second run: selecting 2 displays `NO`.

### T40 — Menu( CLEAR aborts program
**Steps:** While a `Menu(` overlay is visible (mid-execution of T39 program), press CLEAR.
**Expected:** Menu overlay closes. Program execution halts. Calculator returns to normal home screen. No lockup.

---

## Cursor Behavior in PRGM Screens

### T41 — Cursor blinks in editor
**Steps:** Open any program in the editor. Observe the cursor for ~2 seconds without pressing any key.
**Expected:** Cursor block blinks at ~530 ms interval on the current line.

### T42 — 2nd mode reflected in editor cursor
**Steps:** With editor open, press 2nd.
**Expected:** Cursor immediately turns **amber** with `^` inside. Pressing any non-modifier key returns cursor to white block (2nd mode consumed).

### T43 — ALPHA mode reflected in editor cursor
**Steps:** With editor open, press ALPHA.
**Expected:** Cursor immediately turns **green** with `A` inside. Pressing a letter key inserts the letter and cursor returns to white. Pressing ALPHA again cancels ALPHA mode.

### T44 — Cursor blinks in name-entry screen
**Steps:** Open name-entry screen (EDIT tab → empty slot → ENTER). Observe for ~2 seconds.
**Expected:** Cursor on name-entry screen blinks at ~530 ms interval.

---

## Notes

- All tab and item highlights are **yellow** (`0xFFFF00`). Scroll indicators are **amber** (`0xFFAA00`). If you see amber on a tab or item cursor (not an arrow), that is a regression.
- T08 digit entry: digits are typed without ALPHA (normal key layer). ALPHA re-engages after each digit so the next key can be a letter without re-pressing ALPHA.
- T07 / T10 distinction: T07 skips the name but opens the editor; T10 cancels and returns to the menu. Both discard any partially-typed name.
- T16 ERASE: only named programs (non-empty `names[]`) appear in the ERASE tab. A program edited via T07 (no user name) will NOT appear in ERASE even if it has body code.
- T19 empty-slot execution: the executor receives an empty body string and terminates immediately; this should be silent with no output or error.
- T24 may produce several `X` rows before CLEAR is processed (queue latency); that is acceptable.
- T17 requires a full power cycle (USB unplug/replug), not just SWD reset, to verify FLASH persistence. 2nd+ON saves state first.
- T13 CTL sub-menu: opened with the PRGM key from the editor (not MATH). 14 items total; items 9–14 require scrolling below the 7th row. Overflow indicators (↑/↓) apply.
- T14 I/O sub-menu: reached by pressing LEFT or RIGHT from within the CTL sub-menu. 6 items fit without scrolling.
- T33/T34 subroutine: maximum call stack depth is 4; exceeding it causes the `prgm` call to be treated as a no-op (no crash).
- T38 Output(: `row` and `col` are 1-based. `row` must be 1–8; `col` 1–N. Out-of-range values clip silently.
- Keypad alpha-layer mapping: T=key for X position, E=key for LOG position, etc. Verify against physical keypad sticker if uncertain.
