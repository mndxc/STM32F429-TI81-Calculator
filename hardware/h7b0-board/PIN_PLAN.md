# Main board pin plan (STM32H743V, LQFP100)

Updated 2026-11-27, when the MCU changed from the STM32H7B0VBT6 to the **STM32H743** to get the firmware into internal flash. Pins and alternate functions come from `_reference/datasheets/STM32H743VG.pdf` (local): ST DS12110, which covers both the H743xI and H743xG (Table 9 pin definitions, LQFP100 column, and Tables 10–20 AF tables). **Check the AF numbers in CubeMX before the schematic is frozen.**

## Part choice (live JLCPCB data, 2026-11-27)

| Part | JLCPCB # | Flash / RAM | JLCPCB stock | Price |
|---|---|---|---|---|
| **STM32H743VGT6** (**chosen for prototypes**, 2026-11-27) | C730183 | 1 MB / 1 MB | **268** | $11.31 (1+), $9.08 (90+) |
| STM32H743VIT6 (drop-in upgrade if firmware outgrows 1 MB) | C114409 | 2 MB / 1 MB | **0**, pre-order (est. 12 days) | est. $10.16–10.55 |
| W25Q128JVSIQ | C97521 | 16 MB NOR | **63,068**, **Basic** part | $2.16 |

Both H743 parts have the **identical LQFP100 pinout**, so this plan works for either. **Prototypes use the VGT6:** it's in stock, minimum 1, no pre-order. Its 1 MB holds the ~820 KB firmware (measured 2026-04-28) with about 200 KB to spare. If the firmware outgrows that, move LVGL fonts and images to the W25Q, or fit the VIT6 with no board change. LCSC also showed the VIT6 out of stock (1+ $10.64 … 100+ $7.69). The STM32H750VBT6 (C404010, 7,389 in stock, $6.81) shares the pinout but has only 128 KB flash, so it isn't a fallback.

## Decisions

- **MCU: STM32H743V (LQFP100)**. The firmware runs from internal flash. The W25Q128 holds **user data only**: no XIP, no boot stub, no external-flash loader.
- **Display bus: 8080 16-bit through FMC bank 1.** The panel is set to IM[3:0] = `0001` (8080 16-bit Interface I, D[15:0]). The framebuffer goes in internal RAM: 320 × 240 × 2 = 150 KB of the 1 MB. No SDRAM.
- **Board split:** the panel sits on a **display carrier** board on the case posts. An FFC runs from there to this main board (see `../enclosure/CASE_MEASUREMENTS.md`).
- **Keypad:** your own 50-key board as in `neo81-v1`. Keys go on this board's front side, parts on the back.

## Display (FMC, NE1)

| Signal | Pin | LQFP100 | Panel FPC pin |
|---|---|---|---|
| D0–D3 | PD14, PD15, PD0, PD1 | 61, 62, 81, 82 | 32, 31, 30, 29 |
| D4–D12 | PE7–PE15 | 37–45 | 28–20 |
| D13–D15 | PD8, PD9, PD10 | 55, 56, 57 | 19, 18, 17 |
| CSX ← FMC_NE1 | PD7 | 88 | 38 |
| D/CX ← FMC_A16 | PD11 | 58 | 37 |
| WRX ← FMC_NWE | PD5 | 86 | 36 |
| RDX ← FMC_NOE | PD4 | 85 | 35 |
| RESX (GPIO) | PD13 | 60 | 10 |
| TE (EXTI6) | PD6 | 87 | 39 |
| Backlight PWM (TIM4_CH1) | PD12 | 59 | via driver → LEDK pin 1 |

All display signals are on ports D and E. Panel DB17/DB16 (FPC pins 15, 16) aren't used in 16-bit mode: tie them to GND on the carrier. A16 as D/CX puts the data register at `0x60020000` for 16-bit accesses (A16 → HADDR bit 17). Set the FMC region to **Device / non-cacheable in the MPU**. Without it, the Cortex-M7 can merge or reorder display writes.

## External flash (W25Q128JV, QUADSPI bank 1, indirect mode only)

| Signal | Pin | LQFP100 | Datasheet function |
|---|---|---|---|
| CLK | PB2 | 36 | QUADSPI_CLK |
| NCS | PB6 | 92 | QUADSPI_BK1_NCS |
| IO0 | PC9 | 66 | QUADSPI_BK1_IO0 |
| IO1 | PC10 | 78 | QUADSPI_BK1_IO1 |
| IO2 (/WP) | PE2 | 1 | QUADSPI_BK1_IO2. **The only IO2 option on LQFP100** |
| IO3 (/HOLD) | PA1 | 23 | QUADSPI_BK1_IO3 |

The other IO0/IO1/IO3 options (PD11–PD13) are taken by FMC. PC11 is QUADSPI **bank 2** NCS, so it can't be used.

## Keypad (KeyRow1–10 × KeyCol1–5 + KEY_POWER, net names as in `neo81-v1/keypad.kicad_sch`)

| Lines | Pins | LQFP100 | Notes |
|---|---|---|---|
| KeyRow1–6 | PE0, PE1, PE3, PE4, PE5, PE6 | 97, 98, 2–5 | Drive lines (PE2 went to QUADSPI) |
| KeyRow7–10 | PC13, PC6, PC7, PC8 | 7, 63, 64, 65 | Drive lines. PC13 is a backup-domain pin (low drive, ≤ 2 MHz): fine for a keypad |
| KeyCol1–5 | PB10–PB14 | 46, 47, 51–53 | Sense lines, EXTI10–14 |
| KEY_POWER | PA0 | 22 | Wake-up pin: wakes from Standby as well as Stop |

Each sense line has its own EXTI line, so any key wakes the MCU from Stop: drive all rows low before sleeping. EXTI5/6 are left for the charger INT and the panel TE. The firmware keypad driver (`App/HW/Keypad/`) moves from the TI-81's 7 × 8 matrix to this 10 × 5 one.

## Power, USB, debug, clocks

| Function | Pin | LQFP100 |
|---|---|---|
| USB D− / D+ (OTG_FS, internal PHY; 48 MHz from HSI48 + CRS) | PA11, PA12 | 70, 71 |
| RT9471 I²C1 SCL / SDA | PB8, PB9 | 95, 96 |
| RT9471 INT (EXTI5) | PB5 | 91 |
| Battery voltage (ADC123_INP10, divider) | PC0 | 15 |
| SWDIO / SWCLK / SWO | PA13, PA14, PB3 | 72, 76, 89 |
| HSE crystal | PH0, PH1 | 12, 13 |
| LSE 32.768 kHz (RTC) | PC14, PC15 | 8, 9 |
| BOOT0 | — | 94, pull-down |
| VCAP ×2 | — | 48, 73: 2.2 µF each (no SMPS on this package; LDO only) |

## Spare GPIO (after this plan)

PA2–PA10 (PA9/PA10 = USART1 debug UART), PA15, PB0, PB1, PB4, PB7, PB15, PC1, PC4, PC5, PC11, PC12, PD2: 22 pins. Reserve SPI + UART + an interrupt line from these for the future Pi Zero 2 W link (RT4812 is DNF until then). PC2_C/PC3_C are analog-only.

## Carrier ↔ main board FFC (proposed, 40-pin 0.5 mm)

16 data + 6 control (CSX, D/CX, WRX, RDX, RESX, TE) + 3V3 ×2 + LEDA ×2 + LEDK ×2 = 28. The other 12 are GND, spread through the data bus (about one every 4 signals). Final pin order gets set when the two schematics are drawn.

## Open items for the schematic

- **Backlight driver:** 4 LEDs in parallel, Vf ≈ 3.2–3.4 V, 70–80 mA total. 3.3 V has no headroom, so drive LEDA from SYS (3.5–4.4 V). Resistor current would swing too much over that range, so use a small constant-current LED driver or a PWM'd current sink on LEDK. The driver goes on the main board.
- **Panel supply:** VCI and VDDI from the 3.3 V rail (allowed range up to 3.3 V).
- The existing open items in `PARTS_VERIFICATION.md` (USB CC/PSEL, footprint swaps) still apply. The STM32H7B0 rows there are superseded by this part change.
