/**
 * @file    ui_yvars.h
 * @brief   Y-VARS menu UI — initialization, display-update, and token handler
 *          declarations.
 *
 * Three-tab menu opened by 2nd+VARS (TOKEN_Y_VARS):
 *   Y   — inserts Y₁–Y₄ equation reference strings into the expression buffer
 *   ON  — enables Y₁–Y₄ equations (writes graph_state.enabled[])
 *   OFF — disables Y₁–Y₄ equations (writes graph_state.enabled[])
 *
 * Parametric entries (X₁t, Y₁t etc.) are deferred until parametric graphing
 * Y-VARS support is implemented.
 *
 * Font notes (see CLAUDE.md gotcha #14):
 *   ₁₂₃₄ = U+2081–2084 → \xE2\x82\x81 … \xE2\x82\x84
 */

#ifndef APP_UI_YVARS_H
#define APP_UI_YVARS_H

#include "app_common.h"
#include "lvgl.h"

/*---------------------------------------------------------------------------
 * Menu navigation state
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    tab;         /* 0=Y  1=ON  2=OFF */
    uint8_t    item_cursor; /* Highlighted row index (0-based) */
    CalcMode_t return_mode; /* Mode to restore on CLEAR or action */
} YVarsMenuState_t;

/*---------------------------------------------------------------------------
 * Externally visible state
 *---------------------------------------------------------------------------*/

extern YVarsMenuState_t yvars_menu_state;

/*---------------------------------------------------------------------------
 * Screen show/hide API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Yvars_ShowScreen(void);
void Yvars_HideScreen(void);

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Initialises Y-VARS menu state and shows the screen. Called under lvgl_lock(). */
void       Yvars_MenuOpen(CalcMode_t return_to);

/** Resets Y-VARS menu state and returns the saved return mode. Called from menu_close(). */
CalcMode_t Yvars_MenuClose(void);

/** Creates the Y-VARS menu screen (hidden at start). */
void ui_init_yvars_screen(void);

/** Redraws the Y-VARS menu display from current state. */
void ui_update_yvars_display(void);

/** Token handler for MODE_YVARS_MENU.  Returns true to consume token. */
bool handle_yvars_menu(Token_t t);

#endif /* APP_UI_YVARS_H */
