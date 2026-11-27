# TI-81 case measurements

Caliper readings from the front shell (blue case, version B). Photos are in `_reference/TI-81 Stuff/disassembly/`.

## Display bay

| Dimension | mm | Photo | Notes |
|---|---|---|---|
| Bay interior width | 74.47 | IMG_4979 | Confirmed by the user |
| Opening in the lip the display rests on (width) | 64.93 | IMG_4982 | Confirmed by the user. The panel sits **on top of** this lip |
| Lip opening height | 41.96 | IMG_4981 | Confirmed. With the width, the lip opening is 64.93 × 41.96 |
| Top inner wall to the **outer** side of the display screw posts | 53.72 | IMG_4980 | Confirmed |
| Original window sticker width | 58.49 | IMG_4983 | Confirmed. This is the protective sticker on the outside of the lip, not structure. It will be replaced |
| Display post spacing, **centre to centre** | 63.02 | — | Caliper reading by the user. Photo check: scaled to the 74.47 bay width, the centre-to-centre spacing comes out at 63.6 (IMG_4389) and 63.8 (IMG_4980). Edge to edge would give about 67.7 outside or 59.9 inside, so 63.02 is centre to centre |
| Post boss outside diameter | ~4 (photo estimate) | IMG_4980, IMG_4389 | Measured in pixels against the bay width, ±0.5. Confirm with the caliper |
| Post hole diameter | ~2 (photo estimate) | IMG_4980, IMG_4389 | Same method, ±0.5. It takes small self-tapping screws (shown in IMG_4389). Confirm with the caliper or a drill-bit gauge |
| Post position | — | IMG_4389, IMG_4980 | Both posts sit at the **keypad end** of the bay, one in each corner, about 5.7 from the side walls ((74.47 − 63.02) / 2). A black foam gasket runs along the lip between them |

## ER-TFT028A2-4 dimensions (from the datasheet drawing)

Source: `hardware/datasheets/ER-TFT028A2-4_Datasheet.pdf`, page 6, "3.1 ER-TFT028A2-4 with No Touch Panel Outline Drawing" (rev 1.0, Dec-10-2019). Portrait orientation, with the FPC at the bottom.

| Item | mm |
|---|---|
| Module outline | 50.20 × 69.30 |
| Thickness, FPC folded | 2.80 |
| LCD glass | 46.20 × 64.75 |
| Visual area (VA) | 44.80 × 59.20 |
| Active area (AA) | 43.20 × 57.60 (240 × 320 at 0.18 pitch) |
| Top edge (away from the FPC) to glass / VA / AA | 1.70 / 2.40 / 3.20 |
| FPC-end edge to AA | 8.50 (= 69.30 − 3.20 − 57.60) |
| Side margins to AA | 3.50 each side (the drawing shows AA centred across the 50.20 width) |
| Back frame | 47.04 × 67.75, with 1.00 MAX component height in the FPC area |
| FPC tail, from module edge to the end of the contacts | 26.70 ± 0.50 |
| FPC wide section next to the module | 45.90 wide, extends 13.20 |
| FPC contact end | 25.50 wide stiffener, 3.50 long, 0.30 ± 0.03 thick, 50 pins at 0.5 pitch |
| "Remove tape" tab | Sticks out from one long side; peel it off before fitting |

The datasheet gives the outline **with the FPC folded** as 50.20 × 69.30 × 2.80. So the FPC is designed to fold flat behind the panel inside the 69.30 length; it does not need extra room at the side.

## Fit check: panel in landscape

Panel orientation: 69.30 across the case, 50.20 top to bottom, FPC on the left or right.

| Check | Result |
|---|---|
| Width: 69.30 panel in 74.47 bay | Fits, 2.59 mm spare each side |
| Resting on the lip across: 69.30 over 64.93 opening | 2.19 mm overlap each side |
| Resting on the lip vertically: 50.20 over 41.96 opening | 4.12 mm overlap top and bottom |
| AA across, panel centred in the bay | AA spans 11.09 to 68.69 mm from the FPC-side wall. Lip opening spans 4.77 to 69.70 (assuming it's centred). **All 320 columns visible.** AA sits 3.15 mm off-centre toward the far side, because of the 8.50 / 3.20 asymmetry |
| AA vertically: 43.20 through 41.96 | **1.24 mm short: about 7 pixel rows hidden.** Trim the lip by at least 0.62 mm top and bottom to show the whole AA (1.42 mm each to show the full VA) |
| Height: 50.20 panel vs the posts | The far side of the posts is 53.72 from the top wall. Minus a ~4 boss, the **near** side is about 49.8, so a 50.20 panel pushed against the top wall would **overlap the bosses by ~0.4 mm** in plan view. The posts stand up from the front shell at the same depth as the panel, so this could be a real collision. Measure top wall → near side of the posts directly. If it's tight, shave the top-wall ledge or the boss faces (allowed) |
| FPC | Folds flat behind the panel within the outline (2.80 total). The side gap is no longer the problem. The open question is routing the 26.70 mm tail from behind the panel to a ZIF on the carrier |

## Incline (photo estimate)

Source: IMG_4984–4986, the front shell lying face down, photographed from a low angle. The best view is the far bay wall in IMG_4984.

The lip the panel rests on is **not parallel to the seam** (the rim where the two shell halves meet). Measured on the far wall, the wall height from the rim down to the lip is:

| Point | Pixels (4× crop) | mm (estimate) |
|---|---|---|
| Keypad end, at the posts | 55 | ~3.3 |
| Top end of the bay | 125 | ~7.4 |

The two points are about 41 mm apart. That gives **≈ 6° (±2°)**: atan((7.4 − 3.3) / 41).

How the numbers were scaled: the horizontal scale comes from the bay length (~21 px/mm in the crop). Vertical distances on the wall are corrected for a camera elevation of ~38°, estimated from how foreshortened the 64.93 × 41.96 lip opening looks. Perspective is the main error source. Using the ratio of wall heights cancels camera roll, but not depth differences along the wall.

What this means:
- The case is thicker at the top end. The lip, and so the panel, is parallel to the front face and tilted ~6° relative to the seam and the main board.
- At the post end, only ~3.3 mm of front-shell depth sits behind the lip. The 2.80 mm panel uses almost all of it, so the carrier and ZIF sit at about the seam or into the back shell there. At the top end there is ~4 mm more room: a good place for the ZIF.
- To pin the angle down: use the caliper's depth rod from the rim to the lip at both ends of the bay.

## Case modification is allowed

The user plans to cut the shell with a Dremel wherever it helps: for example, opening up the lip to show the whole active area. They will make a new window cover from other plastic if needed. So the lip opening and the original window are **not** hard limits. The bay walls, the posts and the outside of the case are.

## Status

Measurement phase closed on 2026-11-27: these numbers are good enough to design the carrier. The few open sub-mm items get settled on the first prototype. That includes the possible ~0.4 mm overlap between the panel and the post bosses, which a Dremel can fix.
