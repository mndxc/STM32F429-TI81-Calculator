/**
 * @file    test_stat.c
 * @brief   Host unit tests for the pure statistical math layer (calc_stat.c).
 *
 * Tests: 1-Var, LinReg (including variable storage and Pearson r), LnReg,
 *        ExpReg, PwrReg (degenerate), SortX, SortY, Clear.
 *
 * Build and run (no ARM toolchain required):
 *   cmake -S App/Tests -B build-tests && cmake --build build-tests
 *   ctest --test-dir build-tests -R stat
 *
 * Returns 0 on all pass, 1 on any failure.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "calc_stat.h"
#include "calc_engine.h"   /* calc_variables */

/* --- Test infrastructure -------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name) do {                                         \
    if (cond) {                                                        \
        g_passed++;                                                    \
    } else {                                                           \
        g_failed++;                                                    \
        printf("  FAIL [line %d]: %s\n", __LINE__, (name));           \
    }                                                                  \
} while (0)

#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-3)
#define NEARF(a, b, tol) (fabs((double)(a) - (double)(b)) < (double)(tol))

/* --- Helpers -------------------------------------------------------------- */

/** Populate d with n sequential x values 1..n, y values given. */
static void fill_xy(StatData_t *d, const float *xs, const float *ys, uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        d->list_x[i] = xs[i];
        d->list_y[i] = ys[i];
    }
    d->list_len = n;
}

/* --- Tests ---------------------------------------------------------------- */

static void test_1var_basic(void)
{
    printf("test_1var_basic\n");
    StatData_t d = {0};
    StatResults_t r = {0};

    float xs[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float ys[5] = {0};
    fill_xy(&d, xs, ys, 5);

    CalcStat_Compute1Var(&d, &r);

    CHECK(r.valid, "1var_basic: valid");
    CHECK(NEAR(r.n, 5.0f),         "1var_basic: n=5");
    CHECK(NEAR(r.mean_x, 3.0f),    "1var_basic: mean=3");
    CHECK(NEAR(r.sum_x, 15.0f),    "1var_basic: sumx=15");
    CHECK(NEAR(r.sum_x2, 55.0f),   "1var_basic: sumx2=55");
    /* Sample stdev: sqrt(10/4) ≈ 1.5811 */
    CHECK(NEARF(r.sx, 1.5811f, 0.001f), "1var_basic: sx≈1.5811");
    /* Pop stdev: sqrt(2) ≈ 1.4142 */
    CHECK(NEARF(r.sigma_x, 1.4142f, 0.001f), "1var_basic: sigma_x≈1.4142");
}

static void test_1var_single(void)
{
    printf("test_1var_single\n");
    StatData_t d = {0};
    StatResults_t r = {0};

    float xs[1] = {7.0f}, ys[1] = {0};
    fill_xy(&d, xs, ys, 1);
    CalcStat_Compute1Var(&d, &r);

    CHECK(r.valid,            "1var_single: valid");
    CHECK(NEAR(r.n, 1.0f),   "1var_single: n=1");
    CHECK(r.sx    == 0.0f,   "1var_single: sx=0");
    CHECK(r.sigma_x == 0.0f, "1var_single: sigma_x=0");
}

static void test_1var_empty(void)
{
    printf("test_1var_empty\n");
    StatData_t d = {0};
    StatResults_t r = {0};

    CalcStat_Compute1Var(&d, &r);
    CHECK(!r.valid, "1var_empty: not valid");
}

static void test_linreg_perfect(void)
{
    printf("test_linreg_perfect\n");
    /* y = 2x + 1 => a=2, b=1, r=1.0 */
    StatData_t d = {0};
    StatResults_t r = {0};
    float xs[3] = {1.0f, 2.0f, 3.0f};
    float ys[3] = {3.0f, 5.0f, 7.0f};
    fill_xy(&d, xs, ys, 3);

    bool ok = CalcStat_ComputeLinReg(&d, &r);

    CHECK(ok, "linreg_perfect: ok");
    CHECK(NEARF(r.reg_a, 2.0f, 0.001f), "linreg_perfect: a=2");
    CHECK(NEARF(r.reg_b, 1.0f, 0.001f), "linreg_perfect: b=1");
    CHECK(NEARF(r.reg_r, 1.0f, 0.001f), "linreg_perfect: r=1");
}

static void test_linreg_stores_vars(void)
{
    printf("test_linreg_stores_vars\n");
    StatData_t d = {0};
    StatResults_t r = {0};
    float xs[3] = {1.0f, 2.0f, 3.0f};
    float ys[3] = {3.0f, 5.0f, 7.0f};
    fill_xy(&d, xs, ys, 3);

    /* Clear calc_variables before the call */
    for (int i = 0; i < 26; i++) calc_variables[i] = 0.0f;
    CalcStat_ComputeLinReg(&d, &r);

    /* TI-81: a → variable A (index 0), b → variable B (index 1) */
    CHECK(NEARF(calc_variables[0], 2.0f, 0.001f), "linreg_vars: A=a=2");
    CHECK(NEARF(calc_variables[1], 1.0f, 0.001f), "linreg_vars: B=b=1");
}

static void test_linreg_degenerate(void)
{
    printf("test_linreg_degenerate\n");
    /* All X same — degenerate, should return false */
    StatData_t d = {0};
    StatResults_t r = {0};
    float xs[3] = {2.0f, 2.0f, 2.0f};
    float ys[3] = {1.0f, 3.0f, 5.0f};
    fill_xy(&d, xs, ys, 3);

    bool ok = CalcStat_ComputeLinReg(&d, &r);
    CHECK(!ok, "linreg_degenerate: returns false");
}

static void test_linreg_too_few(void)
{
    printf("test_linreg_too_few\n");
    StatData_t d = {0};
    StatResults_t r = {0};
    float xs[1] = {1.0f}, ys[1] = {2.0f};
    fill_xy(&d, xs, ys, 1);

    bool ok = CalcStat_ComputeLinReg(&d, &r);
    CHECK(!ok, "linreg_too_few: returns false for n=1");
}

static void test_lnreg(void)
{
    printf("test_lnreg\n");
    /* y = 1 + 2*ln(x) => a=2, b=1 */
    StatData_t d = {0};
    StatResults_t r = {0};
    float xs[4] = {1.0f, 2.0f, 4.0f, 8.0f};
    float ln_xs[4];
    for (int i = 0; i < 4; i++) ln_xs[i] = logf(xs[i]);
    /* y = 1 + 2*ln(x) */
    float ys[4];
    for (int i = 0; i < 4; i++) ys[i] = 1.0f + 2.0f * ln_xs[i];
    fill_xy(&d, xs, ys, 4);

    bool ok = CalcStat_ComputeLnReg(&d, &r);

    CHECK(ok, "lnreg: ok");
    CHECK(NEARF(r.reg_a, 2.0f, 0.01f), "lnreg: a=2");
    CHECK(NEARF(r.reg_b, 1.0f, 0.01f), "lnreg: b=1");
    CHECK(NEARF(r.reg_r, 1.0f, 0.01f), "lnreg: r=1");
}

static void test_expreg(void)
{
    printf("test_expreg\n");
    /* y = 2 * e^(0.5*x) => a=2, b=0.5 */
    StatData_t d = {0};
    StatResults_t r = {0};
    float xs[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    float ys[4];
    for (int i = 0; i < 4; i++) ys[i] = 2.0f * expf(0.5f * xs[i]);
    fill_xy(&d, xs, ys, 4);

    bool ok = CalcStat_ComputeExpReg(&d, &r);

    CHECK(ok, "expreg: ok");
    CHECK(NEARF(r.reg_a, 2.0f, 0.01f), "expreg: a=2");
    CHECK(NEARF(r.reg_b, 0.5f, 0.01f), "expreg: b=0.5");
    CHECK(NEARF(r.reg_r, 1.0f, 0.01f), "expreg: r=1");
}

static void test_sort_x(void)
{
    printf("test_sort_x\n");
    StatData_t d = {0};
    float xs[4] = {3.0f, 1.0f, 4.0f, 2.0f};
    float ys[4] = {30.0f, 10.0f, 40.0f, 20.0f};
    fill_xy(&d, xs, ys, 4);

    CalcStat_SortX(&d);

    CHECK(d.list_x[0] == 1.0f && d.list_y[0] == 10.0f, "sortx: row0 x=1 y=10");
    CHECK(d.list_x[1] == 2.0f && d.list_y[1] == 20.0f, "sortx: row1 x=2 y=20");
    CHECK(d.list_x[2] == 3.0f && d.list_y[2] == 30.0f, "sortx: row2 x=3 y=30");
    CHECK(d.list_x[3] == 4.0f && d.list_y[3] == 40.0f, "sortx: row3 x=4 y=40");
}

static void test_sort_y(void)
{
    printf("test_sort_y\n");
    StatData_t d = {0};
    float xs[4] = {30.0f, 10.0f, 40.0f, 20.0f};
    float ys[4] = {3.0f, 1.0f, 4.0f, 2.0f};
    fill_xy(&d, xs, ys, 4);

    CalcStat_SortY(&d);

    CHECK(d.list_y[0] == 1.0f && d.list_x[0] == 10.0f, "sorty: row0 y=1 x=10");
    CHECK(d.list_y[1] == 2.0f && d.list_x[1] == 20.0f, "sorty: row1 y=2 x=20");
    CHECK(d.list_y[2] == 3.0f && d.list_x[2] == 30.0f, "sorty: row2 y=3 x=30");
    CHECK(d.list_y[3] == 4.0f && d.list_x[3] == 40.0f, "sorty: row3 y=4 x=40");
}

static void test_clear(void)
{
    printf("test_clear\n");
    StatData_t d = {0};
    float xs[3] = {1.0f, 2.0f, 3.0f};
    float ys[3] = {4.0f, 5.0f, 6.0f};
    fill_xy(&d, xs, ys, 3);

    CalcStat_Clear(&d);

    CHECK(d.list_len == 0, "clear: list_len=0");
    CHECK(d.list_x[0] == 0.0f, "clear: list_x[0]=0");
    CHECK(d.list_y[0] == 0.0f, "clear: list_y[0]=0");
}

/* --- main ----------------------------------------------------------------- */

int main(void)
{
    printf("=== test_stat ===\n");

    test_1var_basic();
    test_1var_single();
    test_1var_empty();
    test_linreg_perfect();
    test_linreg_stores_vars();
    test_linreg_degenerate();
    test_linreg_too_few();
    test_lnreg();
    test_expreg();
    test_sort_x();
    test_sort_y();
    test_clear();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}
