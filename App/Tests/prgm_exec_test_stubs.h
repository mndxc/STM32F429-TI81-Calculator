/**
 * @file    prgm_exec_test_stubs.h
 * @brief   Host-test stubs replacing ui_prgm.h, calc_internal.h, and graph.h
 *          when prgm_exec.c is compiled with -DHOST_TEST=1.
 *
 * Provides:
 *   - Minimal CalcMode_t enum (values must match app_common.h exactly)
 *   - HistoryEntry_t struct matching calc_internal.h
 *   - Constants from calc_internal.h
 *   - Extern declarations for all shared state (defined in test_prgm_exec.c)
 *   - Inline implementations of prgm_parse_from_store, prgm_slot_is_used,
 *     prgm_slot_id_str, and format_calc_result
 *
 * All LVGL / Graph / UI calls in prgm_exec.c are inside #ifndef HOST_TEST
 * guards and are never compiled in, so no LVGL stubs are needed here.
 */

#ifndef PRGM_EXEC_TEST_STUBS_H
#define PRGM_EXEC_TEST_STUBS_H

#include "calc_engine.h"   /* CalcResult_t, Calc_Evaluate, Calc_FormatResult,
                               calc_variables[] — host-safe, no HAL deps */
#include "prgm_exec.h"     /* ProgramStore_t, PRGM_* limits — included before
                               this header so types are already defined */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * CalcMode_t — minimal subset; values match app_common.h exactly.
 *---------------------------------------------------------------------------*/
typedef enum {
    MODE_NORMAL       = 0,
    MODE_PRGM_RUNNING = 19,
} CalcMode_t;

/*---------------------------------------------------------------------------
 * History and display constants (from calc_internal.h / app_common.h)
 *---------------------------------------------------------------------------*/
#define HISTORY_LINE_COUNT   1
#define MAX_EXPR_LEN        96  /* Must match app_common.h */
#define MAX_RESULT_LEN      96
#define DISP_ROW_COUNT       8
#define MENU_VISIBLE_ROWS    7
#define MATRIX_RING_COUNT    1

/*---------------------------------------------------------------------------
 * ExprBuffer_t — local definition matching expr_util.h exactly.
 * Avoids pulling in the full app_common.h / expr_util.h include chain, which
 * would conflict with this stub's minimal CalcMode_t definition.
 *---------------------------------------------------------------------------*/
typedef struct {
    char    buf[MAX_EXPR_LEN];
    uint8_t len;
    uint8_t cursor;
} ExprBuffer_t;

static inline void ExprBuffer_Clear(ExprBuffer_t *b)
    { b->buf[0] = '\0'; b->len = 0; b->cursor = 0; }

/*---------------------------------------------------------------------------
 * HistoryEntry_t — must match calc_internal.h exactly.
 *---------------------------------------------------------------------------*/
typedef struct {
    char    expression[MAX_EXPR_LEN];
    char    result[MAX_RESULT_LEN];
    bool    has_matrix;
    uint8_t matrix_ring_idx;
    uint8_t matrix_ring_gen;
    uint8_t matrix_rows_cache;
} HistoryEntry_t;

/*---------------------------------------------------------------------------
 * Shared state — defined in test_prgm_exec.c
 *---------------------------------------------------------------------------*/
extern CalcMode_t current_mode;
extern float      ans;
extern bool       ans_is_matrix;
extern bool       angle_degrees;

extern HistoryEntry_t history[HISTORY_LINE_COUNT];
extern uint8_t        history_count;
extern int8_t         history_recall_offset;

extern ExprBuffer_t expr;   /* .buf = expression string, .len = length, .cursor = insertion point */

/* Program editor working buffer — also defined in test_prgm_exec.c */
extern char    prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];
extern uint8_t prgm_edit_num_lines;

/*---------------------------------------------------------------------------
 * prgm_parse_from_store — inline stub: split the stored body for @p idx
 * on '\n' into prgm_edit_lines[] and set prgm_edit_num_lines.
 *---------------------------------------------------------------------------*/
static inline void prgm_parse_from_store(uint8_t idx)
{
    const char *body = Prgm_GetBody(idx);
    uint8_t n = 0;
    const char *p = body;
    while (*p && n < PRGM_MAX_LINES) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len >= PRGM_MAX_LINE_LEN) len = PRGM_MAX_LINE_LEN - 1;
        memcpy(prgm_edit_lines[n], p, len);
        prgm_edit_lines[n][len] = '\0';
        n++;
        if (!eol) break;
        p = eol + 1;
    }
    prgm_edit_num_lines = n;
}

/*---------------------------------------------------------------------------
 * prgm_slot_is_used — slot is occupied when it has a non-empty name.
 *---------------------------------------------------------------------------*/
static inline bool prgm_slot_is_used(uint8_t slot)
{
    return Prgm_IsSlotOccupied(slot);
}

/*---------------------------------------------------------------------------
 * prgm_slot_id_str — write the 1-2 char slot ID into out[3].
 * Slots 0-9  → "1"–"9","0"  (TI-81: 1-9=slots 0-8, 0=slot 9)
 * Slots 10-35 → "A"–"Z"
 * Slot 36    → "T" (θ, represented as 'T' in plain ASCII)
 *---------------------------------------------------------------------------*/
static inline void prgm_slot_id_str(uint8_t slot, char *out)
{
    if (slot < 9)       { out[0] = (char)('1' + slot); out[1] = '\0'; }
    else if (slot == 9) { out[0] = '0';                out[1] = '\0'; }
    else if (slot < 36) { out[0] = (char)('A' + slot - 10); out[1] = '\0'; }
    else                { out[0] = 'T';                out[1] = '\0'; }
}

/*---------------------------------------------------------------------------
 * ANS getter/setter stubs — operate on the test-owned extern ans/ans_is_matrix
 * defined in test_prgm_exec.c.  Mirror the API in calculator_core.h.
 *---------------------------------------------------------------------------*/
static inline void  Calc_SetAnsScalar(float value)  { ans = value; ans_is_matrix = false; }
static inline void  Calc_SetAnsMatrix(float idx)    { ans = idx;   ans_is_matrix = true;  }
static inline float Calc_GetAns(void)               { return ans; }
static inline bool  Calc_GetAnsIsMatrix(void)       { return ans_is_matrix; }

/*---------------------------------------------------------------------------
 * format_calc_result — simplified version for host tests.
 * Formats a scalar result into buf using Calc_FormatResult; updates ans via
 * Calc_SetAns* so prgm_exec.c sees the correct state.
 * Matrix results fall through to a placeholder string.
 *---------------------------------------------------------------------------*/
static inline void format_calc_result(const CalcResult_t *r, char *buf, int buf_size)
{
    if (r->error != CALC_OK) {
        snprintf(buf, (size_t)buf_size, "ERR");
        return;
    }
    if (r->has_matrix) {
        snprintf(buf, (size_t)buf_size, "[matrix]");
        Calc_SetAnsMatrix((float)r->matrix_idx);
        return;
    }
    Calc_FormatResult(r->value, buf, (uint8_t)buf_size);
    Calc_SetAnsScalar(r->value);
}

#endif /* PRGM_EXEC_TEST_STUBS_H */
