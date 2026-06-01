/**
 * @file    ui_sto.c
 * @brief   STO→variable and STO→matrix state machine.
 *
 * Extracted from ui_input.c. Part of the UI super-module.
 *
 * Contains the STO destination state machine: variable (A–Z, θ), whole-matrix
 * ([A]/[B]/[C]), matrix element ([A](row,col)), and Y= slot (Y-VARS menu).
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "calc_engine.h"
#  include "prgm_exec.h"
#  include "ui_sto.h"
#  include "calculator_core_test_stubs.h"
#  include "calculator_core.h"
#  include "expr_editor.h"
#else
#  include "ui_shared.h"
#  include "calculator_core.h"
#  include "calc_history.h"
#  include "calc_engine.h"
#  include "ui_yvars.h"
#  include "ui_error.h"
#  include "expr_editor.h"
#  include "ui_stat.h"
#endif
#include "expr_util.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * STO→matrix state machine
 *---------------------------------------------------------------------------*/

typedef enum {
    STO_MAT_WAIT,      /* received matrix token; waiting for ( or ENTER */
    STO_MAT_ROW,       /* inside (, collecting row digit (1–6) */
    STO_MAT_SEP,       /* row received; waiting for comma */
    STO_MAT_COL,       /* comma seen; collecting col digit (1–6) */
    STO_MAT_RPAREN,    /* col received; waiting for ) */
    STO_MAT_ELEM,      /* ) seen; waiting for ENTER to commit to element */
} StoMatPhase_t;

static uint8_t       s_sto_mat_dst   = 0xFF;  /* 0xFF=none; 0/1/2 = [A]/[B]/[C] */
static StoMatPhase_t s_sto_mat_phase;
static uint8_t       s_sto_mat_row;            /* 1-based row, 0 = not set */
static uint8_t       s_sto_mat_col;            /* 1-based col, 0 = not set */

/*---------------------------------------------------------------------------
 * STO→matrix helper implementations
 *---------------------------------------------------------------------------*/

static bool sto_mat_cancel(void)
{
    ExprEditor_SetStoPending(false);
    s_sto_mat_dst = 0xFF;
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;  /* fall through to normal-mode handler */
}

static bool sto_mat_commit_whole(void)
{
    const char *ebuf = ExprEditor_GetBuf();
    static const char * const mat_names[3] = {"[A]", "[B]", "[C]"};
    uint8_t mat_idx = s_sto_mat_dst;

    ExprEditor_SetStoPending(false);
    s_sto_mat_dst = 0xFF;

    CalcResult_t r = Calc_Evaluate(ebuf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());

    char expr_hist[MAX_EXPR_LEN + 8];
    snprintf(expr_hist, sizeof(expr_hist), "%s->%s", ebuf, mat_names[mat_idx]);

    if (r.error != CALC_OK) {
#ifndef HOST_TEST
        char saved[MAX_EXPR_LEN];
        strncpy(saved, ebuf, MAX_EXPR_LEN - 1);
        saved[MAX_EXPR_LEN - 1] = '\0';
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        Error_Open(r.error, saved, r.error_offset, true);
        return true;
#else
        CalcHistory_Commit(expr_hist, r.error_msg, false, 0, 0, 0);
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
#endif
    } else if (r.has_matrix) {
        /* Matrix-to-matrix copy: dimensions follow source */
        calc_matrices[mat_idx] = calc_matrices[r.matrix_idx];
        Calc_SetAnsMatrix((float)mat_idx);
        Calc_CommitMatrixToHistory(expr_hist, mat_idx);
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
    } else {
        /* Scalar fills every element; requires existing dimensions */
        CalcMatrix_t *m = &calc_matrices[mat_idx];
        if (m->rows == 0 || m->cols == 0) {
            CalcHistory_Commit(expr_hist, "ERR:DIMENSION", false, 0, 0, 0);
            lvgl_lock();
            CalcHistory_UpdateDisplay();
            ui_update_status_bar();
            lvgl_unlock();
        } else {
            for (uint8_t ri = 0; ri < m->rows; ri++)
                for (uint8_t ci = 0; ci < m->cols; ci++)
                    m->data[ri][ci] = r.value;
            Calc_SetAnsMatrix((float)mat_idx);
            Calc_CommitMatrixToHistory(expr_hist, mat_idx);
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
        }
    }

    ExprEditor_Clear();
    CalcHistory_ResetRecallOffset();
    return true;
}

static bool sto_mat_commit_elem(void)
{
    const char *ebuf = ExprEditor_GetBuf();
    static const char * const mat_names[3] = {"[A]", "[B]", "[C]"};
    uint8_t mat_idx = s_sto_mat_dst;
    uint8_t row     = s_sto_mat_row;  /* 1-based */
    uint8_t col     = s_sto_mat_col;  /* 1-based */

    ExprEditor_SetStoPending(false);
    s_sto_mat_dst = 0xFF;

    CalcResult_t r = Calc_Evaluate(ebuf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());

    char expr_hist[MAX_EXPR_LEN + 16];
    snprintf(expr_hist, sizeof(expr_hist), "%s->%s(%u,%u)",
             ebuf, mat_names[mat_idx], (unsigned)row, (unsigned)col);

    char result_str[MAX_RESULT_LEN];
    if (r.error != CALC_OK) {
#ifndef HOST_TEST
        char saved[MAX_EXPR_LEN];
        strncpy(saved, ebuf, MAX_EXPR_LEN - 1);
        saved[MAX_EXPR_LEN - 1] = '\0';
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        Error_Open(r.error, saved, r.error_offset, true);
        return true;
#else
        strncpy(result_str, r.error_msg, MAX_RESULT_LEN - 1);
        result_str[MAX_RESULT_LEN - 1] = '\0';
        CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
#endif
    } else if (r.has_matrix) {
        strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
        result_str[MAX_RESULT_LEN - 1] = '\0';
    } else {
        CalcMatrix_t *m = &calc_matrices[mat_idx];
        if (row == 0 || col == 0 || row > m->rows || col > m->cols) {
            strncpy(result_str, "ERR:DIMENSION", MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else {
            m->data[row - 1][col - 1] = r.value;
            Calc_SetAnsScalar(r.value);
            Calc_FormatResult(r.value, result_str, MAX_RESULT_LEN);
        }
    }

    CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
    ExprEditor_Clear();
    CalcHistory_ResetRecallOffset();
    lvgl_lock();
    CalcHistory_UpdateDisplay();
    ui_update_status_bar();
    lvgl_unlock();
    return true;
}

static bool handle_sto_mat_elem(Token_t t)
{
    switch (s_sto_mat_phase) {
    case STO_MAT_WAIT:
        if (t == TOKEN_L_PAR) {
            s_sto_mat_phase = STO_MAT_ROW;
            return true;
        }
        if (t == TOKEN_ENTER)
            return sto_mat_commit_whole();
        if (t == TOKEN_CLEAR)
            return sto_mat_cancel();
        return sto_mat_cancel();

    case STO_MAT_ROW:
        if (t >= TOKEN_1 && t <= TOKEN_6) {
            s_sto_mat_row   = (uint8_t)(t - TOKEN_0);
            s_sto_mat_phase = STO_MAT_SEP;
            return true;
        }
        return sto_mat_cancel();

    case STO_MAT_SEP:
        if (t == TOKEN_COMMA) {
            s_sto_mat_phase = STO_MAT_COL;
            return true;
        }
        return sto_mat_cancel();

    case STO_MAT_COL:
        if (t >= TOKEN_1 && t <= TOKEN_6) {
            s_sto_mat_col   = (uint8_t)(t - TOKEN_0);
            s_sto_mat_phase = STO_MAT_RPAREN;
            return true;
        }
        return sto_mat_cancel();

    case STO_MAT_RPAREN:
        if (t == TOKEN_R_PAR) {
            s_sto_mat_phase = STO_MAT_ELEM;
            return true;
        }
        return sto_mat_cancel();

    case STO_MAT_ELEM:
        if (t == TOKEN_ENTER)
            return sto_mat_commit_elem();
        if (t == TOKEN_CLEAR)
            return sto_mat_cancel();
        return sto_mat_cancel();
    }
    return sto_mat_cancel();  /* unreachable; satisfies compiler */
}

/*---------------------------------------------------------------------------
 * STO→{x}(n) / {y}(n) state machine
 *---------------------------------------------------------------------------*/

typedef enum {
    STO_LIST_INDEX,   /* received list token; collecting index digits */
    STO_LIST_RPAREN,  /* one or more digits received; waiting for ) */
    STO_LIST_COMMIT,  /* ) seen; waiting for ENTER to commit */
} StoListPhase_t;

static uint8_t       s_sto_list_is_y = 0xFF; /* 0xFF=none; 0={x}, 1={y} */
static StoListPhase_t s_sto_list_phase;
static uint16_t      s_sto_list_idx;          /* accumulates digit(s); 1-based */

static bool sto_list_cancel(void)
{
    ExprEditor_SetStoPending(false);
    s_sto_list_is_y = 0xFF;
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;
}

static bool sto_list_commit(void)
{
    const char *ebuf = ExprEditor_GetBuf();
    uint8_t is_y  = s_sto_list_is_y;
    uint8_t idx   = (uint8_t)s_sto_list_idx;

    ExprEditor_SetStoPending(false);
    s_sto_list_is_y = 0xFF;

    CalcResult_t r = Calc_Evaluate(ebuf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());

    char expr_hist[MAX_EXPR_LEN + 12];
    snprintf(expr_hist, sizeof(expr_hist), "%s->{%c}(%u)",
             ebuf, is_y ? 'y' : 'x', (unsigned)idx);

    char result_str[MAX_RESULT_LEN];
    if (r.error != CALC_OK) {
#ifndef HOST_TEST
        char saved[MAX_EXPR_LEN];
        strncpy(saved, ebuf, MAX_EXPR_LEN - 1);
        saved[MAX_EXPR_LEN - 1] = '\0';
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        Error_Open(r.error, saved, r.error_offset, true);
        return true;
#else
        strncpy(result_str, r.error_msg, MAX_RESULT_LEN - 1);
        result_str[MAX_RESULT_LEN - 1] = '\0';
        CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
#endif
    } else if (r.has_matrix) {
        strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
        result_str[MAX_RESULT_LEN - 1] = '\0';
    } else {
#ifndef HOST_TEST
        if (is_y)
            Stat_WriteListY(idx, r.value);
        else
            Stat_WriteListX(idx, r.value);
#endif
        Calc_SetAnsScalar(r.value);
        Calc_FormatResult(r.value, result_str, MAX_RESULT_LEN);
    }

    CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
    ExprEditor_Clear();
    CalcHistory_ResetRecallOffset();
    lvgl_lock();
    CalcHistory_UpdateDisplay();
    ui_update_status_bar();
    lvgl_unlock();
    return true;
}

static bool handle_sto_list_elem(Token_t t)
{
    switch (s_sto_list_phase) {
    case STO_LIST_INDEX:
        if (t >= TOKEN_1 && t <= TOKEN_9) {
            s_sto_list_idx  = (uint16_t)(t - TOKEN_0);
            s_sto_list_phase = STO_LIST_RPAREN;
            return true;
        }
        return sto_list_cancel();

    case STO_LIST_RPAREN:
        if (t >= TOKEN_0 && t <= TOKEN_9) {
            /* allow multi-digit index up to 150 */
            uint16_t next = s_sto_list_idx * 10u + (uint16_t)(t - TOKEN_0);
            if (next <= STAT_MAX_POINTS) s_sto_list_idx = next;
            return true;
        }
        if (t == TOKEN_R_PAR) {
            s_sto_list_phase = STO_LIST_COMMIT;
            return true;
        }
        return sto_list_cancel();

    case STO_LIST_COMMIT:
        if (t == TOKEN_ENTER)
            return sto_list_commit();
        if (t == TOKEN_CLEAR)
            return sto_list_cancel();
        return sto_list_cancel();
    }
    return sto_list_cancel();
}

/*---------------------------------------------------------------------------
 * STO pending handler
 *---------------------------------------------------------------------------*/

bool handle_sto_pending(Token_t t)
{
    /* Matrix-element collection in progress — delegate entire phase */
    if (s_sto_mat_dst != 0xFF)
        return handle_sto_mat_elem(t);

    /* Stat-list element collection in progress — delegate entire phase */
    if (s_sto_list_is_y != 0xFF)
        return handle_sto_list_elem(t);

    if (t >= TOKEN_A && t <= TOKEN_Z) {
        ExprEditor_SetStoPending(false);
        const char *ebuf = ExprEditor_GetBuf();
        static const char var_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        uint8_t var_idx = t - TOKEN_A;

        CalcResult_t result = Calc_Evaluate(ebuf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                            Calc_GetAngleDegrees());

        char result_str[MAX_RESULT_LEN];
        char expr_hist[MAX_EXPR_LEN + 4];  /* expression + "->A\0" */
        snprintf(expr_hist, sizeof(expr_hist), "%s->%c", ebuf, var_names[var_idx]);

        if (result.error != CALC_OK) {
#ifndef HOST_TEST
            char saved[MAX_EXPR_LEN];
            strncpy(saved, ebuf, MAX_EXPR_LEN - 1);
            saved[MAX_EXPR_LEN - 1] = '\0';
            ExprEditor_Clear();
            CalcHistory_ResetRecallOffset();
            Error_Open(result.error, saved, result.error_offset, true);
#else
            strncpy(result_str, result.error_msg, MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
            CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
            ExprEditor_Clear();
            CalcHistory_ResetRecallOffset();
            lvgl_lock();
            CalcHistory_UpdateDisplay();
            ui_update_status_bar();
            lvgl_unlock();
#endif
            return true;
        } else if (result.has_matrix) {
            strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else {
            calc_variables[var_idx] = result.value;
            Calc_SetAnsScalar(result.value);
            Calc_FormatResult(result.value, result_str, MAX_RESULT_LEN);
        }

        CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();

        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
        return true;

    } else if (t == TOKEN_THETA) {
        ExprEditor_SetStoPending(false);
        const char *ebuf = ExprEditor_GetBuf();
        CalcResult_t result = Calc_Evaluate(ebuf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                            Calc_GetAngleDegrees());
        char result_str[MAX_RESULT_LEN];
        char expr_hist[MAX_EXPR_LEN + 6];
        snprintf(expr_hist, sizeof(expr_hist), "%s->\xCE\xB8", ebuf);  /* ->θ */
        if (result.error != CALC_OK) {
#ifndef HOST_TEST
            char saved[MAX_EXPR_LEN];
            strncpy(saved, ebuf, MAX_EXPR_LEN - 1);
            saved[MAX_EXPR_LEN - 1] = '\0';
            ExprEditor_Clear();
            CalcHistory_ResetRecallOffset();
            Error_Open(result.error, saved, result.error_offset, true);
#else
            strncpy(result_str, result.error_msg, MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
            CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
            ExprEditor_Clear();
            CalcHistory_ResetRecallOffset();
            lvgl_lock();
            CalcHistory_UpdateDisplay();
            ui_update_status_bar();
            lvgl_unlock();
#endif
            return true;
        } else if (result.has_matrix) {
            strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else {
            calc_variables[26] = result.value;
            Calc_SetAnsScalar(result.value);
            Calc_FormatResult(result.value, result_str, MAX_RESULT_LEN);
        }
        CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
        ExprEditor_Clear();
        CalcHistory_ResetRecallOffset();
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
        return true;

    } else if (t == TOKEN_MTRX_A || t == TOKEN_MTRX_B || t == TOKEN_MTRX_C) {
        /* STO→[A/B/C] — enter matrix-destination phase; ENTER=whole, (=element */
        s_sto_mat_dst   = (uint8_t)(t - TOKEN_MTRX_A);
        s_sto_mat_phase = STO_MAT_WAIT;
        s_sto_mat_row   = 0;
        s_sto_mat_col   = 0;
        /* sto_pending stays true so the cursor keeps showing the → glyph */
        return true;

    } else if (t == TOKEN_LIST_X || t == TOKEN_LIST_Y) {
        /* STO→{x}(n) or STO→{y}(n) — enter list-element phase */
        s_sto_list_is_y  = (t == TOKEN_LIST_Y) ? 1u : 0u;
        s_sto_list_phase = STO_LIST_INDEX;
        s_sto_list_idx   = 0;
        /* sto_pending stays true */
        return true;

    } else if (t == TOKEN_Y_VARS) {
        /* STO→Yn — open Y-VARS menu in STO context; expression stores to chosen slot */
        ExprEditor_SetStoPending(false);
        const char *ebuf = ExprEditor_GetBuf();
        lvgl_lock();
        Yvars_OpenForSto(ebuf);
        lvgl_unlock();
        ExprEditor_Clear();
        return true;

    } else if (t == TOKEN_CLEAR || t == TOKEN_2ND || t == TOKEN_ALPHA) {
        ExprEditor_SetStoPending(false);
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    }

    /* Any other key cancels STO silently and falls through */
    ExprEditor_SetStoPending(false);
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;
}
