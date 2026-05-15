/**
 * @file    ui_prgm_io.h
 * @brief   PRGM I/O sub-menu — state, LVGL init, display-update, and token handler.
 *
 * Extracted from ui_prgm.c. Part of the calculator UI super-module.
 *
 * Consumers: ui_prgm.c (init, submenu tab-switch), calculator_core.c (dispatcher).
 */

#ifndef UI_PRGM_IO_H
#define UI_PRGM_IO_H

#include "app_common.h"

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates all LVGL objects for the I/O sub-menu screen (hidden at start). */
void ui_init_prgm_io_screen(lv_obj_t *parent);

/**
 * @brief Unhide the I/O screen, reset navigation state, and refresh the display.
 *        Called by prgm_submenu_tab_switch(), already under lvgl_lock().
 */
void ui_prgm_io_reset_and_show(void);

/** Hide the I/O screen.  NULL-safe. */
void ui_prgm_io_hide(void);

/** Redraws I/O sub-menu labels with current cursor highlight.
 *  Must be called under lvgl_lock(). */
void ui_update_prgm_io_display(void);

/** Token handler for MODE_PRGM_IO_MENU. Returns true to consume the token. */
bool handle_prgm_io_menu(Token_t t);

#endif /* UI_PRGM_IO_H */
