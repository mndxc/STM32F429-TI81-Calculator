/**
 * @file    graph.h
 * @brief   Graphing subsystem — Y= equation renderer.
 */
#ifndef GRAPH_MODULE_H
#define GRAPH_MODULE_H

#include "lvgl.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Constants
 *--------------------------------------------------------------------------*/
#define GRAPH_W         320
#define GRAPH_H         220     /* Full display minus status bar */

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

#endif /* GRAPH_MODULE_H */