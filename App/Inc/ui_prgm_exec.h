/**
 * @file    ui_prgm_exec.h
 * @brief   PRGM EXEC sub-menu — state, LVGL init, display-update, and token handler.
 *
 * The EXEC sub-menu is the subroutine slot picker that appears when the user
 * presses PRGM→EXEC from inside the program editor.  It is the third tab of
 * the CTL/I/O/EXEC sub-menu set.
 *
 * Extracted alongside CTL and I/O sub-menus (Item 1 of INTERFACE_REFACTOR_PLAN.md).
 * Part of the calculator UI super-module; include calc_internal.h before this
 * header in translation units that need full shared-state context.
 *
 * Consumers: ui_prgm.c (init, submenu tab-switch), calculator_core.c (dispatcher).
 */

#ifndef UI_PRGM_EXEC_H
#define UI_PRGM_EXEC_H

#include "app_common.h"

/*---------------------------------------------------------------------------
 * Externally visible screen object
 *---------------------------------------------------------------------------*/

extern lv_obj_t *ui_prgm_exec_screen;

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates all LVGL objects for the EXEC sub-menu screen (hidden at start). */
void ui_init_prgm_exec_screen(lv_obj_t *parent);

/**
 * @brief Reset EXEC navigation state (cursor, scroll) and refresh the display.
 *        Called by prgm_submenu_tab_switch(), already under lvgl_lock().
 */
void ui_prgm_exec_reset_and_show(void);

/** Redraws EXEC sub-menu labels with current cursor highlight and slot names.
 *  Must be called under lvgl_lock(). */
void ui_update_prgm_exec_display(void);

/** Token handler for MODE_PRGM_EXEC_MENU. Returns true to consume the token. */
bool handle_prgm_exec_menu(Token_t t);

#endif /* UI_PRGM_EXEC_H */
