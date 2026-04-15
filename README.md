# Neo-81: bringing new life to the classic.

[![Build](https://github.com/mndxc/STM32F429-TI81-Calculator/actions/workflows/build.yml/badge.svg)](https://github.com/mndxc/STM32F429-TI81-Calculator/actions)
[![License](https://img.shields.io/github/license/mndxc/STM32F429-TI81-Calculator)](LICENSE)
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

The core calculator experience is complete and daily-usable on the STM32F429I-DISC1 development board. A custom PCB to replace the original TI-81 internals while keeping the original shell is in progress.

**Working today:**
- Arithmetic, expression evaluation, operator precedence, parentheses, history recall
- A–Z variables, Ans; full MATH / TRIG / ANGLE / TEST function menus
- Function graphing (Y₁–Y₄), parametric mode, TRACE, ZOOM, RANGE, DRAW overlay
- Matrix operations — 3 matrices up to 6×6; arithmetic, det, transpose, row operations
- Statistics — 1-Var, four regression models, scatter / XYLine / histogram plots
- PRGM editor and executor — `If`, `Goto/Lbl`, loops, `Disp`, `Input`, subroutines; programs persist across power-off
- VARS / Y-VARS menus; all results persist in FLASH

**Still in progress:**
- Sci / Eng notation and remaining MODE screen options
- Startup splash screen
- Custom PCB (drop-in replacement for original TI-81 internals)

---

## No hardware? No problem.

You can build and run the full test suite on any machine — no STM32 board required.

```bash
git clone https://github.com/mndxc/STM32F429-TI81-Calculator.git
cd STM32F429-TI81-Calculator
cmake -S App/Tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests
```

All host tests pass on plain x86/ARM Linux and macOS with any standard C compiler — see [docs/TESTING.md](docs/TESTING.md) for the current suite breakdown. No toolchain, no board, no USB cable needed.

---

## Documentation

| I want to… | Go here |
|---|---|
| Build and flash the firmware | [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) |
| Understand the architecture | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| Read the full technical reference | [docs/TECHNICAL.md](docs/TECHNICAL.md) |
| Run the host tests | [docs/TESTING.md](docs/TESTING.md) |
| Troubleshoot common issues | [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) |
| See development process standards | [docs/MAINTENANCE_STANDARDS.md](docs/MAINTENANCE_STANDARDS.md) |
| Understand display stability and power-off behaviour | [docs/DISPLAY_STABILITY.md](docs/DISPLAY_STABILITY.md) |
| Understand power management and Stop mode | [docs/POWER_MANAGEMENT.md](docs/POWER_MANAGEMENT.md) |
| Contribute | [CONTRIBUTING.md](CONTRIBUTING.md) |

---

## Contributing

Contributions are welcome — bug fixes, feature work, or documentation improvements. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. All contributors are expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
