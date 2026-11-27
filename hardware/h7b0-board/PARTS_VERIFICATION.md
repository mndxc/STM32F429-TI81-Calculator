# H7B0 board: parts verification

**Checked 2026-11-27.** Each part's symbol, pinout, and footprint in the datasheets.md library (workspace "Personal") were compared with the manufacturer's original PDF. The service's own extracted text was not used. The STM32 was checked by script: all 100 LQFP100 pins against ST's Table 7. The tool notes and raw coverage results are in `~/Projects/Reference/KiCad-Tools/datasheets-md/`.

## Library parts

| Part | Pinout vs datasheet | Footprint | Verdict |
|---|---|---|---|
| STM32H7B0VBT6 | ✅ 100/100 (KiCad's own `STM32H7B0VBTx` symbol) | ✅ LQFP-100 14×14 P0.5 | Use |
| W25Q128JVSIQ | ✅ 8/8, active-low /CS /WP /HOLD marked | ✅ SOIC-8 5.3×5.3 (208-mil) | Use |
| RT8059GJ5 | ✅ EN, GND, LX, VIN, FB | ✅ TSOT-23-5 | Use (generated box symbol) |
| RT4812GJ8F | ✅ 8/8 | ✅ TSOT-23-8 (Richtek land pattern: P 0.65, pad 1.0×0.45) | Use (generated box symbol) |
| RT9471GQW | ✅ 25/25 (plain RT9471) | ⚠️ generated from the outline drawing; the land pattern was not read | Use the symbol. **Swap the footprint for KiCad `QFN-24-1EP_4x4mm_P0.5mm_EP2.6x2.6mm`**, which matches Richtek Option 2 (Sx = Sy = 2.60) |
| TPD4E05U06DQAR | ✅ 10/10 | ✅ USON-10 2.5×1.0 | Use. TI makes this part only in USON-10 (DQA). NC pins 6, 7, 9 and 10 are for optional straight-through routing |
| AO3401A | ✅ G/S/D = 1/2/3 | ✅ SOT-23 | Use. S and D are typed power_in, which causes cosmetic ERC noise only |
| BAT54WS-7-F | ✅ 1 = K, 2 = A | ✅ SOD-323 | Use |
| JST S2B-PH-SM4-TB | pins OK | ❌ none | Use KiCad `Connector_JST:JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal` |
| TYPE-C-31-M-12 | numbered 1–16, not A1–B12, no shield | ❌ none | Don't use. Use `libs/JP-USB-C.kicad_sym` with the EasyEDA footprint |

Not in the library: the ER-TFT028A2-4 display and IP5310 (neither is on DigiKey), and AMS1117-3.3, for which only UTD's SOT-89 version is listed. Passives use KiCad's built-in symbols.

Nothing is marked "reviewed" in datasheets.md yet.

## Charger USB detection: RT9471 vs RT9471D (evaluated 2026-11-27)

**Where the question came from.** `docs/PCB_DESIGN.md` lists the **RT9471** in the BOM. It also says the ESD part goes "before the RT9471 D+/D− pins". The plain RT9471 has no D+/D− pins: its pins 2 and 3 are PSEL and PG. Only the **RT9471D** has D+/D−, which it uses for BC1.2 charger-type detection. It drops PSEL and PG, and swaps VBUS and VAC between pins 1 and 24. The two lines contradict each other, and no other doc records a requirement for BC1.2 detection. So the D+/D− line looks like a doc mistake, not a design decision.

**What each variant gets you:**

| | RT9471 (BOM today) | RT9471D |
|---|---|---|
| How it picks the input current limit | The PSEL pin (high = 0.5 A, low = 2.4 A), then firmware sets AICR over I²C | Automatic BC1.2 detection on D+/D− (SDP, CDP or DCP) |
| What that detection helps | — | Only **legacy USB-A chargers on an A-to-C cable**. USB-C chargers advertise current on **CC**, which neither chip reads |
| USB data to the MCU | D+/D− go straight to the STM32 (PA11/PA12, internal FS PHY) | D+/D− would be shared between the charger and the STM32, so the STM32's D+ pull-up and the BC1.2 detection phase have to be kept from clashing |
| Extra pins | PG power-good output | — |

The STM32H7B0's own OTG_HS supports **BC1.2** (datasheet §3.47), so the MCU can detect legacy chargers itself if that's ever wanted.

**Recommendation: keep the plain RT9471** and correct the doc:
1. Route USB D+/D− to the **STM32 (PA11/PA12)**, not the charger. That keeps USB data available: the ROM bootloader's USB DFU gives firmware updates without an ST-Link (confirm the H7B0 entry in ST AN2606), and it leaves room for program or variable transfer later.
2. **Add 5.1 kΩ pull-downs (Rd) on CC1 and CC2. They're missing from the design today.** Without them, USB-C to USB-C chargers won't turn on VBUS at all.
3. Tie **PSEL high** for a safe 0.5 A default. At boot, firmware reads the CC voltage with the ADC (Default, 1.5 A or 3 A advertisement) and only then raises AICR to 1.5 A. Setting 1.5 A unconditionally, as `PCB_DESIGN.md` says now, overdraws a 500 mA computer port. The RT9471's VBUS voltage regulation (VINDPM) would limit the damage, but it's still out of spec.
4. The 4-channel TPD4E05U06 can cover D+, D−, CC1 and CC2.

The RT9471D would only make sense if the calculator never used USB data and you wanted fast charging from old USB-A bricks with no firmware involved. Neither seems to be a goal.

**Still to confirm before layout:** JLCPCB/LCSC stock for **RT9471GQW** specifically (`PCB_DESIGN.md` says "all verified on JLCPCB" but doesn't name the variant); the CC-sensing ADC pins and divider; and AN2606 USB DFU support on PA11/PA12 for the H7B0.
