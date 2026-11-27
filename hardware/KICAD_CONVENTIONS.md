# KiCad conventions for this project

These came out of the 2026-11-27 review of `neo81-v1`. Each rule names the problem it fixes in that board. Follow them for `h7b0-board` from day one.

## Schematic

1. **The hierarchy holds only what's on this board.** Drafts and ideas go in `unused-sheets/` or their own project.
   *neo81-v1:* a half-finished STM32 sheet was in the hierarchy. It wasn't on the PCB, so it caused 174 ERC errors that buried the real ones.
2. **Annotate everything.** No `J?` or `U?`. Run *Tools → Annotate* before every PCB update.
3. **Prefer KiCad's built-in symbols.** Use `Device:R`, `Device:C`, `Device:L`, `Device:LED`, `Device:Q_PMOS` and the standard footprints (`Resistor_SMD:R_0603_1608Metric`, …), and put the part number in fields. Import EasyEDA/LCSC symbols only for ICs and connectors KiCad doesn't have.
4. **After importing a symbol, fix its pin electrical types:**
   - connectors, passives and transistors: **Passive**;
   - real supply pins: Power input or Power output;
   - stacked duplicate pins (several VOUT, LX or GND): one keeps its type and the rest are **Passive**.
   *neo81-v1:* the imported symbols caused 32 ERC conflicts, from resistor pins typed "Input" and all-"Unspecified" USB-C pins.
5. **Put a `PWR_FLAG` on every supply net** that only a connector or passive part drives (GND, VBUS from a connector, battery).
6. **Use the same fields on every part:** `Reference`, `Value`, `Footprint`, `Datasheet`, `LCSC`, `MPN`, `Manufacturer`. Leave out stock, price and category; they go stale. *neo81-v1:* it mixed `LCSC Part` and `LCSC`, and its LEDs carried 17 scraped JLCPCB fields.
7. **Set the BOM flags correctly.** Copper-only features (touch-key pads, logos, test pads, mounting holes) get *Exclude from BOM* and *Exclude from position files*. Fitted-later or optional parts get **DNP**.
   *neo81-v1:* all 50 touch keys went to JLCPCB as BOM lines with no part number.

## Board

8. **Set the fab limits before routing** (*Board Setup → Constraints*), and **don't switch checks off to silence them.** Fix the problem, or exclude that one violation with a comment. *neo81-v1:* every minimum was 0 and the solder-mask bridge check was off, which hid 110 findings.
9. **Use net classes for power.** The `Power` class here is 0.5 mm track, 0.8/0.4 via, and covers GND, 5VOUT, 3V3OUT, V_BUS and battery nets. *neo81-v1* has 0.2 mm segments on 5VOUT (15) and V_BUS (4); widen them on the next spin.
10. **Mark footprints that bridge on purpose** (touch keys, interleaved pads) with *Allow soldermask bridges* instead of turning the check off.
11. **Keep the schematic and board in sync:** *Update PCB from Schematic* (F8), then DRC with **Test for parity** ticked. Parity errors must be 0 before ordering.
12. **Put a board name, revision and date on the silkscreen**, using the `${REVISION}` and `${ISSUE_DATE}` text variables. *neo81-v1* has none.
13. **Exposed pads need full thermal connections** on every plane. *neo81-v1:* the IP5310's center-pad vias have only 1 spoke on In2.Cu.

## Libraries

14. Project libraries live in `hardware/libs/` and load through each board's `sym-lib-table` and `fp-lib-table` (`${KIPRJMOD}/../libs/...`). Never use absolute paths.
15. 3D models go next to their footprint library and are referenced through `${KIPRJMOD}/../libs/...`.

## Before ordering

- [ ] ERC: 0 errors, with every remaining warning understood
- [ ] DRC with parity: 0 errors, 0 parity issues
- [ ] BOM export has an LCSC/MPN on every line, and no copper-only items
- [ ] Regenerate gerbers, drill, BOM and CPL into `fab-outputs/<date or rev>/`
- [ ] Commit, and tag the commit with the board revision
