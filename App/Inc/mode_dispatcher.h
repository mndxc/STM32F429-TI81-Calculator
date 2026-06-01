/**
 * @file    mode_dispatcher.h
 * @brief   Token dispatch and FreeRTOS task entry point.
 *
 * Execute_Token() is the single entry point for all calculator token processing.
 * Process_Hardware_Key() translates raw key IDs to tokens and posts them to the
 * keypad queue.  StartCalcCoreTask() is the FreeRTOS task body.
 *
 * Navigation helpers (hide_all_screens, menu_open/close, menu_insert_text,
 * tab_move) are also here — they are only called from mode handler modules
 * that already depend on this layer.
 */

#ifndef MODE_DISPATCHER_H
#define MODE_DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include "app_common.h"   /* CalcMode_t, Token_t */

/*
 * Primary dispatch entry point — processes one token.
 * Called by StartCalcCoreTask after dequeuing from keypadQueueHandle.
 */
void Execute_Token(Token_t t);

/*
 * Translates a hardware key ID into a token (applying modifier-mode layer
 * selection) and posts it to keypadQueueHandle.
 * Called from keypadTask (HW/Keypad layer).
 */
void Process_Hardware_Key(uint8_t key_id);

/*
 * FreeRTOS calculator core task body.
 * Waits for LVGL init, creates all screens, then loops on Execute_Token.
 */
void StartCalcCoreTask(void const *argument);

/*
 * Hides every graph editor, menu overlay, and the graph canvas.
 * Must be called inside lvgl_lock().
 */
void hide_all_screens(void);

/*
 * Opens a menu (MATH, TEST, MATRIX, PRGM, STAT, DRAW, VARS, Y-VARS).
 * Hides all current screens first.  return_to: mode to restore on close.
 */
void menu_open(Token_t menu_token, CalcMode_t return_to);

/*
 * Closes a menu and restores the calling screen.
 * Returns the restored CalcMode_t.
 */
CalcMode_t menu_close(Token_t menu_token);

/*
 * Inserts a string into the appropriate context:
 * PRGM editor, Y= editor, or normal expression buffer.
 * Updates *ret_mode to MODE_NORMAL on exit.
 */
void menu_insert_text(const char *ins, CalcMode_t *ret_mode);

/*
 * Moves the active tab in a multi-tab menu left or right.
 * Resets item cursor and scroll offset on tab change.
 */
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll,
              uint8_t tab_count, bool left, void (*update)(void));

#ifdef HOST_TEST
/*
 * Validates that every CalcMode_t value is covered by the routing table.
 * Available only in HOST_TEST builds; called from test_mode_topology.c.
 */
bool calc_mode_topology_validate(void);
#endif

#endif /* MODE_DISPATCHER_H */
