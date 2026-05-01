/**
 * @file    graph_draw.c
 * @brief   Draw layer — persistent user-drawn overlay (DRAW menu operations).
 *
 * Owns draw_buf (SDRAM at 0xD0090800) and all Graph_DrawLayer* / Graph_DrawF /
 * Graph_Shade operations.  graph.c calls Graph_ApplyDrawLayer(graph_buf) at the
 * end of every render pass so drawn content persists across equation re-renders.
 *
 * Coordinate helpers here mirror the private statics in graph.c; both read only
 * from the graph state via Graph_GetState().
 */
#include "graph_draw.h"
#include "graph.h"          /* GRAPH_W, GRAPH_H, Graph_GetState() */
#include "calc_engine.h"    /* Calc_EvaluateAt, CalcResult_t, CALC_OK */
#include "calculator_core.h"
#include <math.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Private state
 *--------------------------------------------------------------------------*/

/* DRAW menu overlay buffer.
 * Embedded: SDRAM immediately after the LVGL heap (see memory layout comment).
 * HOST_TEST: static array in BSS so graph_draw.c links for host tests.
 * Memory layout: graph_buf=0xD0025800 (150 KB), graph_buf_clean=0xD004B000
 * (150 KB), LVGL heap=0xD0070800 (128 KB), draw_buf=0xD0090800 (150 KB). */
#ifndef HOST_TEST
static uint16_t * const draw_buf = (uint16_t *)0xD0090800;
#else
static uint16_t draw_buf_storage[GRAPH_H * GRAPH_W];
static uint16_t * const draw_buf = draw_buf_storage;
#endif

/*---------------------------------------------------------------------------
 * Private coordinate helpers
 * These mirror math_x_to_px / math_y_to_px / px_to_math_x in graph.c;
 * they read only from the global graph_state, so no coupling beyond that.
 *--------------------------------------------------------------------------*/


static float draw_px_to_math_x(int32_t px)
{
    const GraphState_t *gs = Graph_GetState();
    return gs->x_min +
           (float)px / (float)(GRAPH_W - 1) *
           (gs->x_max - gs->x_min);
}

static int32_t draw_math_y_to_px(float y)
{
    const GraphState_t *gs = Graph_GetState();
    float range = gs->y_max - gs->y_min;
    if (fabsf(range) < 1e-9f) return 0;
    return (int32_t)((gs->y_max - y) / range * (GRAPH_H - 1));
}

/*---------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

void Graph_DrawLayerClear(void)
{
    memset(draw_buf, 0, (size_t)GRAPH_W * GRAPH_H * sizeof(uint16_t));
}

void Graph_DrawLayerSetPixel(int32_t px, int32_t py, uint16_t color)
{
    if (px < 0 || px >= GRAPH_W || py < 0 || py >= GRAPH_H) return;
    draw_buf[py * GRAPH_W + px] = color;
}

uint16_t Graph_DrawLayerGetPixel(int32_t px, int32_t py)
{
    if (px < 0 || px >= GRAPH_W || py < 0 || py >= GRAPH_H) return 0;
    return draw_buf[py * GRAPH_W + px];
}

void Graph_DrawLayerLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         uint16_t color)
{
    int32_t dx = x1 - x0, dy = y1 - y0;
    int32_t ax = dx < 0 ? -dx : dx;
    int32_t ay = dy < 0 ? -dy : dy;
    int32_t sx = dx >= 0 ? 1 : -1;
    int32_t sy = dy >= 0 ? 1 : -1;
    int32_t err = ax - ay;
    int32_t cx = x0, cy = y0;
    while (1) {
        Graph_DrawLayerSetPixel(cx, cy, color);
        if (cx == x1 && cy == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -ay) { err -= ay; cx += sx; }
        if (e2 <  ax) { err += ax; cy += sy; }
    }
}

void Graph_DrawF(const char *expr, uint16_t color)
{
    bool angle_degrees = Calc_GetAngleDegrees();
    for (int32_t px = 0; px < GRAPH_W; px++) {
        float x = draw_px_to_math_x(px);
        CalcResult_t r = Calc_EvaluateAt(expr, x, 0.0f, angle_degrees);
        if (r.error != CALC_OK || isnan(r.value) || isinf(r.value)) continue;
        int32_t py = draw_math_y_to_px(r.value);
        Graph_DrawLayerSetPixel(px, py, color);
    }
}

void Graph_Shade(const char *lower_expr, const char *upper_expr,
                 int resolution, float x_beg, float x_end,
                 uint16_t fill_color)
{
    bool angle_degrees = Calc_GetAngleDegrees();
    GraphEquation_t lower_eq, upper_eq;
    if (Calc_PrepareGraphEquation(lower_expr, 0.0f, &lower_eq) != CALC_OK) return;
    if (Calc_PrepareGraphEquation(upper_expr, 0.0f, &upper_eq) != CALC_OK) return;

    const GraphState_t *gs = Graph_GetState();
    float x_range = gs->x_max - gs->x_min;
    if (fabsf(x_range) < 1e-9f) return;

    /* Convert math-world boundaries to pixel columns */
    if (x_beg > x_end) { float tmp = x_beg; x_beg = x_end; x_end = tmp; }
    int32_t px_beg = (int32_t)((x_beg - gs->x_min) / x_range * (GRAPH_W - 1));
    int32_t px_end = (int32_t)((x_end - gs->x_min) / x_range * (GRAPH_W - 1));
    if (px_beg < 0) px_beg = 0;
    if (px_end >= GRAPH_W) px_end = GRAPH_W - 1;

    for (int32_t px = px_beg; px <= px_end; px++) {
        float x = draw_px_to_math_x(px);
        CalcResult_t r_lo = Calc_EvalGraphEquation(&lower_eq, x, angle_degrees);
        CalcResult_t r_hi = Calc_EvalGraphEquation(&upper_eq, x, angle_degrees);

        bool lo_valid = (r_lo.error == CALC_OK && !isnan(r_lo.value) && !isinf(r_lo.value));
        bool hi_valid = (r_hi.error == CALC_OK && !isnan(r_hi.value) && !isinf(r_hi.value));

        /* Draw the two boundary curves at every column */
        if (lo_valid) Graph_DrawLayerSetPixel(px, draw_math_y_to_px(r_lo.value), fill_color);
        if (hi_valid) Graph_DrawLayerSetPixel(px, draw_math_y_to_px(r_hi.value), fill_color);

        /* Fill only where lower < upper; respect resolution column spacing */
        if (!lo_valid || !hi_valid) continue;
        if (r_lo.value >= r_hi.value) continue;
        if (resolution > 1 && (px % resolution) != 0) continue;

        int32_t py_top = draw_math_y_to_px(r_hi.value); /* higher Y → lower row */
        int32_t py_bot = draw_math_y_to_px(r_lo.value);
        if (py_top > py_bot) { int32_t tmp = py_top; py_top = py_bot; py_bot = tmp; }
        if (py_top < 0) py_top = 0;
        if (py_bot >= GRAPH_H) py_bot = GRAPH_H - 1;
        for (int32_t py = py_top; py <= py_bot; py++)
            Graph_DrawLayerSetPixel(px, py, fill_color);
    }
}

void Graph_ApplyDrawLayer(uint16_t *dest)
{
    for (int32_t i = 0; i < GRAPH_W * GRAPH_H; i++) {
        if (draw_buf[i] != 0)
            dest[i] = draw_buf[i];
    }
}
