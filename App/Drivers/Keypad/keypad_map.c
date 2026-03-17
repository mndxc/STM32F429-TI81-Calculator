/**
 * @file    keypad_map.c
 * @brief   TI-81 hardware key to token lookup table.
 *
 * Maps each physical key ID (from Keypad_Scan) to its three token layers:
 *   - normal: primary key function
 *   - second: function when 2nd modifier is active (yellow labels)
 *   - alpha:  function when ALPHA modifier is active (letters)
 *
 * Entries use designated initialisers indexed by hardware ID enum values
 * from keypad.h. Unpopulated matrix positions (UNUSED keys) have no entry
 * and default to zero (TOKEN_NONE across all layers).
 */

#include "keypad_map.h"
#include "keypad.h"

const KeyDefinition_t TI81_LookupTable[] = {

    /* --- Function row (top) --- */
    [ID_B5_A7] = { TOKEN_Y_EQUALS, TOKEN_NONE,   TOKEN_NONE  },
    [ID_B4_A7] = { TOKEN_RANGE,    TOKEN_NONE,   TOKEN_NONE  },
    [ID_B3_A7] = { TOKEN_ZOOM,     TOKEN_NONE,   TOKEN_NONE  },
    [ID_B2_A7] = { TOKEN_TRACE,    TOKEN_NONE,   TOKEN_NONE  },
    [ID_B1_A7] = { TOKEN_GRAPH,    TOKEN_NONE,   TOKEN_NONE  },

    /* --- Edit row --- */
    [ID_B6_A7] = { TOKEN_2ND,      TOKEN_NONE,   TOKEN_NONE  },
    [ID_B7_A7] = { TOKEN_INS,      TOKEN_NONE,   TOKEN_NONE  },
    [ID_B8_A7] = { TOKEN_DEL,      TOKEN_NONE,   TOKEN_NONE  },

    /* --- Directional pad --- */
    [ID_B4_A6] = { TOKEN_UP,       TOKEN_NONE,   TOKEN_NONE  },
    [ID_B1_A6] = { TOKEN_DOWN,     TOKEN_NONE,   TOKEN_NONE  },
    [ID_B2_A6] = { TOKEN_LEFT,     TOKEN_NONE,   TOKEN_NONE  },
    [ID_B3_A6] = { TOKEN_RIGHT,    TOKEN_NONE,   TOKEN_NONE  },

    /* --- Top left cluster --- */
    [ID_B8_A1] = { TOKEN_ALPHA,    TOKEN_A_LOCK, TOKEN_NONE  },
    [ID_B8_A2] = { TOKEN_X_T,      TOKEN_NONE,   TOKEN_NONE  },
    [ID_B8_A3] = { TOKEN_MODE,     TOKEN_NONE,   TOKEN_NONE  },
    [ID_B8_A4] = { TOKEN_ON,       TOKEN_ON,     TOKEN_NONE  }, /* Placeholder ON mapping for Discovery board */

    /* --- Menu row --- */
    [ID_B7_A1] = { TOKEN_MATH,     TOKEN_TEST,   TOKEN_A     },
    [ID_B7_A2] = { TOKEN_MATRX,    TOKEN_STAT,   TOKEN_B     },
    [ID_B7_A3] = { TOKEN_PRGM,     TOKEN_DRAW,   TOKEN_C     },
    [ID_B7_A4] = { TOKEN_VARS,     TOKEN_Y_VARS, TOKEN_NONE  },
    [ID_B7_A5] = { TOKEN_CLEAR,    TOKEN_QUIT,   TOKEN_NONE  },

    /* --- Trig and advanced functions --- */
    [ID_B6_A1] = { TOKEN_X_INV,    TOKEN_ABS,    TOKEN_D     },
    [ID_B6_A2] = { TOKEN_SIN,      TOKEN_ASIN,   TOKEN_E     },
    [ID_B6_A3] = { TOKEN_COS,      TOKEN_ACOS,   TOKEN_F     },
    [ID_B6_A4] = { TOKEN_TAN,      TOKEN_ATAN,   TOKEN_G     },
    [ID_B6_A5] = { TOKEN_POWER,    TOKEN_PI,     TOKEN_H     },

    /* --- Powers, parentheses and division --- */
    [ID_B5_A1] = { TOKEN_SQUARE,   TOKEN_SQRT,   TOKEN_I     },
    [ID_B5_A2] = { TOKEN_EE,       TOKEN_NONE,   TOKEN_J     },
    [ID_B5_A3] = { TOKEN_L_PAR,    TOKEN_NONE,   TOKEN_K     },
    [ID_B5_A4] = { TOKEN_R_PAR,    TOKEN_NONE,   TOKEN_L     },
    [ID_B5_A5] = { TOKEN_DIV,      TOKEN_NONE,   TOKEN_M     },

    /* --- Log, 7 8 9 and multiply --- */
    [ID_B4_A1] = { TOKEN_LOG,      TOKEN_TEN_X,  TOKEN_N     },
    [ID_B4_A2] = { TOKEN_7,        TOKEN_NONE,   TOKEN_O     },
    [ID_B4_A3] = { TOKEN_8,        TOKEN_NONE,   TOKEN_P     },
    [ID_B4_A4] = { TOKEN_9,        TOKEN_NONE,   TOKEN_Q     },
    [ID_B4_A5] = { TOKEN_MULT,     TOKEN_NONE,   TOKEN_R     },

    /* --- Ln, 4 5 6 and subtract --- */
    [ID_B3_A1] = { TOKEN_LN,       TOKEN_E_X,    TOKEN_S     },
    [ID_B3_A2] = { TOKEN_4,        TOKEN_NONE,   TOKEN_T     },
    [ID_B3_A3] = { TOKEN_5,        TOKEN_NONE,   TOKEN_U     },
    [ID_B3_A4] = { TOKEN_6,        TOKEN_NONE,   TOKEN_V     },
    [ID_B3_A5] = { TOKEN_SUB,      TOKEN_NONE,   TOKEN_W     },

    /* --- STO, 1 2 3 and add --- */
    [ID_B2_A1] = { TOKEN_STO,      TOKEN_NONE,   TOKEN_X     },
    [ID_B2_A2] = { TOKEN_1,        TOKEN_MTRX_A, TOKEN_Y     },
    [ID_B2_A3] = { TOKEN_2,        TOKEN_MTRX_B, TOKEN_Z     },
    [ID_B2_A4] = { TOKEN_3,        TOKEN_MTRX_C, TOKEN_THETA },
    [ID_B2_A5] = { TOKEN_ADD,      TOKEN_RESET,  TOKEN_QUOTES},

    /* --- Bottom row: 0, decimal, negate, enter --- */
    [ID_B1_A2] = { TOKEN_0,        TOKEN_LIST_X, TOKEN_SPACE },
    [ID_B1_A3] = { TOKEN_DECIMAL,  TOKEN_LIST_Y, TOKEN_COMMA },
    [ID_B1_A4] = { TOKEN_NEG,      TOKEN_ANS,    TOKEN_QSTN_M},
    [ID_B1_A5] = { TOKEN_ENTER,    TOKEN_ENTRY,  TOKEN_NONE  },
};


const uint32_t TI81_LookupTable_Size =
    sizeof(TI81_LookupTable) / sizeof(TI81_LookupTable[0]);