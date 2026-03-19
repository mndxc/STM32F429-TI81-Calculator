# Neo-81: bringing new life to the classic. 

![Calculator running on STM32F429I-DISC1](docs/Image_2.jpg)

A TI-81 inspired calculator running on the STM32F429I-DISC1 discovery board.
Built as a learning exercise and prototype for a future custom PCB build.

---

## Hardware

STM32F429I-DISC1 (Cortex-M4, 180 MHz, 2.4" ILI9341 display, 8 MB SDRAM) with a salvaged TI-81 key matrix wired to the GPIO header.

**Software:** LVGL v9 · FreeRTOS · GCC ARM · CMake

---

## Status

Core arithmetic, standard math functions (trig, hyperbolic, log, √), variables A–Z / ANS, TEST operators, and the MATH/MODE menus all work. Function graphing is fully implemented — Y= editor, RANGE, ZOOM (8 presets, ZBox, Set Factors), and TRACE. State persists across power cycles; `2nd+ON` enters Stop mode with full wake restore.

STAT, PRGM, and MATRIX are not yet implemented.

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

## Why

The TI-81 was the first graphing calculator that was genuinely accessible for learning — affordable, approachable, and in the hands of a generation of students. It also has a look that hasn't aged: boxy, purposeful, built to last.

The problem is that the originals are now 30+ years old and the displays are failing. Rather than let them become landfill, this project replaces the internals while keeping everything that made the hardware iconic. A fresh screen, a modern MCU, and the same keypad you already know.

The end goal is a calculator you can actually use — not a shelf piece. That means completing the feature set, finishing the custom PCB, and ending up with something better than the original: same form factor, same feel, working display, and no artificial limits on what it can do.
