/**
 * @file    ui_graph_zoom.h
 * @brief   ZOOM menu screen: state, display, and token handler.
 *
 * Extracted from graph_ui.c (INTERFACE_REFACTOR_PLAN Item 4); part of the
 * calculator UI super-module.
 * Include calc_internal.h before this header in .c files that need it.
 */

#ifndef UI_GRAPH_ZOOM_H
#define UI_GRAPH_ZOOM_H

#include "app_common.h"
#include "lvgl.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Screen pointer — extern so hide_all_screens() and calc_internal.h
 * can reach it.
 *---------------------------------------------------------------------------*/

extern lv_obj_t *ui_graph_zoom_screen;

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/

/** Create all LVGL objects for the ZOOM menu screen.
 *  Called once from ui_init_graph_screens() during startup. */
void ui_init_zoom_screen(lv_obj_t *parent);

/*---------------------------------------------------------------------------
 * Display helpers
 *---------------------------------------------------------------------------*/

/** Redraw ZOOM item rows. */
void ui_update_zoom_display(void);

/*---------------------------------------------------------------------------
 * State helpers
 *---------------------------------------------------------------------------*/

/** Reset ZOOM menu cursor/scroll to zero. Called from graph_ui.c nav helpers. */
void zoom_menu_reset(void);

/*---------------------------------------------------------------------------
 * Token handler (called from Execute_Token dispatcher)
 *---------------------------------------------------------------------------*/

bool handle_zoom_mode(Token_t t);

#endif /* UI_GRAPH_ZOOM_H */
