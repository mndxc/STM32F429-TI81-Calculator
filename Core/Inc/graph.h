/**
 * @file    graph.h
 * @brief   Graphing subsystem — Y= equation renderer.
 */
#ifndef GRAPH_H
#define GRAPH_H

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
void Graph_Render(void);

/**
 * @brief Shows or hides the graph screen.
 */
void Graph_SetVisible(bool visible);

#endif /* GRAPH_H */