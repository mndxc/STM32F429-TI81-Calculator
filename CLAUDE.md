# CLAUDE.md — STM32F429 TI-81 Calculator Project Context

This file provides continuity for AI-assisted development sessions. It summarises
all key decisions, gotchas, and work-in-progress state for the project.

---

## Project Overview

A TI-81 calculator recreation running on an STM32F429I-DISC1 discovery board.

- **MCU:** STM32F429ZIT6 (Cortex-M4, 2MB Flash, 192KB RAM, 64KB CCMRAM)
- **Display:** ILI9341 240×320 RGB565 via LTDC, landscape orientation (software rotated)
- **UI:** LVGL v9.x
- **RTOS:** FreeRTOS via CMSIS-OS v1
- **Toolchain:** GCC ARM None EABI 14.3.1, CMake, VSCode + stm32-cube-clangd extension
- **Build system:** CMake with `cmake/gcc-arm-none-eabi.cmake`
- **Repository:** https://github.com/mndxc/STM32F429-TI81-Calculator

---

## Session Stopping Point (2026-03-12)

### What was just completed (uncommitted working-tree changes)

Building on commit `ca98667`, the following are implemented but not yet committed:

1. **Input text wrap** — Long expressions wrap across multiple display rows instead of truncating. `MAX_EXPR_LEN` raised to 96. `expr_chars_per_row` measured at init from JetBrains Mono glyph width. `ui_refresh_display` rewritten to render expression sub-rows and place the cursor on the correct sub-row/column.
2. **MATH HYP items 4–6 renamed** — `sinh^-1(` / `cosh^-1(` / `tanh^-1(` → `asinh(` / `acosh(` / `atanh(` in both display names and insert strings.
3. **√ radical symbol** — `calc_engine.c` tokenizer now matches the UTF-8 sequence `"\xE2\x88\x9A"` (√) instead of `"sqrt"`, so the radical symbol works as a live token.
4. **ZOOM Set Factors sub-screen** — Selecting ZOOM option 4 ("Set Factors") opens `ui_graph_zoom_factors_screen`. Two editable fields: XFact (default 4) and YFact (default 4). UP/DOWN move between fields; number keys and DEL edit; ENTER commits and returns to ZOOM menu. `MODE_GRAPH_ZOOM_FACTORS` added to `CalcMode_t`. Zoom In / Zoom Out now use `zoom_x_fact` / `zoom_y_fact`.
5. **CMakeLists cleanup** — Removed unused BSP font sources (`font20.c`, `font16.c`, `font12.c`, `font8.c`); kept only `font24.c` (required by BSP LCD driver symbol).

### Previously completed (earlier sessions, committed)
- JetBrains Mono font wired into LVGL (`jetbrains_mono_24.c`, `lv_conf.h` updated)
- MODE screen — arrow-key navigation, row highlight, ENTER commits
- MATH menu — four tabs (MATH/NUM/HYP/PRB), scrollable item list, overflow indicators
- ZOOM cursor navigation — UP/DOWN highlight; ENTER selects; scroll indicators for items 7–8
- RANGE Xres= field — seventh field added; combined name=value labels
- Overwrite mode — INS toggles; `expr_insert_char/str` respects mode
- `graph_state.x_res` field added
- Arrow key hold-to-repeat (400ms delay, 80ms rate; arrows only)
- Y= cursor navigation (LEFT/RIGHT within equation, DEL at cursor, UP/DOWN between rows)
- Free graph screen navigation (any graph key works from any graph screen)
- Context-aware CLEAR (wipes eq/field; exits if already empty)
- RANGE ZOOM bug fix (ZOOM from RANGE opens ZOOM menu, not ZStandard)
- UP arrow history recall (UP scrolls back through history; DOWN scrolls forward)
- Removed DEG/RAD corner indicator from main screen

### Known issues
- **Scroll indicator glyphs** — Menus use literal `v`/`^` as overflow indicators because U+2193/U+2191 availability in JetBrains Mono at the current glyph range was not verified. Confirm glyph coverage and switch to ↓/↑ if present.
- **Red flashing LED** — Irregular-period LED present on board. Decide: remove or set to a regular interval (e.g. 1 Hz heartbeat).

### Next session priorities (in order)

**1. TEST menu** — New backlog item; spec TBD.

**2. MATRIX menu** — Deferred; start with menu and 3×3 input UI before wiring math.

**3. Additional math functions** — Factorial, combinations, permutations (wiring for MATH PRB items already in menu).

**4. PRGM** — Program editor and runner.

**5. Battery voltage ADC** — Custom PCB only.

---

## Menu Specs

These specs describe the intended final state of each menu screen.

### General menu rules
- The menu top bar uses the same font as the items below.
- When a menu scrolls, the top tab bar stays fixed and items scroll into the visible window.
- Normal cursor entry applies in menus (INS toggles overwrite/insert; LEFT/RIGHT move cursor).
- Overflow indicators: `v` at bottom means list continues below; `^` at top means list continues above. Use U+2193/U+2191 if the font confirms support.

---

### MODE screen
No title text. Screen filled with option rows; arrow keys navigate.

| Row | Options |
|-----|---------|
| 1 | Normal \| Sci \| Eng |
| 2 | Float \| 0 1 2 3 4 5 6 7 8 9 |
| 3 | Radian \| Degree |
| 4 | Function \| Param |
| 5 | Connected \| Dot |
| 6 | Sequential \| Simul |
| 7 | Grid off \| Grid on |
| 8 | Polar \| Seq |

- LEFT/RIGHT moves selection within a row. UP/DOWN changes active row.
- ENTER commits the highlighted selection; stays in MODE screen.
- State persists in `CalcSettings_t` (only Radian/Degree and basic fields wired so far).

---

### MATH menu
Four tabs. Tab LEFT/RIGHT; item UP/DOWN; ENTER or number key inserts.

**MATH tab:**
```
MATH NUM HYP PRB
1:R>P(
2:P>R(
3:³           (cubed symbol)
4:∛(          (cube root symbol)
5: !
6:°           (degree symbol)
7vr           (v = overflow indicator; list continues)
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
2: nPr        (spaces before and after are inserted into expression)
3: nCr        (spaces before and after are inserted into expression)
```

---

### ZOOM menu
```
ZOOM
1:Box
2:Zoom In
3:Zoom Out
4:Set Factors
5:Square
6:Standard
7vTrig        (v = overflow indicator)
  8:Integer   (visible after scrolling)
```
Navigation: UP/DOWN cursor; ENTER selects. Number keys 1–8 are direct shortcuts.

**Set Factors sub-screen (implemented):**
```
ZOOM FACTORS
XFact=4
YFact=4
```

---

### RANGE menu
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
Cursor edits value directly after the `=` sign. ENTER/UP/DOWN commit and move between fields.

---

### MATRIX menu (deferred)

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
1:[A] 6×6
2:[B] 6×6
3:[C] 6×6
```

---

## Critical Build Settings

### Float printf support
`--specs=nano.specs` disables float in `snprintf` by default. Fixed with:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs -u _printf_float")
```
Without `-u _printf_float`, `%f`, `%g`, and `%e` produce empty strings silently.

### Memory regions
```
RAM:     192 KB @ 0x20000000   (~57% used)
CCMRAM:   64 KB @ 0x10000000   (~98% used — graph_buf was here, now moved)
FLASH:     2 MB @ 0x08000000   (~35% used)
SDRAM:    64 MB @ 0xD0000000   (external, initialised in main.c)
```

### SDRAM layout
```
0xD0000000  LCD framebuffer     320×240×2 = 153,600 bytes
0xD0025800  graph_buf           320×220×2 = 140,800 bytes
0xD0047E00  graph_buf_clean     320×220×2 = 140,800 bytes  (trace cache)
```
`graph_buf` and `graph_buf_clean` are pointers into SDRAM, not static arrays:
```c
static uint16_t * const graph_buf       = (uint16_t *)0xD0025800;
static uint16_t * const graph_buf_clean = (uint16_t *)0xD0047E00;
```

### VSCode extensions
- Use **stm32-cube-clangd** for IntelliSense, NOT Microsoft C/C++ (cpptools)
- Disable cpptools for workspace only: Extensions → C/C++ → gear → Disable (Workspace)

### FreeRTOS stack sizes (do not let CubeMX reset these)
```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 4096 * 2);  // 8192 words
osThreadDef(keypadTask,  StartKeypadTask,  osPriorityNormal, 0, 1024 * 2);  // 2048 words
osThreadDef(calcCore,    StartCalcCoreTask,osPriorityNormal, 0, 1024 * 2);  // 2048 words
#define configTOTAL_HEAP_SIZE ((size_t)65536)
```
CubeMX resets these when regenerating — always check after any `.ioc` changes.

---

## Architecture

### File structure (application code only)
```
Core/Inc/
    app_common.h        — shared types, extern declarations, CalcMode_t enum
    calc_engine.h       — math engine public API
    graph.h             — graphing subsystem public API
Core/Src/
    main.c              — HAL init, SDRAM init, LTDC/LCD setup, task creation
    calculator_core.c   — UI creation, token processing, calculator state
    calc_engine.c       — tokenizer, shunting-yard, RPN evaluator
    graph.c             — graph canvas, renderer, axes, curve plotting
    freertos.c          — FreeRTOS task definitions
Core/Fonts/
    jetbrains_mono_20.c — JetBrains Mono 20px LVGL font
    jetbrains_mono_24.c — JetBrains Mono 24px LVGL font
Drivers/Keypad/
    keypad.c/h          — hardware key matrix scanning
    keypad_map.c/h      — Token_t enum, hardware key → token lookup table
Middlewares/Third_Party/
    lv_conf.h           — LVGL configuration
    lv_port_disp.c/h    — LVGL display driver
    lv_port_indev.c/h   — LVGL input driver
```

### Task architecture
```
KeypadTask   → scans matrix every 20ms → posts Token_t to keypadQueueHandle
CalcCoreTask → blocks on queue → calls Execute_Token()
DefaultTask  → runs lv_task_handler() in a loop (LVGL tick + cursor blink timer)
```

### LVGL thread safety
All LVGL calls must be wrapped:
```c
lvgl_lock();
// lv_* calls here
lvgl_unlock();
```
Defined in `calculator_core.c` using `xLVGL_Mutex`.

**Critical:** Never call `lvgl_lock()` inside `cursor_timer_cb()` — it runs inside
`lv_task_handler()` which DefaultTask already holds the mutex for; a second lock deadlocks.

---

## Input Mode System

`CalcMode_t` in `app_common.h`:
```c
typedef enum {
    MODE_NORMAL,        // standard calculator input
    MODE_2ND,           // 2nd function layer (sticky, resets after one keypress)
    MODE_ALPHA,         // alpha character layer (sticky, resets after one keypress)
    MODE_ALPHA_LOCK,    // alpha locked on (2nd+ALPHA); stays until ALPHA pressed again
    MODE_GRAPH_YEQ,     // Y= equation editor active
    MODE_GRAPH_RANGE,   // RANGE field editor active
    MODE_GRAPH_ZOOM,    // ZOOM preset menu active
    MODE_GRAPH_TRACE,   // trace cursor active on graph
    MODE_GRAPH_ZBOX,    // ZBox rubber-band zoom active
    MODE_MODE_SCREEN,        // MODE settings screen active
    MODE_MATH_MENU,          // MATH/NUM/HYP/PRB menu active
    MODE_GRAPH_ZOOM_FACTORS  // ZOOM FACTORS sub-screen (XFact/YFact editing)
} CalcMode_t;
```

`Execute_Token()` in `calculator_core.c` is structured as early-return mode handlers
at the top, followed by a main `switch(t)` for MODE_NORMAL. Handler order:
1. MODE_GRAPH_YEQ
2. MODE_GRAPH_RANGE
3. MODE_GRAPH_ZOOM
4. MODE_GRAPH_ZOOM_FACTORS
5. MODE_GRAPH_ZBOX
6. MODE_MODE_SCREEN
7. MODE_MATH_MENU
8. MODE_GRAPH_TRACE ← exits trace then **falls through** to main switch
9. STO pending check ← fires if `sto_pending`, then falls through
10. Main switch (MODE_NORMAL)

### Modifier key behaviour
- Pressing 2ND or ALPHA a second time cancels the mode (toggle)
- `TOKEN_A_LOCK` (2nd+ALPHA) enters MODE_ALPHA_LOCK — stays active per keypress
- Pressing ALPHA while in MODE_ALPHA_LOCK exits to previous mode
- `TOKEN_STO` sets `sto_pending = true`; next keypress uses the `key.alpha` layer automatically
- Mode state shown entirely through the block cursor — no status bar:
  - Normal: blinking light-grey block, no inner character
  - 2nd: steady amber block, `^` inside
  - ALPHA / A-LOCK: steady green block, `A` inside
  - STO pending: steady green block, `A` inside
- `ui_lbl_modifier` is an invisible LVGL label on the main screen. It holds the current
  modifier text/color so `ui_update_status_bar()` can mirror it to `ui_lbl_yeq_modifier`
  and `ui_lbl_range_modifier` on graph editing screens.

### Cursor implementation
`cursor_update(row_label, char_pos)` in `calculator_core.c`:
- Uses `lv_label_get_letter_pos()` to find pixel X of the insertion point
- Positions `cursor_box` (sized for JetBrains Mono, `lv_obj`) over that point
- Sets `cursor_box` background color and `cursor_inner` label text based on mode / `sto_pending`
- LVGL timer `cursor_timer_cb` fires every `CURSOR_BLINK_MS` (530 ms)

### Insert / Overwrite mode
- Default: overwrite mode (`insert_mode = false`)
- INS key toggles `insert_mode`
- `expr_insert_char` / `expr_insert_str` replace character at cursor in overwrite mode, shift right in insert mode
- Cursor appearance: block for overwrite, underscore for insert (TBD)

### Auto-ANS insertion
When the expression is empty and a binary operator is pressed, `expr_prepend_ans_if_empty()`
prepends `"ANS"` before the operator. Triggers on: `TOKEN_ADD`, `TOKEN_SUB`, `TOKEN_MULT`,
`TOKEN_DIV`, `TOKEN_POWER`, `TOKEN_SQUARE`, `TOKEN_X_INV`.

---

## Calculator UI

No status bar. The full 320×240 display is a scrolling console.

### Display layout
```
DISP_ROW_COUNT = 8    rows
DISP_ROW_H     = 30   px per row   (8 × 30 = 240px — fills screen exactly)
Font: JetBrains Mono 24 (monospaced)
MAX_EXPR_LEN   = 96   chars (~4 wrapped rows)
```

Each history entry occupies two rows:
- Even row: expression — left-aligned, grey (`0x888888`)
- Odd row: result — right-aligned, white (`0xFFFFFF`)

Current expression being typed: one or more rows (wraps), left-aligned, light grey
(`0xCCCCCC`). `expr_chars_per_row` is measured at init from the JetBrains Mono glyph
width; `ui_refresh_display` slices `expression[]` into `cpr`-char segments and renders
each onto its own display row. The cursor is placed on the sub-row containing `cursor_pos`.

### Key display functions
```c
void Update_Calculator_Display(void);   // called after every keypress that changes expression
static void ui_refresh_display(void);   // raw redraw — must be called under lvgl_lock
static void ui_update_history(void);    // called after ENTER commits a result
```

---

## Graphing System

### State
```c
GraphState_t graph_state;   // defined in calculator_core.c, extern in app_common.h

typedef struct {
    char    equations[GRAPH_NUM_EQ][64];
    float   x_min, x_max, y_min, y_max;
    float   x_scl, y_scl;
    float   x_res;   // render step (1 = every pixel column)
    bool    active;
} GraphState_t;
```

### Graph screens (all children of lv_scr_act(), all hidden at startup)
- `ui_graph_yeq_screen` — Y= equation editor
- `ui_graph_range_screen` — RANGE value editor
- `ui_graph_zoom_screen` — ZOOM preset menu
- `graph_screen` (in graph.c) — canvas + X/Y readout label

### Navigation key flow
Any of these keys works from any graph screen (free navigation):
```
Y=    → MODE_GRAPH_YEQ,   show ui_graph_yeq_screen
RANGE → MODE_GRAPH_RANGE, show ui_graph_range_screen
ZOOM  → MODE_GRAPH_ZOOM,  show ui_graph_zoom_screen
GRAPH → Graph_SetVisible(true), Graph_Render()
TRACE → MODE_GRAPH_TRACE, Graph_DrawTrace() at midpoint
CLEAR → clears active content (eq/field), or exits to calculator if nothing to clear
```

### Y= editor cursor
`yeq_cursor_pos` (uint8_t in calculator_core.c) tracks insertion point within the selected equation.
- LEFT/RIGHT move it; DEL deletes at cursor; characters insert at cursor
- Reset to end-of-equation when switching rows (UP/DOWN) or opening Y= from main screen

### ZOOM menu
Eight options (number keys 1–8 or UP/DOWN + ENTER):
- 1: Box → MODE_GRAPH_ZBOX (rubber-band zoom)
- 2: Zoom In (uses `zoom_x_fact` / `zoom_y_fact`)
- 3: Zoom Out (uses `zoom_x_fact` / `zoom_y_fact`)
- 4: Set Factors → MODE_GRAPH_ZOOM_FACTORS, opens `ui_graph_zoom_factors_screen`
- 5–8: Fixed presets via `apply_zoom_preset()` → renders graph immediately

**ZOOM FACTORS sub-screen** (`ui_graph_zoom_factors_screen`):
- Two rows: `XFact=` and `YFact=` (defaults 4.0)
- State: `zoom_x_fact`, `zoom_y_fact`, `zoom_factors_field` (0/1), `zoom_factors_buf[16]`, `zoom_factors_len`, `zoom_factors_cursor`
- UP/DOWN move between fields; digits and DEL edit value; ENTER commits and exits to ZOOM menu
- Cursor box (`zoom_factors_cursor_box` / `zoom_factors_cursor_inner`) works same as RANGE editor

### RANGE editor
`range_field_selected` (0=Xmin … 6=Xres), `range_field_buf[16]`, `range_field_len`.
Fields commit on UP/DOWN/ENTER. CLEAR clears in-progress edit; if already empty, exits screen.
ZOOM from RANGE navigates to the ZOOM menu (does not reset to ZStandard).

### Renderer
`Graph_Render(bool angle_degrees)` in `graph.c`:
1. Clears canvas to black
2. Draws axes (grey lines at x=0, y=0 if in window)
3. Draws tick marks at x_scl, y_scl intervals
4. Per pixel column: maps to x_math → `Calc_EvaluateAt` → y_px → vertical segment to prev point

### Trace mode
`Graph_DrawTrace()` in `graph.c`:
- memcpy `graph_buf_clean` → `graph_buf` if `graph_clean_valid` (fast path)
- Otherwise calls `Graph_Render` to populate cache
- Draws green crosshair ±5px at cursor; updates X=/Y= readout label
- `graph_clean_valid` invalidated by `Graph_SetVisible(false)`

### ZBox mode
`Graph_DrawZBox()` in `graph.c`:
- Restores clean frame, draws yellow crosshair, white rectangle (once corner1 set)
- ENTER sets first corner or commits zoom; ZOOM cancels and stays on graph; CLEAR exits to calculator

---

## Calculator Engine

### Pipeline
```
string → Tokenize() → infix TokenList_t
       → ShuntingYard() → postfix TokenList_t
       → EvaluateRPN() → CalcResult_t
```

### Public API
```c
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool angle_degrees);
CalcResult_t Calc_EvaluateAt(const char *expr, float x_val, float ans, bool angle_degrees);
void         Calc_FormatResult(float value, char *buf, uint8_t buf_len);
```

### Variables
- `ANS` — last result
- `x` / `X` — graph variable; `Calc_EvaluateAt` passes x_val; `Calc_Evaluate` uses `calc_variables['X'-'A']`
- `A`–`Z` — user variables in `calc_variables[26]` in `calc_engine.c`; stored via STO→

### Float formatting
`%.6g` unreliable on newlib-nano. Current approach: `%.6f` with manual trailing-zero trim.
Switches to `%.4e` for values ≥1e7 or <1e-4 (non-zero).

### Tokenizer special case
`-` immediately after `^` followed by a digit is folded into a negative number literal to prevent
shunting-yard from treating it as binary subtraction. `-3^2` still evaluates as `-(3^2) = -9`.

---

## Keypad Driver

`Drivers/Keypad/keypad.c`:
- 7×8 matrix: A-lines (cols) driven HIGH one at a time, B-lines (rows) read
- Key ID = `(row * 7) + col`; range 1–55; 0xFF = no key
- `StartKeypadTask` scans every 20ms, fires `Process_Hardware_Key()` on new keypress
- Arrow keys auto-repeat: 400ms initial delay, 80ms repeat rate (KEY_REPEAT_DELAY_TICKS=20, KEY_REPEAT_RATE_TICKS=4)
- Only arrow keys repeat; all other keys fire once on initial press

`Process_Hardware_Key()` in `calculator_core.c`:
- Translates raw key ID → Token_t using `TI81_LookupTable`
- Handles 2ND/ALPHA mode layers and STO before queuing the token
- Sticky 2ND/ALPHA: pressing twice cancels; `return_mode` saves the pre-modifier mode to restore after one keypress

---

## Planned Features (backlog)

In priority order:
1. TEST menu (spec TBD)
2. MATRIX menu and 3×3 input UI
3. Additional math functions (factorial, nPr, nCr wiring)
4. PRGM — program editor and runner
5. Battery voltage ADC (custom PCB only)
6. Red flashing LED — decide: remove or regularize at 1 Hz heartbeat

---

## PCB Design (paused, resuming after software)

All ICs verified available on JLCPCB:

| IC | Purpose |
|----|---------|
| STM32H7B0VBT6 | Main MCU LQFP100 |
| RT9526A | LiPo charger |
| MT2492 | 3.3V main buck (R_upper=100K, R_lower=22.1K 1%) |
| RT9078 | 3.3V always-on LDO |
| RT6150 | 5V Pi boost — DNF Rev1 |
| USBLC6-2SC6 | USB ESD protection |

Battery monitoring: voltage divider R1=100K, R2=82K 1% into STM32 ADC.
Max ADC voltage at 4.2V LiPo = 1.89V (safe for 3.3V VDDA reference).

---

## Git Workflow

```bash
git add Core/Src/calculator_core.c Core/Inc/app_common.h Core/Src/graph.c \
        Core/Inc/graph.h Drivers/Keypad/keypad.c CLAUDE.md
git commit -m "description"
git push
```

Commit when a feature works end to end or before starting a risky change.
Do not commit half-finished work that doesn't build.

### .gitignore key exclusions
```
build/
LVGL/
Middlewares/Third_Party/lvgl/tests/
Middlewares/Third_Party/lvgl/docs/
Middlewares/Third_Party/lvgl/examples/
Middlewares/Third_Party/lvgl/demos/
Drivers/STM32F4xx_HAL_Driver/
Drivers/CMSIS/
Middlewares/Third_Party/FreeRTOS/
Middlewares/ST/
```

---

## Common Gotchas

1. **CubeMX resets FreeRTOS stack sizes** — always check after regenerating
2. **nano.specs drops float printf** — always include `-u _printf_float`
3. **LVGL calls outside mutex** — hard faults or display corruption
4. **Never call lvgl_lock() inside cursor_timer_cb** — deadlock (already holds mutex)
5. **CCMRAM is nearly full** — do not add more large static buffers there
6. **SDRAM must be initialised before use** — happens in `main.c` before tasks start
7. **White screen after flash** — usually stale binary; power cycle the board
8. **`%.6g` unreliable on ARM newlib-nano** — use `%.6f` with manual trimming
9. **graph.h include guard is `GRAPH_MODULE_H`** — not `GRAPH_H` (conflicts with the height constant `GRAPH_H`)
10. **`2^-3` tokenizer** — `-` after `^` before digit/dot is a negative literal, not subtraction
11. **strncpy does not null-terminate** — always add `buf[n-1] = '\0'` after strncpy
12. **MODE_GRAPH_TRACE falls through** — after exiting trace mode, execution continues into the main switch to process the triggering key normally. This is intentional.
