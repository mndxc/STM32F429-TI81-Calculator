/**
 * @file    ui_sto.h
 * @brief   STO→variable and STO→matrix state machine.
 *
 * Extracted from ui_input.c. Part of the UI super-module.
 *
 * Public API:
 *   handle_sto_pending — intercepts the next token after STO key; evaluates
 *                        the current expression and stores the result into the
 *                        chosen destination (variable, matrix, or Y= slot).
 */

#ifndef UI_STO_H
#define UI_STO_H

#include "app_common.h"
#include <stdbool.h>

/**
 * @brief Handle a keypress when STO is pending (next alpha key stores ans).
 * @return true if the token was consumed; false to fall through to normal mode.
 */
bool handle_sto_pending(Token_t t);

#endif /* UI_STO_H */
