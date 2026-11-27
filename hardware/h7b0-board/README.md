# H7B0 board (next revision)

The custom board that replaces the STM32F429 Discovery: STM32H7B0VBT6, RT9471 charger and power path, RT8059 3.3 V buck, W25Q128JV OctoSPI flash, and a LiPo. The RT4812 5 V boost is DNF until the Pi Zero 2 W revision.

**Status:** design notes only. There's no KiCad project yet, and the work is paused until the firmware is done.

- Design notes: [`docs/PCB_DESIGN.md`](../../docs/PCB_DESIGN.md)
- Parts check, footprint swaps and the open charger/USB items: [`PARTS_VERIFICATION.md`](PARTS_VERIFICATION.md)
- Datasheets: [`../datasheets/`](../datasheets/) (local)

## Starting the KiCad project

1. In KiCad 10, create `h7b0-board.kicad_pro` in this folder.
2. Copy `../neo81-v1/sym-lib-table` and `fp-lib-table` here. They point at `../libs`. For datasheets.md parts, add their library the same way.
   Follow [`../KICAD_CONVENTIONS.md`](../KICAD_CONVENTIONS.md) from the first symbol.
3. Settle the open items in `PARTS_VERIFICATION.md` before you start the schematic: USB D+/D− routing, the CC pull-downs and PSEL.
