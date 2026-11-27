# Main board (next revision): STM32H743

The folder is still named `h7b0-board/` from before the MCU change on 2026-11-27.

The custom board that replaces the STM32F429 Discovery: STM32H743V (LQFP100; was the H7B0VBT6), RT9471 charger and power path, RT8059 3.3 V buck, W25Q128JV QUADSPI flash for user data, and a LiPo. The RT4812 5 V boost is DNF until the Pi Zero 2 W revision.

**Status (2026-11-27):** active. It's being designed together with the display carrier board, so the Discovery can be retired. There's no KiCad project yet.

- **Display bus: 8080 16-bit through FMC** (not RGB/LTDC). Pin assignments: [`PIN_PLAN.md`](PIN_PLAN.md)

- Design notes: [`docs/PCB_DESIGN.md`](../../docs/PCB_DESIGN.md)
- Parts check, footprint swaps and the open charger/USB items: [`PARTS_VERIFICATION.md`](PARTS_VERIFICATION.md)
- Datasheets: [`../datasheets/`](../datasheets/) (local)

## Starting the KiCad project

1. In KiCad 10, create `h7b0-board.kicad_pro` in this folder.
2. Copy `../neo81-v1/sym-lib-table` and `fp-lib-table` here. They point at `../libs`. For datasheets.md parts, add their library the same way.
   Follow [`../KICAD_CONVENTIONS.md`](../KICAD_CONVENTIONS.md) from the first symbol.
3. Settle the open items in `PARTS_VERIFICATION.md` before you start the schematic: USB D+/D− routing, the CC pull-downs and PSEL.
