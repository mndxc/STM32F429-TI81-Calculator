/**
 * @file    graph_ui.h
 * @brief   Graph screen UI handlers and helpers (Y=, RANGE, ZOOM, TRACE, ZBox).
 *
 * Extracted from calculator_core.c following the same pattern as ui_matrix.h
 * and ui_prgm.h. Zero behavioral changes — purely a file organisation refactor.
 */

#ifndef APP_GRAPH_UI_H
#define APP_GRAPH_UI_H

#include "app_common.h"
#include "ui_param_yeq.h"
#include "lvgl.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Screen pointers (also declared as extern in calc_internal.h so that
 * hide_all_screens() and menu_close() in calculator_core.c can reach them)
 *---------------------------------------------------------------------------*/
extern lv_obj_t *ui_graph_yeq_screen;
extern lv_obj_t *ui_graph_range_screen;
extern lv_obj_t *ui_graph_zoom_screen;
extern lv_obj_t *ui_graph_zoom_factors_screen;
extern lv_obj_t *ui_param_yeq_screen;

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

/** Reposition RANGE cursor box over the active field. Called by cursor_timer_cb. */
void range_cursor_update(void);

/** Reposition ZOOM FACTORS cursor box. Called by cursor_timer_cb. */
void zoom_factors_cursor_update(void);

/** Redraw all RANGE field labels from graph_state. Called by StartCalcCoreTask after persist load. */
void ui_update_range_display(void);

/** Redraw ZOOM FACTORS labels from s_zf state. Called by StartCalcCoreTask after persist load. */
void ui_update_zoom_factors_display(void);

/** Redraw ZOOM item rows. Called by StartCalcCoreTask init. */
void ui_update_zoom_display(void);

/**
 * Insert @p ins into the active Y= equation at the current cursor position,
 * then restore the Y= screen. Called by math_menu_insert, test_menu_insert,
 * and menu_insert_text in calculator_core.c when return_mode == MODE_GRAPH_YEQ.
 */
void graph_ui_yeq_insert(const char *ins);

/** Sync all Y= equation labels from graph_state. Replaces direct ui_lbl_yeq_eq[] access. */
void graph_ui_sync_yeq_labels(void);

/*---------------------------------------------------------------------------
 * Persist accessors — s_zf is private to graph_ui.c
 *---------------------------------------------------------------------------*/
float graph_ui_get_zoom_x_fact(void);
float graph_ui_get_zoom_y_fact(void);
void  graph_ui_set_zoom_facts(float x_fact, float y_fact);

/*---------------------------------------------------------------------------
 * Token handler functions (called from Execute_Token dispatcher)
 *---------------------------------------------------------------------------*/
bool handle_yeq_mode(Token_t t);
bool handle_range_mode(Token_t t);
bool handle_zoom_mode(Token_t t);
bool handle_zoom_factors_mode(Token_t t);
bool handle_zbox_mode(Token_t t);
bool handle_trace_mode(Token_t t);

#endif /* APP_GRAPH_UI_H */
