/**
 * @file    graph_ui.c
 * @brief   Graph screen UI handlers and helpers (Y=, RANGE, ZOOM, TRACE, ZBox).
 *
 * Extracted from calculator_core.c following the same pattern as ui_matrix.c
 * and ui_prgm.c. Zero behavioral changes — purely a file organisation refactor.
 *
 * All LVGL calls must be made under lvgl_lock()/lvgl_unlock() except from
 * cursor_timer_cb (which runs inside lv_task_handler — mutex already held).
 */

#include "graph_ui.h"
#include "graph_ui_range.h"
#include "ui_param_yeq.h"
#include "ui_graph_zoom.h"
#include "calc_internal.h"
#include "app_common.h"
#include "graph.h"
#include "expr_util.h"
#include "calc_engine.h"
#include "ui_palette.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/

typedef struct {
    float   x;          /* Current trace x position */
    uint8_t eq_idx;     /* Which equation the crosshair is on */
} TraceState_t;

typedef struct {
    int32_t  px,  py;           /* Current cursor pixel position */
    int32_t  px1, py1;          /* First corner (valid once corner1_set) */
    bool     corner1_set;
} ZBoxState_t;

typedef struct {
    uint8_t  selected;          /* Which Y= row is active */
    uint8_t  cursor_pos;        /* Byte offset of insertion point within the equation */
    bool     on_equal;          /* True if cursor is on the '=' sign */
} YeqEditorState_t;

/*---------------------------------------------------------------------------
 * LVGL object pointers — screen pointers are non-static (extern in headers)
 *---------------------------------------------------------------------------*/

/* Screen pointer — private to graph_ui.c; accessed externally via Graph_*YeqScreen() */
static lv_obj_t *ui_graph_yeq_screen   = NULL;
/* ui_graph_zoom_screen defined in ui_graph_zoom.c */
/* ui_graph_range_screen and ui_graph_zoom_factors_screen defined in graph_ui_range.c */

/* Y= editor labels and cursor */
static lv_obj_t *ui_lbl_yeq_name[GRAPH_NUM_EQ];
static lv_obj_t *ui_lbl_yeq_equal[GRAPH_NUM_EQ];
static lv_obj_t *ui_lbl_yeq_eq[GRAPH_NUM_EQ];
static lv_obj_t *yeq_cursor_box   = NULL;
static lv_obj_t *yeq_cursor_inner = NULL;

/* ZOOM FACTORS labels and cursor in graph_ui_range.c */

/*---------------------------------------------------------------------------
 * State instances
 *---------------------------------------------------------------------------*/

static YeqEditorState_t   s_yeq   = {0};
static TraceState_t       s_trace = {0};
static ZBoxState_t        s_zbox  = { .px = GRAPH_W / 2, .py = GRAPH_H / 2 };
/* ZoomMenuState_t s_zoom in ui_graph_zoom.c */
/* RangeEditorState_t s_range and ZoomFactorsState_t s_zf in graph_ui_range.c */

/*---------------------------------------------------------------------------
 * Forward declarations for static helpers
 *---------------------------------------------------------------------------*/

static bool yeq_cursor_move(Token_t t);
static bool yeq_row_switch(Token_t t);
static void yeq_del_at_cursor(void);

/*---------------------------------------------------------------------------
 * Screen show/hide/visibility
 *---------------------------------------------------------------------------*/

void Graph_ShowYeqScreen(void) { lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN); }
void Graph_HideYeqScreen(void) { lv_obj_add_flag(ui_graph_yeq_screen,   LV_OBJ_FLAG_HIDDEN); }
bool Graph_IsYeqScreenVisible(void)
{
    return ui_graph_yeq_screen != NULL &&
           !lv_obj_has_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------------------
 * Initialisation helpers (one per screen)
 *---------------------------------------------------------------------------*/

static void ui_init_yeq_screen(lv_obj_t *parent)
{
    ui_graph_yeq_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_graph_yeq_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_yeq_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_yeq_screen,
                               lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_graph_yeq_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_yeq_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);

    const char *eq_row_names[] = { "Y\xe2\x82\x81", "Y\xe2\x82\x82", "Y\xe2\x82\x83", "Y\xe2\x82\x84" };
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        int32_t row_y = 4 + i * 26;
        ui_lbl_yeq_name[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_name[i], 4, row_y);
        lv_obj_set_style_text_font(ui_lbl_yeq_name[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_yeq_name[i], eq_row_names[i]);

        ui_lbl_yeq_equal[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_equal[i], 31, row_y); /* Shifted right from name */
        lv_obj_set_style_text_font(ui_lbl_yeq_equal[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_equal[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_yeq_equal[i], "=");

        ui_lbl_yeq_eq[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_eq[i], 44, row_y);
        lv_obj_set_width(ui_lbl_yeq_eq[i], DISPLAY_W - 48);
        lv_obj_set_style_text_font(ui_lbl_yeq_eq[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_eq[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_long_mode(ui_lbl_yeq_eq[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(ui_lbl_yeq_eq[i], "");
    }

    cursor_box_create(ui_graph_yeq_screen, true, &yeq_cursor_box, &yeq_cursor_inner);
}


/* ui_init_range_screen: moved to graph_ui_range.c */
/* ui_init_zoom_screen: moved to ui_graph_zoom.c */
/* ui_init_zoom_factors_screen: moved to graph_ui_range.c */

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/

/**
 * @brief Creates the Y=, RANGE, ZOOM, ZOOM FACTORS screens and graph canvas.
 *        Called once from StartCalcCoreTask under lvgl_lock().
 */
void ui_init_graph_screens(void)
{
    lv_obj_t *scr = lv_scr_act();

    ui_init_yeq_screen(scr);
    param_yeq_init_screen(scr);
    graph_ui_range_init_screens(scr);   /* RANGE + ZOOM FACTORS (graph_ui_range.c) */
    ui_init_zoom_screen(scr);

    /* Init graph canvas */
    Graph_Init(scr);
}

/*---------------------------------------------------------------------------
 * Display update helpers (some non-static — called from calculator_core.c)
 *---------------------------------------------------------------------------*/

/* ui_update_range_display: moved to graph_ui_range.c */

/**
 * @brief Reflows Y= row positions after equation text changes.
 *        Must be called under lvgl_lock().
 */
void yeq_reflow_rows(void)
{
    lv_obj_update_layout(ui_graph_yeq_screen);
    int32_t y = 4;
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        lv_obj_set_pos(ui_lbl_yeq_name[i],  4,  y);
        lv_obj_set_pos(ui_lbl_yeq_equal[i], 31, y);
        lv_obj_set_pos(ui_lbl_yeq_eq[i],     44, y);
        int32_t h = lv_obj_get_height(ui_lbl_yeq_eq[i]);
        if (h < 26) h = 26;
        y += h + 2;
    }
}

/**
 * @brief Positions the Y= equation editor cursor.
 *        Called from cursor_timer_cb — must NOT call lvgl_lock().
 */
void yeq_cursor_update(void)
{
    if (yeq_cursor_box == NULL) return;
    if (s_yeq.on_equal) {
        cursor_render(yeq_cursor_box, yeq_cursor_inner,
                      ui_lbl_yeq_equal[s_yeq.selected], 0,
                      cursor_visible, current_mode, insert_mode);
    } else {
        if (ui_lbl_yeq_eq[s_yeq.selected] == NULL) return;
        const char *txt = lv_label_get_text(ui_lbl_yeq_eq[s_yeq.selected]);
        uint32_t glyph_pos = ExprUtil_Utf8ByteToGlyph(txt, s_yeq.cursor_pos);
        cursor_render(yeq_cursor_box, yeq_cursor_inner,
                      ui_lbl_yeq_eq[s_yeq.selected], glyph_pos,
                      cursor_visible, current_mode, insert_mode);
    }
}

/* range_cursor_update: moved to graph_ui_range.c */

/* Highlights the active Y= row label in yellow; all others are white. */
static void yeq_update_highlight(void)
{
    for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
        /* Name is yellow if selected row, white otherwise */
        lv_color_t name_col = (i == s_yeq.selected) ? lv_color_hex(COLOR_YELLOW)
                                                   : lv_color_hex(COLOR_WHITE);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i], name_col, 0);

        /* Equal sign is amber if enabled, inactive grey if disabled */
        lv_color_t eq_col = Graph_GetState()->enabled[i] ? lv_color_hex(COLOR_AMBER)
                                                         : lv_color_hex(COLOR_GREY_INACTIVE);
        lv_obj_set_style_text_color(ui_lbl_yeq_equal[i], eq_col, 0);
    }
}

/* apply_zoom_preset, ui_update_zoom_display: moved to ui_graph_zoom.c */

/*---------------------------------------------------------------------------
 * Graph/menu state helpers
 *---------------------------------------------------------------------------*/

static uint8_t find_first_active_eq(void)
{
    for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
        if (strlen(Graph_GetState()->equations[i]) > 0 && Graph_GetState()->enabled[i])
            return i;
    }
    return 0;
}

/* range_field_max, range_sync_names, range_field_reset, range_field_value,
 * range_load_field: moved to graph_ui_range.c */
/* zoom_menu_reset, zoom_show_graph, zoom_scale_view, zoom_enter_factors,
 * zoom_execute_item: moved to ui_graph_zoom.c */

/* Enter ZBox rubber-band selection mode.
 * Kept here (not in ui_graph_zoom.c) because it initialises s_zbox, which is
 * owned by handle_zbox_mode in this file. Declared in calc_internal.h. */
void zoom_enter_zbox(void)
{
    s_zbox.px = GRAPH_W / 2; s_zbox.py = GRAPH_H / 2; s_zbox.corner1_set = false;
    current_mode = MODE_GRAPH_ZBOX;
    lvgl_lock();
    Zoom_HideScreen();
    Graph_SetVisible(true);
    Graph_DrawZBox(s_zbox.px, s_zbox.py, 0, 0, false, angle_degrees);
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * RANGE editor helpers: range_commit_field, range_update_highlight
 * moved to graph_ui_range.c
 *---------------------------------------------------------------------------*/

/* ZOOM FACTORS helpers (zoom_factors_reset/load/commit/update_highlight,
 * ui_update_zoom_factors_display, zoom_factors_cursor_update) and
 * persist accessors (graph_ui_get/set_zoom_facts): moved to graph_ui_range.c */

/*---------------------------------------------------------------------------
 * Y= helpers called from calculator_core.c
 *---------------------------------------------------------------------------*/

/** Sync all Y= equation labels from graph_state after a persist load. */
void graph_ui_sync_yeq_labels(void)
{
    for (int i = 0; i < GRAPH_NUM_EQ; i++)
        lv_label_set_text(ui_lbl_yeq_eq[i], Graph_GetState()->equations[i]);
    yeq_update_highlight();
}

/**
 * @brief Insert @p ins into the active Y= equation at the cursor, restore
 *        the Y= screen, and update the display.  Called from menu_insert_text,
 *        math_menu_insert, and test_menu_insert in calculator_core.c when the
 *        return mode is MODE_GRAPH_YEQ.
 *
 * Sets current_mode = MODE_GRAPH_YEQ and acquires lvgl_lock() internally.
 */
void graph_ui_yeq_insert(const char *ins)
{
    current_mode = MODE_GRAPH_YEQ;
    char *eq = Graph_GetEquationBuf(s_yeq.selected);
    size_t ins_len = strlen(ins);
    size_t eq_len  = strlen(eq);
    if (eq_len + ins_len < 63) {
        memmove(&eq[s_yeq.cursor_pos + ins_len], &eq[s_yeq.cursor_pos],
                eq_len - s_yeq.cursor_pos + 1);
        memcpy(&eq[s_yeq.cursor_pos], ins, ins_len);
        s_yeq.cursor_pos += (uint8_t)ins_len;
    }
    lvgl_lock();
    lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
    yeq_reflow_rows();
    yeq_cursor_update();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * nav_to — navigates to a graph-related screen from any current mode
 *---------------------------------------------------------------------------*/

/**
 * @brief Navigates to a graph-related screen from any current mode.
 *
 * Handles all hide/show/state-reset logic in one place.
 * Pass MODE_NORMAL to press GRAPH: renders the graph canvas.
 */
void nav_to(CalcMode_t target)
{
    lvgl_lock();
    hide_all_screens();
    current_mode = target;

    switch (target) {
    case MODE_GRAPH_YEQ:
        Graph_SetActive(false);
        s_yeq.on_equal   = false;
        s_yeq.cursor_pos = (uint8_t)strlen(Graph_GetEquationBuf(s_yeq.selected));
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            lv_label_set_text(ui_lbl_yeq_eq[i], Graph_GetState()->equations[i]);
        yeq_update_highlight();
        yeq_reflow_rows();
        yeq_cursor_update();
        break;

    case MODE_GRAPH_RANGE:
        Graph_SetActive(false);
        range_nav_enter();
        break;

    case MODE_GRAPH_ZOOM:
        Graph_SetActive(false);
        zoom_menu_reset();
        Zoom_ShowScreen();
        ui_update_zoom_display();
        break;

    case MODE_NORMAL:  /* TOKEN_GRAPH — show graph canvas and render */
        Graph_SetActive(true);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        break;

    case MODE_GRAPH_TRACE:
        Graph_SetActive(true);
        if (Graph_GetState()->param_mode) {
            /* Find first enabled parametric pair */
            s_trace.eq_idx = 0;
            for (uint8_t i = 0; i < GRAPH_NUM_PARAM; i++) {
                if (Graph_GetState()->param_enabled[i] &&
                    strlen(Graph_GetState()->param_x[i]) > 0 &&
                    strlen(Graph_GetState()->param_y[i]) > 0) {
                    s_trace.eq_idx = i;
                    break;
                }
            }
            /* Start trace at midpoint of T range */
            s_trace.x = (Graph_GetState()->t_min + Graph_GetState()->t_max) * 0.5f;
        } else {
            s_trace.eq_idx = find_first_active_eq();
            s_trace.x      = (Graph_GetState()->x_min + Graph_GetState()->x_max) * 0.5f;
        }
        Graph_SetVisible(true);
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        break;

    case MODE_GRAPH_PARAM_YEQ:
        Graph_SetActive(false);
        param_yeq_nav_enter();
        break;

    default:
        break;
    }
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Per-mode token handlers
 *---------------------------------------------------------------------------*/

/**
 * @brief Handles cursor navigation tokens within the Y= editor.
 *
 * Covers: LEFT, RIGHT, UP, DOWN, DEL, CLEAR, ENTER, INS, and graph
 * navigation keys (RANGE, ZOOM, TRACE, GRAPH, Y_EQUALS).
 *
 * @return true if the token was consumed, false otherwise.
 */
/* Cursor movement within the current equation: LEFT, RIGHT, INS. */
static bool yeq_cursor_move(Token_t t)
{
    char *eq       = Graph_GetEquationBuf(s_yeq.selected);
    uint8_t eq_len = (uint8_t)strlen(eq);
    switch (t) {
    case TOKEN_LEFT:
        if (s_yeq.on_equal) return true;
        if (s_yeq.cursor_pos > 0) {
            if (ExprUtil_MatrixTokenSizeBefore(eq, s_yeq.cursor_pos) == 3) {
                s_yeq.cursor_pos -= 3;
            } else {
                do { s_yeq.cursor_pos--; }
                while (s_yeq.cursor_pos > 0 &&
                       ((uint8_t)eq[s_yeq.cursor_pos] & 0xC0) == 0x80);
            }
        } else {
            s_yeq.on_equal = true;
        }
        break;
    case TOKEN_RIGHT:
        if (s_yeq.on_equal) {
            s_yeq.on_equal   = false;
            s_yeq.cursor_pos = 0;
        } else if (s_yeq.cursor_pos < eq_len) {
            uint8_t mat = ExprUtil_MatrixTokenSizeAt(eq, s_yeq.cursor_pos, eq_len);
            if (mat) {
                s_yeq.cursor_pos += mat;
            } else {
                uint8_t step = ExprUtil_Utf8CharSize(&eq[s_yeq.cursor_pos]);
                s_yeq.cursor_pos += step ? step : 1;
            }
            if (s_yeq.cursor_pos > eq_len) s_yeq.cursor_pos = eq_len;
        }
        break;
    case TOKEN_INS:
        insert_mode = !insert_mode;
        break;
    default:
        return false;
    }
    lvgl_lock();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

/* Row switching between Y1–Y4 equations: UP, DOWN, ENTER. */
static bool yeq_row_switch(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_yeq.selected > 0) s_yeq.selected--;
        if (!s_yeq.on_equal)
            s_yeq.cursor_pos = strlen(Graph_GetState()->equations[s_yeq.selected]);
        break;
    case TOKEN_ENTER:
        if (s_yeq.on_equal) {
            Graph_SetEquationEnabled(s_yeq.selected, !Graph_GetState()->enabled[s_yeq.selected]);
            lvgl_lock();
            yeq_update_highlight();
            lvgl_unlock();
            return true;
        }
        __attribute__((fallthrough));
    case TOKEN_DOWN:
        if (s_yeq.selected < GRAPH_NUM_EQ - 1) s_yeq.selected++;
        if (!s_yeq.on_equal)
            s_yeq.cursor_pos = strlen(Graph_GetState()->equations[s_yeq.selected]);
        break;
    default:
        return false;
    }
    lvgl_lock();
    yeq_update_highlight();
    yeq_reflow_rows();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

/* Delete the character immediately before the cursor in the current equation. */
static void yeq_del_at_cursor(void)
{
    char *eq       = Graph_GetEquationBuf(s_yeq.selected);
    uint8_t eq_len = (uint8_t)strlen(eq);
    if (s_yeq.cursor_pos == 0) return;
    uint8_t prev;
    uint8_t mat = ExprUtil_MatrixTokenSizeBefore(eq, s_yeq.cursor_pos);
    if (mat) {
        prev = s_yeq.cursor_pos - mat;
    } else {
        prev = s_yeq.cursor_pos;
        do { prev--; }
        while (prev > 0 && ((uint8_t)eq[prev] & 0xC0) == 0x80);
    }
    memmove(&eq[prev], &eq[s_yeq.cursor_pos], eq_len - s_yeq.cursor_pos + 1);
    s_yeq.cursor_pos = prev;
    lvgl_lock();
    lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
    yeq_reflow_rows();
    yeq_cursor_update();
    lvgl_unlock();
}

static bool handle_yeq_navigation(Token_t t)
{
    char *eq = Graph_GetEquationBuf(s_yeq.selected);
    switch (t) {
    case TOKEN_GRAPH:    nav_to(MODE_NORMAL);                      return true;
    case TOKEN_RANGE:    nav_to(MODE_GRAPH_RANGE);                 return true;
    case TOKEN_ZOOM:     zoom_menu_reset(); nav_to(MODE_GRAPH_ZOOM); return true;
    case TOKEN_TRACE:    nav_to(MODE_GRAPH_TRACE);                 return true;
    case TOKEN_Y_EQUALS:
        current_mode = MODE_NORMAL;
        lvgl_lock(); hide_all_screens(); lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
        eq[0] = '\0'; s_yeq.cursor_pos = 0;
        lvgl_lock();
        lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
        yeq_reflow_rows(); yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_DEL:      yeq_del_at_cursor();                      return true;
    default:
        return yeq_cursor_move(t) || yeq_row_switch(t);
    }
}

/**
 * @brief Handles character and token insertion tokens within the Y= editor.
 *
 * Covers: digit keys, operator keys, function keys (sin/cos/etc.), letter
 * keys (ALPHA layer), and other tokens that append text to the equation.
 *
 * @return true if the token was consumed, false otherwise.
 */
static bool handle_yeq_insertion(Token_t t)
{
    char *eq     = Graph_GetEquationBuf(s_yeq.selected);
    uint8_t eq_len = (uint8_t)strlen(eq);
    const char *append = NULL;
    char num_buf[2] = {0, 0};

    switch (t) {
    case TOKEN_X_T:   append = "x";     break;
    case TOKEN_0 ... TOKEN_9:
        num_buf[0] = (char)((t - TOKEN_0) + '0');
        append = num_buf;
        break;
    case TOKEN_DECIMAL: append = ".";     break;
    case TOKEN_EE:      append = "*10^";  break;
    case TOKEN_E_X:     append = "exp(";  break;
    case TOKEN_TEN_X:   append = "10^(";  break;
    case TOKEN_ADD:     append = "+";     break;
    case TOKEN_SUB:     append = "-";     break;
    case TOKEN_MULT:    append = "*";     break;
    case TOKEN_DIV:     append = "/";     break;
    case TOKEN_POWER:   append = "^";     break;
    case TOKEN_L_PAR:   append = "(";     break;
    case TOKEN_R_PAR:   append = ")";     break;
    case TOKEN_SIN:     append = "sin(";  break;
    case TOKEN_COS:     append = "cos(";  break;
    case TOKEN_TAN:     append = "tan(";  break;
    case TOKEN_ASIN:    append = "sin\xEE\x80\x81("; break;   /* sin⁻¹( */
    case TOKEN_ACOS:    append = "cos\xEE\x80\x81("; break;   /* cos⁻¹( */
    case TOKEN_ATAN:    append = "tan\xEE\x80\x81("; break;   /* tan⁻¹( */
    case TOKEN_LN:      append = "ln(";   break;
    case TOKEN_LOG:     append = "log(";  break;
    case TOKEN_SQRT:    append = "\xE2\x88\x9A("; break;
    case TOKEN_ABS:     append = "abs(";  break;
    case TOKEN_SQUARE:  append = "^2";    break;
    case TOKEN_PI:      append = "π";     break;
    case TOKEN_NEG:     append = "-";     break;
    case TOKEN_X_INV:   append = "\xEE\x80\x81";   break;  /* ⁻¹ U+E001 */
    case TOKEN_ANS:     append = "ANS";   break;
    case TOKEN_MATRX:
    case TOKEN_MTRX_A:
    case TOKEN_MTRX_B:
    case TOKEN_MTRX_C:
        return true;
    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z: {
        static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        num_buf[0] = alpha_chars[t - TOKEN_A];
        append = num_buf;
        break;
    }
    default: return false;
    }

    if (append != NULL && !s_yeq.on_equal) {
        size_t slen = strlen(append);
        uint8_t eq_len_u8 = (uint8_t)eq_len;
        if (slen == 1) {
            ExprUtil_InsertChar(eq, &eq_len_u8, &s_yeq.cursor_pos, 63, insert_mode, append[0]);
        } else if (eq_len_u8 + (uint8_t)slen < 63) {
            ExprUtil_InsertStr(eq, &eq_len_u8, &s_yeq.cursor_pos, 63, append);
        } else {
            return true;
        }
        lvgl_lock();
        lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
    }
    return true;
}

bool handle_yeq_mode(Token_t t)
{
    if (s_yeq.cursor_pos > (uint8_t)strlen(Graph_GetState()->equations[s_yeq.selected]))
        s_yeq.cursor_pos = (uint8_t)strlen(Graph_GetState()->equations[s_yeq.selected]);

    if (handle_yeq_navigation(t)) {
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    }

    /* Menu-open tokens handled inline */
    if (t == TOKEN_MATH) {
        menu_open(TOKEN_MATH, MODE_GRAPH_YEQ);
        return true;
    }
    if (t == TOKEN_TEST) {
        menu_open(TOKEN_TEST, MODE_GRAPH_YEQ);
        return true;
    }
    if (t == TOKEN_VARS) {
        menu_open(TOKEN_VARS, MODE_GRAPH_YEQ);
        return true;
    }
    if (t == TOKEN_Y_VARS) {
        menu_open(TOKEN_Y_VARS, MODE_GRAPH_YEQ);
        return true;
    }

    if (handle_yeq_insertion(t)) {
        lvgl_lock();
        ui_update_status_bar();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    }

    lvgl_lock();
    ui_update_status_bar();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

/* FieldEditor_t, field_editor_handle, handle_range_mode:
 * moved to graph_ui_range.c */
/* handle_zoom_mode: moved to ui_graph_zoom.c */
/* handle_zoom_factors_mode: moved to graph_ui_range.c */

bool handle_zbox_mode(Token_t t)
{
    switch (t) {
    case TOKEN_LEFT:
        if (s_zbox.px > 0) s_zbox.px--;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (s_zbox.px < GRAPH_W - 1) s_zbox.px++;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_UP:
        if (s_zbox.py > 0) s_zbox.py--;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s_zbox.py < GRAPH_H - 1) s_zbox.py++;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        if (!s_zbox.corner1_set) {
            s_zbox.px1         = s_zbox.px;
            s_zbox.py1         = s_zbox.py;
            s_zbox.corner1_set = true;
            lvgl_lock();
            Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
            lvgl_unlock();
        } else {
            int32_t x_lo = s_zbox.px1 < s_zbox.px ? s_zbox.px1 : s_zbox.px;
            int32_t x_hi = s_zbox.px1 < s_zbox.px ? s_zbox.px  : s_zbox.px1;
            int32_t y_lo = s_zbox.py1 < s_zbox.py ? s_zbox.py1 : s_zbox.py;
            int32_t y_hi = s_zbox.py1 < s_zbox.py ? s_zbox.py  : s_zbox.py1;
            if (x_hi > x_lo && y_hi > y_lo) {
                const GraphState_t *gs = Graph_GetState();
                float x_range   = gs->x_max - gs->x_min;
                float y_range   = gs->y_max - gs->y_min;
                float new_x_min = gs->x_min + (float)x_lo / (float)(GRAPH_W - 1) * x_range;
                float new_x_max = gs->x_min + (float)x_hi / (float)(GRAPH_W - 1) * x_range;
                float new_y_max = gs->y_max - (float)y_lo / (float)(GRAPH_H - 1) * y_range;
                float new_y_min = gs->y_max - (float)y_hi / (float)(GRAPH_H - 1) * y_range;
                Graph_SetWindow(new_x_min, new_x_max, new_y_min, new_y_max,
                                gs->x_scl, gs->y_scl, gs->x_res);
            }
            current_mode       = MODE_NORMAL;
            s_zbox.corner1_set = false;
            lvgl_lock();
            Graph_ClearTrace();
            Graph_Render(angle_degrees);
            lvgl_unlock();
        }
        return true;
    case TOKEN_CLEAR:
        s_zbox.corner1_set = false;
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return true;
    case TOKEN_ZOOM:
        s_zbox.corner1_set = false;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_GRAPH:
        s_zbox.corner1_set = false;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_Y_EQUALS:
        s_zbox.corner1_set = false;
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        s_zbox.corner1_set = false;
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_TRACE:
        s_zbox.corner1_set = false;
        nav_to(MODE_GRAPH_TRACE);
        return true;
    default:
        s_zbox.corner1_set = false;
        nav_to(MODE_NORMAL);
        return false; /* fall through to main switch */
    }
}

bool handle_trace_mode(Token_t t)
{
    const GraphState_t *gs = Graph_GetState();
    float step = gs->param_mode
        ? gs->t_step
        : (gs->x_max - gs->x_min) / (float)(GRAPH_W - 1);
    if (step <= 0.0f) step = 0.1309f;

    switch (t) {
    case TOKEN_LEFT:
        if (gs->param_mode) {
            if (s_trace.x > gs->t_min) s_trace.x -= step;
        } else {
            if (s_trace.x > gs->x_min) s_trace.x -= step;
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (gs->param_mode) {
            if (s_trace.x < gs->t_max) s_trace.x += step;
        } else {
            if (s_trace.x < gs->x_max) s_trace.x += step;
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_UP:
        if (gs->param_mode) {
            /* Cycle backward through enabled parametric pairs */
            for (uint8_t i = 1; i <= GRAPH_NUM_PARAM; i++) {
                uint8_t idx = (s_trace.eq_idx + GRAPH_NUM_PARAM - i) % GRAPH_NUM_PARAM;
                if (gs->param_enabled[idx] &&
                    strlen(gs->param_x[idx]) > 0 &&
                    strlen(gs->param_y[idx]) > 0) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        } else {
            for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
                uint8_t idx = (s_trace.eq_idx + GRAPH_NUM_EQ - i) % GRAPH_NUM_EQ;
                if (strlen(gs->equations[idx]) > 0 && gs->enabled[idx]) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (gs->param_mode) {
            for (uint8_t i = 1; i <= GRAPH_NUM_PARAM; i++) {
                uint8_t idx = (s_trace.eq_idx + i) % GRAPH_NUM_PARAM;
                if (gs->param_enabled[idx] &&
                    strlen(gs->param_x[idx]) > 0 &&
                    strlen(gs->param_y[idx]) > 0) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        } else {
            for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
                uint8_t idx = (s_trace.eq_idx + i) % GRAPH_NUM_EQ;
                if (strlen(gs->equations[idx]) > 0 && gs->enabled[idx]) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_TRACE:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        Graph_ClearTrace();
        Graph_Render(angle_degrees);
        lvgl_unlock();
        return true;
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
        nav_to(MODE_NORMAL);
        return true;
    default:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}
