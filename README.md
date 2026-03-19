# Neo-81 — bringing new life to the classic. 

![Calculator running on STM32F429I-DISC1](docs/Image_2.jpg)

A TI-81 inspired calculator running on the STM32F429I-DISC1 discovery board.
Built as a learning exercise and prototype for a future custom PCB build.

---

## Hardware

| | |
|---|---|
| MCU | STM32F429ZIT6 — Cortex-M4, 180 MHz |
| Board | STM32F429I-DISC1 |
| Display | 2.4" ILI9341 TFT, 240×320, landscape via LTDC |
| Keypad | TI-81 key matrix, 7 columns × 8 rows |
| SDRAM | IS42S16400J, 8 MB @ 0xD0000000 |

**Software stack:** LVGL v9 · FreeRTOS · GCC ARM · CMake

---

## What works

| Area | Status |
|---|---|
| Arithmetic `+ - × ÷ ^ x² x⁻¹` | ✅ |
| Trig, hyperbolic, log, `√` | ✅ |
| Variables A–Z, ANS, auto-ANS | ✅ |
| TEST operators `= ≠ > ≥ < ≤` | ✅ |
| NUM functions `round iPart fPart int` | ✅ |
| Expression wrap, scrolling history, history recall | ✅ |
| Insert / overwrite mode, UTF-8 cursor | ✅ |
| MODE screen (angle, decimal places, grid) | ✅ |
| MATH menu (MATH / NUM / HYP / PRB tabs) | ✅ |
| Y= editor — up to 4 equations | ✅ |
| Function graphing with axes, grid, tick marks | ✅ |
| RANGE editor (Xmin/Xmax/Yscl/Xres…) | ✅ |
| ZOOM menu — 8 presets, ZBox, Set Factors | ✅ |
| TRACE with X= / Y= readout | ✅ |
| Persistent storage — A–Z, ANS, MODE, graph, RANGE, ZOOM survive power-off | ✅ |
| Stop mode sleep (`2nd+ON`) with full wake restore | ✅ |
| STAT, PRGM, MATRIX | 🚧 Planned |

---

## Build

**Requirements:** STM32CubeMX · arm-none-eabi-gcc · CMake 3.22+ · ST-LINK

```bash
# 1. Open STM32F429-TI81-Calculator.ioc in CubeMX and generate code once
# 2. Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# 3. Flash
st-flash write build/STM32F429-TI81-Calculator.bin 0x08000000
```

---

## Documentation

| Document | Description |
|---|---|
| [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) | Zero-to-build guide — prerequisites, hardware wiring, toolchain setup, flashing. Start here if you are new to the project. |
| [docs/TECHNICAL.md](docs/TECHNICAL.md) | Architecture reference — file structure, build config, memory layout, LVGL threading, calculator engine, graphing system, keypad driver. |
| [docs/UsageDelayedQueue.md](docs/UsageDelayedQueue.md) | Original design plan for persistent FLASH storage — `PersistBlock_t` layout, sector assignment, RAM-function requirements. Feature is fully implemented. |

### Datasheets

| File | Component |
|---|---|
| [TI81Guidebook.pdf](docs/Datasheets/TI81Guidebook.pdf) | TI-81 user manual — calculator behaviour, key layout, PRGM syntax (pages 133–150) |
| [RT9471Charger.pdf](docs/Datasheets/RT9471Charger.pdf) | Richtek RT9471 — LiPo charger with power-path management |
| [W25Q128JVSIQFlashMem.pdf](docs/Datasheets/W25Q128JVSIQFlashMem.pdf) | Winbond W25Q128JV — 16MB SPI NOR flash for firmware XIP + user data |
| [RT4812Boost.pdf](docs/Datasheets/RT4812Boost.pdf) | Richtek RT4812 — 5V boost (DNF Rev1; reserved for Rev2 with RPi Zero 2 W) |
| [RT8059Buck.pdf](docs/Datasheets/RT8059Buck.pdf) | Richtek RT8059 — 3.3V buck reference |
| [TPD4E05U06DQARDiode.pdf](docs/Datasheets/TPD4E05U06DQARDiode.pdf) | TI TPD4E05U06DQAR — USB ESD protection |

---

## Planned hardware

The final target is a custom PCB — STM32H7B0VBT6, 16-bit 8080 parallel display
(eliminates the LTDC pixel-clock artifact), LiPo charging, 3.3V buck. The
software — LVGL, FreeRTOS tasks, math engine, keypad driver — transfers
unchanged. Only the display port layer needs rewriting.
