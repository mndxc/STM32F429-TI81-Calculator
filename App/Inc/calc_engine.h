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

/* Matrix dimensions */
#define CALC_MATRIX_DIM    3    /* Fixed 3×3 */
#define CALC_MATRIX_COUNT  4    /* [A]=0 [B]=1 [C]=2 ANS=3 */

/*---------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

/** 3×3 float matrix — shared between calc_engine and calculator_core. */
typedef struct {
    float   data[CALC_MATRIX_DIM][CALC_MATRIX_DIM];
    uint8_t rows;
    uint8_t cols;
} CalcMatrix_t;

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
    MATH_FUNC_ROUND,        /* round( — round to nearest integer */
    MATH_FUNC_IPART,        /* iPart( — integer part (truncate toward zero) */
    MATH_FUNC_FPART,        /* fPart( — fractional part */
    MATH_FUNC_INT,          /* int(   — floor (greatest integer) */
    MATH_PAREN_LEFT,
    MATH_PAREN_RIGHT,
    /* Comparison operators — return 1.0 (true) or 0.0 (false) */
    MATH_OP_EQ,             /* =  */
    MATH_OP_NEQ,            /* ≠  U+2260 */
    MATH_OP_GT,             /* >  */
    MATH_OP_GTE,            /* ≥  U+2265 */
    MATH_OP_LT,             /* <  */
    MATH_OP_LTE,            /* ≤  U+2264 */
    /* PRB operators */
    MATH_OP_FACT,           /* !  — factorial (postfix unary) */
    MATH_OP_NPR,            /* nPr — permutations (binary) */
    MATH_OP_NCR,            /* nCr — combinations (binary) */
    /* Matrix value reference — value field holds matrix index (0=A, 1=B, 2=C, 3=ANS) */
    MATH_MATRIX_VAL,
    /* Matrix operations */
    MATH_FUNC_DET,          /* det( — 1 arg: matrix → scalar */
    MATH_OP_TRANSPOSE,      /* T   — postfix unary: matrix → matrix */
    MATH_FUNC_ROWSWAP,      /* rowSwap( — 3 args: matrix, r1, r2 → matrix */
    MATH_FUNC_ROWPLUS,      /* row+( — 3 args: matrix, r1, r2 → matrix (row[r1]+=row[r2]) */
    MATH_FUNC_MROW,         /* *row( — 3 args: k, matrix, r → matrix (row[r]*=k) */
    MATH_FUNC_MROWPLUS,     /* *row+( — 4 args: k, matrix, r1, r2 → matrix (row[r1]+=k*row[r2]) */
    MATH_COMMA,             /* , — argument separator (consumed by ShuntingYard, never in RPN) */
} MathTokenType_t;

typedef struct {
    MathTokenType_t type;
    float           value;  /* Only used when type == MATH_NUMBER */
} MathToken_t;

typedef struct {
    float       value;          /* Computed scalar result */
    CalcError_t error;          /* Error code, CALC_OK if successful */
    char        error_msg[24];  /* Human readable error string */
    bool        has_matrix;     /* True if result is a matrix, not a scalar */
    uint8_t     matrix_idx;     /* Index into calc_matrices[] when has_matrix is true */
} CalcResult_t;

/*---------------------------------------------------------------------------
 * Variable storage
 *--------------------------------------------------------------------------*/

/** User variable storage — A through Z, indexed by (ch - 'A'). */
extern float calc_variables[26];

/** Matrix storage — [A]=0, [B]=1, [C]=2, ANS=3. */
extern CalcMatrix_t calc_matrices[CALC_MATRIX_COUNT];

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

/**
 * @brief Sets the decimal display mode used by Calc_FormatResult.
 *
 * @param mode  0 = Float (auto), 1 = Fix 0, 2 = Fix 1, … 10 = Fix 9.
 *              Matches mode_committed[1] from the MODE screen directly.
 */
void Calc_SetDecimalMode(uint8_t mode);

/**
 * @brief Evaluates an infix expression with a specific value substituted for X.
 *
 * Used by the graphing subsystem — the graph x coordinate overrides whatever
 * is stored in calc_variables['X'-'A'] for the duration of this call.
 *
 * @param expr          Null-terminated infix expression in terms of x
 * @param x_val         Value to substitute for the variable x/X
 * @param ans           Current ANS value substituted for "ANS" in expression
 * @param angle_degrees True for degrees, false for radians
 * @return              CalcResult_t containing value or error
 */
CalcResult_t Calc_EvaluateAt(const char *expr, float x_val,
                              float ans, bool angle_degrees);

#endif /* CALC_ENGINE_H */