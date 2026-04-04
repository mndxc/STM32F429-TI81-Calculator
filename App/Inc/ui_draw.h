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
 * Externally visible objects / state
 *---------------------------------------------------------------------------*/

extern DrawMenuState_t draw_menu_state;
extern lv_obj_t *ui_draw_screen;

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates the DRAW menu screen (hidden at start). */
void ui_init_draw_screen(void);

/** Redraws the DRAW menu display from current state. */
void ui_update_draw_display(void);

/** Token handler for MODE_DRAW_MENU.  Returns true to consume token. */
bool handle_draw_menu(Token_t t);

#endif /* APP_UI_DRAW_H */
