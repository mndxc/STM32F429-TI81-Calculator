/**
 * @file    graph_coord.h
 * @brief   Shared coordinate transform helpers for graph.c and graph_draw.c.
 *
 * All four transforms are static inline — zero runtime cost.  Formulas match
 * the former private statics in graph.c, which were the authoritative reference.
 */
#ifndef GRAPH_COORD_H
#define GRAPH_COORD_H
#include "graph.h"  /* GraphState_t, GRAPH_W, GRAPH_H */
#include <math.h>   /* fabsf */
#include <stdint.h>

static inline int32_t graph_coord_math_x_to_px(const GraphState_t *s, float x)
{
    float range = s->x_max - s->x_min;
    if (fabsf(range) < 1e-9f) return 0;
    return (int32_t)((x - s->x_min) / range * (GRAPH_W - 1));
}

static inline int32_t graph_coord_math_y_to_px(const GraphState_t *s, float y)
{
    float range = s->y_max - s->y_min;
    if (fabsf(range) < 1e-9f) return 0;
    return (int32_t)((s->y_max - y) / range * (GRAPH_H - 1));
}

static inline float graph_coord_px_to_math_x(const GraphState_t *s, int32_t px)
{
    return s->x_min +
           (float)px / (float)(GRAPH_W - 1) *
           (s->x_max - s->x_min);
}

static inline float graph_coord_px_to_math_y(const GraphState_t *s, int32_t py)
{
    return s->y_max -
           (float)py / (float)(GRAPH_H - 1) *
           (s->y_max - s->y_min);
}

#endif /* GRAPH_COORD_H */
