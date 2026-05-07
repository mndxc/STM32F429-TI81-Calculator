/**
 * @file    ui_error.h
 * @brief   TI-81 error overlay screen.
 *
 * Screen layout (guidebook pp. 1-26, B-4):
 *   ERROR nn type           (e.g. "ERROR 06 SYNTAX")
 *   1:Goto Error
 *   2:Quit
 *
 * Pressing 1 restores the faulting expression in the editor with the cursor
 * at the byte offset of the fault.  Pressing 2 or CLEAR dismisses the screen
 * and returns to a clean home screen.
 *
 * Spec: TI-81 guidebook pp. 1-26 to 1-27.
 */

#ifndef UI_ERROR_H
#define UI_ERROR_H

#include "lvgl.h"
#include "app_common.h"
#include "keypad_map.h"
#include "calc_engine.h"

extern lv_obj_t *ui_error_screen;

void Error_ShowScreen(void);
void Error_HideScreen(void);

void ui_init_error_screen(void);

/**
 * @brief Opens the error overlay for a fault that originated from an expression.
 *
 * @param err     Error code returned by Calc_Evaluate().
 * @param expr    The expression string that produced the error.
 * @param offset  Byte offset within @p expr where the fault was detected (0 if unknown).
 * @param can_goto  True if "1:Goto Error" should be offered.  Pass false for
 *                  errors where re-entering the editor makes no sense (RANGE/ZOOM).
 */
void Error_Open(CalcError_t err, const char *expr, uint16_t offset, bool can_goto);

/** Token handler for MODE_ERROR_SCREEN.  Returns true to consume the token. */
bool handle_error_screen(Token_t t);

#endif /* UI_ERROR_H */
