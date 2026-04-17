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
#include "app_common.h"  /* CalcMode_t */

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

#endif /* CALCULATOR_CORE_H */
