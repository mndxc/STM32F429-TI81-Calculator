/**
 * @file    test_yvars.c
 * @brief   Host-compiled tests for Y-VARS calc_engine integration.
 *
 * Covers:
 *   1. Y₁–Y₄ with no registered equations → evaluate to 0
 *   2. Y₁ reference evaluates the registered equation at x_val
 *   3. Y₁ in a composite expression (Y₁+1, 2*Y₁)
 *   4. Y₂–Y₄ with independent equations
 *   5. Reentrancy guard — Y₁ equation that itself references Y₁ returns 0
 *      for the inner reference rather than infinite-looping
 *   6. Y₁ in Calc_EvaluateAt — correct x_val propagated
 *
 * Build and run (no ARM toolchain required):
 *   cmake -S App/Tests -B build-tests
 *   cmake --build build-tests
 *   ./build-tests/test_yvars
 *
 * Returns 0 on all pass, 1 on any failure.
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

/* -------------------------------------------------------------------------
 * Test equation storage (simulating graph_state.equations[][64])
 * ---------------------------------------------------------------------- */

#define EQ_SLOTS 4
static char s_eqs[EQ_SLOTS][64];

static void reset_eqs(void)
{
    for (int i = 0; i < EQ_SLOTS; i++)
        s_eqs[i][0] = '\0';
    for (int i = 0; i < 26; i++) calc_variables[i] = 0.0f;
}

/* =========================================================================
 * Group 1 — No registered equations → Y₁–Y₄ evaluate to 0
 * ====================================================================== */
static void test_unregistered(void)
{
    printf("[1]  Y₁–Y₄ unregistered (no equations set) → 0\n");
    Calc_RegisterYEquations(NULL, 0);

    CalcResult_t r;

    r = Calc_Evaluate("Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0), "Y1 unregistered = 0");

    r = Calc_Evaluate("Y\xE2\x82\x82", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0), "Y2 unregistered = 0");

    r = Calc_Evaluate("Y\xE2\x82\x83", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0), "Y3 unregistered = 0");

    r = Calc_Evaluate("Y\xE2\x82\x84", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0), "Y4 unregistered = 0");
}

/* =========================================================================
 * Group 2 — Registered constant equations
 * ====================================================================== */
static void test_constant_eq(void)
{
    printf("[2]  Y₁=5 registered constant equation\n");
    reset_eqs();
    strncpy(s_eqs[0], "5", 63);
    Calc_RegisterYEquations((const char (*)[64])s_eqs, EQ_SLOTS);

    CalcResult_t r;

    r = Calc_Evaluate("Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 5.0), "Y1 = 5");

    r = Calc_Evaluate("Y\xE2\x82\x81+1", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 6.0), "Y1+1 = 6");

    r = Calc_Evaluate("2*Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 10.0), "2*Y1 = 10");
}

/* =========================================================================
 * Group 3 — Y₁=X equation evaluated at stored X value
 * ====================================================================== */
static void test_x_substitution(void)
{
    printf("[3]  Y₁=X: evaluates at stored X variable\n");
    reset_eqs();
    strncpy(s_eqs[0], "X", 63);
    Calc_RegisterYEquations((const char (*)[64])s_eqs, EQ_SLOTS);

    /* Store X=3, then evaluate Y₁ — should give 3 */
    calc_variables['X' - 'A'] = 3.0f;
    CalcResult_t r = Calc_Evaluate("Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 3.0), "Y1=X, X=3 → 3");

    /* Y₁+2 = 5 */
    r = Calc_Evaluate("Y\xE2\x82\x81+2", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 5.0), "Y1+2 = 5 when X=3");

    /* Calc_EvaluateAt overrides X: Y₁ at X=7 should give 7 */
    r = Calc_EvaluateAt("Y\xE2\x82\x81", 7.0f, 0, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 7.0), "Y1=X via EvaluateAt(7) → 7");
}

/* =========================================================================
 * Group 4 — Multiple slots (Y₂, Y₃, Y₄ independent)
 * ====================================================================== */
static void test_multiple_slots(void)
{
    printf("[4]  Multiple equation slots\n");
    reset_eqs();
    strncpy(s_eqs[0], "2",     63);   /* Y1=2 */
    strncpy(s_eqs[1], "3",     63);   /* Y2=3 */
    strncpy(s_eqs[2], "4",     63);   /* Y3=4 */
    strncpy(s_eqs[3], "5",     63);   /* Y4=5 */
    Calc_RegisterYEquations((const char (*)[64])s_eqs, EQ_SLOTS);

    CalcResult_t r;

    r = Calc_Evaluate("Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 2.0), "Y1=2");

    r = Calc_Evaluate("Y\xE2\x82\x82", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 3.0), "Y2=3");

    r = Calc_Evaluate("Y\xE2\x82\x83", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 4.0), "Y3=4");

    r = Calc_Evaluate("Y\xE2\x82\x84", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 5.0), "Y4=5");

    r = Calc_Evaluate("Y\xE2\x82\x81+Y\xE2\x82\x82+Y\xE2\x82\x83+Y\xE2\x82\x84",
                      0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 14.0), "Y1+Y2+Y3+Y4 = 14");
}

/* =========================================================================
 * Group 5 — Y₁ empty slot evaluates to 0 without error
 * ====================================================================== */
static void test_empty_slot(void)
{
    printf("[5]  Empty equation slot evaluates to 0\n");
    reset_eqs();
    strncpy(s_eqs[0], "7", 63);   /* Y1=7 */
    /* s_eqs[1] left empty */
    Calc_RegisterYEquations((const char (*)[64])s_eqs, EQ_SLOTS);

    CalcResult_t r;

    r = Calc_Evaluate("Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 7.0), "Y1=7 (non-empty)");

    r = Calc_Evaluate("Y\xE2\x82\x82", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0), "Y2=empty → 0");
}

/* =========================================================================
 * Group 6 — Reentrancy guard: Y₁ self-reference returns 0 for inner call
 * ====================================================================== */
static void test_reentrancy_guard(void)
{
    printf("[6]  Reentrancy guard: Y₁=Y₁ inner reference returns 0\n");
    reset_eqs();
    /* Y₁ equation itself references Y₁ — inner call must not loop */
    strncpy(s_eqs[0], "Y\xE2\x82\x81+1", 63);   /* Y1 = Y1 + 1 */
    Calc_RegisterYEquations((const char (*)[64])s_eqs, EQ_SLOTS);

    /* Outer Y₁ calls EvaluateAt("Y₁+1", ...). Inner Y₁ hits depth guard,
     * evaluates to 0. So result should be 0+1 = 1. */
    CalcResult_t r = Calc_Evaluate("Y\xE2\x82\x81", 0, false, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0),
          "Y1=Y1+1: inner Y1→0, outer→0+1=1");
}

/* =========================================================================
 * Group 7 — Y₁ with non-trivial expression (sin(X))
 * ====================================================================== */
static void test_trig_equation(void)
{
    printf("[7]  Y₁=sin(X) via EvaluateAt\n");
    reset_eqs();
    strncpy(s_eqs[0], "sin(X)", 63);
    Calc_RegisterYEquations((const char (*)[64])s_eqs, EQ_SLOTS);

    /* sin(0) = 0 */
    CalcResult_t r = Calc_EvaluateAt("Y\xE2\x82\x81", 0.0f, 0, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 0.0), "Y1=sin(0)=0");

    /* sin(π/2) = 1 (radians) */
    r = Calc_EvaluateAt("Y\xE2\x82\x81", 3.14159265f / 2.0f, 0, false);
    CHECK(r.error == CALC_OK && NEAR(r.value, 1.0), "Y1=sin(pi/2)=1");
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    printf("=== test_yvars ===\n");

    test_unregistered();
    test_constant_eq();
    test_x_substitution();
    test_multiple_slots();
    test_empty_slot();
    test_reentrancy_guard();
    test_trig_equation();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
