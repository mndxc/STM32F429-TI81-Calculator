/**
 * @file    calc_stat.h
 * @brief   Pure statistical math layer — 1-Var, regressions, sort, clear.
 *
 * No LVGL, HAL, or RTOS dependencies. All functions operate on StatData_t
 * and StatResults_t structs defined in app_common.h.
 */

#ifndef CALC_STAT_H
#define CALC_STAT_H

#include "app_common.h"

/**
 * @brief Compute 1-Variable statistics for the data list.
 *
 * Sets r->n, r->mean_x, r->sx (sample stdev), r->sigma_x (population stdev),
 * r->sum_x, r->sum_x2. Sets r->valid = true on success, false if list empty.
 * Regression fields (reg_a, reg_b, reg_r) are not modified.
 */
void CalcStat_Compute1Var(const StatData_t *d, StatResults_t *r);

/**
 * @brief Compute linear regression y = a*x + b.
 *
 * Writes a and b to calc_variables['A'-'A'] and calc_variables['B'-'A'] per
 * TI-81 convention. Sets r->reg_a, r->reg_b, r->reg_r (Pearson r).
 * Returns false if n < 2 or all X values are equal (degenerate).
 */
bool CalcStat_ComputeLinReg(const StatData_t *d, StatResults_t *r);

/**
 * @brief Compute logarithmic regression y = a + b*ln(x).
 *
 * Linearises by substituting u = ln(x) then applies linear regression.
 * Returns false if any x <= 0 or degenerate.
 */
bool CalcStat_ComputeLnReg(const StatData_t *d, StatResults_t *r);

/**
 * @brief Compute exponential regression y = a * e^(b*x).
 *
 * Linearises by substituting v = ln(y) then applies linear regression.
 * Returns false if any y <= 0 or degenerate.
 */
bool CalcStat_ComputeExpReg(const StatData_t *d, StatResults_t *r);

/**
 * @brief Compute power regression y = a * x^b.
 *
 * Linearises by taking ln(x) and ln(y) then applies linear regression.
 * Returns false if any x <= 0 or y <= 0, or degenerate.
 */
bool CalcStat_ComputePwrReg(const StatData_t *d, StatResults_t *r);

/**
 * @brief Sort data pairs by x ascending (y follows x).
 */
void CalcStat_SortX(StatData_t *d);

/**
 * @brief Sort data pairs by y ascending (x follows y).
 */
void CalcStat_SortY(StatData_t *d);

/**
 * @brief Clear all data points (set list_len = 0, zero arrays).
 */
void CalcStat_Clear(StatData_t *d);

/**
 * @brief Y-variable statistics — Σy, Σy², Σxy, ȳ, Sy (sample), σy (population).
 *
 * These mirror the x-stat fields stored in StatResults_t but are computed
 * on-the-fly from the raw list_y array so that any precision fix applied to
 * CalcStat_Compute1Var automatically propagates to the y path.
 */
float CalcStat_SumY(const StatData_t *d);
float CalcStat_SumY2(const StatData_t *d);
float CalcStat_SumXY(const StatData_t *d);
float CalcStat_MeanY(const StatData_t *d);
float CalcStat_SxY(const StatData_t *d);
float CalcStat_SigmaY(const StatData_t *d);

#endif /* CALC_STAT_H */
