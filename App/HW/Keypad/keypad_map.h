/**
 * @file    keypad_map.h
 * @brief   TI-81 token definitions and hardware key lookup table.
 *
 * Defines the complete Token_t enum representing every logical function
 * available on the TI-81 calculator across all input layers (normal, 2nd,
 * alpha). Also declares the lookup table that maps hardware key IDs to
 * their token triples.
 */

#ifndef KEYPAD_MAP_H
#define KEYPAD_MAP_H

#include "keypad.h" /* KeyDefinition_t */

/*---------------------------------------------------------------------------
 * Token definitions
 *--------------------------------------------------------------------------*/

/**
 * @brief Logical calculator tokens.
 *
 * Each token represents a single calculator function regardless of which
 * physical key or input layer produced it. The calculator core operates
 * entirely on tokens — it has no knowledge of the hardware key matrix.
 *
 * Ordering within groups is intentional:
 *   TOKEN_0..TOKEN_9 are contiguous so arithmetic (t - TOKEN_0) works.
 */
typedef enum {

    TOKEN_NONE = 0,

    /* Numbers — contiguous, do not reorder */
    TOKEN_0, TOKEN_1, TOKEN_2, TOKEN_3, TOKEN_4,
    TOKEN_5, TOKEN_6, TOKEN_7, TOKEN_8, TOKEN_9,

    /* Numeric input */
    TOKEN_DECIMAL,  /* .              */
    TOKEN_NEG,      /* (-) negation   */
    TOKEN_EE,       /* Scientific notation */

    /* Basic operators */
    TOKEN_ADD,      /* +              */
    TOKEN_SUB,      /* -              */
    TOKEN_MULT,     /* *              */
    TOKEN_DIV,      /* /              */
    TOKEN_POWER,    /* ^ caret        */
    TOKEN_ENTER,    /* ENTER / equals */

    /* Grouping */
    TOKEN_L_PAR,    /* (              */
    TOKEN_R_PAR,    /* )              */

    /* Trig functions — normal layer */
    TOKEN_SIN,
    TOKEN_COS,
    TOKEN_TAN,

    /* Trig functions — 2nd layer */
    TOKEN_ASIN,
    TOKEN_ACOS,
    TOKEN_ATAN,

    /* Logarithmic */
    TOKEN_LN,
    TOKEN_LOG,

    /* Exponential — 2nd layer */
    TOKEN_E_X,      /* e^x            */
    TOKEN_TEN_X,    /* 10^x           */

    /* Powers and roots */
    TOKEN_SQRT,     /* Square root    */
    TOKEN_SQUARE,   /* x²             */
    TOKEN_X_INV,    /* x⁻¹            */

    /* Constants */
    TOKEN_PI,       /* π              */
    TOKEN_ABS,

    /* Calculator control */
    TOKEN_2ND,
    TOKEN_ALPHA,
    TOKEN_A_LOCK,
    TOKEN_MODE,
    TOKEN_CLEAR,
    TOKEN_DEL,
    TOKEN_INS,
    TOKEN_ON,
    TOKEN_STO,

    /* Results */
    TOKEN_ANS,      /* 2nd + (-)      */
    TOKEN_ENTRY,    /* 2nd + ENTER    */

    /* System */
    TOKEN_QUIET,    /* 2nd + MODE     */
    TOKEN_RESET,

    /* Navigation */
    TOKEN_UP,
    TOKEN_DOWN,
    TOKEN_LEFT,
    TOKEN_RIGHT,

    /* Graphing */
    TOKEN_Y_EQUALS,
    TOKEN_RANGE,
    TOKEN_ZOOM,
    TOKEN_TRACE,
    TOKEN_GRAPH,

    /* Variables and memory */
    TOKEN_X_T,      /* X|T variable   */
    TOKEN_VARS,
    TOKEN_Y_VARS,
    TOKEN_STAT,
    TOKEN_SET_UP,
    TOKEN_P_MEM,
    TOKEN_DRAW,
    TOKEN_ANS_VAR,

    /* Menus */
    TOKEN_PRGM,
    TOKEN_MATRX,
    TOKEN_MATH,
    TOKEN_TEST,
    TOKEN_QUIT,

    /* Matrix variables */
    TOKEN_MTRX_A,
    TOKEN_MTRX_B,
    TOKEN_MTRX_C,

    /* List variables */
    TOKEN_LIST_X,
    TOKEN_LIST_Y,

    /* Alpha characters — A through Z */
    TOKEN_A, TOKEN_B, TOKEN_C, TOKEN_D, TOKEN_E, TOKEN_F, TOKEN_G,
    TOKEN_H, TOKEN_I, TOKEN_J, TOKEN_K, TOKEN_L, TOKEN_M, TOKEN_N,
    TOKEN_O, TOKEN_P, TOKEN_Q, TOKEN_R, TOKEN_S, TOKEN_T, TOKEN_U,
    TOKEN_V, TOKEN_W, TOKEN_X, TOKEN_Y, TOKEN_Z,

    /* Alpha punctuation */
    TOKEN_THETA,    /* Alpha + 3      */
    TOKEN_SPACE,    /* Alpha + 0      */
    TOKEN_QUOTES,   /* Alpha + +      */
    TOKEN_COMMA,    /* Alpha + .      */
    TOKEN_QSTN_M,   /* Alpha + (-)    */

    /* PRGM control-flow and I/O tokens — produced only by in-editor sub-menus */
    TOKEN_PRGM_IF,
    TOKEN_PRGM_THEN,
    TOKEN_PRGM_ELSE,
    TOKEN_PRGM_END,
    TOKEN_PRGM_WHILE,
    TOKEN_PRGM_FOR,
    TOKEN_PRGM_GOTO,
    TOKEN_PRGM_LBL,
    TOKEN_PRGM_PAUSE,
    TOKEN_PRGM_STOP,
    TOKEN_PRGM_RETURN,
    TOKEN_PRGM_CALL,      /* prgm<NAME> — invoke another program */
    TOKEN_PRGM_DISP,
    TOKEN_PRGM_INPUT,
    TOKEN_PRGM_PROMPT,
    TOKEN_PRGM_CLRHOME,

    TOKEN_MAX       /* Sentinel — useful for array sizing */

} Token_t;

/*---------------------------------------------------------------------------
 * Lookup table
 *--------------------------------------------------------------------------*/

/**
 * @brief Maps hardware key IDs to their token triples (normal, 2nd, alpha).
 *
 * Indexed directly by the key ID returned from Keypad_Scan().
 * Defined in keypad_map.c.
 */
extern const KeyDefinition_t TI81_LookupTable[];
extern const uint32_t TI81_LookupTable_Size;

#endif /* KEYPAD_MAP_H */