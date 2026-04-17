/**
 * @file    graph.h
 * @brief   Graphing subsystem — Y= equation renderer and STAT plot functions.
 */
#ifndef GRAPH_MODULE_H
#define GRAPH_MODULE_H

#include "app_common.h"   /* GraphState_t, StatData_t */
#include "lvgl.h"
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
void Graph_SetGridOn(bool on);
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
void Graph_Render(bool angle_degrees);

/**
 * @brief Shows or hides the graph screen.
 */
void Graph_SetVisible(bool visible);

/**
 * @brief Re-renders the graph and draws a trace cursor at math coordinate x.
 *        Updates the X/Y readout label. eq_idx selects which Y= equation to
 *        evaluate for the Y value (0–GRAPH_NUM_EQ-1).
 */
void Graph_DrawTrace(float x, uint8_t eq_idx, bool angle_degrees);

/**
 * @brief Clears the X/Y readout label left by Graph_DrawTrace.
 */
void Graph_ClearTrace(void);

/**
 * @brief Renders parametric X(t)/Y(t) pairs onto the canvas.
 *        Dispatched from Graph_Render when graph_state.param_mode is true.
 */
void Graph_RenderParametric(bool angle_degrees);

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
                    bool corner1_set, bool angle_degrees);

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

#endif /* GRAPH_MODULE_H */