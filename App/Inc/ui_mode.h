/**
 * @file    ui_mode.h
 * @brief   MODE settings screen — state, init, display, and token handler.
 *
 * Extracted from calculator_core.c. Part of the UI super-module:
 * include calc_internal.h before this file in translation units that need
 * the full shared-state context; this header is self-contained for callers
 * that only need the type definitions and the public API.
 *
 * Consumers: calculator_core.c (via calc_internal.h), ui_mode.c itself.
 */

#ifndef UI_MODE_H
#define UI_MODE_H

#include "app_common.h"   /* Token_t, CalcMode_t */
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define MODE_ROW_COUNT   8    /**< Rows in the MODE screen                  */
#define MODE_MAX_COLS   11    /**< Max options per MODE row (row 2 has 11)   */

/*---------------------------------------------------------------------------
 * Types
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t  row_selected;             /**< Currently navigated row (0-based) */
    uint8_t  cursor[MODE_ROW_COUNT];   /**< Highlighted option per row        */
    uint8_t  committed[MODE_ROW_COUNT];/**< Committed (active) option per row */
} ModeScreenState_t;

/*---------------------------------------------------------------------------
 * Shared state (defined in ui_mode.c)
 *---------------------------------------------------------------------------*/

/** MODE screen state — accessed by Persist_BuildBlock / Persist_ApplyBlock in persist.c. */
extern ModeScreenState_t s_mode;

/*---------------------------------------------------------------------------
 * Public API
 * Functions that reference LVGL types are guarded so this header is safe
 * to include in HOST_TEST builds before LVGL stubs are loaded.
 *---------------------------------------------------------------------------*/

#ifndef HOST_TEST
#  include "lvgl.h"

/**
 * @brief  Hide the MODE overlay screen. Caller holds lvgl_lock().
 *         Used by hide_all_screens() in calculator_core.c.
 */
void Mode_HideScreen(void);

/**
 * @brief  Create all LVGL objects for the MODE settings screen.
 *         Called once from StartCalcCoreTask while the LVGL mutex is held.
 */
void ui_mode_init(void);

/**
 * @brief  Open the MODE screen: sync cursor to committed values, set mode,
 *         hide all overlays, and make the MODE screen visible.
 *         Replaces the inline TOKEN_MODE handler in calculator_core.c.
 */
void ui_mode_open(void);

/**
 * @brief  Token handler for MODE_MODE_SCREEN.
 * @return true if the token was consumed; false to fall through to normal mode.
 */
bool handle_mode_screen(Token_t t);

/**
 * @brief  Redraw all MODE screen option labels with correct highlight colours.
 *         Must be called under lvgl_lock().
 */
void ui_update_mode_display(void);

#endif /* !HOST_TEST */

#endif /* UI_MODE_H */
