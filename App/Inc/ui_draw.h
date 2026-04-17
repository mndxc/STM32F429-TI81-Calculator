/**
 * @file    ui_draw.h
 * @brief   DRAW menu UI — initialization, display-update, and token handler
 *          declarations.
 *
 * The DRAW menu is a single-list (no tabs) 7-item menu opened by 2nd+PRGM.
 * Items 2–7 insert their function token string into the expression buffer;
 * item 1 (ClrDraw) executes immediately.
 */

#ifndef APP_UI_DRAW_H
#define APP_UI_DRAW_H

#include "app_common.h"
#include "lvgl.h"

/*---------------------------------------------------------------------------
 * Menu navigation state
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    item_cursor;  /* Highlighted row (0-based) */
    CalcMode_t return_mode;  /* Mode to restore when the menu is closed */
} DrawMenuState_t;

/*---------------------------------------------------------------------------
 * Externally visible state
 *---------------------------------------------------------------------------*/

extern DrawMenuState_t draw_menu_state;

/*---------------------------------------------------------------------------
 * Screen show/hide API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Draw_ShowScreen(void);
void Draw_HideScreen(void);

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Initialises DRAW menu state and shows the screen. Called under lvgl_lock(). */
void       Draw_MenuOpen(CalcMode_t return_to);

/** Resets DRAW menu state and returns the saved return mode. Called from menu_close(). */
CalcMode_t Draw_MenuClose(void);

/** Creates the DRAW menu screen (hidden at start). */
void ui_init_draw_screen(void);

/** Redraws the DRAW menu display from current state. */
void ui_update_draw_display(void);

/** Token handler for MODE_DRAW_MENU.  Returns true to consume token. */
bool handle_draw_menu(Token_t t);

/**
 * @brief Try to execute a DRAW command from the current expression buffer.
 *
 * Called from history_enter_evaluate() in calculator_core.c on TOKEN_ENTER.
 * Returns true if the expression was a recognised DRAW command (Line(, PT-On(,
 * PT-Off(, PT-Chg(, DrawF, Shade(, ClrDraw); the caller is responsible for
 * showing "Done" and clearing the expression buffer.
 * Returns false if not a DRAW command (caller falls through to Calc_Evaluate).
 */
bool try_execute_draw_command(void);

#endif /* APP_UI_DRAW_H */
