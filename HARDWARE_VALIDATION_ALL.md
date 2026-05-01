# Neo-81 — Full Hardware Validation Checklist
_Generated 2026-04-18. Updated 2026-04-28 to include all commits through cf931ab._
_All features implemented; none yet verified on hardware._
_Mark each test: ✅ PASS  ❌ FAIL  ⚠️ PARTIAL_

---

## Pre-flight (do before any section)

- [ ] Latest firmware builds with 0 errors
- [ ] All host tests pass (`cmake -S App/Tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`)
- [ ] Flash to board; power-cycle (USB unplug/replug — not just SWD reset)

---
Manual Notes:
The INS button is not accessed using a 2nd key press. it is a standalone key with no second function.
Trace values display have what appears to be an overlap of a LVGL item on top of the X= values. it looks like a small rectangle that has no data inside of it slightly opaque over valid numbers from the trace.
Some menus are buggy like the Parametric mode RANGE menu. Reexamine how menus work to refactor a full scale solution. They should be flexible enough to scroll as needed, and wrap to other side of screen when using tabs. For example the PRGM menu tab wrap
When using STO> key pressing the button: X|T doesn't place the letter in the input area in the same manner as using ALPHA X
In Y= menu the toggle of the equal sign to enable and disable the function from graphing is not easy to see. compare against the original calculator system and recommend options to improve visibility and align with original.
The text entry cursor still seems to be buggy. Sometimes there is no cursor at all like in the MODE menu, highlighting is sometimes helpful but not preferred. Compare against original calculator functionality and make recommendations to align with that spec and improve from current status. Reexamine cursor processes to propose a refactor to unify across whole calculator user experience.
---

## Section 1 — Cursor rendering (P28)
_Verifies `cursor_render()` refactor produces zero visible behaviour change across all 7 editors._

### Main expression cursor

| # | Action | Expected | Result |
|---|---|---|---|
| 1 | Power on, wait for home screen | Cursor blinks grey block on expression row | |
| 2 | Press `2ND` | Cursor turns amber with `^` inside | |
| 3 | Press any function key (e.g. `SIN`) | Cursor resets to grey block | |
| 4 | Press `ALPHA` | Cursor turns green with `A` inside | |
| 5 | Press any letter key (e.g. `A`) | Letter inserted, cursor resets to grey | |
| 6 | Press `STO→` | Cursor turns green with `A` inside (STO-pending state) | |
| 7 | While STO-pending, press any letter | Stores to that variable, cursor resets to grey block | |
| 8 | Press `2ND+INS` | Cursor changes to underscore style | |
| 9 | Press `2ND+INS` again | Cursor returns to full-height block | |

---
Manual Notes:
Tasks 1-9 above pass
---


### Y= editor

| # | Action | Expected | Result |
|---|---|---|---|
| 10 | Press `Y=` | Cursor blinks on first equation field | |
| 11 | Press `2ND` | Cursor turns amber with `^` | |
| 12 | Press any key to resolve 2ND | Cursor resets | |
| 13 | Press `2ND+INS` | Cursor changes to underscore style | |
| 14 | Press `2ND+INS` again | Cursor returns to block | |

---
Manual Notes:
Tasks 10-14 above pass with the universal note again that the INS button does not need 2nd key
---


### RANGE editor

| # | Action | Expected | Result |
|---|---|---|---|
| 15 | Press `RANGE` | Cursor blinks in Xmin field | |
| 16 | Press `2ND+INS` | Cursor changes to underscore style | |
| 17 | Press `2ND+INS` again | Cursor returns to block | |
---
Manual Notes:
Tasks 15-17 above pass
---



### ZOOM FACTORS editor

| # | Action | Expected | Result |
|---|---|---|---|
| 18 | `ZOOM` → Select Factors | Cursor blinks in XFact field | |
| 19 | Press `2ND+INS` | Cursor changes to underscore style | |
| 20 | Press `2ND+INS` again | Cursor returns to block | |
---
Manual Notes:
Tasks 18-20 above pass
---



### Matrix editor — insert mode must NOT change cursor shape

| # | Action | Expected | Result |
|---|---|---|---|
| 21 | `MATRX` → EDIT → select matrix → navigate to a cell | Cursor is full-height grey block | |
| 22 | Press `2ND+INS` | Cursor shape does **not** change | |
| 23 | Press `2ND` | Cursor turns amber with `^` | |
| 24 | Press `ALPHA` | Cursor turns green with `A` | |
---
Manual Notes:
Tasks 21, 23 above pass
Task 22 fails
Task 24 doesn't work as expected
---


### PRGM name entry — insert mode must NOT change cursor shape

| # | Action | Expected | Result |
|---|---|---|---|
| 25 | `PRGM` → EDIT → empty slot → ENTER | Cursor blinks in name field; full-height block | |
| 26 | Press `2ND+INS` | Cursor shape does **not** change | |
---
Manual Notes:
Task 25 above pass
Task 26 i don't understand what is meant in this test
---


### PRGM line editor

| # | Action | Expected | Result |
|---|---|---|---|
| 27 | `PRGM` → EDIT → select a program | Cursor blinks on program line | |
| 28 | Press `2ND+INS` | Cursor changes to underscore style | |
| 29 | Press `2ND+INS` again | Cursor returns to block | |
---
Manual Notes:
Tasks 27-29 above pass
---



---

## Section 2 — Parametric graphing (P35h)
_Validates MODE→Param mode end-to-end._

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | `MODE` → row 4 → select `Param` → press `Y=` | Y= screen shows `X₁t=` / `Y₁t=` layout (not `Y₁=`) | |
| 2 | In param Y=, enter `cos(T)` for X₁t and `sin(T)` for Y₁t using `T` key | Equation accepted; T token inserted | |
| 3 | Set `RANGE`: Tmin=0, Tmax=6.28, Tstep=0.13, Xmin=-2, Xmax=2, Ymin=-2, Ymax=2 → press `GRAPH` | Circle rendered on graph canvas | |
| 4 | Press `RANGE` while in param mode | Exactly 9 fields shown: Tmin, Tmax, Tstep, Xmin, Xmax, Xscl, Ymin, Ymax, Yscl | |
| 5 | Press `TRACE` and use LEFT/RIGHT | Readout shows `T=` / `X=` / `Y=` (not just `X=`/`Y=`) | |
| 6 | `2nd+ON` to save → power-cycle → enter param Y= | X₁t/Y₁t equations intact | |
---
Manual Notes:
Tasks 1-2 above pass
Task 3 fails. 
Bug: the Range screen in Param mode does not scroll to allow user to see all values. 
Task 4 fails. The screen only displays the values that fit on screen. ending with Ymin and doesn't allow scrolling.
Task 5 fails. was unable to get anything to display on graph.
Tas 6 fails.
---


---

## Section 3 — DRAW menu (P29h)
_Validates `2nd+PRGM` draw overlay commands._

**Setup:** Ensure a Y= equation is entered (e.g. Y₁=X) and GRAPH has been rendered at least once so the graph canvas exists.

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `2ND+PRGM` | DRAW menu opens with 7 items: `ClrDraw`, `Line(`, `PT-On(`, `PT-Off(`, `PT-Chg(`, `DrawF`, `Shade(`. Digit shortcuts `1`–`7` work; UP/DOWN navigate; CLEAR exits. | |
| 2 | Type `ClrDraw` from expression buffer (via DRAW menu → item 1) → ENTER | Draw layer cleared; result row shows `Done` | |
| 3 | Type `Line(0,0,5,5)` → ENTER | Diagonal line drawn on graph canvas from (0,0) to (5,5) | |
| 4 | Type `PT-On(2,3)` → ENTER | Single pixel set at graph coordinates (2,3) | |
| 5 | Type `PT-Off(2,3)` → ENTER | That pixel cleared | |
| 6 | Type `PT-Chg(2,3)` → ENTER twice | Pixel toggles: on after first ENTER, off after second | |
| 7 | Type `DrawF sin(X)` → ENTER | Sine curve drawn as white overlay on graph canvas | |
| 8 | Type `Shade(-1,1)` → ENTER | Horizontal band between y=−1 and y=1 shaded on graph canvas (constant boundaries) | |
| 9 | After drawing, press `ZOOM` → `Standard` → return to graph | Draw layer content still visible (persists across re-render) | |
| 10 | Type `ClrDraw` → ENTER | All drawn content cleared from graph canvas | |
| 11 | Set window X[-5,5] Y[-5,5] → type `Shade(sin(X),cos(X),2,-3,3)` → ENTER | Shaded region appears only where sin(X) < cos(X) within X[-3,3]; boundary curves drawn; resolution=2 visibly sparser than default | |
| 12 | Type `Shade(X+1,X^3-8*X)` → ENTER | Shading appears in columns where X+1 < X³−8X; boundary curves drawn (guidebook p. 5-10 example) | |
| 13 | Type `Shade(sin(X),cos(X),1,-1,1)` → ENTER, then `Shade(sin(X),cos(X),8,-1,1)` → ENTER | Resolution 1 produces denser fill than resolution 8 | |
---
Manual Notes:
Task 1 above pass
Task 2 clears but does not show Done on screen when complete
Task 3 pass
Tasks 4-13 pass 
Bug: Zoom or graph scale change does not impact items that have been added to the graph via DRAW menu. 

---


---

## Section 4 — STAT menu (P30h)
_Validates statistics data entry, calculations, and graph plots._

**Setup:** Before starting, open STAT → DATA → Edit and confirm the list is empty (or ClrStat first).

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `2ND+MATRX` | STAT menu opens with 3 tabs: CALC / DRAW / DATA | |
| 2 | DATA tab → Edit → enter 5 pairs: (1,3), (2,5), (3,7), (4,9), (5,11) | All 5 pairs accepted and visible in the editor | |
| 3 | CALC → `1:1-Var` | Results: n=5, x̄=3, Sx≈1.5811 (accept ±0.001) | |
| 4 | CALC → `2:LinReg` | Results: a=2, b=1, r=1; variables A=2 and B=1 set in calc engine (verify with `A` and `B` on home screen) | |
| 5 | DATA → `3:xSort` | x-values sorted ascending; y-values reordered to match | |
| 6 | DATA → `4:ySort` | y-values sorted ascending; x-values reordered to match | |
| 7 | Set RANGE: Xmin=0, Xmax=6, Ymin=0, Ymax=12 → DRAW → `2:Scatter` | Scatter plot of data points visible on graph canvas | |
| 8 | DRAW → `3:xyLine` | Points connected by lines on graph canvas | |
| 9 | DRAW → `1:Hist` | Histogram bars visible on graph canvas | |
| 10 | `2nd+ON` to save → power-cycle → STAT → DATA → Edit | All 5 data pairs intact | |
| 11 | DATA → Edit → navigate to row 3 → press RIGHT to reach column 3 (row-select state) | `>` prefix appears before row 3 cursor; pressing INS/DEL applies to whole row | |
| 12 | With `>` cursor at row 3 → press `2ND+INS` | New row (0,0) inserted before the current row 3; remaining rows shift down; total becomes 6 pairs | |
| 13 | With `>` cursor at a row → press `DEL` | That row is removed; remaining rows shift up | |
| 14 | From home screen, after running 1-Var with 5 pairs: type `{x}(1)` → ENTER | Result is the first x-value (1); type `{y}(3)` → ENTER: result is the third y-value (7); type `{x}(6)` → ENTER: DOMAIN ERR (out of bounds) | |
---
Manual Notes:
Tasks 1-2 above pass
Task 3 fails to display the character of the x with the bar above it properly. Also the display fails to fit on the screen.
Task 4 fails if task 3 isn't done prior to task 4.
Tasks 5-6 pass.
Task 7 fail. It seems to properly display data points for about 1 second then the screen clears.
Task 8-9 fail. The both freeze up the calculator. interestingly even though the buttons cease responding the heartbeat led still works and the green led which toggles with each key press still function.
Task 10 fail
Task 11 pass according to test instruction description but the interaction is not desirable. the location where keypad input will go is ambiguous on the highlighted line.
Task 12-14 pass

---


---

## Section 5 — VARS menu (P31h)
_Validates all 5 tabs of the VARS key menu._

**Setup:** Run 1-Var (with the 5 stat pairs from Section 4) and LinReg before starting, so XY/LR/Σ tabs have values. Ensure at least one matrix has non-trivial dimensions.

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `VARS` | 5-tab menu opens: XY / Σ / LR / DIM / RNG. LEFT/RIGHT navigate tabs; UP/DOWN navigate items; digit shortcuts 1–9/0 work; CLEAR exits | |
| 2 | RNG tab | Shows 10 items. Scroll indicators (↑↓) appear when scrolling past 7. Item `0:Tstep` shortcut via `0` key inserts current Tstep value into expression | |
| 3 | XY tab → select `2:x̄` | Correct mean value (3) inserted into expression | |
| 4 | XY tab → select `3:Sx` | Correct Sx value (≈1.5811) inserted | |
| 5 | DIM tab → select `1:Arow` | Current row count of matrix [A] inserted | |
| 6 | DIM tab → select `2:Acol` | Current col count of matrix [A] inserted | |
| 7 | Press `Y=`, move cursor to Y₁= equation field, press `VARS` → XY → `2:x̄` | Value inserted into Y= equation (not home screen) | |
| 8 | LR tab → select `1:a` | Regression coefficient a=2 inserted | |
| 9 | LR tab → select `4:RegEQ` | String `aX+b` inserted into expression | |
| 10 | DIM tab → scroll to item `7:Dim{x}` → select | Current stat list length (5, if 5 pairs were entered) inserted into expression; verify by pressing ENTER and confirming the value matches the pair count | |
---
Manual Notes:
Tasks 1-2 above pass
Tasks 3-4 fail
Tasks 5-7 pass
Tasks 8-9 fail
Task 10 pass
---


---

## Section 6 — Y-VARS menu (P32h)
_Validates `2nd+VARS` equation reference and enable/disable tabs._

**Setup:** Ensure Y₁=X is entered and enabled. Enter Y₂=X² as well. In parametric mode, ensure X₁t=cos(T) / Y₁t=sin(T) is entered.

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `2ND+VARS` | 3-tab menu opens: Y / ON / OFF. LEFT/RIGHT navigate tabs; UP/DOWN navigate items; digit shortcuts 1–5 work; CLEAR exits | |
| 2 | Y tab → select `1:Y₁` | String `Y₁` inserted into expression buffer | |
| 3 | Store `3→X`; type `Y₁` (via Y-VARS) → ENTER | Result is 3 (evaluates Y₁=X at X=3) | |
| 4 | Press `Y=`; move cursor into Y₁ equation field; press `2ND+VARS` → Y → select `2:Y₂` | `Y₂` inserted into the Y= equation field (not home screen) | |
| 5 | OFF tab → `1:All-Off` → press `Y=` | All equations show `-` (disabled); no `=` visible | |
| 6 | ON tab → `2:Y₁-On` → press `Y=` | Y₁ shows `=` (enabled); Y₂–Y₄ remain `-` | |
| 7 | Y tab → press `1` directly (no UP/DOWN first) | `Y₁` inserted immediately — digit shortcut works without navigating | |
| 8 | In Func mode: Y tab shows exactly 4 items (Y₁–Y₄); in Param mode: Y tab shows 10 items (Y₁–Y₄ then X₁t, Y₁t, X₂t, Y₂t, X₃t, Y₃t) with scroll indicators (↑↓) appearing once past item 7 | Switch to Param via MODE; re-open Y-VARS → Y tab; confirm 10 items and scroll | |
| 9 | In Param mode: Y tab → press `6` (digit shortcut for item 6) | `X₁t` string inserted immediately | |
| 10 | In Param mode: Y tab → scroll down to item `8:Y₂t` → ENTER | `Y₂t` inserted into expression buffer | |
| 11 | In Param mode: ON tab | 8 items visible: `1:All-On`, `2:Y₁-On`, `3:Y₂-On`, `4:Y₃-On`, `5:Y₄-On`, `6:X₁t-On`, `7:X₂t-On`, `8:X₃t-On` | |
| 12 | In Param mode: OFF tab → select `6:X₁t-Off` → press `Y=` | X₁t/Y₁t pair shows `-` (disabled); other pairs unaffected | |
---
Manual Notes:
Tasks 1-2 above pass
Task 3 fail. Produces a full lock up. Both the red heartbeat LED stops and the green key toggling led stops.
Tasks 4-7 pass
Task 8 fail. the menu looks exactly the same in both Func and Param modes
Task 9 pass (note there was an error in the instruction, the digit shortcut 6 refers to Y1t)
Tasks 10-11 pass
Task 12 fail
---

---

## Section 7 — PRGM system (P10)
_50 tests + 5 new. Hardware: STM32F429I-DISC1. Run after flashing latest build._
_Mark ✅ / ❌ / ⚠️. See Notes section at end for context on individual tests._

### Menu & Navigation

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T01 | Open PRGM menu | From home, press `PRGM` | PRGM menu opens: `EXEC  EDIT  ERASE` tab bar, EXEC tab yellow. All 37 slots in format `N:PrgmN`. Named slots show `N:PrgmN  NAME` in second column. | |
| T02 | Tab navigation with wrap | With PRGM open, press RIGHT twice | Tab advances EXEC→EDIT→ERASE. LEFT at EXEC wraps to ERASE; RIGHT at ERASE wraps to EXEC. All 3 tabs show all 37 slots. | |
| T03 | Scroll indicators | EXEC or EDIT tab → press DOWN past slot 7 | `↓` amber indicator at bottom when more items below; `↑` at top once scrolled. Disappear at boundaries. | |
| T04 | Close menu | With PRGM open, press CLEAR | PRGM menu closes; home screen returns; expression unchanged | |
| T05 | Open PRGM from MATH menu | MATH key → then press PRGM | MATH closes; PRGM opens on EXEC tab; no display corruption | |

### Name Entry & Program Creation

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T06 | Create new program with name | PRGM → EDIT → empty slot → ENTER → type `TEST` → ENTER | Name-entry screen appears with cursor in alpha mode (green `A`). On ENTER, editor opens with title `Prgm1  TEST` and blank `:` line. | |
| T06a | Name-entry LEFT/RIGHT cursor | On name-entry with `ABCD` typed, press LEFT twice → type `X` | `X` inserted: field reads `ABXCD`. RIGHT RIGHT moves to end; DEL removes last char. | |
| T07 | Create program — skip name | EDIT → empty slot → ENTER → immediately ENTER again | Editor opens with title `PrgmN` (no user name). Body editable. | |
| T08 | Digits allowed in name | EDIT → empty slot → ENTER → type `A1B2` (ALPHA for letters, plain for digits) | `A`, `1`, `B`, `2` all accepted. Digits typed without ALPHA. | |
| T08a | DOWN from name-entry opens body | Name-entry with `MYTEST` typed → press DOWN | Editor body opens; title shows `PrgmN  MYTEST`. Pressing UP from line 0 at col 0 returns to name-entry with name intact. | |
| T09 | Name-entry DEL | Type a few letters → press DEL | Last character removed on first DEL press. ALPHA mode re-engages automatically. | |
| T09d | ALPHA_LOCK in name entry | From name-entry, press `2ND+ALPHA` to engage ALPHA_LOCK | Cursor green `A`. Typing letters inserts into name field. Pressing ALPHA once exits ALPHA_LOCK. | |
| T09b | ENTER works on first press | Type name → press ENTER exactly once | Editor opens on first ENTER; second press not needed | |
| T09c | CLEAR works on first press | Type letters in name entry → press CLEAR | Returns to PRGM EDIT tab on first CLEAR; no name or body saved | |
| T10 | Cancel name entry (empty) | From name-entry (no letters typed) → CLEAR | Returns to PRGM EDIT tab | |
| T11 | Open existing program | EDIT tab → navigate to named slot → ENTER | Editor opens directly (no name-entry screen). Title shows name. | |
| T11b | EDIT tab digit shortcut | PRGM → EDIT tab → press `1` without UP/DOWN | Editor for slot 1 opens immediately | |

### Editor

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T12 | Character input | Open any program → type `A+1` | Line shows `:A+1`; cursor advances | |
| T12b | Insert mode off by default | Open program → navigate LEFT → type a character (no INS pressed) | Character overwrites (no push-right). INS key toggles insert mode. | |
| T13 | CTL menu inserts keyword | In editor → PRGM → `3:If` (or press `3`) | CTL closes; line shows `:If `. Menu has exactly 8 items: `Lbl`, `Goto`, `If`, `IS>(`, `DS<(`, `Pause`, `End`, `Stop`. | |
| T14 | I/O menu inserts keyword | In editor → PRGM → LEFT/RIGHT to I/O tab → `1:Disp` (or press `1`) | Line shows `:Disp `. Menu has exactly 5 items: `Disp`, `Input`, `DispHome`, `DispGraph`, `ClrHome`. | |
| T15 | Multi-line scroll | In editor → press ENTER 6+ times | `↓` indicator on last visible row; `↑` appears after scrolling. Title stays fixed. | |
| T16 | Erase a program | PRGM → ERASE tab → navigate to named slot → ENTER | Confirmation dialog: `1:Do not erase` / `2:Erase`. Press `2` — slot cleared immediately (also test digit shortcuts on the confirmation dialog). | |
| T16b | ERASE shows all 37 slots | Open PRGM → ERASE tab | All 37 slots visible (named and unnamed). Named show `N:PrgmN  NAME`; unnamed show `N:PrgmN`. | |

### Persistence

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T17 | Programs survive power cycle | Create program `SAVE` in slot 1 with `Disp "OK"` → `2nd+ON` → USB unplug/replug | `1:Prgm1  SAVE` in EXEC/EDIT lists; body intact | |

### Executor — Basic Execution

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T18 | Run Disp program | Slot 1 has `HELLO` with body `Disp "HELLO"` → EXEC → slot 1 → ENTER → ENTER | `HELLO` in history (left-aligned, grey). `Done` as result row (right-aligned, white). | |
| T19 | Run empty slot | EXEC → slot with no body → ENTER → ENTER | `Done` appears; no error or lockup | |
| T20 | Expression evaluation and ANS | Program: `2+2` then `Disp ANS` → run | `4` in history output; ANS=4 after | |
| T21 | Input and variable store | Program: `Input A` then `Disp A` → run → type `7` → ENTER | `?` prompt shown; after ENTER: `7` displayed; A=7 | |
| T22 | If single-line skip | Program: `0→A` / `If A=1` / `Disp "YES"` / `Disp "DONE"` → run | `YES` does not appear; `DONE` appears | |
| T23 | EXEC number-key shortcut | PRGM → EXEC tab → press `1` (no UP/DOWN) | `prgm1` (or `prgmNAME`) inserted in expression buffer | |
| T24 | CLEAR aborts execution | Program: `Lbl A` / `Disp "X"` / `Goto A` → run → press CLEAR while running | Execution stops; home screen returns; no lockup | |

### Executor — Advanced Control Flow

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T25 | Lbl/Goto label entry | In editor → CTL → `Lbl` → type `A` → try to type a second letter | _(Note: single-char enforcement is a known intentional constraint; multi-char throws error on execution, not on entry)_ | |
| T26 | Disp string — left alignment | Program: `Disp "HELLO"` → run | `HELLO` left-aligned, grey | |
| T27 | Disp variable — right alignment | Program: `5→A` / `Disp A` → run | `5` right-aligned, white | |
| T28 | Goto/Lbl jump | Program: `Goto END` / `Disp "SKIP"` / `Lbl END` / `Disp "DONE"` → run | `SKIP` absent; `DONE` appears once | |
| T29 | Pause halts and resumes | Program: `Disp "WAIT"` / `Pause` / `Disp "RESUMED"` → run | Halts after `WAIT`; ENTER resumes; `RESUMED` appears | |
| T30 | Stop terminates early | Program: `Disp "A"` / `Stop` / `Disp "B"` → run | `A` appears; `B` does not | |
| T31 | IS>( increment and skip | Program: `1→I` / `IS>(I,2)` / `Disp "SKP"` / `Disp I` → run | I=1→2; 2 is not >2 so `SKP` NOT skipped; `I` shows 2 | |
| T32 | DS<( decrement and skip | Program: `3→I` / `DS<(I,3)` / `Disp "SKP"` / `Disp I` → run | I=3→2; 2<3 so `SKP` IS skipped; `I` shows 2 | |
| T33 | Subroutine auto-return | Slot 2: `Disp "SUB"`. Slot 1: `Disp "MAIN"` / `prgm2` / `Disp "BACK"` (insert via PRGM→EXEC) → run slot 1 | History: `MAIN`, `SUB`, `BACK` in order | |
| T33b | EXEC sub-menu tab | In editor → PRGM → RIGHT → RIGHT | EXEC tab (third tab) highlighted. Shows all 37 slots. Select slot 2 → `:prgm2` inserted. LEFT from CTL wraps to EXEC. | |
| T34 | Nested subroutine (2 deep) | Slot 3: `Disp "DEEP"`. Slot 2: `prgm3` / `Disp "MID"`. Slot 1: `prgm2` / `Disp "TOP"` → run slot 1 | History: `DEEP`, `MID`, `TOP` in order | |

### Executor — I/O Commands

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T35 | prgmNAME execution (named) | EXEC → named slot (e.g. `1:Prgm1  TEST`) → ENTER → ENTER | `prgmTEST` in expression (grey, left-aligned); after run: `Done` result row | |
| T35b | prgmNAME execution (unnamed) | EXEC → unnamed slot (e.g. `3:Prgm3`) → ENTER → ENTER | `prgm3` in expression; `Done` result row | |
| T36 | ClrHome clears history | Program: `Disp "LINE1"` / `Disp "LINE2"` / `ClrHome` / `Disp "AFTER"` → run | After ClrHome, LINE1 and LINE2 gone; only `AFTER` visible | |
| T37 | DispGraph switches to graph view | Ensure Y₁=X. Program: `DispGraph` / `Pause` / `DispHome` → run | Graph canvas appears (no lockup); ENTER at Pause switches back to home screen | |

### Executor — End command

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T38 | End terminates at top level | Program: `Disp "A"` / `End` / `Disp "B"` → run | `A` appears; `B` does not; `Done` shown (same behaviour as Stop) | |
| T39 | End returns from subroutine | Slot 2: `Disp "SUB"` / `End`. Slot 1: `Disp "MAIN"` / `prgm2` / `Disp "BACK"` → run slot 1 | History: `MAIN`, `SUB`, `BACK` in order — End in subroutine returns to caller, does not terminate the parent | |

### PRGM MODE sub-menu

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T40 | PRGM MODE opens NUMBER tab | In editor → press `MODE` key | PRGM MODE sub-menu opens with `NUMBER` tab active; 7 items: `1:Norm`, `2:Sci`, `3:Eng`, `4:Fix`, `5:Float`, `6:Rad`, `7:Deg`; CLEAR exits | |
| T41 | PRGM MODE NUMBER item inserts keyword | Select `2:Sci` | `:Sci` inserted into current program line | |
| T42 | PRGM MODE GRAPH tab | From NUMBER tab, press RIGHT | GRAPH tab active; 10 items: `1:Function`–`9:Rect` with `0:` (10th) requiring scroll; LEFT/RIGHT tab wrap works | |
| T43 | PRGM MODE executes notation change | Slot 1: `Sci`. Run slot 1 from home. Type `123` → ENTER | Result shows `1.23E2` — Sci mode active; confirm by re-running `Norm` program and checking `123` ENTER → `123` | |

### Editor Alpha Behaviour

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T41 | Cursor blinks in editor | Open any program; observe for ~2 seconds | Cursor blinks at ~530 ms interval on current line | |
| T42 | 2ND mode in editor | In editor → press `2ND` | Cursor turns amber with `^`; resolves on next non-modifier key | |
| T43 | ALPHA single in editor | In editor → press `ALPHA` once | Cursor green `A`; one letter inserted; cursor resets to white. Second ALPHA press cancels ALPHA mode. | |
| T43b | ALPHA_LOCK in editor | In editor → press `2ND+ALPHA` | Cursor green `A`; multiple letters inserted into program body; ALPHA press once cancels; digits work without exiting ALPHA_LOCK | |
| T44 | Cursor blinks in name entry | Open name-entry screen; observe for ~2 seconds | Cursor blinks at ~530 ms interval | |

### Editor Body Behaviour

| # | Test | Steps | Expected | Result |
|---|---|---|---|---|
| T45 | Body-only slot opens directly | Editor → type content → CLEAR at name-entry (skip name). Reopen EDIT → same slot → ENTER | Editor opens directly (no name-entry screen); body preserved | |
| T46 | Tab wrap in sub-menus | In editor → PRGM → CTL tab. Press LEFT. | Wraps to EXEC. LEFT→I/O; LEFT→CTL. RIGHT: CTL→I/O→EXEC→CTL | |
| T46b | EXEC digit and ALPHA+letter shortcuts | On EXEC sub-menu → press `2` | `:prgm2` inserted immediately. Then EXEC tab again → ALPHA+A → `:prgmA` inserted (slot 10). | |

---

## Notes (PRGM section)

- All tab/item highlights are **yellow** (`0xFFFF00`). Scroll indicators are **amber** (`0xFFAA00`). Amber on a tab or item cursor (not an arrow) is a regression.
- T17 requires USB unplug/replug (full power cycle), not just SWD reset.
- T33/T34: max call stack depth is 4; exceeding it makes the `prgm` call a no-op (no crash). No explicit `Return` command — subroutines auto-return at end of body.
- T24 CLEAR abort: a few `X` rows may appear before CLEAR is processed (queue latency) — acceptable.
- CTL menu must have exactly **8 items**. `Then`, `Else`, `While`, `For(`, `Return`, `prgm` must not appear.
- I/O menu must have exactly **5 items**. `Prompt`, `Output(`, `Menu(` must not appear.
- T25 label enforcement: single-char limit is enforced at entry in the editor (typing a second char is ignored). This is intentional. If you want to change this to allow multi-char labels with a runtime error, add it to "Next session priorities" in CLAUDE.md.
- T40–T43: PRGM MODE NUMBER/GRAPH tab numbers listed above; verify via docs/PRGM_COMMANDS.md for full item list.

---

## Section 8 — MODE screen — display and graph modes
_Validates MODE rows 0, 5, 6, 8 and the free-roaming graph cursor introduced with MODE_GRAPH_FREE_CURSOR._

**Setup:** Start in default state (Normal notation, Func mode, Connected, Sequential, Rect display). Use `2nd+ON` → power-cycle to verify persistence tests.

### Display notation (Sci / Eng / Norm)

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `MODE` → inspect row 0 | Three options shown: `Norm  Sci  Eng`; current selection highlighted | |
| 2 | Select `Sci` → ENTER → return to home → type `12345` → ENTER | Result shows `1.2345E4` (exactly one digit before decimal point) | |
| 3 | In Sci mode, type `0.00123` → ENTER | Result shows `1.23E-3` | |
| 4 | Select `Eng` → ENTER → type `12345` → ENTER | Result shows `12.345E3` (exponent is a multiple of 3) | |
| 5 | In Eng mode, type `0.00123` → ENTER | Result shows `1.23E-3` | |
| 6 | Select `Norm` → ENTER → type `0.0005` → ENTER | Result shows `5E-4` (auto-sci threshold: values < 0.001 display in scientific notation) | |
| 7 | In Norm mode, type `0.001` → ENTER | Result shows `0.001` (threshold is strictly less than 0.001, so 0.001 stays in Norm) | |
| 8 | Set `Sci` → `2nd+ON` → power-cycle → check home screen with `100` → ENTER | Result shows `1E2` — notation mode persisted | |

### Free-roaming graph cursor

**Setup:** Ensure Y₁=sin(X) is entered and enabled. Set RANGE to ZTrig preset (ZOOM → ZTrig).

| # | Test | Expected | Result |
|---|---|---|---|
| 9 | Press `GRAPH` from home screen | Blinking white crosshair appears at screen centre; `X=` and `Y=` labels below canvas show current math coordinates | |
| 10 | Press LEFT, RIGHT, UP, DOWN | Crosshair moves one pixel per press; `X=` and `Y=` update with each step; cursor remains visible immediately after each press (blink timer resets) | |
| 11 | From free-cursor position, press `TRACE` | Cursor snaps to nearest active equation at the current X position; crosshair turns to trace style (green); `X=`/`Y=` show on-curve values | |
| 12 | While in TRACE mode, press `TRACE` again | Cursor re-snaps to X=x_mid on the same equation — does NOT exit trace mode; no visible glitch | |
| 13 | While in TRACE mode, press `GRAPH` | Graph re-renders; blinking white free cursor appears at screen centre | |
| 14 | While in TRACE mode, press a digit key (e.g. `5`) | Trace exits; `5` appears in expression buffer on home screen | |
| 15 | From `Y=` screen, press `GRAPH` | Blinking free cursor appears (not a static render) | |
| 16 | From `RANGE` screen, press `GRAPH` | Blinking free cursor appears | |
| 17 | From `ZOOM` menu, press `GRAPH` | Blinking free cursor appears | |
| 18 | Navigate to home screen or any menu after graphing | Cursor blink timer stops — no blinking animation occurs when graph canvas is not the active screen | |

### Connected / Dot plot mode (P33h)

| # | Test | Expected | Result |
|---|---|---|---|
| 19 | Press `MODE` → row 5 shows `Connected  Dot` | Both options visible; current selection highlighted | |
| 20 | Select `Dot` → ENTER → enter Y₁=sin(X) → set ZTrig preset → `GRAPH` | Only individual pixels plotted; no line segments connecting them | |
| 21 | Select `Connected` → ENTER → re-`GRAPH` (or press GRAPH again) | Interpolated line segments connect the plotted pixels | |
| 22 | While in Dot mode, select MODE row 4 `Param` → ENTER | Y= layout switches to X₁t/Y₁t (row index bug verified fixed — no mis-routing to Connected/Dot row) | |
| 23 | Set `Dot` → `2nd+ON` → power-cycle → check MODE row 5 | `Dot` remains selected | |

### Sequential / Simultaneous plot mode (P38h)

**Setup:** Enter Y₁=X and Y₂=X² in Func mode. Set RANGE: Xmin=-5, Xmax=5, Ymin=-5, Ymax=25.

| # | Test | Expected | Result |
|---|---|---|---|
| 24 | Press `MODE` → row 6 shows `Sequential  Simul` | Both options visible | |
| 25 | Select `Simul` → ENTER → `GRAPH` | Both Y₁ and Y₂ advance across the screen simultaneously — visible interleaving as the graph draws | |
| 26 | Select `Sequential` → ENTER → `GRAPH` | Y₁=X plots completely first, then Y₂=X² plots | |
| 27 | Set `Simul` → `2nd+ON` → power-cycle → check MODE row 6 | `Simul` remains selected | |

### Polar coordinate display (P40h)

**Setup:** Return to Rect mode and Func mode. Ensure Y₁=X² is entered.

| # | Test | Expected | Result |
|---|---|---|---|
| 28 | Press `MODE` → row 8 shows `Rect  Pol` | Both options visible | |
| 29 | Select `Pol` → ENTER → `GRAPH` with Y₁=X² → move free cursor | Readout below canvas shows `R=` and `θ=` instead of `X=` / `Y=` | |
| 30 | In Pol display mode, press `TRACE` and move LEFT/RIGHT | Trace readout shows `R=` / `θ=` for each step | |
| 31 | Select `Rect` → ENTER → `GRAPH` | Readout reverts to `X=` / `Y=` | |
| 32 | Set `Pol` → `2nd+ON` → power-cycle → check MODE row 8 | `Pol` remains selected | |
| 33 | On home screen (Rad angle mode), type `R>P(-1,0)` → ENTER | Result is `1`; then type `θ` → ENTER: result is `3.14159...` | |
| 34 | Type `P>R(1,0)` → ENTER | Result is `1`; then type `Y` → ENTER: result is `0` | |
| 35 | Type `3` → `STO→` → `θ` → ENTER → type `θ` → ENTER | θ variable stores 3; second ENTER returns `3` | |

---

## Section 9 — MATH extensions (HYP tab + nDeriv + expression fixes)
_Validates hyperbolic functions, numerical derivative, and auto-close trailing parenthesis._

**Setup:** Ensure angle mode is Rad.

### MATH HYP tab

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `MATH` → navigate to `HYP` tab | HYP tab shows 6 items: `sinh(`, `cosh(`, `tanh(`, `asinh(`, `acosh(`, `atanh(` | |
| 2 | Select `1:sinh(` → type `0` → ENTER | Result is `0` | |
| 3 | Select `2:cosh(` → type `0` → ENTER | Result is `1` | |
| 4 | Select `3:tanh(` → type `0` → ENTER | Result is `0` | |
| 5 | Select `4:asinh(` → type `1` → ENTER | Result ≈ `0.881374` | |
| 6 | Select `5:acosh(` → type `1` → ENTER | Result is `0` | |
| 7 | Select `5:acosh(` → type `0` → ENTER | `DOMAIN ERR` (acosh domain is x ≥ 1) | |
| 8 | Select `6:atanh(` → type `0` → ENTER | Result is `0` | |
| 9 | Select `6:atanh(` → type `1` → ENTER | `DOMAIN ERR` (atanh domain is |x| < 1) | |
| 10 | Type `sinh(1)` directly in expression buffer without menu | Result ≈ `1.1752` (confirms keyword tokenizer matches `sinh` before `sin`) | |

### nDeriv(

| # | Test | Expected | Result |
|---|---|---|---|
| 11 | Type `nDeriv(X^2,X,3)` → ENTER | Result ≈ `6` (derivative of X² at X=3 is 2X=6; tolerance ±0.01) | |
| 12 | Type `nDeriv(sin(X),X,0)` → ENTER | Result ≈ `1` (derivative of sin at 0 is cos(0)=1; tolerance ±0.01) | |
| 13 | Type `nDeriv(X^3,X,2)` → ENTER | Result ≈ `12` (derivative of X³ at X=2 is 3X²=12; tolerance ±0.01) | |

### Auto-close trailing parenthesis

| # | Test | Expected | Result |
|---|---|---|---|
| 14 | Type `sin(1` (no closing `)`) → ENTER | Result ≈ `0.841471` — unmatched `(` is auto-closed; no SYNTAX ERR | |
| 15 | Type `(1+2` → ENTER | Result is `3` — leading unmatched `(` auto-closed | |
| 16 | Type `sin(cos(0` → ENTER | Result ≈ `0.841471` — two unmatched `(` auto-closed in order | |

---

## Section 10 — Matrix row operations (MATRX MATH menu)
_Validates rowSwap, Row+, \*Row, \*Row+ added to MATRX MATH menu._

**Setup:** Set matrix [A] to a 2×2 matrix with values [[1,2],[3,4]]. (MATRX → EDIT → [A] → set 2 rows, 2 cols → enter values.)

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `MATRX` | MATRX menu opens; navigate to MATH tab | MATH tab shows 6 items: `1:det(`, `2:T`, `3:rowSwap(`, `4:Row+(`, `5:*Row(`, `6:*Row+(` | |
| 2 | Select `1:det(` → type `[A])` → ENTER | Result is `-2` (det of [[1,2],[3,4]] = 1×4 − 2×3 = −2) | |
| 3 | Select `2:T` → type `[A])` → ENTER | Result is [[1,3],[2,4]] — transpose of [A] | |
| 4 | Select `3:rowSwap(` → type `[A],1,2)` → ENTER | Result is [[3,4],[1,2]] — rows 1 and 2 swapped | |
| 5 | Select `4:Row+(` → type `[A],1,2)` → ENTER | Result is [[1,2],[4,6]] — row 1 added to row 2 | |
| 6 | Select `5:*Row(` → type `2,[A],1)` → ENTER | Result is [[2,4],[3,4]] — row 1 multiplied by scalar 2 | |
| 7 | Select `6:*Row+(` → type `2,[A],1,2)` → ENTER | Result is [[1,2],[5,8]] — 2×row 1 added to row 2 | |
| 8 | All row-op results: original [A] unchanged after evaluating expressions | Press `[A]` → ENTER: still [[1,2],[3,4]] (row-ops return new matrix, do not modify in place) | |

---

## Section 11 — RESET menu
_Validates `2nd++` (2nd then +) RESET MEMORY confirmation screen per guidebook p. 1-28._

**Setup:** Ensure some data exists before testing reset: enter a stat data pair, name a program, store a value to variable A.

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `2ND` then `+` from home screen | RESET MEMORY screen appears with title `MEMORY`, stat point count (`Sts: N pts`), program count (`Pgm: N stored`), and choices `1:No` / `2:Reset` | |
| 2 | On RESET screen: press `1` or `CLEAR` | RESET screen closes; returns to previous screen; no data changed | |
| 3 | Stat count display | Open RESET screen when 5 stat pairs are entered | `Sts: 5 pts` shown | |
| 4 | Program count display | Open RESET screen when 2 programs are named | `Pgm: 2 stored` shown | |
| 5 | Press `2` to confirm reset | RESET executes; home screen shows `Mem cleared`; ANS=0 | |
| 6 | After reset: verify stat cleared | STAT → DATA → Edit | List is empty | |
| 7 | After reset: verify variables cleared | Type `A` → ENTER | Result is `0` | |
| 8 | After reset: verify Y= cleared | Press `Y=` | All equations show blank / disabled | |
| 9 | After reset: verify RANGE defaults | Press `RANGE` | Xmin=-10, Xmax=10, Xscl=1, Ymin=-10, Ymax=10, Yscl=1 | |
| 10 | `2ND++` accessible from non-home screens | Open STAT menu → press `2ND` then `+` | RESET screen appears (TOKEN_RESET fires from any mode) | |

---

## Summary

| Section | Feature | Tests | Result |
|---|---|---|---|
| 1 | Cursor rendering (P28) | 29 | |
| 2 | Parametric graphing (P35h) | 6 | |
| 3 | DRAW menu (P29h) | 13 | |
| 4 | STAT menu (P30h) | 14 | |
| 5 | VARS menu (P31h) | 10 | |
| 6 | Y-VARS menu (P32h) | 12 | |
| 7 | PRGM system (P10) | 55 | |
| 8 | MODE screen (notation + graph modes + free cursor) | 35 | |
| 9 | MATH extensions (HYP + nDeriv + auto-close) | 16 | |
| 10 | Matrix row operations | 8 | |
| 11 | RESET menu | 10 | |
| **Total** | | **208** | |

---

## Sign-off

When all sections pass:
1. Delete `docs/p28_cursor_manual_tests.md` (P28 complete)
2. Delete `docs/prgm_manual_tests.md` (P10 complete)
3. Add a Resolved Items row to `docs/PROJECT_HISTORY.md` for each section
4. Remove completed hardware items from "Next session priorities" in `CLAUDE.md`
