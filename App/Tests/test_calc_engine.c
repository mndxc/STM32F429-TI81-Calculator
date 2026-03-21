#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "calc_engine.h"

// Mock some variables that might be needed
float calc_variables[26];
CalcMatrix_t calc_matrices[CALC_MATRIX_COUNT];

void test_basic_arithmetic() {
    printf("Testing basic arithmetic... ");
    CalcResult_t res = Calc_Evaluate("2+2", 0, false, false);
    assert(res.error == CALC_OK);
    assert(res.value == 4.0f);

    res = Calc_Evaluate("10-5*2", 0, false, false);
    assert(res.error == CALC_OK);
    assert(res.value == 0.0f);

    res = Calc_Evaluate("2^3", 0, false, false);
    assert(res.error == CALC_OK);
    assert(res.value == 8.0f);
    printf("PASSED\n");
}

void test_parentheses() {
    printf("Testing parentheses... ");
    CalcResult_t res = Calc_Evaluate("(2+3)*4", 0, false, false);
    assert(res.error == CALC_OK);
    assert(res.value == 20.0f);

    res = Calc_Evaluate("2*(3+4)", 0, false, false);
    assert(res.error == CALC_OK);
    assert(res.value == 14.0f);
    printf("PASSED\n");
}

void test_trig() {
    printf("Testing trig functions... ");
    // sin(0) = 0
    CalcResult_t res = Calc_Evaluate("sin(0)", 0, false, false);
    assert(res.error == CALC_OK);
    assert(fabs(res.value) < 0.0001f);

    // cos(0) = 1
    res = Calc_Evaluate("cos(0)", 0, false, false);
    assert(res.error == CALC_OK);
    assert(fabs(res.value - 1.0f) < 0.0001f);
    printf("PASSED\n");
}

void test_error_handling() {
    printf("Testing error handling... ");
    // Division by zero
    CalcResult_t res = Calc_Evaluate("1/0", 0, false, false);
    assert(res.error == CALC_ERR_DIV_ZERO);

    // Syntax error
    res = Calc_Evaluate("2++2", 0, false, false);
    assert(res.error == CALC_ERR_SYNTAX);

    // Unbalanced parens
    res = Calc_Evaluate("(2+2", 0, false, false);
    assert(res.error == CALC_ERR_SYNTAX);
    printf("PASSED\n");
}

int main() {
    printf("Running calc_engine tests...\n");
    test_basic_arithmetic();
    test_parentheses();
    test_trig();
    test_error_handling();
    printf("All tests PASSED!\n");
    return 0;
}
