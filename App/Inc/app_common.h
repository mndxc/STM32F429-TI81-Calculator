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
    MODE_GRAPH_ZBOX,         /* ZBox rubber-band zoom active */
    MODE_GRAPH_ZOOM_CURSOR,  /* Single-point cursor-pick for Zoom In/Out/Integer */
    MODE_GRAPH_DRAW_CURSOR,  /* Interactive cursor-pick for DRAW Line(/PT-On(/PT-Off(/PT-Chg( */

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

    MODE_COUNT,              /* sentinel — keep last; used by calc_mode_topology_validate() */
} CalcMode_t;

/*---------------------------------------------------------------------------
 * Graph state
 *---------------------------------------------------------------------------*/

#define GRAPH_NUM_EQ    4   /* Number of simultaneous Y= equations */
#define GRAPH_NUM_PARAM 3   /* Number of simultaneous parametric X/Y pairs */

/*
 * GraphState_t — ownership and mutation rules (updated post-T2-C)
 *
 * graph_state is defined (static) in graph.c.  External callers must use
 * Graph_GetState() (read-only pointer) or the write accessors declared in
 * graph.h (Graph_SetWindow, Graph_SetEquationEnabled, etc.).  All mutations
 * must happen under lvgl_lock() because they are followed immediately by
 * LVGL label/display updates in the same critical section.
 *
 * Cursor overlay state (s_trace, s_free, s_zbox) is NOT part of GraphState_t.
 * It lives as separate statics in graph.c and is exposed via
 * Graph_GetTraceState() / Graph_GetFreeCursorState() / Graph_GetZBoxState().
 * graph_ui.c reads cursor state only through those accessors.
 *
 * Field ownership by module:
 *
 *   equations[]/enabled[]      — Written via Graph_GetEquationBuf() / Graph_SetEquationEnabled()
 *                                by graph_ui.c (Y= editor), ui_param_yeq.c (parametric editor),
 *                                ui_yvars.c (ON/OFF tab actions).
 *                                Read by graph.c (render), graph_ui.c (display labels),
 *                                calculator_core.c (persist).
 *                                Valid in all modes; only rendered when param_mode=false.
 *
 *   x_min/x_max/y_min/y_max    — Written via Graph_SetWindow() by graph_ui_range.c (RANGE editor),
 *   x_scl/y_scl/x_res            ui_graph_zoom.c (ZOOM preset and factor actions),
 *                                graph_ui.c (ZBox ENTER commit).
 *                                Read by graph.c (render), graph_draw.c (coordinate helpers),
 *                                graph_ui.c (trace/cursor math), calculator_core.c (persist).
 *                                Valid in all modes.
 *
 *   param_x[]/param_y[]        — Written by ui_param_yeq.c directly into the buffers returned
 *   param_enabled[]              by Graph_GetParamEquationXBuf() / Graph_GetParamEquationYBuf().
 *                                param_enabled[] written via Graph_SetParamEnabled() by ui_param_yeq.c.
 *                                Read by graph.c (parametric render), graph_ui.c (trace/cursor skip).
 *                                Valid in all modes; only rendered when param_mode=true.
 *
 *   t_min/t_max/t_step         — Written via Graph_SetParamWindow() by graph_ui_range.c
 *                                (parametric RANGE editor).
 *                                Read by graph.c (parametric render), graph_ui.c (trace step).
 *                                Only meaningful when param_mode=true.
 *
 *   param_mode                 — Written via Graph_SetParamMode() by ui_mode.c (MODE screen row 4).
 *                                Read by all graph modules to branch behaviour.
 *
 *   plot_connected             — Written via Graph_SetConnectedMode() by ui_mode.c (MODE screen row 5).
 *                                Read by graph.c (connect adjacent valid pixels with a line segment).
 *
 *   plot_sequential            — Written via Graph_SetSequentialMode() by ui_mode.c (MODE screen row 6).
 *                                Read by graph.c.
 *
 *   grid_on                    — Written via Graph_SetGridOn() by ui_mode.c (MODE screen row 7).
 *                                Read by graph.c.
 *
 *   polar_display              — Written via Graph_SetPolarDisplay() by ui_mode.c (MODE screen row 8).
 *                                Read by graph.c (cursor and trace coordinate readout format).
 *
 *   active                     — Written via Graph_SetActive() by graph_ui.c (GRAPH key handler).
 *                                Read by calculator_core.c, graph.c.
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
    char    equations[GRAPH_NUM_EQ][64]; /* Y= strings; written by graph_ui.c/ui_yvars.c via Graph_GetEquationBuf() */
    bool    enabled[GRAPH_NUM_EQ];       /* Plot enable flags; written via Graph_SetEquationEnabled() */
    float   x_min;          /* Left edge of window; written via Graph_SetWindow() */
    float   x_max;          /* Right edge of window; written via Graph_SetWindow() */
    float   y_min;          /* Bottom edge of window; written via Graph_SetWindow() */
    float   y_max;          /* Top edge of window; written via Graph_SetWindow() */
    float   x_scl;          /* X axis tick spacing; written via Graph_SetWindow() */
    float   y_scl;          /* Y axis tick spacing; written via Graph_SetWindow() */
    float   x_res;          /* Render step (1–8 pixel columns per sample); written via Graph_SetWindow() */
    bool    active;         /* True when graph display is visible; written via Graph_SetActive() by graph_ui.c */
    bool    grid_on;        /* Grid dots enabled; written via Graph_SetGridOn() (MODE row 7) */

    /* Parametric equation pairs — X₁t/Y₁t, X₂t/Y₂t, X₃t/Y₃t; only rendered when param_mode=true */
    char    param_x[GRAPH_NUM_PARAM][64];   /* X(t) strings; written by ui_param_yeq.c via Graph_GetParamEquationXBuf() */
    char    param_y[GRAPH_NUM_PARAM][64];   /* Y(t) strings; written by ui_param_yeq.c via Graph_GetParamEquationYBuf() */
    bool    param_enabled[GRAPH_NUM_PARAM]; /* Pair enable flags; written via Graph_SetParamEnabled() by ui_param_yeq.c */

    /* T range — default 0 to 2π in π/24 steps; only meaningful when param_mode=true */
    float   t_min;   /* Written via Graph_SetParamWindow() by graph_ui_range.c */
    float   t_max;   /* Written via Graph_SetParamWindow() by graph_ui_range.c */
    float   t_step;  /* Written via Graph_SetParamWindow() by graph_ui_range.c */

    /* Mode flags — driven by MODE screen rows 4–8; written by ui_mode.c via Graph_Set*() */
    bool    param_mode;       /* false=function (Y=), true=parametric (X/Y pairs); Graph_SetParamMode() */
    bool    plot_connected;   /* true=Connected (line segments), false=Dot (pixels only); Graph_SetConnectedMode() (MODE row 5) */
    bool    plot_sequential;  /* true=Sequential, false=Simultaneous; Graph_SetSequentialMode() (MODE row 6) */
    bool    polar_display;    /* true=Pol (R/θ readout), false=Rect (X/Y); Graph_SetPolarDisplay() (MODE row 8) */
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