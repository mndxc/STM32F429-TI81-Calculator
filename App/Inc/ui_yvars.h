/**
 * @file    ui_yvars.h
 * @brief   Y-VARS menu UI — initialization, display-update, and token handler
 *          declarations.
 *
 * Three-tab menu opened by 2nd+VARS (TOKEN_Y_VARS):
 *   Y   — inserts Y₁–Y₄ / X₁t–X₃t / Y₁t–Y₃t equation reference strings
 *         into the expression buffer (scrolls when list exceeds 7 rows)
 *   ON  — enables Y₁–Y₄ equations and/or parametric pairs X₁t–X₃t
 *   OFF — disables Y₁–Y₄ equations and/or parametric pairs X₁t–X₃t
 *
 * Font notes (see CLAUDE.md gotcha #14):
 *   ₁₂₃₄ = U+2081–2084 → \xE2\x82\x81 … \xE2\x82\x84
 */

#ifndef APP_UI_YVARS_H
#define APP_UI_YVARS_H

#include "app_common.h"
#include "lvgl.h"
#include "menu_state.h"

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

/**
 * Opens Y-VARS in STO context: saves @p expr_to_store, then when the user
 * selects Y₁–Y₄ the expression is written to that Y= slot instead of being
 * inserted into the expression buffer.  Pass NULL to clear the context.
 */
void       Yvars_OpenForSto(const char *expr_to_store);

/** Resets Y-VARS menu state and returns the saved return mode. Called from menu_close(). */
CalcMode_t Yvars_MenuClose(void);

/** Creates the Y-VARS menu screen (hidden at start). */
void ui_init_yvars_screen(void);

/** Redraws the Y-VARS menu display from current state. */
void ui_update_yvars_display(void);

/** Token handler for MODE_YVARS_MENU.  Returns true to consume token. */
bool handle_yvars_menu(Token_t t);

#endif /* APP_UI_YVARS_H */
