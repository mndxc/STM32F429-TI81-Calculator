/**
 * @file    graph_ui_range.h
 * @brief   RANGE field editor and ZOOM FACTORS editor screens.
 *
 * Extracted from graph_ui.c; part of the calculator UI super-module.
 * Include calc_internal.h before this header in .c files that need it.
 */

#ifndef APP_GRAPH_UI_RANGE_H
#define APP_GRAPH_UI_RANGE_H

#include "app_common.h"
#include "lvgl.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Screen show/hide/visibility API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Graph_ShowRangeScreen(void);
void Graph_HideRangeScreen(void);
/** Returns true if the RANGE screen is not hidden (NULL-safe). */
bool Graph_IsRangeScreenVisible(void);

void Graph_ShowZoomFactorsScreen(void);
void Graph_HideZoomFactorsScreen(void);
/** Returns true if the ZOOM FACTORS screen is not hidden (NULL-safe). */
bool Graph_IsZoomFactorsScreenVisible(void);

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/

/** Create all LVGL objects for the RANGE and ZOOM FACTORS screens.
 *  Called once from ui_init_graph_screens() during startup. */
void graph_ui_range_init_screens(lv_obj_t *parent);

/*---------------------------------------------------------------------------
 * Nav entry points — called with lvgl_lock() already held
 *---------------------------------------------------------------------------*/

/**
 * @brief Enter the RANGE screen (called from nav_to while lvgl_lock held).
 *
 * Syncs names to param/func mode, resets editor state, loads first field,
 * shows the screen, and positions the cursor.
 */
void range_nav_enter(void);

/**
 * @brief Enter the ZOOM FACTORS screen (called from zoom_enter_factors
 *        while lvgl_lock held).
 *
 * Resets editor state, loads first field, shows the screen, and positions
 * the cursor.
 */
void zoom_factors_nav_enter(void);

/*---------------------------------------------------------------------------
 * Display helpers — called from cursor_timer_cb and calculator_core.c
 *---------------------------------------------------------------------------*/

/** Reposition RANGE cursor box over the active field.
 *  Called by cursor_timer_cb — must NOT call lvgl_lock(). */
void range_cursor_update(void);

/** Redraw all RANGE field labels from graph_state. */
void ui_update_range_display(void);

/** Reposition ZOOM FACTORS cursor box.
 *  Called by cursor_timer_cb — must NOT call lvgl_lock(). */
void zoom_factors_cursor_update(void);

/** Redraw ZOOM FACTORS labels from state. */
void ui_update_zoom_factors_display(void);

/*---------------------------------------------------------------------------
 * Persist accessors — s_zf is private to graph_ui_range.c
 *---------------------------------------------------------------------------*/

float graph_ui_get_zoom_x_fact(void);
float graph_ui_get_zoom_y_fact(void);
void  graph_ui_set_zoom_facts(float x_fact, float y_fact);

/*---------------------------------------------------------------------------
 * Token handlers (called from Execute_Token dispatcher)
 *---------------------------------------------------------------------------*/

bool handle_range_mode(Token_t t);
bool handle_zoom_factors_mode(Token_t t);

#endif /* APP_GRAPH_UI_RANGE_H */
