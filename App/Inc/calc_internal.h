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
 *   graph_ui.c          — graph editor screens (Y=, RANGE, ZOOM, TRACE, ZBox)
 *   ui_param_yeq.c      — parametric Y= editor screen (X₁t/Y₁t … X₃t/Y₃t)
 *   ui_matrix.c         — matrix cell editor and MATRIX menu
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
#include "lvgl.h"
#include "calc_engine.h"
#include "ui_mode.h"    /* ModeScreenState_t, s_mode, ui_mode_open, handle_mode_screen */
#include "ui_input.h"   /* expr_delete_at_cursor, handle_normal_mode, handle_sto_pending */

/* LVGL Fonts */
extern const lv_font_t jetbrains_mono_24;
extern const lv_font_t jetbrains_mono_20;

/* Global Calculator States */
extern CalcMode_t current_mode;
extern CalcMode_t return_mode;
extern bool         insert_mode;
extern bool         cursor_visible;
extern float ans;
extern bool ans_is_matrix;
extern bool angle_degrees;
extern bool sto_pending;

/* Menu visible rows constant */
#define MENU_VISIBLE_ROWS 7

/* Main Display / Screens */
extern lv_obj_t *ui_matrix_screen;
extern lv_obj_t *ui_matrix_edit_screen;

/* Graph screen pointers (defined in graph_ui.c) */
extern lv_obj_t *ui_graph_yeq_screen;
extern lv_obj_t *ui_graph_range_screen;
extern lv_obj_t *ui_graph_zoom_screen;
extern lv_obj_t *ui_graph_zoom_factors_screen;
extern lv_obj_t *ui_param_yeq_screen;

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
void ui_update_zoom_display(void);
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

#define HISTORY_LINE_COUNT  1           /* Expression+result pairs stored in history (TI-81 spec: 1 entry) */
#define MAX_EXPR_LEN        96          /* Supports up to 4 wrapped display rows */
#define MAX_RESULT_LEN      96   /* 32 for scalars; up to ~80 for 3×3 matrix rows */

#define MATRIX_RING_COUNT   1   /* ring buffer slots for matrix history results */

typedef struct {
    char    expression[MAX_EXPR_LEN];
    char    result[MAX_RESULT_LEN];   /* Scalar/error result; newline-separated rows for matrix fallback */
    bool    has_matrix;               /* True when this entry holds a matrix result */
    uint8_t matrix_ring_idx;         /* Ring slot index (0..MATRIX_RING_COUNT-1); valid iff has_matrix */
    uint8_t matrix_ring_gen;         /* Expected generation at that slot; mismatch means evicted */
    uint8_t matrix_rows_cache;       /* Cached row count; valid even after eviction */
} HistoryEntry_t;

extern HistoryEntry_t history[HISTORY_LINE_COUNT];
extern uint8_t        history_count;
extern int8_t         history_recall_offset;
extern int8_t         matrix_scroll_focus;   /* history slot with scroll focus; -1=none */
extern uint8_t        matrix_scroll_offset;  /* horizontal character scroll offset */

extern char         expression[MAX_EXPR_LEN];
extern uint8_t      expr_len;
extern uint8_t      cursor_pos;

void ui_update_history(void);
void ui_refresh_display(void);
void ui_output_row(uint8_t row_1based, const char *text);
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size, float *ans_ptr);
void handle_history_nav(Token_t t);      /* sub-handler for history/cursor nav keys */
void reset_matrix_scroll_focus(void);   /* clears matrix_scroll_focus/offset */

#endif /* APP_CALC_INTERNAL_H */
