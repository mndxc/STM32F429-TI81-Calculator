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
- **Repository:** https://github.com/mndxc/STM32F429-TI81-Calculator (private)

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
```
`graph_buf` is a pointer into SDRAM, not a static array:
```c
static uint16_t * const graph_buf = (uint16_t *)0xD0025800;
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
KeypadTask   → scans matrix → posts Token_t to keypadQueueHandle
CalcCoreTask → receives tokens → calls Execute_Token()
DefaultTask  → runs lv_task_handler() in a loop (LVGL tick)
```

### LVGL thread safety
All LVGL calls must be wrapped:
```c
lvgl_lock();
// lv_* calls here
lvgl_unlock();
```
These are defined in `calculator_core.c` using `xLVGL_Mutex`.

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

### Variables supported in expressions
- `ANS` — last result
- `x` or `X` — graph variable (substituted by `Calc_EvaluateAt`)

### Float formatting fix
`%.6g` was unreliable on newlib-nano. Current implementation uses explicit `%.6f`
with manual trailing zero trimming:
```c
void Calc_FormatResult(float value, char *buf, uint8_t buf_len)
{
    if (fabsf(value) >= 1e7f || (fabsf(value) < 1e-4f && value != 0.0f)) {
        snprintf(buf, buf_len, "%.4e", value);
        return;
    }
    float rounded = roundf(value);
    if (fabsf(value - rounded) < 1e-4f &&
        rounded >= -9999999.0f && rounded <= 9999999.0f) {
        snprintf(buf, buf_len, "%d", (int)rounded);
        return;
    }
    snprintf(buf, buf_len, "%.6f", value);
    char *dot = strchr(buf, '.');
    if (dot != NULL) {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') *end-- = '\0';
        if (*end == '.') *end = '\0';
    }
}
```

---

## Input Mode System

`CalcMode_t` in `app_common.h`:
```c
typedef enum {
    MODE_NORMAL,        — standard calculator input
    MODE_2ND,           — 2nd function layer (sticky, resets after one keypress)
    MODE_ALPHA,         — alpha character layer (sticky, resets after one keypress)
    MODE_GRAPH_YEQ,     — Y= equation editor active
    MODE_GRAPH_RANGE    — RANGE field editor active (WIP)
} CalcMode_t;
```

`Execute_Token()` in `calculator_core.c` checks `current_mode` at the top before
the main switch. Each mode intercepts keypresses and routes them appropriately.

---

## Graphing System

### State
```c
typedef struct {
    char    equation[64];   // Y= equation string in terms of x
    float   x_min;          // default -10.0f
    float   x_max;          // default  10.0f
    float   y_min;          // default -10.0f
    float   y_max;          // default  10.0f
    float   x_scl;          // default   1.0f
    float   y_scl;          // default   1.0f
    bool    active;
} GraphState_t;

GraphState_t graph_state;   // defined in calculator_core.c, extern in app_common.h
```

### Graph screens (all children of lv_scr_act())
- `ui_graph_yeq_screen` — Y= equation editor, hidden by default
- `ui_graph_range_screen` — RANGE value editor, hidden by default
- `graph_screen` (in graph.c) — canvas container, hidden by default

### Key flow
```
Y=    → MODE_GRAPH_YEQ, show ui_graph_yeq_screen
RANGE → MODE_GRAPH_RANGE, show ui_graph_range_screen (editing WIP)
ZOOM  → ZStandard reset (±10), fall through to GRAPH
GRAPH → hide other screens, Graph_SetVisible(true), Graph_Render(angle_degrees)
TRACE → TODO
```

### Renderer
`Graph_Render(bool angle_degrees)` in `graph.c`:
1. Clears canvas to black
2. Draws axes (grey lines at x=0, y=0 if in window)
3. Draws tick marks at x_scl, y_scl intervals
4. For each pixel column: maps to x_math, calls `Calc_EvaluateAt`, maps result
   to y_px, connects to previous point with vertical line segment to avoid gaps

### Angle mode and graphing
Graph evaluates using the same angle mode as the calculator.
- In DEG mode, `sin(x)` on ±10 window looks nearly flat (correct behaviour)
- Switch to RAD mode with MODE key to see sine waves properly
- This matches TI-81 behaviour

---

## RANGE Editor (Work In Progress)

State variables added to `calculator_core.c`:
```c
static uint8_t  range_field_selected = 0;   // 0=xmin 1=xmax 2=ymin 3=ymax 4=xscl 5=yscl
static char     range_field_buf[16];
static uint8_t  range_field_len = 0;
static lv_obj_t **range_value_label_ptrs[6];
```

`range_value_label_ptrs` is populated in `ui_init_graph_screens()`:
```c
range_value_label_ptrs[0] = &ui_lbl_range_xmin;
range_value_label_ptrs[1] = &ui_lbl_range_xmax;
range_value_label_ptrs[2] = &ui_lbl_range_ymin;
range_value_label_ptrs[3] = &ui_lbl_range_ymax;
range_value_label_ptrs[4] = &ui_lbl_range_xscl;
range_value_label_ptrs[5] = &ui_lbl_range_yscl;
```

**Next step:** Add `MODE_GRAPH_RANGE` handler to `Execute_Token()`:
- UP/DOWN arrows move `range_field_selected` between 0-5
- Number keys and decimal append to `range_field_buf`
- NEG key handles negative values
- ENTER/DOWN confirms field, parses `range_field_buf` with `strtof`, writes to
  the appropriate `graph_state` field, advances to next field
- GRAPH exits range editing and renders
- RANGE exits range editing without rendering
- Highlight the selected field label differently (e.g. text colour change)

---

## Planned Features (not yet started)

- **TRACE mode** — cursor moves along curve with LEFT/RIGHT, X/Y shown at bottom
- **Additional math functions** — factorial, combinations, permutations, hyperbolic
- **Implicit multiplication** — `2(3+4)` treated as `2*(3+4)`
- **Expression history navigation** — UP/DOWN in calculator mode to recall history
- **Multiple Y= equations** — Y1 through Y4
- **Battery voltage ADC** — for custom PCB (Rev 1 hardware)

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
git add .
git commit -m "description of what changed"
git push
```

Commit when a feature works end to end or before starting a risky change.
Do not commit half-finished work that doesn't build or untested behaviour.

### .gitignore key exclusions
```
build/
LVGL/                                      # duplicate LVGL copy
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
3. **LVGL calls outside mutex** — will cause hard faults or corruption
4. **CCMRAM is nearly full** — do not add more large static buffers there
5. **SDRAM must be initialised before use** — happens in `main.c` before tasks start
6. **White screen after flash** — usually stale binary; power cycle the board
7. **`%.6g` unreliable on ARM newlib-nano** — use `%.6f` with manual trimming
8. **graph.h include guard conflict** — guard is `GRAPH_MODULE_H` not `GRAPH_H`
   (GRAPH_H conflicts with the height constant)
