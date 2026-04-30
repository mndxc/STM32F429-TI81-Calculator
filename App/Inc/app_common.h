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
 * Fundamental limits
 *---------------------------------------------------------------------------*/

/** Maximum byte length of the expression buffer (supports 4 wrapped display rows). */
#define MAX_EXPR_LEN 96

/*---------------------------------------------------------------------------
 * Shared types
 *---------------------------------------------------------------------------*/

/**
 * @brief Calculator input mode — controls which key layer or UI screen is active.
 *
 * Modes are grouped into six categories documented inline below:
 *   Overlay      — stack over current_mode; revert to it after one key (ALPHA_LOCK: toggle)
 *   Graph editor — exclusive graph-navigation screens (Y=, RANGE, ZOOM, parametric Y=)
 *   Graph cursor — live cursor state on the graph canvas
 *   Modal screen — exclusive UI screen; exits via CLEAR or QUIT
 *   Program      — PRGM menus, editor, and runtime execution
 *   Synthetic    — never stored in current_mode; used only for derived rendering state
 *
 * Dispatch routing in Execute_Token():
 *   Overlay modes (2ND, ALPHA, ALPHA_LOCK) fall through to handle_normal_mode().
 *   MODE_PRGM_RUNNING is special-cased *before* k_mode_handlers[].
 *   MODE_PRGM_EDITOR and MODE_PRGM_NEW_NAME use ALPHA_LOCK compound conditions *after* the table.
 *   All other non-overlay modes map 1-to-1 in k_mode_handlers[].
 */
typedef enum {
    /* --- Base mode --- */
    MODE_NORMAL,

    /* --- Overlay modes: stack over current_mode; transient (reset after one key) --- */
    MODE_2ND,
    MODE_ALPHA,
    MODE_ALPHA_LOCK,    /* persistent — stays active after each keypress until toggled off */

    /* --- Graph editor / navigation modes: exclusive modal screens (exit via CLEAR or QUIT) --- */
    MODE_GRAPH_YEQ,     /* Y= equation editor active */
    MODE_GRAPH_RANGE,   /* RANGE field editor active */
    MODE_GRAPH_ZOOM,    /* ZOOM preset menu active */

    /* --- Graph cursor modes: live on the graph canvas (exit via non-graph key) --- */
    MODE_GRAPH_TRACE,        /* Trace cursor active on graph */
    MODE_GRAPH_FREE_CURSOR,  /* Free-roaming crosshair on graph canvas; TRACE snaps to curve */
    MODE_GRAPH_ZBOX,    /* ZBox rubber-band zoom active */

    /* --- Modal screens: exclusive UI screens (exit via CLEAR or QUIT) --- */
    MODE_MODE_SCREEN,        /* MODE settings screen active */
    MODE_MATH_MENU,          /* MATH/NUM/HYP/PRB menu active */
    MODE_GRAPH_ZOOM_FACTORS, /* ZOOM FACTORS sub-screen active */
    MODE_TEST_MENU,          /* TEST comparison-operator menu active */
    MODE_MATRIX_MENU,        /* MATRIX/EDIT tabs active */
    MODE_MATRIX_EDIT,        /* Matrix cell editor active */

    /* --- Program execution: PRGM menus, editor, and runtime --- */
    MODE_PRGM_MENU,          /* PRGM EXEC/EDIT/NEW tab selection */
    MODE_PRGM_EDITOR,        /* Program line editor (ALPHA_LOCK compound; routed after dispatch table) */
    MODE_PRGM_CTL_MENU,      /* PRGM CTL sub-menu (Lbl, Goto, If…) */
    MODE_PRGM_IO_MENU,       /* PRGM I/O sub-menu (Disp, Input…) */
    MODE_PRGM_EXEC_MENU,     /* PRGM EXEC sub-menu (subroutine slot picker) */
    MODE_PRGM_RUNNING,       /* Program execution in progress (special-cased before dispatch table) */
    MODE_PRGM_NEW_NAME,      /* Name-entry dialog (ALPHA_LOCK compound; routed after dispatch table) */

    /* --- Graph editor / navigation modes (continued — added after program cluster) --- */
    MODE_GRAPH_PARAM_YEQ,    /* Parametric X/Y pair editor (6 rows: X₁t..Y₃t) */

    /* --- Modal screens (continued — added incrementally after initial modal group) --- */
    MODE_STAT_MENU,          /* STAT menu (CALC/DRAW/DATA tabs) active */
    MODE_STAT_EDIT,          /* STAT DATA list editor active */
    MODE_STAT_RESULTS,       /* STAT results screen active */
    MODE_DRAW_MENU,          /* DRAW menu (single-list, 7 items) active */
    MODE_VARS_MENU,          /* VARS menu (5-tab: XY/Σ/LR/DIM/RNG) active */
    MODE_YVARS_MENU,         /* Y-VARS menu (3-tab: Y/ON/OFF) active */

    /* --- Program execution (continued — MODE screen sub-menus added later) --- */
    MODE_PRGM_MODE_NUMBER,   /* PRGM MODE NUMBER tab (Norm/Sci/Eng/Fix/Float/Rad/Deg) */
    MODE_PRGM_MODE_GRAPH,    /* PRGM MODE GRAPH tab (Function/Param/Connected/Dot/Sequence/Simul/Grid Off/Grid On/Rect/Polar) */

    /* --- Modal screens (continued) --- */
    MODE_RESET_CONFIRM,      /* RESET confirmation screen (2nd++): 1:No / 2:Reset */

    /* --- Synthetic: never stored in current_mode; used only for derived rendering --- */
    MODE_STO,                /* STO pending — cursor shows green 'A'; only passed to cursor_render() */
} CalcMode_t;

/*---------------------------------------------------------------------------
 * Graph state
 *---------------------------------------------------------------------------*/

#define GRAPH_NUM_EQ    4   /* Number of simultaneous Y= equations */
#define GRAPH_NUM_PARAM 3   /* Number of simultaneous parametric X/Y pairs */

/*
 * GraphState_t — ownership and mutation rules
 *
 * graph_state is defined (static) in graph.c.  External callers must use
 * Graph_GetState() (read-only pointer) or the write accessors declared in
 * graph.h (Graph_SetWindow, Graph_SetEquationEnabled, etc.).  All mutations
 * must happen under lvgl_lock() because they are followed immediately by
 * LVGL label/display updates in the same critical section.
 *
 * Field ownership by module:
 *
 *   equations[]/enabled[]    — Written via Graph_GetEquationBuf() / Graph_SetEquationEnabled()
 *                              by graph_ui.c (Y= editor), ui_param_yeq.c (parametric editor),
 *                              ui_yvars.c (ON/OFF tab actions).
 *                              Read by graph.c (render), calculator_core.c (save/load).
 *
 *   xmin/xmax/ymin/ymax      — Written via Graph_SetWindow() by graph_ui_range.c (RANGE editor),
 *                              ui_graph_zoom.c (ZOOM preset actions), graph_ui.c (ZBox commit).
 *                              Read by graph.c (render), calculator_core.c (save/load).
 *
 *   tmin/tmax/tstep          — Written via Graph_SetParamWindow() by graph_ui_range.c
 *                              (parametric RANGE editor).  Read by graph.c (parametric render).
 *
 *   param_mode               — Written via Graph_SetParamMode() by ui_mode.c (MODE screen row 4).
 *                              Read by all graph modules to branch behaviour.
 *
 *   plot_sequential          — Written via Graph_SetSequentialMode() by ui_mode.c (MODE screen row 6).
 *                              Read by graph.c.
 *
 *   grid_on                  — Written via Graph_SetGridOn() by ui_mode.c (MODE screen row 7).
 *                              Read by graph.c.
 *
 *   polar_display            — Written via Graph_SetPolarDisplay() by ui_mode.c (MODE screen row 8).
 *                              Read by graph.c (cursor and trace readout format).
 *
 *   active                   — Written via Graph_SetActive() by graph_ui.c (GRAPH key handler).
 *                              Read by calculator_core.c, graph.c.
 *
 * Adding a new graph feature: add its fields here with their owning module,
 * then update docs/TECHNICAL.md "State Ownership" section.
 */

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

    /* Mode flags — driven by MODE rows 4, 5, 6, and 8 */
    bool    param_mode;       /* false=function (Y=), true=parametric (X/Y pairs) */
    bool    plot_connected;   /* true=Connected (lines), false=Dot (pixels only) */
    bool    plot_sequential;  /* true=Sequential, false=Simultaneous — MODE row 6 */
    bool    polar_display;    /* true=Pol (show R/θ at cursor), false=Rect (X/Y) — MODE row 8 */
} GraphState_t;


/*---------------------------------------------------------------------------
 * Statistics state
 *---------------------------------------------------------------------------*/

#define STAT_MAX_POINTS 150  /* Maximum number of (x,y) data points — guidebook p. 7-2 */

/**
 * @brief Holds the user's statistics data list.
 *        Shared between ui_stat.c and calculator_core.c (persist).
 */
typedef struct {
    float   list_x[STAT_MAX_POINTS];
    float   list_y[STAT_MAX_POINTS];
    uint8_t list_len;   /* Number of valid (x,y) pairs; 0 = empty */
} StatData_t;

/**
 * @brief Holds results from the most recent statistical calculation.
 *        Populated by CalcStat_Compute1Var / CalcStat_ComputeLinReg etc.
 */
typedef struct {
    float n, mean_x, sx, sigma_x, sum_x, sum_x2;
    float reg_a, reg_b, reg_r;
    bool  valid;
} StatResults_t;

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