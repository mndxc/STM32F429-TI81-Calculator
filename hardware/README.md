# Neo-81 hardware

Everything for the physical calculator: boards, libraries and enclosure. The firmware is at the repo root, and the board design notes are in [`docs/PCB_DESIGN.md`](../docs/PCB_DESIGN.md).

| Folder | What it is | Status |
|---|---|---|
| [`h7b0-board/`](h7b0-board/) | **Next board**: STM32H743V (was H7B0), RT9471 charger, RT8059 buck, W25Q128 flash, own 50-key keypad; plus a display carrier board | Pin plan and parts list; no KiCad project yet |
| [`neo81-v1/`](neo81-v1/) | First board: keypad + power (IP5310 power bank SoC, 50 touch keys), 6-layer; connects to the STM32F429 Discovery via the J4 header. Its schematic also held an MCU draft, now in `unused-sheets/` | KiCad project plus prepared gerber/BOM/CPL exports; **never ordered** |
| [`libs/`](libs/) | Project KiCad libraries: EasyEDA/LCSC parts, IP5310, JP-USB-C, softKey key footprints | Both boards use them through `${KIPRJMOD}/../libs` |
| [`enclosure/`](enclosure/) | FreeCAD: front shell, keypad PCB outline, key footprint model; `PCBPlan.pages` | Early (Oct–Nov 2025) |
| [`KICAD_CONVENTIONS.md`](KICAD_CONVENTIONS.md) | How symbols, fields, BOM flags, rules and libraries are set up, and the pre-order checklist | Read before any KiCad work |

Datasheets, the TI-81 guidebook, teardown photos and downloaded KiCad libraries are local only, in `../_reference/` (see [What's not in this repo](../README.md#whats-not-in-this-repo)).

## Opening a board in KiCad (10.x)

1. Open `neo81-v1/neo81.kicad_pro`. Its `sym-lib-table` and `fp-lib-table` load `libs/`, so the project works from any path or clone without global library setup.
2. **The JLCPCB library is not in the repo.** Four parts (`PCM_JLCPCB-*`) come from the CDFER JLCPCB library. Install it once with *Plugin and Content Manager → Install from File…* and pick the downloaded zip (kept locally at `_reference/kicad-libs/third-party/com.github.CDFER.JLCPCB-Kicad-Library_v2025.07.18.zip`). You can also install it straight from the PCM catalogue.
3. Checks after the 2026-11-27 cleanup (`kicad-cli` 10.0.0): ERC has **1 error**: IP5310 pin 31 (LIGHT, an output) is tied to GND. Check that against the IP5310 datasheet. DRC with parity: 0 parity issues and 0 unconnected pads. The 7 starved-thermal and 6 USB-C edge-clearance findings are real copper items for the next spin. `unused-sheets/` holds the draft MCU sheet and two orphan sheets that aren't part of this board. Library messages:
   - `PCM_JLCPCB` / `PCM_JLCPCB-Diodes` not configured. Step 2 fixes this.
   - `STM32F429IITx_multi` isn't in KiCad 10's `MCU_ST_STM32F4`. The schematic uses its own stored copy.
   - Several "doesn't match copy in library" warnings. The board and schematic keep their own copies of each part; **don't run "Update from Library" in bulk** without reviewing the changes.
   - The softKey footprints refer to 3D models (`footprintv1.wrl`, `Unnamed.wrl`) that were never exported. Only the 3D view is affected. The source is `enclosure/footprintv1.FCStd`.

## Ordering and assembly

- `neo81-v1/fab-outputs/`: `jlcpcb/production_files/` holds the JLCPCB plugin export (gerbers, `BOM-neo81.csv`, `CPL-neo81.csv`). `manual-gerbers/` is a hand export, and `bom-cpl/` holds its position files. Three zipped gerber sets are export variants that were prepared but never ordered: `neo81v01.zip` (2025-11-19), `neo81MarkH.zip` (2025-11-13), and `neo81TwoBoard.zip` (2026-02-04).
- Before ordering a new board, check the parts in [`h7b0-board/PARTS_VERIFICATION.md`](h7b0-board/PARTS_VERIFICATION.md). Some library footprints must be swapped, and there are open design items.

## What git tracks

The design files are committed: `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, lib tables, project libraries, FreeCAD and fab outputs. Tool output is ignored by pattern: KiCad backups (`*-backups/`, `*-bkp-*`), caches (`*.kicad_prl`, `fp-info-cache`) and FreeCAD backups (`*.FCBak`). Datasheets and downloaded libraries are local only, under `_reference/`; see the root `.gitignore`.
