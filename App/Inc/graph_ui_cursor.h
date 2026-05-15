/**
 * @file    graph_ui_cursor.h
 * @brief   Trace and free-cursor mode handlers for the graph canvas.
 *
 * Extracted from graph_ui.c. These two state machines share no mutable state
 * with the Y= editor or the other canvas-mode handlers (ZBox, ZOOM cursor,
 * DRAW cursor) that remain in graph_ui.c.
 *
 * Callers that already include graph_ui.h receive these declarations
 * automatically via the #include "graph_ui_cursor.h" in that header.
 */

#ifndef GRAPH_UI_CURSOR_H
#define GRAPH_UI_CURSOR_H

#include "app_common.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * PRGM graph-exploration callback types
 * (used by Graph_StartPrgmInput declared below; re-exported through graph_ui.h)
 *---------------------------------------------------------------------------*/
typedef void (*GraphFreeCursorEnterCb_t)(float x, float y);
typedef void (*GraphFreeCursorAbortCb_t)(void);

/**
 * @brief Called by prgm_exec.c when a program executes Input with no argument.
 *        Registers on_enter / on_abort callbacks and enters MODE_GRAPH_FREE_CURSOR.
 *
 * on_enter(x, y) — ENTER pressed: store coordinates and resume the program.
 * on_abort()     — CLEAR pressed: abort the program and return to normal mode.
 */
void Graph_StartPrgmInput(GraphFreeCursorEnterCb_t on_enter,
                          GraphFreeCursorAbortCb_t on_abort);

/**
 * @brief Start the free-cursor blink timer.
 *
 * Must be called under lvgl_lock().  Invoked from nav_to() when entering
 * MODE_GRAPH_FREE_CURSOR so that the timer state stays inside this module.
 */
void GraphCursor_StartBlink(void);

/*---------------------------------------------------------------------------
 * Token handlers (called from Execute_Token dispatcher via graph_ui.h)
 *---------------------------------------------------------------------------*/
bool handle_trace_mode(Token_t t);
bool handle_free_cursor_mode(Token_t t);

#endif /* GRAPH_UI_CURSOR_H */
