/**
 * @file    app_common.h
 * @brief   Shared types, handles and function declarations for the calculator.
 *
 * This header is included by all application modules to provide a common
 * interface. It intentionally keeps dependencies minimal — only types and
 * declarations that are genuinely shared across multiple modules belong here.
 */

#ifndef APP_COMMON_H
#define APP_COMMON_H

#ifndef HOST_TEST
#include "cmsis_os.h"   /* FreeRTOS queue and task types */
#endif
#include "keypad_map.h" /* Token_t and KeyDefinition_t */
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Shared types
 *---------------------------------------------------------------------------*/

/**
 * @brief Calculator input mode — controls which token layer is active.
 *
 * MODE_NORMAL: Standard key function
 * MODE_2ND:    2nd function layer (sticky, resets after one keypress)
 * MODE_ALPHA:  Alpha character layer (sticky, resets after one keypress)
 */
typedef enum {
    MODE_NORMAL,
    MODE_2ND,
    MODE_ALPHA,
    MODE_ALPHA_LOCK,    /* ALPHA locked — stays active after each keypress */
    MODE_GRAPH_YEQ,     /* Y= equation editor active */
    MODE_GRAPH_RANGE,   /* RANGE field editor active */
    MODE_GRAPH_ZOOM,    /* ZOOM preset menu active */
    MODE_GRAPH_TRACE,   /* Trace cursor active on graph */
    MODE_GRAPH_ZBOX,    /* ZBox rubber-band zoom active */
    MODE_MODE_SCREEN,        /* MODE settings screen active */
    MODE_MATH_MENU,          /* MATH/NUM/HYP/PRB menu active */
    MODE_GRAPH_ZOOM_FACTORS, /* ZOOM FACTORS sub-screen active */
    MODE_TEST_MENU,          /* TEST comparison-operator menu active */
    MODE_MATRIX_MENU,        /* MATRIX/EDIT tabs active */
    MODE_MATRIX_EDIT,        /* Matrix cell editor active */
    MODE_PRGM_MENU,          /* PRGM EXEC/EDIT/NEW tab selection */
    MODE_PRGM_EDITOR,        /* Program line editor */
    MODE_PRGM_CTL_MENU,      /* PRGM CTL sub-menu (Lbl, Goto, If…) */
    MODE_PRGM_IO_MENU,       /* PRGM I/O sub-menu (Disp, Input…) */
    MODE_PRGM_EXEC_MENU,     /* PRGM EXEC sub-menu (subroutine slot picker) */
    MODE_PRGM_RUNNING,       /* Program execution in progress */
    MODE_PRGM_NEW_NAME,      /* Name-entry dialog for new program */
    MODE_GRAPH_PARAM_YEQ,    /* Parametric X/Y pair editor (6 rows: X₁t..Y₃t) */
    MODE_STAT_MENU,          /* STAT menu (CALC/DRAW/DATA tabs) active */
    MODE_STAT_EDIT,          /* STAT DATA list editor active */
    MODE_STAT_RESULTS,       /* STAT results screen active */
    MODE_DRAW_MENU,          /* DRAW menu (single-list, 7 items) active */
    MODE_VARS_MENU,          /* VARS menu (5-tab: XY/Σ/LR/DIM/RNG) active */
    MODE_YVARS_MENU,         /* Y-VARS menu (3-tab: Y/ON/OFF) active */
    MODE_STO,                /* Synthetic: STO pending — cursor shows green 'A'; never set as current_mode */
} CalcMode_t;

/*---------------------------------------------------------------------------
 * Graph state
 *---------------------------------------------------------------------------*/

#define GRAPH_NUM_EQ    4   /* Number of simultaneous Y= equations */
#define GRAPH_NUM_PARAM 3   /* Number of simultaneous parametric X/Y pairs */

/**
 * @brief Holds all state for the graphing subsystem.
 *
 * Initialised to ZStandard defaults (±10 range).
 */
typedef struct {
    char    equations[GRAPH_NUM_EQ][64]; /* Y= equation strings in terms of x */
    bool    enabled[GRAPH_NUM_EQ];       /* True if equation is plotted */
    float   x_min;          /* Left edge of graph window */
    float   x_max;          /* Right edge of graph window */
    float   y_min;          /* Bottom edge of graph window */
    float   y_max;          /* Top edge of graph window */
    float   x_scl;          /* X axis tick spacing */
    float   y_scl;          /* Y axis tick spacing */
    float   x_res;          /* Graph resolution (1 = evaluate at every pixel column) */
    bool    active;         /* True when in graph mode */
    bool    grid_on;        /* True when grid dots are enabled (MODE row 7) */

    /* Parametric equation pairs — X₁t/Y₁t, X₂t/Y₂t, X₃t/Y₃t */
    char    param_x[GRAPH_NUM_PARAM][64];   /* X(t) equation strings */
    char    param_y[GRAPH_NUM_PARAM][64];   /* Y(t) equation strings */
    bool    param_enabled[GRAPH_NUM_PARAM]; /* Pair enable flags */

    /* T range — default 0 to 2π in π/24 steps */
    float   t_min;
    float   t_max;
    float   t_step;

    /* Mode flag — driven by MODE row 4 */
    bool    param_mode;     /* false=function (Y=), true=parametric (X/Y pairs) */
} GraphState_t;

/** Global graph state — owned by calculator_core.c */
extern GraphState_t graph_state;

/*---------------------------------------------------------------------------
 * Statistics state
 *---------------------------------------------------------------------------*/

#define STAT_MAX_POINTS 99  /* Maximum number of (x,y) data points */

/**
 * @brief Holds the user's statistics data list.
 *        Shared between ui_stat.c and calculator_core.c (persist).
 */
typedef struct {
    float   list_x[STAT_MAX_POINTS];
    float   list_y[STAT_MAX_POINTS];
    uint8_t list_len;   /* Number of valid (x,y) pairs; 0 = empty */
} StatData_t;

/** Global stat data — owned by ui_stat.c */
extern StatData_t stat_data;

/**
 * @brief Holds results from the most recent statistical calculation.
 *        Populated by CalcStat_Compute1Var / CalcStat_ComputeLinReg etc.
 */
typedef struct {
    float n, mean_x, sx, sigma_x, sum_x, sum_x2;
    float reg_a, reg_b, reg_r;
    bool  valid;
} StatResults_t;

/** Global stat results — owned by ui_stat.c */
extern StatResults_t stat_results;

/*---------------------------------------------------------------------------
 * Shared handles
 *---------------------------------------------------------------------------*/

#ifndef HOST_TEST
/** Queue for passing keypad tokens from the keypad task to the core task. */
extern osMessageQId      keypadQueueHandle;

/** LVGL mutex — all lv_* calls must be wrapped with lvgl_lock/lvgl_unlock. */
extern SemaphoreHandle_t xLVGL_Mutex;

/** Binary semaphore signalled by DefaultTask once LVGL is initialised. */
extern SemaphoreHandle_t xLVGL_Ready;
#endif /* HOST_TEST */

/*---------------------------------------------------------------------------
 * Function declarations
 *---------------------------------------------------------------------------*/

/** FreeRTOS task entry points */
void StartCalcCoreTask(void const *argument);
void StartKeypadTask(void const *argument);

/** Translates a raw hardware key ID into a token and posts it to the queue */
void Process_Hardware_Key(uint8_t key_id);

/** Processes a single token — updates internal calculator state */
void Execute_Token(Token_t t);

#endif /* APP_COMMON_H */