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
 */

#ifndef APP_UI_VARS_H
#define APP_UI_VARS_H

#include "app_common.h"
#include "lvgl.h"

/*---------------------------------------------------------------------------
 * Menu navigation state
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    tab;           /* 0=XY 1=Σ 2=LR 3=DIM 4=RNG */
    uint8_t    item_cursor;   /* Visible-row highlight (relative to scroll_offset) */
    uint8_t    scroll_offset; /* Index of first visible item (non-zero only for RNG tab) */
    CalcMode_t return_mode;   /* Mode to restore on CLEAR */
} VarsMenuState_t;

/*---------------------------------------------------------------------------
 * Externally visible objects / state
 *---------------------------------------------------------------------------*/

extern VarsMenuState_t vars_menu_state;
extern lv_obj_t       *ui_vars_screen;

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
