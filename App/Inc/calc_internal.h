/**
 * @file    calc_internal.h
 * @brief   Shared internal states and helpers for calculator UI modules.
 */

#ifndef APP_CALC_INTERNAL_H
#define APP_CALC_INTERNAL_H

/*
 * UI Module Shared State — intentionally broad scope
 *
 * This header is the single shared-state contract for the multi-file UI module
 * set.  It is intended to be included ONLY by the following translation units:
 *
 *   calculator_core.c   — dispatcher, main screen, token handling
 *   graph_ui.c          — graph editor screens (Y=, TRACE, ZBox)
 *   ui_graph_zoom.c     — ZOOM menu screen (extracted from graph_ui.c — Item 4)
 *   ui_param_yeq.c      — parametric Y= editor screen (X₁t/Y₁t … X₃t/Y₃t)
 *   ui_matrix.c         — matrix cell editor and MATRIX menu
 *   ui_math_menu.c      — MATH/NUM/HYP/PRB and TEST menus
 *   ui_prgm.c           — program menu, line editor, and CTL/I/O sub-menus
 *   ui_mode.c           — MODE settings screen
 *   ui_input.c          — normal-mode expression input handlers
 *
 * These files form a single logical "super-module" that was split purely to
 * keep individual translation units at a manageable size.  They share
 * calculator state (current_mode, ans, history, expression, …) and LVGL object
 * pointers as if they were one file, which is why the externs here are numerous.
 *
 * Do NOT narrow the scope of this header in an attempt to "hide" state.  The
 * broad sharing is the correct pattern for the multi-file UI extraction — any
 * attempt to split it further would require either a deep refactor of the
 * ownership model or additional accessor indirection that adds complexity without
 * benefit.  If you are adding a genuinely new module that is not part of the
 * UI super-module, do not include this header; expose only what you need via a
 * dedicated public API header instead.
 */

#include "app_common.h"
#include "calc_history.h" /* HistoryEntry_t, HISTORY_LINE_COUNT, CalcHistory_* API */
#include "calculator_core.h" /* Calc_GetAns / Calc_SetAnsScalar / Calc_SetAnsMatrix */
#include "expr_util.h"    /* ExprBuffer_t and ExprBuffer_* / ExprUtil_* helpers */
#include "lvgl.h"
#include "calc_engine.h"
#include "ui_mode.h"      /* ModeScreenState_t, s_mode, ui_mode_open, handle_mode_screen */
#include "ui_input.h"     /* expr_delete_at_cursor, handle_normal_mode, handle_sto_pending */
#include "ui_math_menu.h" /* ui_math_screen, ui_test_screen, math/test menu open/close/handlers */

/* LVGL Fonts */
extern const lv_font_t jetbrains_mono_24;
extern const lv_font_t jetbrains_mono_20;

/* Global Calculator States */
/* current_mode and return_mode are now private to calculator_core.c.
 * Use Calc_GetMode() / Calc_SetMode() / Calc_GetReturnMode() / Calc_SetReturnMode()
 * declared in calculator_core.h (included above). */
extern bool         insert_mode;
extern bool         cursor_visible;
/* ans and ans_is_matrix are now private to calculator_core.c.
 * Use Calc_GetAns() / Calc_SetAnsScalar() / Calc_SetAnsMatrix()
 * declared in calculator_core.h (included above). */
extern bool angle_degrees;
extern bool sto_pending;

/* Menu visible rows constant */
#define MENU_VISIBLE_ROWS 7

/* Shared UI functions */
void cursor_render(lv_obj_t *box, lv_obj_t *inner, lv_obj_t *parent_label,
                   uint32_t glyph_pos, bool visible, CalcMode_t mode, bool insert);
void cursor_box_create(lv_obj_t *parent, bool is_overlay, lv_obj_t **out_box, lv_obj_t **out_inner);
void hide_all_screens(void);
void nav_to(CalcMode_t target);
void Update_Calculator_Display(void);
CalcMode_t menu_close(Token_t menu_token);
void menu_open(Token_t menu_token, CalcMode_t return_to);
void ui_update_status_bar(void);
void graph_ui_yeq_insert(const char *ins);
/* ui_update_zoom_display declared in ui_graph_zoom.h */
/* zoom_enter_zbox: defined in graph_ui.c; initialises s_zbox (ZBox state owner)
 * then enters MODE_GRAPH_ZBOX. Called from zoom_execute_item in ui_graph_zoom.c. */
void zoom_enter_zbox(void);
void lvgl_lock(void);
void lvgl_unlock(void);
lv_obj_t *screen_create(lv_obj_t *parent);
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll, uint8_t tab_count, bool left, void (*update)(void));
void menu_insert_text(const char *ins, CalcMode_t *ret_mode);

#define DISPLAY_W           320
#define DISPLAY_H           240
#define DISP_ROW_COUNT      8           /* Visible text rows on the main screen */
#define DISP_ROW_H          30          /* Pixels per row */
#define CURSOR_BLINK_MS     530         /* Cursor blink interval */

/* HISTORY_LINE_COUNT, MAX_RESULT_LEN, MATRIX_RING_COUNT, HistoryEntry_t, and
 * the CalcHistory_* API are now in calc_history.h (included above). */

extern ExprBuffer_t expr;   /* expression buffer: .buf, .len, .cursor */

void ui_refresh_display(void);
void ui_output_row(uint8_t row_1based, const char *text);
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size);
void handle_history_nav(Token_t t);      /* sub-handler for history/cursor nav keys */

#endif /* APP_CALC_INTERNAL_H */
