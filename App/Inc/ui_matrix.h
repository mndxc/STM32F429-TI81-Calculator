/**
 * @file    ui_matrix.h
 * @brief   Matrix Editor and Menu UI initialization and logic.
 */

#ifndef APP_UI_MATRIX_H
#define APP_UI_MATRIX_H

#include "app_common.h"

/* UI Initialization */
void ui_init_matrix_screen(void);

/* UI Display Updates */
void ui_update_matrix_display(void);
void ui_update_matrix_edit_display(void);

/* Token Handlers */
bool handle_matrix_menu(Token_t t);
void handle_matrix_edit(Token_t t);

#endif /* APP_UI_MATRIX_H */
