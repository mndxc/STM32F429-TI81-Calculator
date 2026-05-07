/**
 * @file    test_error_codes.c
 * @brief   Host tests for Calc_GetErrorString() and error_offset propagation.
 *
 * Build: cmake -S App/Tests -B build/tests && cmake --build build/tests
 * Run:  ./build/tests/test_error_codes
 */

#include <stdio.h>
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

/* =========================================================================
 * Group 1 — Calc_GetErrorString()
 * ====================================================================== */

static void test_error_strings(void)
{
    printf("[1]  Calc_GetErrorString() TI-81 format\n");

    CHECK(strcmp(Calc_GetErrorString(CALC_OK),                  "") == 0,
          "CALC_OK returns empty string");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_RESULT_OVERFLOW), "ERROR 01 MATH") == 0,
          "CALC_ERR_RESULT_OVERFLOW => ERROR 01 MATH");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_DIV_ZERO),        "ERROR 02 MATH") == 0,
          "CALC_ERR_DIV_ZERO => ERROR 02 MATH");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_IMAGINARY),       "ERROR 03 MATH") == 0,
          "CALC_ERR_IMAGINARY => ERROR 03 MATH");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_DOMAIN),          "ERROR 04 MATH") == 0,
          "CALC_ERR_DOMAIN => ERROR 04 MATH");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_MATRIX_OP),       "ERROR 05 MATH") == 0,
          "CALC_ERR_MATRIX_OP => ERROR 05 MATH");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_SYNTAX),          "ERROR 06 SYNTAX") == 0,
          "CALC_ERR_SYNTAX => ERROR 06 SYNTAX");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_OVERFLOW),        "ERROR 07 MEMORY") == 0,
          "CALC_ERR_OVERFLOW => ERROR 07 MEMORY");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_UNDEFINED),       "ERROR 08 MEMORY") == 0,
          "CALC_ERR_UNDEFINED => ERROR 08 MEMORY");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_RANGE),           "ERROR 11 RANGE") == 0,
          "CALC_ERR_RANGE => ERROR 11 RANGE");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_ZOOM),            "ERROR 12 ZOOM") == 0,
          "CALC_ERR_ZOOM => ERROR 12 ZOOM");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_BREAK),           "ERROR 13 BREAK") == 0,
          "CALC_ERR_BREAK => ERROR 13 BREAK");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_PRGM_NO_LABEL),   "ERROR 14 PRGM") == 0,
          "CALC_ERR_PRGM_NO_LABEL => ERROR 14 PRGM");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_PRGM_NESTING),    "ERROR 15 PRGM") == 0,
          "CALC_ERR_PRGM_NESTING => ERROR 15 PRGM");
    CHECK(strcmp(Calc_GetErrorString(CALC_ERR_INVALID),         "ERROR 16 INVALID") == 0,
          "CALC_ERR_INVALID => ERROR 16 INVALID");
}

/* =========================================================================
 * Group 2 — error_offset in CalcResult_t
 * ====================================================================== */

static void test_error_offset(void)
{
    printf("[2]  error_offset propagation\n");

    /* Successful evaluation: error_offset stays 0 */
    CalcResult_t r = Calc_Evaluate("1+2", 0.0f, false, true);
    CHECK(r.error == CALC_OK, "1+2 succeeds");
    CHECK(r.error_offset == 0, "no error => offset 0");

    /* Syntax error from unknown character '@' at position 2 ("1+@") */
    r = Calc_Evaluate("1+@", 0.0f, false, true);
    CHECK(r.error != CALC_OK, "1+@ fails");
    CHECK(r.error_offset == 2, "unknown char '@' at byte offset 2");

    /* Syntax error at start: '@' at position 0 */
    r = Calc_Evaluate("@+1", 0.0f, false, true);
    CHECK(r.error != CALC_OK, "@+1 fails");
    CHECK(r.error_offset == 0, "unknown char '@' at byte offset 0");

    /* Empty expression: no offset info (0) */
    r = Calc_Evaluate("", 0.0f, false, true);
    CHECK(r.error != CALC_OK, "empty expression fails");
    CHECK(r.error_offset == 0, "empty expression offset 0");

    /* Division by zero: eval-time error, offset stays 0 */
    r = Calc_Evaluate("1/0", 0.0f, false, true);
    CHECK(r.error == CALC_ERR_DIV_ZERO, "1/0 => CALC_ERR_DIV_ZERO");
    CHECK(r.error_offset == 0, "eval-time error offset is 0");
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void)
{
    printf("=== test_error_codes ===\n\n");

    test_error_strings();
    test_error_offset();

    int total = g_passed + g_failed;
    printf("\n=== Results: %d/%d passed", g_passed, total);
    if (g_failed > 0)
        printf(", %d FAILED", g_failed);
    printf(" ===\n");

    return (g_failed > 0) ? 1 : 0;
}
