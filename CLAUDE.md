# CLAUDE.md — STM32F429 TI-81 Calculator Project Context

This file provides continuity for AI-assisted development sessions. It summarises
all key decisions, gotchas, and work-in-progress state for the project.

---

## Project Overview

A TI-81 calculator recreation running on an STM32F429I-DISC1 discovery board.

- **MCU:** STM32F429ZIT6 (Cortex-M4, 2MB Flash, 192KB RAM, 64KB CCMRAM)
- **Display:** ILI9341 240x320 RGB565 via LTDC, landscape orientation (software rotated)
- **UI:** LVGL v9.x
- **RTOS:** FreeRTOS via CMSIS-OS v1
- **Toolchain:** GCC ARM None EABI 14.3.1, CMake, VSCode + stm32-cube-clangd extension
- **Build system:** CMake with `cmake/gcc-arm-none-eabi.cmake`
- **Repository:** https://github.com/mndxc/STM32F429-TI81-Calculator

---

## Session Stopping Point (2026-03-12)

### What was just completed
Changes in `calculator_core.c` and `keypad.c` — committed.

**Features implemented and working:**
1. **Arrow key hold-to-repeat** — Hold any arrow key for 400ms then repeats at 80ms. Only arrow keys repeat; all others fire once.
2. **Y= cursor navigation** — LEFT/RIGHT move insertion point within equations. DEL deletes at cursor. Characters insert at cursor. UP/DOWN move between Y= rows and reset cursor to end-of-equation.
3. **Free graph screen navigation** — Any graph navigation key (Y=, RANGE, ZOOM, GRAPH, TRACE) works from any graph screen without needing a specific exit first.
4. **CLEAR behavior revised:**
   - Y= screen: CLEAR wipes the current equation (stays in Y= screen)
   - RANGE screen: CLEAR clears in-progress field edit; if field already empty, exits to calculator
   - TRACE/graph view: CLEAR exits to calculator
   - Main calculator: CLEAR clears expression
5. **RANGE ZOOM bug fixed** — ZOOM from RANGE now opens the ZOOM menu (previously reset to ZStandard)
6. **Code quality cleanup** — Fixed critical null-termination bug in history `strncpy` calls, added explicit null terminators throughout, fixed zoom preset comment, added `(void)argument` casts to FreeRTOS tasks, added default case to `apply_zoom_preset()`.

### Known issues to fix in next session
- **Missing scroll indicator characters** — Menu items use "down arrow" and "up arrow" symbols to indicate that the list continues beyond the visible window (rows 7↓ and 8↑ in MATH menu). The font must include U+2193 (↓) and U+2191 (↑), or the implementation must substitute ASCII approximations (`v` / `^`) until a suitable font is confirmed. Verify the chosen monospaced font covers these glyphs during the font switch task.
- **HYP menu inverse trig display** — `sinh^-1(`, `cosh^-1(`, `tanh^-1(` must be rendered as literal text `sinh^-1(` etc. since there is no dedicated superscript-minus-one character available. See HYP menu spec below.
- **ZOOM cursor navigation** — ZOOM menu must support UP/DOWN arrow keys to move a highlight cursor through options, with ENTER to select. Number key shortcuts (1–8) remain as a secondary method but should not be the only way to select.

### Next session priorities (in order)

**1. Monospaced font** — Switch the main calculator display from Montserrat 24 (proportional) to a fixed-width font so the cursor block aligns correctly with any character. Options:
- `lv_font_unscii_16` — built into LVGL, truly monospaced, but bitmap/blocky
- Roboto Mono or Courier-style — requires LVGL font converter (`lv_font_conv`) to generate a `.c` font file. Recommended size: 18–20px to fit 8 rows in 240px height.
- The cursor block is currently 16×26px, sized for Montserrat 24. It will need resizing to match the new font's cell dimensions.
- All graph screens use Montserrat 24 for content — consider whether to keep those proportional or unify everything.
- consider JetBrains Mono as a preferred option. verify it has characters for various math needs: squared symbol, cubed symbol, power of negative one symbol, triangle pointing right symbol, degrees symbol, pi symbol, theta symbol, uppercase sigma symbol, lowercase sigma symbol, x bar symbol, y bar symbol, 

**2. MODE screen** — TI-81 MODE has no title text. The screen is just filled with options to select. The options are not selected by pressing numbers on the key pad. The selection occurs when the arrow keys move the blinking highlighted option. Moving the highlighted blinking right and left change the selection for that line and using up and down change the active row. There are no dividers between row options, just a single whitespace. Pressing enter key moves the 'activated' option to the cursor location and stays in this screen
- Row 1: Normal | Sci | Eng  (number format)
- Row 2: Float | 0–9  (decimal places — currently hardcoded to 6)
- Row 3: Radian | Degree  (currently just toggled with MODE key — no screen)
- Row 4: Function | Param  (graph type — graph only supports Function currently)
- Row 5: Connected | Dot  (graph draw style)
- Row 6: Sequential | Simul  (equation evaluation order)
- Row 7:Grid off | Grid on
- Row 8: Polar | Seq  
- Mode state persists in a `CalcSettings_t` struct. Only Radian/Degree is currently wired.

**3. UP arrow recalls history** — In MODE_NORMAL with an empty expression, UP should load the previous history entry into the expression buffer (same as ENTRY token / 2nd+ENTER). Subsequent UP presses scroll further back. DOWN scrolls forward. This is a small change to the main TOKEN_UP case.

**4. Overwrite mode by default / INS toggles insert** — Currently always insert mode. Add `insert_mode` bool (default false = overwrite). In overwrite mode, `expr_insert_char/str` replaces the character at cursor_pos instead of shifting right. INS key toggles the flag. Cursor appearance can differ (underscore for overwrite vs. block for insert).

**5. MATH menu** — Key opens a menu screen. TI-81 MATH has four tabs (MATH, NUM, HYP and PRB). Menu screens use highlighted text to display which tab is active. Change the active taby using the left and right keys and select which item of the list is active highlighted by up and down arrow keys. Using enter on the key or pressing the noted number key will insert that function into the previously active entry screen.

- MATH:
- Row 1: MATH NUM HYP PRB (tabs, active one selected by left and right arrows)(MATH highlighted to indicate it is active)
- Row 2: 1:R>P( (selected by pressing number 1 as indicated)
- Row 3: 2:P>R(
- Row 4: 3:3 (cubed symbol)
- Row 5: 4: (3rd root symbol)
- Row 6: 5: !
- Row 7: 6: (degree symbol)
- Row 8: 7↓r  (↓ replaces colon to signal list continues below; use literal "v" if font lacks U+2193)
- Row 9: 8: NDeriv(  (visible after scrolling; the overflowed item at top gains ↑ before its number to signal list continues above; use "^" if font lacks U+2191)

- NUM: 
- Row 1: MATH NUM HYP PRB (NUM highlighted to indicate it is active)
- Row 2: 1:Round(
- Row 3: 2:IPart
- Row 4: 3:FPart
- Row 5: 4:Int

- HYP:
- Row 1: MATH NUM HYP PRB (HYP highlighted to indicate it is active)
- Row 2: 1:sinh
- Row 3: 2:cosh
- Row 4: 3:tanh
- Row 5: 4:sinh^-1(   (rendered as literal text "sinh^-1(" — no special superscript character)
- Row 6: 5:cosh^-1(   (rendered as literal text "cosh^-1(")
- Row 7: 6:tanh^-1(   (rendered as literal text "tanh^-1(")

- PRB:
- Row 1: MATH NUM HYP PRB (PRB highlighted to indicate it is active)
- Row 2: 1:Rand
- Row 3: 2: nPr (note intended white space before and after this item. it is inserted into the active screen with that space)
- Row 4: 3: nCr  (note intended white space before and after this item. it is inserted into the active screen with that space)
- Implement as a MODE_MATH_MENU state with a screen similar to ZOOM.

**6. MATRIX menu** — Deferred. Start with menu and 3×3 input UI before wiring math.
- MATRIX:
- Row 1: MATRIX EDIT (tabs, active one selected by left and right arrows)(MATRIX highlighted to indicate it is active)
- Row 2: 1:RowSwap(
- Row 3: 2:Row+(
- Row 4: 3:*Row(
- Row 5: 4:*Row+(
- Row 6: 5:det
- Row 7: T (superscript T symbol)

- EDIT:
- Row 1: MATRIX EDIT (tabs, active one selected by left and right arrows)(EDIT highlighted to indicate it is active)
- Row 2: 1: [A] 6x6
- Row 3: 2: [B] 6x6
- Row 4: 3: [C] 6x6

**7. ZOOM menu** — Adjust to better mimic original

- Row 1: ZOOM (appears to be an active tab even though there is no other option)
- Row 2: 1:Box
- Row 3: 2:Zoom In
- Row 4: 3:Zoom Out
- Row 5: 4:Set Factors (describes how much zoom in and zoom out are scaled)
- Row 6: 5:Square
- Row 7: 6:Standard
- Row 8: 7↓Trig  (the ↓ replaces the colon to indicate the list continues below; use literal "v" if font lacks U+2193)
- Row 9: 8:Integer  (visible after scrolling down; row 8 header gains ↑ indicator)
- Navigation: UP/DOWN arrows move the highlight cursor through options; ENTER selects. Number keys 1–8 remain as direct shortcuts but cursor-based navigation is the primary method.

**7.1 ZOOM Set Factors sub menu** - To do.
- Row 1: ZOOM FACTORS
- Row 2: XFact= (4 by default)
- Row 3: YFact= (4 by default)

**8. RANGE menu** — Adjust to better mimic original. The cursor edits the value directly after the equal sign
- Row 1: RANGE (appears to be an active tab even though there is no other option)
- Row 2: Xmin=
- Row 3: Xmax=
- Row 4: Xscl=
- Row 5: Ymin=
- Row 6: Ymax=
- Row 7: Yscl=
- Row 8: Xres=


Note regarding menus: 
The menu top bar uses the same font as the items below.
When a menu scrolls up and down the top 'tab bar' stays active and the options listed below it scroll up and down into the visible window.
Allow normal cursor entry in menus. For example allow INS to be used to insert and left right arrows to move cursor for overwrite.
In normal calculator mode cause text input to wrap around to a new line instead of staying on current line and scrolling offscreen.
---

## Critical Build Settings

### Float printf support
`--specs=nano.specs` disables float in `snprintf` by default. Fixed with:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs -u _printf_float")
```
Without `-u _printf_float`, `%f`, `%g`, and `%e` all produce empty strings silently.

### Memory regions
```
RAM:     192 KB @ 0x20000000   (currently ~57% used)
CCMRAM:   64 KB @ 0x10000000   (currently ~98% used — graph_buf was here, now moved)
FLASH:     2 MB @ 0x08000000   (currently ~35% used)
SDRAM:    64 MB @ 0xD0000000   (external, initialised in main.c)
```

### SDRAM layout
```
0xD0000000  LCD framebuffer     320*240*2 = 153,600 bytes
0xD0025800  graph_buf           320*220*2 = 140,800 bytes
0xD0047E00  graph_buf_clean     320*220*2 = 140,800 bytes  (trace cache)
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
CubeMX resets these when regenerating — always check after any .ioc changes.

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
These are defined in `calculator_core.c` using `xLVGL_Mutex`.

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
    MODE_GRAPH_ZBOX     // ZBox rubber-band zoom active
} CalcMode_t;
```

`Execute_Token()` in `calculator_core.c` is structured as a series of early-return mode handlers
at the top, followed by a main `switch(t)` for MODE_NORMAL. Handler order:
1. MODE_GRAPH_YEQ
2. MODE_GRAPH_RANGE
3. MODE_GRAPH_ZOOM
4. MODE_GRAPH_ZBOX
5. MODE_GRAPH_TRACE  ← exits trace then **falls through** to main switch
6. STO pending check ← fires if `sto_pending`, then falls through
7. Main switch (MODE_NORMAL)

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
  and `ui_lbl_range_modifier` on the graph editing screens.

### Cursor implementation
`cursor_update(row_label, char_pos)` in `calculator_core.c`:
- Uses `lv_label_get_letter_pos()` to find pixel X of the insertion point
- Positions `cursor_box` (16×26 px `lv_obj`) over that point
- Sets `cursor_box` background color and `cursor_inner` label text based on mode / `sto_pending`
- LVGL timer `cursor_timer_cb` fires every `CURSOR_BLINK_MS` (530 ms)

### Auto-ANS insertion
When the expression is empty and a binary operator is pressed, `expr_prepend_ans_if_empty()`
prepends `"ANS"` before the operator. Triggers on: `TOKEN_ADD`, `TOKEN_SUB`, `TOKEN_MULT`,
`TOKEN_DIV`, `TOKEN_POWER`, `TOKEN_SQUARE`, `TOKEN_X_INV`.

---

## Calculator UI

No status bar. The full 320×240 display is a scrolling console.

### Display layout
```
DISP_ROW_COUNT = 8   rows
DISP_ROW_H     = 30  px per row   (8 × 30 = 240px — fills screen exactly)
Font: Montserrat 24 (proportional — switching to monospaced is next task)
```

Each history entry occupies two rows:
- Even row: expression — left-aligned, grey (`0x888888`)
- Odd row: result — right-aligned, white (`0xFFFFFF`)

Current expression being typed: last row, left-aligned, light grey (`0xCCCCCC`).
Small `DEG` / `RAD` indicator: top-right corner, Montserrat 14, dim grey (`0x444444`).

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

// graph_state.active is set by Graph_SetVisible() — use it to check if graph is on-screen
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
Six options (number keys 1–6):
- 1: ZBox → MODE_GRAPH_ZBOX (rubber-band zoom)
- 2–6: Fixed presets via `apply_zoom_preset(preset - 1)` → renders graph immediately

### RANGE editor
`range_field_selected` (0=Xmin … 5=Yscl), `range_field_buf[16]`, `range_field_len`.
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

In priority order for next sessions:
1. **Monospaced font** — see "Next session priorities" above
2. **MODE screen** — angle, number format, decimal places, graph type
3. **UP arrow history recall** — MODE_NORMAL + empty expression: UP scrolls back through history
4. **Overwrite/insert cursor mode** — default overwrite, INS key toggles
5. **MATH menu** — MATH and NUM submenus with function insertion
6. **MATRIX menu** — input UI and matrix math (deferred until MATH done)
7. **Additional math functions** — factorial, hyperbolic trig, combinations, permutations
8. **PRGM** — program editor and runner
9. **Battery voltage ADC** — custom PCB only

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
