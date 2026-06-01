/**
 * @file    ui_main_display.h
 * @brief   New API declarations for functions extracted from calculator_core.c
 *          to ui_main_display.c: matrix ring writes, display accessors, and
 *          the top-level init call.
 *
 * Functions already declared in ui_shared.h (cursor_box_create, cursor_render,
 * screen_create, Update_Calculator_Display, ui_update_status_bar,
 * hide_all_screens, lvgl_lock, lvgl_unlock, etc.) and in calculator_core.h
 * (ui_refresh_display, ui_output_row, format_calc_result) are NOT re-declared
 * here — include ui_shared.h or calculator_core.h as appropriate.
 */

#ifndef UI_MAIN_DISPLAY_H
#define UI_MAIN_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "app_common.h"
#include "calc_engine.h"   /* CalcMatrix_t */
#include "calc_history.h"  /* HistoryEntry_t */

/*
 * Matrix ring write — called by commit_history_entry and Calc_CommitMatrixToHistory
 * in calculator_core.c to store a matrix result into the display ring before
 * passing the ring index/gen to CalcHistory_Commit.
 *
 * Copies calc_matrices[mat_idx] into the next ring slot and advances the write
 * counter.  Fills *ring_idx_out, *ring_gen_out, *rows_cache_out for the caller.
 */
void UiDisplay_PushMatrixToRing(uint8_t mat_idx,
                                uint8_t *ring_idx_out,
                                uint8_t *ring_gen_out,
                                uint8_t *rows_cache_out);

/*
 * Returns the number of monospace characters that fit on one display row.
 * Used by handle_history_nav (calculator_core.c) to bound matrix scroll offsets.
 */
uint8_t UiDisplay_GetExprCharsPerRow(void);

/*
 * Returns the CalcMatrix_t for history entry e, or NULL if evicted from the ring.
 * Called by render_result_row (ui_main_display.c) and handle_history_nav
 * (calculator_core.c) to determine matrix scroll extents.
 */
const CalcMatrix_t *UiDisplay_GetHistoryMatrix(const HistoryEntry_t *e);

/*
 * Perform the full main-screen UI initialisation sequence.
 * Creates display rows, registers the history display callback, and starts
 * the cursor blink timer.
 * Called from StartCalcCoreTask (mode_dispatcher.c) under lvgl_lock().
 */
void UiMainDisplay_Init(void);

#endif /* UI_MAIN_DISPLAY_H */
