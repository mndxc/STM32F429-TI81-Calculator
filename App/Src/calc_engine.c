/**
 * @file    calc_engine.c
 * @brief   Calculator expression parser and evaluator.
 *
 * Processing pipeline:
 *   Tokenize() — string -> MathToken_t array (infix)
 *   ShuntingYard() — infix MathToken_t array -> postfix MathToken_t array
 *   EvaluateRPN() — postfix MathToken_t array -> float result
 */

#include "calc_engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

/* User variable storage — A through Z, indexed by (ch - 'A') */
float calc_variables[26] = {0};

/* Matrix storage — [A]=0, [B]=1, [C]=2, ANS=3 (result of last matrix op) */
CalcMatrix_t calc_matrices[CALC_MATRIX_COUNT] = {
    { .rows = 3, .cols = 3 },
    { .rows = 3, .cols = 3 },
    { .rows = 3, .cols = 3 },
    { .rows = 3, .cols = 3 },
};

/* Decimal display mode: 0=Float, 1=Fix0 … 10=Fix9 (mirrors MODE screen row 2) */
static uint8_t calc_decimal_mode = 0;


void Calc_SetDecimalMode(uint8_t mode)
{
    calc_decimal_mode = mode;
}

/*---------------------------------------------------------------------------
 * Private types
 *--------------------------------------------------------------------------*/

typedef struct {
    MathToken_t tokens[CALC_MAX_TOKENS];
    uint8_t     count;
} TokenList_t;

/*---------------------------------------------------------------------------
 * Private helpers — operator properties
 *--------------------------------------------------------------------------*/

static bool is_operator(MathTokenType_t t)
{
    return (t == MATH_OP_ADD || t == MATH_OP_SUB ||
            t == MATH_OP_MUL || t == MATH_OP_DIV ||
            t == MATH_OP_POW || t == MATH_OP_NEG ||
            t == MATH_OP_EQ  || t == MATH_OP_NEQ ||
            t == MATH_OP_GT  || t == MATH_OP_GTE ||
            t == MATH_OP_LT  || t == MATH_OP_LTE ||
            t == MATH_OP_NPR || t == MATH_OP_NCR);
    /* Note: MATH_OP_FACT is postfix unary — handled separately, not here */
}

static bool is_function(MathTokenType_t t)
{
    return (t == MATH_FUNC_SIN     || t == MATH_FUNC_COS     ||
            t == MATH_FUNC_TAN     || t == MATH_FUNC_ASIN    ||
            t == MATH_FUNC_ACOS    || t == MATH_FUNC_ATAN    ||
            t == MATH_FUNC_LN      || t == MATH_FUNC_LOG     ||
            t == MATH_FUNC_SQRT    || t == MATH_FUNC_ABS     ||
            t == MATH_FUNC_EXP     || t == MATH_FUNC_ROUND   ||
            t == MATH_FUNC_IPART   || t == MATH_FUNC_FPART   ||
            t == MATH_FUNC_INT     ||
            /* Matrix functions */
            t == MATH_FUNC_DET     || t == MATH_FUNC_ROWSWAP ||
            t == MATH_FUNC_ROWPLUS || t == MATH_FUNC_MROW    ||
            t == MATH_FUNC_MROWPLUS);
}

static int precedence(MathTokenType_t t)
{
    switch (t) {
    case MATH_OP_EQ:
    case MATH_OP_NEQ:
    case MATH_OP_GT:
    case MATH_OP_GTE:
    case MATH_OP_LT:
    case MATH_OP_LTE: return 0; /* lowest — arithmetic fully evaluated first */
    case MATH_OP_ADD:
    case MATH_OP_SUB: return 1;
    case MATH_OP_MUL:
    case MATH_OP_DIV:
    case MATH_OP_NPR:
    case MATH_OP_NCR: return 2;
    case MATH_OP_NEG: return 3;
    case MATH_OP_POW: return 4;
    default:          return -1;
    }
}

static bool is_right_assoc(MathTokenType_t t)
{
    /* Power and unary negation are right associative */
    return (t == MATH_OP_POW || t == MATH_OP_NEG);
}

/*---------------------------------------------------------------------------
 * Stage 1 — Tokenizer helpers
 *--------------------------------------------------------------------------*/

/* Try to tokenize a numeric literal at *p.
 * Handles plain numbers (digit or '.') and the '-' after '^' special case.
 * Returns CALC_OK with *matched==true on success, CALC_OK with *matched==false
 * if *p is not a number, or an error code on overflow/syntax. */
static CalcError_t try_tokenize_number(const char **p, TokenList_t *out, bool *matched)
{
    *matched = false;

    /* Special case: '-' immediately after '^' before a digit or dot —
       fold the sign into the number literal so shunting-yard never
       tries to pop '^' before its right operand is ready.
       e.g. 2^-3  →  tokens [2, ^, -3]  rather than [2, ^, NEG, 3]. */
    if (**p == '-' &&
        out->count > 0 &&
        out->tokens[out->count - 1].type == MATH_OP_POW &&
        (isdigit((unsigned char)(*p)[1]) || (*p)[1] == '.')) {
        char *end;
        float val = strtof(*p, &end);
        if (end == *p) return CALC_ERR_SYNTAX;
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_NUMBER;
        out->tokens[out->count].value = val;
        out->count++;
        *p = end;
        *matched = true;
        return CALC_OK;
    }

    if (!isdigit((unsigned char)**p) && **p != '.')
        return CALC_OK; /* no match */

    char *end;
    float val = strtof(*p, &end);
    if (end == *p) return CALC_ERR_SYNTAX;
    if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
    out->tokens[out->count].type  = MATH_NUMBER;
    out->tokens[out->count].value = val;
    out->count++;
    *p = end;
    *matched = true;
    return CALC_OK;
}

/* Try to tokenize a named identifier or variable at *p.
 * Covers: ANS, X, matrix refs [A/B/C], T-transpose, pi, rand, named
 * functions (sin/cos/…), the 'e' constant, and user variables A–Z.
 * Returns CALC_OK with *matched==true on success, CALC_OK with *matched==false
 * if *p is not a known identifier, or an error code on overflow. */
static CalcError_t try_tokenize_identifier(const char **p, TokenList_t *out,
                                            float ans, bool ans_is_matrix,
                                            float x_val, bool *matched)
{
    *matched = false;

    /* ANS */
    if (strncmp(*p, "ANS", 3) == 0) {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        if (ans_is_matrix) {
            out->tokens[out->count].type  = MATH_MATRIX_VAL;
            out->tokens[out->count].value = 3.0f; /* ANS matrix slot */
        } else {
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = ans;
        }
        out->count++;
        *p += 3;
        *matched = true;
        return CALC_OK;
    }

    /* X variable */
    if (**p == 'x' || **p == 'X') {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_NUMBER;
        out->tokens[out->count].value = x_val;
        out->count++;
        *p += 1;
        *matched = true;
        return CALC_OK;
    }

    /* Matrix references [A], [B], [C] — must be checked before the
       uppercase variable handler so 'A'/'B'/'C' are not consumed first. */
    if ((*p)[0] == '[' &&
        ((*p)[1] == 'A' || (*p)[1] == 'B' || (*p)[1] == 'C') &&
        (*p)[2] == ']') {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_MATRIX_VAL;
        out->tokens[out->count].value = (float)((*p)[1] - 'A');
        out->count++;
        *p += 3;
        *matched = true;
        return CALC_OK;
    }

    /* T immediately after a matrix value or ')' → transpose postfix operator.
       Must be checked before the uppercase variable handler; otherwise 'T'
       is consumed as user variable T. */
    if (**p == 'T' && out->count > 0) {
        MathTokenType_t prev = out->tokens[out->count - 1].type;
        if (prev == MATH_MATRIX_VAL || prev == MATH_PAREN_RIGHT) {
            if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_OP_TRANSPOSE;
            out->tokens[out->count].value = 0;
            out->count++;
            (*p)++;
            *matched = true;
            return CALC_OK;
        }
    }

    /* pi constant — accept both ASCII "pi" and UTF-8 π (0xCF 0x80) */
    if (strncmp(*p, "pi", 2) == 0 ||
        ((unsigned char)(*p)[0] == 0xCFu && (unsigned char)(*p)[1] == 0x80u)) {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_NUMBER;
        out->tokens[out->count].value = 3.14159265358979f;
        out->count++;
        *p += 2; /* "pi" and UTF-8 π (0xCF 0x80) are both 2 bytes */
        *matched = true;
        return CALC_OK;
    }

    /* rand — zero-argument: evaluate immediately to a random [0, 1) number */
    if (strncmp(*p, "rand", 4) == 0) {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_NUMBER;
        out->tokens[out->count].value = (float)rand() / ((float)RAND_MAX + 1.0f);
        out->count++;
        *p += 4;
        *matched = true;
        return CALC_OK;
    }

    /* Named functions and binary-operator keywords.
       Longer names that share a prefix must appear first.
       Do NOT include '(' in the name — it is tokenized separately as
       MATH_PAREN_LEFT, which ShuntingYard needs to pop on ')'. */
    static const struct { const char *name; MathTokenType_t type; } funcs[] = {
        { "sin",     MATH_FUNC_SIN      },
        { "cos",     MATH_FUNC_COS      },
        { "tan",     MATH_FUNC_TAN      },
        { "asin",    MATH_FUNC_ASIN     },
        { "acos",    MATH_FUNC_ACOS     },
        { "atan",    MATH_FUNC_ATAN     },
        { "ln",      MATH_FUNC_LN       },
        { "log",     MATH_FUNC_LOG      },
        { "\xE2\x88\x9A", MATH_FUNC_SQRT },
        { "abs",     MATH_FUNC_ABS      },
        { "exp",     MATH_FUNC_EXP      },
        { "round",   MATH_FUNC_ROUND    },
        { "iPart",   MATH_FUNC_IPART    },
        { "fPart",   MATH_FUNC_FPART    },
        { "int",     MATH_FUNC_INT      },
        { "nPr",     MATH_OP_NPR        },
        { "nCr",     MATH_OP_NCR        },
        /* Matrix functions — "*row+" must precede "*row" (longer match wins) */
        { "*row+",   MATH_FUNC_MROWPLUS },
        { "*row",    MATH_FUNC_MROW     },
        { "rowSwap", MATH_FUNC_ROWSWAP  },
        { "row+",    MATH_FUNC_ROWPLUS  },
        { "det",     MATH_FUNC_DET      },
    };

    for (int i = 0; i < (int)(sizeof(funcs)/sizeof(funcs[0])); i++) {
        size_t len = strlen(funcs[i].name);
        if (strncmp(*p, funcs[i].name, len) == 0) {
            if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = funcs[i].type;
            out->tokens[out->count].value = 0;
            out->count++;
            *p += len;
            *matched = true;
            return CALC_OK;
        }
    }

    /* e constant — Euler's number (must be after named-function check so
       "exp" is consumed above before we reach here) */
    if (**p == 'e') {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_NUMBER;
        out->tokens[out->count].value = 2.71828182845905f;
        out->count++;
        (*p)++;
        *matched = true;
        return CALC_OK;
    }

    /* User variables A–Z (uppercase; skip X — handled above as graph var) */
    if (isupper((unsigned char)**p) && **p != 'X') {
        if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
        out->tokens[out->count].type  = MATH_NUMBER;
        out->tokens[out->count].value = calc_variables[**p - 'A'];
        out->count++;
        (*p)++;
        *matched = true;
        return CALC_OK;
    }

    return CALC_OK; /* no match */
}

/* Try to tokenize an operator at *p.
 * Covers UTF-8 multi-byte comparison operators and single-character operators.
 * Returns CALC_OK with *matched==true on success, CALC_OK with *matched==false
 * if *p is not a known operator, or an error code on overflow. */
static CalcError_t try_tokenize_operator(const char **p, TokenList_t *out, bool *matched)
{
    *matched = false;

    /* UTF-8 multi-byte comparison operators (must be checked before single-char) */
    static const struct { const char *seq; MathTokenType_t type; } cmp3[] = {
        { "\xE2\x89\xA0", MATH_OP_NEQ }, /* U+2260 ≠ */
        { "\xE2\x89\xA5", MATH_OP_GTE }, /* U+2265 ≥ */
        { "\xE2\x89\xA4", MATH_OP_LTE }, /* U+2264 ≤ */
    };
    for (int i = 0; i < 3; i++) {
        if (strncmp(*p, cmp3[i].seq, 3) == 0) {
            if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = cmp3[i].type;
            out->tokens[out->count].value = 0.0f;
            out->count++;
            *p += 3;
            *matched = true;
            return CALC_OK;
        }
    }

    /* Single-character operators */
    MathTokenType_t op_type;
    switch (**p) {
    case '+': op_type = MATH_OP_ADD;      break;
    case '=': op_type = MATH_OP_EQ;       break;
    case '>': op_type = MATH_OP_GT;       break;
    case '<': op_type = MATH_OP_LT;       break;
    case '-':
        /* Unary negation if at start or after operator/left paren */
        if (out->count == 0 ||
            is_operator(out->tokens[out->count - 1].type) ||
            out->tokens[out->count - 1].type == MATH_PAREN_LEFT) {
            op_type = MATH_OP_NEG;
        } else {
            op_type = MATH_OP_SUB;
        }
        break;
    case '*': op_type = MATH_OP_MUL;      break;
    case '/': op_type = MATH_OP_DIV;      break;
    case '^': op_type = MATH_OP_POW;      break;
    case '!': op_type = MATH_OP_FACT;     break;
    case '(': op_type = MATH_PAREN_LEFT;  break;
    case ')': op_type = MATH_PAREN_RIGHT; break;
    case ',': op_type = MATH_COMMA;       break;
    default:
        return CALC_OK; /* no match */
    }

    if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
    out->tokens[out->count].type  = op_type;
    out->tokens[out->count].value = 0;
    out->count++;
    (*p)++;
    *matched = true;
    return CALC_OK;
}

/*---------------------------------------------------------------------------
 * Stage 1 — Tokenizer
 *--------------------------------------------------------------------------*/

static CalcError_t Tokenize(const char *expr, float ans, bool ans_is_matrix,
                             float x_val, TokenList_t *out)
{
    out->count = 0;
    const char *p = expr;

    while (*p != '\0') {
        if (isspace((unsigned char)*p)) { p++; continue; }

        bool matched;
        CalcError_t err;

        err = try_tokenize_number(&p, out, &matched);
        if (err != CALC_OK) return err;
        if (matched) continue;

        err = try_tokenize_identifier(&p, out, ans, ans_is_matrix, x_val, &matched);
        if (err != CALC_OK) return err;
        if (matched) continue;

        err = try_tokenize_operator(&p, out, &matched);
        if (err != CALC_OK) return err;
        if (matched) continue;

        return CALC_ERR_SYNTAX; /* Unknown character */
    }

    return CALC_OK;
}

/*---------------------------------------------------------------------------
 * Stage 1b — Implicit multiplication pass
 *
 * Inserts MATH_OP_MUL between adjacent tokens where implicit multiplication
 * is implied by juxtaposition.  Handled patterns:
 *   number·function    2sin(x)
 *   number·paren       2(3+4)
 *   paren·paren        (a+b)(c+d)
 *   number·number      2pi  (pi already resolved to a number token)
 *   paren·number       (x+1)2
 *
 * Rule: insert '*' when left token is NUMBER or ')' AND
 *       right token is NUMBER, '(', or any function.
 *---------------------------------------------------------------------------*/

static CalcError_t ImplicitMulPass(TokenList_t *list)
{
    for (int i = 0; i < (int)list->count - 1; i++) {
        MathTokenType_t cur  = list->tokens[i].type;
        MathTokenType_t next = list->tokens[i + 1].type;

        bool cur_is_value  = (cur  == MATH_NUMBER || cur  == MATH_PAREN_RIGHT ||
                              cur  == MATH_MATRIX_VAL);
        bool next_is_factor = (next == MATH_NUMBER || next == MATH_PAREN_LEFT ||
                               next == MATH_MATRIX_VAL || is_function(next));

        if (cur_is_value && next_is_factor) {
            if (list->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            /* Shift everything from i+1 onwards one position right */
            for (int j = (int)list->count; j > i + 1; j--)
                list->tokens[j] = list->tokens[j - 1];
            list->tokens[i + 1].type  = MATH_OP_MUL;
            list->tokens[i + 1].value = 0.0f;
            list->count++;
            i++; /* skip the inserted '*' on the next iteration */
        }
    }
    return CALC_OK;
}

/*---------------------------------------------------------------------------
 * Stage 2 — Shunting-yard (infix -> postfix)
 *--------------------------------------------------------------------------*/

static CalcError_t ShuntingYard(const TokenList_t *in, TokenList_t *out)
{
    MathToken_t op_stack[CALC_MAX_STACK];
    int         op_top = -1;
    out->count = 0;

    for (int i = 0; i < in->count; i++) {
        MathToken_t tok = in->tokens[i];

        if (tok.type == MATH_NUMBER || tok.type == MATH_MATRIX_VAL) {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count++] = tok;
        }
        else if (is_function(tok.type)) {
            if (op_top >= CALC_MAX_STACK - 1)
                return CALC_ERR_OVERFLOW;
            op_stack[++op_top] = tok;
        }
        else if (tok.type == MATH_OP_FACT || tok.type == MATH_OP_TRANSPOSE) {
            /* Postfix unary: output immediately — applies to whatever is on top */
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count++] = tok;
        }
        else if (tok.type == MATH_COMMA) {
            /* Function argument separator: pop operators until '(' is found.
               The '(' stays on the op stack; comma is discarded (never in RPN). */
            while (op_top >= 0 &&
                   op_stack[op_top].type != MATH_PAREN_LEFT) {
                if (out->count >= CALC_MAX_TOKENS)
                    return CALC_ERR_OVERFLOW;
                out->tokens[out->count++] = op_stack[op_top--];
            }
            if (op_top < 0)
                return CALC_ERR_SYNTAX; /* Comma outside function call */
        }
        else if (is_operator(tok.type)) {
            while (op_top >= 0 &&
                   (is_function(op_stack[op_top].type) ||
                    (is_operator(op_stack[op_top].type) &&
                     (precedence(op_stack[op_top].type) > precedence(tok.type) ||
                      (precedence(op_stack[op_top].type) == precedence(tok.type) &&
                       !is_right_assoc(tok.type))) &&
                     op_stack[op_top].type != MATH_PAREN_LEFT))) {
                if (out->count >= CALC_MAX_TOKENS)
                    return CALC_ERR_OVERFLOW;
                out->tokens[out->count++] = op_stack[op_top--];
            }
            if (op_top >= CALC_MAX_STACK - 1)
                return CALC_ERR_OVERFLOW;
            op_stack[++op_top] = tok;
        }
        else if (tok.type == MATH_PAREN_LEFT) {
            if (op_top >= CALC_MAX_STACK - 1)
                return CALC_ERR_OVERFLOW;
            op_stack[++op_top] = tok;
        }
        else if (tok.type == MATH_PAREN_RIGHT) {
            while (op_top >= 0 &&
                   op_stack[op_top].type != MATH_PAREN_LEFT) {
                if (out->count >= CALC_MAX_TOKENS)
                    return CALC_ERR_OVERFLOW;
                out->tokens[out->count++] = op_stack[op_top--];
            }
            if (op_top < 0)
                return CALC_ERR_SYNTAX; /* Mismatched parentheses */
            op_top--; /* Pop left paren */

            /* If top of stack is a function, pop it too */
            if (op_top >= 0 && is_function(op_stack[op_top].type)) {
                if (out->count >= CALC_MAX_TOKENS)
                    return CALC_ERR_OVERFLOW;
                out->tokens[out->count++] = op_stack[op_top--];
            }
        }
    }

    /* Pop remaining operators */
    while (op_top >= 0) {
        if (op_stack[op_top].type == MATH_PAREN_LEFT)
            return CALC_ERR_SYNTAX; /* Mismatched parentheses */
        if (out->count >= CALC_MAX_TOKENS)
            return CALC_ERR_OVERFLOW;
        out->tokens[out->count++] = op_stack[op_top--];
    }

    return CALC_OK;
}

/*---------------------------------------------------------------------------
 * Stage 2b — PRB math helpers
 *--------------------------------------------------------------------------*/

static float calc_factorial(int n)
{
    if (n < 0)  return NAN;
    if (n == 0) return 1.0f;
    float r = 1.0f;
    for (int i = 2; i <= n; i++)
        r *= (float)i;
    return r;
}

/* P(n, r) = n * (n-1) * ... * (n-r+1) */
static float calc_npr(int n, int r)
{
    if (n < 0 || r < 0 || r > n) return NAN;
    float result = 1.0f;
    for (int i = n; i > n - r; i--)
        result *= (float)i;
    return result;
}

/* C(n, r) = P(n, r) / r! */
static float calc_ncr(int n, int r)
{
    if (n < 0 || r < 0 || r > n) return NAN;
    if (r > n - r) r = n - r;  /* use smaller side */
    float result = 1.0f;
    for (int i = 0; i < r; i++)
        result = result * (float)(n - i) / (float)(i + 1);
    return result;
}

/*---------------------------------------------------------------------------
 * Stage 2c — Matrix math helpers
 *--------------------------------------------------------------------------*/

/** Determinant for any square matrix (1×1 through 6×6) via Gaussian elimination. */
static float mat_det(const CalcMatrix_t *m)
{
    int n = m->rows;
    float a[CALC_MATRIX_MAX_DIM][CALC_MATRIX_MAX_DIM];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            a[i][j] = m->data[i][j];

    float det = 1.0f;
    for (int col = 0; col < n; col++) {
        /* Partial pivoting */
        int pivot = col;
        float max_val = fabsf(a[col][col]);
        for (int row = col + 1; row < n; row++) {
            if (fabsf(a[row][col]) > max_val) {
                max_val = fabsf(a[row][col]);
                pivot = row;
            }
        }
        if (max_val < 1e-10f) return 0.0f; /* singular */
        if (pivot != col) {
            for (int j = 0; j < n; j++) {
                float tmp = a[col][j]; a[col][j] = a[pivot][j]; a[pivot][j] = tmp;
            }
            det = -det;
        }
        det *= a[col][col];
        for (int row = col + 1; row < n; row++) {
            float factor = a[row][col] / a[col][col];
            for (int j = col; j < n; j++)
                a[row][j] -= factor * a[col][j];
        }
    }
    return det;
}

/** Transpose src into dst (dst may be the ANS slot, src must differ). */
static void mat_transpose(const CalcMatrix_t *src, CalcMatrix_t *dst)
{
    dst->rows = src->cols;
    dst->cols = src->rows;
    for (int r = 0; r < src->rows; r++)
        for (int c = 0; c < src->cols; c++)
            dst->data[c][r] = src->data[r][c];
}

/** Copy src to dst then swap rows r1 and r2 (0-based). */
static void mat_rowswap(const CalcMatrix_t *src, int r1, int r2, CalcMatrix_t *dst)
{
    *dst = *src;
    for (int c = 0; c < src->cols; c++) {
        float tmp        = dst->data[r1][c];
        dst->data[r1][c] = dst->data[r2][c];
        dst->data[r2][c] = tmp;
    }
}

/** Copy src to dst then add row[r2] into row[r1] (0-based). */
static void mat_rowplus(const CalcMatrix_t *src, int r1, int r2, CalcMatrix_t *dst)
{
    *dst = *src;
    for (int c = 0; c < src->cols; c++)
        dst->data[r1][c] += dst->data[r2][c];
}

/** Copy src to dst then multiply row[r] by scalar k (0-based). */
static void mat_mrow(float k, const CalcMatrix_t *src, int r, CalcMatrix_t *dst)
{
    *dst = *src;
    for (int c = 0; c < src->cols; c++)
        dst->data[r][c] *= k;
}

/** Copy src to dst then add k*row[r2] into row[r1] (0-based). */
static void mat_mrowplus(float k, const CalcMatrix_t *src, int r1, int r2,
                          CalcMatrix_t *dst)
{
    *dst = *src;
    for (int c = 0; c < src->cols; c++)
        dst->data[r1][c] += k * src->data[r2][c];
}

/** Element-wise add: A + B → dst.  Requires identical dimensions. */
static bool mat_add(const CalcMatrix_t *a, const CalcMatrix_t *b, CalcMatrix_t *dst)
{
    if (a->rows != b->rows || a->cols != b->cols) return false;
    dst->rows = a->rows; dst->cols = a->cols;
    for (int r = 0; r < a->rows; r++)
        for (int c = 0; c < a->cols; c++)
            dst->data[r][c] = a->data[r][c] + b->data[r][c];
    return true;
}

/** Element-wise subtract: A - B → dst.  Requires identical dimensions. */
static bool mat_sub(const CalcMatrix_t *a, const CalcMatrix_t *b, CalcMatrix_t *dst)
{
    if (a->rows != b->rows || a->cols != b->cols) return false;
    dst->rows = a->rows; dst->cols = a->cols;
    for (int r = 0; r < a->rows; r++)
        for (int c = 0; c < a->cols; c++)
            dst->data[r][c] = a->data[r][c] - b->data[r][c];
    return true;
}

/** Matrix multiply: A × B → dst.  Requires A.cols == B.rows. */
static bool mat_mul(const CalcMatrix_t *a, const CalcMatrix_t *b, CalcMatrix_t *dst)
{
    if (a->cols != b->rows) return false;
    dst->rows = a->rows; dst->cols = b->cols;
    for (int r = 0; r < a->rows; r++)
        for (int c = 0; c < b->cols; c++) {
            float sum = 0.0f;
            for (int k = 0; k < a->cols; k++)
                sum += a->data[r][k] * b->data[k][c];
            dst->data[r][c] = sum;
        }
    return true;
}

/** Scalar × matrix: k * M → dst. */
static void mat_scale(float k, const CalcMatrix_t *m, CalcMatrix_t *dst)
{
    dst->rows = m->rows; dst->cols = m->cols;
    for (int r = 0; r < m->rows; r++)
        for (int c = 0; c < m->cols; c++)
            dst->data[r][c] = k * m->data[r][c];
}

/*---------------------------------------------------------------------------
 * Stage 3 — RPN evaluator dispatch helpers
 *--------------------------------------------------------------------------*/

static inline void rpn_set_error(CalcResult_t *res, CalcError_t code, const char *msg)
{
    res->error = code;
    strncpy(res->error_msg, msg, sizeof(res->error_msg) - 1);
    res->error_msg[sizeof(res->error_msg) - 1] = '\0';
}

static inline bool rpn_push(float *stack, bool *is_matrix, int *top,
                             float val, bool mat, CalcResult_t *res)
{
    if (*top >= CALC_MAX_STACK - 1) {
        rpn_set_error(res, CALC_ERR_OVERFLOW, "Stack overflow");
        return false;
    }
    stack[++(*top)] = val;
    is_matrix[*top] = mat;
    return true;
}

/* Matrix-specific operations: transpose, det, row ops, round, matrix arithmetic. */
static bool eval_matrix_func(MathTokenType_t type,
                              float *stack, bool *is_matrix, int *top,
                              CalcResult_t *res)
{
#define MERR(code, msg) do { rpn_set_error(res, (code), (msg)); return false; } while(0)

    if (type == MATH_OP_TRANSPOSE) {
        if (*top < 0 || !is_matrix[*top]) MERR(CALC_ERR_SYNTAX, "Syntax error");
        int idx = (int)roundf(stack[*top]);
        if (idx < 0 || idx >= CALC_MATRIX_COUNT) MERR(CALC_ERR_DOMAIN, "Bad matrix index");
        mat_transpose(&calc_matrices[idx], &calc_matrices[3]);
        stack[*top] = 3.0f; is_matrix[*top] = true;
        return true;
    }
    if (type == MATH_FUNC_DET) {
        if (*top < 0 || !is_matrix[*top]) MERR(CALC_ERR_SYNTAX, "Syntax error");
        int idx = (int)roundf(stack[*top]);
        if (idx < 0 || idx >= CALC_MATRIX_COUNT) MERR(CALC_ERR_DOMAIN, "Bad matrix index");
        const CalcMatrix_t *dm = &calc_matrices[idx];
        if (dm->rows != dm->cols) MERR(CALC_ERR_DOMAIN, "det: need square");
        stack[*top] = mat_det(dm); is_matrix[*top] = false;
        return true;
    }
    if (type == MATH_FUNC_ROWSWAP) {
        if (*top < 2) MERR(CALC_ERR_SYNTAX, "Syntax error");
        int r2 = (int)roundf(stack[(*top)--]) - 1; /* 1-based → 0-based */
        int r1 = (int)roundf(stack[(*top)--]) - 1;
        int idx = (int)roundf(stack[*top]);
        if (!is_matrix[*top] || idx < 0 || idx >= CALC_MATRIX_COUNT ||
            r1 < 0 || r1 >= calc_matrices[idx].rows ||
            r2 < 0 || r2 >= calc_matrices[idx].rows)
            MERR(CALC_ERR_DOMAIN, "Bad row index");
        mat_rowswap(&calc_matrices[idx], r1, r2, &calc_matrices[3]);
        stack[*top] = 3.0f; is_matrix[*top] = true;
        return true;
    }
    if (type == MATH_FUNC_ROWPLUS) {
        if (*top < 2) MERR(CALC_ERR_SYNTAX, "Syntax error");
        int r2 = (int)roundf(stack[(*top)--]) - 1;
        int r1 = (int)roundf(stack[(*top)--]) - 1;
        int idx = (int)roundf(stack[*top]);
        if (!is_matrix[*top] || idx < 0 || idx >= CALC_MATRIX_COUNT ||
            r1 < 0 || r1 >= calc_matrices[idx].rows ||
            r2 < 0 || r2 >= calc_matrices[idx].rows)
            MERR(CALC_ERR_DOMAIN, "Bad row index");
        mat_rowplus(&calc_matrices[idx], r1, r2, &calc_matrices[3]);
        stack[*top] = 3.0f; is_matrix[*top] = true;
        return true;
    }
    if (type == MATH_FUNC_MROW) {
        if (*top < 2) MERR(CALC_ERR_SYNTAX, "Syntax error");
        int   r   = (int)roundf(stack[(*top)--]) - 1;
        int   idx = (int)roundf(stack[(*top)--]);
        float k   = stack[*top];
        if (is_matrix[*top] || idx < 0 || idx >= CALC_MATRIX_COUNT ||
            r < 0 || r >= calc_matrices[idx].rows)
            MERR(CALC_ERR_DOMAIN, "Bad row index");
        mat_mrow(k, &calc_matrices[idx], r, &calc_matrices[3]);
        stack[*top] = 3.0f; is_matrix[*top] = true;
        return true;
    }
    if (type == MATH_FUNC_MROWPLUS) {
        if (*top < 3) MERR(CALC_ERR_SYNTAX, "Syntax error");
        int   r2  = (int)roundf(stack[(*top)--]) - 1;
        int   r1  = (int)roundf(stack[(*top)--]) - 1;
        int   idx = (int)roundf(stack[(*top)--]);
        float k   = stack[*top];
        if (is_matrix[*top] || idx < 0 || idx >= CALC_MATRIX_COUNT ||
            r1 < 0 || r1 >= calc_matrices[idx].rows ||
            r2 < 0 || r2 >= calc_matrices[idx].rows)
            MERR(CALC_ERR_DOMAIN, "Bad row index");
        mat_mrowplus(k, &calc_matrices[idx], r1, r2, &calc_matrices[3]);
        stack[*top] = 3.0f; is_matrix[*top] = true;
        return true;
    }
    if (type == MATH_FUNC_ROUND) {
        if (*top < 1) MERR(CALC_ERR_SYNTAX, "Syntax error");
        float decimals = stack[(*top)--];
        float factor   = powf(10.0f, decimals);
        if (is_matrix[*top]) {
            int idx = (int)roundf(stack[*top]);
            if (idx < 0 || idx >= CALC_MATRIX_COUNT) MERR(CALC_ERR_DOMAIN, "Bad matrix index");
            const CalcMatrix_t *src = &calc_matrices[idx];
            CalcMatrix_t       *dst = &calc_matrices[3];
            dst->rows = src->rows; dst->cols = src->cols;
            for (int r = 0; r < src->rows; r++)
                for (int c = 0; c < src->cols; c++)
                    dst->data[r][c] = roundf(src->data[r][c] * factor) / factor;
            stack[*top] = 3.0f; /* is_matrix[*top] stays true */
        } else {
            stack[*top] = roundf(stack[*top] * factor) / factor;
            is_matrix[*top] = false;
        }
        return true;
    }

    /* Matrix arithmetic: ADD, SUB, MUL when at least one operand is a matrix */
    if (*top < 1) MERR(CALC_ERR_SYNTAX, "Syntax error");
    bool  b_mat = is_matrix[*top];
    bool  a_mat = is_matrix[*top - 1];
    float bv = stack[(*top)--];
    float av = stack[*top];
    CalcMatrix_t *dst = &calc_matrices[3];
    if (a_mat && b_mat) {
        int ia = (int)roundf(av), ib = (int)roundf(bv);
        if (ia < 0 || ia >= CALC_MATRIX_COUNT || ib < 0 || ib >= CALC_MATRIX_COUNT)
            MERR(CALC_ERR_DOMAIN, "Bad matrix index");
        bool ok = (type == MATH_OP_ADD) ? mat_add(&calc_matrices[ia], &calc_matrices[ib], dst)
                : (type == MATH_OP_SUB) ? mat_sub(&calc_matrices[ia], &calc_matrices[ib], dst)
                :                         mat_mul(&calc_matrices[ia], &calc_matrices[ib], dst);
        if (!ok) MERR(CALC_ERR_DOMAIN, "Dimension mismatch");
    } else if (type == MATH_OP_MUL) {
        float k   = a_mat ? bv : av;
        int   idx = (int)roundf(a_mat ? av : bv);
        if (idx < 0 || idx >= CALC_MATRIX_COUNT) MERR(CALC_ERR_DOMAIN, "Bad matrix index");
        mat_scale(k, &calc_matrices[idx], dst);
    } else {
        MERR(CALC_ERR_DOMAIN, "Matrix op error");
    }
    stack[*top] = 3.0f;
#undef MERR
    is_matrix[*top] = true;
    return true;
}

/* Scalar unary operators and single-argument math functions. */
static bool eval_unary_func(MathTokenType_t type,
                             float *stack, bool *is_matrix, int *top,
                             float deg_factor, CalcResult_t *res)
{
#define MERR(code, msg) do { rpn_set_error(res, (code), (msg)); return false; } while(0)
    if (*top < 0) MERR(CALC_ERR_SYNTAX, "Syntax error");
    float a = stack[*top];
    switch (type) {
    case MATH_OP_NEG:      stack[*top] = -a;                      break;
    case MATH_OP_FACT: {
        int n = (int)roundf(a);
        if (n < 0) MERR(CALC_ERR_DOMAIN, "Domain error: n!");
        stack[*top] = calc_factorial(n);
        break;
    }
    case MATH_FUNC_SIN:    stack[*top] = sinf(a * deg_factor);    break;
    case MATH_FUNC_COS:    stack[*top] = cosf(a * deg_factor);    break;
    case MATH_FUNC_TAN:    stack[*top] = tanf(a * deg_factor);    break;
    case MATH_FUNC_ASIN:
        if (a < -1.0f || a > 1.0f) MERR(CALC_ERR_DOMAIN, "Domain error: asin");
        stack[*top] = asinf(a) / deg_factor;
        break;
    case MATH_FUNC_ACOS:
        if (a < -1.0f || a > 1.0f) MERR(CALC_ERR_DOMAIN, "Domain error: acos");
        stack[*top] = acosf(a) / deg_factor;
        break;
    case MATH_FUNC_ATAN:   stack[*top] = atanf(a) / deg_factor;  break;
    case MATH_FUNC_LN:
        if (a <= 0.0f) MERR(CALC_ERR_DOMAIN, "Domain error: ln");
        stack[*top] = logf(a);
        break;
    case MATH_FUNC_LOG:
        if (a <= 0.0f) MERR(CALC_ERR_DOMAIN, "Domain error: log");
        stack[*top] = log10f(a);
        break;
    case MATH_FUNC_SQRT:
        if (a < 0.0f) MERR(CALC_ERR_DOMAIN, "Domain error: sqrt");
        stack[*top] = sqrtf(a);
        break;
    case MATH_FUNC_ABS:    stack[*top] = fabsf(a);               break;
    case MATH_FUNC_EXP:    stack[*top] = expf(a);                break;
    case MATH_FUNC_IPART:  stack[*top] = truncf(a);              break;
    case MATH_FUNC_FPART:  stack[*top] = a - truncf(a);          break;
    case MATH_FUNC_INT:    stack[*top] = floorf(a);              break;
    default:               break;
    }
#undef MERR
    is_matrix[*top] = false;
    return true;
}

/* Comparison operators: =, ≠, >, ≥, <, ≤ — always return 0.0 or 1.0. */
static bool eval_comparison(MathTokenType_t type,
                             float *stack, bool *is_matrix, int *top,
                             CalcResult_t *res)
{
    if (*top < 1) { rpn_set_error(res, CALC_ERR_SYNTAX, "Syntax error"); return false; }
    float b = stack[(*top)--];
    float a = stack[*top];
    switch (type) {
    case MATH_OP_EQ:  stack[*top] = (a == b) ? 1.0f : 0.0f; break;
    case MATH_OP_NEQ: stack[*top] = (a != b) ? 1.0f : 0.0f; break;
    case MATH_OP_GT:  stack[*top] = (a >  b) ? 1.0f : 0.0f; break;
    case MATH_OP_GTE: stack[*top] = (a >= b) ? 1.0f : 0.0f; break;
    case MATH_OP_LT:  stack[*top] = (a <  b) ? 1.0f : 0.0f; break;
    case MATH_OP_LTE: stack[*top] = (a <= b) ? 1.0f : 0.0f; break;
    default:          break;
    }
    is_matrix[*top] = false;
    return true;
}

/* Scalar binary arithmetic: +, -, *, /, ^, nPr, nCr. */
static bool eval_binary_op(MathTokenType_t type,
                            float *stack, bool *is_matrix, int *top,
                            CalcResult_t *res)
{
#define MERR(code, msg) do { rpn_set_error(res, (code), (msg)); return false; } while(0)
    if (*top < 1) MERR(CALC_ERR_SYNTAX, "Syntax error");
    float b = stack[(*top)--];
    float a = stack[*top];
    switch (type) {
    case MATH_OP_ADD:  stack[*top] = a + b; break;
    case MATH_OP_SUB:  stack[*top] = a - b; break;
    case MATH_OP_MUL:  stack[*top] = a * b; break;
    case MATH_OP_DIV:
        if (b == 0.0f) MERR(CALC_ERR_DIV_ZERO, "Division by zero");
        stack[*top] = a / b;
        break;
    case MATH_OP_POW:  stack[*top] = powf(a, b); break;
    case MATH_OP_NPR: {
        float v = calc_npr((int)roundf(a), (int)roundf(b));
        if (isnan(v)) MERR(CALC_ERR_DOMAIN, "Domain error: nPr");
        stack[*top] = v;
        break;
    }
    case MATH_OP_NCR: {
        float v = calc_ncr((int)roundf(a), (int)roundf(b));
        if (isnan(v)) MERR(CALC_ERR_DOMAIN, "Domain error: nCr");
        stack[*top] = v;
        break;
    }
    default: break;
    }
#undef MERR
    is_matrix[*top] = false;
    return true;
}

/*---------------------------------------------------------------------------
 * Stage 3 — RPN evaluator
 *--------------------------------------------------------------------------*/

static CalcResult_t EvaluateRPN(const TokenList_t *rpn, bool angle_degrees)
{
    CalcResult_t res = { 0.0f, CALC_OK, "", false, 0 };

    float stack[CALC_MAX_STACK];
    bool  is_matrix[CALC_MAX_STACK];
    int   top = -1;

    float deg_factor = angle_degrees ? (3.14159265358979f / 180.0f) : 1.0f;

    for (int i = 0; i < rpn->count; i++) {
        MathToken_t     tok = rpn->tokens[i];
        MathTokenType_t tt  = tok.type;

        if (tt == MATH_NUMBER) {
            if (!rpn_push(stack, is_matrix, &top, tok.value, false, &res)) return res;
            continue;
        }
        if (tt == MATH_MATRIX_VAL) {
            if (!rpn_push(stack, is_matrix, &top, tok.value, true, &res)) return res;
            continue;
        }

        /* Route ADD/SUB/MUL to matrix handler when either operand is a matrix */
        bool mat_arith = (tt == MATH_OP_ADD || tt == MATH_OP_SUB || tt == MATH_OP_MUL)
                         && top >= 1 && (is_matrix[top] || is_matrix[top - 1]);

        bool ok;
        if (tt == MATH_OP_TRANSPOSE || tt == MATH_FUNC_DET     ||
            tt == MATH_FUNC_ROWSWAP || tt == MATH_FUNC_ROWPLUS  ||
            tt == MATH_FUNC_MROW    || tt == MATH_FUNC_MROWPLUS ||
            tt == MATH_FUNC_ROUND   || mat_arith)
        {
            ok = eval_matrix_func(tt, stack, is_matrix, &top, &res);
        } else if (tt == MATH_OP_NEG || tt == MATH_OP_FACT || is_function(tt)) {
            ok = eval_unary_func(tt, stack, is_matrix, &top, deg_factor, &res);
        } else if (tt == MATH_OP_EQ  || tt == MATH_OP_NEQ || tt == MATH_OP_GT  ||
                   tt == MATH_OP_GTE || tt == MATH_OP_LT  || tt == MATH_OP_LTE) {
            ok = eval_comparison(tt, stack, is_matrix, &top, &res);
        } else {
            ok = eval_binary_op(tt, stack, is_matrix, &top, &res);
        }

        if (!ok) return res;
    }

    if (top != 0) {
        res.error = CALC_ERR_SYNTAX;
        strncpy(res.error_msg, "Syntax error", sizeof(res.error_msg) - 1);
        return res;
    }

    if (is_matrix[0]) {
        res.has_matrix = true;
        res.matrix_idx = (uint8_t)(int)roundf(stack[0]);
    } else {
        res.value = stack[0];
    }
    return res;
}

/*---------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief Evaluates an infix expression string.
 *
 * Passes the stored value of variable X as x_val so that bare references
 * to "X" in regular (non-graph) mode use whatever was last stored via STO→X.
 *
 * @param expr          Null-terminated infix expression e.g. "3+sin(45)*2"
 * @param ans           Current ANS value substituted for "ANS" in expression
 * @param angle_degrees True for degrees, false for radians
 * @return              CalcResult_t containing value or error
 */
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool ans_is_matrix,
                           bool angle_degrees)
{
    CalcResult_t res = { 0.0f, CALC_OK, "", false, 0 };


    if (expr == NULL || strlen(expr) == 0) {
        res.error = CALC_ERR_SYNTAX;
        strncpy(res.error_msg, "Empty expression",
                sizeof(res.error_msg) - 1);
        return res;
    }

    TokenList_t infix  = { .count = 0 };
    TokenList_t postfix = { .count = 0 };

    CalcError_t err = Tokenize(expr, ans, ans_is_matrix,
                               calc_variables['X' - 'A'], &infix);
    if (err != CALC_OK) {
        res.error = err;
        strncpy(res.error_msg, "Tokenize error",
                sizeof(res.error_msg) - 1);
        return res;
    }

    err = ImplicitMulPass(&infix);
    if (err != CALC_OK) {
        res.error = err;
        strncpy(res.error_msg, "Expression too long",
                sizeof(res.error_msg) - 1);
        return res;
    }

    err = ShuntingYard(&infix, &postfix);
    if (err != CALC_OK) {
        res.error = err;
        strncpy(res.error_msg, "Syntax error",
                sizeof(res.error_msg) - 1);
        return res;
    }

    return EvaluateRPN(&postfix, angle_degrees);
}

/**
 * @brief Evaluates an infix expression with a specific value substituted for X.
 *
 * Used by the graphing subsystem so that each pixel column can pass its own
 * x coordinate, independent of the stored variable value.
 *
 * @param expr          Null-terminated infix expression in terms of x
 * @param x_val         Value to substitute for the variable x/X
 * @param ans           Current ANS value substituted for "ANS" in expression
 * @param angle_degrees True for degrees, false for radians
 * @return              CalcResult_t containing value or error
 */
CalcResult_t Calc_EvaluateAt(const char *expr, float x_val,
                              float ans, bool angle_degrees)
{
    CalcResult_t res = { 0.0f, CALC_OK, "", false, 0 };

    if (expr == NULL || strlen(expr) == 0) {
        res.error = CALC_ERR_SYNTAX;
        strncpy(res.error_msg, "Empty expression",
                sizeof(res.error_msg) - 1);
        return res;
    }
    TokenList_t infix   = { .count = 0 };
    TokenList_t postfix = { .count = 0 };
    /* Graph context is always scalar — ANS cannot be a matrix in Y= equations */
    CalcError_t err = Tokenize(expr, ans, false, x_val, &infix);
    if (err != CALC_OK) {
        res.error = err;
        strncpy(res.error_msg, "Tokenize error",
                sizeof(res.error_msg) - 1);
        return res;
    }
    err = ImplicitMulPass(&infix);
    if (err != CALC_OK) {
        res.error = err;
        strncpy(res.error_msg, "Expression too long",
                sizeof(res.error_msg) - 1);
        return res;
    }
    err = ShuntingYard(&infix, &postfix);
    if (err != CALC_OK) {
        res.error = err;
        strncpy(res.error_msg, "Syntax error",
                sizeof(res.error_msg) - 1);
        return res;
    }
    return EvaluateRPN(&postfix, angle_degrees);
}

/**
 * @brief Formats a float result into a clean display string.
 *
 * Produces integers without a decimal point, trims trailing zeros from
 * decimal results, and switches to scientific notation for values outside
 * the range [1e-4, 1e7).  Uses explicit %.6f rather than %.6g because
 * newlib-nano's %g formatting is unreliable on ARM targets.
 *
 * @param value   Float to format
 * @param buf     Output buffer
 * @param buf_len Buffer size in bytes
 */
void Calc_FormatResult(float value, char *buf, uint8_t buf_len)
{
    /* Fixed-decimal mode: fix_decimals >= 0; Float mode: fix_decimals = -1 */
    int fix_decimals = (calc_decimal_mode > 0) ? (int)calc_decimal_mode - 1 : -1;

    /* Scientific notation for very large or very small non-zero values */
    if (fabsf(value) >= 1e7f || (fabsf(value) < 1e-4f && value != 0.0f)) {
        if (fix_decimals < 0) {
            snprintf(buf, buf_len, "%.4e", value);
        } else {
            char fmt[8];
            snprintf(fmt, sizeof(fmt), "%%.%de", fix_decimals);
            snprintf(buf, buf_len, fmt, value);
        }
        return;
    }

    if (fix_decimals < 0) {
        /* Float mode: show integers without decimal point; trim trailing zeros */
        float rounded = roundf(value);
        if (fabsf(value - rounded) < 1e-4f &&
            rounded >= -9999999.0f && rounded <= 9999999.0f) {
            snprintf(buf, buf_len, "%d", (int)rounded);
            return;
        }

        snprintf(buf, buf_len, "%.6f", value);

        char *dot = strchr(buf, '.');
        if (dot != NULL) {
            char *end = buf + strlen(buf) - 1;
            while (end > dot && *end == '0') {
                *end-- = '\0';
            }
            if (*end == '.') {
                *end = '\0';
            }
        }
    } else {
        /* Fix N mode: always show exactly N decimal places */
        char fmt[8];
        snprintf(fmt, sizeof(fmt), "%%.%df", fix_decimals);
        snprintf(buf, buf_len, fmt, value);
    }
}
