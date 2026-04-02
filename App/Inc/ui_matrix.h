/**
 * @file    ui_matrix.h
 * @brief   Matrix Editor and Menu UI initialization and logic.
 */

#ifndef APP_UI_MATRIX_H
#define APP_UI_MATRIX_H

#include "app_common.h"

typedef struct {
    uint8_t    tab;             /* 0=MATRX, 1=EDIT */
    uint8_t    item_cursor;
    CalcMode_t return_mode;     /* Mode to restore after selection */
} MatrixMenuState_t;

extern MatrixMenuState_t matrix_menu_state;

/* UI Initialization */
void ui_init_matrix_screen(void);

/* UI Display Updates */
void ui_update_matrix_display(void);
void ui_update_matrix_edit_display(void);
void matrix_edit_cursor_update(void);

/* Token Handlers */
bool handle_matrix_menu(Token_t t, MatrixMenuState_t *s);
void handle_matrix_edit(Token_t t);

#endif /* APP_UI_MATRIX_H */
