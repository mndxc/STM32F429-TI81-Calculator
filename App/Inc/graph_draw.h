/**
 * @file    graph_draw.h
 * @brief   Draw layer — persistent user-drawn overlay (DRAW menu operations).
 *
 * Owns the draw_buf SDRAM buffer at 0xD0080800.  The draw layer is blended
 * over the equation render at the end of every Graph_Render /
 * Graph_RenderParametric pass via Graph_ApplyDrawLayer().
 */
#ifndef GRAPH_DRAW_H
#define GRAPH_DRAW_H

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Draw layer — DRAW menu persistent overlay
 *
 * Buffer is 320×240 RGB565 (0x0000 = transparent sentinel).
 * ClrDraw zeros the buffer; pressing GRAPH re-applies the layer
 * automatically because Graph_Render calls Graph_ApplyDrawLayer().
 *---------------------------------------------------------------------------*/

/** Clears all user-drawn content from the draw layer. */
void Graph_DrawLayerClear(void);

/** Sets one draw-layer pixel at canvas coordinates (px, py). */
void Graph_DrawLayerSetPixel(int32_t px, int32_t py, uint16_t color);

/** Returns the current draw-layer color at (px, py); 0x0000 = transparent. */
uint16_t Graph_DrawLayerGetPixel(int32_t px, int32_t py);

/** Draws a Bresenham line between canvas pixel coordinates on the draw layer. */
void Graph_DrawLayerLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         uint16_t color);

/**
 * @brief Evaluates expr at every pixel column and draws the resulting curve
 *        onto the draw layer.
 */
void Graph_DrawF(const char *expr, uint16_t color, bool angle_degrees);

/**
 * @brief Fills the draw layer between y_low and y_high at every pixel column.
 *        y_low and y_high are math-world Y coordinates.
 */
void Graph_Shade(float y_low, float y_high, uint16_t fill_color);

/**
 * @brief Blends the draw layer over dest (graph_buf passed from graph.c).
 *        Called at the end of every render pass; not part of the public DRAW API.
 */
void Graph_ApplyDrawLayer(uint16_t *dest);

#endif /* GRAPH_DRAW_H */
