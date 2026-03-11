# STM32F429 TI-81 Calculator

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
  Inc/
    main.h
    app_common.h            Shared types, handles and function declarations
    calc_engine.h           Math engine interface

Drivers/
  BSP/
    STM32F429I-Discovery/   ST board support package (LCD, SDRAM)
    Components/             ILI9341 driver and fonts
  Keypad/
    keypad.c                Matrix scan driver and FreeRTOS scan task
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
Scans the 7x8 key matrix every 20ms. On a new keypress, calls
`Process_Hardware_Key()` which translates the hardware ID into a token
and posts it to `keypadQueueHandle`.

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

## Math Engine

The calculator uses a three-stage expression evaluator:

```
Expression string → Tokenizer → Shunting-yard → RPN Evaluator → Result
```

**Tokenizer** — splits the infix string into typed math tokens (numbers,
operators, functions, parentheses). Handles unary negation and the ANS
variable.

**Shunting-yard** — converts infix token stream to postfix (RPN) respecting
operator precedence, associativity, and function calls.

**RPN Evaluator** — evaluates the postfix token stream using a float stack.
Returns a `CalcResult_t` containing either the computed value or a specific
error code and message.

### Supported Functions

| Category      | Functions                                      |
|---------------|------------------------------------------------|
| Arithmetic    | `+` `-` `*` `/` `^` `(-)`                     |
| Trig          | `sin` `cos` `tan` `asin` `acos` `atan`         |
| Logarithmic   | `ln` `log`                                     |
| Other         | `sqrt` `abs`                                   |
| Constants     | `ANS`                                          |

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

## UI Layout

Landscape 320x240 display with a dark theme:

```
┌────────────────────────────────────────┐  y=0
│ DEG          2nd / ALPHA          ANS  │  status bar (20px)
├────────────────────────────────────────┤  y=20
│ 3+sin(45)*2                    24.000  │
│ 12/4                            3.000  │  history (4 lines)
│ ANS+1                           4.000  │
│ ANS*2                           8.000  │
├────────────────────────────────────────┤  y=180
│  3+sin(45)*2^3                         │  expression (30px)
├────────────────────────────────────────┤  y=210
│                                24.000  │  result (30px)
└────────────────────────────────────────┘  y=240
```

| Element         | Font              | Color     |
|-----------------|-------------------|-----------|
| Status bar      | Montserrat 14     | `#888888` |
| 2nd indicator   | Montserrat 14     | `#F5A623` |
| Alpha indicator | Montserrat 14     | `#7ED321` |
| History         | Montserrat 14     | `#888888` |
| Expression      | Montserrat 14     | `#CCCCCC` |
| Result          | Montserrat 28     | `#FFFFFF` |
| Background      | —                 | `#1A1A1A` |
| Status bar bg   | —                 | `#2A2A2A` |

---

## Key Configuration — Read Before Building

Several settings are not obvious and some are reset by STM32CubeMX when
regenerating code. Check these every time you regenerate:

### 1. FreeRTOS heap size — `FreeRTOSConfig.h`
CubeMX resets this to 32KB which is insufficient for three tasks.
```c
#define configTOTAL_HEAP_SIZE    ((size_t)(65536))
```

### 2. LVGL OS integration — `Middlewares/Third_Party/lv_conf.h`
Must be `LV_OS_NONE`. Setting this to `LV_OS_FREERTOS` causes LVGL to
attempt creating its own internal task which fails silently and breaks
the render pipeline.
```c
#define LV_USE_OS   LV_OS_NONE
```

### 3. BSP pixel format fix — `stm32f429i_discovery_lcd.c`
The BSP hardcodes ARGB8888 in `BSP_LCD_LayerDefaultInit()`. Change it
to RGB565 to match the LTDC and LVGL configuration:
```c
/* In BSP_LCD_LayerDefaultInit(): */
LTDC_PIXEL_FORMAT_RGB565   /* was LTDC_PIXEL_FORMAT_ARGB8888 */
```

### 4. Pixel clock — CubeMX Clock Configuration
Set via PLLSAI. The recommended setting for comfortable rendering:
```
PLLSAIN = 100
PLLSAIR = 4
PLLSAIDivR = 2
→ 16.67 MHz pixel clock
```
Below ~6 MHz produces visible diagonal motion artifacts on mid-range
background colours. Above ~20 MHz exceeds the ILI9341 specification.

### 5. DefaultTask stack size
The software rotation flush is pixel-by-pixel and stack intensive.
The DefaultTask stack must be at least 8192 words:
```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 8192);
```

---

## Building

**Requirements:**
- arm-none-eabi-gcc
- CMake 3.22+
- ST-LINK for flashing

**Build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Flash:**
```bash
st-flash write build/BlankDefaultConfigAttempt.bin 0x08000000
```

---

## Status

| Feature                        | Status         |
|--------------------------------|----------------|
| Display initialisation         | ✅ Working     |
| Landscape orientation          | ✅ Working     |
| LVGL rendering                 | ✅ Working     |
| Keypad matrix scan             | ✅ Working     |
| 2nd and Alpha modifier keys    | ✅ Working     |
| Expression building            | ✅ Working     |
| Basic arithmetic               | ✅ Working     |
| Trig functions                 | ✅ Working     |
| Logarithmic functions          | ✅ Working     |
| ANS variable                   | ✅ Working     |
| DEG/RAD mode toggle            | ✅ Working     |
| History display                | ✅ Working     |
| Error messages                 | ✅ Working     |
| Full expression parsing        | ✅ Working     |
| Matrix and list operations     | 🚧 Planned     |
| PRGM / variable storage        | 🚧 Planned     |
| Full TI-81 function parity     | 🚧 Planned     |

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
- Same keypad matrix hardware
- FMC peripheral configured for native 8080 timing

The LVGL architecture, FreeRTOS task structure, math engine, and keypad
driver will transfer unchanged. Only `lv_port_disp.c` requires rewriting
for the new interface.
