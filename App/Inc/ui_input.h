/**
 * @file    ui_input.h
 * @brief   Normal-mode expression input handlers and expression buffer utilities.
 *
 * Extracted from calculator_core.c.  Part of the UI super-module; ui_input.c
 * may include calc_internal.h freely.
 *
 * Public API exposed here:
 *   expr_delete_at_cursor — backspace helper called by ui_prgm.c as well
 *   handle_normal_mode    — normal-mode token dispatcher (called from Execute_Token)
 *   handle_sto_pending    — STO intercept layer (called from Execute_Token)
 */

#ifndef UI_INPUT_H
#define UI_INPUT_H

#include "app_common.h"
#include <stdbool.h>

/**
 * @brief Inserts a string at cursor_pos and advances the cursor by its length.
 *        Called from calculator_core.c's menu insert helpers as well.
 */
void expr_insert_str(const char *s);

/**
 * @brief Deletes the character immediately before cursor_pos (backspace).
 *        UTF-8 and matrix-token aware; no-op when cursor is at 0.
 *        Called from ui_prgm.c's running-program key handler as well.
 */
void expr_delete_at_cursor(void);

/**
 * @brief Dispatch all token types that apply in normal calculator mode.
 *        Routes digit/op/function keys, navigation, CLEAR, STO, graph nav,
 *        and menu-open tokens.
 */
void handle_normal_mode(Token_t t);

/**
 * @brief Handle a keypress when STO is pending (next alpha key stores ans).
 * @return true if the token was consumed; false to fall through to normal mode.
 */
bool handle_sto_pending(Token_t t);

#endif /* UI_INPUT_H */
