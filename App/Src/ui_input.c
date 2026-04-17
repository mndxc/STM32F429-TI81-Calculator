/**
 * @file    ui_input.c
 * @brief   Normal-mode expression input handlers extracted from calculator_core.c.
 *
 * Part of the UI super-module; may include calc_internal.h freely.
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
#  include "calc_internal.h"   /* includes calculator_core.h */
#  include "ui_mode.h"
#  include "graph.h"
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

/*---------------------------------------------------------------------------
 * Expression buffer helpers
 *---------------------------------------------------------------------------*/

static void expr_prepend_ans_if_empty(void)
{
    ExprUtil_PrependAns(expr.buf, &expr.len, &expr.cursor, MAX_EXPR_LEN);
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
    ExprUtil_InsertChar(expr.buf, &expr.len, &expr.cursor, MAX_EXPR_LEN, insert_mode, c);
}

/**
 * @brief Inserts a string at cursor and advances the cursor by its length.
 */
void expr_insert_str(const char *s)
{
    ExprUtil_InsertStr(expr.buf, &expr.len, &expr.cursor, MAX_EXPR_LEN, s);
}

/**
 * @brief Deletes the character immediately before cursor (backspace).
 */
void expr_delete_at_cursor(void)
{
    ExprBuffer_Delete(&expr);
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
    if (t >= TOKEN_A && t <= TOKEN_Z) {
        sto_pending = false;
        static const char var_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        uint8_t var_idx = t - TOKEN_A;

        CalcResult_t result = Calc_Evaluate(expr.buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                            angle_degrees);

        char result_str[MAX_RESULT_LEN];
        char expr_hist[MAX_EXPR_LEN + 4];  /* expression + "->A\0" */
        snprintf(expr_hist, sizeof(expr_hist), "%s->%c", expr.buf, var_names[var_idx]);

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

        uint8_t idx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[idx].expression, expr_hist, MAX_EXPR_LEN - 1);
        history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
        strncpy(history[idx].result, result_str, MAX_RESULT_LEN - 1);
        history[idx].result[MAX_RESULT_LEN - 1] = '\0';
        history[idx].has_matrix = false;
        reset_matrix_scroll_focus();
        history_count++;

        ExprBuffer_Clear(&expr);
        history_recall_offset = 0;

        lvgl_lock();
        ui_update_history();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    } else if (t == TOKEN_CLEAR || t == TOKEN_2ND || t == TOKEN_ALPHA) {
        sto_pending = false;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    }
    /* Any other key cancels STO silently and falls through */
    sto_pending = false;
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;
}

/*---------------------------------------------------------------------------
 * Normal-mode sub-handlers
 *---------------------------------------------------------------------------*/

static void handle_function_insert(Token_t t)
{
    switch (t) {
    case TOKEN_MTRX_A: expr_insert_str("[A]"); break;
    case TOKEN_MTRX_B: expr_insert_str("[B]"); break;
    case TOKEN_MTRX_C: expr_insert_str("[C]"); break;

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
    ExprBuffer_Clear(&expr);
    Update_Calculator_Display();
}

static void handle_sto_key(void)
{
    if (expr.len == 0) {
        expr_prepend_ans_if_empty();
        Update_Calculator_Display();
    }
    sto_pending = true;
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
    case TOKEN_GRAPH: nav_to(MODE_NORMAL);       break;
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
    case TOKEN_INS:                 insert_mode = !insert_mode;
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
