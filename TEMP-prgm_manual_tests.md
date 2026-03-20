# PRGM System — Manual Test Plan

Hardware: STM32F429I-DISC1. Run after flashing the latest build.
Pass criterion listed for each test. Mark ✅ / ❌ / ⚠️.

---

## Menu & Navigation

### T01 — Open PRGM menu
**Steps:** From calculator home, press PRGM.
**Expected:** PRGM menu opens showing `EXEC  EDIT  NEW` tab bar. EXEC tab highlighted amber. Item list below (empty if no programs stored).

### T02 — Tab navigation
**Steps:** With PRGM menu open, press RIGHT twice.
**Expected:** Active tab advances: EXEC → EDIT → NEW. Pressing LEFT returns. Tab highlight moves; item list updates accordingly.

### T03 — Close PRGM menu
**Steps:** With PRGM menu open, press CLEAR.
**Expected:** PRGM menu closes, calculator home screen returns. Expression buffer unchanged.

### T04 — Open PRGM menu from MATH menu
**Steps:** Open MATH menu (MATH key), then press PRGM.
**Expected:** MATH menu closes, PRGM menu opens on EXEC tab. No display corruption.

---

## Name Entry & Program Creation

### T05 — Create new program
**Steps:** PRGM → RIGHT RIGHT (NEW tab) → ENTER.
**Expected:** Name-entry screen appears showing `PROGRAM NAME=` with cursor. Alpha mode auto-engaged (green cursor with `A` inside).

### T06 — Type name and confirm
**Steps:** From name-entry screen, press keys spelling `TEST` (T, E, S, T in ALPHA layer — keys X, STO, S, T). Then ENTER.
**Expected:** Each letter appears after the `=`. On ENTER, editor opens with title `TEST` and one blank line with cursor.

### T07 — Name entry DEL
**Steps:** From name-entry with one or more letters typed, press DEL.
**Expected:** Last character removed. Cursor moves back. ALPHA re-engages after each letter as expected.

### T08 — Cancel name entry
**Steps:** From name-entry screen, press CLEAR.
**Expected:** Returns to PRGM menu on NEW tab. No program created.

---

## Editor

### T09 — Editor cursor and character input
**Steps:** Open editor for a program. Type `A+1` (TOKEN_A, TOKEN_ADD, TOKEN_1).
**Expected:** Characters appear on the current line with `:A+1`. Cursor advances with each keypress.

### T10 — CTL sub-menu inserts keyword
**Steps:** In editor, press MATH (opens CTL menu). Navigate to `If ` (item 1) and press ENTER.
**Expected:** CTL menu closes, editor line now contains `:If ` with cursor after it. Program body updated in store.

### T11 — I/O sub-menu inserts keyword
**Steps:** In editor, press TEST (opens I/O menu). Select `Disp ` (item 3).
**Expected:** I/O menu closes, editor line contains `:Disp ` with cursor after it.

### T12 — Multi-line editing and scroll
**Steps:** In editor, press ENTER six times to create 7+ lines. Keep pressing DOWN.
**Expected:** Scroll indicators (↑/↓) appear when lines extend beyond 7 visible rows. Title stays fixed. Lines scroll correctly.

### T13 — Delete program from EDIT tab
**Steps:** With at least one program in the store, go to PRGM → EDIT tab, select the program, press DEL.
**Expected:** Program removed from list. List shifts up. `count` decrements. Confirm by re-entering PRGM — program gone.

---

## Persistence

### T14 — Programs survive 2nd+ON / power cycle
**Steps:** Create a program named `SAVE` with body `Disp "OK"`. Press 2nd+ON to save. Power-cycle the board. Open PRGM.
**Expected:** `SAVE` appears in EXEC and EDIT lists. Body intact when opened in editor.

---

## Executor — Basic Execution

### T15 — Run a Disp program
**Steps:** Create program `HELLO`:
```
Disp "HELLO"
```
Go to EXEC tab, select `HELLO`, press ENTER.
**Expected:** PRGM menu closes, calculator home screen shows `HELLO` as a result row. Mode returns to normal.

### T16 — Expression evaluation and ANS
**Steps:** Create program `CALC`:
```
2+2
Disp ANS
```
Run from EXEC tab.
**Expected:** `4` appears in history output. ANS is 4 after program completes.

### T17 — Input and variable store
**Steps:** Create program `INPUTT`:
```
Input A
Disp A
```
Run. When `A=?` prompt appears, type `7` and press ENTER.
**Expected:** `A=?` shown as expression row. After ENTER, `7` shown as result. `7` displayed by Disp. A=7 in variable store after.

### T18 — For( loop
**Steps:** Create program `LOOP`:
```
For(N,1,5,1)
Disp N
End
```
Run from EXEC.
**Expected:** Five history rows appear showing `1`, `2`, `3`, `4`, `5` in order. Returns to normal mode.

### T19 — If/Then/Else/End
**Steps:** Create program `BRANCH`:
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

### T20 — CLEAR aborts execution
**Steps:** Create program `INFLOOP`:
```
Lbl A
Disp "X"
Goto A
```
Run from EXEC. While `X` rows are appearing, press CLEAR.
**Expected:** Execution stops immediately. Calculator returns to normal home screen. No lockup.

---

## Notes

- T06 assumes keypad alpha-layer mapping for T=TOKEN_ALPHA+T, E=TOKEN_ALPHA+E etc. Verify against hardware keypad layout.
- T20 may produce several `X` rows before CLEAR is processed (queue latency); that is acceptable.
- T14 requires a full power cycle (USB unplug/replug), not just SWD reset, to verify FLASH persistence.
