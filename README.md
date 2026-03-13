# STM32F429 TI-81 Calculator

![Calculator running on STM32F429I-DISC1](docs/FrankenCalc.jpeg)

A TI-81 inspired scientific calculator prototype running on the STM32F429I-DISC1
discovery board, using LVGL for the display interface and FreeRTOS for task
management.

This project is a learning exercise and prototype for a future custom PCB build
using the STM32H7B0VBT6 with a 16-bit 8080 parallel display interface. The
software architecture is intentionally layered so that only the display port
needs to change when moving to new hardware.

---

## Hardware

| Component           | Details                                                   |
|---------------------|-----------------------------------------------------------|
| MCU                 | STM32F429ZIT6 (Cortex-M4, 180 MHz)                       |
| Board               | STM32F429I-DISC1                                          |
| Display             | Onboard 2.4" ILI9341 TFT LCD, 240x320, RGB565            |
| Display interface   | LTDC RGB, framebuffer in external SDRAM                   |
| Display orientation | Landscape 320x240 via software rotation in flush callback |
| Keypad              | TI-81 calculator keypad matrix (7 columns x 8 rows)       |
| SDRAM               | IS42S16400J, 8MB at 0xD0000000                            |

---

## Project Structure

```
Core/
  Src/
    main.c                  Application entry, FreeRTOS task setup, hardware init
    calculator_core.c       Calculator UI, token processing, expression building
    calc_engine.c           Expression parser and evaluator (shunting-yard + RPN)
    graph.c                 Graph canvas, axes, tick marks, curve renderer
  Inc/
    main.h
    app_common.h            Shared types, handles and function declarations
    calc_engine.h           Math engine interface
    graph.h                 Graphing subsystem interface

Drivers/
  BSP/
    STM32F429I-Discovery/   ST board support package (LCD, SDRAM)
    Components/             ILI9341 driver and fonts
  Keypad/
    keypad.c                Matrix scan driver, FreeRTOS scan task, arrow auto-repeat
    keypad.h                Key ID enum and scan function prototype
    keypad_map.c            Hardware key to token lookup table
    keypad_map.h            Token_t enum and lookup table declaration

Middlewares/
  Third_Party/
    lvgl/                   LVGL v9.x source (unmodified)
    lv_conf.h               LVGL configuration
    lv_port_disp.c/h        LVGL display port — LTDC framebuffer flush with rotation
    lv_port_indev.c/h       LVGL input device port — keypad registration
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

---

## Input Modes

`CalcMode_t` tracks the current input state:

| Mode                       | Description                                             |
|----------------------------|---------------------------------------------------------|
| `MODE_NORMAL`              | Standard expression entry                               |
| `MODE_2ND`                 | 2nd function layer — one-shot, reverts after next key   |
| `MODE_ALPHA`               | Alpha character layer — one-shot, reverts after next key|
| `MODE_ALPHA_LOCK`          | Alpha stays active (entered via 2nd+ALPHA)              |
| `MODE_GRAPH_YEQ`           | Y= equation editor                                      |
| `MODE_GRAPH_RANGE`         | RANGE field editor                                      |
| `MODE_GRAPH_ZOOM`          | ZOOM preset menu                                        |
| `MODE_GRAPH_TRACE`         | Trace cursor active on graph                            |
| `MODE_GRAPH_ZBOX`          | ZBox rubber-band zoom selection                         |
| `MODE_MODE_SCREEN`         | MODE settings screen                                    |
| `MODE_MATH_MENU`           | MATH/NUM/HYP/PRB function menu                          |
| `MODE_GRAPH_ZOOM_FACTORS`  | ZOOM FACTORS sub-screen (XFact/YFact editing)           |

Pressing 2nd or ALPHA a second time cancels the modifier (toggle). `STO→` sets
a pending flag and automatically enters ALPHA mode for the next keypress so the
destination variable can be typed without pressing ALPHA manually.

Cursor navigation (LEFT / RIGHT) works in both the main expression and the Y=
equation editor, moving the insertion point one character at a time. Characters
are inserted at the cursor and DEL removes the character to its left.

---

## Math Engine

The calculator uses a three-stage expression evaluator:

```
Expression string → Tokenizer → Shunting-yard → RPN Evaluator → Result
```

**Tokenizer** — splits the infix string into typed math tokens (numbers,
operators, functions, parentheses). Handles unary negation, negative exponents
(`2^-3`), and the ANS variable.

**Shunting-yard** — converts infix token stream to postfix (RPN) respecting
operator precedence, associativity, and function calls.

**RPN Evaluator** — evaluates the postfix token stream using a float stack.
Returns a `CalcResult_t` containing either the computed value or a specific
error code and message.

### Supported Functions

| Category      | Functions                                                              |
|---------------|------------------------------------------------------------------------|
| Arithmetic    | `+` `-` `*` `/` `^` `x²` `x⁻¹` unary `-`                             |
| Trig          | `sin` `cos` `tan` `sin⁻¹` `cos⁻¹` `tan⁻¹`                            |
| Hyperbolic    | `sinh` `cosh` `tanh` `asinh` `acosh` `atanh`                          |
| Logarithmic   | `ln` `log`                                                             |
| Other         | `√(` `abs(`                                                            |
| Constants     | `ANS`, `π`, `e`                                                        |
| Variables     | `A`–`Z` (stored via STO→), `X` (graph variable)                       |

The `√` radical symbol is used as the display token for square root. The
tokenizer recognises the UTF-8 sequence directly so expressions containing `√(`
evaluate correctly.

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
3. `calculator_core.c` — add a case in `Execute_Token()` to append the
   function string to the expression (e.g. `"sqrt("`)
4. `calc_engine.c` — add the function name to `Tokenize()` and its
   computation to `EvaluateRPN()`

The shunting-yard algorithm never needs to change.

---

## Graphing

Pressing GRAPH plots all active Y= equations over the current RANGE window.

### Graph screens

| Key    | Screen / action                                                    |
|--------|--------------------------------------------------------------------|
| Y=     | Equation editor — up to four simultaneous equations Y1–Y4         |
| RANGE  | Window settings: Xmin, Xmax, Xscl, Ymin, Ymax, Yscl, Xres        |
| ZOOM   | Preset menu (see below)                                            |
| GRAPH  | Renders all active equations onto the graph canvas                 |
| TRACE  | Moves a crosshair along the curve; LEFT/RIGHT step one pixel       |

### ZOOM presets

Navigate with UP/DOWN arrows and ENTER, or press the number key shortcut directly.

| Key | Preset       | Window                         |
|-----|--------------|--------------------------------|
| 1   | Box          | Rubber-band selection on graph |
| 2   | Zoom In      | Zoom in by Set Factors amount  |
| 3   | Zoom Out     | Zoom out by Set Factors amount |
| 4   | Set Factors  | Configure Zoom In/Out scale    |
| 5   | Square       | Corrects pixel aspect ratio    |
| 6   | Standard     | ±10 on both axes               |
| 7   | Trig         | ±2π / ±4 (scroll to see)       |
| 8   | Integer      | 1 pixel = 1 unit (scroll to see)|

**Box:** after selecting Box (ZBox), a yellow crosshair appears on the graph.
Press ENTER to set the first corner, move with arrow keys, press ENTER again to
zoom to the selected rectangle. CLEAR exits ZBox and returns to the calculator.

**Set Factors:** opens a sub-screen with two editable fields:
```
ZOOM FACTORS
XFact=4
YFact=4
```
UP/DOWN move between fields; number keys and DEL edit the value; ENTER commits
and exits. Zoom In and Zoom Out then use the stored factors.

### Angle mode

GRAPH respects the current angle mode (DEG / RAD). Toggle with the MODE key.
In DEG mode `sin(x)` on a ±10 window is nearly flat — this is correct TI-81
behaviour. Switch to RAD to see sine waves.

### CLEAR behaviour in graph screens

| Screen        | CLEAR with content        | CLEAR with no content   |
|---------------|---------------------------|-------------------------|
| Y= editor     | Clears current equation   | —                       |
| RANGE editor  | Clears current field edit | Exits to calculator     |
| Graph canvas  | —                         | Exits to calculator     |
| TRACE         | (exits trace first, then) | Exits to calculator     |
| ZBox          | —                         | Exits to calculator     |

---

## Key Configuration — Read Before Building

Several settings are not obvious and some are reset by STM32CubeMX when
regenerating code. Check these every time you regenerate:

### 1. FreeRTOS heap size — `FreeRTOSConfig.h`
CubeMX resets this to 32KB which is insufficient for three tasks.
```c
#define configTOTAL_HEAP_SIZE    ((size_t)(65536))
```

### 2. FreeRTOS stack sizes — `freertos.c`
CubeMX resets task stack sizes. Required values:
```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 4096 * 2);
osThreadDef(keypadTask,  StartKeypadTask,  osPriorityNormal, 0, 1024 * 2);
osThreadDef(calcCore,    StartCalcCoreTask,osPriorityNormal, 0, 1024 * 2);
```

### 3. LVGL OS integration — `Middlewares/Third_Party/lv_conf.h`
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
PLLSAIN = 100
PLLSAIR = 4
PLLSAIDivR = 2
→ 16.67 MHz pixel clock
```
Below ~6 MHz produces visible diagonal motion artifacts on mid-range
background colours. Above ~20 MHz exceeds the ILI9341 specification.

### 6. Float printf — `CMakeLists.txt`
`--specs=nano.specs` disables `%f`/`%g`/`%e` in `snprintf` by default.
The linker flags include `-u _printf_float` to re-enable it:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "... --specs=nano.specs -u _printf_float")
```

---

## Building

**Requirements:**
- STM32CubeMX (to generate vendor library sources)
- arm-none-eabi-gcc
- CMake 3.22+
- ST-LINK for flashing

### Step 1 — Generate vendor sources

The STM32 HAL, CMSIS, and FreeRTOS sources are not included in this repository
(they are large, redistributable by ST, and gitignored). You must generate them
once using STM32CubeMX before building:

1. Open `STM32F429-TI81-Calculator.ioc` in STM32CubeMX
2. Click **Project → Generate Code**
3. This will populate:
   - `Drivers/STM32F4xx_HAL_Driver/`
   - `Drivers/CMSIS/`
   - `Middlewares/Third_Party/FreeRTOS/`

After generating, re-apply the manual changes documented in
[Key Configuration](#key-configuration--read-before-building) as CubeMX
will reset them.

### Step 2 — Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Step 3 — Flash

```bash
st-flash write build/STM32F429-TI81-Calculator.bin 0x08000000
```

---

## Status

| Feature                                    | Status         |
|--------------------------------------------|----------------|
| Display initialisation                     | ✅ Working     |
| Landscape orientation                      | ✅ Working     |
| LVGL rendering                             | ✅ Working     |
| Keypad matrix scan                         | ✅ Working     |
| Arrow key auto-repeat                      | ✅ Working     |
| 2nd and Alpha modifier keys                | ✅ Working     |
| 2nd / Alpha toggle (press twice to cancel) | ✅ Working     |
| Alpha lock (A-LOCK via 2nd+ALPHA)          | ✅ Working     |
| Alpha character input (A–Z)                | ✅ Working     |
| Block cursor with mode indicator           | ✅ Working     |
| Expression building                        | ✅ Working     |
| Cursor navigation within expression        | ✅ Working     |
| Input text wrap (multi-row expression)     | ✅ Working     |
| Basic arithmetic                           | ✅ Working     |
| Trig functions (deg and rad)               | ✅ Working     |
| Hyperbolic functions (sinh/cosh/tanh/…)   | ✅ Working     |
| Logarithmic functions                      | ✅ Working     |
| √ radical symbol for square root           | ✅ Working     |
| ANS variable and auto-ANS                  | ✅ Working     |
| User variables A–Z (STO→)                 | ✅ Working     |
| DEG/RAD mode toggle                        | ✅ Working     |
| History display (scrolling console)        | ✅ Working     |
| History recall with UP/DOWN arrows         | ✅ Working     |
| Overwrite / insert cursor mode (INS)       | ✅ Working     |
| Error messages                             | ✅ Working     |
| JetBrains Mono monospaced font             | ✅ Working     |
| MODE screen (number format, graph type…)   | ✅ Working     |
| MATH menu (MATH/NUM/HYP/PRB tabs)          | ✅ Working     |
| Y= equation editor (up to 4 equations)    | ✅ Working     |
| Cursor navigation within Y= equations     | ✅ Working     |
| Graph rendering                            | ✅ Working     |
| RANGE window editor (7 fields incl. Xres) | ✅ Working     |
| TRACE cursor with X/Y readout             | ✅ Working     |
| ZOOM menu (8 presets, cursor + number-key) | ✅ Working     |
| ZOOM Set Factors sub-screen                | ✅ Working     |
| ZBox rubber-band zoom                      | ✅ Working     |
| Context-aware CLEAR on all screens         | ✅ Working     |
| Free navigation between graph screens      | ✅ Working     |
| MATRIX menu and operations                 | 🚧 Planned     |
| TEST menu                                  | 🚧 Planned     |
| Additional math functions (!, nPr, nCr)    | 🚧 Planned     |
| Persist state across resets                | 🚧 Planned     |
| PRGM editor and runner                     | 🚧 Planned     |

---

## Known Limitations

- Background colours with mixed RGB565 bit patterns show a faint diagonal
  motion artifact at certain pixel clock frequencies due to the LTDC RGB
  interface. Fully saturated colours are not affected. This is a
  characteristic of the RGB interface on this display and will not be
  present on the target 8080 parallel interface hardware.

- The software rotation in `disp_flush` is pixel-by-pixel and CPU
  intensive. A DMA2D accelerated solution would improve performance on
  a more demanding UI.

- The math engine uses `float` (32-bit) precision. Results may differ
  slightly from the original TI-81 which used an 8-byte BCD
  floating-point format.

---

## Planned Hardware

The final target is a custom PCB using the STM32H7B0VBT6 with:
- 16-bit 8080 parallel display interface (eliminates pixel clock artifact)
- Large internal SRAM (no external SDRAM required)
- LiPo charging (RT9526A), 3.3V buck (MT2492), always-on LDO (RT9078)
- Same keypad matrix hardware

The LVGL architecture, FreeRTOS task structure, math engine, and keypad
driver will transfer unchanged. Only `lv_port_disp.c` requires rewriting
for the new interface.
