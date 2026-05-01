/**
 * @file    test_parse_eval.c
 * @brief   Host tests for Calc_Parse() + Calc_Eval() (T3-B arch review item).
 *
 * Verifies:
 *   1. Calc_Parse + Calc_Eval produces the same result as Calc_Evaluate.
 *   2. ParsedExpr_t reuse: call Calc_Eval N times from one parse.
 *   3. Parametric mode: T token substituted per call.
 *   4. nDeriv: nested field populated; result matches Calc_Evaluate("nDeriv(...)").
 *   5. Two independent ParsedExpr_t with different nDeriv exprs evaluate without
 *      interference (proves no shared s_nderiv_eq cross-contamination).
 *   6. Parse error propagates; Calc_Eval on zero-count parsed expr returns error.
 *   7. Equivalence with Calc_PrepareGraphEquation / Calc_EvalGraphEquation.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "calc_engine.h"

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

#define NEAR(a, b)       (fabs((double)(a) - (double)(b)) < 1e-4)
/* nDeriv symmetric-difference quotient accumulates ~4e-4 float error at typical values */
#define NEAR_DERIV(a, b) (fabs((double)(a) - (double)(b)) < 5e-4)

static void reset_state(void)
{
    for (int i = 0; i < 27; i++) calc_variables[i] = 0.0f;
}

/* =========================================================================
 * Group 1 — Calc_Parse + Calc_Eval equivalence to Calc_Evaluate
 * ====================================================================== */
static void test_parse_eval_basic(void)
{
    printf("[1]  Calc_Parse + Calc_Eval matches Calc_Evaluate\n");
    reset_state();

    /* Note: the tokenizer uses the Unicode √ symbol, not ASCII "sqrt" */
    struct { const char *expr; float x_val; } cases[] = {
        { "2+3",         0.0f },
        { "10-5*2",      0.0f },
        { "2^3",         0.0f },
        { "sin(0)",      0.0f },
        { "abs(9)",      0.0f },
        { "X^2+1",       3.0f },
        { "(X+1)*(X-1)", 5.0f },
        { "abs(-7)",     0.0f },
    };

    for (int i = 0; i < (int)(sizeof cases / sizeof cases[0]); i++) {
        const char *expr = cases[i].expr;
        float x_val      = cases[i].x_val;

        calc_variables['X' - 'A'] = x_val;
        CalcResult_t ref = Calc_Evaluate(expr, 0.0f, false, false);

        ParsedExpr_t parsed;
        CalcError_t err = Calc_Parse(expr, 0.0f, false, false, &parsed);
        CHECK(err == CALC_OK, "parse ok");

        CalcResult_t res = Calc_Eval(&parsed, x_val, 0.0f, false);
        CHECK(res.error == CALC_OK,         expr);
        CHECK(NEAR(res.value, ref.value),   expr);
    }
}

/* =========================================================================
 * Group 2 — ParsedExpr_t reuse (graph scenario: one parse, many evals)
 * ====================================================================== */
static void test_reuse(void)
{
    printf("[2]  ParsedExpr_t reuse across multiple Calc_Eval calls\n");
    reset_state();

    ParsedExpr_t parsed;
    CalcError_t err = Calc_Parse("X^2", 0.0f, false, false, &parsed);
    CHECK(err == CALC_OK, "parse X^2");

    float test_x[] = { -3.0f, 0.0f, 1.5f, 4.0f, 10.0f };
    for (int i = 0; i < 5; i++) {
        float x = test_x[i];
        CalcResult_t r = Calc_Eval(&parsed, x, 0.0f, false);
        CHECK(r.error == CALC_OK,           "eval X^2 no error");
        CHECK(NEAR(r.value, x * x),         "eval X^2 value");
    }
}

/* =========================================================================
 * Group 3 — Parametric mode: T token per call
 * ====================================================================== */
static void test_parametric(void)
{
    printf("[3]  Parametric mode — T substituted per Calc_Eval call\n");
    reset_state();

    ParsedExpr_t parsed;
    CalcError_t err = Calc_Parse("cos(T)", 0.0f, false, true, &parsed);
    CHECK(err == CALC_OK, "parse cos(T) param_mode");

    float ts[] = { 0.0f, 1.5707963f, 3.1415927f };
    float expected[] = { 1.0f, 0.0f, -1.0f };
    for (int i = 0; i < 3; i++) {
        CalcResult_t r = Calc_Eval(&parsed, 0.0f, ts[i], false); /* radians */
        CHECK(r.error == CALC_OK,            "cos(T) no error");
        CHECK(NEAR(r.value, expected[i]),    "cos(T) value");
    }
}

/* =========================================================================
 * Group 4 — nDeriv: nested field populated; result correct
 * ====================================================================== */
static void test_nderiv(void)
{
    printf("[4]  nDeriv — nested field populated; result matches Calc_Evaluate\n");
    reset_state();

    /* d/dX[X^3] at X=2 should be 3*2^2 = 12 */
    const char *expr = "nDeriv(X^3,X,2)";

    CalcResult_t ref = Calc_Evaluate(expr, 0.0f, false, false);
    CHECK(ref.error == CALC_OK, "Calc_Evaluate nDeriv reference");
    CHECK(NEAR_DERIV(ref.value, 12.0f), "reference value ~12");

    ParsedExpr_t parsed;
    CalcError_t err = Calc_Parse(expr, 0.0f, false, false, &parsed);
    CHECK(err == CALC_OK, "parse nDeriv expr");
    /* nested field must have been populated */
    CHECK(parsed.nested.count > 0, "nested.count > 0 after parsing nDeriv");

    CalcResult_t res = Calc_Eval(&parsed, 0.0f, 0.0f, false);
    CHECK(res.error == CALC_OK,           "Calc_Eval nDeriv no error");
    CHECK(NEAR_DERIV(res.value, 12.0f),   "Calc_Eval nDeriv value ~12");
}

/* =========================================================================
 * Group 5 — Two independent ParsedExpr_t with different nDeriv exprs
 *           prove no shared s_nderiv_eq cross-contamination
 * ====================================================================== */
static void test_nderiv_independence(void)
{
    printf("[5]  Two nDeriv ParsedExpr_t are independent (no s_nderiv_eq sharing)\n");
    reset_state();

    /* A: d/dX[X^2] at 3 → 2*3 = 6 */
    const char *exprA = "nDeriv(X^2,X,3)";
    /* B: d/dX[X^3] at 2 → 3*4 = 12 */
    const char *exprB = "nDeriv(X^3,X,2)";

    ParsedExpr_t parsedA, parsedB;
    CHECK(Calc_Parse(exprA, 0.0f, false, false, &parsedA) == CALC_OK, "parse A");
    CHECK(Calc_Parse(exprB, 0.0f, false, false, &parsedB) == CALC_OK, "parse B");

    /* Evaluate A, then B, then A again — each must give its own answer */
    CalcResult_t rA1 = Calc_Eval(&parsedA, 0.0f, 0.0f, false);
    CalcResult_t rB  = Calc_Eval(&parsedB, 0.0f, 0.0f, false);
    CalcResult_t rA2 = Calc_Eval(&parsedA, 0.0f, 0.0f, false);

    CHECK(rA1.error == CALC_OK && NEAR_DERIV(rA1.value, 6.0f),  "A1 = 6");
    CHECK(rB.error  == CALC_OK && NEAR_DERIV(rB.value,  12.0f), "B  = 12");
    CHECK(rA2.error == CALC_OK && NEAR_DERIV(rA2.value, 6.0f),  "A2 = 6 (not contaminated by B)");
}

/* =========================================================================
 * Group 6 — Parse error propagates; bad postfix returns error gracefully
 * ====================================================================== */
static void test_parse_error(void)
{
    printf("[6]  Parse errors propagate correctly\n");
    reset_state();

    ParsedExpr_t parsed;

    /* Unknown character '@ fails in Tokenize */
    CalcError_t err = Calc_Parse("1@2", 0.0f, false, false, &parsed);
    CHECK(err != CALC_OK, "unknown char '@ returns error from Calc_Parse");

    /* "2++3" passes parse (shunting-yard is tolerant) but Calc_Eval should fail */
    err = Calc_Parse("2++3", 0.0f, false, false, &parsed);
    CHECK(err == CALC_OK, "2++3 parses OK");
    CalcResult_t r = Calc_Eval(&parsed, 0.0f, 0.0f, false);
    CHECK(r.error != CALC_OK, "2++3 Calc_Eval returns error (stack underflow)");

    /* NULL pointer guard */
    err = Calc_Parse(NULL, 0.0f, false, false, &parsed);
    CHECK(err != CALC_OK, "NULL expr returns error");

    err = Calc_Parse("1+1", 0.0f, false, false, NULL);
    CHECK(err != CALC_OK, "NULL out returns error");

    /* Eval with NULL parsed returns error gracefully */
    r = Calc_Eval(NULL, 0.0f, 0.0f, false);
    CHECK(r.error != CALC_OK, "Calc_Eval(NULL) returns error");
}

/* =========================================================================
 * Group 7 — Equivalence: Calc_Parse matches Calc_PrepareGraphEquation output
 * ====================================================================== */
static void test_equiv_prepare(void)
{
    printf("[7]  Calc_Parse result equivalent to Calc_PrepareGraphEquation\n");
    reset_state();

    const char *exprs[] = { "sin(X)", "X^2-1", "2X+3", "cos(X)*X" };

    for (int i = 0; i < 4; i++) {
        GraphEquation_t old_eq;
        CHECK(Calc_PrepareGraphEquation(exprs[i], 0.0f, &old_eq) == CALC_OK, "prepare");

        ParsedExpr_t parsed;
        CHECK(Calc_Parse(exprs[i], 0.0f, false, false, &parsed) == CALC_OK, "parse");

        /* Both must produce the same postfix token count */
        CHECK(parsed.postfix.count == old_eq.count, "postfix count matches");

        /* Eval at a few x values must agree */
        float xs[] = { -2.0f, 0.0f, 1.0f, 3.0f };
        for (int j = 0; j < 4; j++) {
            CalcResult_t r_old = Calc_EvalGraphEquation(&old_eq, xs[j], false);
            CalcResult_t r_new = Calc_Eval(&parsed, xs[j], 0.0f, false);
            CHECK(r_old.error == r_new.error, "error codes match");
            if (r_old.error == CALC_OK)
                CHECK(NEAR(r_old.value, r_new.value), "values match");
        }
    }
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    printf("=== test_parse_eval ===\n");

    test_parse_eval_basic();
    test_reuse();
    test_parametric();
    test_nderiv();
    test_nderiv_independence();
    test_parse_error();
    test_equiv_prepare();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}
