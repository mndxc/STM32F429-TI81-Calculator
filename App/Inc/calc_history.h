/**
 * @file    calc_history.h
 * @brief   History ring buffer: expressions, results, recall, and matrix scroll state.
 *
 * Ownership: all history state is private to calc_history.c.
 *
 * CalcHistory_UpdateDisplay() is declared here but defined in calculator_core.c
 * because it must call ui_refresh_display(), which accesses private LVGL
 * display-row objects owned by that translation unit.  Callers must hold the
 * LVGL mutex when calling CalcHistory_UpdateDisplay().
 */

#ifndef APP_CALC_HISTORY_H
#define APP_CALC_HISTORY_H

#include "app_common.h"   /* MAX_EXPR_LEN */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>       /* size_t */

/* ---- Constants ----------------------------------------------------------- */
#define HISTORY_LINE_COUNT  1   /* Expression+result pairs stored (TI-81 spec: 1 entry) */
#define MAX_RESULT_LEN     96   /* 32 chars for scalars; up to ~80 for 3x3 matrix rows  */
#define MATRIX_RING_COUNT   1   /* Ring-buffer slots for matrix history results          */

/* ---- Types --------------------------------------------------------------- */
typedef struct {
    char    expression[MAX_EXPR_LEN];
    char    result[MAX_RESULT_LEN];     /* Scalar/error string; newline-separated rows for matrices */
    bool    has_matrix;                 /* True when this entry holds a matrix result */
    uint8_t matrix_ring_idx;            /* Ring slot index; valid iff has_matrix */
    uint8_t matrix_ring_gen;            /* Generation at that slot; mismatch = evicted */
    uint8_t matrix_rows_cache;          /* Cached row count; valid even after eviction */
} HistoryEntry_t;

/* ---- Lifecycle ----------------------------------------------------------- */
/** Zero all state.  Call once at startup or before each host-test group. */
void CalcHistory_Init(void);

/* ---- Commit / clear ------------------------------------------------------ */
/** Store one expression+result pair.  Matrix ring fields are supplied by the
 *  caller (calculator_core.c owns the matrix ring). */
void CalcHistory_Commit(const char *expression, const char *result,
                        bool has_matrix, uint8_t ring_idx,
                        uint8_t ring_gen, uint8_t rows_cache);

/** Reset history_count to 0 (ClrHome behaviour). */
void CalcHistory_Clear(void);

/* ---- Recall navigation --------------------------------------------------- */
/** Increment recall offset toward older entries (bounds-checked internally). */
void CalcHistory_RecallUp(void);
/** Decrement recall offset toward newer entries. */
void CalcHistory_RecallDown(void);

/* ---- Getters ------------------------------------------------------------- */
uint8_t               CalcHistory_GetCount(void);
int8_t                CalcHistory_GetRecallOffset(void);
void                  CalcHistory_ResetRecallOffset(void);
/** Return a read-only pointer to the entry at slot @p idx. */
const HistoryEntry_t *CalcHistory_GetEntry(uint8_t idx);
/** Copy the expression for the current recall offset into @p buf. */
void                  CalcHistory_GetExprForRecall(char *buf, size_t len);

/* ---- Matrix scroll ------------------------------------------------------- */
void    CalcHistory_ResetMatrixScroll(void);
int8_t  CalcHistory_GetMatrixScrollFocus(void);
uint8_t CalcHistory_GetMatrixScrollOffset(void);
void    CalcHistory_SetMatrixScrollOffset(uint8_t offset);

/* ---- Display ------------------------------------------------------------- */
/** Refresh the history display rows.  Defined in calculator_core.c (needs the
 *  private LVGL display-row array).  Caller must hold the LVGL mutex. */
void CalcHistory_UpdateDisplay(void);

#endif /* APP_CALC_HISTORY_H */
