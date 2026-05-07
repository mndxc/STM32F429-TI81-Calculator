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
#define CALC_MATRIX_MAX_DIM  6  /* Maximum 6×6; actual size stored in rows/cols */
#define CALC_MATRIX_COUNT    4  /* [A]=0 [B]=1 [C]=2 ANS=3 */

/*---------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

/** Up-to-6×6 float matrix — shared between calc_engine and calculator_core. */
typedef struct {
    float   data[CALC_MATRIX_MAX_DIM][CALC_MATRIX_MAX_DIM];
    uint8_t rows;
    uint8_t cols;
} CalcMatrix_t;

typedef enum {
    CALC_OK = 0,
    /* TI-81 error 01 MATH — numerical result or intermediate >= 1E100 */
    CALC_ERR_RESULT_OVERFLOW,
    /* TI-81 error 02 MATH — division by zero */
    CALC_ERR_DIV_ZERO,
    /* TI-81 error 03 MATH — result is imaginary (sqrt(-1), asin(>1)…) */
    CALC_ERR_IMAGINARY,
    /* TI-81 error 04 MATH — invalid argument or undefined intermediate */
    CALC_ERR_DOMAIN,
    /* TI-81 error 05 MATH — incompatible matrix dimensions for operation */
    CALC_ERR_MATRIX_OP,
    /* TI-81 error 06 SYNTAX — mismatched parens, bad expression structure */
    CALC_ERR_SYNTAX,
    /* TI-81 error 07 MEMORY — expression too complex (token/stack overflow) */
    CALC_ERR_OVERFLOW,
    /* TI-81 error 08 MEMORY — referenced variable not defined */
    CALC_ERR_UNDEFINED,
    /* TI-81 error 11 RANGE — Xmax <= Xmin or Ymax <= Ymin */
    CALC_ERR_RANGE,
    /* TI-81 error 12 ZOOM — zoom magnitude out of numerical range */
    CALC_ERR_ZOOM,
    /* TI-81 error 13 BREAK — ON key pressed during execution */
    CALC_ERR_BREAK,
    /* TI-81 error 14 PRGM — Goto label not found */
    CALC_ERR_PRGM_NO_LABEL,
    /* TI-81 error 15 PRGM — subroutine nesting exceeds 10 */
    CALC_ERR_PRGM_NESTING,
    /* TI-81 error 16 INVALID — invalid dimension, Xres, or subscript */
    CALC_ERR_INVALID,
} CalcError_t;

typedef enum {
    MATH_NUMBER,
    MATH_VAR_X,             /* X variable — value substituted at evaluation time */
    MATH_VAR_T,             /* T variable — parametric free variable, substituted at eval time */
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
    MATH_FUNC_SINH,
    MATH_FUNC_COSH,
    MATH_FUNC_TANH,
    MATH_FUNC_ASINH,
    MATH_FUNC_ACOSH,
    MATH_FUNC_ATANH,
    MATH_FUNC_LN,
    MATH_FUNC_LOG,
    MATH_FUNC_SQRT,
    MATH_FUNC_ABS,
    MATH_FUNC_EXP,          /* e^x */
    MATH_FUNC_ROUND,        /* round( — round to nearest integer */
    MATH_FUNC_IPART,        /* iPart( — integer part (truncate toward zero) */
    MATH_FUNC_FPART,        /* fPart( — fractional part */
    MATH_FUNC_INT,          /* int(   — floor (greatest integer) */
    MATH_FUNC_R_TO_P,       /* R>P(x,y) — rect→polar; stores R,θ; returns R */
    MATH_FUNC_P_TO_R,       /* P>R(r,θ) — polar→rect; stores X,Y; returns X */
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
    /* Angle-override postfix operators (guidebook p. 2-3/2-5) */
    MATH_OP_DEGREE,         /* °  U+00B0 — interpret arg as degrees regardless of mode */
    MATH_OP_RADIAN,         /* r  (lowercase) — interpret arg as radians regardless of mode */
    /* Y= equation references — evaluated at x_val when encountered in RPN */
    MATH_VAR_Y1,   /* Y₁ — evaluates graph_state.equations[0] at x_val */
    MATH_VAR_Y2,   /* Y₂ */
    MATH_VAR_Y3,   /* Y₃ */
    MATH_VAR_Y4,   /* Y₄ */
    /* Matrix value reference — value field holds matrix index (0=A, 1=B, 2=C, 3=ANS) */
    MATH_MATRIX_VAL,
    /* Matrix operations */
    MATH_FUNC_DET,          /* det( — 1 arg: matrix → scalar */
    MATH_OP_TRANSPOSE,      /* T   — postfix unary: matrix → matrix */
    MATH_FUNC_ROWSWAP,      /* rowSwap( — 3 args: matrix, r1, r2 → matrix */
    MATH_FUNC_ROWPLUS,      /* row+( — 3 args: matrix, r1, r2 → matrix (row[r1]+=row[r2]) */
    MATH_FUNC_MROW,         /* *row( — 3 args: k, matrix, r → matrix (row[r]*=k) */
    MATH_FUNC_MROWPLUS,     /* *row+( — 4 args: k, matrix, r1, r2 → matrix (row[r1]+=k*row[r2]) */
    MATH_FUNC_NDERIV,       /* nDeriv( — 3 args: expr (pre-compiled), var, val → numeric derivative */
    MATH_FUNC_LIST_X,       /* {x}(n) — nth stat x data point (1-based) */
    MATH_FUNC_LIST_Y,       /* {y}(n) — nth stat y data point (1-based) */
    MATH_COMMA,             /* , — argument separator (consumed by ShuntingYard, never in RPN) */
} MathTokenType_t;

typedef struct {
    MathTokenType_t type;
    float           value;  /* Only used when type == MATH_NUMBER */
} MathToken_t;

/**
 * @brief Cached postfix (RPN) form of a Y= equation.
 *        Produced by Calc_PrepareGraphEquation(); evaluated by Calc_EvalGraphEquation().
 *        Stores MATH_VAR_X tokens so X is substituted at evaluation time, not at
 *        parse time — allows the postfix to be reused across all pixel columns.
 */
typedef struct {
    MathToken_t tokens[CALC_MAX_TOKENS];
    uint8_t     count;
} GraphEquation_t;

typedef struct {
    float       value;          /* Computed scalar result */
    CalcError_t error;          /* Error code, CALC_OK if successful */
    char        error_msg[24];  /* Human readable error string */
    bool        has_matrix;     /* True if result is a matrix, not a scalar */
    uint8_t     matrix_idx;     /* Index into calc_matrices[] when has_matrix is true */
    uint16_t    error_offset;   /* Byte offset in source expression where fault was detected (0 if unknown) */
} CalcResult_t;

/**
 * @brief Pre-parsed expression ready for repeated evaluation.
 *
 * Produced by Calc_Parse(); consumed by Calc_Eval(). Separates the
 * tokenize+shunting-yard pass (done once per equation) from the RPN
 * evaluation pass (done once per call or 240× per graph render column).
 *
 * The @c nested field holds the inner expression compiled from nDeriv()'s
 * first argument. Its count is 0 when the expression contains no nDeriv().
 */
typedef struct {
    GraphEquation_t postfix;   /* shunting-yard output */
    GraphEquation_t nested;    /* nDeriv inner expression, if any */
} ParsedExpr_t;

/*---------------------------------------------------------------------------
 * Variable storage
 *--------------------------------------------------------------------------*/

/** User variable storage — A through Z, indexed by (ch - 'A'); [26] = θ. */
extern float calc_variables[27];

/** Matrix storage — [A]=0, [B]=1, [C]=2, ANS=3. */
extern CalcMatrix_t calc_matrices[CALC_MATRIX_COUNT];


/*---------------------------------------------------------------------------
 * Function declarations
 *--------------------------------------------------------------------------*/

/**
 * @brief Registers the Y= equation string array so that Y₁–Y₄ references in
 *        expressions can be evaluated recursively.
 *
 * Must be called once at startup, passing a pointer to
 * graph_state.equations[][64].  In HOST_TEST builds the pointer can be NULL
 * — references then evaluate to 0.
 *
 * @param eqs    Pointer to an array of GRAPH_NUM_EQ strings (each [64] bytes)
 * @param count  Number of valid equation slots (must be <= GRAPH_NUM_EQ)
 */
void Calc_RegisterYEquations(const char (*eqs)[64], uint8_t count);

/**
 * @brief Registers stat list accessor callbacks so that {x}(n) / {y}(n)
 *        can be evaluated without coupling calc_engine to the UI stat layer.
 *        Pass NULL to disable; evaluates to CALC_ERR_DOMAIN when not registered.
 *
 * @param get_x   Returns list_x value at 1-based index n
 * @param get_y   Returns list_y value at 1-based index n
 * @param get_len Returns number of valid data points
 */
void Calc_RegisterStatAccessors(float (*get_x)(int n),
                                 float (*get_y)(int n),
                                 int   (*get_len)(void));

/**
 * @brief Evaluates an infix expression string.
 *
 * @param expr           Null-terminated infix expression e.g. "3+sin(45)*2"
 * @param ans            Current ANS value. Holds a scalar result normally, or a
 *                       matrix slot index (cast to float) when ans_is_matrix is true.
 * @param ans_is_matrix  True when the previous result was a matrix stored in
 *                       calc_matrices[3]. Causes "ANS" to tokenize as MATH_MATRIX_VAL
 *                       rather than MATH_NUMBER.
 * @param angle_degrees  True for degrees, false for radians
 * @return               CalcResult_t containing value or error
 */
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool ans_is_matrix,
                            bool angle_degrees);

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
uint8_t Calc_GetDecimalMode(void);

/**
 * @brief Sets the notation mode used by Calc_FormatResult.
 *
 * @param mode  0 = Normal, 1 = Sci, 2 = Eng.
 *              Matches mode_committed[0] from the MODE screen directly.
 */
void Calc_SetNotationMode(uint8_t mode);
uint8_t Calc_GetNotationMode(void);

/**
 * @brief Returns the TI-81 error string for an error code, e.g. "ERROR 06 SYNTAX".
 *
 * The returned pointer is to a string literal — do not free or modify it.
 * CALC_OK returns an empty string "".
 */
const char *Calc_GetErrorString(CalcError_t err);

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

/**
 * @brief Compiles a Y= equation string into a cached postfix form.
 *
 * Runs Tokenize + ImplicitMulPass + ShuntingYard once per equation. The
 * resulting GraphEquation_t can be passed to Calc_EvalGraphEquation() for
 * each pixel column, avoiding repeated parsing on every call.
 *
 * MATH_VAR_X tokens in the output are placeholders; the actual x value is
 * supplied at evaluation time by Calc_EvalGraphEquation().
 *
 * @param expr  Null-terminated infix expression (e.g. "sin(X)")
 * @param ans   ANS value to bake in for any "ANS" reference in the expression
 * @param out   Output postfix cache — must not be NULL
 * @return      CALC_OK, or an error code if the expression is invalid
 */
CalcError_t Calc_PrepareGraphEquation(const char *expr, float ans,
                                      GraphEquation_t *out);

/**
 * @brief Evaluates a pre-compiled graph equation at a specific x coordinate.
 *
 * Calls EvaluateRPN on the cached postfix, substituting x_val for every
 * MATH_VAR_X token. No string parsing or shunting-yard conversion is done.
 *
 * @param eq            Postfix cache produced by Calc_PrepareGraphEquation()
 * @param x_val         X coordinate to evaluate at
 * @param angle_degrees True for degrees, false for radians
 * @return              CalcResult_t containing value or error
 */
CalcResult_t Calc_EvalGraphEquation(const GraphEquation_t *eq, float x_val,
                                    bool angle_degrees);

/**
 * @brief Compiles a parametric equation string into a cached postfix form.
 *
 * Identical to Calc_PrepareGraphEquation() except "T" (or "t") is tokenized
 * as MATH_VAR_T rather than as a stored variable.  "X" still tokenizes as
 * MATH_VAR_X.  Pass the result to Calc_EvalParamEquation().
 *
 * @param expr  Null-terminated infix expression in terms of T (e.g. "cos(T)")
 * @param ans   ANS value to bake in for any "ANS" reference in the expression
 * @param out   Output postfix cache — must not be NULL
 * @return      CALC_OK, or an error code if the expression is invalid
 */
CalcError_t Calc_PrepareParamEquation(const char *expr, float ans,
                                      GraphEquation_t *out);

/**
 * @brief Evaluates a pre-compiled parametric equation at a specific t value.
 *
 * Substitutes t_val for every MATH_VAR_T token; MATH_VAR_X tokens resolve
 * to the stored calc_variables['X'-'A'] value (not overridden).
 *
 * @param eq            Postfix cache produced by Calc_PrepareParamEquation()
 * @param t_val         Parametric parameter value to substitute for T
 * @param angle_degrees True for degrees, false for radians
 * @return              CalcResult_t containing value or error
 */
CalcResult_t Calc_EvalParamEquation(const GraphEquation_t *eq, float t_val,
                                    bool angle_degrees);

/**
 * @brief Tokenizes, applies implicit multiplication, and shunting-yards an
 *        infix expression into a @c ParsedExpr_t ready for Calc_Eval().
 *
 * Separates the parse pass from the evaluation pass so callers that evaluate
 * the same expression repeatedly (e.g. 240 pixel columns per graph render)
 * avoid re-parsing on every call.
 *
 * If the expression contains @c nDeriv(), the inner sub-expression is
 * compiled into @c out->nested; otherwise @c out->nested.count is 0.
 *
 * @param expr           Null-terminated infix expression
 * @param ans            ANS value substituted for "ANS" tokens
 * @param ans_is_matrix  True when ANS holds a matrix slot index
 * @param param_mode     True to tokenize 'T'/'t' as MATH_VAR_T (parametric)
 * @param out            Output ParsedExpr_t — must not be NULL
 * @return               CALC_OK or an error code
 */
CalcError_t Calc_Parse(const char *expr, float ans, bool ans_is_matrix,
                       bool param_mode, ParsedExpr_t *out);

/**
 * @brief Evaluates a pre-parsed expression at specific x and t coordinates.
 *
 * No string parsing or shunting-yard conversion — pure RPN evaluation.
 * Unifies the function-graph (x_val varies, t_val=0) and parametric-graph
 * (x_val=stored X, t_val varies) cases under one call.
 *
 * @param parsed         ParsedExpr_t produced by Calc_Parse()
 * @param x_val          Value substituted for MATH_VAR_X tokens
 * @param t_val          Value substituted for MATH_VAR_T tokens (0 for function mode)
 * @param angle_degrees  True for degrees, false for radians
 * @return               CalcResult_t containing value or error
 */
CalcResult_t Calc_Eval(const ParsedExpr_t *parsed, float x_val, float t_val,
                       bool angle_degrees);

#endif /* CALC_ENGINE_H */