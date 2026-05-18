/**
 * @file    graph.h
 * @brief   Graphing subsystem — Y= equation renderer and STAT plot functions.
 */
#ifndef GRAPH_MODULE_H
#define GRAPH_MODULE_H

#include "app_common.h"   /* GraphState_t, StatData_t */
#ifndef HOST_TEST
#  include "lvgl.h"
#else
   /* Minimal type stub — full stubs are in graph_render_test_stubs.h (included
    * by graph.c); graph.h only needs lv_obj_t for the Graph_Init signature. */
#  ifndef LV_OBJ_T_DEFINED
#    define LV_OBJ_T_DEFINED
     typedef struct lv_obj_s { int dummy; } lv_obj_t;
#  endif
#endif
#include <stdbool.h>
#include <stddef.h>

/*---------------------------------------------------------------------------
 * Constants
 *--------------------------------------------------------------------------*/
#define GRAPH_W         320
#define GRAPH_H         240     /* Full display height */

/** Byte length of each equation string buffer (including NUL terminator). */
#define GRAPH_EQUATION_BUF_LEN  64

/*---------------------------------------------------------------------------
 * Graph state accessor API
 *
 * graph_state is owned by graph.c. All external reads go through
 * Graph_GetState(); all writes go through the named setters below.
 * Callers must already hold lvgl_lock() when calling setters that are
 * immediately followed by LVGL display updates in the same critical section.
 *--------------------------------------------------------------------------*/

/** Read-only view of the graph state. */
const GraphState_t *Graph_GetState(void);

/** Write accessors — thin wrappers; no validation, no lock acquisition. */
void Graph_SetEquationEnabled(uint8_t idx, bool enabled);
void Graph_SetWindow(float xmin, float xmax, float ymin, float ymax,
                     float xscl, float yscl, float xres);
void Graph_SetParamEnabled(uint8_t idx, bool enabled);
void Graph_SetParamWindow(float tmin, float tmax, float tstep);
void Graph_SetParamMode(bool param);
void Graph_SetConnectedMode(bool connected);
void Graph_SetSequentialMode(bool sequential);
void Graph_SetGridOn(bool on);
void Graph_SetPolarDisplay(bool polar);
void Graph_SetActive(bool active);

/**
 * Mutable buffer accessors — for in-place string editing in the YEQ and
 * parametric editors.  Callers must not write past GRAPH_EQUATION_BUF_LEN-1
 * bytes and must NUL-terminate.
 */
char *Graph_GetEquationBuf(uint8_t idx);
char *Graph_GetParamEquationXBuf(uint8_t pair);
char *Graph_GetParamEquationYBuf(uint8_t pair);

/*---------------------------------------------------------------------------
 * Function declarations
 *--------------------------------------------------------------------------*/

/**
 * @brief Creates the graph screen LVGL objects.
 *        Call once during UI initialisation.
 */
void Graph_Init(lv_obj_t *parent);

/**
 * @brief Renders the current Y= equation onto the canvas.
 *        Uses graph_state for equation, range and scale.
 */
void Graph_Render(void);

/**
 * @brief Shows or hides the graph screen.
 */
void Graph_SetVisible(bool visible);

/**
 * @brief Re-renders the graph and draws a trace cursor at math coordinate x.
 *        Updates the X/Y readout label. eq_idx selects which Y= equation to
 *        evaluate for the Y value (0–GRAPH_NUM_EQ-1).
 */
void Graph_DrawTrace(float x, uint8_t eq_idx);

/**
 * @brief Clears the X/Y readout label left by Graph_DrawTrace.
 */
void Graph_ClearTrace(void);

/**
 * @brief Restores the clean frame and draws a free-roaming crosshair at
 *        math coordinate (x, y). Updates the X=/Y= readout labels.
 *        White crosshair — distinct from the green trace cursor.
 */
void Graph_DrawFreeCursor(float x, float y);

/**
 * @brief Restores the clean frame without crosshair pixels. Coordinate labels
 *        are left unchanged. Called by the blink timer to hide the cursor
 *        between flashes. Must be called under lvgl_lock().
 */
void Graph_EraseFreeCursor(void);

/**
 * @brief Renders parametric X(t)/Y(t) pairs onto the canvas.
 *        Dispatched from Graph_Render when graph_state.param_mode is true.
 */
void Graph_RenderParametric(void);

/**
 * @brief Invalidates all per-equation postfix caches so the next render
 *        re-parses equations from graph_state.  Call when param_mode changes.
 */
void Graph_InvalidateCache(void);

/**
 * @brief Draws the ZBox rubber-band overlay on the graph canvas.
 *        Restores the clean frame then overlays:
 *          - A yellow crosshair at the current cursor (px, py).
 *          - A white rectangle from (px1, py1) to (px, py) once the first
 *            corner has been set (corner1_set = true).
 *        Updates the X/Y readout label with the math coordinates of (px, py).
 */
void Graph_DrawZBox(int32_t px, int32_t py,
                    int32_t px1, int32_t py1,
                    bool corner1_set);

/**
 * @brief Restore the clean frame, draw an optional preview line from (px1,py1)
 *        to (px,py), then draw the cursor crosshair at (px,py).
 *        Used by handle_draw_cursor_mode for Line( second-pick preview.
 */
void Graph_DrawLineCursor(int32_t px, int32_t py,
                          bool has_preview, int32_t px1, int32_t py1);

/**
 * @brief Draws a scatter plot of the stat data list onto the graph canvas.
 *        Each point is rendered as a 3×3 cross.
 *        Calls Graph_SetVisible(true) to display the canvas.
 */
void Graph_DrawScatter(const StatData_t *d);

/**
 * @brief Draws a scatter plot with consecutive points connected by lines.
 *        Calls Graph_SetVisible(true) to display the canvas.
 */
void Graph_DrawXYLine(const StatData_t *d);

/**
 * @brief Draws a histogram of the X values (10 equal-width bins, Y = count).
 *        Calls Graph_SetVisible(true) to display the canvas.
 */
void Graph_DrawHistogram(const StatData_t *d);

/** True if the graph screen is currently visible. */
bool Graph_IsVisible(void);

/** Converts math-world X to canvas pixel column (clamped to [0, GRAPH_W-1]). */
int32_t Graph_MathXToPx(float x);

/** Converts math-world Y to canvas pixel row (clamped to [0, GRAPH_H-1]). */
int32_t Graph_MathYToPx(float y);

/**
 * @brief Unified graph-mode key dispatcher.
 *
 * Single entry point called by Execute_Token for all graph sub-modes.
 * Reads the current CalcMode_t and delegates to the appropriate per-mode
 * handler (handle_yeq_mode, handle_trace_mode, etc.).  Owns the full
 * round-trip: mode check → handler → optional re-render.
 *
 * Not compiled in HOST_TEST builds (graph_ui.h / lvgl.h unavailable there).
 */
bool Graph_HandleKey(Token_t t);

/*---------------------------------------------------------------------------
 * Cursor state types and accessors
 *
 * These three structs are separate from GraphState_t (which owns equations,
 * window, and mode flags).  Cursor state is transient — it is never persisted
 * to FLASH and is reset whenever graph rendering restarts.
 *
 * Owned by graph.c; graph_ui.c reads via const pointers and writes via the
 * named setters below.  All setters are thin wrappers with no lock acquisition;
 * callers must hold lvgl_lock() when a setter is followed by a draw call in
 * the same critical section.
 *--------------------------------------------------------------------------*/

typedef struct {
    float   x;       /* Current trace x (or t in param mode) position */
    uint8_t eq_idx;  /* Which equation / parametric pair the crosshair is on */
} TraceState_t;

typedef struct {
    float x_math;
    float y_math;
    bool  cursor_visible;
} FreeCursorState_t;

typedef struct {
    int32_t  px,  py;   /**< Current crosshair cursor position, in pixels. */
    int32_t  px1, py1;  /**< First anchored corner, in pixels; valid only when corner1_set
                          *   is true. No ordering guarantee vs. (px,py) — normalize before use. */
    bool     corner1_set; /**< True after the user has pressed ENTER to anchor the first corner. */
} ZBoxState_t;

const TraceState_t      *Graph_GetTraceState(void);
void Graph_SetTraceX(float x);
void Graph_SetTraceEqIdx(uint8_t idx);

const FreeCursorState_t *Graph_GetFreeCursorState(void);
void Graph_SetFreeCursorPos(float x, float y);
void Graph_SetFreeCursorVisible(bool v);

const ZBoxState_t       *Graph_GetZBoxState(void);
void Graph_SetZBoxCursorPos(int32_t px, int32_t py);
void Graph_SetZBoxCorner1(int32_t px1, int32_t py1);
void Graph_ClearZBoxCorner1(void);
void Graph_ResetZBox(void);

#endif /* GRAPH_MODULE_H */