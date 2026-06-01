/**
 * @file  calculator_core.c
 * @brief Pure calculator state: mode, ANS, angle, insert-mode, and history.
 *
 * Remaining responsibilities after extraction:
 *   1. Pure state variables (current_mode, return_mode, ans, ans_is_matrix,
 *      angle_degrees, insert_mode) and their getter/setter API.
 *   2. LVGL mutex helpers (lvgl_lock / lvgl_unlock).
 *   3. History evaluation and navigation (commit_history_entry,
 *      history_enter_evaluate, handle_history_nav, Calc_CommitMatrixToHistory).
 *
 * Extracted responsibilities:
 *   - LVGL UI object creation and display rendering → ui_main_display.c
 *   - Token dispatch table and FreeRTOS task → mode_dispatcher.c
 *   - Expression buffer state → expr_editor.c
 *   - History ring-buffer state → calc_history.c
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "calc_history.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "expr_util.h"
#  include "ui_palette.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "calculator_core_test_stubs.h"
#  include "expr_editor.h"
#  include "calculator_core.h"
#  include "calc_mode_topology.h"
#else
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "graph.h"
#  include "graph_draw.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "ui_shared.h"
#  include "calc_history.h"
#  include "calculator_core.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "ui_math_menu.h"
#  include "ui_matrix.h"
#  include "ui_prgm.h"
#  include "prgm_editor.h"
#  include "ui_prgm_ctl.h"
#  include "ui_prgm_io.h"
#  include "ui_prgm_exec.h"
#  include "ui_prgm_mode.h"
#  include "ui_stat.h"
#  include "ui_draw.h"
#  include "ui_vars.h"
#  include "ui_yvars.h"
#  include "ui_reset.h"
#  include "ui_error.h"
#  include "graph_ui.h"
#  include "graph_ui_range.h"
#  include "ui_graph_zoom.h"
#  include "ui_palette.h"
#  include "expr_util.h"
#  include "expr_editor.h"
#  include "cmsis_os.h"
#  include "lvgl.h"
#  include "main.h"
#  include "calc_mode_topology.h"
#endif
#include "ui_main_display.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Private variables
 *---------------------------------------------------------------------------*/

/* expr / sto_pending / cursor_visible moved to expr_editor.c */
#ifdef HOST_TEST
bool insert_mode = false;
#else
static bool insert_mode = false;
#endif

#ifdef HOST_TEST
CalcMode_t   current_mode = MODE_NORMAL;
CalcMode_t   return_mode  = MODE_NORMAL;
#else
static CalcMode_t   current_mode = MODE_NORMAL;
static CalcMode_t   return_mode  = MODE_NORMAL;
#endif
bool         angle_degrees = true;

#ifdef HOST_TEST
float ans           = 0.0f;
bool  ans_is_matrix = false;
#else
static float ans           = 0.0f;
static bool  ans_is_matrix = false;
#endif

/*---------------------------------------------------------------------------
 * ANS getter/setter API (declared in calculator_core.h)
 *---------------------------------------------------------------------------*/

void  Calc_SetAnsScalar(float value)  { ans = value; ans_is_matrix = false; }
void  Calc_SetAnsMatrix(float idx)    { ans = idx;   ans_is_matrix = true;  }
float Calc_GetAns(void)               { return ans; }
bool  Calc_GetAnsIsMatrix(void)       { return ans_is_matrix; }

/*---------------------------------------------------------------------------
 * Mode getter/setter API (declared in calculator_core.h)
 *---------------------------------------------------------------------------*/

void Calc_SetMode(CalcMode_t mode)
{
#ifndef NDEBUG
    assert(CalcMode_IsValidTransition(mode));
#endif
    current_mode = mode;
}
void        Calc_SetReturnMode(CalcMode_t mode) { return_mode  = mode; }
CalcMode_t  Calc_GetMode(void)                  { return current_mode; }
CalcMode_t  Calc_GetReturnMode(void)            { return return_mode; }
bool        Calc_GetAngleDegrees(void)          { return angle_degrees; }
void        Calc_SetAngleDegrees(bool degrees)  { angle_degrees = degrees; }
const char *Calc_GetExprBuf(void)      { return ExprEditor_GetBuf(); }
void        Calc_ResetInputState(void) { ExprEditor_Reset(); }
void        Calc_ClearExpr(void)       { ExprEditor_Clear(); }

/*---------------------------------------------------------------------------
 * Expression editor state getter/setter API (declared in calculator_core.h)
 *---------------------------------------------------------------------------*/

bool          Calc_GetInsertMode(void)      { return insert_mode; }
void          Calc_SetInsertMode(bool v)    { insert_mode = v; }
bool          Calc_GetCursorVisible(void)   { return ExprEditor_GetCursorVisible(); }
void          Calc_SetCursorVisible(bool v) { ExprEditor_SetCursorVisible(v); }

/*---------------------------------------------------------------------------
 * LVGL thread safety helpers
 *---------------------------------------------------------------------------*/

void lvgl_lock(void) {
#ifndef HOST_TEST
    if (xLVGL_Mutex != NULL)
        xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY);
#endif
}

void lvgl_unlock(void) {
#ifndef HOST_TEST
    if (xLVGL_Mutex != NULL)
        xSemaphoreGive(xLVGL_Mutex);
#endif
}

/*---------------------------------------------------------------------------
 * Forward declarations for helpers defined later in this file
 *---------------------------------------------------------------------------*/

static void history_load_offset(uint8_t offset);
static void history_enter_evaluate(void);
void        handle_history_nav(Token_t t);  /* non-static: called from ui_input.c */

/*---------------------------------------------------------------------------
 * History commit helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief Write a completed evaluation into the next history slot and refresh the display.
 *
 * Calls UiDisplay_PushMatrixToRing when the result contains a matrix, then
 * commits to the history ring buffer and triggers a UI refresh.
 */
static void commit_history_entry(const char *expr_buf, const char *result_str,
                                 const CalcResult_t *r)
{
    uint8_t ring_idx = 0, ring_gen = 0, rows_cache = 0;
    if (r->has_matrix) {
        UiDisplay_PushMatrixToRing((uint8_t)r->matrix_idx,
                                   &ring_idx, &ring_gen, &rows_cache);
    }
    CalcHistory_Commit(expr_buf, result_str, r->has_matrix, ring_idx, ring_gen, rows_cache);
    lvgl_lock();
    CalcHistory_UpdateDisplay();
    lvgl_unlock();
}

void Calc_CommitMatrixToHistory(const char *expr_text, uint8_t mat_idx)
{
    static const char * const mat_names[4] = {"[A]", "[B]", "[C]", "[ANS]"};
    uint8_t ring_idx, ring_gen, rows_cache;
    UiDisplay_PushMatrixToRing(mat_idx, &ring_idx, &ring_gen, &rows_cache);
    CalcHistory_Commit(expr_text, mat_idx < 4 ? mat_names[mat_idx] : "?",
                       true, ring_idx, ring_gen, rows_cache);
    lvgl_lock();
    CalcHistory_UpdateDisplay();
    lvgl_unlock();
}

/* Load a history entry at the given scroll offset into the expression buffer. */
static void history_load_offset(uint8_t offset)
{
    uint8_t idx = (uint8_t)((CalcHistory_GetCount() - offset) % HISTORY_LINE_COUNT);
    const HistoryEntry_t *e = CalcHistory_GetEntry(idx);
    ExprEditor_LoadStr(e->expression);
    Update_Calculator_Display();
}

/* Evaluate (or run) the current expression on TOKEN_ENTER.
 * Called only when expr_len > 0. */
static void history_enter_evaluate(void)
{
    const char *ebuf = ExprEditor_GetBuf();
    if (strncmp(ebuf, "prgm", 4) == 0) {
        int8_t slot = Prgm_LookupSlot(ebuf + 4);
        CalcHistory_Commit(ebuf, "", false, 0, 0, 0);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        Update_Calculator_Display();
        if (slot >= 0)
            Prgm_RunStart((uint8_t)slot);
        return;
    }
#ifndef HOST_TEST
    if (try_execute_draw_command()) {
        CalcHistory_Commit(ebuf, "Done", false, 0, 0, 0);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        lvgl_unlock();
        Update_Calculator_Display();
        return;
    }
#endif /* HOST_TEST */
    CalcResult_t result = Calc_Evaluate(ebuf, ans, ans_is_matrix, angle_degrees);
    if (result.error != CALC_OK) {
#ifndef HOST_TEST
        Error_Open(result.error, ebuf, result.error_offset, true);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
#else
        char result_str[MAX_RESULT_LEN];
        format_calc_result(&result, result_str, MAX_RESULT_LEN);
        commit_history_entry(ebuf, result_str, &result);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
#endif
        return;
    }
    char result_str[MAX_RESULT_LEN];
    format_calc_result(&result, result_str, MAX_RESULT_LEN);
    commit_history_entry(ebuf, result_str, &result);
    ExprEditor_Clear();
    CalcHistory_ResetRecallOffset();
}

void handle_history_nav(Token_t t)
{
    switch (t) {

    case TOKEN_LEFT:
        {
            int8_t focus = CalcHistory_GetMatrixScrollFocus();
            if (ExprEditor_GetLen() == 0 && focus >= 0 &&
                UiDisplay_GetHistoryMatrix(CalcHistory_GetEntry((uint8_t)focus)) != NULL) {
                uint8_t off = CalcHistory_GetMatrixScrollOffset();
                if (off > 0) {
                    CalcHistory_SetMatrixScrollOffset(off - 1);
                    Update_Calculator_Display();
                }
            } else if (ExprEditor_GetCursor() > 0) {
                ExprEditor_Left();
                Update_Calculator_Display();
            }
        }
        break;

    case TOKEN_RIGHT:
        {
            int8_t focus = CalcHistory_GetMatrixScrollFocus();
            if (ExprEditor_GetLen() == 0 && focus >= 0) {
                const CalcMatrix_t *m =
                    UiDisplay_GetHistoryMatrix(CalcHistory_GetEntry((uint8_t)focus));
                if (m != NULL) {
                    /* matrix_row_total_width computation: [ + cols*width + separators + ] */
                    /* Re-derive total width inline using the same formula as ui_main_display.c */
                    uint8_t widths[CALC_MATRIX_MAX_DIM] = {0};
                    for (int r = 0; r < (int)m->rows; r++) {
                        for (int c = 0; c < (int)m->cols; c++) {
                            char cell[12];
                            Calc_FormatResult(m->data[r][c], cell, sizeof(cell));
                            cell[8] = '\0';
                            uint8_t len = (uint8_t)strlen(cell);
                            if (len > widths[c]) widths[c] = len;
                        }
                    }
                    int total_w = 2;
                    for (int c = 0; c < (int)m->cols; c++) {
                        if (c > 0) total_w++;
                        total_w += widths[c];
                    }
                    int cpr = (int)UiDisplay_GetExprCharsPerRow();
                    int max_off = (total_w > cpr) ? (total_w - cpr) : 0;
                    uint8_t off = CalcHistory_GetMatrixScrollOffset();
                    if ((int)off < max_off) {
                        CalcHistory_SetMatrixScrollOffset(off + 1);
                        Update_Calculator_Display();
                    }
                }
            } else if (ExprEditor_GetCursor() < ExprEditor_GetLen()) {
                ExprEditor_Right();
                Update_Calculator_Display();
            }
        }
        break;

    case TOKEN_UP:
        {
            int8_t recall = CalcHistory_GetRecallOffset();
            if ((ExprEditor_GetLen() == 0 || recall > 0) &&
                recall < (int8_t)CalcHistory_GetCount() &&
                recall < (int8_t)HISTORY_LINE_COUNT) {
                CalcHistory_RecallUp();
                history_load_offset((uint8_t)CalcHistory_GetRecallOffset());
            }
        }
        break;

    case TOKEN_DOWN:
        {
            int8_t recall = CalcHistory_GetRecallOffset();
            if (recall > 0) {
                CalcHistory_RecallDown();
                recall = CalcHistory_GetRecallOffset();
                if (recall == 0) {
                    ExprEditor_Clear();
                    Update_Calculator_Display();
                } else {
                    history_load_offset((uint8_t)recall);
                }
            }
        }
        break;

    case TOKEN_ENTER:
        if (ExprEditor_GetLen() == 0 && CalcHistory_GetCount() > 0) {
            uint8_t last_idx = (CalcHistory_GetCount() - 1u) % HISTORY_LINE_COUNT;
            const HistoryEntry_t *last = CalcHistory_GetEntry(last_idx);
            CalcResult_t result = Calc_Evaluate(last->expression,
                                                ans, ans_is_matrix, angle_degrees);
            char result_str[MAX_RESULT_LEN];
            format_calc_result(&result, result_str, MAX_RESULT_LEN);
            commit_history_entry(last->expression, result_str, &result);
            CalcHistory_ResetRecallOffset();
        } else if (ExprEditor_GetLen() > 0) {
            history_enter_evaluate();
        }
        break;

    case TOKEN_ENTRY:
        if (CalcHistory_GetCount() > 0) {
            CalcHistory_ResetRecallOffset();
            CalcHistory_RecallUp();
            history_load_offset(1);
        }
        break;

    default: break;
    }
}
