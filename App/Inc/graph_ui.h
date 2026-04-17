/**
 * @file    graph_ui.h
 * @brief   Graph screen UI handlers and helpers (Y=, TRACE, ZBox).
 *
 * Extracted from calculator_core.c following the same pattern as ui_matrix.h
 * and ui_prgm.h. Zero behavioral changes — purely a file organisation refactor.
 *
 * RANGE and ZOOM FACTORS screens are in graph_ui_range.h / graph_ui_range.c.
 * ZOOM menu is in ui_graph_zoom.h / ui_graph_zoom.c.
 * Including this header also pulls in graph_ui_range.h and ui_param_yeq.h
 * via the includes below.
 */

#ifndef APP_GRAPH_UI_H
#define APP_GRAPH_UI_H

#include "app_common.h"
#include "ui_param_yeq.h"
#include "graph_ui_range.h"
#include "lvgl.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Screen show/hide/visibility API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Graph_ShowYeqScreen(void);
void Graph_HideYeqScreen(void);
/** Returns true if the Y= screen is not hidden (NULL-safe). */
bool Graph_IsYeqScreenVisible(void);

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/
/** Create all graph-screen LVGL objects. Called once from StartCalcCoreTask. */
void ui_init_graph_screens(void);

/*---------------------------------------------------------------------------
 * Helpers called from calculator_core.c
 *---------------------------------------------------------------------------*/
/** Reflow Y= equation rows after text change. Called by menu_insert_text. */
void yeq_reflow_rows(void);

/** Reposition Y= cursor box over the insertion point. Called by cursor_timer_cb. */
void yeq_cursor_update(void);

/**
 * Insert @p ins into the active Y= equation at the current cursor position,
 * then restore the Y= screen. Called by math_menu_insert, test_menu_insert,
 * and menu_insert_text in calculator_core.c when return_mode == MODE_GRAPH_YEQ.
 */
void graph_ui_yeq_insert(const char *ins);

/** Sync all Y= equation labels from graph_state. Replaces direct ui_lbl_yeq_eq[] access. */
void graph_ui_sync_yeq_labels(void);

/*---------------------------------------------------------------------------
 * Token handler functions (called from Execute_Token dispatcher)
 *---------------------------------------------------------------------------*/
bool handle_yeq_mode(Token_t t);
/* handle_zoom_mode declared in ui_graph_zoom.h */
bool handle_zbox_mode(Token_t t);
bool handle_trace_mode(Token_t t);

#endif /* APP_GRAPH_UI_H */
