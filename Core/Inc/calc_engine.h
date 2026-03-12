/**
 * @file    calc_engine.h
 * @brief   Calculator expression parser and evaluator interface.
 *
 * Implements a two-pass expression evaluator:
 *   1. Tokenizer  — splits an infix expression string into math tokens
 *   2. Shunting-yard — converts infix tokens to postfix (RPN)
 *   3. RPN evaluator — evaluates the postfix token stream
 *
 * Supports: basic arithmetic, parentheses, power, trig functions,
 * logarithms, sqrt, abs, and the ANS variable.
 */

#ifndef CALC_ENGINE_H
#define CALC_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Constants
 *--------------------------------------------------------------------------*/

#define CALC_MAX_TOKENS     64
#define CALC_MAX_STACK      32
#define CALC_EXPR_MAX_LEN   64

/*---------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

typedef enum {
    CALC_OK = 0,
    CALC_ERR_DIV_ZERO,
    CALC_ERR_DOMAIN,        /* e.g. sqrt(-1), log(-1) */
    CALC_ERR_SYNTAX,        /* unmatched parentheses, bad expression */
    CALC_ERR_OVERFLOW,      /* too many tokens or stack overflow */
    CALC_ERR_UNDEFINED,     /* unknown token */
} CalcError_t;

typedef enum {
    MATH_NUMBER,
    MATH_OP_ADD,
    MATH_OP_SUB,
    MATH_OP_MUL,
    MATH_OP_DIV,
    MATH_OP_POW,
    MATH_OP_NEG,            /* Unary negation */
    MATH_FUNC_SIN,
    MATH_FUNC_COS,
    MATH_FUNC_TAN,
    MATH_FUNC_ASIN,
    MATH_FUNC_ACOS,
    MATH_FUNC_ATAN,
    MATH_FUNC_LN,
    MATH_FUNC_LOG,
    MATH_FUNC_SQRT,
    MATH_FUNC_ABS,
    MATH_FUNC_EXP,          /* e^x */
    MATH_PAREN_LEFT,
    MATH_PAREN_RIGHT,
} MathTokenType_t;

typedef struct {
    MathTokenType_t type;
    float           value;  /* Only used when type == MATH_NUMBER */
} MathToken_t;

typedef struct {
    float       value;          /* Computed result */
    CalcError_t error;          /* Error code, CALC_OK if successful */
    char        error_msg[24];  /* Human readable error string */
} CalcResult_t;

/*---------------------------------------------------------------------------
 * Variable storage
 *--------------------------------------------------------------------------*/

/** User variable storage — A through Z, indexed by (ch - 'A'). */
extern float calc_variables[26];

/*---------------------------------------------------------------------------
 * Function declarations
 *--------------------------------------------------------------------------*/

/**
 * @brief Evaluates an infix expression string.
 *
 * @param expr          Null-terminated infix expression e.g. "3+sin(45)*2"
 * @param ans           Current ANS value substituted for "ANS" in expression
 * @param angle_degrees True for degrees, false for radians
 * @return              CalcResult_t containing value or error
 */
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool angle_degrees);

/**
 * @brief Formats a float result into a clean display string.
 *
 * Produces integers without decimal points, trims trailing zeros,
 * and switches to scientific notation for very large or small values.
 *
 * @param value   Float to format
 * @param buf     Output buffer
 * @param buf_len Buffer size
 */
void Calc_FormatResult(float value, char *buf, uint8_t buf_len);


CalcResult_t Calc_EvaluateAt(const char *expr, float x_val,
                              float ans, bool angle_degrees);

#endif /* CALC_ENGINE_H */