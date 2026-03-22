/**
 * @file ui_palette.h
 * @brief Named colour constants for all UI elements.
 *
 * Use these constants with lv_color_hex() everywhere instead of inline hex
 * literals.  Changing a colour here propagates automatically across the whole
 * codebase.
 */

#ifndef UI_PALETTE_H
#define UI_PALETTE_H

/* ---------------------------------------------------------------------------
 * Core palette
 * -------------------------------------------------------------------------*/

/** Pure black — all screen backgrounds, overlay backgrounds, graph canvas */
#define COLOR_BLACK         0x000000
/** White — result text, menu item default text, Y= equation text */
#define COLOR_WHITE         0xFFFFFF
/** Yellow — selected/highlighted menu items, active field labels */
#define COLOR_YELLOW        0xFFFF00
/** Amber — scroll-overflow indicators (↑↓ glyphs) */
#define COLOR_AMBER         0xFFAA00

/* ---------------------------------------------------------------------------
 * Grey scale
 * -------------------------------------------------------------------------*/

/** Light grey — live expression being typed */
#define COLOR_GREY_LIGHT    0xCCCCCC
/** Medium grey — committed history expressions, graph axis lines */
#define COLOR_GREY_MED      0x888888
/** Inactive grey — inactive menu tabs, unselected item text */
#define COLOR_GREY_INACTIVE 0x666666
/** Dark grey — graph grid dots, power-off overlay text */
#define COLOR_GREY_DARK     0x444444
/** Tick grey — graph axis tick marks */
#define COLOR_GREY_TICK     0x555555

/* ---------------------------------------------------------------------------
 * Mode-indicator colours (cursor block tints)
 * -------------------------------------------------------------------------*/

/** Amber — cursor block when 2ND modifier is active */
#define COLOR_2ND           0xF5A623
/** Green — cursor block when ALPHA modifier is active */
#define COLOR_ALPHA         0x7ED321

/* ---------------------------------------------------------------------------
 * Graph curve colours (one per Y= equation slot)
 * -------------------------------------------------------------------------*/

/** Y1 curve — white */
#define COLOR_CURVE_Y1      0xFFFFFF
/** Y2 curve — cyan */
#define COLOR_CURVE_Y2      0x00FFFF
/** Y3 curve — yellow */
#define COLOR_CURVE_Y3      0xFFFF00
/** Y4 curve — magenta */
#define COLOR_CURVE_Y4      0xFF80FF

#endif /* UI_PALETTE_H */
