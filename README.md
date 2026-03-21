# Neo-81: bringing new life to the classic.

[![Build Status](https://github.com/jonathanpool/STM32F429-TI81-Calculator/actions/workflows/build.yml/badge.svg)](https://github.com/jonathanpool/STM32F429-TI81-Calculator/actions)
[![License](https://img.shields.io/github/license/jonathanpool/STM32F429-TI81-Calculator)](LICENSE)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)


![Calculator running on STM32F429I-DISC1](docs/Image_2.jpg)

## Why

The TI-81 was the first graphing calculator that was genuinely accessible for learning — affordable, approachable, and in the hands of a generation of students. It also has a look that hasn't aged: boxy, purposeful, built to last.

The problem is that the originals are now 30+ years old and the displays are failing. Rather than let them become landfill, this project replaces the internals while keeping everything that made the hardware iconic. A fresh screen, a modern MCU, and the same keypad you already know.

The end goal is a calculator you can actually use — not a shelf piece. That means completing the feature set, finishing the custom PCB, and ending up with something better than the original: same form factor, same feel, working display, and no artificial limits on what it can do.

---

## Hardware

STM32F429I-DISC1 (Cortex-M4, 180 MHz, 2.4" ILI9341 display, 8 MB SDRAM) with a salvaged TI-81 key matrix wired to the GPIO header.

**Software:** LVGL v9 · FreeRTOS · GCC ARM · CMake

---

## Status

Core arithmetic, standard math functions (trig, hyperbolic, log, √), variables A–Z / ANS, TEST operators, and the MATH/MODE menus all work. Function graphing is fully implemented — Y= editor, RANGE, ZOOM (8 presets, ZBox, Set Factors), and TRACE. State persists across power cycles; `2nd+ON` enters Stop mode with full wake restore.

MATRIX is ~95% complete: variable dimensions (1–6×6), full arithmetic (+, −, ×, scalar×matrix), det, transpose, all row operations, scrolling cell editor with dim-mode resizing, and FLASH persistence.

**PRGM is coded but not yet hardware-validated — treat as non-functional until testing is complete.** The UI (EXEC/EDIT/ERASE menu, all 37 slots, name entry, line editor, CTL/I/O sub-menus) and a full text interpreter (`If/Then/Else/End`, `While`, `For(`, `Goto/Lbl`, `Pause`, `Stop`, `Return`, subroutine calls, `Disp`, `Input`, `Prompt`, `ClrHome`, assignment, expression lines) are implemented and programs persist in FLASH. None of this has been run on hardware yet — see `TEMP-prgm_manual_tests.md` for the 28-test validation plan that must pass before PRGM is considered functional.

STAT is not yet implemented.

---

## Documentation

| Document | Description |
|---|---|
| [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) | Zero-to-build guide — prerequisites, hardware wiring, toolchain setup, flashing. |
| [docs/TECHNICAL.md](docs/TECHNICAL.md) | Architecture reference — file structure, build config, memory layout, LVGL threading, calculator engine, graphing system, keypad driver. |
| [CLAUDE.md](CLAUDE.md) | AI development context — full feature status, architecture decisions, known issues, gotchas, and next session priorities. Read this before making any code changes. |

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

## Contributing

We welcome contributions from the community! Whether you're fixing a bug, adding a feature, or improving documentation, please see our [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to get started.

All contributors are expected to follow our [Code of Conduct](CODE_OF_CONDUCT.md).

