/**
 * @file    calculator_core.h
 * @brief   Public API for calculator_core.c — mode routing, UI refresh,
 *          and insert/cursor state.
 *
 * ANS getter/setter and angle-mode accessor declarations have moved to
 * calc_engine.h (included below) so Application Core modules can access
 * them without depending on the UI Logic layer.
 */

#ifndef CALCULATOR_CORE_H
#define CALCULATOR_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "app_common.h"  /* CalcMode_t */
#include "calc_engine.h" /* CalcResult_t — needed for format_calc_result */

/*
 * ANS getter/setter and angle-mode API.
 *
 * These are also declared in calc_engine.h (included above) so that
 * Application Core modules (persist.c, prgm_exec.c) can access them without
 * including this UI Logic header.  Redundant declarations are compatible in C.
 *
 * In HOST_TEST builds, calc_engine.h hides these declarations (to avoid
 * conflict with the static-inline stubs in prgm_exec_test_stubs.h), so this
 * header remains the authoritative declaration for all other HOST_TEST files.
 *
 * Rules:
 *   - Calc_SetAnsScalar: sets ans = value, ans_is_matrix = false.
 *   - Calc_SetAnsMatrix: sets ans = matrix slot index (as float),
 *                        ans_is_matrix = true.
 *   - Never write the backing statics directly from outside calculator_core.c.
 */
void  Calc_SetAnsScalar(float value);
void  Calc_SetAnsMatrix(float matrix_idx);
float Calc_GetAns(void);
bool  Calc_GetAnsIsMatrix(void);
bool  Calc_GetAngleDegrees(void);
void  Calc_SetAngleDegrees(bool degrees);

/* Calc_ClearExpr() is also declared in calc_engine.h (included above). */
void Calc_ClearExpr(void);

/*
 * Mode getter/setter API.
 *
 * Rules:
 *   - All mode transitions outside calculator_core.c must go through
 *     Calc_SetMode() / Calc_SetReturnMode().
 *   - These are plain setters — no business logic, no lock acquisition.
 *     Callers hold lvgl_lock() when needed, same as before.
 */
void        Calc_SetMode(CalcMode_t mode);
void        Calc_SetReturnMode(CalcMode_t mode);
CalcMode_t  Calc_GetMode(void);
CalcMode_t  Calc_GetReturnMode(void);

/*
 * Expression buffer read accessor — returns expr.buf as a const pointer.
 * Used by ui_draw.c to inspect the current expression.
 */
const char *Calc_GetExprBuf(void);

/*
 * Input state reset — sets sto_pending = false and clears the expression
 * buffer.  Used by ui_reset.c after a full memory clear.
 */
void Calc_ResetInputState(void);

/*
 * Input-mode state getter/setter API.
 *
 * insert_mode and cursor_visible are shared across all overlay cursor editors
 * (Y=, RANGE, MATRIX, PRGM, etc.) and remain in calculator_core.c.
 * The expression buffer, sto_pending flag, and main-screen cursor objects
 * have moved to expr_editor.c — use ExprEditor_* (from expr_editor.h) directly.
 */
bool Calc_GetInsertMode(void);
void Calc_SetInsertMode(bool v);
bool Calc_GetCursorVisible(void);
void Calc_SetCursorVisible(bool v);

/*
 * Super-module internal display/nav functions — defined in calculator_core.c,
 * called by other UI super-module files (graph_ui.c, ui_prgm.c, etc.).
 */
void ui_refresh_display(void);
void ui_output_row(uint8_t row_1based, const char *text);
void handle_history_nav(Token_t t);

/*
 * Matrix history commit — writes calc_matrices[mat_idx] into the matrix ring
 * and calls CalcHistory_Commit with the has_matrix fields filled in.
 * Used by handle_sto_pending (ui_input.c) which cannot access the private ring.
 * Must not be called when the matrix has rows == 0 or cols == 0.
 */
void Calc_CommitMatrixToHistory(const char *expr_text, uint8_t mat_idx);

#ifdef HOST_TEST
/**
 * @brief Validate CalcMode_t routing topology.
 *
 * Walk every value in [0, MODE_COUNT) and assert each appears in exactly one of:
 *   (a) k_route_table[] — a non-fallback predicate fires when current_mode == mode, or
 *   (b) known_special_cases[] — modes intentionally handled outside per-mode dispatch.
 *
 * Also checks that known_special_cases[] contains no out-of-range values.
 * Available only in HOST_TEST builds; call from test_mode_topology.c.
 */
bool calc_mode_topology_validate(void);
#endif /* HOST_TEST */

#endif /* CALCULATOR_CORE_H */
