/**
 * @file    ui_prgm_io.h
 * @brief   PRGM I/O sub-menu — state, LVGL init, display-update, and token handler.
 *
 * Extracted from ui_prgm.c. Part of the calculator UI super-module; include
 * calc_internal.h before this header in translation units that need full shared
 * state context.  This header is self-contained for callers that only need the
 * public API.
 *
 * Consumers: ui_prgm.c (init, submenu tab-switch), calculator_core.c (dispatcher).
 */

#ifndef UI_PRGM_IO_H
#define UI_PRGM_IO_H

#include "app_common.h"

/*---------------------------------------------------------------------------
 * Externally visible screen object
 * Referenced by hide_prgm_screens() in ui_prgm.c and by prgm_submenu_tab_switch().
 * lv_obj_t must be defined before including this header (provided by lvgl.h
 * or by the HOST_TEST stubs in calculator_core_test_stubs.h).
 *---------------------------------------------------------------------------*/

extern lv_obj_t *ui_prgm_io_screen;

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates all LVGL objects for the I/O sub-menu screen (hidden at start). */
void ui_init_prgm_io_screen(lv_obj_t *parent);

/**
 * @brief Reset I/O navigation state (cursor) and refresh the display.
 *        Called by prgm_submenu_tab_switch(), already under lvgl_lock().
 */
void ui_prgm_io_reset_and_show(void);

/** Redraws I/O sub-menu labels with current cursor highlight.
 *  Must be called under lvgl_lock(). */
void ui_update_prgm_io_display(void);

/** Token handler for MODE_PRGM_IO_MENU. Returns true to consume the token. */
bool handle_prgm_io_menu(Token_t t);

#endif /* UI_PRGM_IO_H */
