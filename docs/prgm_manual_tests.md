# PRGM System — Manual Test Plan

Hardware: STM32F429I-DISC1. Run after flashing the latest build.
Pass criterion listed for each test. Mark ✅ / ❌ / ⚠️.

Last updated: 2026-03-28 (Session 30: new RESULT fields added for re-validation run)

---

## Menu & Navigation

### T01 — Open PRGM menu
**Steps:** From calculator home, press PRGM.
**Expected:** PRGM menu opens showing `EXEC  EDIT  ERASE` tab bar. EXEC tab highlighted **yellow**. All 37 program slots listed in format `1:Prgm1`, `2:Prgm2`, … `9:Prgm9`, `0:Prgm0`, `A:PrgmA`, … `θ:Prgmθ`. Programs that have been given a user name show it in a second column: `1:Prgm1  MYNAME`.
PREV RESULT: PASS
RESULT: PASS

### T02 — Tab navigation (with wrap)
**Steps:** With PRGM menu open, press RIGHT twice.
**Expected:** Active tab advances: EXEC → EDIT → ERASE. Tab highlight (yellow) moves. EXEC, EDIT, and ERASE all show all 37 slots in the same `N:PrgmN` format (with optional user name column). Named slots show `N:PrgmN  NAME` in all three tabs.
Also verify wrap: pressing LEFT at EXEC wraps to ERASE; pressing RIGHT at ERASE wraps to EXEC.
PREV RESULT: PASS
RESULT: PASS

### T03 — Scroll indicators on EXEC/EDIT
**Steps:** Open PRGM menu on EXEC or EDIT tab. Press DOWN repeatedly past the 7th visible slot.
**Expected:** `↓` amber indicator appears at the bottom of the list when more items exist below. `↑` amber indicator appears at the top once scrolled past the first slot. Indicators disappear when scrolled to the respective boundary.
PREV RESULT: PASS
RESULT: PASS

### T04 — Close PRGM menu
**Steps:** With PRGM menu open, press CLEAR.
**Expected:** PRGM menu closes, calculator home screen returns. Expression buffer unchanged.
PREV RESULT: PASS
RESULT: PASS

### T05 — Open PRGM menu from MATH menu
**Steps:** Open MATH menu (MATH key), then press PRGM.
**Expected:** MATH menu closes, PRGM menu opens on EXEC tab. No display corruption.
PREV RESULT: PASS
RESULT: PASS

---

## Name Entry & Program Creation

### T06 — Create new program via EDIT tab (with name)
**Steps:** PRGM → RIGHT (EDIT tab) → navigate to an empty slot (e.g., `1:Prgm1`) → ENTER.
**Expected:** Name-entry screen appears showing `Prgm1:` with cursor. Alpha mode auto-engaged (green cursor with `A` inside).
Type `TEST` (T, E, S, T on ALPHA layer). Then ENTER.
**Expected:** `Prgm1:TEST` updates as each letter is typed. On ENTER, editor opens with title `Prgm1  TEST` and one blank `:` line with cursor.
PREV RESULT: SEE NOTE Regarding navigation while entering program names
RESULT: PASS

### T06a — Name-entry LEFT/RIGHT cursor navigation (F10)
**Steps:** On name-entry screen with `ABCD` typed, press LEFT twice.
**Expected:** Cursor moves back two characters (now between `AB` and `CD`). Press `X` (on ALPHA layer).
**Expected:** `X` is inserted at the cursor: field now reads `ABXCD`. Press RIGHT RIGHT — cursor moves to end. DEL removes `D`.
RESULT: Partial fail. while this does exactly as noted in expectation of T06a, that is undesireable. the cursor does not show that it's in insert mode, it shows default mode blinking alpha cursor indicating overwrite with alpha

### T07 — Create new program — skip name (name is optional)
**Steps:** PRGM → RIGHT (EDIT tab) → select an empty slot → ENTER → immediately press ENTER again (no name typed).
**Expected:** Editor opens with title `Prgm<N>` (no user name). The slot still appears in the ERASE tab (all 37 slots are always shown). Program body can be edited normally.
PREV RESULT: PASS
RESULT: PASS

### T08 — Digits allowed in program name
**Steps:** PRGM → RIGHT (EDIT tab) → select an empty slot → ENTER → type `A1B2` (ALPHA+A, then 1, then ALPHA+B, then 2) → ENTER.
**Expected:** Each character appended: `A`, `1`, `B`, `2`. Digits `1` and `2` typed without requiring ALPHA. On ENTER, editor opens with title showing `A1B2` as the user name.
PREV RESULT: SEE NOTE Regarding navigation while entering program names
RESULT: FAIL

### T08a — Name-entry DOWN navigates to editor body; UP returns to name (F10)
**Steps:** On name-entry screen with `MYTEST` typed, press DOWN.
**Expected:** Editor body opens (no name save dialog). First program line `:` is visible with cursor. Title shows `PrgmN  MYTEST`.
Now press UP from editor line 0 (with cursor at col 0).
**Expected:** Name-entry screen re-appears with `MYTEST` intact. Cursor positioned at end of name.
RESULT: Partial FAIL. while this does work properly upon initial progarm edit and save, upon re-entry of a previously saved program the program name is inaccessable.

### T09 — Name entry DEL
**Steps:** From name-entry screen, type two or three letters, then press DEL.
**Expected:** Last character removed with the **first** DEL press (not the second). Cursor moves back. ALPHA mode re-engages automatically. Next keypress continues inserting a letter without re-pressing ALPHA.
PREV RESULT: FAIL Alpha lock does not re-engage. User must press delete then press ALPHA then it proceeds as described. After pressing DEL the cursor returns to non-alpha but if the user presses 2nd+ALPHA in this state all characters are sent to calculator screen.
RESULT: PASS 

### T09d — 2nd+ALPHA (ALPHA_LOCK) in name entry routes to name field (F5b)
**Steps:** From name-entry screen, press 2nd+ALPHA to engage ALPHA_LOCK.
**Expected:** Cursor turns green with `A` inside. Typing letters inserts them into the **name field** (not the calculator expression). ALPHA_LOCK remains until ALPHA is pressed again.
RESULT: Partial PASS. pressing ALPHA while alpha lock is active does not remove alpha lock as expected

### T09b — ENTER works on first press after name
**Steps:** From name-entry screen, type a name (e.g. `MYTEST`). Press ENTER exactly once.
**Expected:** Editor opens on the **first** ENTER press. A second ENTER press is not required.
PREV RESULT: PASS
RESULT: PASS

### T09c — CLEAR works on first press in name entry
**Steps:** From name-entry screen with letters typed (alpha mode engaged), press CLEAR.
**Expected:** Returns to PRGM menu on EDIT tab on the **first** CLEAR press. No name or body saved. A second CLEAR press is not required.
PREV RESULT: PASS
RESULT: PASS

### T10 — Cancel name entry (empty)
**Steps:** From name-entry screen (no letters typed), press CLEAR.
**Expected:** Returns to PRGM menu on EDIT tab. No name or body saved.
PREV RESULT: PASS
RESULT: PASS

### T11 — Open existing program from EDIT tab
**Steps:** With at least one named program, open PRGM → EDIT tab → navigate to that slot (shows `N:PrgmN  NAME`) → ENTER.
**Expected:** Editor opens directly (no name-entry screen). Title shows `PrgmN  NAME`.
PREV RESULT: PASS
RESULT: NOTE while this test does exactly as T11 expects, i would like to simplify it to just always open the same way, with the program name entry active

### T11b — EDIT tab digit shortcut
**Steps:** Open PRGM → RIGHT (EDIT tab). Press `1` without pressing UP/DOWN.
**Expected:** The editor for slot 1 (Prgm1) opens immediately. A second keypress or ENTER is not required.
PREV RESULT: FAIL -  while this does work with numeric shortcut keys it fails when the user presses ALPHA+LETTER instead of activating the shortcut for them
RESULT: PASS

---

## Editor

### T12 — Editor cursor and character input
**Steps:** Open editor for any program slot. Type `A+1` (TOKEN_A in ALPHA layer, TOKEN_ADD, TOKEN_1).
**Expected:** Characters appear on the current (yellow-highlighted) line as `:A+1`. Cursor advances with each keypress.
PREV RESULT: PASS
RESULT: PASS

### T12b — Insert mode off by default
**Steps:** Open any program in the editor. Without pressing INS, type a character at a non-end cursor position (navigate LEFT first).
**Expected:** The character at the cursor is **overwritten** (not inserted), matching the default overwrite behavior of the main calculator entry. The cursor does not push following characters to the right unless INS was pressed.
PREV RESULT: FAIL - insert stuck on, unable to deactivate. Also the cursor does not indicate that it is in INSERT mode. See new cursor issue note
RESULT:

### T13 — CTL sub-menu inserts keyword
**Steps:** In editor, press PRGM (opens CTL sub-menu). Navigate to `3:If ` and press ENTER (or press `3`).
**Expected:** CTL menu closes. Editor line now contains `:If ` with cursor after it.
Also verify: CTL menu has exactly **8 items**, all fitting without scrolling:
`1:Lbl`, `2:Goto`, `3:If`, `4:IS>(`, `5:DS<(`, `6:Pause`, `7:End`, `8:Stop`.
PREV RESULT: PASS
RESULT:

### T14 — I/O sub-menu inserts keyword
**Steps:** In editor, press PRGM (CTL sub-menu opens). Press LEFT or RIGHT to switch to I/O tab. Navigate to `1:Disp ` and press ENTER (or press `1`).
**Expected:** I/O menu closes. Editor line contains `:Disp ` with cursor after it.
I/O menu has exactly **5 items**, all fitting without scrolling:
`1:Disp`, `2:Input`, `3:DispHome`, `4:DispGraph`, `5:ClrHome`.
PREV RESULT: PASS
RESULT:

### T15 — Multi-line editing and scroll
**Steps:** In editor, press ENTER six times to create 7+ lines. Keep pressing DOWN.
**Expected:** `↓` amber indicator appears on the last visible line when lines extend beyond 7 rows. `↑` appears when scrolled past row 0. Title stays fixed. Lines scroll correctly.
PREV RESULT: PASS
RESULT:

### T16 — Erase a program via ERASE tab
**Steps:** Go to PRGM → RIGHT RIGHT (ERASE tab). All 37 slots are visible. Navigate to any slot (named or unnamed) → ENTER.
**Expected:** Confirmation dialog appears showing the slot title and `1:Do not erase` / `2:Erase`. Press `2` directly — the slot is erased **immediately** (no extra ENTER required). Press `1` directly to cancel immediately. Program body and name cleared; slot shows bare `N:PrgmN` format again.
PREV RESULT: FAIL while this does operate as explained here - it fails to operate when the menu shortcuts are entered to select the program slot to erase. Any time there is a menu with shortcuts shown they should be exactly the same as navigating to that item and pressing enter
RESULT:

### T16b — ERASE shows all 37 slots (not just named ones)
**Steps:** Ensure some program slots have names and others are empty. Open PRGM → ERASE tab.
**Expected:** All 37 slots are visible — both named and unnamed. Named slots show `N:PrgmN  NAME`; unnamed slots show only `N:PrgmN`. Slot count does not change between EXEC, EDIT, and ERASE tabs.
PREV RESULT: PASS
RESULT:

---

## Persistence

### T17 — Programs survive 2nd+ON / power cycle
**Steps:** Create a program named `SAVE` in slot 1 with body `Disp "OK"`. Press 2nd+ON to save state. Power-cycle the board (USB unplug/replug). Open PRGM.
**Expected:** `1:Prgm1  SAVE` appears in EXEC and EDIT lists. Body intact when opened in editor.
PREV RESULT: PASS
RESULT:

---

## Executor — Basic Execution

### T18 — Run a Disp program via prgmNAME model
**Steps:** Ensure slot 1 has a program named `HELLO` with body:
```
Disp "HELLO"
```
From calculator home, open PRGM → EXEC tab → navigate to slot 1 → ENTER.
**Expected:** PRGM menu closes. Calculator home shows `prgmHELLO` in the expression buffer (left-aligned, grey text). Press ENTER.
**Expected (after ENTER):** `HELLO` appears in history output. History expression row shows `prgmHELLO` (grey, left-aligned). After execution, `Done` appears as the result row (white, right-aligned). Mode returns to normal.
PREV RESULT: PASS
RESULT:

### T19 — Run an empty slot (no-op)
**Steps:** In EXEC tab, navigate to a slot with no body (shows `N:PrgmN` with no user name column). Press ENTER.
**Expected:** PRGM menu closes. Calculator home shows `prgmN` in the expression buffer. Press ENTER.
**Expected:** `Done` appears as result row. No error. No crash or lockup.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 15 (test_empty_body) — empty-body execution covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

### T20 — Expression evaluation and ANS
**Steps:** Create and run program:
```
2+2
Disp ANS
```
**Expected:** `4` appears in history output. ANS is 4 after program completes.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 9 (test_general_expr) — expression evaluation and ANS update covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

### T21 — Input and variable store
**Steps:** Create and run program:
```
Input A
Disp A
```
When `?` prompt appears, type `7` and press ENTER.
**Expected:** `?` shown as expression row (variable name **not** shown — original TI-81 behaviour). After ENTER, `7` shown as result. `7` displayed by Disp. A=7 in variable store after.
PREV RESULT: SEE NOTE regarding 'Input' program command
HOST COVERAGE: test_prgm_exec Group 11 (test_input_prompt) partial — suspension and variable store covered; `?` prompt display and variable readback require hardware.
RESULT:

### T22 — If single-line skip
**Steps:** Create and run program:
```
0->A
If A=1
Disp "YES"
Disp "DONE"
```
**Expected:** `YES` does NOT appear (condition false → next line skipped). `DONE` appears. Mode returns to normal.
PREV RESULT: FAIL - Unable to enter the TEST menu to select a conditional operator to insert. Also unable to enter the MATH menu
HOST COVERAGE: test_prgm_exec Group 2 (test_if_single) — If skip/execute logic covered; TEST menu access from editor requires hardware.
RESULT:

### T23 — EXEC number-key shortcut
**Steps:** Open PRGM menu on EXEC tab. Press `1` without using UP/DOWN.
**Expected:** PRGM menu closes. Calculator home shows `prgm1` (or `prgmNAME` if slot 1 is named) in the expression buffer — same as navigating to slot 1 and pressing ENTER.
PREV RESULT: PASS
RESULT:

### T24 — CLEAR aborts execution
**Steps:** Create and run program:
```
Lbl A
Disp "X"
Goto A
```
While `X` rows are appearing, press CLEAR.
**Expected:** Execution stops immediately. Calculator returns to normal home screen. No lockup.
PREV RESULT: FAIL - screen goes black then becomes unresponsive. Note that the heartbeat red LED begins to flash much slower than in normal operation. User is able to exit this state but when it returns there is no graphical response to keys being pressed. The green LED does still toggle with each keystroke. Requires hard reset to exit this state
RESULT:

---

## Executor — Advanced Control Flow

### T25 — Lbl/Goto single-char label enforcement
**Steps:** In editor, insert `Lbl ` from CTL menu. Type one letter (e.g. `A`). Try to type a second letter.
**Expected:** Only the first character is accepted. Typing a second character on the same line is ignored. Line reads `:Lbl A`.
PREV RESULT: PASS
RESULT: this occurs as T25 explains but it's an undesireable extra constraint beyond original constraints. allow text entry without limit but throw an error upon execution if the program attempts to find a multicharacter label. 

### T26 — Disp string alignment (left)
**Steps:** Create and run program:
```
Disp "HELLO"
```
**Expected:** `HELLO` appears **left-aligned** (in the expression row, grey text). No result row below it.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 10 (test_disp) — string→expression slot covered; visual grey/left rendering requires hardware.
RESULT:

### T27 — Disp variable alignment (right)
**Steps:** Create and run program:
```
5->A
Disp A
```
**Expected:** `5` (or `5.000000` formatted) appears **right-aligned** (in the result row, white text). No expression row above the result for the Disp output.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 10 (test_disp) — variable→result slot covered; visual white/right rendering requires hardware.
RESULT:

### T28 — Goto/Lbl jump
**Steps:** Create and run program:
```
Goto END
Disp "SKIP"
Lbl END
Disp "DONE"
```
**Expected:** `SKIP` does not appear. `DONE` appears once.
PREV RESULT: PASS even though this instruction is incorrectly showing multi-character labels. Program properly allows only single character
HOST COVERAGE: test_prgm_exec Group 1 (test_goto_lbl) — Goto/Lbl jump and skip logic covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

### T29 — Pause halts and resumes on ENTER
**Steps:** Create and run program:
```
Disp "WAIT"
Pause
Disp "RESUMED"
```
**Expected:** `WAIT` appears. Execution halts. Pressing ENTER displays `RESUMED` and returns to normal mode.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 7 (test_stop_pause_return) partial — suspension covered; `WAIT`/`RESUMED` display and ENTER resume require hardware.
RESULT:

### T30 — Stop terminates early
**Steps:** Create and run program:
```
Disp "A"
Stop
Disp "B"
```
**Expected:** `A` appears. `B` does not appear. Mode returns to normal after `Stop`.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 7 (test_stop_pause_return) — Stop halt and post-Stop skip fully covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

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
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 6 (test_is_ds) — IS>( increment, skip, and threshold boundary fully covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

### T32 — DS<( decrement and skip
**Steps:** Create and run program:
```
3->I
DS<(I,3)
Disp "SKP"
Disp I
```
**Expected:** `I` starts at 3; DS<( decrements to 2; 2 < 3, so `SKP` IS skipped. Only `I` (showing `2`) appears.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 6 (test_is_ds) — DS<( decrement, skip, and threshold boundary fully covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

### T33 — Subroutine auto-return at end of body
**Steps:** Create program in slot 2 with body:
```
Disp "SUB"
```
Create program in slot 1 with body (use PRGM → EXEC tab to insert `prgm2`):
```
Disp "MAIN"
prgm2
Disp "BACK"
```
To insert `prgm2`: in the editor on the second line, press PRGM → navigate RIGHT to EXEC tab → navigate to slot 2 → ENTER. Confirm `:prgm2` appears on that line.
Run slot 1.
**Expected:** History shows `MAIN`, then `SUB`, then `BACK` in order. The subroutine returns automatically when its last line is reached (no explicit Return needed).
PREV RESULT: FAIL - EXEC menu entirely missing from PRGM menus while in program edit mode
HOST COVERAGE: test_prgm_exec Group 13 (test_subroutine) — subroutine call, auto-return, and caller resume fully covered; EXEC sub-menu insertion requires hardware.
RESULT:

### T33b — EXEC sub-menu tab navigation and insertion (F3)
**Steps:** Open any program in the editor. Press PRGM to open the CTL sub-menu. Press RIGHT to switch to I/O tab. Press RIGHT again.
**Expected:** EXEC tab is highlighted (third tab). A scrollable list of all 37 slots is shown (`N:PrgmN` or `N:PrgmN  NAME`). Navigate to slot 2 → ENTER.
**Expected:** EXEC menu closes. Editor line now contains `:prgm2` (or `:prgmNAME` if slot 2 is named). Press PRGM → LEFT from CTL: wraps to EXEC tab.
RESULT:

### T34 — Nested subroutine (2 levels deep)
**Steps:** Create slot 3:
```
Disp "DEEP"
```
Create slot 2 (insert `prgm3` via PRGM → EXEC tab):
```
prgm3
Disp "MID"
```
Create slot 1 (insert `prgm2` via PRGM → EXEC tab):
```
prgm2
Disp "TOP"
```
Run slot 1.
**Expected:** History shows `DEEP`, `MID`, `TOP` in order. Each subroutine auto-returns when its body is exhausted.
PREV RESULT: FAIL - EXEC menu entirely missing from PRGM menus while in program edit mode
HOST COVERAGE: test_prgm_exec Group 17 (test_nested_subroutine) — 2-level call chain and stack-overflow no-crash fully covered; EXEC sub-menu insertion requires hardware.
RESULT:

---

## Executor — I/O Commands

### T35 — prgmNAME execution model (named program)
**Steps:** Open PRGM → EXEC tab → navigate to a named program (e.g. `1:Prgm1  TEST`) → ENTER.
**Expected:** PRGM menu closes. Calculator home screen shows `prgmTEST` in the expression buffer (left-aligned, live input). Press ENTER.
**Expected (after ENTER):** Program executes. History shows `prgmTEST` as expression row (grey, left-aligned). After execution completes, `Done` appears as the result row (white, right-aligned). Mode returns to normal.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 16 (test_lookup_slot) partial — name→slot lookup covered; expression-buffer insertion and `Done` display require hardware.
RESULT:

### T35b — prgmNAME execution model (unnamed slot)
**Steps:** Open PRGM → EXEC tab → navigate to an unnamed slot (e.g. `3:Prgm3`, no user name) → ENTER.
**Expected:** PRGM menu closes. Calculator home shows `prgm3` (the canonical slot ID, not a user name). Press ENTER. Program executes (or is silent if empty). `Done` appears as result row.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 16 (test_lookup_slot) partial — canonical ID lookup covered; expression-buffer insertion and `Done` display require hardware.
RESULT:

### T36 — ClrHome clears history
**Steps:** Create and run program:
```
Disp "LINE1"
Disp "LINE2"
ClrHome
Disp "AFTER"
```
**Expected:** After `ClrHome`, `LINE1` and `LINE2` are cleared from the display. Only `AFTER` remains visible.
PREV RESULT: PASS
HOST COVERAGE: test_prgm_exec Group 12 (test_clrhome) — history_count reset and post-ClrHome Disp fully covered; skip if host suite passes and no prgm_exec.c changes.
RESULT:

### T37 — DispGraph switches to graph view
**Steps:** Ensure at least one Y= equation is entered (e.g. `Y1=X`). Create and run program:
```
DispGraph
Pause
DispHome
```
**Expected:** Graph canvas appears after `DispGraph` **without a lockup or hardware reset**. Pressing ENTER at `Pause` switches back to the calculator home screen via `DispHome`.
PREV RESULT: FAIL - DispGraph appears to lock up. LED Heartbeat appears to slow
RESULT:

---

## Editor Alpha Behavior

### T41 — Cursor blinks in editor
**Steps:** Open any program in the editor. Observe the cursor for ~2 seconds without pressing any key.
**Expected:** Cursor block blinks at ~530 ms interval on the current line.
PREV RESULT: PASS
RESULT:

### T42 — 2nd mode reflected in editor cursor
**Steps:** With editor open, press 2nd.
**Expected:** Cursor immediately turns **amber** with `^` inside. Pressing any non-modifier key returns cursor to white block (2nd mode consumed).
PREV RESULT: PASS
RESULT:

### T43 — ALPHA mode (single) in editor
**Steps:** With editor open, press ALPHA once.
**Expected:** Cursor turns **green** with `A` inside. Pressing a letter key inserts the letter into the program body (not the calculator expression) and cursor returns to white. Pressing ALPHA again cancels ALPHA mode.
PREV RESULT: FAIL - Pressing ALPHA once operates as expected and allows a single alpha character to be entered. pressing alpha 2 times in a row does not cause it to exit alpha mode
RESULT:

### T43b — ALPHA_LOCK mode in editor
**Steps:** With editor open, press 2nd then ALPHA (engages ALPHA_LOCK).
**Expected:** Cursor shows green `A`. Multiple letter keys pressed in sequence all insert letters into the **program body** (not the calculator expression). ALPHA_LOCK remains active until ALPHA is pressed again to cancel it. Digits can be typed without exiting ALPHA_LOCK.
PREV RESULT: PASS
RESULT:

### T44 — Cursor blinks in name-entry screen
**Steps:** Open name-entry screen (EDIT tab → empty slot → ENTER). Observe for ~2 seconds.
**Expected:** Cursor on name-entry screen blinks at ~530 ms interval.
PREV RESULT: PASS
RESULT:

---

## Editor Body Behavior

### T45 — Body-only slot opens editor directly
**Steps:** Open the editor for any slot, type some program content (e.g. `Disp "HI"`), then close without saving a name (press CLEAR from name-entry to skip the name). Reopen PRGM → EDIT tab → navigate to that same slot.
**Expected:** The slot has body content but no user name. Pressing ENTER opens the editor directly — the name-entry screen does NOT appear. Existing body content is preserved.
PREV RESULT: PASS
RESULT:

### T46 — Tab wrap in sub-menus
**Steps:** While in the editor, press PRGM to open the CTL sub-menu. Three tabs are visible: CTL (yellow), I/O, EXEC. Press LEFT from CTL.
**Expected:** Navigation wraps to EXEC (rightmost). Press LEFT again → I/O. Press LEFT again → CTL. Pressing RIGHT from CTL → I/O → EXEC → CTL (wrap).
PREV RESULT: PASS
RESULT:

### T46b — EXEC sub-menu digit and ALPHA+letter shortcuts (F3/F6)
**Steps:** On the EXEC sub-menu tab, press `2` directly.
**Expected:** Menu closes immediately; `:prgm2` inserted into the current line (same as navigating to slot 2 and pressing ENTER).
Now open EXEC tab again. Press ALPHA+A.
**Expected:** `:prgmA` inserted (or `:prgmNAME` if slot A is named). Slot A = slot index 10.
RESULT:

---

## Notes

- All tab and item highlights are **yellow** (`0xFFFF00`). Scroll indicators are **amber** (`0xFFAA00`). If you see amber on a tab or item cursor (not an arrow), that is a regression.
- T09/T09b/T09c: The A2 fix ensures ENTER, DEL, and CLEAR act on their primary function on the **first** keypress even when ALPHA mode is auto-engaged. If any of these require two presses, the fix is not working.
- T09 DEL re-engages ALPHA: Session 29 (F5a) fixed this. After DEL the cursor stays in ALPHA mode — next keypress should insert a letter without pressing ALPHA again.
- T09d 2nd+ALPHA routing: Session 29 (F5b) fixed ALPHA_LOCK routing so letters go to the name field, not the calculator screen.
- T11b/ALPHA+letter shortcuts: Session 29 (F6) added ALPHA+letter (slots 10–35) and ALPHA+θ (slot 36) shortcuts to all three PRGM tabs (EXEC, EDIT, ERASE).
- T12b insert mode: Session 29 (F8) added INS key to toggle insert mode in the editor. Default is overwrite. In insert mode the cursor renders as a 3 px underline (not `I` in a box — that was the old incorrect rendering).
- T16 ERASE tab shortcuts: Session 29 (F7) extended digit and ALPHA+letter shortcuts to the ERASE tab — selecting a slot immediately shows the confirmation dialog.
- T21 Input prompt: Session 29 (F9) changed `cmd_input()` to always show `?` only. The `A=?` format is gone. This matches the original TI-81 behaviour.
- T22 TEST/MATH in editor: Session 29 (F4) wired TOKEN_TEST and TOKEN_MATH in the editor handler. Pressing 2nd+MATH or 2nd+TEST from the editor now opens the respective menu; the selection is inserted into the current program line.
- T24 CLEAR aborts: Session 29 (F1) added `prgm_request_abort()` called from keypadTask + `osDelay(0)` per line. A few `X` rows may appear before CLEAR is processed (queue latency) — that is acceptable.
- T33/T34/T33b EXEC sub-menu: Session 29 (F3) added the EXEC tab to the in-editor CTL/IO sub-menu. Three tabs now: CTL (default) → I/O → EXEC. Use PRGM → RIGHT → RIGHT to reach EXEC. Slot picker works like the main PRGM EXEC tab.
- T37 DispGraph: Session 29 (F2) added `osDelay(20)` before `Graph_Render` in `cmd_dispgraph`. Should complete without lockup.
- T06a/T08a Name-entry arrow navigation: Session 29 (F10) implemented LEFT/RIGHT cursor movement, DOWN transitions to editor body, UP at editor line 0 returns to name-entry screen.
- T43b ALPHA_LOCK: Session 26 (A6) fixed routing so ALPHA_LOCK keystrokes go to the program editor, not the calculator. Before the fix, letters typed in ALPHA_LOCK went to the calculator expression.
- T45 body-only: Session 26 (D3) fixed detection — a slot with body content but no user name should open the editor directly, not the name-entry screen.
- T25 Lbl/Goto: Session 26 (B4) enforces single-character labels in the editor input handler. Attempting to type a second label character after `Lbl A` is silently ignored.
- CTL menu has exactly **8 items**: `Lbl`, `Goto`, `If`, `IS>(`, `DS<(`, `Pause`, `End`, `Stop`. `Then`, `Else`, `While`, `For(`, `Return`, and `prgm` are **not present** — if seen, that is a regression.
- I/O menu has exactly **5 items**: `Disp`, `Input`, `DispHome`, `DispGraph`, `ClrHome`. `Prompt`, `Output(`, and `Menu(` are **not present** — if seen, that is a regression.
- T17 requires a full power cycle (USB unplug/replug), not just SWD reset, to verify FLASH persistence. 2nd+ON saves state first.
- T33/T34 subroutine: maximum call stack depth is 4; exceeding it causes the `prgm` call to be treated as a no-op (no crash). There is no explicit Return command — subroutines auto-return on the last line.
- Keypad alpha-layer mapping: T=key for X position, E=key for LOG position, etc. Verify against physical keypad sticker if uncertain.
- T35/T35b prgmNAME: Execution model change in Session 26 (C1). Programs are no longer run automatically on EXEC selection; they are inserted into the expression buffer first.
- T12b: Insert mode was on by default in the editor before Session 26 (D1 bug). Default must be overwrite (no shift-right behavior). INS key now correctly toggles it.


Current run notes:
Using arrow keys left and right should not exit alpha lock when entering program name. using arrow key to move down from program name entry should exit alpha lock and reentering the program name field should re-engage alpha lock. insert mode should not be active by default
when editing a program that was saved at a previous time, the name is unable to be edited later. user is prohibited from using the up arrow key to reenter the name field
When entering a program name the cursor behaves unexpectedly. The ALPHA lock is difficult to remove. only certain key press combinations remove it. for example: When already in alpha lock pressing the ALPHA key again does note remove alpha-lock. but if i press 2nd + ALPHA and then press ALPHA one more time it will remove it. expected behavior is that pressing ALPHA key when in alpha-lock will exit alpha-lock mode
What is the error handling system for user programs? develop a plan to handle possible errors