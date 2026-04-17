/**
 * @file    ui_matrix.h
 * @brief   Matrix Editor and Menu UI initialization and logic.
 */

#ifndef APP_UI_MATRIX_H
#define APP_UI_MATRIX_H

#include "app_common.h"
#include "menu_state.h"

extern MenuState_t matrix_menu_state;

/* Screen show/hide/visibility API (caller holds lvgl_lock) */
void Matrix_ShowMenuScreen(void);
void Matrix_HideMenuScreen(void);
void Matrix_ShowEditScreen(void);
void Matrix_HideEditScreen(void);
/** Returns true if the matrix edit screen is not hidden (NULL-safe). */
bool Matrix_IsEditScreenVisible(void);

/* UI Initialization */
void ui_init_matrix_screen(void);

/* UI Display Updates */
void ui_update_matrix_display(void);
void ui_update_matrix_edit_display(void);
void matrix_edit_cursor_update(void);

/* Open / close helpers (called from menu_open / menu_close in calculator_core.c) */
void       Matrix_MenuOpen(CalcMode_t return_to);
CalcMode_t Matrix_MenuClose(void);

/* Token Handlers */
bool handle_matrix_menu(Token_t t, MenuState_t *s);
void handle_matrix_edit(Token_t t);

#endif /* APP_UI_MATRIX_H */
