# Hardware Validation Checklist

Hardware: STM32F429I-DISC1. Flash latest build before running.
Mark each test ✅ PASS / ❌ FAIL / ⚠️ PARTIAL.

---

## Contents

- [Normal Mode / Home Screen](#normal-mode--home-screen) — NM
- [MODE Menu](#mode-menu) — MD
- [VARS Menu](#vars-menu) — V
- [Y-VARS Menu](#y-vars-menu) — YV
- [MATRIX](#matrix) — MX
- [Graph — Standard](#graph--standard) — G
- [Graph — TRACE and Free Cursor](#graph--trace-and-free-cursor) — GT
- [Graph — Connected/Dot Mode](#graph--connecteddot-mode) — GC
- [Graph — Parametric](#graph--parametric) — GP
- [Graph — Sequential/Simultaneous](#graph--sequentialsimultaneous) — GS
- [Graph — Polar](#graph--polar) — PO
- [DRAW Menu](#draw-menu) — DR
- [STAT](#stat) — ST
- [PRGM](#prgm) — T (T01–T46b)

---

## Quick Reference

- Tab and item highlights are **yellow**. Scroll indicators are **amber**. Amber on a tab or item cursor is a regression.
- All menus: CLEAR closes, digit keys are shortcuts where shown.
- Power-cycle tests require full USB unplug/replug — SWD reset does not flush FLASH.

---

## Normal Mode / Home Screen

### NM01 — Basic arithmetic
Type `2+3`, press ENTER. Verify result `5` appears right-aligned in white. Expression row shows `2+3` in grey above it.
RESULT:

### NM02 — History navigation
With at least two previous expressions in history, press UP. Verify previous expression is recalled into the expression buffer. Press UP again for the one before. Press DOWN to move forward. Press ENTER — recalled expression evaluates.
RESULT:

### NM03 — STO variable
Type `7`, press STO, press ALPHA+A (stores to A), press ENTER. Verify `7` shown as result. Then type `A`, press ENTER. Verify `7` returned.
RESULT:
_Host test: test_normal_mode STO group._

### NM04 — Cursor render — mode indicators
Verify the cursor changes correctly for each state: white block (normal), amber+`^` (2nd active), green+`A` (ALPHA), green+`A`+STO styling (STO pending after pressing STO). Pressing 2nd again cancels 2nd; pressing ALPHA again cancels ALPHA.
RESULT:

### NM05 — CLEAR clears expression
With an expression typed, press CLEAR. Verify expression buffer clears. Press CLEAR on an empty buffer — verify history is cleared.
RESULT:

---

## MODE Menu

### MD01 — Open and navigate MODE menu
Press MODE. Verify 8 rows of settings displayed, first row highlighted. Navigate with UP/DOWN. Verify LEFT/RIGHT change the selected option in each row. Press CLEAR to close.
RESULT:

### MD02 — Degree/Radian mode
Set MODE → Radian. Evaluate `sin(π/2)` — verify result is `1`. Set MODE → Degree. Evaluate `sin(90)` — verify result is `1`.
RESULT:

### MD03 — Float vs. fixed decimal
Set MODE → Float. Evaluate `1/3` — verify result shows trailing digits (e.g. `.333333`). Set MODE → `2` (2 decimal places). Evaluate `1/3` — verify result shows `.33`.
RESULT:

### MD04 — Sci notation
Set MODE → Sci. Evaluate `12345`. Verify result displayed in scientific notation (e.g. `1.2345e4`). Restore to Normal.
RESULT:

---

## VARS Menu

### V01 — Open VARS and navigate tabs
Press 2nd+VARS. Verify menu opens with tabs: XY, Σ, LR, DIM, RNG. Navigate between tabs. Verify items are listed under each tab.
RESULT:

### V02 — Insert variable into expression
With VARS menu open on XY tab, select `x̄`. Verify menu closes and `x̄` is inserted into the expression buffer. Press ENTER — verify it evaluates (may be 0 if no STAT data).
RESULT:

### V03 — Stat variables populated after calculation
Enter STAT data (at least 3 x/y pairs). Run 1-Var Stats. Open VARS → XY tab. Verify `n`, `x̄`, `Sx`, `σx` show meaningful values when inserted and evaluated.
RESULT:

### V04 — Window range variables (RNG tab)
Open VARS → RNG tab. Insert `Xmin` into expression, press ENTER. Verify the current window Xmin value is returned (e.g. `-10` after a ZStandard).
RESULT:

---

## Y-VARS Menu

### YV01 — Open Y-VARS and navigate tabs
Press 2nd+Y-VARS. Verify tabs: OFF, Y, ON. Navigate between tabs. Y tab shows Y₁–Y₄ and parametric pairs (X₁t/Y₁t etc.).
RESULT:

### YV02 — Toggle equations ON/OFF
With Y₁ defined, open Y-VARS → OFF tab, select `Y₁-Off`. Press GRAPH. Verify Y₁ is not plotted. Open Y-VARS → ON tab, select `Y₁-On`. Press GRAPH. Verify Y₁ is plotted again.
RESULT:

### YV03 — Insert Y variable into expression
With Y₁=X+1, open Y-VARS → Y tab, select `Y₁`. Verify `Y₁` inserted. Type `(3)` to form `Y₁(3)`. Press ENTER. Verify result is `4`.
RESULT:
_Host test: test_yvars covers Y₁–Y₄ tokenization and evaluation._

---

## MATRIX

### MX01 — Open MATRIX and set dimensions
Press MATRIX → EDIT tab → select [A]. Verify dimension-entry screen appears. Set 2×2. Verify the cell editor opens with a 2×2 grid.
RESULT:

### MX02 — Enter matrix values
In the [A] cell editor, navigate with arrow keys and enter values: `1`, `2`, `3`, `4` across the four cells. Press CLEAR. Reopen [A]. Verify values are preserved.
RESULT:

### MX03 — Matrix arithmetic
Set [A] = 2×2 with values `1 2 / 3 4`. Set [B] = 2×2 with values `5 6 / 7 8`. On home screen, type `[A]+[B]`, press ENTER. Verify result matrix `6 8 / 10 12`.
RESULT:
_Host test: test_calc_engine covers matrix arithmetic._

### MX04 — det( operation
With [A] = `1 2 / 3 4`, open MATRIX → MATH tab → select `det(`. Complete expression as `det([A])`. Verify result is `-2`.
RESULT:
_Host test: test_calc_engine covers det._

### MX05 — Transpose
With [A] = `1 2 / 3 4`, type `[A]` then open MATRIX → MATH → select `T`. Verify result matrix `1 3 / 2 4`.
RESULT:

### MX06 — Row operations
With [A] = 3×3, open MATRIX → MATH → select `rowSwap(`. Complete as `rowSwap([A],1,2)`. Verify rows 1 and 2 are swapped in the result.
RESULT:

### MX07 — Matrices survive power cycle
Set [A] = 2×2 with known values. Press 2nd+ON, then full USB power cycle. Open MATRIX → EDIT → [A]. Verify dimensions and values are intact.
RESULT:

---

## Graph — Standard

### G01 — Enter equation and graph
Press Y=. Enter `Y₁=X²`. Press GRAPH. Verify a parabola renders. No lockup, no white screen.
RESULT:

### G02 — WINDOW settings
From graph screen press WINDOW. Verify Xmin, Xmax, Xscl, Ymin, Ymax, Yscl, Xres fields are editable. Change Xmin to `-5`, Xmax to `5`, press GRAPH. Verify graph re-renders with new window.
RESULT:

### G03 — ZOOM Standard
Press ZOOM → select `ZStandard` (or press the shortcut number). Verify window resets to standard range and graph re-renders.
RESULT:

### G04 — Multiple equations
Enter Y₁=X, Y₂=X². Press GRAPH. Verify both curves appear on the same canvas without corruption.
RESULT:

### G05 — Return to home from graph
While on graph screen, press CLEAR (or 2nd+QUIT). Verify calculator home screen returns with expression buffer unchanged.
RESULT:

---

## Graph — TRACE and Free Cursor

### GT01 — TRACE mode
With Y₁=X² graphed, press TRACE. Verify a blinking cursor appears on the curve, and X/Y coordinate values are displayed at the bottom of the screen.
RESULT:

### GT02 — TRACE navigation
In TRACE mode, press RIGHT several times. Verify cursor moves along the curve and displayed X/Y values update with each step.
RESULT:

### GT03 — TRACE between equations
With two equations graphed, press UP/DOWN in TRACE mode. Verify cursor jumps to the other equation and the equation label updates.
RESULT:

### GT04 — Free cursor
From graph screen (not in TRACE), press any arrow key. Verify a crosshair free cursor appears and moves freely around the canvas. X/Y coordinates shown at the bottom update as cursor moves.
RESULT:

### GT05 — Exit TRACE/cursor
In TRACE or free-cursor mode, press CLEAR. Verify cursor disappears and mode returns to normal graph view (no coordinates displayed).
RESULT:

---

## Graph — Connected/Dot Mode

### GC01 — Connected mode
Press MODE → set row 4 to **Connected**. Graph Y₁=X². Verify the plotted curve is drawn with continuous lines connecting computed points.
RESULT:

### GC02 — Dot mode
Press MODE → set row 4 to **Dot**. Graph Y₁=X². Verify only individual computed points are plotted — no connecting lines.
RESULT:

---

## Graph — Parametric

### GP01 — Enter parametric equations
Press MODE → set row 3 to **Param**. Press Y=. Verify entry fields show X₁t and Y₁t (not Y₁). Enter X₁t=cos(T), Y₁t=sin(T).
RESULT:

### GP02 — Graph parametric curve
With X₁t=cos(T) and Y₁t=sin(T) entered, press GRAPH. Verify a circle (or arc) is rendered. No lockup.
RESULT:

### GP03 — TRACE on parametric
With parametric curve graphed, press TRACE. Verify cursor appears on the curve and the T parameter value is displayed alongside X/Y. LEFT/RIGHT updates T and moves the cursor.
RESULT:

---

## Graph — Sequential/Simultaneous

### GS01 — Sequential mode
Press MODE → set row 5 to **Sequential**. Graph Y₁=X and Y₂=X². Verify equations are drawn one at a time (first Y₁ completes, then Y₂ starts).
RESULT:

### GS02 — Simultaneous mode
Press MODE → set row 5 to **Simul**. Graph Y₁=X and Y₂=X². Verify both curves are drawn together, advancing one pixel column at a time across both equations simultaneously.
RESULT:

---

## Graph — Polar

### PO01 — Enter polar equation
Press MODE → set row 3 to... verify polar is available. (Note: if the TI-81 MODE row 3 is Func/Param only, skip this section — polar may not be implemented.) Set polar mode. Press Y=. Verify r₁ entry field.
RESULT:

### PO02 — Graph polar curve
Enter r₁=2 (circle). Press GRAPH. Verify a circle is rendered.
RESULT:

### PO03 — Polar coordinate display
Press MODE → set row 7 to **Pol**. Graph a curve. Press TRACE or move free cursor. Verify coordinates are displayed in polar form (r, θ) rather than rectangular (x, y).
RESULT:

---

## DRAW Menu

### DR01 — Open DRAW menu
From graph screen, press 2nd+DRAW. Verify DRAW menu opens listing: ClrDraw, Line(, PT-On(, PT-Off(, PT-Chg(, DrawF, Shade(.
RESULT:

### DR02 — ClrDraw
Use free cursor or another DRAW command to mark something on the canvas. Then open DRAW → ClrDraw. Verify the draw layer is cleared and graph re-renders cleanly.
RESULT:

### DR03 — Line(
Open DRAW → Line(. Complete the expression as `Line(−5,−5,5,5)` on the home screen and press ENTER. Verify a diagonal line appears on the graph canvas.
RESULT:

### DR04 — Shade(
Open DRAW → Shade(. Complete as `Shade(−1,1)`. Verify the region between y=−1 and y=1 is shaded on the graph canvas.
RESULT:

---

## STAT

### ST01 — Enter data via STAT EDIT
Press STAT → EDIT tab → Edit. Verify a two-column (x, y) data editor opens. Enter at least 4 data pairs (e.g. x: 1,2,3,4; y: 2,4,6,8). Press CLEAR to exit.
RESULT:

### ST02 — 1-Var Stats
With data entered, press STAT → CALC tab → 1-Var. Press ENTER. Verify statistical output shows n, x̄, Sx, σx and other 1-Var values. Values should match the entered data.
RESULT:
_Host test: test_stat covers 1-Var calculations._

### ST03 — LinReg
With data entered, press STAT → CALC tab → LinReg. Press ENTER. Verify a=2, b=0 (or close) for the linear data above, and r≈1.
RESULT:
_Host test: test_stat covers LinReg including Pearson r._

### ST04 — STAT DRAW scatter plot
With data entered, press STAT → DRAW tab → Scatter. Verify a scatter plot of the entered data points is rendered on the graph canvas.
RESULT:

### ST05 — ClrStat
Press STAT → DATA tab → ClrStat. Re-open STAT EDIT. Verify all data fields are cleared.
RESULT:
_Host test: test_stat covers Clear._

---

## PRGM

### Quick Reference (PRGM-specific)
- CTL sub-menu has exactly **8 items**: Lbl, Goto, If, IS>(, DS<(, Pause, End, Stop.
- I/O sub-menu has exactly **5 items**: Disp, Input, DispHome, DispGraph, ClrHome.
- Subroutines auto-return when the last line is reached. Max call depth is 4.
- T17 requires a full USB power cycle (unplug/replug), not just SWD reset.

---

### T01 — Open PRGM menu
Press PRGM from home. Verify EXEC tab highlighted yellow, all 37 slots listed as `N:PrgmN`. Named slots show a second column: `N:PrgmN  NAME`.
RESULT: PASS

### T02 — Tab navigation with wrap
With PRGM open, press RIGHT twice. Tab advances EXEC → EDIT → ERASE. Verify LEFT from EXEC wraps to ERASE; RIGHT from ERASE wraps to EXEC. All three tabs show all 37 slots.
RESULT: PASS

### T03 — Scroll indicators
Open PRGM menu. Scroll DOWN past the 7th visible slot. Verify amber `↓` appears at the bottom and amber `↑` appears once scrolled past the first slot. Both disappear at their respective boundaries.
RESULT: PASS

### T04 — Close PRGM menu
With PRGM open, press CLEAR. Verify menu closes, home screen returns, expression buffer unchanged.
RESULT: PASS

### T05 — Open PRGM from another menu
Open MATH menu then press PRGM. Verify MATH closes, PRGM opens on EXEC tab with no display corruption.
RESULT: PASS

---

### T06 — Create new program with a name
PRGM → EDIT tab → select empty slot → ENTER. Verify name-entry screen shows `PrgmN:` with alpha-mode cursor (green A). Type `TEST`. Verify name updates as typed. Press ENTER. Verify editor opens with title `PrgmN  TEST` and one blank `:` line with cursor.
RESULT: PASS

### T06a — Name-entry cursor navigation
On name-entry screen with `ABCD` typed, press LEFT twice. Verify cursor moves between `AB` and `CD`. Type `X` — field should read `ABXCD`. Press RIGHT RIGHT, cursor moves to end. Press DEL — `D` removed.
RESULT: Partial fail. while this does exactly as noted in expectation of T06a, that is undesireable. the cursor does not show that it's in insert mode, it shows default mode blinking alpha cursor indicating overwrite with alpha

### T07 — Create program without a name
PRGM → EDIT → select empty slot → ENTER → immediately press ENTER again (skip name). Verify editor opens titled `PrgmN` with no user name. Body can be edited normally.
RESULT: PASS

### T08 — Digits allowed in program name
PRGM → EDIT → empty slot → ENTER. Type `A1B2` (ALPHA+A, 1, ALPHA+B, 2). Verify digits appear without requiring ALPHA. Press ENTER. Verify editor title shows `A1B2`.
RESULT: FAIL

### T08a — DOWN transitions to editor; UP returns to name entry
On name-entry screen with `MYTEST` typed, press DOWN. Verify editor opens with cursor on first line, title shows `PrgmN  MYTEST`. From editor line 0 col 0, press UP. Verify name-entry screen reappears with `MYTEST` intact.
RESULT: Partial FAIL. while this does work properly upon initial program edit and save, upon re-entry of a previously saved program the program name is inaccessable.

### T09 — DEL in name entry
Type two or three letters on name-entry screen. Press DEL. Verify last character is removed on the **first** DEL press. Verify ALPHA mode re-engages automatically and next keypress inserts a letter without re-pressing ALPHA.
RESULT: PASS

### T09b — ENTER works on first press
Type a name on name-entry screen. Press ENTER once. Verify editor opens — a second ENTER press is not required.
RESULT: PASS

### T09c — CLEAR works on first press
On name-entry screen with letters typed, press CLEAR. Verify returns to PRGM EDIT tab on the first CLEAR press — a second press is not required.
RESULT: PASS

### T09d — ALPHA LOCK in name entry
On name-entry screen, press 2nd+ALPHA to engage ALPHA LOCK. Verify cursor shows green A. Type letters — they should insert into the **name field**, not the calculator expression. Press ALPHA to exit ALPHA LOCK.
RESULT: Partial PASS. pressing ALPHA while alpha lock is active does not remove alpha lock as expected

### T10 — Cancel name entry when empty
On name-entry screen with no letters typed, press CLEAR. Verify returns to PRGM EDIT tab with no name or body saved.
RESULT: PASS

### T11 — Open existing named program
With at least one named program, PRGM → EDIT → navigate to `N:PrgmN  NAME` → ENTER. Verify editor opens directly with title `PrgmN  NAME`.
RESULT: NOTE while this test does exactly as T11 expects, i would like to simplify it to just always open the same way, with the program name entry active

### T11b — EDIT tab digit shortcut
Open PRGM → EDIT tab. Press `1` without pressing UP/DOWN. Verify slot 1 editor opens immediately.
RESULT: PASS

---

### T12 — Character input
Open editor. Type `A+1` (ALPHA+A, +, 1). Verify line reads `:A+1`, cursor advances with each keypress.
RESULT: PASS

### T12b — Overwrite mode is default
Open editor. Navigate LEFT so cursor is not at end. Type a character **without** pressing INS first. Verify the character at cursor is overwritten — following characters do not shift right.
RESULT:

### T13 — CTL sub-menu inserts keyword
In editor, press PRGM (CTL sub-menu opens). Navigate to `3:If` or press `3`. Verify menu closes and `:If ` appears on the current line. Verify CTL shows exactly 8 items without scrolling.
RESULT:

### T14 — I/O sub-menu inserts keyword
In editor, press PRGM → switch to I/O tab → select `1:Disp` or press `1`. Verify `:Disp ` inserted. Verify I/O shows exactly 5 items without scrolling.
RESULT:

### T15 — Multi-line scroll
In editor, press ENTER 6+ times. Press DOWN past the last visible line. Verify amber `↓` appears, amber `↑` appears after scrolling past row 0, title stays fixed.
RESULT: PASS

### T16 — Erase a program
PRGM → ERASE tab → navigate to any slot → ENTER. Verify confirmation shows `1:Do not erase` / `2:Erase`. Press `2` — slot erased immediately, no extra ENTER. Press `1` on another — cancels immediately. Verify erased slot reverts to bare `N:PrgmN`.
Also verify: pressing a digit shortcut on the ERASE tab acts identically to navigating to that slot and pressing ENTER.
RESULT:

### T16b — ERASE shows all 37 slots
Open PRGM → ERASE tab. Verify all 37 slots visible (named and unnamed). Named show `N:PrgmN  NAME`; unnamed show `N:PrgmN`. Slot count matches EXEC and EDIT tabs.
RESULT:

---

### T17 — Programs survive power cycle
Create slot 1 program named `SAVE` with body `Disp "OK"`. Press 2nd+ON to save state. Perform a **full USB power cycle** (unplug/replug — not SWD reset). Open PRGM. Verify `1:Prgm1  SAVE` in list and body intact when opened.
RESULT:

---

### T18 — Run a program via EXEC
Create slot 1 named `HELLO` with body `Disp "HELLO"`. PRGM → EXEC → slot 1 → ENTER. Verify home shows `prgmHELLO` in the expression buffer (grey, left-aligned). Press ENTER. Verify `HELLO` appears in history output and `Done` appears as a right-aligned white result row.
RESULT:

### T19 — Run empty slot
On EXEC tab, select a slot with no body → ENTER. Verify `prgmN` inserted in expression buffer. Press ENTER. Verify `Done` appears, no error, no crash.
RESULT:
_Host test: test_prgm_exec empty-body group — skip if host suite passes and prgm_exec.c is unchanged._

### T20 — Expression evaluation and ANS
Run program: `2+2` / `Disp ANS`. Verify `4` appears in history and ANS=4 after completion.
RESULT:
_Host test: test_prgm_exec expression eval group._

### T21 — Input and variable store
Run program: `Input A` / `Disp A`. When `?` appears, type `7` and press ENTER. Verify only `?` is shown (not `A=?`). Verify `7` displayed by Disp and A=7 in variable store after.
RESULT:
_Host test: test_prgm_exec input group covers suspension and variable store; `?` display requires hardware._

### T22 — If single-line skip
Run program: `0->A` / `If A=1` / `Disp "YES"` / `Disp "DONE"`. Verify `YES` does NOT appear (condition false → next line skipped), `DONE` appears.
RESULT:
_Host test: test_prgm_exec If group; TEST menu access from editor requires hardware._

### T23 — EXEC number-key shortcut
Open PRGM EXEC tab. Press `1` without UP/DOWN. Verify menu closes and `prgm1` (or `prgmNAME` if named) appears in expression buffer — same as navigating to slot 1 and pressing ENTER.
RESULT: PASS

### T24 — CLEAR aborts execution
Run an infinite loop: `Lbl A` / `Disp "X"` / `Goto A`. While `X` rows appear, press CLEAR. Verify execution stops, home screen returns, no lockup. A few `X` rows before CLEAR registers is acceptable.
RESULT:

---

### T25 — Lbl/Goto jump
Run program: `Goto END` / `Disp "SKIP"` / `Lbl END` / `Disp "DONE"`. Verify `SKIP` does not appear, `DONE` appears once.
RESULT:
_Host test: test_prgm_exec Goto/Lbl group._

### T26 — Disp string — left-aligned
Run program: `Disp "HELLO"`. Verify `HELLO` appears left-aligned in a grey expression row. No result row below it.
RESULT:
_Host test: test_prgm_exec Disp group covers string→expression slot; visual rendering requires hardware._

### T27 — Disp variable — right-aligned
Run program: `5->A` / `Disp A`. Verify `5` appears right-aligned in a white result row. No grey expression row above it.
RESULT:
_Host test: test_prgm_exec Disp group covers variable→result slot; visual rendering requires hardware._

### T28 — Pause halts and resumes
Run program: `Disp "WAIT"` / `Pause` / `Disp "RESUMED"`. Verify `WAIT` appears, execution halts. Press ENTER — verify `RESUMED` appears and mode returns to normal.
RESULT:
_Host test: test_prgm_exec stop/pause group covers suspension; display and ENTER resume require hardware._

### T29 — Stop terminates early
Run program: `Disp "A"` / `Stop` / `Disp "B"`. Verify `A` appears, `B` does not, mode returns to normal.
RESULT:
_Host test: test_prgm_exec stop/pause group._

### T30 — IS>( increment and skip
Run program: `1->I` / `IS>(I,2)` / `Disp "SKP"` / `Disp I`. Verify I increments to 2; 2 is NOT > 2, so `SKP` appears and I shows `2`. Re-run with `2->I`: I increments to 3; 3>2 so `SKP` is skipped and only I=`3` appears.
RESULT:
_Host test: test_prgm_exec IS>/DS< group._

### T31 — DS<( decrement and skip
Run program: `3->I` / `DS<(I,3)` / `Disp "SKP"` / `Disp I`. Verify I decrements to 2; 2<3 so `SKP` IS skipped and only I=`2` appears.
RESULT:
_Host test: test_prgm_exec IS>/DS< group._

### T32 — Subroutine auto-return
Create slot 2: `Disp "SUB"`. Create slot 1: `Disp "MAIN"` / `prgm2` / `Disp "BACK"`. To insert `prgm2` in the editor: press PRGM → switch to EXEC tab → select slot 2 → ENTER. Run slot 1. Verify history shows `MAIN`, `SUB`, `BACK` in order.
RESULT:
_Host test: test_prgm_exec subroutine group covers call and auto-return; EXEC tab insertion requires hardware._

### T33b — EXEC sub-menu tab navigation and insertion
In editor, press PRGM (CTL tab). Press RIGHT → I/O tab. Press RIGHT again → EXEC tab. Verify all 37 slots listed. Navigate to slot 2 → ENTER. Verify `:prgm2` inserted on the current line. Also verify LEFT from CTL wraps to EXEC.
RESULT:

### T34 — Nested subroutine (2 levels deep)
Create slot 3: `Disp "DEEP"`. Create slot 2: `prgm3` / `Disp "MID"`. Create slot 1: `prgm2` / `Disp "TOP"`. Run slot 1. Verify history shows `DEEP`, `MID`, `TOP` in order.
RESULT:
_Host test: test_prgm_exec nested subroutine group; EXEC tab insertion requires hardware._

---

### T35 — Named program execution model
EXEC → named slot (e.g. `1:Prgm1  TEST`) → ENTER. Verify `prgmTEST` appears in expression buffer (grey, left-aligned). Press ENTER. Verify program runs, `Done` appears as right-aligned white result row.
RESULT:
_Host test: test_prgm_exec name lookup group; expression buffer insertion and Done display require hardware._

### T35b — Unnamed slot execution model
EXEC → unnamed slot (e.g. `3:Prgm3`) → ENTER. Verify `prgm3` (canonical slot ID) appears in expression buffer. Press ENTER. Verify `Done` appears.
RESULT:
_Host test: test_prgm_exec name lookup group._

### T36 — ClrHome clears history
Run program: `Disp "LINE1"` / `Disp "LINE2"` / `ClrHome` / `Disp "AFTER"`. Verify `LINE1` and `LINE2` are cleared after `ClrHome`. Only `AFTER` remains visible.
RESULT:
_Host test: test_prgm_exec ClrHome group._

### T37 — DispGraph and DispHome
Ensure at least one Y= equation is entered (e.g. `Y1=X`). Run program: `DispGraph` / `Pause` / `DispHome`. Verify graph appears after `DispGraph` **without lockup or hardware reset**. Press ENTER at Pause — home screen returns.
RESULT:

---

### T41 — Cursor blinks in editor
Open any program in editor. Observe cursor for ~2 seconds without pressing any key. Verify cursor blinks at ~530 ms interval.
RESULT:

### T42 — 2nd mode cursor
With editor open, press 2nd. Verify cursor turns amber with `^`. Pressing any non-modifier key returns cursor to white (2nd consumed).
RESULT: PASS

### T43 — ALPHA mode (single)
With editor open, press ALPHA once. Verify cursor turns green with `A`. Pressing a letter inserts it into the **program body** (not the calculator expression), then cursor returns to white. Pressing ALPHA again cancels ALPHA mode.
RESULT:

### T43b — ALPHA LOCK mode in editor
With editor open, press 2nd+ALPHA (ALPHA LOCK). Verify cursor shows green `A`. Multiple letter keypresses should all insert into the **program body**. ALPHA LOCK stays active until ALPHA is pressed again.
RESULT: PASS

### T44 — Cursor blinks on name-entry screen
Open name-entry screen (EDIT tab → empty slot → ENTER). Observe for ~2 seconds. Verify cursor blinks.
RESULT: PASS

---

### T45 — Body-only slot opens editor directly
Open editor, type content, press CLEAR from name-entry to skip the name. Reopen PRGM → EDIT → same slot → ENTER. Verify editor opens directly — name-entry screen does NOT appear. Body is preserved.
RESULT: PASS

### T46 — Sub-menu tab wrap
In editor, press PRGM (CTL tab). Press LEFT — wraps to EXEC. LEFT again → I/O. LEFT again → CTL. Verify RIGHT from CTL → I/O → EXEC → CTL.
RESULT: PASS

### T46b — EXEC sub-menu digit and ALPHA shortcuts
On EXEC sub-menu, press `2`. Verify `:prgm2` inserted immediately. Open EXEC tab again. Press ALPHA+A. Verify `:prgmA` inserted (or `:prgmNAME` if slot A is named).
RESULT:
