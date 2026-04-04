/**
 * @file    calc_stat.c
 * @brief   Pure statistical math layer — 1-Var, regressions, sort, clear.
 *
 * No LVGL, HAL, or RTOS dependencies. Operates only on StatData_t /
 * StatResults_t; writes regression coefficients to calc_variables per TI-81
 * convention (a → calc_variables[0], b → calc_variables[1]).
 */

#include "calc_stat.h"
#include "calc_engine.h"   /* calc_variables */
#include <math.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Internal helper — linear regression on transformed arrays
 *---------------------------------------------------------------------------*/

/**
 * @brief Least-squares linear regression on raw arrays u[] and v[].
 *        Writes slope to *slope, intercept to *intercept, Pearson r to *r_out.
 *        Returns false if degenerate (n < 2 or all u equal).
 */
static bool linreg_raw(const float *u, const float *v, uint8_t n,
                       float *slope, float *intercept, float *r_out)
{
    if (n < 2) return false;

    float sum_u = 0.0f, sum_v = 0.0f, sum_uu = 0.0f, sum_uv = 0.0f, sum_vv = 0.0f;
    for (uint8_t i = 0; i < n; i++) {
        sum_u  += u[i];
        sum_v  += v[i];
        sum_uu += u[i] * u[i];
        sum_uv += u[i] * v[i];
        sum_vv += v[i] * v[i];
    }

    float fn    = (float)n;
    float denom = fn * sum_uu - sum_u * sum_u;
    if (fabsf(denom) < 1e-12f) return false;

    *slope     = (fn * sum_uv - sum_u * sum_v) / denom;
    *intercept = (sum_v - *slope * sum_u) / fn;

    /* Pearson r */
    float ss_u = fn * sum_uu - sum_u * sum_u;
    float ss_v = fn * sum_vv - sum_v * sum_v;
    if (ss_u <= 0.0f || ss_v <= 0.0f) {
        *r_out = 0.0f;
    } else {
        *r_out = (fn * sum_uv - sum_u * sum_v) / sqrtf(ss_u * ss_v);
    }

    return true;
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

void CalcStat_Compute1Var(const StatData_t *d, StatResults_t *r)
{
    r->valid = false;
    if (d->list_len == 0) return;

    float n    = (float)d->list_len;
    float sx   = 0.0f, sx2 = 0.0f;
    for (uint8_t i = 0; i < d->list_len; i++) {
        sx  += d->list_x[i];
        sx2 += d->list_x[i] * d->list_x[i];
    }

    r->n      = n;
    r->sum_x  = sx;
    r->sum_x2 = sx2;
    r->mean_x = sx / n;

    /* Sample stdev Sx = sqrt(Σ(xi-x̄)² / (n-1)) */
    if (d->list_len >= 2) {
        float var_s = (sx2 - sx * sx / n) / (n - 1.0f);
        r->sx = (var_s > 0.0f) ? sqrtf(var_s) : 0.0f;
    } else {
        r->sx = 0.0f;
    }

    /* Population stdev σx = sqrt(Σ(xi-x̄)² / n) */
    {
        float var_p = (sx2 - sx * sx / n) / n;
        r->sigma_x = (var_p > 0.0f) ? sqrtf(var_p) : 0.0f;
    }

    r->valid = true;
}

bool CalcStat_ComputeLinReg(const StatData_t *d, StatResults_t *r)
{
    float slope, intercept, rval;
    if (!linreg_raw(d->list_x, d->list_y, d->list_len, &slope, &intercept, &rval))
        return false;

    r->reg_a = slope;
    r->reg_b = intercept;
    r->reg_r = rval;

    /* TI-81 convention: store a in variable A (index 0), b in variable B (index 1) */
    calc_variables[0] = slope;
    calc_variables[1] = intercept;

    return true;
}

bool CalcStat_ComputeLnReg(const StatData_t *d, StatResults_t *r)
{
    if (d->list_len < 2) return false;

    /* Transform: u = ln(x), v = y */
    float u[STAT_MAX_POINTS];
    for (uint8_t i = 0; i < d->list_len; i++) {
        if (d->list_x[i] <= 0.0f) return false;
        u[i] = logf(d->list_x[i]);
    }

    float slope, intercept, rval;
    if (!linreg_raw(u, d->list_y, d->list_len, &slope, &intercept, &rval))
        return false;

    /* y = intercept + slope * ln(x)  →  a = slope, b = intercept */
    r->reg_a = slope;
    r->reg_b = intercept;
    r->reg_r = rval;
    calc_variables[0] = slope;
    calc_variables[1] = intercept;
    return true;
}

bool CalcStat_ComputeExpReg(const StatData_t *d, StatResults_t *r)
{
    if (d->list_len < 2) return false;

    /* Transform: u = x, v = ln(y) */
    float v[STAT_MAX_POINTS];
    for (uint8_t i = 0; i < d->list_len; i++) {
        if (d->list_y[i] <= 0.0f) return false;
        v[i] = logf(d->list_y[i]);
    }

    float slope, intercept, rval;
    if (!linreg_raw(d->list_x, v, d->list_len, &slope, &intercept, &rval))
        return false;

    /* y = e^intercept * e^(slope*x)  →  a = e^intercept, b = slope */
    r->reg_a = expf(intercept);
    r->reg_b = slope;
    r->reg_r = rval;
    calc_variables[0] = r->reg_a;
    calc_variables[1] = slope;
    return true;
}

bool CalcStat_ComputePwrReg(const StatData_t *d, StatResults_t *r)
{
    if (d->list_len < 2) return false;

    /* Transform: u = ln(x), v = ln(y) */
    float u[STAT_MAX_POINTS], v[STAT_MAX_POINTS];
    for (uint8_t i = 0; i < d->list_len; i++) {
        if (d->list_x[i] <= 0.0f || d->list_y[i] <= 0.0f) return false;
        u[i] = logf(d->list_x[i]);
        v[i] = logf(d->list_y[i]);
    }

    float slope, intercept, rval;
    if (!linreg_raw(u, v, d->list_len, &slope, &intercept, &rval))
        return false;

    /* y = e^intercept * x^slope  →  a = e^intercept, b = slope */
    r->reg_a = expf(intercept);
    r->reg_b = slope;
    r->reg_r = rval;
    calc_variables[0] = r->reg_a;
    calc_variables[1] = slope;
    return true;
}

void CalcStat_SortX(StatData_t *d)
{
    /* Insertion sort — pairs travel together */
    for (uint8_t i = 1; i < d->list_len; i++) {
        float kx = d->list_x[i], ky = d->list_y[i];
        int16_t j = (int16_t)i - 1;
        while (j >= 0 && d->list_x[j] > kx) {
            d->list_x[j + 1] = d->list_x[j];
            d->list_y[j + 1] = d->list_y[j];
            j--;
        }
        d->list_x[j + 1] = kx;
        d->list_y[j + 1] = ky;
    }
}

void CalcStat_SortY(StatData_t *d)
{
    for (uint8_t i = 1; i < d->list_len; i++) {
        float kx = d->list_x[i], ky = d->list_y[i];
        int16_t j = (int16_t)i - 1;
        while (j >= 0 && d->list_y[j] > ky) {
            d->list_x[j + 1] = d->list_x[j];
            d->list_y[j + 1] = d->list_y[j];
            j--;
        }
        d->list_x[j + 1] = kx;
        d->list_y[j + 1] = ky;
    }
}

void CalcStat_Clear(StatData_t *d)
{
    memset(d, 0, sizeof(*d));
}
