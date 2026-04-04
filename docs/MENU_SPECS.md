# Menu Specs

Single source of truth for all calculator menu layouts, navigation rules, and implementation status. Read this file before working on any menu UI.

---

## General Rules

These rules apply to every menu unless a specific menu notes an exception.

- **Font:** the top tab bar uses the same font as the item rows below it (JetBrains Mono 24px).
- **Scrolling:** when a list overflows the visible window, the tab bar stays fixed and items scroll beneath it.
- **Overflow indicators:** ↓ (U+2193) at the bottom row means the list continues below; ↑ (U+2191) at the top means it continues above. Both glyphs are already in the font (`\xE2\x86\x93` / `\xE2\x86\x91`). The indicator overwrites the `:` prefix character of the adjacent item row — the indicator label has an opaque black background (`LV_OPA_COVER`) to cover it cleanly.
- **Cursor entry:** INS toggles overwrite/insert mode; LEFT/RIGHT move the cursor within an editable field. This applies in field-editor screens (RANGE, ZOOM FACTORS) but not in plain list menus.
- **Tab navigation:** LEFT/RIGHT switch between tabs in any multi-tab menu.
- **Item selection:** UP/DOWN move the highlight; ENTER or a direct number key select an item.
- **Highlight colour:** yellow (`0xFFFF00`) for MATH, TEST, MATRIX, and PRGM menus.
- **Insertion behaviour:** selecting an item normally inserts a token string at the current cursor position in the calling expression and closes the menu, returning to the calling screen. Exceptions are noted per menu.
- **Exit:** CLEAR closes most menus and returns to the calling screen. Some menus also accept 2nd+<key> to exit (noted per menu).

---

## Menus

### MODE screen

No title text. The full screen is filled with option rows; no tab bar.

| Row | Options | Wired? |
|-----|---------|--------|
| 1 | Normal \| Sci \| Eng | No — display notation not implemented |
| 2 | Float \| 0 1 2 3 4 5 6 7 8 9 | Yes — `mode_committed[1]`, `Calc_SetDecimalMode()` |
| 3 | Radian \| Degree | Yes — `mode_committed[2]`, `angle_degrees` |
| 4 | Function \| Param | No — parametric graphing not implemented |
| 5 | Connected \| Dot | No — Connected/Dot curve rendering not implemented |
| 6 | Sequential \| Simul | No — simultaneous graphing not implemented |
| 7 | Grid off \| Grid on | Yes — `mode_committed[6]`, `graph_state.grid_on` |
| 8 | Polar \| Seq | No — polar/sequence graphing not implemented |

- LEFT/RIGHT moves the selection within a row. UP/DOWN changes the active row.
- ENTER commits the highlighted selection and stays on the MODE screen.
- Active selections stored in `mode_committed[8]`; wired rows take effect immediately on ENTER.
- MODE key opens the MODE screen from any screen (handled as an early-return check before all mode handlers in `Execute_Token`).

---

### MATH menu

Four tabs (MATH / NUM / HYP / PRB). Tab LEFT/RIGHT; item UP/DOWN; ENTER or a number key inserts the token and closes the menu.

**MATH tab:**
```
MATH NUM HYP PRB
1:R>P(
2:P>R(
3:³           (cubed symbol)
4:∛(          (cube root symbol)
5: !
6:°           (degree symbol)
7↓r           (↓ = overflow indicator; list continues)
  8:NDeriv(   (visible after scrolling)
```

**NUM tab:**
```
MATH NUM HYP PRB
1:Round(
2:iPart
3:fPart
4:int(
```

**HYP tab:**
```
MATH NUM HYP PRB
1:sinh(
2:cosh(
3:tanh(
4:asinh(
5:acosh(
6:atanh(
```

**PRB tab:**
```
MATH NUM HYP PRB
1:rand
2: nPr        (spaces before and after are inserted into the expression)
3: nCr        (spaces before and after are inserted into the expression)
```

---

### ZOOM menu

No tabs. Single list with 8 items; items 7–8 require scrolling.

```
ZOOM
1:Box
2:Zoom In
3:Zoom Out
4:Set Factors
5:Square
6:Standard
7↓Trig        (↓ = overflow indicator; list continues)
  8:Integer   (visible after scrolling)
```

Navigation: UP/DOWN cursor; ENTER selects. Number keys 1–8 are direct shortcuts.

Item behaviour:
- **1: Box** → enters `MODE_GRAPH_ZBOX` (rubber-band zoom)
- **2: Zoom In / 3: Zoom Out** → uses `zoom_x_fact` / `zoom_y_fact`
- **4: Set Factors** → opens ZOOM FACTORS sub-screen (see below)
- **5–8** → fixed presets applied immediately via `apply_zoom_preset()`; graph renders at once

**Set Factors sub-screen** (`ui_graph_zoom_factors_screen`):
```
ZOOM FACTORS
XFact=4
YFact=4
```
- State: `zoom_x_fact`, `zoom_y_fact`, `zoom_factors_field` (0/1), `zoom_factors_buf[16]`, `zoom_factors_len`, `zoom_factors_cursor`.
- UP/DOWN move between the two fields; digit keys and DEL edit the value; ENTER commits and exits to the ZOOM menu.
- Cursor box (`zoom_factors_cursor_box` / `zoom_factors_cursor_inner`) works the same as the RANGE editor cursor.

---

### RANGE menu

No tabs. Seven labelled fields; cursor edits the value to the right of `=`.

```
RANGE
Xmin=
Xmax=
Xscl=
Ymin=
Ymax=
Yscl=
Xres=
```

- ENTER / UP / DOWN commit the current field and move to the adjacent one.
- CLEAR clears any in-progress edit; if the field is already empty, exits the screen.
- ZOOM from RANGE navigates to the ZOOM menu (does not auto-trigger ZStandard).

---

### MATRIX menu

Two tabs (MATRIX / EDIT). Navigation differs from other menus — see note below.

**Navigation model — important:** The MATRIX menu does **not** behave like MATH or TEST.
- Items in the **MATRIX tab** insert a function token (e.g. `det(`, `rowSwap(`) into the calling expression and close the menu.
- Items in the **EDIT tab** always open that matrix's cell editor (`MODE_MATRIX_EDIT`) regardless of the calling screen. They never insert `[A]`, `[B]`, or `[C]` into any expression. Matrix names reach the expression only via the dedicated MTRX_A/B/C key tokens (2nd layer on keypad).

**MATRIX tab:**
```
MATRIX EDIT
1:RowSwap(
2:Row+(
3:*Row(
4:*Row+(
5:det(
6:T            (transpose superscript)
```

**EDIT tab:**
```
MATRIX EDIT
1:[A] 3×3
2:[B] 3×3
3:[C] 3×3
```

Pressing ENTER on any EDIT item opens the cell editor for that matrix (`MODE_MATRIX_EDIT`). It does not insert `[A]`, `[B]`, or `[C]` into any expression.

**Cell editor** (`MODE_MATRIX_EDIT`):
- Shows 7 visible cell rows; ↑/↓ amber scroll indicators appear when the matrix is taller than the viewport.
- Navigating UP past the first cell enters **dim mode**: the cursor lands on the title label (`[A] RxC`); LEFT/RIGHT switch between the rows digit and cols digit; digit keys resize the matrix live.
- Dim-mode title renders in yellow; normal cell mode in white.

---

### TEST menu

No tabs. Yellow "TEST" title at the top row; 6 items below.

```
TEST
1:=
2:≠
3:>
4:≥
5:<
6:≤
```

- Navigation: UP/DOWN cursor; ENTER selects. Number keys 1–6 are direct shortcuts.
- CLEAR or 2nd+MATH exits and returns to the calling screen.
- Accessible from normal mode (2nd+MATH) and from the Y= editor.
- Selected operator is inserted at the cursor position as a UTF-8 string.
- All 6 operators are fully evaluated by `calc_engine.c` — they return 1 (true) or 0 (false).
- `and` / `or` / `not` are not present on the TI-81 and are not planned.

---

### STAT menu

Three tabs (CALC / DRAW / DATA). Not yet implemented.

**CALC tab:**
```
CALC DRAW DATA
1:1-Var
2:LinReg
3:LnReg
4:ExpReg
5:PwrReg
```

**DRAW tab:**
```
CALC DRAW DATA
1:Hist
2:Scatter
3:xyLine
```

**DATA tab:**
```
CALC DRAW DATA
1:Edit
2:ClrStat
3:xSort
4:ySort
```

---

### DRAW menu

No tabs. Single list. Not yet implemented.

```
DRAW
1:ClrDraw
2:Line(
3:PT-On(
4:PT-Off(
5:PT-Chg(
6:DrawF
7:Shade(
```

---

### VARS menu (VARS key) — **IMPLEMENTED** (`ui_vars.c`, hardware validation pending P31h)

Five tabs: XY / Σ / LR / DIM / RNG. Tab LEFT/RIGHT; item UP/DOWN or number key; ENTER inserts the current numeric value of the selected variable at the cursor. DIM tab has 6 items (Arow–Ccol); RNG tab has 10 items with scroll. ȳ/Sy/σy/Σy/Σy²/Σxy computed on-the-fly from `stat_data`; RegEQ inserts `aX+b` from `stat_results.reg_a/reg_b`.

> **Font note:** x̄ (U+E000 custom PUA glyph) and ȳ (U+0233) are in the custom TTF and regenerated font files. Σ (U+03A3) is already in the font. See CLAUDE.md gotcha #14 for font regeneration commands.

**XY tab** (statistics summary variables):
```
XY Σ LR DIM RNG
1:n
2:x̄
3:Sx
4:σx
5:ȳ
6:Sy
7:σy
```

**Σ tab** (summation variables):
```
XY Σ LR DIM RNG
1:Σx
2:Σx²
3:Σy
4:Σy²
5:Σxy
```

**LR tab** (linear regression variables):
```
XY Σ LR DIM RNG
1:a
2:b
3:r
4:RegEQ
```

**DIM tab** (matrix dimension variables):
```
XY Σ LR DIM RNG
1:Arow
2:Acol
3:Brow
4:Bcol
5:Crow
6:Ccol
7:Dim{x}
```

**RNG tab** (window range variables):
```
XY Σ LR DIM RNG
1:Xmin
2:Xmax
3:Xscl
4:Ymin
5:Ymax
6:Yscl
7↓Xres   (↓ = overflow indicator; list continues)
  8:Tmin
  9:Tmax
  0:Tstep
```

---

### Y-VARS menu (2nd+VARS)

Three tabs: Y / ON / OFF. Tab LEFT/RIGHT; item UP/DOWN or number key.
- **Y tab** inserts an equation reference token into the expression.
- **ON / OFF tabs** enable or disable the named equation directly — no token is inserted.

**Y tab** (equation references):
```
Y ON OFF
1:Y₁
2:Y₂
3:Y₃
4:Y₄
5:X₁t
6:Y₁t
7↓X₂t   (↓ = overflow indicator; list continues)
  8:Y₂t
  9:X₃t
  0:Y₃t
```

**ON tab** (enable equations):
```
Y ON OFF
1:All-On
2:Y₁-On
3:Y₂-On
4:Y₃-On
5:Y₄-On
6:X₁t-On
7↓X₂t-On   (↓ = overflow indicator; list continues)
  8:X₃t-On
```

**OFF tab** (disable equations):
```
Y ON OFF
1:All-Off
2:Y₁-Off
3:Y₂-Off
4:Y₃-Off
5:Y₄-Off
6:X₁t-Off
7↓X₂t-Off   (↓ = overflow indicator; list continues)
  8:X₃t-Off
```

---

### PRGM editor sub-menus

Accessible only while editing a program (PRGM → EDIT → select a slot). A three-tab bar (CTL / I/O / EXEC) replaces the standard calculator interface. Tab LEFT/RIGHT switches tabs; UP/DOWN highlights items; ENTER or a number key inserts the selected token at the cursor in the program editor. Overflow indicator (↓/↑) overwrites the `:` prefix glyph, same as MATH and ZOOM menus.

**CTL tab:**
```
CTL I/O EXEC
1:Lbl
2:Goto
3:If
4:IS>(
5:DS<(
6:Pause
7↓End
8:Stop
```

**I/O tab:**
```
CTL I/O EXEC
1:Disp
2:Input
3:DispHome
4:DispGraph
5:ClrHome
```

**EXEC tab:**
```
CTL I/O EXEC
1:Prgm1
2:Prgm2
…
```

Lists all 37 program slots with user-assigned names (same `N:PrgmN  USER_NAME` format as the main PRGM EXEC screen). Selecting a slot inserts a `prgm<name>` subroutine-call token at the cursor. Overflow indicators (↓/↑) overwrite the `:` prefix glyph when the list scrolls.
