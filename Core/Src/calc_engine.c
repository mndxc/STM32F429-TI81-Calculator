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
            t == MATH_OP_POW || t == MATH_OP_NEG);
}

static bool is_function(MathTokenType_t t)
{
    return (t == MATH_FUNC_SIN  || t == MATH_FUNC_COS  ||
            t == MATH_FUNC_TAN  || t == MATH_FUNC_ASIN ||
            t == MATH_FUNC_ACOS || t == MATH_FUNC_ATAN ||
            t == MATH_FUNC_LN   || t == MATH_FUNC_LOG  ||
            t == MATH_FUNC_SQRT || t == MATH_FUNC_ABS  ||
            t == MATH_FUNC_EXP);
}

static int precedence(MathTokenType_t t)
{
    switch (t) {
    case MATH_OP_ADD:
    case MATH_OP_SUB: return 1;
    case MATH_OP_MUL:
    case MATH_OP_DIV: return 2;
    case MATH_OP_NEG: return 3;
    case MATH_OP_POW: return 4;
    default:          return 0;
    }
}

static bool is_right_assoc(MathTokenType_t t)
{
    /* Power and unary negation are right associative */
    return (t == MATH_OP_POW || t == MATH_OP_NEG);
}

/*---------------------------------------------------------------------------
 * Stage 1 — Tokenizer
 *--------------------------------------------------------------------------*/

static CalcError_t Tokenize(const char *expr, float ans, float x_val,
                             TokenList_t *out)
{
    out->count = 0;
    const char *p = expr;

    while (*p != '\0') {

        /* Skip whitespace */
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }

        /* Number */
        if (isdigit((unsigned char)*p) || *p == '.') {
            char   *end;
            float   val = strtof(p, &end);
            if (end == p)
                return CALC_ERR_SYNTAX;
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = val;
            out->count++;
            p = end;
            continue;
        }

        /* ANS variable */
        if (strncmp(p, "ANS", 3) == 0) {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = ans;
            out->count++;
            p += 3;
            continue;
        }

        /* X variable */
        if (*p == 'x' || *p == 'X') {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = x_val;
            out->count++;
            p += 1;
            continue;
        }

        /* User variables A–Z (uppercase; skip X — handled above as graph var) */
        if (isupper((unsigned char)*p) && *p != 'X') {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = calc_variables[*p - 'A'];
            out->count++;
            p += 1;
            continue;
        }

        /* pi constant */
        if (strncmp(p, "pi", 2) == 0) {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = 3.14159265358979f;
            out->count++;
            p += 2;
            continue;
        }

        /* Named functions */
        struct { const char *name; MathTokenType_t type; } funcs[] = {
            { "sin",  MATH_FUNC_SIN  },
            { "cos",  MATH_FUNC_COS  },
            { "tan",  MATH_FUNC_TAN  },
            { "asin", MATH_FUNC_ASIN },
            { "acos", MATH_FUNC_ACOS },
            { "atan", MATH_FUNC_ATAN },
            { "ln",   MATH_FUNC_LN   },
            { "log",  MATH_FUNC_LOG  },
            { "sqrt", MATH_FUNC_SQRT },
            { "abs",  MATH_FUNC_ABS  },
            { "exp",  MATH_FUNC_EXP  },
        };

        bool matched = false;
        for (int i = 0; i < 11; i++) {
            size_t len = strlen(funcs[i].name);
            if (strncmp(p, funcs[i].name, len) == 0) {
                if (out->count >= CALC_MAX_TOKENS)
                    return CALC_ERR_OVERFLOW;
                out->tokens[out->count].type  = funcs[i].type;
                out->tokens[out->count].value = 0;
                out->count++;
                p += len;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        /* e constant — Euler's number (must be after named-function check so
           "exp" is consumed above before we reach here) */
        if (*p == 'e') {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = 2.71828182845905f;
            out->count++;
            p++;
            continue;
        }

        /* Special case: '-' immediately after '^' before a digit or dot —
           fold the sign into the number literal so shunting-yard never
           tries to pop '^' before its right operand is ready.
           e.g. 2^-3  →  tokens [2, ^, -3]  rather than [2, ^, NEG, 3]. */
        if (*p == '-' &&
            out->count > 0 &&
            out->tokens[out->count - 1].type == MATH_OP_POW &&
            (isdigit((unsigned char)p[1]) || p[1] == '.')) {
            char *end;
            float val = strtof(p, &end);
            if (end == p) return CALC_ERR_SYNTAX;
            if (out->count >= CALC_MAX_TOKENS) return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = MATH_NUMBER;
            out->tokens[out->count].value = val;
            out->count++;
            p = end;
            continue;
        }

        /* Single character tokens */
        MathTokenType_t op_type;
        bool is_op = true;

        switch (*p) {
        case '+': op_type = MATH_OP_ADD;      break;
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
        case '(': op_type = MATH_PAREN_LEFT;  break;
        case ')': op_type = MATH_PAREN_RIGHT; break;
        default:  is_op   = false;             break;
        }

        if (is_op) {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count].type  = op_type;
            out->tokens[out->count].value = 0;
            out->count++;
            p++;
            continue;
        }

        /* Unknown character */
        return CALC_ERR_SYNTAX;
    }

    return CALC_OK;
}

/*---------------------------------------------------------------------------
 * Stage 1b — Implicit multiplication pass
 *
 * Inserts MATH_OP_MUL between adjacent tokens where implicit multiplication
 * is implied: e.g. 2sin(x), 2(3+4), (a+b)(c+d), 2pi, 3ANS.
 *
 * Rule: insert '*' when left token is NUMBER or ')' AND
 *       right token is NUMBER, '(', or any function.
 *--------------------------------------------------------------------------*/

static CalcError_t ImplicitMulPass(TokenList_t *list)
{
    for (int i = 0; i < (int)list->count - 1; i++) {
        MathTokenType_t cur  = list->tokens[i].type;
        MathTokenType_t next = list->tokens[i + 1].type;

        bool cur_is_value  = (cur  == MATH_NUMBER || cur  == MATH_PAREN_RIGHT);
        bool next_is_factor = (next == MATH_NUMBER || next == MATH_PAREN_LEFT ||
                               is_function(next));

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

        if (tok.type == MATH_NUMBER) {
            if (out->count >= CALC_MAX_TOKENS)
                return CALC_ERR_OVERFLOW;
            out->tokens[out->count++] = tok;
        }
        else if (is_function(tok.type)) {
            if (op_top >= CALC_MAX_STACK - 1)
                return CALC_ERR_OVERFLOW;
            op_stack[++op_top] = tok;
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
 * Stage 3 — RPN evaluator
 *--------------------------------------------------------------------------*/

static CalcResult_t EvaluateRPN(const TokenList_t *rpn, bool angle_degrees)
{
    CalcResult_t res = { 0.0f, CALC_OK, "" };
    float        stack[CALC_MAX_STACK];
    int          top = -1;

    float deg_factor = angle_degrees ? (3.14159265358979f / 180.0f) : 1.0f;

    for (int i = 0; i < rpn->count; i++) {
        MathToken_t tok = rpn->tokens[i];

        if (tok.type == MATH_NUMBER) {
            if (top >= CALC_MAX_STACK - 1) {
                res.error = CALC_ERR_OVERFLOW;
                strncpy(res.error_msg, "Stack overflow",
                        sizeof(res.error_msg) - 1);
                return res;
            }
            stack[++top] = tok.value;
            continue;
        }

        /* Unary operators and functions — need one operand */
        if (tok.type == MATH_OP_NEG || is_function(tok.type)) {
            if (top < 0) {
                res.error = CALC_ERR_SYNTAX;
                strncpy(res.error_msg, "Syntax error",
                        sizeof(res.error_msg) - 1);
                return res;
            }
            float a = stack[top];

            switch (tok.type) {
            case MATH_OP_NEG:      stack[top] = -a;                      break;
            case MATH_FUNC_SIN:    stack[top] = sinf(a * deg_factor);    break;
            case MATH_FUNC_COS:    stack[top] = cosf(a * deg_factor);    break;
            case MATH_FUNC_TAN:    stack[top] = tanf(a * deg_factor);    break;
            case MATH_FUNC_ASIN:
                if (a < -1.0f || a > 1.0f) {
                    res.error = CALC_ERR_DOMAIN;
                    strncpy(res.error_msg, "Domain error: asin",
                            sizeof(res.error_msg) - 1);
                    return res;
                }
                stack[top] = asinf(a) / deg_factor;
                break;
            case MATH_FUNC_ACOS:
                if (a < -1.0f || a > 1.0f) {
                    res.error = CALC_ERR_DOMAIN;
                    strncpy(res.error_msg, "Domain error: acos",
                            sizeof(res.error_msg) - 1);
                    return res;
                }
                stack[top] = acosf(a) / deg_factor;
                break;
            case MATH_FUNC_ATAN:   stack[top] = atanf(a) / deg_factor;  break;
            case MATH_FUNC_LN:
                if (a <= 0.0f) {
                    res.error = CALC_ERR_DOMAIN;
                    strncpy(res.error_msg, "Domain error: ln",
                            sizeof(res.error_msg) - 1);
                    return res;
                }
                stack[top] = logf(a);
                break;
            case MATH_FUNC_LOG:
                if (a <= 0.0f) {
                    res.error = CALC_ERR_DOMAIN;
                    strncpy(res.error_msg, "Domain error: log",
                            sizeof(res.error_msg) - 1);
                    return res;
                }
                stack[top] = log10f(a);
                break;
            case MATH_FUNC_SQRT:
                if (a < 0.0f) {
                    res.error = CALC_ERR_DOMAIN;
                    strncpy(res.error_msg, "Domain error: sqrt",
                            sizeof(res.error_msg) - 1);
                    return res;
                }
                stack[top] = sqrtf(a);
                break;
            case MATH_FUNC_ABS:    stack[top] = fabsf(a);               break;
            case MATH_FUNC_EXP:    stack[top] = expf(a);                break;
            default:               break;
            }
            continue;
        }

        /* Binary operators — need two operands */
        if (top < 1) {
            res.error = CALC_ERR_SYNTAX;
            strncpy(res.error_msg, "Syntax error",
                    sizeof(res.error_msg) - 1);
            return res;
        }
        float b = stack[top--];
        float a = stack[top];

        switch (tok.type) {
        case MATH_OP_ADD:
            stack[top] = a + b;
            break;
        case MATH_OP_SUB:
            stack[top] = a - b;
            break;
        case MATH_OP_MUL:
            stack[top] = a * b;
            break;
        case MATH_OP_DIV:
            if (b == 0.0f) {
                res.error = CALC_ERR_DIV_ZERO;
                strncpy(res.error_msg, "Division by zero",
                        sizeof(res.error_msg) - 1);
                return res;
            }
            stack[top] = a / b;
            break;
        case MATH_OP_POW:
            stack[top] = powf(a, b);
            break;
        default:
            break;
        }
    }

    if (top != 0) {
        res.error = CALC_ERR_SYNTAX;
        strncpy(res.error_msg, "Syntax error",
                sizeof(res.error_msg) - 1);
        return res;
    }

    res.value = stack[0];
    return res;
}

/*---------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief Evaluates an infix expression string.
 */
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool angle_degrees)
{
    CalcResult_t res = { 0.0f, CALC_OK, "" };

    if (expr == NULL || strlen(expr) == 0) {
        res.error = CALC_ERR_SYNTAX;
        strncpy(res.error_msg, "Empty expression",
                sizeof(res.error_msg) - 1);
        return res;
    }

    TokenList_t infix  = { .count = 0 };
    TokenList_t postfix = { .count = 0 };

    CalcError_t err = Tokenize(expr, ans, calc_variables['X' - 'A'], &infix);
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

CalcResult_t Calc_EvaluateAt(const char *expr, float x_val,
                              float ans, bool angle_degrees)
{
    CalcResult_t res = { 0.0f, CALC_OK, "" };
    if (expr == NULL || strlen(expr) == 0) {
        res.error = CALC_ERR_SYNTAX;
        strncpy(res.error_msg, "Empty expression",
                sizeof(res.error_msg) - 1);
        return res;
    }
    TokenList_t infix   = { .count = 0 };
    TokenList_t postfix = { .count = 0 };
    CalcError_t err = Tokenize(expr, ans, x_val, &infix);
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
 */
void Calc_FormatResult(float value, char *buf, uint8_t buf_len)
{
    /* Use scientific notation for very large or very small values */
    if (fabsf(value) >= 1e7f || (fabsf(value) < 1e-4f && value != 0.0f)) {
        snprintf(buf, buf_len, "%.4e", value);
        return;
    }

    /* Check for integer result using epsilon comparison */
    float rounded = roundf(value);
    if (fabsf(value - rounded) < 1e-4f &&
        rounded >= -9999999.0f && rounded <= 9999999.0f) {
        snprintf(buf, buf_len, "%d", (int)rounded);
        return;
    }

    /* Standard decimal — print with 6 decimal places then trim zeros */
    snprintf(buf, buf_len, "%.6f", value);

    /* Trim trailing zeros after decimal point */
    char *dot = strchr(buf, '.');
    if (dot != NULL) {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        /* Remove trailing dot if all decimals were zero */
        if (*end == '.') {
            *end = '\0';
        }
    }
}
