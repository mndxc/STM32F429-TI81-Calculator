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
    graph.c                     Graph canvas, axes, tick marks, curve renderer
    graph_ui.c                  Graph screen UI and handlers (extracted module)
    persist.c                   FLASH sector 10 erase/write/load for calculator state
    prgm_exec.c                 Program storage — FLASH sector 11 erase/write/load
    ui_matrix.c                 Matrix cell editor UI (extracted module)
    ui_prgm.c                   Program menu and editor UI (extracted module)
  Inc/
    app_init.h                  App_RTOS_Init() and App_DefaultTask_Run() declarations
    app_common.h                Shared types, handles and function declarations
    calc_engine.h               Math engine interface
    calc_internal.h             Shared internal state for calculator UI modules
    expr_util.h                 Expression buffer utility API
    graph.h                     Graphing subsystem interface
    graph_ui.h                  Graph screen UI interface
    persist.h                   Persistent storage API
    prgm_exec.h                 Program storage and FLASH persistence API
    ui_matrix.h                 Matrix editor UI interface
    ui_prgm.h                   Program menu UI interface
    ui_palette.h                Named colour constants (COLOR_BLACK, COLOR_YELLOW, etc.)
  Fonts/
    JetBrainsMono-Regular.ttf   Source font (Apache 2.0)
    jetbrains_mono_24.c         LVGL bitmap font 24px (generated from TTF)
    jetbrains_mono_20.c         LVGL bitmap font 20px (generated from TTF)
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
    CMakeLists.txt              Host test build (3 executables, 301 tests total)
    test_calc_engine.c          153 tests: tokenizer, shunting-yard, RPN, matrix
    test_expr_util.c            96 tests: UTF-8 cursor, insert/delete, matrix atomicity
    test_persist_roundtrip.c    52 tests: PersistBlock_t checksum and round-trip

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

> [!WARNING]
> **Partially Completed Refactor: PRGM Module**
> The Program (PRGM) logic has been extracted to `ui_prgm.c` and `ui_prgm.h` as an early iteration of this pattern, but it is currently **only partially complete and needs significant work**. While the UI screens (EXEC, EDIT, NEW, CTL, I/O) exist and tab routing functions correctly, the backend execution, true tokenization, memory management, and I/O handling are not yet feature-complete. Future work must bridge the UI module perfectly with the evaluator engine and persistent storage.

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
| `MODE_GRAPH_YEQ`           | Y= equation editor (includes enable/disable toggles)    |
| `MODE_GRAPH_RANGE`         | RANGE field editor                                      |
| `MODE_GRAPH_ZOOM`          | ZOOM preset menu                                        |
| `MODE_GRAPH_TRACE`         | Trace cursor active on graph                            |
| `MODE_GRAPH_ZBOX`          | ZBox rubber-band zoom selection                         |
| `MODE_MODE_SCREEN`         | MODE settings screen                                    |
| `MODE_MATH_MENU`           | MATH/NUM/HYP/PRB function menu                          |
| `MODE_GRAPH_ZOOM_FACTORS`  | ZOOM FACTORS sub-screen (XFact/YFact editing)           |
| `MODE_TEST_MENU`           | TEST comparison-operator menu                           |

Pressing 2nd or ALPHA a second time cancels the modifier (toggle). `STO→` sets
a pending flag and automatically enters ALPHA mode for the next keypress so the
destination variable can be typed without pressing ALPHA manually.

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

---

## Graphing

Pressing GRAPH plots all active Y= equations over the current RANGE window.

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

---

## Building

**Requirements:**
- STM32CubeMX (to generate vendor library sources)
- arm-none-eabi-gcc
- CMake 3.22+
- OpenOCD for flashing

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
[Build Configuration](#build-configuration--read-before-building) as CubeMX
will reset them.

### Step 2 — Build

The project includes a `CMakePresets.json` that configures the ARM toolchain automatically.

**VSCode (recommended):** Use the CMake build button — the stm32-cube-clangd extension handles the toolchain.

**Command line** (ARM toolchain must be on PATH):
```bash
export PATH="$HOME/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

### Step 3 — Flash

```bash
openocd \
  -f /opt/homebrew/Cellar/open-ocd/0.12.0_1/share/openocd/scripts/board/stm32f429disc1.cfg \
  -c "program build/Debug/STM32F429-TI81-Calculator.elf verify reset exit"
```

---

## Memory Layout

```
RAM:     192 KB @ 0x20000000   (~68% used)
CCMRAM:   64 KB @ 0x10000000   (0% used — available for matrix storage or large buffers)
FLASH:     2 MB @ 0x08000000   (~33% used)
SDRAM:    64 MB @ 0xD0000000   (external, IS42S16400J)
```

SDRAM layout:
```
0xD0000000  LCD framebuffer     320×240×2 = 153,600 bytes
0xD0025800  graph_buf           320×240×2 = 153,600 bytes
0xD004B000  graph_buf_clean     320×240×2 = 153,600 bytes  (trace cache)
```

FLASH sector map (STM32F429ZIT6, 2 MB dual-bank):

| Sector | Address      | Size        | Contents                                   |
|--------|--------------|-------------|--------------------------------------------|
| 0–3    | 0x08000000   | 16 KB each  | Firmware                                   |
| 4      | 0x08010000   | 64 KB       | Firmware                                   |
| 5–9    | 0x08020000   | 128 KB each | Firmware                                   |
| **10** | **0x080C0000** | **128 KB** | **Calculator state (`PersistBlock_t`, 864 B)** |
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
