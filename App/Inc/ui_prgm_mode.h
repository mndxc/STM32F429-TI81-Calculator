/**
 * @file    ui_prgm_mode.h
 * @brief   PRGM MODE sub-menu (NUMBER and GRAPH tabs).
 *
 * Accessed via [MODE] from the program editor.  The NUMBER tab inserts
 * display/angle mode commands (Norm, Sci, Eng, Fix, Float, Rad, Deg);
 * the GRAPH tab inserts graph-mode commands (Function, Param, Connected,
 * Dot, Sequence, Simul, Grid Off, Grid On, Rect, Polar).
 *
 * Guidebook reference: p. 8-16 / 8-18, "Setting Modes from a Program".
 */
#ifndef UI_PRGM_MODE_H
#define UI_PRGM_MODE_H

#include "app_common.h"
#include "lvgl.h"
#include <stdbool.h>

void ui_init_prgm_mode_screens(lv_obj_t *parent);
/** Unhide the NUMBER screen, reset navigation, and refresh.  Call under lvgl_lock(). */
void ui_prgm_mode_num_reset_and_show(void);
/** Unhide the GRAPH screen, reset navigation, and refresh.  Call under lvgl_lock(). */
void ui_prgm_mode_gph_reset_and_show(void);
/** Hide the NUMBER screen.  NULL-safe. */
void ui_prgm_mode_num_hide(void);
/** Hide the GRAPH screen.  NULL-safe. */
void ui_prgm_mode_gph_hide(void);
bool handle_prgm_mode_number(Token_t t);
bool handle_prgm_mode_graph(Token_t t);

#endif /* UI_PRGM_MODE_H */
