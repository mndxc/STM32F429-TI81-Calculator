/**
 * @file    graph_draw.c
 * @brief   Draw layer — persistent user-drawn overlay (DRAW menu operations).
 *
 * Owns draw_buf (SDRAM at 0xD0080800) and all Graph_DrawLayer* / Graph_DrawF /
 * Graph_Shade operations.  graph.c calls Graph_ApplyDrawLayer(graph_buf) at the
 * end of every render pass so drawn content persists across equation re-renders.
 *
 * Coordinate helpers here mirror the private statics in graph.c; both read only
 * from the graph state via Graph_GetState().
 */
#include "graph_draw.h"
#include "graph.h"          /* GRAPH_W, GRAPH_H, Graph_GetState() */
#include "calc_engine.h"    /* Calc_EvaluateAt, CalcResult_t, CALC_OK */
#include <math.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Private state
 *--------------------------------------------------------------------------*/

/* DRAW menu overlay buffer — SDRAM immediately after the LVGL heap gap.
 * 0x0000 = transparent sentinel; any other value = drawn pixel colour.
 * Memory layout: graph_buf=0xD0025800 (150 KB), graph_buf_clean=0xD004B000
 * (150 KB), LVGL heap=0xD0070800 (64 KB), draw_buf=0xD0080800 (150 KB). */
static uint16_t * const draw_buf = (uint16_t *)0xD0080800;

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

void Graph_DrawF(const char *expr, uint16_t color, bool angle_degrees)
{
    for (int32_t px = 0; px < GRAPH_W; px++) {
        float x = draw_px_to_math_x(px);
        CalcResult_t r = Calc_EvaluateAt(expr, x, 0.0f, angle_degrees);
        if (r.error != CALC_OK || isnan(r.value) || isinf(r.value)) continue;
        int32_t py = draw_math_y_to_px(r.value);
        Graph_DrawLayerSetPixel(px, py, color);
    }
}

void Graph_Shade(float y_low, float y_high, uint16_t fill_color)
{
    if (y_low > y_high) { float tmp = y_low; y_low = y_high; y_high = tmp; }
    for (int32_t px = 0; px < GRAPH_W; px++) {
        int32_t py_top = draw_math_y_to_px(y_high);
        int32_t py_bot = draw_math_y_to_px(y_low);
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
