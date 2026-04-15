/**
 * @file    test_param.c
 * @brief   Host-compiled unit tests for parametric equation preparation and evaluation.
 *
 * Exercises Calc_PrepareParamEquation() and Calc_EvalParamEquation() from
 * calc_engine.c.  No LVGL, HAL, or RTOS dependencies — pure C.
 *
 * Build and run:
 *   cmake -S App/Tests -B build-tests && cmake --build build-tests
 *   ctest --test-dir build-tests -R param
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "calc_engine.h"

/* -------------------------------------------------------------------------
 * Test infrastructure
 * ---------------------------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name) do {                                          \
    if (cond) {                                                         \
        g_passed++;                                                     \
    } else {                                                            \
        g_failed++;                                                     \
        printf("  FAIL [line %d]: %s\n", __LINE__, (name));            \
    }                                                                   \
} while (0)

#define NEAR(a, b)  (fabs((double)(a) - (double)(b)) < 1e-4)

static void reset_state(void)
{
    for (int i = 0; i < 26; i++) calc_variables[i] = 0.0f;
    Calc_SetDecimalMode(0);
}

/* =========================================================================
 * Group 1 — Prepare: T tokenised as MATH_VAR_T, not variable T
 * ====================================================================== */
static void test_prepare(void)
{
    printf("[1]  Prepare parametric equation\n");
    GraphEquation_t eq;

    /* Linear: T itself */
    CalcError_t err = Calc_PrepareParamEquation("T", 0.0f, &eq);
    CHECK(err == CALC_OK, "prepare 'T' succeeds");
    CHECK(eq.count > 0, "prepare 'T' emits tokens");

    /* Constant expression (no T) */
    err = Calc_PrepareParamEquation("3+4", 0.0f, &eq);
    CHECK(err == CALC_OK, "prepare '3+4' succeeds");

    /* Trig of T */
    err = Calc_PrepareParamEquation("sin(T)", 0.0f, &eq);
    CHECK(err == CALC_OK, "prepare 'sin(T)' succeeds");

    /* Invalid expression */
    err = Calc_PrepareParamEquation("(+", 0.0f, &eq);
    CHECK(err != CALC_OK, "prepare '(+' fails");

    /* Implicit multiply: 2T */
    err = Calc_PrepareParamEquation("2T", 0.0f, &eq);
    CHECK(err == CALC_OK, "prepare '2T' succeeds (implicit mul)");
}

/* =========================================================================
 * Group 2 — Eval: T substituted correctly
 * ====================================================================== */
static void test_eval_linear(void)
{
    printf("[2]  Eval linear parametric (X(t) = T)\n");
    GraphEquation_t eq;

    Calc_PrepareParamEquation("T", 0.0f, &eq);

    CalcResult_t r;

    r = Calc_EvalParamEquation(&eq, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "T at t=0 → 0");

    r = Calc_EvalParamEquation(&eq, 1.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "T at t=1 → 1");

    r = Calc_EvalParamEquation(&eq, -3.14f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, -3.14f), "T at t=-3.14 → -3.14");

    r = Calc_EvalParamEquation(&eq, 100.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 100.0f), "T at t=100 → 100");
}

/* =========================================================================
 * Group 3 — Eval: circle parametric (cos(T), sin(T))
 * ====================================================================== */
static void test_eval_circle(void)
{
    printf("[3]  Eval circle parametric (cos(T) / sin(T))\n");
    GraphEquation_t eq_x, eq_y;

    Calc_PrepareParamEquation("cos(T)", 0.0f, &eq_x);
    Calc_PrepareParamEquation("sin(T)", 0.0f, &eq_y);

    CalcResult_t rx, ry;

    /* t=0: (1, 0) */
    rx = Calc_EvalParamEquation(&eq_x, 0.0f, false);
    ry = Calc_EvalParamEquation(&eq_y, 0.0f, false);
    CHECK(rx.error == CALC_OK && NEAR(rx.value, 1.0f), "cos(0)=1");
    CHECK(ry.error == CALC_OK && NEAR(ry.value, 0.0f), "sin(0)=0");

    /* t=π/2: (0, 1) */
    float pi2 = (float)(M_PI / 2.0);
    rx = Calc_EvalParamEquation(&eq_x, pi2, false);
    ry = Calc_EvalParamEquation(&eq_y, pi2, false);
    CHECK(rx.error == CALC_OK && NEAR(rx.value, 0.0f), "cos(π/2)≈0");
    CHECK(ry.error == CALC_OK && NEAR(ry.value, 1.0f), "sin(π/2)=1");

    /* t=π: (-1, 0) */
    float pi = (float)M_PI;
    rx = Calc_EvalParamEquation(&eq_x, pi, false);
    ry = Calc_EvalParamEquation(&eq_y, pi, false);
    CHECK(rx.error == CALC_OK && NEAR(rx.value, -1.0f), "cos(π)=-1");
    CHECK(ry.error == CALC_OK && NEAR(ry.value, 0.0f), "sin(π)≈0");

    /* Identity: cos²(T)+sin²(T)=1 at arbitrary t */
    float t = 1.23f;
    rx = Calc_EvalParamEquation(&eq_x, t, false);
    ry = Calc_EvalParamEquation(&eq_y, t, false);
    float sq_sum = rx.value * rx.value + ry.value * ry.value;
    CHECK(rx.error == CALC_OK && ry.error == CALC_OK && NEAR(sq_sum, 1.0f),
          "cos²(t)+sin²(t)=1 for t=1.23");
}

/* =========================================================================
 * Group 4 — Eval: scaled and offset (2T+1)
 * ====================================================================== */
static void test_eval_linear_scaled(void)
{
    printf("[4]  Eval scaled/offset parametric (2T+1)\n");
    GraphEquation_t eq;

    Calc_PrepareParamEquation("2T+1", 0.0f, &eq);

    CalcResult_t r;

    r = Calc_EvalParamEquation(&eq, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "2*0+1=1");

    r = Calc_EvalParamEquation(&eq, 3.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 7.0f), "2*3+1=7");

    r = Calc_EvalParamEquation(&eq, -1.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, -1.0f), "2*(-1)+1=-1");
}

/* =========================================================================
 * Group 5 — T does not clobber stored variable 'T'
 * ====================================================================== */
static void test_t_var_independence(void)
{
    printf("[5]  T variable independence from stored calc_variables['T'-'A']\n");

    /* Set the stored T variable to a known value */
    calc_variables['T' - 'A'] = 999.0f;

    GraphEquation_t eq;
    Calc_PrepareParamEquation("T", 0.0f, &eq);

    /* Eval with t_val=5 — should use 5, not 999 */
    CalcResult_t r = Calc_EvalParamEquation(&eq, 5.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 5.0f),
          "T resolves to t_val=5, ignores stored calc_variables['T'-'A']=999");

    /* Stored variable should be unchanged */
    CHECK(NEAR(calc_variables['T' - 'A'], 999.0f),
          "calc_variables['T'-'A'] unchanged after eval");

    calc_variables['T' - 'A'] = 0.0f;
}

/* =========================================================================
 * Group 6 — X variable still works inside parametric equation
 * ====================================================================== */
static void test_x_in_param(void)
{
    printf("[6]  X variable usable in parametric expression\n");

    calc_variables['X' - 'A'] = 4.0f;

    GraphEquation_t eq;
    /* Expression: T + X  — T is substituted by t_val; X from calc_variables */
    Calc_PrepareParamEquation("T+X", 0.0f, &eq);

    CalcResult_t r = Calc_EvalParamEquation(&eq, 2.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 6.0f),
          "T+X at t=2, X=4 → 6");

    calc_variables['X' - 'A'] = 0.0f;
}

/* =========================================================================
 * Group 7 — Degrees mode for trig of T
 * ====================================================================== */
static void test_degrees_mode(void)
{
    printf("[7]  Degrees mode in parametric trig\n");
    GraphEquation_t eq;

    Calc_PrepareParamEquation("sin(T)", 0.0f, &eq);

    /* sin(90°) = 1 in degrees mode */
    CalcResult_t r = Calc_EvalParamEquation(&eq, 90.0f, true);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "sin(90°)=1 in degrees mode");

    /* sin(90) in radians ≈ 0.8940 */
    r = Calc_EvalParamEquation(&eq, 90.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, sinf(90.0f)), "sin(90 rad) in radians mode");
}

/* =========================================================================
 * Group 8 — Constant expression (no T) behaves like normal expression
 * ====================================================================== */
static void test_constant_expr(void)
{
    printf("[8]  Constant expression in parametric prepare/eval\n");
    GraphEquation_t eq;

    Calc_PrepareParamEquation("3*4", 0.0f, &eq);

    /* t_val is irrelevant for a constant */
    CalcResult_t r = Calc_EvalParamEquation(&eq, 99.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 12.0f),
          "3*4=12 regardless of t_val");
}

/* =========================================================================
 * Group 9 — Error propagation
 * ====================================================================== */
static void test_error_propagation(void)
{
    printf("[9]  Error propagation in parametric eval\n");
    GraphEquation_t eq;

    /* sqrt of a negative T value at runtime.
     * Use the Unicode √ glyph (\xE2\x88\x9A) — the tokenizer maps this to
     * MATH_FUNC_SQRT; the ASCII string "sqrt" is not in the function table. */
    Calc_PrepareParamEquation("\xE2\x88\x9A(T)", 0.0f, &eq);
    CalcResult_t r = Calc_EvalParamEquation(&eq, -1.0f, false);
    CHECK(r.error != CALC_OK, "sqrt(-1) \xe2\x86\x92 error at eval time");

    /* Division by zero via T */
    Calc_PrepareParamEquation("1/T", 0.0f, &eq);
    r = Calc_EvalParamEquation(&eq, 0.0f, false);
    CHECK(r.error != CALC_OK, "1/0 → error at eval time");
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    reset_state();

    test_prepare();
    test_eval_linear();
    test_eval_circle();
    test_eval_linear_scaled();
    test_t_var_independence();
    test_x_in_param();
    test_degrees_mode();
    test_constant_expr();
    test_error_propagation();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
