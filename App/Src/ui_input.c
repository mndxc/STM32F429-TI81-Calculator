/**
 * @file    ui_input.c
 * @brief   Normal-mode expression input handlers extracted from calculator_core.c.
 *
 * Part of the UI super-module.
 *
 * Functions moved here from calculator_core.c:
 *   expr_prepend_ans_if_empty, expr_insert_char, expr_insert_str,
 *   expr_delete_at_cursor, handle_digit_key, handle_arithmetic_op,
 *   handle_function_insert, handle_sto_pending, handle_sto_key,
 *   handle_clear_key, handle_normal_graph_nav, handle_normal_mode.
 *
 * Stays in calculator_core.c (uses private statics):
 *   handle_history_nav, commit_history_entry, history_enter_evaluate,
 *   history_load_offset, try_execute_draw_command.
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "calc_engine.h"
#  include "prgm_exec.h"
#  include "ui_input.h"
#  include "calculator_core_test_stubs.h"
#  include "calculator_core.h"
#else
#  include "ui_shared.h"
#  include "calculator_core.h"
#  include "calc_history.h"
#  include "calc_engine.h"
#  include "ui_mode.h"
#  include "graph.h"
#  include "ui_yvars.h"
#endif
#include "expr_util.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * Forward declarations (static helpers)
 *---------------------------------------------------------------------------*/

static void expr_prepend_ans_if_empty(void);
static void expr_insert_char(char c);
void expr_insert_str(const char *s);
static void handle_digit_key(Token_t t);
static void handle_arithmetic_op(Token_t t);
static void handle_function_insert(Token_t t);
static void handle_sto_key(void);
static void handle_clear_key(void);
static void handle_normal_graph_nav(Token_t t);
static bool sto_mat_cancel(void);
static bool sto_mat_commit_whole(void);
static bool sto_mat_commit_elem(void);
static bool handle_sto_mat_elem(Token_t t);

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
 * Expression buffer helpers
 *---------------------------------------------------------------------------*/

static void expr_prepend_ans_if_empty(void)
{
    ExprBuffer_t *e = Calc_GetExpr();
    ExprUtil_PrependAns(e->buf, &e->len, &e->cursor, MAX_EXPR_LEN);
}

/**
 * @brief Inserts or overwrites a single character at cursor_pos.
 *
 * In overwrite mode (insert_mode == false) and cursor not at end, replaces
 * the character at cursor_pos and advances.  In insert mode, or at end,
 * characters shift right.
 */
static void expr_insert_char(char c)
{
    ExprBuffer_t *e = Calc_GetExpr();
    ExprUtil_InsertChar(e->buf, &e->len, &e->cursor, MAX_EXPR_LEN, Calc_GetInsertMode(), c);
}

/**
 * @brief Inserts a string at cursor and advances the cursor by its length.
 */
void expr_insert_str(const char *s)
{
    ExprBuffer_t *e = Calc_GetExpr();
    ExprUtil_InsertStr(e->buf, &e->len, &e->cursor, MAX_EXPR_LEN, s);
}

/**
 * @brief Deletes the character immediately before cursor (backspace).
 */
void expr_delete_at_cursor(void)
{
    ExprBuffer_Delete(Calc_GetExpr());
}

/*---------------------------------------------------------------------------
 * Digit and operator key handlers
 *---------------------------------------------------------------------------*/

static void handle_digit_key(Token_t t)
{
    if (t == TOKEN_DECIMAL) {
        expr_insert_char('.');
    } else {
        expr_insert_char((char)((t - TOKEN_0) + '0'));
    }
    Update_Calculator_Display();
}

static void handle_arithmetic_op(Token_t t)
{
    switch (t) {
    case TOKEN_ADD:    expr_prepend_ans_if_empty(); expr_insert_char('+');  break;
    case TOKEN_SUB:    expr_prepend_ans_if_empty(); expr_insert_char('-');  break;
    case TOKEN_MULT:   expr_prepend_ans_if_empty(); expr_insert_char('*');  break;
    case TOKEN_DIV:    expr_prepend_ans_if_empty(); expr_insert_char('/');  break;
    case TOKEN_SQUARE: expr_prepend_ans_if_empty(); expr_insert_str("^2"); break;
    case TOKEN_X_INV:  expr_prepend_ans_if_empty(); expr_insert_str("^-1");break;
    case TOKEN_POWER:  expr_prepend_ans_if_empty(); expr_insert_char('^'); break;
    case TOKEN_L_PAR:  expr_insert_char('(');                              break;
    case TOKEN_R_PAR:  expr_insert_char(')');                              break;
    case TOKEN_NEG:    expr_insert_char('-');                              break;
    default: break;
    }
    Update_Calculator_Display();
}

/*---------------------------------------------------------------------------
 * STO pending handler
 *---------------------------------------------------------------------------*/

bool handle_sto_pending(Token_t t)
{
    ExprBuffer_t *e = Calc_GetExpr();

    /* Matrix-element collection in progress — delegate entire phase */
    if (s_sto_mat_dst != 0xFF)
        return handle_sto_mat_elem(t);

    if (t >= TOKEN_A && t <= TOKEN_Z) {
        Calc_SetStoPending(false);
        static const char var_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        uint8_t var_idx = t - TOKEN_A;

        CalcResult_t result = Calc_Evaluate(e->buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                            Calc_GetAngleDegrees());

        char result_str[MAX_RESULT_LEN];
        char expr_hist[MAX_EXPR_LEN + 4];  /* expression + "->A\0" */
        snprintf(expr_hist, sizeof(expr_hist), "%s->%c", e->buf, var_names[var_idx]);

        if (result.error != CALC_OK) {
            strncpy(result_str, result.error_msg, MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else if (result.has_matrix) {
            strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else {
            calc_variables[var_idx] = result.value;
            Calc_SetAnsScalar(result.value);
            Calc_FormatResult(result.value, result_str, MAX_RESULT_LEN);
        }

        CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
        ExprBuffer_Clear(e);
        CalcHistory_ResetRecallOffset();

        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
        return true;

    } else if (t == TOKEN_THETA) {
        Calc_SetStoPending(false);
        CalcResult_t result = Calc_Evaluate(e->buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                            Calc_GetAngleDegrees());
        char result_str[MAX_RESULT_LEN];
        char expr_hist[MAX_EXPR_LEN + 6];
        snprintf(expr_hist, sizeof(expr_hist), "%s->\xCE\xB8", e->buf);  /* ->θ */
        if (result.error != CALC_OK) {
            strncpy(result_str, result.error_msg, MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else if (result.has_matrix) {
            strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else {
            calc_variables[26] = result.value;
            Calc_SetAnsScalar(result.value);
            Calc_FormatResult(result.value, result_str, MAX_RESULT_LEN);
        }
        CalcHistory_Commit(expr_hist, result_str, false, 0, 0, 0);
        ExprBuffer_Clear(e);
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

    } else if (t == TOKEN_Y_VARS) {
        /* STO→Yn — open Y-VARS menu in STO context; expression stores to chosen slot */
        Calc_SetStoPending(false);
        lvgl_lock();
        Yvars_OpenForSto(e->buf);
        lvgl_unlock();
        ExprBuffer_Clear(e);
        return true;

    } else if (t == TOKEN_CLEAR || t == TOKEN_2ND || t == TOKEN_ALPHA) {
        Calc_SetStoPending(false);
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    }

    /* Any other key cancels STO silently and falls through */
    Calc_SetStoPending(false);
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;
}

/*---------------------------------------------------------------------------
 * STO→matrix helper implementations
 *---------------------------------------------------------------------------*/

static bool sto_mat_cancel(void)
{
    Calc_SetStoPending(false);
    s_sto_mat_dst = 0xFF;
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;  /* fall through to normal-mode handler */
}

static bool sto_mat_commit_whole(void)
{
    ExprBuffer_t *e = Calc_GetExpr();
    static const char * const mat_names[3] = {"[A]", "[B]", "[C]"};
    uint8_t mat_idx = s_sto_mat_dst;

    Calc_SetStoPending(false);
    s_sto_mat_dst = 0xFF;

    CalcResult_t r = Calc_Evaluate(e->buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());

    char expr_hist[MAX_EXPR_LEN + 8];
    snprintf(expr_hist, sizeof(expr_hist), "%s->%s", e->buf, mat_names[mat_idx]);

    if (r.error != CALC_OK) {
        CalcHistory_Commit(expr_hist, r.error_msg, false, 0, 0, 0);
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
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

    ExprBuffer_Clear(e);
    CalcHistory_ResetRecallOffset();
    return true;
}

static bool sto_mat_commit_elem(void)
{
    ExprBuffer_t *e = Calc_GetExpr();
    static const char * const mat_names[3] = {"[A]", "[B]", "[C]"};
    uint8_t mat_idx = s_sto_mat_dst;
    uint8_t row     = s_sto_mat_row;  /* 1-based */
    uint8_t col     = s_sto_mat_col;  /* 1-based */

    Calc_SetStoPending(false);
    s_sto_mat_dst = 0xFF;

    CalcResult_t r = Calc_Evaluate(e->buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());

    char expr_hist[MAX_EXPR_LEN + 16];
    snprintf(expr_hist, sizeof(expr_hist), "%s->%s(%u,%u)",
             e->buf, mat_names[mat_idx], (unsigned)row, (unsigned)col);

    char result_str[MAX_RESULT_LEN];
    if (r.error != CALC_OK) {
        strncpy(result_str, r.error_msg, MAX_RESULT_LEN - 1);
        result_str[MAX_RESULT_LEN - 1] = '\0';
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
    ExprBuffer_Clear(e);
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
 * Normal-mode sub-handlers
 *---------------------------------------------------------------------------*/

static void handle_function_insert(Token_t t)
{
    switch (t) {
    case TOKEN_MTRX_A: expr_insert_str("[A]");  break;
    case TOKEN_MTRX_B: expr_insert_str("[B]");  break;
    case TOKEN_MTRX_C: expr_insert_str("[C]");  break;
    case TOKEN_LIST_X: expr_insert_str("{x}("); break;
    case TOKEN_LIST_Y: expr_insert_str("{y}("); break;

    case TOKEN_SIN:   expr_insert_str("sin(");  break;
    case TOKEN_COS:   expr_insert_str("cos(");  break;
    case TOKEN_TAN:   expr_insert_str("tan(");  break;
    case TOKEN_ASIN:  expr_insert_str("sin\xEE\x80\x81("); break;   /* sin⁻¹( */
    case TOKEN_ACOS:  expr_insert_str("cos\xEE\x80\x81("); break;   /* cos⁻¹( */
    case TOKEN_ATAN:  expr_insert_str("tan\xEE\x80\x81("); break;   /* tan⁻¹( */
    case TOKEN_ABS:   expr_insert_str("abs(");  break;
    case TOKEN_LN:    expr_insert_str("ln(");   break;
    case TOKEN_LOG:   expr_insert_str("log(");  break;
    case TOKEN_SQRT:  expr_insert_str("\xE2\x88\x9A("); break;
    case TOKEN_EE:    expr_insert_str("*10^");  break;
    case TOKEN_E_X:   expr_insert_str("exp(");  break;
    case TOKEN_TEN_X: expr_insert_str("10^(");  break;
    case TOKEN_PI:    expr_insert_str("π");     break;
    case TOKEN_ANS:   expr_insert_str("ANS");   break;
    case TOKEN_THETA: expr_insert_str("θ");     break;
    case TOKEN_SPACE: expr_insert_char(' ');    break;
    case TOKEN_COMMA: expr_insert_char(',');    break;
    case TOKEN_QUOTES: expr_insert_char('"');   break;
    case TOKEN_QSTN_M: expr_insert_char('?');   break;

    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z: {
        static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        char ch[2] = { alpha_chars[t - TOKEN_A], '\0' };
        expr_insert_str(ch);
        break;
    }

    default: break;
    }
    Update_Calculator_Display();
}

static void handle_clear_key(void)
{
    if (Graph_GetState()->active) {
        lvgl_lock();
        Graph_SetVisible(false);
        lvgl_unlock();
        return;
    }
    ExprBuffer_Clear(Calc_GetExpr());
    Update_Calculator_Display();
}

static void handle_sto_key(void)
{
    if (Calc_GetExpr()->len == 0) {
        expr_prepend_ans_if_empty();
        Update_Calculator_Display();
    }
    Calc_SetStoPending(true);
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
}

static void handle_normal_graph_nav(Token_t t)
{
    switch (t) {
    case TOKEN_Y_EQUALS:
        nav_to(Graph_GetState()->param_mode ? MODE_GRAPH_PARAM_YEQ : MODE_GRAPH_YEQ);
        break;
    case TOKEN_RANGE: nav_to(MODE_GRAPH_RANGE); break;
    case TOKEN_ZOOM:  nav_to(MODE_GRAPH_ZOOM);  break;
    case TOKEN_GRAPH: nav_to(MODE_GRAPH_FREE_CURSOR); break;
    case TOKEN_TRACE: nav_to(MODE_GRAPH_TRACE); break;
    default:          break;
    }
}

/*---------------------------------------------------------------------------
 * Normal-mode main dispatcher
 *---------------------------------------------------------------------------*/

void handle_normal_mode(Token_t t)
{
    switch (t) {
    case TOKEN_0 ... TOKEN_9:
    case TOKEN_DECIMAL:
        handle_digit_key(t);        break;
    case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT: case TOKEN_DIV:
    case TOKEN_SQUARE: case TOKEN_X_INV: case TOKEN_POWER:
    case TOKEN_L_PAR: case TOKEN_R_PAR: case TOKEN_NEG:
        handle_arithmetic_op(t);    break;
    case TOKEN_LEFT: case TOKEN_RIGHT:
    case TOKEN_UP:   case TOKEN_DOWN:
    case TOKEN_ENTER: case TOKEN_ENTRY:
        handle_history_nav(t);      break;
    case TOKEN_CLEAR:               handle_clear_key();          break;
    case TOKEN_DEL:                 expr_delete_at_cursor();
                                    Update_Calculator_Display(); break;
    case TOKEN_INS:                 Calc_SetInsertMode(!Calc_GetInsertMode());
                                    Update_Calculator_Display(); break;
    case TOKEN_MODE:                ui_mode_open();              break;
    case TOKEN_MATH:                menu_open(TOKEN_MATH,  MODE_NORMAL); break;
    case TOKEN_TEST:                menu_open(TOKEN_TEST,  MODE_NORMAL); break;
    case TOKEN_MATRX:               menu_open(TOKEN_MATRX, MODE_NORMAL); break;
    case TOKEN_PRGM:                menu_open(TOKEN_PRGM,  MODE_NORMAL); break;
    case TOKEN_STAT:                menu_open(TOKEN_STAT,  MODE_NORMAL); break;
    case TOKEN_DRAW:                menu_open(TOKEN_DRAW,  MODE_NORMAL); break;
    case TOKEN_VARS:                menu_open(TOKEN_VARS,   MODE_NORMAL); break;
    case TOKEN_Y_VARS:              menu_open(TOKEN_Y_VARS, MODE_NORMAL); break;
    case TOKEN_MTRX_A: case TOKEN_MTRX_B: case TOKEN_MTRX_C:
    case TOKEN_LIST_X: case TOKEN_LIST_Y:
    case TOKEN_SIN: case TOKEN_COS: case TOKEN_TAN:
    case TOKEN_ASIN: case TOKEN_ACOS: case TOKEN_ATAN:
    case TOKEN_ABS: case TOKEN_LN: case TOKEN_LOG: case TOKEN_SQRT:
    case TOKEN_EE: case TOKEN_E_X: case TOKEN_TEN_X:
    case TOKEN_PI: case TOKEN_ANS: case TOKEN_THETA:
    case TOKEN_SPACE: case TOKEN_COMMA: case TOKEN_QUOTES: case TOKEN_QSTN_M:
    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z:
        handle_function_insert(t);  break;
    case TOKEN_STO:                 handle_sto_key();            break;
    case TOKEN_X_T:
        /* In param mode insert T; in function mode insert X */
        handle_function_insert(Graph_GetState()->param_mode ? TOKEN_T : TOKEN_X);
        break;
    case TOKEN_Y_EQUALS: case TOKEN_RANGE: case TOKEN_ZOOM:
    case TOKEN_GRAPH:    case TOKEN_TRACE:
        handle_normal_graph_nav(t); break;
    default:                        break;
    }
}
