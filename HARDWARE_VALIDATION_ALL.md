# Neo-81 — Full Hardware Validation Checklist
_Generated 2026-04-18. All features implemented; none yet verified on hardware._
_Mark each test: ✅ PASS  ❌ FAIL  ⚠️ PARTIAL_

---

## Pre-flight (do before any section)

- [ ] Latest firmware builds with 0 errors
- [ ] All host tests pass (`cmake -S App/Tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`)
- [ ] Flash to board; power-cycle (USB unplug/replug — not just SWD reset)

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

### Y= editor

| # | Action | Expected | Result |
|---|---|---|---|
| 10 | Press `Y=` | Cursor blinks on first equation field | |
| 11 | Press `2ND` | Cursor turns amber with `^` | |
| 12 | Press any key to resolve 2ND | Cursor resets | |
| 13 | Press `2ND+INS` | Cursor changes to underscore style | |
| 14 | Press `2ND+INS` again | Cursor returns to block | |

### RANGE editor

| # | Action | Expected | Result |
|---|---|---|---|
| 15 | Press `RANGE` | Cursor blinks in Xmin field | |
| 16 | Press `2ND+INS` | Cursor changes to underscore style | |
| 17 | Press `2ND+INS` again | Cursor returns to block | |

### ZOOM FACTORS editor

| # | Action | Expected | Result |
|---|---|---|---|
| 18 | `ZOOM` → Select Factors | Cursor blinks in XFact field | |
| 19 | Press `2ND+INS` | Cursor changes to underscore style | |
| 20 | Press `2ND+INS` again | Cursor returns to block | |

### Matrix editor — insert mode must NOT change cursor shape

| # | Action | Expected | Result |
|---|---|---|---|
| 21 | `MATRX` → EDIT → select matrix → navigate to a cell | Cursor is full-height grey block | |
| 22 | Press `2ND+INS` | Cursor shape does **not** change | |
| 23 | Press `2ND` | Cursor turns amber with `^` | |
| 24 | Press `ALPHA` | Cursor turns green with `A` | |

### PRGM name entry — insert mode must NOT change cursor shape

| # | Action | Expected | Result |
|---|---|---|---|
| 25 | `PRGM` → EDIT → empty slot → ENTER | Cursor blinks in name field; full-height block | |
| 26 | Press `2ND+INS` | Cursor shape does **not** change | |

### PRGM line editor

| # | Action | Expected | Result |
|---|---|---|---|
| 27 | `PRGM` → EDIT → select a program | Cursor blinks on program line | |
| 28 | Press `2ND+INS` | Cursor changes to underscore style | |
| 29 | Press `2ND+INS` again | Cursor returns to block | |

**P28 sign-off:** all 29 pass → delete `docs/p28_cursor_manual_tests.md`, add row to `docs/PROJECT_HISTORY.md`.

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
| 8 | Type `Shade(-1,1)` → ENTER | Horizontal band between y=−1 and y=1 shaded on graph canvas | |
| 9 | After drawing, press `ZOOM` → `Standard` → return to graph | Draw layer content still visible (persists across re-render) | |
| 10 | Type `ClrDraw` → ENTER | All drawn content cleared from graph canvas | |

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

---

## Section 6 — Y-VARS menu (P32h)
_Validates `2nd+VARS` equation reference and enable/disable tabs._

**Setup:** Ensure Y₁=X is entered and enabled. Enter Y₂=X² as well.

| # | Test | Expected | Result |
|---|---|---|---|
| 1 | Press `2ND+VARS` | 3-tab menu opens: Y / ON / OFF. LEFT/RIGHT navigate tabs; UP/DOWN navigate items; digit shortcuts 1–5 work; CLEAR exits | |
| 2 | Y tab → select `1:Y₁` | String `Y₁` inserted into expression buffer | |
| 3 | Store `3→X`; type `Y₁` (via Y-VARS) → ENTER | Result is 3 (evaluates Y₁=X at X=3) | |
| 4 | Press `Y=`; move cursor into Y₁ equation field; press `2ND+VARS` → Y → select `2:Y₂` | `Y₂` inserted into the Y= equation field (not home screen) | |
| 5 | OFF tab → `1:All-Off` → press `Y=` | All equations show `-` (disabled); no `=` visible | |
| 6 | ON tab → `2:Y₁-On` → press `Y=` | Y₁ shows `=` (enabled); Y₂–Y₄ remain `-` | |
| 7 | Y tab → press `1` directly (no UP/DOWN first) | `Y₁` inserted immediately — digit shortcut works without navigating | |

---

## Section 7 — PRGM system (P10)
_50 tests. Hardware: STM32F429I-DISC1. Run after flashing latest build._
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

---

## Summary

| Section | Feature | Tests | Result |
|---|---|---|---|
| 1 | Cursor rendering (P28) | 29 | |
| 2 | Parametric graphing (P35h) | 6 | |
| 3 | DRAW menu (P29h) | 10 | |
| 4 | STAT menu (P30h) | 10 | |
| 5 | VARS menu (P31h) | 9 | |
| 6 | Y-VARS menu (P32h) | 7 | |
| 7 | PRGM system (P10) | 50 | |
| **Total** | | **121** | |

---

## Sign-off

When all sections pass:
1. Delete `docs/p28_cursor_manual_tests.md` (P28 complete)
2. Delete `docs/prgm_manual_tests.md` (P10 complete)
3. Add a Resolved Items row to `docs/PROJECT_HISTORY.md` for each section
4. Remove completed hardware items from "Next session priorities" in `CLAUDE.md`
