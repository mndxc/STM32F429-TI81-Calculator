/**
 * @file    calc_history.c
 * @brief   History ring buffer implementation.
 *
 * Owns: s_history[], s_history_count, s_history_recall_offset,
 *       s_matrix_scroll_focus, s_matrix_scroll_offset.
 *
 * CalcHistory_UpdateDisplay() is intentionally NOT defined here.  It is
 * defined in calculator_core.c because it must call ui_refresh_display(),
 * which accesses private LVGL display-row objects owned by that TU.
 */

#include "calc_history.h"
#include <string.h>

/* ---- Private state ------------------------------------------------------- */

static HistoryEntry_t s_history[HISTORY_LINE_COUNT];
static uint8_t        s_history_count         = 0;
static int8_t         s_history_recall_offset = 0;
static int8_t         s_matrix_scroll_focus   = -1;  /* slot with scroll focus; -1=none */
static uint8_t        s_matrix_scroll_offset  = 0;   /* horizontal char scroll offset */

/* ---- Lifecycle ----------------------------------------------------------- */

void CalcHistory_Init(void)
{
    memset(s_history, 0, sizeof(s_history));
    s_history_count         = 0;
    s_history_recall_offset = 0;
    s_matrix_scroll_focus   = -1;
    s_matrix_scroll_offset  = 0;
}

/* ---- Commit / clear ------------------------------------------------------ */

void CalcHistory_Commit(const char *expression, const char *result,
                        bool has_matrix, uint8_t ring_idx,
                        uint8_t ring_gen, uint8_t rows_cache)
{
    uint8_t idx = s_history_count % HISTORY_LINE_COUNT;
    /* Skip strncpy when source and dest are the same pointer (re-eval case). */
    if (s_history[idx].expression != expression) {
        strncpy(s_history[idx].expression, expression, MAX_EXPR_LEN - 1);
        s_history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
    }
    strncpy(s_history[idx].result, result, MAX_RESULT_LEN - 1);
    s_history[idx].result[MAX_RESULT_LEN - 1] = '\0';
    s_history[idx].has_matrix        = has_matrix;
    s_history[idx].matrix_ring_idx   = ring_idx;
    s_history[idx].matrix_ring_gen   = ring_gen;
    s_history[idx].matrix_rows_cache = rows_cache;
    if (has_matrix) {
        s_matrix_scroll_focus  = (int8_t)idx;
        s_matrix_scroll_offset = 0;
    } else {
        s_matrix_scroll_focus  = -1;
        s_matrix_scroll_offset = 0;
    }
    s_history_count++;
}

void CalcHistory_Clear(void)
{
    s_history_count         = 0;
    s_history_recall_offset = 0;
}

/* ---- Recall navigation --------------------------------------------------- */

void CalcHistory_RecallUp(void)
{
    if (s_history_recall_offset < (int8_t)s_history_count &&
        s_history_recall_offset < (int8_t)HISTORY_LINE_COUNT)
        s_history_recall_offset++;
}

void CalcHistory_RecallDown(void)
{
    if (s_history_recall_offset > 0)
        s_history_recall_offset--;
}

/* ---- Getters ------------------------------------------------------------- */

uint8_t CalcHistory_GetCount(void)
{
    return s_history_count;
}

int8_t CalcHistory_GetRecallOffset(void)
{
    return s_history_recall_offset;
}

void CalcHistory_ResetRecallOffset(void)
{
    s_history_recall_offset = 0;
}

const HistoryEntry_t *CalcHistory_GetEntry(uint8_t idx)
{
    return &s_history[idx % HISTORY_LINE_COUNT];
}

void CalcHistory_GetExprForRecall(char *buf, size_t len)
{
    uint8_t idx = (uint8_t)((s_history_count - (uint8_t)s_history_recall_offset)
                             % HISTORY_LINE_COUNT);
    strncpy(buf, s_history[idx].expression, len - 1);
    buf[len - 1] = '\0';
}

/* ---- Matrix scroll ------------------------------------------------------- */

void CalcHistory_ResetMatrixScroll(void)
{
    s_matrix_scroll_focus  = -1;
    s_matrix_scroll_offset = 0;
}

int8_t CalcHistory_GetMatrixScrollFocus(void)
{
    return s_matrix_scroll_focus;
}

uint8_t CalcHistory_GetMatrixScrollOffset(void)
{
    return s_matrix_scroll_offset;
}

void CalcHistory_SetMatrixScrollOffset(uint8_t offset)
{
    s_matrix_scroll_offset = offset;
}
