/**
 * @file    ui_param_yeq.h
 * @brief   Parametric Y= editor screen (X₁t/Y₁t … X₃t/Y₃t).
 *
 * Extracted from graph_ui.c; part of the calculator UI super-module.
 * Include calc_internal.h before this header in .c files that need it.
 */
#ifndef APP_UI_PARAM_YEQ_H
#define APP_UI_PARAM_YEQ_H

#include "app_common.h"
#include "lvgl.h"
#include <stdbool.h>

/** Number of parametric Y= rows (X₁t,Y₁t,X₂t,Y₂t,X₃t,Y₃t). */
#define PARAM_YEQ_ROW_COUNT 6

/** Screen object — extern so hide_all_screens() and calc_internal.h can reach it. */
extern lv_obj_t *ui_param_yeq_screen;

/** Create all LVGL objects for the parametric Y= screen. Called once from
 *  ui_init_graph_screens() during startup. */
void param_yeq_init_screen(lv_obj_t *parent);

/**
 * @brief Enter the parametric Y= screen (called from nav_to while lvgl_lock held).
 *
 * Resets editor state, syncs labels from graph_state, and positions the cursor.
 * Must be called with lvgl_lock() already acquired.
 */
void param_yeq_nav_enter(void);

/** Reposition the cursor box over the active equation row.
 *  Called by cursor_timer_cb (lock already held) and from handler logic. */
void param_yeq_cursor_update(void);

/** Token handler for MODE_GRAPH_PARAM_YEQ. */
bool handle_param_yeq_mode(Token_t t);

#endif /* APP_UI_PARAM_YEQ_H */
