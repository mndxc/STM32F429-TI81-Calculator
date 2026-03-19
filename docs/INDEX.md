# Documentation Index — STM32F429 TI-81 Calculator (Neo-81)

---

## Guides

| Document | Description |
|---|---|
| [GETTING_STARTED.md](GETTING_STARTED.md) | Zero-to-build guide for new contributors. Covers prerequisites, hardware wiring (keypad matrix, display), toolchain setup, and flashing the board. Start here if you are new to the project. |
| [TECHNICAL.md](TECHNICAL.md) | Full architecture and implementation reference. Project file structure, build configuration, memory layout (RAM/FLASH/SDRAM), LVGL threading rules, calculator engine pipeline, graphing system, and keypad driver details. |
| [UsageDelayedQueue.md](UsageDelayedQueue.md) | Implementation plan for persistent FLASH storage. Covers the `PersistBlock_t` struct layout, FLASH sector assignment (Sector 7 @ 0x080C0000), RAM-function requirements for single-bank FLASH, and the `2nd+ON` save gesture. |

---

## Datasheets

| File | Component | Purpose in Project |
|---|---|---|
| [TI81Guidebook.pdf](Datasheets/TI81Guidebook.pdf) | Texas Instruments TI-81 | Original TI-81 user manual. Reference for calculator behaviour, key layout, PRGM syntax (pages 133–150), and menu specifications. |
| [RT9471Charger.pdf](Datasheets/RT9471Charger.pdf) | Richtek RT9471 | LiPo charger IC with power-path management. I²C programmable, 3A charge current. Used on the custom PCB design. |
| [W25Q128JVSIQFlashMem.pdf](Datasheets/W25Q128JVSIQFlashMem.pdf) | Winbond W25Q128JV | 16MB SPI NOR flash. Planned for OctoSPI1 on the custom PCB for firmware XIP and user data storage (variables, programs, settings). |
| [RT4812Boost.pdf](Datasheets/RT4812Boost.pdf) | Richtek RT4812 | 5V boost converter. DNF (Do Not Fit) on PCB Rev1; reserved for Rev2 to supply a Raspberry Pi Zero 2 W. |
| [RT8059Buck.pdf](Datasheets/RT8059Buck.pdf) | Richtek RT8059 | Buck converter reference. Used in the 3.3V main power rail design (see also MT2492 notes in CLAUDE.md). |
| [TPD4E05U06DQARDiode.pdf](Datasheets/TPD4E05U06DQARDiode.pdf) | TI TPD4E05U06DQAR | 4-channel USB ESD protection diode. Placed on USB D+/D− lines on the custom PCB. |

---

## Images

| File | Description |
|---|---|
| [FrankenCalc.jpeg](FrankenCalc.jpeg) | Photo of the prototype hardware ("FrankenCalc") — STM32F429I-DISC1 wired to the original TI-81 keypad and ribbon cable. |
| [Image_1.jpg](Image_1.jpg) | Additional hardware or build photo. |
| [Image_2.jpg](Image_2.jpg) | Additional hardware or build photo. |

---

## Other Resources

- **[CLAUDE.md](../CLAUDE.md)** — AI session context file. Contains feature completion status, known issues, next-session priorities, menu specs, architecture summary, and common gotchas. Kept up to date with every development session.
- **[README.md](../README.md)** — Top-level project overview and quick-start summary.
