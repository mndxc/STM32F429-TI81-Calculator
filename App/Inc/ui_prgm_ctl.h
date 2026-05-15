/**
 * @file    ui_prgm_ctl.h
 * @brief   PRGM CTL sub-menu — state, LVGL init, display-update, and token handler.
 *
 * Extracted from ui_prgm.c. Part of the calculator UI super-module.
 *
 * Consumers: ui_prgm.c (init, submenu tab-switch), calculator_core.c (dispatcher).
 */

#ifndef UI_PRGM_CTL_H
#define UI_PRGM_CTL_H

#include "app_common.h"

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates all LVGL objects for the CTL sub-menu screen (hidden at start). */
void ui_init_prgm_ctl_screen(lv_obj_t *parent);

/**
 * @brief Unhide the CTL screen, reset navigation state, and refresh the display.
 *        Called by prgm_submenu_tab_switch() and the TOKEN_PRGM handler in
 *        handle_prgm_editor(), already under lvgl_lock().
 */
void ui_prgm_ctl_reset_and_show(void);

/** Hide the CTL screen.  NULL-safe. */
void ui_prgm_ctl_hide(void);

/** Redraws CTL sub-menu labels with current cursor highlight.
 *  Must be called under lvgl_lock(). */
void ui_update_prgm_ctl_display(void);

/** Token handler for MODE_PRGM_CTL_MENU. Returns true to consume the token. */
bool handle_prgm_ctl_menu(Token_t t);

#endif /* UI_PRGM_CTL_H */
