/**
 * @file    ui_vars.h
 * @brief   VARS menu UI — initialization, display-update, and token handler
 *          declarations.
 *
 * Five-tab menu opened by the VARS key:
 *   XY  — statistics summary variables (n, x̄, Sx, σx, ȳ, Sy, σy)
 *   Σ   — summation variables (Σx, Σx², Σy, Σy², Σxy)
 *   LR  — linear regression variables (a, b, r, RegEQ)
 *   DIM — matrix dimension variables (Arow, Acol, Brow, Bcol, Crow, Ccol)
 *   RNG — window range variables (Xmin..Tstep, 10 items with scroll)
 *
 * Selecting an item inserts its current numeric value into the expression
 * buffer (or the Y= editor if opened from there).
 *
 * Navigation state uses the shared MenuState_t from menu_state.h (Item 3
 * proof-of-concept; see INTERFACE_REFACTOR_PLAN.md).
 */

#ifndef APP_UI_VARS_H
#define APP_UI_VARS_H

#include "app_common.h"
#include "menu_state.h"
#include "lvgl.h"

/*---------------------------------------------------------------------------
 * Externally visible state
 *---------------------------------------------------------------------------*/

extern MenuState_t vars_menu_state;

/*---------------------------------------------------------------------------
 * Screen show/hide API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Vars_ShowScreen(void);
void Vars_HideScreen(void);

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates the VARS menu screen (hidden at start). */
void ui_init_vars_screen(void);

/** Redraws the VARS menu display from current state. */
void ui_update_vars_display(void);

/** Token handler for MODE_VARS_MENU.  Returns true to consume token. */
bool handle_vars_menu(Token_t t);

#endif /* APP_UI_VARS_H */
