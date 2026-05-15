/**
 * @file    graph_ui_cursor.c
 * @brief   Trace and free-cursor mode state machines for the graph canvas.
 *
 * Extracted from graph_ui.c. Neither handle_trace_mode() nor
 * handle_free_cursor_mode() shares mutable state with the Y= editor or the
 * other six canvas-mode handlers (ZBox, ZOOM cursor, DRAW cursor).
 *
 * All LVGL calls are made under lvgl_lock()/lvgl_unlock() except
 * free_cursor_blink_cb(), which runs inside lv_timer_handler and must not
 * call lvgl_lock() (the mutex is already held by the LVGL task).
 */

#include "graph_ui_cursor.h"
#include "graph_ui.h"
#include "ui_shared.h"
#include "graph.h"
#include "graph_draw.h"
#include "ui_graph_zoom.h"
#include "calc_engine.h"
#include "ui_shared.h"
#include "lvgl.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Module-private state
 *---------------------------------------------------------------------------*/

static lv_timer_t               *s_free_blink_timer = NULL;
static GraphFreeCursorEnterCb_t  s_prgm_enter_cb    = NULL;
static GraphFreeCursorAbortCb_t  s_prgm_abort_cb    = NULL;

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

/* Runs inside lv_task_handler — must NOT call lvgl_lock(). */
static void free_cursor_blink_cb(lv_timer_t *t)
{
    (void)t;
    const FreeCursorState_t *fc = Graph_GetFreeCursorState();
    bool visible = !fc->cursor_visible;
    Graph_SetFreeCursorVisible(visible);
    if (visible)
        Graph_DrawFreeCursor(fc->x_math, fc->y_math);
    else
        Graph_EraseFreeCursor();
}

/* Called under lvgl_lock(). Stops the blink timer and removes cursor pixels. */
static void free_cursor_stop(void)
{
    if (s_free_blink_timer) {
        lv_timer_delete(s_free_blink_timer);
        s_free_blink_timer = NULL;
    }
    Graph_EraseFreeCursor();
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

void GraphCursor_StartBlink(void)
{
    s_free_blink_timer = lv_timer_create(free_cursor_blink_cb, 500, NULL);
}

void Graph_StartPrgmInput(GraphFreeCursorEnterCb_t on_enter,
                          GraphFreeCursorAbortCb_t on_abort)
{
    s_prgm_enter_cb = on_enter;
    s_prgm_abort_cb = on_abort;
    nav_to(MODE_GRAPH_FREE_CURSOR);
}

/*---------------------------------------------------------------------------
 * handle_trace_mode
 *---------------------------------------------------------------------------*/

bool handle_trace_mode(Token_t t)
{
    const GraphState_t *gs = Graph_GetState();
    float step = gs->param_mode
        ? gs->t_step
        : (gs->x_max - gs->x_min) / (float)(GRAPH_W - 1);
    if (step <= 0.0f) step = 0.1309f;

    switch (t) {
    case TOKEN_LEFT: {
        float x = Graph_GetTraceState()->x;
        bool need_render = false;
        if (gs->param_mode) {
            if (x > gs->t_min) x -= step;
        } else if (x > gs->x_min) {
            x -= step;
        } else {
            /* Guidebook p. 3-10: pan viewport left by half the window width. */
            float pan  = (gs->x_max - gs->x_min) * 0.5f;
            float xmin = gs->x_min - pan, xmax = gs->x_max - pan;
            float ymin = gs->y_min, ymax = gs->y_max;
            float xscl = gs->x_scl, yscl = gs->y_scl, xres = gs->x_res;
            x -= step;
            Graph_SetWindow(xmin, xmax, ymin, ymax, xscl, yscl, xres);
            need_render = true;
        }
        Graph_SetTraceX(x);
        lvgl_lock();
        if (need_render) Graph_Render();
        Graph_DrawTrace(x, Graph_GetTraceState()->eq_idx);
        lvgl_unlock();
        return true;
    }
    case TOKEN_RIGHT: {
        float x = Graph_GetTraceState()->x;
        bool need_render = false;
        if (gs->param_mode) {
            if (x < gs->t_max) x += step;
        } else if (x < gs->x_max) {
            x += step;
        } else {
            /* Guidebook p. 3-10: pan viewport right by half the window width. */
            float pan  = (gs->x_max - gs->x_min) * 0.5f;
            float xmin = gs->x_min + pan, xmax = gs->x_max + pan;
            float ymin = gs->y_min, ymax = gs->y_max;
            float xscl = gs->x_scl, yscl = gs->y_scl, xres = gs->x_res;
            x += step;
            Graph_SetWindow(xmin, xmax, ymin, ymax, xscl, yscl, xres);
            need_render = true;
        }
        Graph_SetTraceX(x);
        lvgl_lock();
        if (need_render) Graph_Render();
        Graph_DrawTrace(x, Graph_GetTraceState()->eq_idx);
        lvgl_unlock();
        return true;
    }
    case TOKEN_UP: {
        uint8_t eq_idx = Graph_GetTraceState()->eq_idx;
        if (gs->param_mode) {
            for (uint8_t i = 1; i <= GRAPH_NUM_PARAM; i++) {
                uint8_t idx = (eq_idx + GRAPH_NUM_PARAM - i) % GRAPH_NUM_PARAM;
                if (gs->param_enabled[idx] &&
                    strlen(gs->param_x[idx]) > 0 &&
                    strlen(gs->param_y[idx]) > 0) {
                    eq_idx = idx;
                    break;
                }
            }
        } else {
            for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
                uint8_t idx = (eq_idx + GRAPH_NUM_EQ - i) % GRAPH_NUM_EQ;
                if (strlen(gs->equations[idx]) > 0 && gs->enabled[idx]) {
                    eq_idx = idx;
                    break;
                }
            }
        }
        Graph_SetTraceEqIdx(eq_idx);
        lvgl_lock();
        Graph_DrawTrace(Graph_GetTraceState()->x, eq_idx);
        lvgl_unlock();
        return true;
    }
    case TOKEN_DOWN: {
        uint8_t eq_idx = Graph_GetTraceState()->eq_idx;
        if (gs->param_mode) {
            for (uint8_t i = 1; i <= GRAPH_NUM_PARAM; i++) {
                uint8_t idx = (eq_idx + i) % GRAPH_NUM_PARAM;
                if (gs->param_enabled[idx] &&
                    strlen(gs->param_x[idx]) > 0 &&
                    strlen(gs->param_y[idx]) > 0) {
                    eq_idx = idx;
                    break;
                }
            }
        } else {
            for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
                uint8_t idx = (eq_idx + i) % GRAPH_NUM_EQ;
                if (strlen(gs->equations[idx]) > 0 && gs->enabled[idx]) {
                    eq_idx = idx;
                    break;
                }
            }
        }
        Graph_SetTraceEqIdx(eq_idx);
        lvgl_lock();
        Graph_DrawTrace(Graph_GetTraceState()->x, eq_idx);
        lvgl_unlock();
        return true;
    }
    case TOKEN_TRACE: {
        /* Re-snap to centre of viewport — TI-81 does not exit trace on TRACE. */
        float x = gs->param_mode
                ? (gs->t_min + gs->t_max) * 0.5f
                : (gs->x_min + gs->x_max) * 0.5f;
        Graph_SetTraceX(x);
        lvgl_lock();
        Graph_DrawTrace(x, Graph_GetTraceState()->eq_idx);
        lvgl_unlock();
        return true;
    }
    case TOKEN_Y_EQUALS:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_GRAPH:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        nav_to(MODE_GRAPH_FREE_CURSOR);
        return true;
    default:
        Calc_SetMode(MODE_NORMAL);
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}

/*---------------------------------------------------------------------------
 * handle_free_cursor_mode
 *---------------------------------------------------------------------------*/

bool handle_free_cursor_mode(Token_t t)
{
    const GraphState_t *gs = Graph_GetState();
    float xstep = (gs->x_max - gs->x_min) / (float)(GRAPH_W - 1);
    float ystep = (gs->y_max - gs->y_min) / (float)(GRAPH_H - 1);

    switch (t) {
    case TOKEN_LEFT: {
        float x = Graph_GetFreeCursorState()->x_math;
        float y = Graph_GetFreeCursorState()->y_math;
        if (x > gs->x_min) x -= xstep;
        Graph_SetFreeCursorPos(x, y);
        Graph_SetFreeCursorVisible(true);
        lvgl_lock();
        if (s_free_blink_timer) lv_timer_reset(s_free_blink_timer);
        Graph_DrawFreeCursor(x, y);
        lvgl_unlock();
        return true;
    }
    case TOKEN_RIGHT: {
        float x = Graph_GetFreeCursorState()->x_math;
        float y = Graph_GetFreeCursorState()->y_math;
        if (x < gs->x_max) x += xstep;
        Graph_SetFreeCursorPos(x, y);
        Graph_SetFreeCursorVisible(true);
        lvgl_lock();
        if (s_free_blink_timer) lv_timer_reset(s_free_blink_timer);
        Graph_DrawFreeCursor(x, y);
        lvgl_unlock();
        return true;
    }
    case TOKEN_UP: {
        float x = Graph_GetFreeCursorState()->x_math;
        float y = Graph_GetFreeCursorState()->y_math;
        if (y < gs->y_max) y += ystep;
        Graph_SetFreeCursorPos(x, y);
        Graph_SetFreeCursorVisible(true);
        lvgl_lock();
        if (s_free_blink_timer) lv_timer_reset(s_free_blink_timer);
        Graph_DrawFreeCursor(x, y);
        lvgl_unlock();
        return true;
    }
    case TOKEN_DOWN: {
        float x = Graph_GetFreeCursorState()->x_math;
        float y = Graph_GetFreeCursorState()->y_math;
        if (y > gs->y_min) y -= ystep;
        Graph_SetFreeCursorPos(x, y);
        Graph_SetFreeCursorVisible(true);
        lvgl_lock();
        if (s_free_blink_timer) lv_timer_reset(s_free_blink_timer);
        Graph_DrawFreeCursor(x, y);
        lvgl_unlock();
        return true;
    }
    case TOKEN_TRACE: {
        /* Snap crosshair to first active equation at the current X position. */
        float snap_x = gs->param_mode
                     ? (gs->t_min + gs->t_max) * 0.5f
                     : Graph_GetFreeCursorState()->x_math;
        lvgl_lock(); free_cursor_stop(); lvgl_unlock();
        Graph_SetTraceX(snap_x);
        Graph_SetTraceEqIdx(0);
        nav_to(MODE_GRAPH_TRACE);
        return true;
    }
    case TOKEN_GRAPH: {
        /* Re-render and re-centre cursor (same as entering the graph screen). */
        float fc_x = (gs->x_min + gs->x_max) * 0.5f;
        float fc_y = (gs->y_min + gs->y_max) * 0.5f;
        lvgl_lock();
        free_cursor_stop();
        Graph_SetFreeCursorPos(fc_x, fc_y);
        Graph_Render();
        Graph_SetFreeCursorVisible(true);
        Graph_DrawFreeCursor(fc_x, fc_y);
        s_free_blink_timer = lv_timer_create(free_cursor_blink_cb, 500, NULL);
        lvgl_unlock();
        return true;
    }
    case TOKEN_Y_EQUALS:
        lvgl_lock(); free_cursor_stop(); lvgl_unlock();
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        lvgl_lock(); free_cursor_stop(); lvgl_unlock();
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        lvgl_lock(); free_cursor_stop(); lvgl_unlock();
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_ENTER:
        if (s_prgm_enter_cb) {
            /* Resume program with current cursor coordinates (guidebook p. 8-13).
             * Rectangular mode stores X/Y; Polar mode (R/θ) is a future extension. */
            float x = Graph_GetFreeCursorState()->x_math;
            float y = Graph_GetFreeCursorState()->y_math;
            lvgl_lock(); free_cursor_stop(); lvgl_unlock();
            GraphFreeCursorEnterCb_t cb = s_prgm_enter_cb;
            s_prgm_enter_cb = NULL;
            s_prgm_abort_cb = NULL;
            cb(x, y);
            return true;
        }
        /* No program waiting — fall through to normal mode exit. */
        /* FALLTHROUGH */
    case TOKEN_CLEAR:
        if (s_prgm_abort_cb) {
            /* Abort program on CLEAR during graph-input (same as CLEAR elsewhere). */
            lvgl_lock(); free_cursor_stop(); hide_all_screens(); lvgl_unlock();
            GraphFreeCursorAbortCb_t cb = s_prgm_abort_cb;
            s_prgm_enter_cb = NULL;
            s_prgm_abort_cb = NULL;
            cb();
            return true;
        }
        /* No program waiting — fall through to normal mode exit. */
        /* FALLTHROUGH */
    default:
        Calc_SetMode(MODE_NORMAL);
        lvgl_lock();
        free_cursor_stop();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}
