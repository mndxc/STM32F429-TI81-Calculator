# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

It also provides continuity for AI-assisted development sessions, summarising all key decisions, gotchas, and work-in-progress state.

---

## Feature Completion Status (~50% of original TI-81, as of 2026-03-18)

### Well-implemented (60–100%)

| Area | Est. Done | Notes |
|---|---|---|
| Basic arithmetic | ~95% | +, −, ×, ÷, ^, parentheses, precedence all solid |
| Standard math functions | ~75% | sin/cos/tan, asin/acos/atan, ln, log, √, abs, round, iPart, fPart, int work; factorial, nCr, nPr, cube root, ∛, nDeriv NOT evaluated |
| Variables (A–Z, ANS) | ~90% | STO, ANS, X in graph all work; list variables missing |
| Display / UI / navigation | ~85% | Expression wrap, wrapped history, Fix/Float mode, MATH from Y=, UTF-8 cursor all solid; Sci/Eng notation display not wired |
| Graphing (function mode) | ~75% | 4 equations, axes, grid (toggle from MODE), trace, ZBox, zoom, RANGE, Xres step, interpolated curves; Connected/Dot mode not wired |
| TEST operators | ~100% | Menu (2nd+MATH), UP/DOWN/ENTER/number-key selection, inserts =, ≠, >, ≥, <, ≤; all 6 operators fully evaluated (return 1/0); accessible from Y= editor; and/or/not not present on TI-81 hardware — not planned |

### Entirely missing (0%)

| Area | TI-81 weight | Notes |
|---|---|---|
| STAT | ~15% | 1-Var/2-Var stats, regression, stat plots — nothing implemented |
| PRGM | ~15% | Program editor, runner, control flow, I/O — stub only |
| MATRIX | ~10% | 3 matrices, det, transpose, row ops — stub only |
| DRAW | ~5% | Line, Horizontal, Vertical, DrawF, Shade — stub only |
| Parametric / Polar / Seq graphing | ~5% | Only function mode works |
| VARS menu | ~3% | Window, Zoom, GDB, Picture, Statistics vars — stub only |

The core calculator (arithmetic + standard functions + function graphing + TEST comparisons) covers ~72% of day-to-day TI-81 usage. STAT, PRGM, and MATRIX are entirely absent and together account for roughly 40% of the original hardware's capability.

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

## Build & Flash

### Configure and build (command line)
```bash
cmake -B build/Debug \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
      -G Ninja
cmake --build build/Debug
```
Output: `build/Debug/STM32F429-TI81-Calculator.elf` (and `.map`, `.bin` via objcopy).

Incremental rebuild only:
```bash
cmake --build build/Debug
```

### Flash & debug
Flashing is done through the **VSCode stm32-cube-clangd extension** (ST-Link GDB server). Use `F5` / the Run and Debug panel with the `STM32Cube: STM32 Launch ST-Link GDB Server` configuration defined in `.vscode/launch.json`. There is no standalone CLI flash command configured in this project.

**After flashing:** always do a full USB power cycle (unplug/replug) before testing — the ILI9341 display does not always initialise correctly after a SWD reset alone (white screen symptom).

### No tests
There is no test suite. All validation is done on hardware.

---

## Current Project State (as of 2026-03-18)

All custom application code lives under `App/`. `Core/` contains only CubeMX-generated files. The `main.c` touch points are `#include "app_init.h"` and `App_RTOS_Init()`.

### Completed features (all committed)
- JetBrains Mono font wired into LVGL (`jetbrains_mono_24.c`, `lv_conf.h` updated)
- MODE screen — arrow-key navigation, row highlight, ENTER commits
- MATH menu — four tabs (MATH/NUM/HYP/PRB), scrollable item list, overflow indicators
- ZOOM cursor navigation — UP/DOWN highlight; ENTER selects; scroll indicators for items 7–8
- RANGE Xres= field — seventh field added; combined name=value labels
- Overwrite mode — INS toggles; `expr_insert_char/str` respects mode
- Arrow key hold-to-repeat (400ms delay, 80ms rate; arrows only)
- Y= cursor navigation (LEFT/RIGHT within equation, DEL at cursor, UP/DOWN between rows)
- Free graph screen navigation (any graph key works from any graph screen)
- Context-aware CLEAR (wipes eq/field; exits if already empty)
- RANGE ZOOM bug fix (ZOOM from RANGE opens ZOOM menu, not ZStandard)
- UP arrow history recall (UP scrolls back through history; DOWN scrolls forward)
- Input text wrap; MATH HYP items renamed; √ tokeniser; ZOOM Set Factors sub-screen
- NUM tab functions (round, iPart, fPart, int); Fix decimal mode from MODE; grid toggle from MODE
- MATH menu from Y= editor; UTF-8 aware Y= cursor; wrapped history entries; full-height graph canvas
- Split X=/Y= trace readouts; x_res interpolation; Xres clamped to 1–8
- TEST menu — full UI + all 6 comparison operators evaluated (=, ≠, >, ≥, <, ≤ → 1/0)
- UTF-8 cursor integrity fix — LEFT/RIGHT/DEL/overwrite all handle multi-byte sequences correctly
- Font regeneration — ↑↓ (U+2191/U+2193) and ≠/≥/≤ (U+2260/U+2264/U+2265) added to both font sizes
- Scroll indicator glyphs — ZOOM and MATH menus use ↓/↑ (U+2193/U+2191) amber overlays
- Heartbeat LED fixed to 1 Hz (100 × 5 ms in DefaultTask render loop)
- **Persistent storage** — `App/Inc/persist.h`, `App/Src/persist.c`. Saves A–Z, ANS, MODE, graph equations, RANGE, zoom factors to FLASH sector 10 (0x080C0000). `Calc_BuildPersistBlock` / `Calc_ApplyPersistBlock` in `calculator_core.c`. Load on boot, save on plain ON and 2nd+ON. On boot-load, all screens are synced: Y= labels, MODE highlight, RANGE field labels, and ZOOM FACTORS labels all reflect the restored state.
- **ON button EXTI** — PE6 (`MatrixA0`) reconfigured from push-pull output to EXTI falling-edge with pull-up in `on_button_init()` (called from `App_RTOS_Init`). `EXTI9_5_IRQHandler` and `HAL_GPIO_EXTI_Callback` defined in `app_init.c` — no changes to any CubeMX-generated file.
- **Power management / Stop mode** — `Power_EnterStop()` in `app_init.c`. `2nd+ON` saves state then enters STM32 Stop mode. Wake on ON button press restores PLL, PLLSAI, SDRAM, LTDC, and resumes DefaultTask. `App_SystemClock_Reinit()` wrapper in `main.c` USER CODE BEGIN 4. `g_sleeping` flag guards ISR from posting spurious TOKEN_ON during wake.

### Known issues
- **Display fade on power-off** — After `2nd+ON` the screen slowly fades to white instead of going black cleanly. Root cause: ILI9341 in RGB interface mode has no internal frame buffer; when LTDC pixel clock stops, panel capacitors discharge to white. Current mitigation: framebuffer zeroed to black + 20 ms delay before LTDC disable. `BSP_LCD_DisplayOff()` (SPI DISPOFF command 0x28) does not reliably blank the panel fast enough in RGB mode. Fix still needed: reliable panel blanking before LTDC stops.
- **ZBox arrow key lag** — Screen update rate cannot keep up with held arrow keys during ZBox rubber-band zoom selection. `Graph_DrawZBox()` in `graph.c` redraws from `graph_buf_clean` on every arrow key event; at 80ms repeat rate this may be saturating the LVGL render pipeline. Likely fix: throttle redraws in ZBox mode (skip frames if previous draw not yet flushed), or move crosshair/rectangle rendering to a lightweight overlay rather than full frame restore + redraw each keypress.
- **MODE not accessible from menus/screens** — `TOKEN_MODE` is only handled in `MODE_NORMAL`. It should open the MODE screen from any mode (Y=, RANGE, ZOOM, MATH menu, TEST menu, etc.). This is a general rule: MODE is a top-level key like the graph navigation keys and must always work. Fix: add `TOKEN_MODE` handling as an early-exit check at the very top of `Execute_Token()` in `calculator_core.c`, before any mode-specific handler, similar to how graph nav keys could be made universal.

### Next session priorities (in order)

**1. Display off on power-down** — ILI9341 in RGB mode fades to white when LTDC pixel clock stops. Current mitigation (black framebuffer + 20 ms) reduces but doesn't eliminate the fade. Fix approaches: (a) set LTDC layer alpha to 0 before disabling so the background colour (black) is output for one frame, then disable; (b) investigate whether `BSP_LCD_DisplayOff()` reliably triggers the ILI9341 DISPOFF in the BSP's SPI transaction sequence; (c) toggle the display enable GPIO if one is wired on the DISC1.
- Files: `App/Src/app_init.c` (`Power_EnterStop()`)

**2. MATH PRB menu completion** — PRB tab items (rand, nPr, nCr) are displayed but evaluation is not yet confirmed working end-to-end. Verify on hardware.
- Files: `App/Src/calc_engine.c`, `App/Src/calculator_core.c`, `App/Drivers/Keypad/keypad_map.h`
- Gotchas: `srand(HAL_GetTick())` already added in `app_init.c` after `xSemaphoreGive(xLVGL_Ready)`; nPr/nCr insert as ` nPr ` / ` nCr ` with surrounding spaces per MATH menu spec

**3. MATRIX menu** — Start with menu and 6×6 input UI before wiring math.
- Files: `App/Src/calculator_core.c` (new `MODE_MATRIX_MENU` handler, new screen objects), `App/Inc/app_common.h` (add `MODE_MATRIX_MENU` to `CalcMode_t`), `App/Src/calc_engine.c` (det, transpose later)
- Gotchas: CCMRAM is 0% used — matrix storage can go there (64 KB free) or in RAM (~61 KB free); follow same lvgl_lock/unlock pattern as other menus

**4. PRGM** — Program editor and runner (persistent storage prerequisite now complete).
- Files: `App/Src/calculator_core.c` (new `MODE_PRGM_*` handlers), `App/Inc/app_common.h` (new CalcMode_t values), `App/Src/prgm.c` (new), `App/Inc/prgm.h` (new)
- Gotchas: program text storage goes in FLASH via the persistent storage layer; working buffer during editing can live in CCMRAM (64 KB free); control flow tokens (If/Then/Goto/Lbl) will need new TOKEN_* entries in `App/Drivers/Keypad/keypad_map.h`
- PDF pages 133–150 of `docs/TI81Guidebook.pdf` contain original TI-81 programming instructions

**5. Y= equation enable/disable toggle** — On the original TI-81, pressing LEFT from a Y= equation moves the cursor to the `=` sign; pressing ENTER there toggles whether that equation is plotted. The `=` should show its state visually — TI-81 used inverted colors; this project could use a distinct highlight color (e.g. amber) to make the active/inactive state obvious.
- Files: `App/Src/calculator_core.c` (Y= cursor handling for `=` column), `App/Src/graph.c` (skip disabled equations in renderer)

**6. ENTER on blank line recalculates previous entry** — Pressing ENTER on an empty input line should re-evaluate the last expression (same result as recalling it and pressing ENTER again). Matches common calculator behaviour.
- Files: `App/Src/calculator_core.c` (`TOKEN_ENTER` handler in `MODE_NORMAL`)

**7. 2nd+ENTRY recall conflict** — The UP arrow history recall feature inadvertently displaced the original TI-81 behaviour where `2nd+ENTRY` recalled the last entry. Both behaviours should coexist: UP/DOWN scrolls history as now; `2nd+ENTRY` still recalls the last entry directly.
- Files: `App/Src/calculator_core.c` (`TOKEN_ENTRY` handler under `MODE_2ND`)

**8. Startup splash image** — Display a bitmap or splash screen on boot before the calculator UI initialises. LVGL supports image objects natively; asset format is RGB565 array in FLASH.

**9. ZBox render speed** — See Known Issues entry "ZBox arrow key lag" for root cause and suggested fix (throttle redraws / lightweight overlay).

---

## Menu Specs

These specs describe the intended final state of each menu screen.

### General menu rules
- The menu top bar uses the same font as the items below.
- When a menu scrolls, the top tab bar stays fixed and items scroll into the visible window.
- Normal cursor entry applies in menus (INS toggles overwrite/insert; LEFT/RIGHT move cursor).
- Overflow indicators: ↓ (U+2193) at bottom means list continues below; ↑ (U+2191) at top means list continues above. Both glyphs are in the font and implemented in code (`\xE2\x86\x93` / `\xE2\x86\x91`).

---

### MODE screen
No title text. Screen filled with option rows; arrow keys navigate.

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

- LEFT/RIGHT moves selection within a row. UP/DOWN changes active row.
- ENTER commits the highlighted selection; stays in MODE screen.
- Active selections stored in `mode_committed[8]`; wired rows take effect immediately on ENTER.
- 2ND key should open MODE from any screen (currently only works from `MODE_NORMAL` — see Known Issues).

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
7↓Trig        (↓ = overflow indicator)
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

### TEST menu

No tabs. "TEST" title at top row (yellow), 6 items below.

```
TEST
1:=
2:≠
3:>
4:≥
5:<
6:≤
```

Navigation: UP/DOWN cursor; ENTER selects. Number keys 1–6 are direct shortcuts.
CLEAR or 2nd+MATH exits. Accessible from normal mode (2nd+MATH) and from the Y= editor.
Selected operator is inserted at the cursor position as a UTF-8 string.
All 6 operators are fully evaluated by `calc_engine.c` — return 1 (true) or 0 (false).
and/or/not are not present on the TI-81 and are not planned.


### STAT menu

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

**DRAW tab:**
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

## Critical Build Settings

### Float printf support
`--specs=nano.specs` disables float in `snprintf` by default. Fixed with:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs -u _printf_float")
```
Without `-u _printf_float`, `%f`, `%g`, and `%e` produce empty strings silently.

### Memory regions
```
RAM:     192 KB @ 0x20000000   (~68% used)
CCMRAM:   64 KB @ 0x10000000   (0% used — graph_buf moved to SDRAM)
FLASH:     2 MB @ 0x08000000   (~33% used)
SDRAM:    64 MB @ 0xD0000000   (external, initialised in main.c)
```

### SDRAM layout
```
0xD0000000  LCD framebuffer     320×240×2 = 153,600 bytes
0xD0025800  graph_buf           320×240×2 = 153,600 bytes
0xD004B000  graph_buf_clean     320×240×2 = 153,600 bytes  (trace cache)
```
`graph_buf` and `graph_buf_clean` are pointers into SDRAM, not static arrays:
```c
static uint16_t * const graph_buf       = (uint16_t *)0xD0025800;
static uint16_t * const graph_buf_clean = (uint16_t *)(0xD0025800 + GRAPH_W * GRAPH_H * 2);  /* = 0xD004B000 */
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

### File structure
```
App/Inc/                        ← application headers (custom, not CubeMX)
    app_init.h          — App_RTOS_Init() and App_DefaultTask_Run() declarations
    app_common.h        — shared types, extern declarations, CalcMode_t enum
    calc_engine.h       — math engine public API
    graph.h             — graphing subsystem public API
App/Src/                        ← application sources (custom, not CubeMX)
    app_init.c          — RTOS objects, hardware bring-up, LVGL init, render loop, Power_EnterStop
    calculator_core.c   — UI creation, token processing, calculator state
    calc_engine.c       — tokenizer, shunting-yard, RPN evaluator
    graph.c             — graph canvas, renderer, axes, curve plotting
    persist.c           — FLASH erase/write/load for persistent state (.RamFunc routines)
App/Fonts/
    JetBrainsMono-Regular.ttf — source font (Apache 2.0; committed so regeneration is always possible)
    jetbrains_mono_20.c — JetBrains Mono 20px LVGL font (generated)
    jetbrains_mono_24.c — JetBrains Mono 24px LVGL font (generated)
App/Drivers/Keypad/
    keypad.c/h          — hardware key matrix scanning
    keypad_map.c/h      — Token_t enum, hardware key → token lookup table
App/LVGL/
    lv_conf.h           — LVGL configuration
    lv_port_disp.c/h    — LVGL display driver
    lv_port_indev.c/h   — LVGL input driver
Core/Inc/                       ← CubeMX generated (regenerated from .ioc)
    main.h, stm32f4xx_hal_conf.h, stm32f4xx_it.h, FreeRTOSConfig.h
Core/Src/                       ← CubeMX generated (regenerated from .ioc)
    main.c              — HAL init, SDRAM init, LTDC/LCD setup, task creation
    freertos.c          — FreeRTOS task definitions
    stm32f4xx_it.c, stm32f4xx_hal_msp.c, system_stm32f4xx.c, sysmem.c, syscalls.c
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
    MODE_GRAPH_ZOOM_FACTORS, // ZOOM FACTORS sub-screen (XFact/YFact editing)
    MODE_TEST_MENU           // TEST comparison-operator menu active
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
8. MODE_TEST_MENU
9. MODE_GRAPH_TRACE ← exits trace then **falls through** to main switch
10. STO pending check ← fires if `sto_pending`, then falls through
11. Main switch (MODE_NORMAL)

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
    float   x_res;   // render step (1 = every pixel column, integer 1–8)
    bool    active;
    bool    grid_on; // true when grid dots enabled (MODE row 7)
} GraphState_t;
```

### Graph screens (all children of lv_scr_act(), all hidden at startup)
- `ui_graph_yeq_screen` — Y= equation editor
- `ui_graph_range_screen` — RANGE value editor
- `ui_graph_zoom_screen` — ZOOM preset menu
- `graph_screen` (in graph.c) — full-height canvas (320×240) + split X=/Y= readout labels

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
`yeq_cursor_pos` (uint8_t in calculator_core.c) tracks insertion point within the selected equation as a **byte offset** (not glyph index). UTF-8 multi-byte sequences (e.g. √ U+221A) are handled by `utf8_char_size()` / `utf8_byte_to_glyph()`.
- LEFT/RIGHT move it; DEL deletes at cursor; characters insert at cursor
- Overwrite mode handles multi-byte chars: replaces all bytes of the current char with the incoming ASCII byte
- Reset to end-of-equation when switching rows (UP/DOWN) or opening Y= from main screen
- MATH key opens the MATH menu; on selection, inserts at `yeq_cursor_pos` and restores Y= screen

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
2. Draws grid dots if `graph_state.grid_on` (`draw_grid()`) at every (x_scl, y_scl) intersection
3. Draws axes (grey lines at x=0, y=0 if in window)
4. Draws tick marks at x_scl, y_scl intervals
5. Per `x_res` pixel columns: maps to x_math → `Calc_EvaluateAt` → y_px; linearly interpolates between sampled points to fill gaps when `x_res > 1`

### Trace mode
`Graph_DrawTrace()` in `graph.c`:
- memcpy `graph_buf_clean` → `graph_buf` if `graph_clean_valid` (fast path)
- Otherwise calls `Graph_Render` to populate cache
- Draws green crosshair ±5px at cursor; updates `graph_lbl_x` (X=) and `graph_lbl_y` (Y=)
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

`App/Drivers/Keypad/keypad.c`:
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
1. MATH PRB completion — factorial, nPr, nCr evaluation
2. MATRIX menu and 6×6 input UI
3. PRGM — program editor and runner
4. Battery voltage ADC (custom PCB only)
5. Red flashing LED — decide: remove or regularize at 1 Hz heartbeat

---

## PCB Design (paused, resuming after software)

All ICs verified available on JLCPCB:

| IC | Purpose |
|----|---------|
| STM32H7B0VBT6 | Main MCU LQFP100 |
| W25Q128JV | 16MB OctoSPI NOR flash on OCTOSPI1 — firmware XIP + user data (variables, programs, settings); partitioned by sector |
| RT9471 | LiPo charger with power path management — I²C programmable, 3A, USB OTG boost, WQFN-24L 4×4 |
| MT2492 | 3.3V main buck (R_upper=100K, R_lower=22.1K 1%) |
| RT4812 | 5V boost — **DNF Rev1; reserved for Rev2 when Raspberry Pi Zero 2 W is integrated** |
| TPD4E05U06DQAR | USB ESD protection (TI, 4-channel, SOT-563) |

**Power architecture notes:**

Power flows as follows:
```
USB ──► RT9471 (SYS rail) ──► MT2492 (buck) ──► 3.3V system rail
         ▲        │
        BAT ──────┘  (RT9471 power path selects best source automatically)
```

- **RT9471 SYS rail** is a managed output — not a raw battery passthrough. It has a guaranteed minimum of 3.5V (VSYS_MIN) at all times while any power source is available. When USB is connected, SYS is regulated from VBUS and the system runs normally even with a fully depleted battery.
- **MT2492** bucks the SYS rail down to 3.3V. Minimum headroom at worst case (SYS = 3.5V, VOUT = 3.3V) is 200mV — tight but workable. At 300mA load the buck dropout is ~100mV, well within margin.
- **Low-battery threshold:** set the ADC monitor to flag low battery at ~3.6V (battery terminal, not SYS) to ensure graceful shutdown before SYS hits its 3.5V floor. At 3.6V battery the system still has ~200mV headroom on the MT2492.
- **MT2492 feedback resistors:** R_upper = 100kΩ, R_lower = 22.1kΩ 1% → 3.3V output. Optionally bias to 3.28V to gain a few mV of extra dropout margin.
- **RT9471 I²C:** SCL/SDA to STM32 with 10kΩ pull-ups to 3.3V. Default power-on settings (2A charge current, 4.2V charge voltage, 0.5A input current limit) are safe without firmware initialisation, but the STM32 should configure AICR to 1.5A on boot to allow faster charging when a capable adapter is present. CE pin pulled low to enable charging; INT pin to STM32 GPIO for fault notification.
- **RT9471 package:** WQFN-24L 4×4 with exposed thermal pad — thermal pad must be soldered to a solid GND copper pour for heat dissipation. Requires reflow; not hand-solderable.
- **TPD4E05U06DQAR:** placed on USB D+/D− lines as close to the connector as possible, before the RT9471 D+/D− pins. SOT-563 package.
- **RT4812 (DNF Rev1):** footprint placed on board but unpopulated. Reserved for Rev2 when a Raspberry Pi Zero 2 W is added to the design — the Pi requires a 5V supply rail that the RT4812 will provide by boosting from the LiPo.

**External flash notes:**
- Single W25Q128JV (16MB) on OCTOSPI1 serves both firmware XIP and user data storage. 16MB is far more than enough — current firmware is ~684KB; even with generous growth headroom, the top 1–2MB is more than sufficient for user data.
- **Proposed partition layout:**
  - `0x000000 – 0x0FFFFF` (first 1MB): firmware image (XIP, never written at runtime)
  - `0x100000 – 0xFFFFFF` (remaining 15MB): user data — variables, programs, settings
- **Write-freeze gotcha (critical):** W25Q128JV is single-bank NOR. Erasing or writing any sector while OCTOSPI1 is in memory-mapped (XIP) mode stalls the AHB bus and freezes execution. Any erase/write routine that touches the user data partition **must** run from RAM:
  - Declare with `__attribute__((section(".RamFunc")))` or equivalent linker section
  - Switch OCTOSPI1 to indirect mode before erase/write; switch back to memory-mapped mode before returning
  - Keep the routine short — execution is stalled during the switch and any cache misses will fault
- **Alternative approach (simpler):** only write flash on `2nd+ON` power-off gesture (same plan as STM32F429 persistent storage). This limits erase/write operations to once per session and makes the RAM-execution window very short and predictable.
- Flash wear negligible at this scale — effectively unlimited for calculator use
- **VBAT supply:** no coin cell. Route LiPo BAT pin → BAT54 Schottky diode (SOD-323) → STM32 VBAT. The diode prevents back-feed into the RT9471 power path. VBAT draws ~1–5μA; a 1000mAh LiPo powering only VBAT would last decades. RTC survives soft power-off as long as the LiPo is installed. If LiPo is removed the RTC resets — acceptable since no timekeeping feature is planned.
- Battery monitoring: voltage divider R1=100K / R2=82K 1% into STM32 ADC; max ADC voltage at 4.2V LiPo = 1.89V (safe for 3.3V VDDA reference)

---

## Git Workflow

```bash
git add App/Src/calculator_core.c App/Src/app_init.c App/Inc/app_common.h \
        App/Src/graph.c App/Inc/graph.h App/Drivers/Keypad/keypad.c CLAUDE.md
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
5. **CCMRAM is 0% used (64 KB free)** — `graph_buf` was moved to SDRAM; CCMRAM is available for matrix storage or other large working buffers
6. **SDRAM must be initialised before use** — happens in `main.c` before tasks start
7. **White screen after flash** — usually stale binary; power cycle the board
8. **`%.6g` unreliable on ARM newlib-nano** — use `%.6f` with manual trimming
9. **graph.h include guard is `GRAPH_MODULE_H`** — not `GRAPH_H` (conflicts with the height constant `GRAPH_H`)
10. **`2^-3` tokenizer** — `-` after `^` before digit/dot is a negative literal, not subtraction
11. **strncpy does not null-terminate** — always add `buf[n-1] = '\0'` after strncpy
12. **MODE_GRAPH_TRACE falls through** — after exiting trace mode, execution continues into the main switch to process the triggering key normally. This is intentional.
13. **UTF-8 cursor integrity** — `cursor_pos` in the main expression is a byte offset. Any code that moves or edits at `cursor_pos` must account for multi-byte characters (π=2B, √/≠/≥/≤=3B). Stepping by 1 byte can land inside a sequence; LVGL silently skips invalid UTF-8 so the display looks fine but `Tokenize()` returns `CALC_ERR_SYNTAX`. Rules: LEFT steps back past all `10xxxxxx` continuation bytes; RIGHT steps forward past the full sequence; DEL walks back to the start byte and removes all N bytes; overwrite uses `utf8_char_size()` to remove the full char before writing the replacement. The Y= cursor (`yeq_cursor_pos`) was correct already — use it as the reference implementation.
14. **Font regeneration** — the LVGL `.c` font files are generated from `App/Fonts/JetBrainsMono-Regular.ttf` using `lv_font_conv` (install: `npm i -g lv_font_conv`). Regenerate both sizes with:
    ```bash
    lv_font_conv --font App/Fonts/JetBrainsMono-Regular.ttf \
      -r 0x20-0x7E -r 0x00B0,0x00B2,0x00B3,0x00B9 \
      -r 0x03A3,0x03B8,0x03C0,0x03C3 \
      -r 0x221A -r 0x25B6 -r 0x2191,0x2193 -r 0x2260,0x2264,0x2265 \
      --size 24 --format lvgl --bpp 4 -o App/Fonts/jetbrains_mono_24.c --no-compress

    lv_font_conv --font App/Fonts/JetBrainsMono-Regular.ttf \
      -r 0x20-0x7E -r 0x00B0,0x00B2,0x00B3,0x00B9 \
      -r 0x03A3,0x03B8,0x03C0,0x03C3 \
      -r 0x221A -r 0x25B6 -r 0x2191,0x2193 -r 0x2260,0x2264,0x2265 \
      --size 20 --format lvgl --bpp 4 -o App/Fonts/jetbrains_mono_20.c --no-compress
    ```
    Current Unicode ranges included: ASCII (0x20–0x7E), °²³¹ (superscripts/degree), Σθπσ (Greek), √ ▶ ↑↓ (math/UI), ≠≤≥ (TEST operators).
15. **FLASH sector map — FLASH_SECTOR_7 is NOT 0x080C0000** — On STM32F429ZIT6 (2MB, 12 sectors per bank), the sector layout is: sectors 0–3 = 16 KB, sector 4 = 64 KB, sectors 5–11 = 128 KB. `FLASH_SECTOR_7` is at **0x08060000** (inside the firmware for a ~684 KB image). The persist sector is `FLASH_SECTOR_10` at **0x080C0000**. Never use `FLASH_SECTOR_7` for user data — it will erase firmware code, causing a HardFault loop and a board that fails to boot until reflashed.
16. **Never call lv_timer_handler() from CalcCoreTask while holding xLVGL_Mutex** — `xLVGL_Mutex` is a standard (non-recursive) FreeRTOS mutex. Calling `lv_timer_handler()` inside `lvgl_lock()` from CalcCoreTask will deadlock: LVGL's internal flush handshake waits for `lv_disp_flush_ready()` which only fires when DefaultTask runs — but DefaultTask is blocked on the same mutex. Pattern to show UI feedback before a long operation: `lvgl_lock(); /* create label */; lvgl_unlock(); osDelay(20); /* DefaultTask renders */; /* long operation */`.
17. **ON button EXTI is on EXTI9_5_IRQn** — `EXTI9_5_IRQHandler` is defined in `app_init.c`, not in the CubeMX-generated `stm32f4xx_it.c`. If CubeMX ever regenerates `stm32f4xx_it.c` and adds a duplicate `EXTI9_5_IRQHandler`, there will be a linker error. Keep the handler in `app_init.c` and ensure `stm32f4xx_it.c` does not define it.
18. **Power_EnterStop LTDC/SDRAM order** — LTDC must be disabled BEFORE SDRAM enters self-refresh. In RGB interface mode LTDC continuously reads from the SDRAM framebuffer; if SDRAM enters self-refresh while LTDC is still active, LTDC receives bus errors and drives random pixels to the display. Correct order: zero framebuffer → delay 20 ms → disable LTDC → BSP_LCD_DisplayOff → SDRAM self-refresh → HAL_SuspendTick → WFI.
