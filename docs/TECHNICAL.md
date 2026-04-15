# Technical Reference — STM32F429 TI-81 Calculator

Full architecture, build configuration, and implementation details.

---

## Project Structure

Application code and CubeMX-generated code are in separate top-level directories.
`App/` contains all custom application code. `Core/` contains only files generated
or managed by STM32CubeMX (safe to regenerate without touching application code).

```
App/                            ← Custom application code (never touched by CubeMX)
  Src/
    app_init.c                  RTOS object creation, hardware bring-up, LVGL init, render loop
    calculator_core.c           Calculator UI, token processing, expression building
    calc_engine.c               Expression parser and evaluator (shunting-yard + RPN)
    expr_util.c                 Pure expression-buffer helpers (UTF-8, insert, delete, cursor)
    graph.c                     Graph canvas, axes, tick marks, curve renderer, STAT plot renderers
    graph_draw.c                Draw layer — persistent user-drawn overlay (DRAW menu; SDRAM 0xD0080800)
    graph_ui.c                  Graph screen UI and handlers (Y=, RANGE, ZOOM, TRACE, ZBox)
    persist.c                   FLASH sector 10 erase/write/load for calculator state
    prgm_exec.c                 Program storage (FLASH sector 11 erase/write/load) and execution engine
    ui_matrix.c                 Matrix cell editor UI (extracted module)
    ui_stat.c                   STAT menu, DATA list editor, and results screen UI (extracted module)
    calc_stat.c                 Pure statistical math layer — 1-Var, LinReg, LnReg, ExpReg, PwrReg, sort, clear
    ui_prgm.c                   Program menu and editor UI (extracted module)
    ui_draw.c                   DRAW menu UI and command dispatch (extracted module)
    ui_vars.c                   VARS menu UI — 5-tab (XY/Σ/LR/DIM/RNG) value-insert menu (extracted module)
    ui_yvars.c                  Y-VARS menu UI — 3-tab (Y/ON/OFF) equation-reference insert and enable/disable menu (extracted module)
    ui_param_yeq.c              Parametric Y= editor screen (X₁t/Y₁t … X₃t/Y₃t; extracted module)
  Inc/
    app_init.h                  App_RTOS_Init() and App_DefaultTask_Run() declarations
    app_common.h                Shared types, handles and function declarations
    calc_engine.h               Math engine interface
    calc_internal.h             Shared internal state for calculator UI modules
    calc_stat.h                 Statistical math API (no LVGL/HAL dependencies)
    expr_util.h                 Expression buffer utility API
    graph.h                     Graphing subsystem interface (Y= renderer, trace, ZBox, stat plots)
    graph_draw.h                Draw layer API (Graph_DrawLayer*, Graph_DrawF, Graph_Shade, Graph_ApplyDrawLayer)
    graph_ui.h                  Graph screen UI interface (re-exports ui_param_yeq.h)
    persist.h                   Persistent storage API
    prgm_exec.h                 Program storage/FLASH persistence and execution engine API
    ui_matrix.h                 Matrix editor UI interface
    ui_stat.h                   STAT menu UI interface (StatMenuState_t, handler protos)
    ui_prgm.h                   Program menu UI interface
    ui_draw.h                   DRAW menu UI interface (DrawMenuState_t, handler protos)
    ui_vars.h                   VARS menu UI interface (VarsMenuState_t, handler protos)
    ui_yvars.h                  Y-VARS menu UI interface (YVarsMenuState_t, handler protos)
    ui_param_yeq.h              Parametric Y= editor interface (param_yeq_init_screen, handler)
    ui_palette.h                Named colour constants (COLOR_BLACK, COLOR_YELLOW, etc.)
  Fonts/
    JetBrainsMono-Regular.ttf        Source font (Apache 2.0)
    JetBrainsMono-Regular-Custom.ttf Source font with PUA glyphs U+E000 (x̄) and U+E001 (⁻¹); required for font regeneration
    jetbrains_mono_24.c              LVGL bitmap font 24px (generated from Custom TTF)
    jetbrains_mono_20.c              LVGL bitmap font 20px (generated from Custom TTF)
  HW/
    Keypad/
      keypad.c                  Matrix scan driver, FreeRTOS scan task, arrow auto-repeat
      keypad.h                  Key ID enum and scan function prototype
      keypad_map.c              Hardware key to token lookup table
      keypad_map.h              Token_t enum and lookup table declaration
  Display/
    lv_conf.h                   LVGL configuration
    lv_port_disp.c/h            LVGL display port — LTDC framebuffer flush with rotation
    lv_port_indev.c/h           LVGL input device port — keypad registration
  Tests/
    CMakeLists.txt              Host test build (see docs/TESTING.md for suite list and counts)
    test_calc_engine.c          Tokenizer, shunting-yard, RPN, matrix
    test_expr_util.c            UTF-8 cursor, insert/delete, matrix atomicity
    test_persist_roundtrip.c    PersistBlock_t checksum and round-trip
    test_prgm_exec.c            Active command handlers, control flow, subroutines
    test_normal_mode.c          handle_normal_mode dispatch, all 8 sub-handlers
    test_param.c                Parametric equation preparation and evaluation
    test_stat.c                 1-Var, LinReg/LnReg/ExpReg/PwrReg, SortX/Y, Clear
    prgm_exec_test_stubs.h      Inline stubs for host-compilation of prgm_exec.c
    calculator_core_test_stubs.h  LVGL/RTOS/graph_ui/ui_matrix/ui_prgm/ui_stat stubs for host-compilation of calculator_core.c

Core/                           ← CubeMX-generated code (regenerated from .ioc)
  Src/
    main.c                      HAL init, peripheral setup, minimal USER CODE call-throughs
    freertos.c                  Generated FreeRTOS task stubs
    stm32f4xx_it.c              Interrupt handlers
    stm32f4xx_hal_msp.c         Peripheral init callbacks
    system_stm32f4xx.c          Clock and memory initialisation
    sysmem.c / syscalls.c       libc stubs
  Inc/
    main.h                      Generated declarations
    FreeRTOSConfig.h            FreeRTOS configuration (check after every CubeMX regen)
    stm32f4xx_hal_conf.h        HAL middleware configuration
    stm32f4xx_it.h              Interrupt handler declarations

Drivers/
  BSP/
    STM32F429I-Discovery/       ST board support package (LCD, SDRAM)
    Components/                 ILI9341 driver and fonts

Middlewares/
  Third_Party/
    lvgl/                       LVGL v9.x source (unmodified, gitignored)
```

---

## Architecture

Three FreeRTOS tasks coordinate the system:

**DefaultTask** (8192 word stack)
Initialises SDRAM, LCD, and LVGL in sequence. Drives the LVGL timer handler
every 5ms. Signals `xLVGL_Ready` when initialisation is complete.

**KeypadTask** (2048 word stack)
Scans the 7×8 key matrix every 20ms. On a new keypress, calls
`Process_Hardware_Key()` which translates the hardware ID into a token and
posts it to `keypadQueueHandle`. Arrow keys auto-repeat when held (400ms
initial delay, 80ms repeat rate).

**CalcCoreTask** (2048 word stack)
Waits on `xLVGL_Ready` before creating any UI elements. Blocks on
`keypadQueueHandle` and processes tokens via `Execute_Token()`.

### Synchronisation

| Primitive           | Purpose                                                  |
|---------------------|----------------------------------------------------------|
| `xLVGL_Mutex`       | Mutual exclusion for all LVGL API calls across tasks     |
| `xLVGL_Ready`       | Binary semaphore — signals when LVGL init is complete    |
| `keypadQueueHandle` | Token queue from KeypadTask to CalcCoreTask              |

### UI Extensibility Pattern

To avoid monolithic growth in `calculator_core.c`, complex sub-menus and distinct feature screens (e.g., Matrix Editor, Program Editor) should be extracted into their own modules within `App/Src/`.

1. **Standalone Modules:** Create isolated `ui_<feature>.c/.h` pairs containing the mode-specific LVGL initialization, display updates, and token handling loops (e.g., `ui_matrix.c`).
2. **Shared UI Context:** Include `calc_internal.h` in these modules to access global calculator state (`current_mode`, `ans`, `insert_mode`, `cursor_visible`) and cross-module UI functions (`screen_create`, `lvgl_lock`, `tab_move`, `menu_insert_text`).
3. **Core Integration:** In `calculator_core.c`, remove mode-specific logic. Initialize the extracted screen once in `StartCalcCoreTask` (e.g., `ui_init_matrix_screen()`), and delegate mode-specific token handling to the public handlers defined in `ui_<feature>.h`.

> [!NOTE]
> **PRGM Module:** `ui_prgm.c` handles all PRGM UI screens (EXEC, EDIT, NEW, CTL, I/O). The execution engine lives in `prgm_exec.c`. Both are feature-complete as of PERSIST_VERSION 6. Hardware validation (P10) is the only remaining gate — see `docs/prgm_manual_tests.md`.

---

## Keypad Driver

`App/HW/Keypad/keypad.c` implements the 7×8 matrix scan and auto-repeat:

- A-lines (columns) are driven HIGH one at a time; B-lines (rows) are read back
- Key ID = `(row * 7) + col`; range 1–55; `0xFF` = no key pressed
- `StartKeypadTask` scans every 20 ms; calls `Process_Hardware_Key()` on a new keypress
- Arrow keys auto-repeat: 400 ms initial delay, 80 ms repeat rate (`KEY_REPEAT_DELAY_TICKS=20`, `KEY_REPEAT_RATE_TICKS=4`)
- All other keys fire once on initial press only

`Process_Hardware_Key()` in `calculator_core.c`:
- Translates raw key ID → `Token_t` using `TI81_LookupTable` in `keypad_map.c`
- Handles 2ND/ALPHA mode layers and STO pending flag before queuing the token to `keypadQueueHandle`
- Sticky 2ND/ALPHA: pressing the modifier a second time cancels it; `return_mode` saves the pre-modifier mode to restore after one keypress

---

## Display

The ILI9341 is driven via the STM32 LTDC RGB interface. In this mode the
display controller's internal MADCTL rotation register has no effect — the
LTDC scans the framebuffer directly to the panel in a fixed portrait order.

Landscape orientation is achieved by rotating pixel coordinates in software
inside the LVGL flush callback:

```
logical (x, y) on 320x240 → physical (y, 319-x) on 240x320
```

LVGL is configured with a 320x240 logical canvas. All application code works
in landscape coordinates. The rotation is invisible above the port layer.

---

## UI Layout

The full 320×240 display is used as a scrolling console — there is no status
bar. Eight rows of 30px each fill the screen exactly.

```
┌────────────────────────────────────────┐  y=0
│ 3+sin(45)*2                            │  expression (grey)
│                                 24.000 │  result (white)
│ 12/4                                   │
│                                  3.000 │
│ ANS+1                                  │
│                                  4.000 │
│ ANS*2                                  │
│ 8.0+sin(ANS)*cos(ANS)+tan(ANS)+log(ANS│  long expression: wraps to next row
│ )+ln(ANS)█                             │  continuation + cursor
└────────────────────────────────────────┘  y=240
```

History entries scroll upward as new results are added. The current expression
wraps across multiple rows when it exceeds the line width. The cursor reflects
the active input mode:

| Cursor appearance        | Mode                     |
|--------------------------|--------------------------|
| Blinking grey block      | Normal                   |
| Steady amber `^` block   | 2nd (next key is 2nd fn) |
| Steady green `A` block   | ALPHA / A-LOCK / STO→    |

Font: JetBrains Mono 24 (monospaced) for all content. The number of characters
per row is measured at runtime from the glyph width.

Display constants:

```c
DISP_ROW_COUNT = 8     // rows
DISP_ROW_H     = 30   // px per row  (8 × 30 = 240 px — fills screen exactly)
MAX_EXPR_LEN   = 96   // max expression bytes (~4 wrapped rows)
```

History entries occupy two rows: expression (left-aligned, grey `0x888888`) on the even row; result (right-aligned, white `0xFFFFFF`) on the odd row. The current expression wraps across multiple rows when it exceeds the line width. `expr_chars_per_row` is measured at init from the glyph width; `ui_refresh_display` slices the expression into segments and renders each onto its own row.

Key display functions:

```c
void Update_Calculator_Display(void);  // call after any keypress that changes the expression
// ui_refresh_display() and ui_update_history() are static — call via Update_Calculator_Display()
```

### Font Regeneration

The LVGL bitmap font files are generated from `App/Fonts/JetBrainsMono-Regular-Custom.ttf` using `lv_font_conv`. Always use the **Custom TTF** — it adds two Private Use Area glyphs not in the stock JetBrains Mono: **U+E000** (x̄, xbar) and **U+E001** (⁻¹, superscript negative one).

Install: `npm i -g lv_font_conv`

```bash
lv_font_conv --font App/Fonts/JetBrainsMono-Regular-Custom.ttf \
  -r 0x20-0x7E \
  -r 0x00B0,0x00B2,0x00B3,0x00B9 \
  -r 0x0233 \
  -r 0x03A3,0x03B8,0x03C0,0x03C3 \
  -r 0x2081-0x2084 \
  -r 0x221A -r 0x25B6 -r 0x2191,0x2193 -r 0x2260,0x2264,0x2265 \
  -r 0xE000,0xE001 \
  --size 24 --format lvgl --bpp 4 -o App/Fonts/jetbrains_mono_24.c --no-compress

lv_font_conv --font App/Fonts/JetBrainsMono-Regular-Custom.ttf \
  -r 0x20-0x7E \
  -r 0x00B0,0x00B2,0x00B3,0x00B9 \
  -r 0x0233 \
  -r 0x03A3,0x03B8,0x03C0,0x03C3 \
  -r 0x2081-0x2084 \
  -r 0x221A -r 0x25B6 -r 0x2191,0x2193 -r 0x2260,0x2264,0x2265 \
  -r 0xE000,0xE001 \
  --size 20 --format lvgl --bpp 4 -o App/Fonts/jetbrains_mono_20.c --no-compress
```

Current Unicode ranges: ASCII (0x20–0x7E), °²³¹, ȳ (U+0233), Σθπσ (Greek), ₁₂₃₄ (U+2081–2084), √ ▶ ↑↓, ≠≤≥, U+E000 x̄ / U+E001 ⁻¹ (PUA glyphs).

**Not in custom TTF** (requires further font editing before use): ᵀ U+1D40 (superscript T), ⁻ U+207B (superscript minus alone), ₜ U+209C (subscript t).

---

## Input Modes

`CalcMode_t` tracks the current input state:

| Mode                       | Description                                             |
|----------------------------|---------------------------------------------------------|
| `MODE_NORMAL`              | Standard expression entry                               |
| `MODE_2ND`                 | 2nd function layer — one-shot, reverts after next key   |
| `MODE_ALPHA`               | Alpha character layer — one-shot, reverts after next key|
| `MODE_ALPHA_LOCK`          | Alpha stays active (entered via 2nd+ALPHA)              |
| `MODE_GRAPH_YEQ`           | Y= equation editor (includes enable/disable toggles)    |
| `MODE_GRAPH_RANGE`         | RANGE field editor                                      |
| `MODE_GRAPH_ZOOM`          | ZOOM preset menu                                        |
| `MODE_GRAPH_TRACE`         | Trace cursor active on graph                            |
| `MODE_GRAPH_ZBOX`          | ZBox rubber-band zoom selection                         |
| `MODE_GRAPH_PARAM_YEQ`     | Parametric equation editor (X₁t/Y₁t … X₃t/Y₃t rows)   |
| `MODE_MODE_SCREEN`         | MODE settings screen                                    |
| `MODE_MATH_MENU`           | MATH/NUM/HYP/PRB function menu                          |
| `MODE_GRAPH_ZOOM_FACTORS`  | ZOOM FACTORS sub-screen (XFact/YFact editing)           |
| `MODE_TEST_MENU`           | TEST comparison-operator menu                           |
| `MODE_MATRIX_MENU`         | MATRIX/EDIT tab menu                                    |
| `MODE_MATRIX_EDIT`         | Matrix cell editor ([A]/[B]/[C])                        |
| `MODE_PRGM_MENU`           | PRGM EXEC/EDIT/ERASE tab selection                      |
| `MODE_PRGM_EDITOR`         | Program line editor                                     |
| `MODE_PRGM_CTL_MENU`       | PRGM CTL sub-menu (If, For, While…)                     |
| `MODE_PRGM_IO_MENU`        | PRGM I/O sub-menu (Disp, Input…)                        |
| `MODE_PRGM_EXEC_MENU`      | PRGM EXEC sub-menu (subroutine slot picker from editor) |
| `MODE_PRGM_RUNNING`        | Program execution in progress                           |
| `MODE_PRGM_NEW_NAME`       | Name-entry dialog for new program                       |
| `MODE_STAT_MENU`           | STAT CALC/DRAW/DATA tab menu                            |
| `MODE_STAT_EDIT`           | STAT DATA list editor (x,y pair entry)                  |
| `MODE_STAT_RESULTS`        | STAT calculation results readout                        |
| `MODE_DRAW_MENU`           | DRAW 7-item menu (`2nd+PRGM`): ClrDraw, Line(, PT-On/Off/Chg(, DrawF, Shade( |
| `MODE_VARS_MENU`           | VARS 5-tab menu (XY/Σ/LR/DIM/RNG) — inserts current variable value           |

Pressing 2nd or ALPHA a second time cancels the modifier (toggle). `STO→` sets
a pending flag and automatically enters ALPHA mode for the next keypress so the
destination variable can be typed without pressing ALPHA manually.

### Execute_Token handler order

`Execute_Token()` in `calculator_core.c` processes tokens in this fixed order — earlier handlers return before the main switch fires:

1. `TOKEN_ON` — always fires first; saves state, handles power-down
2. `TOKEN_MODE` — always fires second; hides everything, opens MODE screen
3. `MODE_GRAPH_YEQ`
4. `MODE_GRAPH_RANGE`
5. `MODE_GRAPH_ZOOM`
6. `MODE_GRAPH_ZOOM_FACTORS`
7. `MODE_GRAPH_ZBOX`
8. `MODE_GRAPH_TRACE` — exits trace then **falls through** to main switch
9. `MODE_MODE_SCREEN`
10. `MODE_MATH_MENU`
11. `MODE_TEST_MENU`
12. `MODE_MATRIX_MENU`
13. `MODE_MATRIX_EDIT`
14. `MODE_PRGM_MENU` / `MODE_PRGM_EDITOR` / `MODE_PRGM_CTL_MENU` / `MODE_PRGM_IO_MENU` / `MODE_PRGM_EXEC_MENU` / `MODE_PRGM_RUNNING` / `MODE_PRGM_NEW_NAME`
15. `MODE_STAT_MENU` / `MODE_STAT_EDIT` / `MODE_STAT_RESULTS`
16. `MODE_DRAW_MENU`
17. `MODE_VARS_MENU`
18. STO pending check — fires if `sto_pending`, then falls through
18. Main switch (`MODE_NORMAL`)

Navigation helpers (all static in `calculator_core.c`):

- `hide_all_screens()` — hides all overlays and graph canvas; must be called inside `lvgl_lock()`
- `nav_to(target)` — single entry point for all graph screen transitions; acquires lock internally
- `menu_open(token, return_to)` — opens MATH/TEST/MATRIX with correct return mode
- `menu_close(token)` — closes a menu, restores the calling screen, returns the restored mode
- `tab_move(tab, cursor, scroll, count, left, update)` — shared tab-switching logic for MATH and MATRIX

### Modifier key behaviour

- Pressing 2ND or ALPHA a second time cancels the mode (toggle)
- `TOKEN_A_LOCK` (2nd+ALPHA) enters `MODE_ALPHA_LOCK` — stays active until ALPHA is pressed again
- `TOKEN_STO` sets `sto_pending = true`; the next keypress uses the `key.alpha` layer automatically
- Mode state is shown entirely through the block cursor — no status bar:
  - Normal: blinking light-grey block, no inner character
  - 2nd: steady amber block, `^` inside
  - ALPHA / A-LOCK / STO pending: steady green block, `A` inside
- `ui_lbl_modifier` is an invisible LVGL label that holds the current modifier text/colour; `ui_update_status_bar()` mirrors it to `ui_lbl_yeq_modifier` and `ui_lbl_range_modifier` on graph screens

### Cursor implementation

`cursor_update(row_label, char_pos)` in `calculator_core.c`:

- Uses `lv_label_get_letter_pos()` to find the pixel X of the insertion point
- Positions `cursor_box` over that point; sets background colour and `cursor_inner` label based on mode and `sto_pending`
- LVGL timer `cursor_timer_cb` fires every `CURSOR_BLINK_MS` (530 ms)
- Default: overwrite mode (`insert_mode = false`); INS key toggles; insert mode shifts characters right

### Auto-ANS insertion

When the expression buffer is empty and a binary operator is pressed, `expr_prepend_ans_if_empty()` prepends `"ANS"` before the operator. Triggers on: `TOKEN_ADD`, `TOKEN_SUB`, `TOKEN_MULT`, `TOKEN_DIV`, `TOKEN_POWER`, `TOKEN_SQUARE`, `TOKEN_X_INV`.

---

## Math Engine

The calculator uses a three-stage expression evaluator:

```
Expression string → Tokenizer → Shunting-yard → RPN Evaluator → Result
```

### Expression Pipeline Walkthrough

To illustrate the pipeline, consider the evaluation of the expression `"2 + sin(45)"` (in Degrees mode).

#### Stage 1: Tokenizer
The `Tokenize()` function (and the subsequent `ImplicitMulPass()`) converts the raw UTF-8 string into a list of `MathToken_t` structures.

1.  **Raw String**: `"2 + sin(45)"`
2.  **Linear Scan**: The tokenizer identifies numeric literals, named functions, and operator symbols.
3.  **Infix Token List**:
    - `MATH_NUMBER` (value: 2.0)
    - `MATH_OP_ADD`
    - `MATH_FUNC_SIN`
    - `MATH_PAREN_LEFT`
    - `MATH_NUMBER` (value: 45.0)
    - `MATH_PAREN_RIGHT`

*Note: If the expression was `2sin(45)`, the `ImplicitMulPass` would detect the `NUMBER` followed by a `FUNCTION` and insert a `MATH_OP_MUL` token between them.*

#### Stage 2: Shunting-Yard
The `ShuntingYard()` function implements Dijkstra's Shunting-yard algorithm to convert the infix token list into a postfix (Reverse Polish Notation) list. It uses an internal **operator stack** to manage precedence.

| Input Token | Operator Stack | Output (Postfix List) | Rationale |
| :--- | :--- | :--- | :--- |
| `2` | `[]` | `2` | Numbers go straight to output. |
| `+` | `[+]` | `2` | Push `+` to stack. |
| `sin` | `[+, sin]` | `2` | Push function to stack. |
| `(` | `[+, sin, (]` | `2` | Push `(` to stack. |
| `45` | `[+, sin, (]` | `2, 45` | Numbers go straight to output. |
| `)` | `[+]` | `2, 45, sin` | Pop until `(`; then pop `sin` function to output. |
| *end* | `[]` | **`2, 45, sin, +`** | Pop remaining operators to output. |

#### Stage 3: RPN Evaluator
The `EvaluateRPN()` function processes the postfix list using a **value stack**.

1.  **Input**: `2`, `45`, `sin`, `+`
2.  **Process `2`**: Push `2.0` to stack. Stack: `[2.0]`
3.  **Process `45`**: Push `45.0` to stack. Stack: `[2.0, 45.0]`
4.  **Process `sin`**: Pop `45.0`, calculate `sin(45.0)` (≈ 0.7071), push result. Stack: `[2.0, 0.7071]`
5.  **Process `+`**: Pop `0.7071`, pop `2.0`, calculate `2.0 + 0.7071`, push result. Stack: `[2.7071]`
6.  **Final Result**: The single value remaining on the stack is moved into a `CalcResult_t`.

**Result**: `2.7071` (CALC_OK)

### Supported Functions

| Category      | Functions                                                              |
|---------------|------------------------------------------------------------------------|
| Arithmetic    | `+` `-` `*` `/` `^` `x²` `x⁻¹` unary `-`                             |
| Trig          | `sin` `cos` `tan` `sin⁻¹` `cos⁻¹` `tan⁻¹`                            |
| Hyperbolic    | `sinh` `cosh` `tanh` `asinh` `acosh` `atanh`                          |
| Logarithmic   | `ln` `log`                                                             |
| NUM tab       | `round(` `iPart` `fPart` `int(`                                        |
| Other         | `√(` `abs(`                                                            |
| Constants     | `ANS`, `π`, `e`                                                        |
| Variables     | `A`–`Z` (stored via STO→), `X` (graph variable)                       |
| Comparison    | `=` `≠` `>` `≥` `<` `≤` (return 1 or 0)                              |

Auto-ANS: pressing a binary operator with an empty expression prepends `ANS`
automatically, matching TI-81 behaviour.

### Error Handling

| Error               | Display message        |
|---------------------|------------------------|
| Division by zero    | `Division by zero`     |
| Domain error        | `Domain error: <func>` |
| Syntax error        | `Syntax error`         |
| Mismatched parens   | `Syntax error`         |
| Stack overflow      | `Stack overflow`       |

### Adding New Functions

Each new function requires four small additions:

1. `keypad_map.h` — add `TOKEN_XXX` to the `Token_t` enum
2. `keypad_map.c` — map the token to a physical key in `TI81_LookupTable`
3. `calculator_core.c` — add a case in `Execute_Token()` to append the function string to the expression (e.g. `"sqrt("`)
4. `calc_engine.c` — add the function name to `Tokenize()` and its computation to `EvaluateRPN()`

The shunting-yard algorithm never needs to change.

### Public API

```c
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool ans_is_matrix, bool angle_degrees);
CalcResult_t Calc_EvaluateAt(const char *expr, float x_val, float ans, bool angle_degrees);
void         Calc_FormatResult(float value, char *buf, uint8_t buf_len);
```

### Variables

- **ANS** — last result. When the last evaluation produced a matrix, `ans` holds the matrix slot index (3.0f) and `ans_is_matrix = true`; `Tokenize` emits `MATH_MATRIX_VAL` instead of `MATH_NUMBER` for the `ANS` token. This lets `det(ANS)` / `[A]+ANS` chain correctly after matrix arithmetic. `ans_is_matrix` is a static in `calculator_core.c`, passed explicitly — there is no shared global for this state. `Calc_EvaluateAt` (graphing) always passes `false` since Y= equations cannot reference a matrix ANS.
- **x / X** — graph variable; `Calc_EvaluateAt` injects `x_val`; `Calc_Evaluate` uses `calc_variables['X'-'A']`
- **A–Z** — user variables in `calc_variables[26]` in `calc_engine.c`; stored via `STO→`

### Float formatting

`%.6g` is unreliable on newlib-nano. `Calc_FormatResult` uses `%.6f` with manual trailing-zero trimming. Switches to `%.4e` for values ≥1e7 or <1e-4 (non-zero).

### Tokenizer special case

`-` immediately after `^` followed by a digit is folded into a negative number literal to prevent shunting-yard from treating it as binary subtraction. `-3^2` still evaluates as `-(3^2) = -9`.

---

## Graphing

Pressing GRAPH plots all active Y= equations over the current RANGE window.

### State

```c
GraphState_t graph_state;   // defined in calculator_core.c, extern in app_common.h

typedef struct {
    char  equations[GRAPH_NUM_EQ][64];
    bool  enabled[GRAPH_NUM_EQ];          // true if equation is plotted
    float x_min, x_max, y_min, y_max;
    float x_scl, y_scl;
    float x_res;     // render step (1 = every pixel column, integer 1–8)
    bool  active;
    bool  grid_on;   // true when grid dots enabled (MODE row 7)
    /* Parametric mode (MODE row 4 = Param) */
    char  param_x[GRAPH_NUM_PARAM][64];   // X(t) expressions for 3 pairs
    char  param_y[GRAPH_NUM_PARAM][64];   // Y(t) expressions for 3 pairs
    bool  param_enabled[GRAPH_NUM_PARAM]; // true if pair is plotted
    float t_min, t_max, t_step;           // T range (default 0, 2π, π/24)
    bool  param_mode;                     // true when MODE row 4 = Param
} GraphState_t;
```

### LVGL screen objects (all children of `lv_scr_act()`, hidden at startup)

- `ui_graph_yeq_screen` — Y= equation editor
- `ui_graph_range_screen` — RANGE value editor
- `ui_graph_zoom_screen` — ZOOM preset menu
- `graph_screen` (in `graph.c`) — full-height canvas (320×240) + split X=/Y= readout labels

### Free navigation

Any of these keys works from any graph screen:

```
Y=    → MODE_GRAPH_YEQ,   show ui_graph_yeq_screen
RANGE → MODE_GRAPH_RANGE, show ui_graph_range_screen
ZOOM  → MODE_GRAPH_ZOOM,  show ui_graph_zoom_screen
GRAPH → Graph_SetVisible(true), Graph_Render()
TRACE → MODE_GRAPH_TRACE, Graph_DrawTrace() at midpoint
CLEAR → clears active content (eq/field), or exits to calculator if nothing to clear
```

### Y= editor cursor

`yeq_cursor_pos` (uint8_t in `calculator_core.c`) is a **byte offset** (not glyph index) into the selected equation. UTF-8 multi-byte sequences (e.g. √ U+221A) are handled by `utf8_char_size()` / `utf8_byte_to_glyph()`. Reset to end-of-equation when switching rows (UP/DOWN) or opening Y= from the main screen.

### Graph screens

| Key    | Screen / action                                                    |
|--------|--------------------------------------------------------------------|
| Y=     | Equation editor — up to four equations Y1–Y4; toggle with ENTER on = |
| RANGE  | Window settings: Xmin, Xmax, Xscl, Ymin, Ymax, Yscl, Xres        |
| ZOOM   | Preset menu (see below)                                            |
| GRAPH  | Renders all active equations onto the graph canvas                 |
| TRACE  | Moves a crosshair along the curve; LEFT/RIGHT step one pixel       |

### ZOOM presets

| Key | Preset       | Window                          |
|-----|--------------|-------------------------------- |
| 1   | Box          | Rubber-band selection on graph  |
| 2   | Zoom In      | Zoom in by Set Factors amount   |
| 3   | Zoom Out     | Zoom out by Set Factors amount  |
| 4   | Set Factors  | Configure Zoom In/Out scale     |
| 5   | Square       | Corrects pixel aspect ratio     |
| 6   | Standard     | ±10 on both axes                |
| 7   | Trig         | ±2π / ±4                        |
| 8   | Integer      | 1 pixel = 1 unit                |

**Box (ZBox):** a yellow crosshair appears on the graph. Press ENTER to set the
first corner, move with arrow keys, press ENTER again to zoom to the selected
rectangle. CLEAR exits ZBox.

**Set Factors:** opens a sub-screen with two editable fields (XFact/YFact,
default 4.0). UP/DOWN move between fields; number keys and DEL edit the value;
ENTER commits and exits.

### CLEAR behaviour in graph screens

| Screen        | CLEAR with content        | CLEAR with no content   |
|---------------|---------------------------|-------------------------|
| Y= editor     | Clears current equation   | —                       |
| RANGE editor  | Clears current field edit | Exits to calculator     |
| Graph canvas  | —                         | Exits to calculator     |
| TRACE         | (exits trace first, then) | Exits to calculator     |
| ZBox          | —                         | Exits to calculator     |

### RANGE editor

`range_field_selected` (0=Xmin … 6=Xres), `range_field_buf[16]`, `range_field_len`. Fields commit on UP/DOWN/ENTER. CLEAR clears an in-progress edit; if already empty, exits to the calculator. ZOOM from RANGE navigates to the ZOOM menu — does not reset to ZStandard.

### ZOOM FACTORS sub-screen

Two fields: `XFact=` and `YFact=` (defaults 4.0). State: `zoom_x_fact`, `zoom_y_fact`, `zoom_factors_field` (0/1), `zoom_factors_buf[16]`. UP/DOWN move between fields; digit keys and DEL edit the value; ENTER commits and returns to the ZOOM menu.

### Renderer

`Graph_Render(bool angle_degrees)` in `graph.c`:

1. Clears canvas to black
2. Draws grid dots if `graph_state.grid_on` at every (x_scl, y_scl) intersection
3. Draws axes (grey lines at x=0, y=0 if in window)
4. Draws tick marks at x_scl, y_scl intervals
5. Per `x_res` pixel columns: maps to x_math → `Calc_EvaluateAt` → y_px; linearly interpolates between sampled points to fill gaps when `x_res > 1`

### Trace mode

`Graph_DrawTrace()` in `graph.c`: memcpy `graph_buf_clean` → `graph_buf` if `graph_clean_valid` (fast path); otherwise calls `Graph_Render` to populate the cache. Draws a green crosshair ±5 px at the cursor; updates `graph_lbl_x` (X=) and `graph_lbl_y` (Y=). `graph_clean_valid` is invalidated by `Graph_SetVisible(false)`.

### ZBox mode

`Graph_DrawZBox()` in `graph.c`: restores the clean frame, draws a yellow crosshair, and a white rectangle once the first corner is set. ENTER sets the first corner or commits the zoom; ZOOM cancels and stays on the graph canvas; CLEAR exits to the calculator.

---

## Build Configuration — Read Before Building

Several settings are not obvious and some are reset by STM32CubeMX when
regenerating code. Check these every time you regenerate:

### 1. FreeRTOS heap size — `FreeRTOSConfig.h`
CubeMX resets this to 32KB which is insufficient for three tasks.
```c
#define configTOTAL_HEAP_SIZE    ((size_t)(65536))
```

### 2. FreeRTOS stack sizes
CubeMX resets task stack sizes. Required values:

`main.c` (defaultTask — generated, outside USER CODE):
```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 4096 * 2);
```
`App/Src/app_init.c` — `App_RTOS_Init()` (keypad and calc tasks — application code, not reset by CubeMX):
```c
osThreadDef(keypadTask, StartKeypadTask, osPriorityNormal, 0, 1024 * 2);
osThreadDef(calcCore,   StartCalcCoreTask, osPriorityNormal, 0, 1024 * 2);
```

### 3. LVGL OS integration — `App/Display/lv_conf.h`
Must be `LV_OS_NONE`. Setting this to `LV_OS_FREERTOS` causes LVGL to
attempt creating its own internal task which fails silently and breaks
the render pipeline.
```c
#define LV_USE_OS   LV_OS_NONE
```

### 4. BSP pixel format fix — `stm32f429i_discovery_lcd.c`
The BSP hardcodes ARGB8888 in `BSP_LCD_LayerDefaultInit()`. Change it
to RGB565 to match the LTDC and LVGL configuration:
```c
/* In BSP_LCD_LayerDefaultInit(): */
LTDC_PIXEL_FORMAT_RGB565   /* was LTDC_PIXEL_FORMAT_ARGB8888 */
```

### 5. Pixel clock — CubeMX Clock Configuration
Set via PLLSAI. The recommended setting for comfortable rendering:
```
PLLSAIN = 176
PLLSAIR = 4
PLLSAIDivR = 8
→ 5.5 MHz pixel clock
```
Below ~6 MHz produces visible diagonal motion artifacts on mid-range
background colours. Above ~20 MHz exceeds the ILI9341 specification.

### 6. Float printf — `CMakeLists.txt`
`--specs=nano.specs` disables `%f`/`%g`/`%e` in `snprintf` by default.
The linker flags include `-u _printf_float` to re-enable it:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "... --specs=nano.specs -u _printf_float")
```

### 7. App-managed peripherals (no CubeMX configuration needed)

The peripherals below are initialised entirely in App code. Do not add them to the `.ioc` — CubeMX will generate conflicting GPIO init code.

| Concern | Where |
|---|---|
| Keypad A-line GPIO (PE2–PE5, PB3, PB4, PB7) | `Keypad_GPIO_Init()` in `keypad.c` |
| Keypad B-line GPIO (PA5, PC3, PC8, PC11, PD7, PG2, PG3, PG9) | `Keypad_GPIO_Init()` in `keypad.c` |
| ON button EXTI (PE6) | `on_button_init()` in `app_init.c` |
| LTDC framebuffer address (0xD0000000) | `BSP_LCD_LayerDefaultInit()` in `App_DefaultTask_Run()` |
| FreeRTOS heap size, stack overflow check, mutex/semaphore APIs | USER CODE overrides in `FreeRTOSConfig.h` |

---

## Building

**Requirements:**
- STM32CubeMX (to generate vendor library sources)
- arm-none-eabi-gcc
- CMake 3.22+
- OpenOCD for flashing

First-time CubeMX setup (generating vendor sources, FreeRTOS hooks, and App integration steps) is documented in [docs/GETTING_STARTED.md](GETTING_STARTED.md) section 5. After generating, re-apply the manual changes in [Build Configuration](#build-configuration--read-before-building) — CubeMX resets them.

### Step 1 — Build

The project includes a `CMakePresets.json` that configures the ARM toolchain automatically.

**VSCode (recommended):** Use the CMake build button — the stm32-cube-clangd extension handles the toolchain.

**Command line** (ARM toolchain must be on PATH):
```bash
export PATH="$HOME/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

### Step 2 — Flash

```bash
openocd \
  -f /opt/homebrew/Cellar/open-ocd/0.12.0_1/share/openocd/scripts/board/stm32f429disc1.cfg \
  -c "program build/Debug/STM32F429-TI81-Calculator.elf verify reset exit"
```

---

## Memory Layout

```
RAM:     192 KB @ 0x20000000   (~47% used — LVGL heap moved to SDRAM, Session 18)
CCMRAM:   64 KB @ 0x10000000   (~59% used: prgm_store 19 KB + prgm_flash_buf 19 KB + RamFunc code)
FLASH:     2 MB @ 0x08000000   (~38% used)
SDRAM:    64 MB @ 0xD0000000   (external, IS42S16400J; linker SDRAM region defined from 0xD0070800)
```

SDRAM layout:
```
0xD0000000  LCD framebuffer     320×240×2 = 153,600 bytes  (fixed pointer, app_init.c)
0xD0025800  graph_buf           320×240×2 = 153,600 bytes  (fixed pointer, graph.c)
0xD004B000  graph_buf_clean     320×240×2 = 153,600 bytes  (fixed pointer, graph.c — trace cache)
0xD0070800  .sdram section      64 KB — LVGL heap pool (work_mem_int, NOLOAD, linker-placed)
0xD0080800  free SDRAM          ~63.5 MB remaining
```

FLASH sector map (STM32F429ZIT6, 2 MB dual-bank):

| Sector | Address      | Size        | Contents                                   |
|--------|--------------|-------------|--------------------------------------------|
| 0–3    | 0x08000000   | 16 KB each  | Firmware                                   |
| 4      | 0x08010000   | 64 KB       | Firmware                                   |
| 5–9    | 0x08020000   | 128 KB each | Firmware                                   |
| **10** | **0x080C0000** | **128 KB** | **Calculator state (`PersistBlock_t`, 2060 B, v6)** |
| **11** | **0x080E0000** | **128 KB** | **Program storage (37 slots)**             |

> [!CAUTION]
> **FLASH_SECTOR_7 is at 0x08060000 — inside the firmware image for a ~684 KB build.**
> Using the wrong sector number for user data will erase firmware and put the board into
> a HardFault boot loop. Always use sector 10 (0x080C0000) for calculator state and
> sector 11 (0x080E0000) for programs. The authoritative definitions are in
> `App/Inc/persist.h` and `App/Inc/prgm_exec.h`.

---

## Known Limitations

- Background colours with mixed RGB565 bit patterns show a faint diagonal
  motion artifact at certain pixel clock frequencies due to the LTDC RGB
  interface. Fully saturated colours are not affected. This is a characteristic
  of the RGB interface on this display and will not be present on the target
  8080 parallel interface hardware.

- The software rotation in `disp_flush` is pixel-by-pixel and CPU intensive.
  A DMA2D accelerated solution would improve performance on a more demanding UI.

- The math engine uses `float` (32-bit) precision. Results may differ slightly
  from the original TI-81 which used an 8-byte BCD floating-point format.
