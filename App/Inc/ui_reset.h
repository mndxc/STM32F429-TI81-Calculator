/**
 * @file    ui_reset.h
 * @brief   RESET confirmation screen (2nd + +).
 *
 * Displays memory usage and a two-item confirmation menu:
 *   1:No    — return to the screen where the user was
 *   2:Reset — factory-reset all state and display "Mem cleared"
 *
 * Spec: TI-81 guidebook p. 1-28.
 */

#ifndef UI_RESET_H
#define UI_RESET_H

#include "lvgl.h"
#include "app_common.h"
#include "keypad_map.h"

void Reset_ShowScreen(void);
void Reset_HideScreen(void);
bool Reset_IsVisible(void);

void ui_init_reset_screen(void);

/** Opens the RESET confirmation screen, saving @p return_to as the cancel target. */
void Reset_MenuOpen(CalcMode_t return_to);

/** Token handler for MODE_RESET_CONFIRM.  Returns true to consume the token. */
bool handle_reset_confirm(Token_t t);

#endif /* UI_RESET_H */
