/**
 * @file    calculator_core.h
 * @brief   Public API for calculator_core.c.
 *
 * Exposes the ANS getter/setter API so external modules never write
 * `ans` or `ans_is_matrix` directly.  Always call Calc_SetAnsScalar()
 * or Calc_SetAnsMatrix() together — never update one without the other.
 *
 * Also exposes the mode getter/setter API so external modules never
 * write `current_mode` or `return_mode` directly.
 */

#ifndef CALCULATOR_CORE_H
#define CALCULATOR_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "app_common.h"  /* CalcMode_t */
#include "calc_engine.h" /* CalcResult_t — needed for format_calc_result */

/*
 * ANS getter/setter API.
 *
 * Rules:
 *   - Calc_SetAnsScalar: sets ans = value, ans_is_matrix = false.
 *   - Calc_SetAnsMatrix: sets ans = matrix slot index (as float),
 *                        ans_is_matrix = true.
 *   - Only update via these setters — never raw assignment from outside
 *     calculator_core.c.
 */
void  Calc_SetAnsScalar(float value);
void  Calc_SetAnsMatrix(float matrix_idx);
float Calc_GetAns(void);
bool  Calc_GetAnsIsMatrix(void);

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
 * Angle mode accessor — used by persist.c to restore angle mode without
 * used by persist.c without pulling in LVGL-dependent headers.
 */
bool Calc_GetAngleDegrees(void);
void Calc_SetAngleDegrees(bool degrees);

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
 * Expression buffer clear — clears the main expression buffer and resets the
 * cursor.  Wraps ExprEditor_Clear() so Application Core modules (e.g.
 * prgm_exec.c) can clear the expression without including expr_editor.h.
 * Also declared in calc_engine.h for the same reason.
 */
void Calc_ClearExpr(void);

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
