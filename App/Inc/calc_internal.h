/**
 * @file    calc_internal.h
 * @brief   Shared internal states and helpers for calculator UI modules.
 */

#ifndef APP_CALC_INTERNAL_H
#define APP_CALC_INTERNAL_H

/*
 * UI Module Shared State — intentionally broad scope
 *
 * This header is the shared-state contract for the multi-file UI super-module.
 * It is intended to be included ONLY by the following translation units:
 *
 *   calculator_core.c   — dispatcher, main screen, token handling
 *   graph_ui.c          — graph editor screens (Y=, TRACE, ZBox)
 *   graph_ui_range.c    — RANGE and ZOOM FACTORS editors (extracted from graph_ui.c)
 *   ui_graph_zoom.c     — ZOOM menu screen
 *   ui_param_yeq.c      — parametric Y= editor screen (X₁t/Y₁t … X₃t/Y₃t)
 *   ui_matrix.c         — matrix cell editor and MATRIX menu
 *   ui_math_menu.c      — MATH/NUM/HYP/PRB and TEST menus
 *   ui_prgm.c           — program menu, line editor, and CTL/I/O sub-menus
 *   ui_mode.c           — MODE settings screen
 *   ui_input.c          — normal-mode expression input handlers
 *
 * These files form a single logical "super-module" split purely to keep
 * individual translation units at a manageable size.  They share calculator
 * state (insert_mode, cursor_visible, sto_pending, expr) and
 * LVGL object pointers as if they were one file.
 *
 * Modules outside this set must NOT include this header.  Use ui_shared.h
 * instead for layout constants, fonts, and utility function declarations.
 * If the module also needs calc state, add a typed accessor to calculator_core.h
 * (see Calc_GetExprBuf, Calc_ResetInputState as examples).
 */

#include "ui_shared.h"    /* layout constants, fonts, utility function declarations */
#include "calc_history.h" /* HistoryEntry_t, HISTORY_LINE_COUNT, CalcHistory_* API */
#include "calculator_core.h" /* Calc_GetAns / Calc_SetAnsScalar / Calc_SetAnsMatrix */
#include "expr_util.h"    /* ExprBuffer_t and ExprBuffer_* / ExprUtil_* helpers */
#include "calc_engine.h"
#include "ui_mode.h"      /* ModeScreenState_t, s_mode, ui_mode_open, handle_mode_screen */
#include "ui_input.h"     /* expr_delete_at_cursor, handle_normal_mode, handle_sto_pending */
#include "ui_math_menu.h" /* ui_math_screen, ui_test_screen, math/test menu open/close/handlers */

/* Global Calculator States */
/* current_mode, return_mode, insert_mode, cursor_visible, sto_pending, ans,
 * ans_is_matrix, and expr are all private to calculator_core.c.
 * Use the getter/setter API declared in calculator_core.h (included above):
 *   Calc_GetMode() / Calc_SetMode() / Calc_GetReturnMode() / Calc_SetReturnMode()
 *   Calc_GetAns() / Calc_SetAnsScalar() / Calc_SetAnsMatrix()
 *   Calc_GetInsertMode() / Calc_SetInsertMode()
 *   Calc_GetCursorVisible() / Calc_SetCursorVisible()
 *   Calc_GetStoPending() / Calc_SetStoPending()
 *   Calc_GetExpr() */

/* Shared UI functions (super-module only — not in ui_shared.h) */
void cursor_render(lv_obj_t *box, lv_obj_t *inner, lv_obj_t *parent_label,
                   uint32_t glyph_pos, bool visible, CalcMode_t mode, bool insert);
void cursor_box_create(lv_obj_t *parent, bool is_overlay, lv_obj_t **out_box, lv_obj_t **out_inner);
void graph_ui_yeq_insert(const char *ins);
/* zoom_enter_zbox: defined in graph_ui.c; initialises s_zbox (ZBox state owner)
 * then enters MODE_GRAPH_ZBOX. Called from zoom_execute_item in ui_graph_zoom.c. */
void zoom_enter_zbox(void);

/* HISTORY_LINE_COUNT, MAX_RESULT_LEN, MATRIX_RING_COUNT, HistoryEntry_t, and
 * the CalcHistory_* API are now in calc_history.h (included above). */

void ui_refresh_display(void);
void ui_output_row(uint8_t row_1based, const char *text);
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size);
void handle_history_nav(Token_t t);      /* sub-handler for history/cursor nav keys */

#endif /* APP_CALC_INTERNAL_H */
