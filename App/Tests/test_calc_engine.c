/**
 * @file    test_calc_engine.c
 * @brief   Host-compiled unit tests for the calc_engine module.
 *
 * Build and run (no ARM toolchain required):
 *   cmake -S App/Tests -B build/tests
 *   cmake --build build/tests
 *   ./build/tests/test_calc_engine
 *
 * Returns 0 on all pass, 1 on any failure.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "calc_engine.h"

/* calc_engine.c declares these as extern — define storage here. */
float        calc_variables[26];
CalcMatrix_t calc_matrices[CALC_MATRIX_COUNT];

/* -------------------------------------------------------------------------
 * Test infrastructure — soft-fail counter (does not abort on first failure)
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

/* Near-equality with 1e-4 tolerance for float results. */
#define NEAR(a, b)  (fabs((double)(a) - (double)(b)) < 1e-4)

/* -------------------------------------------------------------------------
 * Helper: reset all variables and matrices to zero/default before a group.
 * ---------------------------------------------------------------------- */
static void reset_state(void)
{
    for (int i = 0; i < 26; i++) calc_variables[i] = 0.0f;
    for (int m = 0; m < CALC_MATRIX_COUNT; m++) {
        calc_matrices[m].rows = 3;
        calc_matrices[m].cols = 3;
        for (int r = 0; r < CALC_MATRIX_MAX_DIM; r++)
            for (int c = 0; c < CALC_MATRIX_MAX_DIM; c++)
                calc_matrices[m].data[r][c] = 0.0f;
    }
    Calc_SetDecimalMode(0);
}

/* =========================================================================
 * Group 1 — Basic arithmetic
 * ====================================================================== */
static void test_arithmetic(void)
{
    printf("[1]  Basic arithmetic\n");
    CalcResult_t r;

    r = Calc_Evaluate("2+2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 4.0f, "2+2=4");

    /* Operator precedence: mul before add */
    r = Calc_Evaluate("10-5*2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "10-5*2=0 (mul before sub)");

    r = Calc_Evaluate("2+3*4", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 14.0f, "2+3*4=14 (mul before add)");

    /* Power before mul */
    r = Calc_Evaluate("2*3^2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 18.0f, "2*3^2=18 (pow before mul)");

    r = Calc_Evaluate("2^3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 8.0f, "2^3=8");

    r = Calc_Evaluate("9/3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 3.0f, "9/3=3");

    r = Calc_Evaluate("7/2", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 3.5f), "7/2=3.5");

    r = Calc_Evaluate("1+2+3+4", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 10.0f, "1+2+3+4=10");
}

/* =========================================================================
 * Group 2 — Negative numbers and unary minus
 * ====================================================================== */
static void test_negation(void)
{
    printf("[2]  Negative numbers / unary minus\n");
    CalcResult_t r;

    r = Calc_Evaluate("-3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == -3.0f, "-3=-3");

    /* TI-81 convention: unary minus has lower precedence than ^ */
    r = Calc_Evaluate("-3^2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == -9.0f, "-3^2=-9 (TI precedence)");

    /* Critical tokenizer edge case: '-' after '^' before digit is a negative literal */
    r = Calc_Evaluate("2^-3", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.125f), "2^-3=0.125");

    /* Parenthesised negative base — (-3)^2 should be 9 */
    r = Calc_Evaluate("(-3)^2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 9.0f, "(-3)^2=9");

    /* Unary minus after a binary operator */
    r = Calc_Evaluate("3*-2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == -6.0f, "3*-2=-6");
}

/* =========================================================================
 * Group 3 — Parentheses and nesting
 * ====================================================================== */
static void test_parentheses(void)
{
    printf("[3]  Parentheses\n");
    CalcResult_t r;

    r = Calc_Evaluate("(2+3)*4", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 20.0f, "(2+3)*4=20");

    r = Calc_Evaluate("2*(3+4)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 14.0f, "2*(3+4)=14");

    r = Calc_Evaluate("((2+3))*4", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 20.0f, "((2+3))*4=20 (double parens)");

    r = Calc_Evaluate("2*(3*(4+1))", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 30.0f, "2*(3*(4+1))=30 (nested)");
}

/* =========================================================================
 * Group 4 — Trig functions (radians)
 * ====================================================================== */
static void test_trig_radians(void)
{
    printf("[4]  Trig (radians)\n");
    CalcResult_t r;

    r = Calc_Evaluate("sin(0)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "sin(0)=0");

    r = Calc_Evaluate("cos(0)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "cos(0)=1");

    r = Calc_Evaluate("tan(0)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "tan(0)=0");

    /* sin(π/2) ≈ 1, cos(π/2) ≈ 0, tan(π/4) ≈ 1 */
    r = Calc_Evaluate("sin(1.5707963)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "sin(pi/2)~1");

    r = Calc_Evaluate("cos(1.5707963)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "cos(pi/2)~0");

    r = Calc_Evaluate("tan(0.7853982)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "tan(pi/4)~1");

    r = Calc_Evaluate("asin(1)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, (float)M_PI / 2.0f), "asin(1)~pi/2");

    r = Calc_Evaluate("acos(1)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "acos(1)~0");

    r = Calc_Evaluate("atan(1)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, (float)M_PI / 4.0f), "atan(1)~pi/4");
}

/* =========================================================================
 * Group 5 — Trig functions (degrees)
 * ====================================================================== */
static void test_trig_degrees(void)
{
    printf("[5]  Trig (degrees)\n");
    CalcResult_t r;

    r = Calc_Evaluate("sin(90)", 0, false, true);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "sin(90deg)=1");

    r = Calc_Evaluate("cos(180)", 0, false, true);
    CHECK(r.error == CALC_OK && NEAR(r.value, -1.0f), "cos(180deg)=-1");

    r = Calc_Evaluate("tan(45)", 0, false, true);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "tan(45deg)=1");

    r = Calc_Evaluate("asin(1)", 0, false, true);
    CHECK(r.error == CALC_OK && NEAR(r.value, 90.0f), "asin(1)=90 in degrees");

    r = Calc_Evaluate("acos(-1)", 0, false, true);
    CHECK(r.error == CALC_OK && NEAR(r.value, 180.0f), "acos(-1)=180 in degrees");
}

/* =========================================================================
 * Group 6 — Math functions
 * ====================================================================== */
static void test_math_functions(void)
{
    printf("[6]  Math functions\n");
    CalcResult_t r;

    /* sqrt is tokenized only via the √ glyph (U+221A = \xE2\x88\x9A) */
    r = Calc_Evaluate("\xE2\x88\x9A(9)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 3.0f), "sqrt(9)=3");

    r = Calc_Evaluate("\xE2\x88\x9A(2)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.41421f), "sqrt(2)~1.41421");

    r = Calc_Evaluate("abs(-5)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 5.0f, "abs(-5)=5");

    r = Calc_Evaluate("abs(3)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 3.0f, "abs(3)=3");

    r = Calc_Evaluate("ln(1)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "ln(1)=0");

    /* ln(e^1) = 1 — use exp(1) as e */
    r = Calc_Evaluate("ln(exp(1))", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "ln(exp(1))=1");

    r = Calc_Evaluate("log(100)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 2.0f), "log(100)=2");

    r = Calc_Evaluate("log(1)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "log(1)=0");

    /* round(value, #decimals) — 2-argument form */
    r = Calc_Evaluate("round(3.7,0)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 4.0f, "round(3.7,0)=4");

    r = Calc_Evaluate("round(3.14159,2)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 3.14f), "round(3.14159,2)=3.14");

    /* iPart = truncate toward zero */
    r = Calc_Evaluate("iPart(3.7)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 3.0f, "iPart(3.7)=3");

    r = Calc_Evaluate("iPart(-3.7)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == -3.0f, "iPart(-3.7)=-3 (toward zero)");

    /* fPart = fractional part, preserves sign */
    r = Calc_Evaluate("fPart(3.7)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.7f), "fPart(3.7)~0.7");

    r = Calc_Evaluate("fPart(-3.7)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, -0.7f), "fPart(-3.7)~-0.7");

    /* int = floor */
    r = Calc_Evaluate("int(3.7)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 3.0f, "int(3.7)=3 (floor)");

    r = Calc_Evaluate("int(-3.7)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == -4.0f, "int(-3.7)=-4 (floor, not truncate)");
}

/* =========================================================================
 * Group 7 — TEST comparison operators
 * UTF-8: ≠=\xE2\x89\xA0  ≥=\xE2\x89\xA5  ≤=\xE2\x89\xA4
 * ====================================================================== */
static void test_comparison_operators(void)
{
    printf("[7]  TEST comparison operators\n");
    CalcResult_t r;

    /* = */
    r = Calc_Evaluate("1=1", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "1=1 → 1");

    r = Calc_Evaluate("1=2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "1=2 → 0");

    /* ≠ (U+2260 = \xE2\x89\xA0) */
    r = Calc_Evaluate("2\xE2\x89\xA0" "1", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "neq: 2!=1 -> 1");

    r = Calc_Evaluate("1\xE2\x89\xA0" "1", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "neq: 1!=1 -> 0");

    /* > */
    r = Calc_Evaluate("3>2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "gt: 3>2 -> 1");

    r = Calc_Evaluate("2>3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "gt: 2>3 -> 0");

    /* ≥ (U+2265 = \xE2\x89\xA5) */
    r = Calc_Evaluate("3\xE2\x89\xA5" "3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "gte: 3>=3 -> 1");

    r = Calc_Evaluate("2\xE2\x89\xA5" "3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "gte: 2>=3 -> 0");

    /* < */
    r = Calc_Evaluate("2<3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "lt: 2<3 -> 1");

    r = Calc_Evaluate("3<2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "lt: 3<2 -> 0");

    /* ≤ (U+2264 = \xE2\x89\xA4) */
    r = Calc_Evaluate("3\xE2\x89\xA4" "3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "lte: 3<=3 -> 1");

    r = Calc_Evaluate("4\xE2\x89\xA4" "3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 0.0f, "lte: 4<=3 -> 0");
}

/* =========================================================================
 * Group 8 — Probability functions (nPr, nCr)
 * ====================================================================== */
static void test_probability(void)
{
    printf("[8]  Probability functions\n");
    CalcResult_t r;

    /* P(5,2) = 5!/3! = 20 */
    r = Calc_Evaluate("5 nPr 2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 20.0f, "5 nPr 2 = 20");

    /* C(5,2) = 10 */
    r = Calc_Evaluate("5 nCr 2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 10.0f, "5 nCr 2 = 10");

    r = Calc_Evaluate("1 nPr 1", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "1 nPr 1 = 1");

    r = Calc_Evaluate("5 nCr 0", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "5 nCr 0 = 1");

    r = Calc_Evaluate("5 nCr 5", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "5 nCr 5 = 1");

    /* n < r → domain error */
    r = Calc_Evaluate("2 nPr 5", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "2 nPr 5 → DOMAIN error");
}

/* =========================================================================
 * Group 9 — ANS substitution
 * ====================================================================== */
static void test_ans_substitution(void)
{
    printf("[9]  ANS substitution\n");
    CalcResult_t r;

    r = Calc_Evaluate("ANS", 42.0f, false, false);
    CHECK(r.error == CALC_OK && r.value == 42.0f, "ANS → 42");

    r = Calc_Evaluate("ANS+1", 42.0f, false, false);
    CHECK(r.error == CALC_OK && r.value == 43.0f, "ANS+1 → 43");

    r = Calc_Evaluate("ANS*2", 42.0f, false, false);
    CHECK(r.error == CALC_OK && r.value == 84.0f, "ANS*2 → 84");

    r = Calc_Evaluate("ANS^2", 3.0f, false, false);
    CHECK(r.error == CALC_OK && r.value == 9.0f, "ANS^2 → 9 (ans=3)");
}

/* =========================================================================
 * Group 10 — Variable substitution (A–Z)
 * ====================================================================== */
static void test_variables(void)
{
    printf("[10] Variable substitution\n");
    CalcResult_t r;

    calc_variables['A' - 'A'] = 5.0f;
    r = Calc_Evaluate("A", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 5.0f, "A → 5");

    r = Calc_Evaluate("A+3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 8.0f, "A+3 → 8");

    r = Calc_Evaluate("A*A", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 25.0f, "A*A → 25");

    calc_variables['X' - 'A'] = 2.0f;
    r = Calc_Evaluate("X^2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 4.0f, "X^2 → 4 (X=2)");
}

/* =========================================================================
 * Group 11 — Calc_EvaluateAt (graphing x-substitution)
 * ====================================================================== */
static void test_evaluate_at(void)
{
    printf("[11] Calc_EvaluateAt (x-substitution)\n");
    CalcResult_t r;

    r = Calc_EvaluateAt("2*X+1", 3.0f, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 7.0f), "2*X+1 at x=3 → 7");

    r = Calc_EvaluateAt("X^2", 4.0f, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 16.0f), "X^2 at x=4 → 16");

    r = Calc_EvaluateAt("sin(X)", 0.0f, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "sin(X) at x=0 → 0");

    r = Calc_EvaluateAt("X", -5.0f, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, -5.0f), "X at x=-5 → -5");

    /* Overrides whatever calc_variables['X'-'A'] holds */
    calc_variables['X' - 'A'] = 99.0f;
    r = Calc_EvaluateAt("X^2-1", 1.0f, 0.0f, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "X^2-1 at x=1 → 0 (overrides var)");
    calc_variables['X' - 'A'] = 0.0f;
}

/* =========================================================================
 * Group 12 — Error handling
 * ====================================================================== */
static void test_errors(void)
{
    printf("[12] Error handling\n");
    CalcResult_t r;

    r = Calc_Evaluate("1/0", 0, false, false);
    CHECK(r.error == CALC_ERR_DIV_ZERO, "1/0 → DIV_ZERO");

    r = Calc_Evaluate("\xE2\x88\x9A(-1)", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "sqrt(-1) -> DOMAIN");

    r = Calc_Evaluate("ln(-1)", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "ln(-1) → DOMAIN");

    r = Calc_Evaluate("log(-1)", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "log(-1) → DOMAIN");

    r = Calc_Evaluate("asin(2)", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "asin(2) → DOMAIN");

    r = Calc_Evaluate("2++2", 0, false, false);
    CHECK(r.error == CALC_ERR_SYNTAX, "2++2 → SYNTAX");

    r = Calc_Evaluate("(2+2", 0, false, false);
    CHECK(r.error == CALC_ERR_SYNTAX, "(2+2 → SYNTAX (unbalanced open)");

    r = Calc_Evaluate("2+2)", 0, false, false);
    CHECK(r.error == CALC_ERR_SYNTAX, "2+2) → SYNTAX (unbalanced close)");
}

/* =========================================================================
 * Group 13 — Calc_FormatResult
 * ====================================================================== */
static void test_format_result(void)
{
    printf("[13] Calc_FormatResult\n");
    char buf[32];

    Calc_SetDecimalMode(0); /* Float mode */

    /* Integers format without decimal point */
    Calc_FormatResult(3.0f, buf, sizeof(buf));
    CHECK(strcmp(buf, "3") == 0, "3.0 → \"3\"");

    Calc_FormatResult(0.0f, buf, sizeof(buf));
    CHECK(strcmp(buf, "0") == 0, "0.0 → \"0\"");

    Calc_FormatResult(-7.0f, buf, sizeof(buf));
    CHECK(strcmp(buf, "-7") == 0, "-7.0 → \"-7\"");

    /* Non-integers: trailing zeros trimmed */
    Calc_FormatResult(3.5f, buf, sizeof(buf));
    CHECK(strcmp(buf, "3.5") == 0, "3.5 → \"3.5\"");

    /* Value just below scientific threshold: 1234567 */
    Calc_FormatResult(1234567.0f, buf, sizeof(buf));
    CHECK(strcmp(buf, "1234567") == 0, "1234567.0 → \"1234567\"");

    /* Large value → scientific notation (%.4e = lowercase 'e') */
    Calc_FormatResult(12345678.0f, buf, sizeof(buf));
    CHECK(strchr(buf, 'e') != NULL, "12345678.0 → scientific notation");

    /* Tiny non-zero value → scientific notation */
    Calc_FormatResult(0.0000001f, buf, sizeof(buf));
    CHECK(strchr(buf, 'e') != NULL, "0.0000001 → scientific notation");

    /* Fix 2 mode: mode=3 → fix_decimals=2 */
    Calc_SetDecimalMode(3);
    Calc_FormatResult(3.14159f, buf, sizeof(buf));
    CHECK(strcmp(buf, "3.14") == 0, "3.14159 in Fix2 → \"3.14\"");

    Calc_FormatResult(3.0f, buf, sizeof(buf));
    CHECK(strcmp(buf, "3.00") == 0, "3.0 in Fix2 → \"3.00\"");

    Calc_SetDecimalMode(0); /* reset to Float */
}

/* =========================================================================
 * Group 14 — Matrix operations
 * Setup: [A]=[[1,2],[3,4]]  [B]=[[5,6],[7,8]]
 * ====================================================================== */
static void test_matrix_ops(void)
{
    printf("[14] Matrix operations\n");
    CalcResult_t r;

    /* Configure 2×2 matrices */
    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    calc_matrices[0].data[1][0] = 3.0f; calc_matrices[0].data[1][1] = 4.0f;

    calc_matrices[1].rows = 2; calc_matrices[1].cols = 2;
    calc_matrices[1].data[0][0] = 5.0f; calc_matrices[1].data[0][1] = 6.0f;
    calc_matrices[1].data[1][0] = 7.0f; calc_matrices[1].data[1][1] = 8.0f;

    /* det([A]) = 1*4 - 2*3 = -2 */
    r = Calc_Evaluate("det([A])", 0, false, false);
    CHECK(r.error == CALC_OK && !r.has_matrix && NEAR(r.value, -2.0f), "det([A])=-2");

    /* det([B]) = 5*8 - 6*7 = -2 */
    r = Calc_Evaluate("det([B])", 0, false, false);
    CHECK(r.error == CALC_OK && !r.has_matrix && NEAR(r.value, -2.0f), "det([B])=-2");

    /* [A]+[B] → [[6,8],[10,12]] stored in calc_matrices[3] */
    r = Calc_Evaluate("[A]+[B]", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "[A]+[B] → matrix result");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 6.0f)  && NEAR(m->data[0][1], 8.0f) &&
              NEAR(m->data[1][0], 10.0f) && NEAR(m->data[1][1], 12.0f),
              "[A]+[B] values: [[6,8],[10,12]]");
    } else {
        g_failed++;
        printf("  FAIL [line %d]: [A]+[B] produced invalid matrix_idx\n", __LINE__);
    }

    /* [A]*[B] → [[19,22],[43,50]] */
    r = Calc_Evaluate("[A]*[B]", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "[A]*[B] → matrix result");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 19.0f) && NEAR(m->data[0][1], 22.0f) &&
              NEAR(m->data[1][0], 43.0f) && NEAR(m->data[1][1], 50.0f),
              "[A]*[B] values: [[19,22],[43,50]]");
    } else {
        g_failed++;
        printf("  FAIL [line %d]: [A]*[B] produced invalid matrix_idx\n", __LINE__);
    }

    /* Transpose of [A]: [[1,3],[2,4]] */
    r = Calc_Evaluate("[A]T", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "[A]T → matrix result");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 1.0f) && NEAR(m->data[0][1], 3.0f) &&
              NEAR(m->data[1][0], 2.0f) && NEAR(m->data[1][1], 4.0f),
              "[A]T values: [[1,3],[2,4]]");
    } else {
        g_failed++;
        printf("  FAIL [line %d]: [A]T produced invalid matrix_idx\n", __LINE__);
    }
}

/* =========================================================================
 * Group 15 — Matrix row operations
 * Setup: [A]=[[1,2],[3,4]] (2×2)
 * ====================================================================== */
static void test_matrix_row_ops(void)
{
    printf("[15] Matrix row operations\n");
    CalcResult_t r;

    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    calc_matrices[0].data[1][0] = 3.0f; calc_matrices[0].data[1][1] = 4.0f;

    /* rowSwap([A],1,2) → rows 1 and 2 swapped → [[3,4],[1,2]] */
    r = Calc_Evaluate("rowSwap([A],1,2)", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "rowSwap([A],1,2) → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 3.0f) && NEAR(m->data[0][1], 4.0f) &&
              NEAR(m->data[1][0], 1.0f) && NEAR(m->data[1][1], 2.0f),
              "rowSwap values: [[3,4],[1,2]]");
    } else { g_failed++; printf("  FAIL: rowSwap invalid result\n"); }

    /* row+([A],1,2) → row1 += row2 → [[1+3, 2+4],[3,4]] = [[4,6],[3,4]] */
    r = Calc_Evaluate("row+([A],1,2)", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "row+([A],1,2) → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 4.0f) && NEAR(m->data[0][1], 6.0f) &&
              NEAR(m->data[1][0], 3.0f) && NEAR(m->data[1][1], 4.0f),
              "row+ values: [[4,6],[3,4]]");
    } else { g_failed++; printf("  FAIL: row+ invalid result\n"); }

    /* *row(2,[A],1) → row1 *= 2 → [[2,4],[3,4]] */
    r = Calc_Evaluate("*row(2,[A],1)", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "*row(2,[A],1) → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 2.0f) && NEAR(m->data[0][1], 4.0f) &&
              NEAR(m->data[1][0], 3.0f) && NEAR(m->data[1][1], 4.0f),
              "*row values: [[2,4],[3,4]]");
    } else { g_failed++; printf("  FAIL: *row invalid result\n"); }

    /* *row+(2,[A],1,2) → row1 += 2*row2 → [[1+6, 2+8],[3,4]] = [[7,10],[3,4]] */
    r = Calc_Evaluate("*row+(2,[A],1,2)", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "*row+(2,[A],1,2) → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 7.0f) && NEAR(m->data[0][1], 10.0f) &&
              NEAR(m->data[1][0], 3.0f) && NEAR(m->data[1][1], 4.0f),
              "*row+ values: [[7,10],[3,4]]");
    } else { g_failed++; printf("  FAIL: *row+ invalid result\n"); }

    /* Out-of-range row index → CALC_ERR_DOMAIN */
    r = Calc_Evaluate("rowSwap([A],1,5)", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "rowSwap out-of-range row → DOMAIN");

    r = Calc_Evaluate("row+([A],1,5)", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "row+ out-of-range row → DOMAIN");
}

/* =========================================================================
 * Group 16 — Matrix subtraction and scalar scaling
 * Setup: [A]=[[1,2],[3,4]]  [B]=[[5,6],[7,8]]  [C]=3×3 (default)
 * ====================================================================== */
static void test_matrix_sub_scale(void)
{
    printf("[16] Matrix subtraction and scalar scaling\n");
    CalcResult_t r;

    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    calc_matrices[0].data[1][0] = 3.0f; calc_matrices[0].data[1][1] = 4.0f;

    calc_matrices[1].rows = 2; calc_matrices[1].cols = 2;
    calc_matrices[1].data[0][0] = 5.0f; calc_matrices[1].data[0][1] = 6.0f;
    calc_matrices[1].data[1][0] = 7.0f; calc_matrices[1].data[1][1] = 8.0f;

    /* [A]-[B] → [[-4,-4],[-4,-4]] */
    r = Calc_Evaluate("[A]-[B]", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "[A]-[B] → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], -4.0f) && NEAR(m->data[0][1], -4.0f) &&
              NEAR(m->data[1][0], -4.0f) && NEAR(m->data[1][1], -4.0f),
              "[A]-[B] values: [[-4,-4],[-4,-4]]");
    } else { g_failed++; printf("  FAIL: [A]-[B] invalid result\n"); }

    /* 2*[A] → [[2,4],[6,8]] */
    r = Calc_Evaluate("2*[A]", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "2*[A] → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 2.0f) && NEAR(m->data[0][1], 4.0f) &&
              NEAR(m->data[1][0], 6.0f) && NEAR(m->data[1][1], 8.0f),
              "2*[A] values: [[2,4],[6,8]]");
    } else { g_failed++; printf("  FAIL: 2*[A] invalid result\n"); }

    /* [A]*3 → [[3,6],[9,12]] */
    r = Calc_Evaluate("[A]*3", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "[A]*3 → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 3.0f)  && NEAR(m->data[0][1], 6.0f) &&
              NEAR(m->data[1][0], 9.0f)  && NEAR(m->data[1][1], 12.0f),
              "[A]*3 values: [[3,6],[9,12]]");
    } else { g_failed++; printf("  FAIL: [A]*3 invalid result\n"); }

    /* Dimension mismatch: [A] is 2×2, [C] is 3×3 → CALC_ERR_DOMAIN */
    r = Calc_Evaluate("[A]+[C]", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "[A]+[C] dimension mismatch → DOMAIN");
}

/* =========================================================================
 * Group 17 — ANS as matrix reference (ans_is_matrix=true)
 * ====================================================================== */
static void test_ans_matrix(void)
{
    printf("[17] ANS as matrix reference\n");
    CalcResult_t r;

    /* Set up [A] = [[1,2],[3,4]] and ANS slot = 2×2 identity */
    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    calc_matrices[0].data[1][0] = 3.0f; calc_matrices[0].data[1][1] = 4.0f;

    calc_matrices[3].rows = 2; calc_matrices[3].cols = 2;
    calc_matrices[3].data[0][0] = 1.0f; calc_matrices[3].data[0][1] = 0.0f;
    calc_matrices[3].data[1][0] = 0.0f; calc_matrices[3].data[1][1] = 1.0f;

    /* det(ANS) — ANS = 2×2 identity, det = 1 */
    r = Calc_Evaluate("det(ANS)", 3.0f, true, false);
    CHECK(r.error == CALC_OK && !r.has_matrix && NEAR(r.value, 1.0f),
          "det(ANS) with ans_is_matrix=true → 1");

    /* ANS+[A] — identity + [[1,2],[3,4]] = [[2,2],[3,5]] */
    /* Reset ANS slot to identity before this test */
    calc_matrices[3].data[0][0] = 1.0f; calc_matrices[3].data[0][1] = 0.0f;
    calc_matrices[3].data[1][0] = 0.0f; calc_matrices[3].data[1][1] = 1.0f;

    r = Calc_Evaluate("ANS+[A]", 3.0f, true, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "ANS+[A] → matrix result");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 2.0f) && NEAR(m->data[0][1], 2.0f) &&
              NEAR(m->data[1][0], 3.0f) && NEAR(m->data[1][1], 5.0f),
              "ANS+[A] values: [[2,2],[3,5]]");
    } else { g_failed++; printf("  FAIL: ANS+[A] invalid result\n"); }

    /* [A]T via ANS — transpose the identity */
    calc_matrices[3].data[0][0] = 1.0f; calc_matrices[3].data[0][1] = 0.0f;
    calc_matrices[3].data[1][0] = 0.0f; calc_matrices[3].data[1][1] = 1.0f;

    r = Calc_Evaluate("ANST", 3.0f, true, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "ANST (transpose of ANS) → matrix");
}

/* =========================================================================
 * Group 18 — round([M], n): element-wise matrix rounding
 * Setup: [A]=[[1.4, 2.6],[3.14, 0.05]]
 * ====================================================================== */
static void test_matrix_round(void)
{
    printf("[18] round([M], n) element-wise\n");
    CalcResult_t r;

    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.4f;  calc_matrices[0].data[0][1] = 2.6f;
    calc_matrices[0].data[1][0] = 3.14f; calc_matrices[0].data[1][1] = 0.05f;

    /* round([A],0) → [[1,3],[3,0]] */
    r = Calc_Evaluate("round([A],0)", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "round([A],0) → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 1.0f) && NEAR(m->data[0][1], 3.0f) &&
              NEAR(m->data[1][0], 3.0f) && NEAR(m->data[1][1], 0.0f),
              "round([A],0) values: [[1,3],[3,0]]");
    } else { g_failed++; printf("  FAIL: round([A],0) invalid result\n"); }

    /* round([A],1) → [[1.4,2.6],[3.1,0.1]] */
    r = Calc_Evaluate("round([A],1)", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "round([A],1) → matrix");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 1.4f) && NEAR(m->data[0][1], 2.6f) &&
              NEAR(m->data[1][0], 3.1f) && NEAR(m->data[1][1], 0.1f),
              "round([A],1) values: [[1.4,2.6],[3.1,0.1]]");
    } else { g_failed++; printf("  FAIL: round([A],1) invalid result\n"); }
}

/* =========================================================================
 * Group 19 — Boundary and edge cases
 * ====================================================================== */
static void test_boundary(void)
{
    printf("[19] Boundary and edge cases\n");
    CalcResult_t r;

    /* det of non-square matrix → CALC_ERR_DOMAIN */
    calc_matrices[0].rows = 1; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    r = Calc_Evaluate("det([A])", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "det(non-square) → DOMAIN");

    /* Factorial: 0! = 1 */
    r = Calc_Evaluate("0!", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "0! = 1");

    /* Factorial: 5! = 120 */
    r = Calc_Evaluate("5!", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 120.0f, "5! = 120");

    /* Factorial of negative (via parenthesis) → DOMAIN */
    r = Calc_Evaluate("(-1)!", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "(-1)! → DOMAIN");

    /* Factorial: 10! = 3628800 */
    r = Calc_Evaluate("10!", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 3628800.0f), "10! = 3628800");

    /* mat_mul dimension mismatch: [A](1×2) * [B](2×2) is OK; [B](2×2) * [A](1×2) fails */
    calc_matrices[1].rows = 2; calc_matrices[1].cols = 2;
    calc_matrices[1].data[0][0] = 1.0f; calc_matrices[1].data[1][1] = 1.0f;
    r = Calc_Evaluate("[B]*[A]", 0, false, false); /* 2×2 * 1×2: cols(B)=2 != rows(A)=1 */
    CHECK(r.error == CALC_ERR_DOMAIN, "[B](2×2)*[A](1×2) dimension mismatch → DOMAIN");

    /* nCr with r > n (boundary: exactly r==n is valid, r==n+1 is not) */
    r = Calc_Evaluate("3 nCr 3", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "3 nCr 3 = 1 (boundary)");

    r = Calc_Evaluate("3 nCr 4", 0, false, false);
    CHECK(r.error == CALC_ERR_DOMAIN, "3 nCr 4 → DOMAIN (r > n)");

    /* Singleton matrix: det([[k]]) = k */
    calc_matrices[0].rows = 1; calc_matrices[0].cols = 1;
    calc_matrices[0].data[0][0] = 7.0f;
    r = Calc_Evaluate("det([A])", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 7.0f), "det([[7]]) = 7");

    /* Singular matrix: det = 0 */
    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    calc_matrices[0].data[1][0] = 2.0f; calc_matrices[0].data[1][1] = 4.0f;
    r = Calc_Evaluate("det([A])", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "det(singular) = 0");
}

/* =========================================================================
 * Group 20 — Tokenizer coverage: pi constant, e constant,
 *            implicit multiplication, T-after-paren
 * ====================================================================== */
static void test_tokenizer_coverage(void)
{
    printf("[20] Tokenizer coverage (pi, e, implicit mul, T-after-paren)\n");
    CalcResult_t r;

    /* pi constant (ASCII "pi" keyword) */
    r = Calc_Evaluate("pi/pi", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "pi/pi = 1");

    r = Calc_Evaluate("round(sin(pi),0)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "round(sin(pi),0) = 0");

    /* e constant (bare Euler's number) */
    r = Calc_Evaluate("ln(e)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "ln(e) = 1");

    r = Calc_Evaluate("e>2", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 1.0f, "e > 2 → 1");

    /* Implicit multiplication: number adjacent to number (2pi) */
    r = Calc_Evaluate("2pi", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 6.28318f), "2pi = 2*pi");

    /* Implicit multiplication: number adjacent to function (2sin(0)) */
    r = Calc_Evaluate("2sin(0)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0f), "2sin(0) = 0");

    /* Implicit multiplication: number adjacent to paren (2(3+1)) */
    r = Calc_Evaluate("2(3+1)", 0, false, false);
    CHECK(r.error == CALC_OK && r.value == 8.0f, "2(3+1) = 8");

    /* UTF-8 π (0xCF 0x80) — separate from ASCII "pi" keyword */
    r = Calc_Evaluate("\xcf\x80/\xcf\x80", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0f), "UTF-8 pi/pi = 1");

    /* rand — evaluates at tokenize time to a value in [0, 1) */
    r = Calc_Evaluate("rand", 0, false, false);
    CHECK(r.error == CALC_OK && r.value >= 0.0f && r.value < 1.0f,
          "rand in [0, 1)");

    r = Calc_Evaluate("rand*0+5", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 5.0f), "rand*0+5 = 5");

    /* 2^-.5 — '-' after '^' before '.' (decimal negative literal) */
    r = Calc_Evaluate("4^-.5", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.5f), "4^-.5 = 0.5");

    /* Operator between ( and , in a function call — exercises ShuntingYard
       COMMA handler loop body (pop operator before pushing output for arg 2) */
    r = Calc_Evaluate("round(3+0.7,0)", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 4.0f), "round(3+0.7,0) = 4");

    /* Comma outside a function call → CALC_ERR_SYNTAX */
    r = Calc_Evaluate("1,2", 0, false, false);
    CHECK(r.error == CALC_ERR_SYNTAX, "1,2 (comma outside func) → SYNTAX");

    /* T (transpose) immediately after closing paren — ([A])T */
    calc_matrices[0].rows = 2; calc_matrices[0].cols = 2;
    calc_matrices[0].data[0][0] = 1.0f; calc_matrices[0].data[0][1] = 2.0f;
    calc_matrices[0].data[1][0] = 3.0f; calc_matrices[0].data[1][1] = 4.0f;

    r = Calc_Evaluate("([A])T", 0, false, false);
    CHECK(r.error == CALC_OK && r.has_matrix, "([A])T → matrix result");
    if (r.has_matrix && r.matrix_idx < CALC_MATRIX_COUNT) {
        CalcMatrix_t *m = &calc_matrices[r.matrix_idx];
        CHECK(NEAR(m->data[0][0], 1.0f) && NEAR(m->data[0][1], 3.0f) &&
              NEAR(m->data[1][0], 2.0f) && NEAR(m->data[1][1], 4.0f),
              "([A])T values: [[1,3],[2,4]]");
    } else { g_failed++; printf("  FAIL: ([A])T invalid result\n"); }
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    printf("=== calc_engine test suite ===\n\n");

    reset_state();
    test_arithmetic();

    reset_state();
    test_negation();

    reset_state();
    test_parentheses();

    reset_state();
    test_trig_radians();

    reset_state();
    test_trig_degrees();

    reset_state();
    test_math_functions();

    reset_state();
    test_comparison_operators();

    reset_state();
    test_probability();

    reset_state();
    test_ans_substitution();

    reset_state();
    test_variables();

    reset_state();
    test_evaluate_at();

    reset_state();
    test_errors();

    reset_state();
    test_format_result();

    reset_state();
    test_matrix_ops();

    reset_state();
    test_matrix_row_ops();

    reset_state();
    test_matrix_sub_scale();

    reset_state();
    test_ans_matrix();

    reset_state();
    test_matrix_round();

    reset_state();
    test_boundary();

    reset_state();
    test_tokenizer_coverage();

    int total = g_passed + g_failed;
    printf("\n=== Results: %d/%d passed", g_passed, total);
    if (g_failed > 0)
        printf(", %d FAILED", g_failed);
    printf(" ===\n");

    return (g_failed > 0) ? 1 : 0;
}
