/**
 * @file    ui_cursor.h
 * @brief   UICursor_t — shared blink-timer cursor lifecycle for UI modules.
 *
 * Wraps the lv_timer_t blink-timer pattern used by graph_ui_cursor.c and
 * the visible-state tracking used by ui_stat_edit.c into a single struct with
 * a minimal API.
 *
 * LVGL lock rules:
 *   UICursor_Start()  — must be called under lvgl_lock().
 *   UICursor_Stop()   — must be called under lvgl_lock().
 *   UICursor_Reset()  — must be called under lvgl_lock().
 *   UICursor_Render() — must NOT acquire lvgl_lock() itself; caller is
 *                       responsible.  Safe to call from a timer callback that
 *                       already holds the LVGL context.
 */

#ifndef UI_CURSOR_H
#define UI_CURSOR_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    lv_timer_t *timer;
    bool        visible;
} UICursor_t;

/**
 * @brief Initialise a UICursor_t to the stopped/hidden state.
 *        Does not allocate any LVGL resources.
 */
void UICursor_Init(UICursor_t *c);

/**
 * @brief Create the blink timer and mark the cursor visible.
 *
 * @param c         Cursor instance.
 * @param cb        Timer callback (lv_timer_cb_t); typically toggles visibility
 *                  and redraws the cursor.  Called under the LVGL context —
 *                  must NOT acquire lvgl_lock().
 * @param period_ms Blink half-period in milliseconds.
 *
 * Must be called under lvgl_lock().
 */
void UICursor_Start(UICursor_t *c, lv_timer_cb_t cb, uint32_t period_ms);

/**
 * @brief Delete the blink timer and mark the cursor hidden.
 *        Must be called under lvgl_lock().
 */
void UICursor_Stop(UICursor_t *c);

/**
 * @brief Reset the blink timer without stopping it (defers the next tick).
 *        No-op if the timer is NULL.
 *        Must be called under lvgl_lock().
 */
void UICursor_Reset(UICursor_t *c);

#endif /* UI_CURSOR_H */
