/**
 * @file    calculator_core.c
 * @brief   Calculator logic, UI management, and FreeRTOS task implementation.
 *
 * This module handles:
 *  - LVGL UI creation and updates
 *  - Token processing from the keypad queue
 *  - Calculator input buffer management
 */

#ifdef HOST_TEST
/* Host-test build: replace all LVGL/RTOS/platform headers with stubs.
 * app_common.h (CalcMode_t, Token_t, GraphState_t) and the safe engine
 * headers are included first; the stubs header follows to provide LVGL
 * type/function no-ops and declarations for cross-module symbols. */
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "expr_util.h"
#  include "ui_palette.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "calculator_core_test_stubs.h"
#  include "calculator_core.h"
#else
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "graph.h"
#  include "graph_draw.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "calc_internal.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "ui_math_menu.h"
#  include "ui_matrix.h"
#  include "ui_prgm.h"
#  include "ui_prgm_ctl.h"
#  include "ui_prgm_io.h"
#  include "ui_prgm_exec.h"
#  include "ui_stat.h"
#  include "ui_draw.h"
#  include "ui_vars.h"
#  include "ui_yvars.h"
#  include "graph_ui.h"
#  include "ui_graph_zoom.h"
#  include "ui_palette.h"
#  include "expr_util.h"
#  include "cmsis_os.h"
#  include "lvgl.h"
#  include "main.h"
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

/* Scrollable menu geometry */
#define MATRIX_MAX_DIM      CALC_MATRIX_MAX_DIM  /* alias — actual size in calc_engine.h */
#define MATRIX_LIST_VISIBLE 7                    /* visible cell rows in the list editor */
#define MATRIX_TAB_COUNT    2           /* MATRX and EDIT tabs */
#define MATRIX_MATRX_ITEMS  6           /* Items in the MATRX operations tab */
#define MATRIX_EDIT_ITEMS   3           /* Items in the EDIT tab */



/*---------------------------------------------------------------------------
 * External references
 *---------------------------------------------------------------------------*/

extern const uint32_t TI81_LookupTable_Size;

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/


/* Editor / cursor state structs — group logically related static variables so they
 * can be snapshot, serialized, and reasoned about as a unit. */

/*---------------------------------------------------------------------------
 * Private variables
 *---------------------------------------------------------------------------*/

/* LVGL objects — main display */
static lv_obj_t *disp_rows[DISP_ROW_COUNT]; /* Full-width text rows (Montserrat 24) */

/* Cursor blink state */
bool        cursor_visible = true;
static lv_timer_t *cursor_timer   = NULL;
static lv_obj_t   *cursor_box     = NULL;  /* Filled-block cursor rectangle */
static lv_obj_t   *cursor_inner   = NULL;  /* Character label inside cursor_box */

/* Styles */
static lv_style_t style_bg;

/* Calculator state */
ExprBuffer_t expr;   /* .buf = expression string, .len = byte length, .cursor = insertion point */
static uint8_t      expr_chars_per_row = 22; /* Chars that fit on one display row; set at init */
bool         insert_mode            = false; /* false=overwrite (default), true=insert */
CalcMode_t   current_mode           = MODE_NORMAL;
CalcMode_t   return_mode            = MODE_NORMAL;
bool         angle_degrees          = true;

#ifdef HOST_TEST
/* In test builds, ans and ans_is_matrix remain non-static so test code can
 * observe and set them directly via the extern declarations in the stubs header. */
float ans          = 0.0f;
bool  ans_is_matrix = false;
#else
static float ans          = 0.0f;
static bool  ans_is_matrix = false; /* true when ans holds a matrix slot index */
#endif
bool                sto_pending    = false;  /* True after STO — next alpha stores ans */

HistoryEntry_t history[HISTORY_LINE_COUNT];
uint8_t        history_count = 0;
int8_t         history_recall_offset = 0; /* 0=not recalling; N=Nth-most-recent entry */
int8_t         matrix_scroll_focus  = -1;   /* history slot with scroll focus; -1=none */
uint8_t        matrix_scroll_offset = 0;    /* horizontal character scroll offset */

/* Matrix history ring buffer — stores CalcMatrix_t for the last MATRIX_RING_COUNT results */
static CalcMatrix_t matrix_ring[MATRIX_RING_COUNT];
static uint8_t      matrix_ring_gen_table[MATRIX_RING_COUNT]; /* generation written to each slot */
static uint8_t      matrix_ring_write_count = 0;              /* total writes, wraps at 256 */

/* graph_state is defined and owned by graph.c; access via Graph_GetState() / setters. */

/* Matrix data lives in calc_matrices[] (calc_engine.c) — accessed via extern.
 * MATH/TEST menu state, data tables, and LVGL objects live in ui_math_menu.c. */

/* MATRIX menu state */

/* MATRIX EDIT sub-screen state */




/*---------------------------------------------------------------------------
 * ANS getter/setter API (declared in calculator_core.h)
 *---------------------------------------------------------------------------*/

void  Calc_SetAnsScalar(float value)  { ans = value; ans_is_matrix = false; }
void  Calc_SetAnsMatrix(float idx)    { ans = idx;   ans_is_matrix = true;  }
float Calc_GetAns(void)               { return ans; }
bool  Calc_GetAnsIsMatrix(void)       { return ans_is_matrix; }

/*---------------------------------------------------------------------------
 * Forward declarations for helpers defined later in this file
 *---------------------------------------------------------------------------*/

/* Private sub-handlers that remain here (use private statics) */
static void history_load_offset(uint8_t offset);
static void history_enter_evaluate(void);
void        handle_history_nav(Token_t t);  /* non-static: called from ui_input.c */
void        reset_matrix_scroll_focus(void); /* clears scroll focus statics; called from ui_input.c */

/*---------------------------------------------------------------------------
 * Persistent storage helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief  Snapshot all saveable calculator state into @p out.
 *
 * Reads static-local variables directly (same translation unit).
 * graph_state.active is intentionally excluded — always boot with graph
 * hidden.
 */
void Calc_BuildPersistBlock(PersistBlock_t *out)
{
    memcpy(out->calc_variables, calc_variables, sizeof(calc_variables));
    out->ans = ans;
    memcpy(out->mode_committed, s_mode.committed, sizeof(s_mode.committed));
    out->zoom_x_fact = graph_ui_get_zoom_x_fact();
    out->zoom_y_fact = graph_ui_get_zoom_y_fact();

    /* Graph state — copy fields individually (skip active) */
    const GraphState_t *gs = Graph_GetState();
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(out->equations[i], Graph_GetEquationBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
    }
    out->x_min   = gs->x_min;
    out->x_max   = gs->x_max;
    out->y_min   = gs->y_min;
    out->y_max   = gs->y_max;
    out->x_scl   = gs->x_scl;
    out->y_scl   = gs->y_scl;
    out->x_res   = gs->x_res;
    out->grid_on = gs->grid_on ? 1u : 0u;
    for (int i = 0; i < 4; i++) out->enabled[i] = gs->enabled[i] ? 1u : 0u;

    /* Matrices [A], [B], [C] — save dimensions and flatten 6×6 data arrays */
    for (int m = 0; m < 3; m++) {
        out->matrix_rows[m] = calc_matrices[m].rows;
        out->matrix_cols[m] = calc_matrices[m].cols;
        memcpy(out->matrix_data[m], calc_matrices[m].data,
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }

    /* Parametric equations and T range */
    for (int i = 0; i < GRAPH_NUM_PARAM; i++) {
        memcpy(out->param_x[i], Graph_GetParamEquationXBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        memcpy(out->param_y[i], Graph_GetParamEquationYBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        out->param_enabled[i] = gs->param_enabled[i] ? 1u : 0u;
    }
    out->param_mode = gs->param_mode ? 1u : 0u;
    out->t_min  = gs->t_min;
    out->t_max  = gs->t_max;
    out->t_step = gs->t_step;

    /* STAT data list */
    memcpy(out->stat_list_x, stat_data.list_x,
           STAT_MAX_POINTS * sizeof(float));
    memcpy(out->stat_list_y, stat_data.list_y,
           STAT_MAX_POINTS * sizeof(float));
    out->stat_list_len = stat_data.list_len;
}

/**
 * @brief  Restore calculator state from a previously loaded block.
 *
 * Re-derives calc_decimal_mode and angle_degrees from s_mode.committed via
 * their existing setters so behaviour is consistent with ENTER on MODE.
 */
void Calc_ApplyPersistBlock(const PersistBlock_t *in)
{
    memcpy(calc_variables, in->calc_variables, sizeof(calc_variables));
    ans = in->ans;
    memcpy(s_mode.committed, in->mode_committed, sizeof(s_mode.committed));

    /* Re-derives state that is computed from s_mode.committed */
    angle_degrees = (in->mode_committed[2] == 1);
    Calc_SetDecimalMode(in->mode_committed[1]);

    graph_ui_set_zoom_facts(in->zoom_x_fact, in->zoom_y_fact);

    /* Restore graph state — leave active = false */
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(Graph_GetEquationBuf((uint8_t)i), in->equations[i], GRAPH_EQUATION_BUF_LEN);
    }
    Graph_SetWindow(in->x_min, in->x_max, in->y_min, in->y_max,
                    in->x_scl, in->y_scl, in->x_res);
    Graph_SetGridOn(in->grid_on != 0);
    for (int i = 0; i < 4; i++) Graph_SetEquationEnabled((uint8_t)i, (in->enabled[i] != 0));

    /* Restore matrices [A], [B], [C] — dimensions and 6×6 data */
    for (int m = 0; m < 3; m++) {
        uint8_t rows = in->matrix_rows[m];
        uint8_t cols = in->matrix_cols[m];
        /* Clamp to valid range in case of corrupt data */
        calc_matrices[m].rows = (rows >= 1 && rows <= CALC_MATRIX_MAX_DIM) ? rows : 3;
        calc_matrices[m].cols = (cols >= 1 && cols <= CALC_MATRIX_MAX_DIM) ? cols : 3;
        memcpy(calc_matrices[m].data, in->matrix_data[m],
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }

    /* Restore parametric equations and T range */
    for (int i = 0; i < GRAPH_NUM_PARAM; i++) {
        char *px = Graph_GetParamEquationXBuf((uint8_t)i);
        memcpy(px, in->param_x[i], GRAPH_EQUATION_BUF_LEN);
        px[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
        char *py = Graph_GetParamEquationYBuf((uint8_t)i);
        memcpy(py, in->param_y[i], GRAPH_EQUATION_BUF_LEN);
        py[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
        Graph_SetParamEnabled((uint8_t)i, (in->param_enabled[i] != 0));
    }
    Graph_SetParamMode(in->param_mode != 0);
    Graph_SetParamWindow(in->t_min, in->t_max,
                         (in->t_step > 0.0f) ? in->t_step : 0.1309f);
    /* Re-sync MODE screen cursor for row 4 (Function|Param) */
    s_mode.cursor[4]    = (in->param_mode != 0) ? 1 : 0;
    s_mode.committed[4] = s_mode.cursor[4];

    /* Restore STAT data list */
    memcpy(stat_data.list_x, in->stat_list_x,
           STAT_MAX_POINTS * sizeof(float));
    memcpy(stat_data.list_y, in->stat_list_y,
           STAT_MAX_POINTS * sizeof(float));
    stat_data.list_len = (in->stat_list_len <= STAT_MAX_POINTS)
                         ? in->stat_list_len : 0u;
}

/*---------------------------------------------------------------------------
 * LVGL thread safety helpers
 *---------------------------------------------------------------------------*/

void lvgl_lock(void) {
#ifndef HOST_TEST
    if (xLVGL_Mutex != NULL)
        xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY);
#endif
}

void lvgl_unlock(void) {
#ifndef HOST_TEST
    if (xLVGL_Mutex != NULL)
        xSemaphoreGive(xLVGL_Mutex);
#endif
}

/*---------------------------------------------------------------------------
 * UI initialisation
 *---------------------------------------------------------------------------*/

/* Initialises the three shared LVGL styles used across the calculator UI. */
static void ui_init_styles(void)
{
    /* Background */
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(COLOR_BLACK));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_pad_all(&style_bg, 0);
}

/* Creates a block cursor box (14×26 px) with an inner label child.
 * All cursor objects across every screen are built through this single function.
 * Change the size, font, or style properties here and all cursors update at once. */
void cursor_box_create(lv_obj_t *parent, bool start_hidden,
                               lv_obj_t **out_box, lv_obj_t **out_inner)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 14, 26);
    lv_obj_set_style_bg_color(box, lv_color_hex(COLOR_GREY_LIGHT), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *inner = lv_label_create(box);
    lv_obj_set_style_text_font(inner, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(inner, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_center(inner);
    lv_label_set_text(inner, "");

    if (start_hidden)
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);

    *out_box   = box;
    *out_inner = inner;
}

/* Creates the main calculator screen: history rows, angle label, and cursor. */
static void ui_init_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &style_bg, 0);
    lv_obj_set_size(scr, DISPLAY_W, DISPLAY_H);

    /* DISP_ROW_COUNT full-width text rows — Montserrat 24, DISP_ROW_H px each */
    for (int i = 0; i < DISP_ROW_COUNT; i++) {
        disp_rows[i] = lv_label_create(scr);
        lv_obj_set_pos(disp_rows[i], 4, i * DISP_ROW_H + 2);
        lv_obj_set_width(disp_rows[i], DISPLAY_W - 8);
        lv_obj_set_style_text_font(disp_rows[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(disp_rows[i],
                                    lv_color_hex(COLOR_GREY_MED), 0);
        lv_label_set_long_mode(disp_rows[i], LV_LABEL_LONG_CLIP);
        lv_label_set_text(disp_rows[i], "");
    }

    /* Cursor block — filled rectangle that overlays the insertion point. */
    cursor_box_create(scr, false, &cursor_box, &cursor_inner);

    /* Measure how many monospaced characters fit on one display row. */
    uint16_t glyph_w = lv_font_get_glyph_width(&jetbrains_mono_24, 'X', 0);
    if (glyph_w > 0)
        expr_chars_per_row = (uint8_t)((DISPLAY_W - 8) / glyph_w);
}

/*---------------------------------------------------------------------------
 * Graph screen initialisation
 *---------------------------------------------------------------------------*/

/* Creates a full-screen opaque black LVGL panel, hidden by default.
 * Used as the base for all overlay screens (MODE, MATH, TEST, MATRIX). */
lv_obj_t *screen_create(lv_obj_t *parent)
{
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_set_size(scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);
    return scr;
}

/* ui_init_math_screen() and ui_init_test_screen() moved to ui_math_menu.c. */

/*---------------------------------------------------------------------------
 * UI update functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Generic block-cursor placement.
 *
 * All cursor appearance logic — visibility, color, inner character, and
 * insert/overwrite shape — is driven by explicit parameters rather than
 * module-level globals.
 *
 * Cursor inner-char key:
 *   MODE_STO        → green 'A'  (STO-pending synthetic mode)
 *   MODE_2ND        → amber '^'
 *   MODE_ALPHA/LOCK → green 'A'
 *   insert=true     → grey underline style  (overwrite = default: blank grey block)
 *
 * @param box          The cursor rectangle LVGL object to move/show/hide.
 * @param inner        The label child of box that shows the inner character.
 * @param parent_label The LVGL label whose text provides the reference position.
 * @param glyph_pos    Glyph index within parent_label at which to place the cursor.
 * @param visible      Whether the cursor is currently in the visible blink phase.
 * @param mode         Calculator mode driving cursor color/inner-char appearance.
 *                     Pass MODE_STO (synthesised from sto_pending) for STO-pending state.
 * @param insert       True when insert mode is active (renders underscore-style cursor).
 */
void cursor_render(lv_obj_t *box, lv_obj_t *inner,
                   lv_obj_t *parent_label, uint32_t glyph_pos,
                   bool visible, CalcMode_t mode, bool insert)
{
    if (box == NULL) return;

    lv_color_t box_color;
    const char *inner_text;

    switch (mode) {
        case MODE_STO:
            box_color  = lv_color_hex(COLOR_ALPHA);
            inner_text = "A";
            break;
        case MODE_2ND:
            box_color  = lv_color_hex(COLOR_2ND);
            inner_text = "^";
            break;
        case MODE_ALPHA:
        case MODE_ALPHA_LOCK:
            box_color  = lv_color_hex(COLOR_ALPHA);
            inner_text = "A";
            break;
        default:
            box_color  = lv_color_hex(COLOR_GREY_LIGHT);
            inner_text = "";  /* insert mode shown by underscore shape, not letter */
            break;
    }

    if (!visible) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_point_t pos;
    lv_label_get_letter_pos(parent_label, glyph_pos, &pos);

    int32_t lx = lv_obj_get_x(parent_label);
    int32_t ly = lv_obj_get_y(parent_label);

    /* Insert mode: underscore-style cursor (3 px at character baseline).
     * Overwrite mode: full-height block cursor (26 px).
     * In insert mode the box is 3 px tall; the inner label (^/A) overflows
     * above the underline via LV_OBJ_FLAG_OVERFLOW_VISIBLE, giving a combined
     * "underline + mode indicator" visual for insert+2ND or insert+ALPHA. */
    bool in_insert = insert && (mode != MODE_STO);
    lv_obj_set_height(box, in_insert ? 3 : 26);
    lv_obj_set_pos(box, lx + pos.x, ly + pos.y + (in_insert ? 23 : 0));

    lv_obj_set_style_bg_color(box, box_color, 0);
    lv_label_set_text(inner, inner_text);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
}


/**
 * @brief Positions the main calculator screen block cursor.
 *
 * @param row_label  The LVGL label containing the current expression.
 * @param char_pos   Character index at which to place the cursor.
 */
static void cursor_update(lv_obj_t *row_label, uint32_t char_pos)
{
    CalcMode_t display_mode = sto_pending ? MODE_STO : current_mode;
    cursor_render(cursor_box, cursor_inner, row_label, char_pos,
                  cursor_visible, display_mode, insert_mode);
}

/**
 * @brief Redraws all DISP_ROW_COUNT display rows from the history buffer
 *        and the current input expression.
 *
 * Each history entry occupies ceil(expr_len/cpr) expression sub-rows + 1 result
 * row, so long committed expressions wrap just as the active expression does.
 * Current expression sub-rows follow immediately after all history rows.
 */
/**
 * @brief Format a CalcResult_t into a displayable string.
 *
 * For scalar results: delegates to Calc_FormatResult.
 * For matrix results: produces a newline-separated grid "[a b c]\n[d e f]\n[g h i]".
 * Updates *ans_out only on successful scalar results.
 */
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size)
{
    memset(buf, 0, (size_t)buf_size);
    if (r->error != CALC_OK) {
        strncpy(buf, r->error_msg, (size_t)(buf_size - 1));
        buf[buf_size - 1] = '\0';
        return;
    }
    if (r->has_matrix) {
        const CalcMatrix_t *m = &calc_matrices[r->matrix_idx];
        int pos = 0;
        for (int row = 0; row < (int)m->rows && pos < buf_size - 2; row++) {
            if (row > 0 && pos < buf_size - 1) buf[pos++] = '\n';
            if (pos < buf_size - 1) buf[pos++] = '[';
            for (int col = 0; col < (int)m->cols; col++) {
                char cell[12];
                Calc_FormatResult(m->data[row][col], cell, sizeof(cell));
                /* Limit cell width to 8 chars to keep lines short */
                cell[8] = '\0';
                if (col > 0 && pos < buf_size - 1) buf[pos++] = ' ';
                int cl = (int)strlen(cell);
                if (pos + cl < buf_size - 1) {
                    memcpy(&buf[pos], cell, (size_t)cl);
                    pos += cl;
                }
            }
            if (pos < buf_size - 1) buf[pos++] = ']';
        }
        buf[pos] = '\0';
        ans_is_matrix = true;
        ans = (float)r->matrix_idx;
    } else {
        Calc_FormatResult(r->value, buf, (uint8_t)buf_size);
        ans_is_matrix = false;
        ans = r->value;
    }
}

/** Returns the number of display lines a result string occupies (newline-separated). */
static int count_result_lines(const char *result)
{
    int n = 1;
    for (; *result; result++)
        if (*result == '\n') n++;
    return n;
}

/**
 * @brief Copy line @p line_idx (0-based) from a newline-separated string into buf.
 *
 * Returns false if line_idx exceeds the number of lines in src.
 */
static bool get_result_line(const char *src, int line_idx, char *buf, int buf_size)
{
    const char *p = src;
    for (int k = 0; k < line_idx; k++) {
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        else return false;  /* line_idx out of range */
    }
    int len = 0;
    while (p[len] && p[len] != '\n' && len < buf_size - 1) len++;
    memcpy(buf, p, (size_t)len);
    buf[len] = '\0';
    return true;
}

/* Compute the maximum formatted-cell width for each column of matrix m. */
static void matrix_col_widths(const CalcMatrix_t *m,
                               uint8_t widths[CALC_MATRIX_MAX_DIM])
{
    for (int c = 0; c < CALC_MATRIX_MAX_DIM; c++) widths[c] = 0;
    for (int r = 0; r < (int)m->rows; r++) {
        for (int c = 0; c < (int)m->cols; c++) {
            char cell[12];
            Calc_FormatResult(m->data[r][c], cell, sizeof(cell));
            cell[8] = '\0';
            uint8_t len = (uint8_t)strlen(cell);
            if (len > widths[c]) widths[c] = len;
        }
    }
}

/* Total characters needed for one formatted row: [ col0 col1 … coln ] */
static int matrix_row_total_width(const CalcMatrix_t *m)
{
    uint8_t widths[CALC_MATRIX_MAX_DIM];
    matrix_col_widths(m, widths);
    int total = 2; /* [ and ] */
    for (int c = 0; c < (int)m->cols; c++) {
        if (c > 0) total++; /* space separator */
        total += widths[c];
    }
    return total;
}

/*
 * Build a display string for one row of matrix m with horizontal scroll applied.
 *
 * display_cols: number of character columns visible (typically expr_chars_per_row).
 * Shows '>' at the right edge when more content is to the right.
 * Shows '<' at the left edge when scrolled past content.
 * buf must be at least display_cols + 2 bytes.
 */
static void matrix_format_row(const CalcMatrix_t *m, int row_idx,
                               int scroll_offset, int display_cols,
                               char *buf, int buf_size)
{
    uint8_t widths[CALC_MATRIX_MAX_DIM];
    matrix_col_widths(m, widths);

    /* Build the full unclipped row string */
    char full[80];
    int pos = 0;
    full[pos++] = '[';
    for (int c = 0; c < (int)m->cols && pos < (int)sizeof(full) - 2; c++) {
        if (c > 0) full[pos++] = ' ';
        char cell[12];
        Calc_FormatResult(m->data[row_idx][c], cell, sizeof(cell));
        cell[8] = '\0';
        int cl = (int)strlen(cell);
        int cw = (int)widths[c];
        /* Right-pad cell to column width */
        for (int p = 0; p < cw && pos < (int)sizeof(full) - 1; p++)
            full[pos++] = (p < cl) ? cell[p] : ' ';
    }
    if (pos < (int)sizeof(full) - 1) full[pos++] = ']';
    full[pos] = '\0';
    int full_len = pos;

    bool clip_left  = (scroll_offset > 0);
    bool clip_right = (scroll_offset + display_cols < full_len);

    /* Source range: shrink by 1 char on each clipped side for the indicator */
    int src_start = scroll_offset + (clip_left  ? 1 : 0);
    int src_end   = scroll_offset + display_cols - (clip_right ? 1 : 0);
    if (src_end > full_len) src_end = full_len;

    int out = 0;
    if (clip_left  && out < buf_size - 1) buf[out++] = '<';
    for (int src = src_start; src < src_end && out < buf_size - 2; src++)
        buf[out++] = full[src];
    if (clip_right && out < buf_size - 1) buf[out++] = '>';
    buf[out] = '\0';
}

/** Returns the CalcMatrix_t for history entry @p e, or NULL if evicted from the ring. */
static const CalcMatrix_t *history_get_matrix(const HistoryEntry_t *e)
{
    if (!e->has_matrix) return NULL;
    uint8_t slot = e->matrix_ring_idx;
    if (matrix_ring_gen_table[slot] != e->matrix_ring_gen) return NULL;
    return &matrix_ring[slot];
}

/**
 * @brief Render one history result row onto @p label.
 *
 * Sets the label colour to COLOR_WHITE, then either formats a matrix row
 * (column-aligned, left-aligned, with horizontal scroll applied) or a scalar
 * result line (right-aligned), and sets the label text.
 */
static void render_result_row(lv_obj_t *label, const HistoryEntry_t *entry,
                               int result_line)
{
    char rbuf[MAX_RESULT_LEN];
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_WHITE), 0);
    if (entry->has_matrix) {
        const CalcMatrix_t *m = history_get_matrix(entry);
        if (m != NULL) {
            int off = (matrix_scroll_focus == (int8_t)(entry - history))
                      ? (int)matrix_scroll_offset : 0;
            matrix_format_row(m, result_line,
                              off, (int)expr_chars_per_row, rbuf, sizeof(rbuf));
        } else {
            /* Matrix evicted from ring — fall back to pre-formatted result string */
            get_result_line(entry->result, result_line, rbuf, sizeof(rbuf));
        }
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    } else {
        get_result_line(entry->result, result_line, rbuf, sizeof(rbuf));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_label_set_text(label, rbuf);
}

void ui_refresh_display(void)
{
    if (disp_rows[0] == NULL) return;

    int cpr = (int)expr_chars_per_row;
    int expr_rows = (expr.len == 0) ? 1 : (expr.len + cpr - 1) / cpr;

    /* Number of history entries visible (circular buffer cap) */
    int num_entries = ((int)history_count < HISTORY_LINE_COUNT)
                      ? (int)history_count : HISTORY_LINE_COUNT;

    /* Total display lines consumed by history (variable per entry).
       Matrix results occupy multiple result lines (one per matrix row). */
    int total_history_lines = 0;
    for (int d = 0; d < num_entries; d++) {
        int idx = (int)((history_count - num_entries + d) % HISTORY_LINE_COUNT);
        int elen = (int)strlen(history[idx].expression);
        int erows = (elen + cpr - 1) / cpr;
        int rlines = history[idx].has_matrix
                     ? (int)history[idx].matrix_rows_cache
                     : count_result_lines(history[idx].result);
        total_history_lines += erows + rlines;
    }

    int total = total_history_lines + expr_rows;
    int start = (total > DISP_ROW_COUNT) ? (total - DISP_ROW_COUNT) : 0;

    /* Which expression sub-row holds the cursor, and column within that row */
    int cursor_expr_row = (int)expr.cursor / cpr;
    int cursor_col      = (int)expr.cursor % cpr;

    for (int row = 0; row < DISP_ROW_COUNT; row++) {
        int li = start + row;

        if (li >= total) {
            lv_label_set_text(disp_rows[row], "");
            continue;
        }

        if (li < total_history_lines) {
            /* Walk history entries to find which owns line li */
            int line = 0;
            for (int d = 0; d < num_entries; d++) {
                int idx = (int)((history_count - num_entries + d) % HISTORY_LINE_COUNT);
                int elen  = (int)strlen(history[idx].expression);
                int erows = (elen + cpr - 1) / cpr;
                int rlines = history[idx].has_matrix
                             ? (int)history[idx].matrix_rows_cache
                             : count_result_lines(history[idx].result);

                if (li < line + erows) {
                    /* Expression sub-row */
                    int er = li - line;
                    int char_start = er * cpr;
                    int char_end = char_start + cpr;
                    if (char_end > elen) char_end = elen;
                    int seg_len = char_end - char_start;
                    if (seg_len < 0) seg_len = 0;
                    char row_buf[MAX_EXPR_LEN + 1];
                    memcpy(row_buf, history[idx].expression + char_start, (size_t)seg_len);
                    row_buf[seg_len] = '\0';
                    lv_obj_set_style_text_color(disp_rows[row],
                                                lv_color_hex(COLOR_GREY_MED), 0);
                    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
                    lv_label_set_text(disp_rows[row], row_buf);
                    break;
                }
                line += erows;

                if (li < line + rlines) {
                    /* Result line for this history entry */
                    int result_line = li - line;
                    render_result_row(disp_rows[row], &history[idx], result_line);
                    break;
                }
                line += rlines;
            }
        } else {
            /* Current expression sub-row */
            int expr_line  = li - total_history_lines;
            int char_start = expr_line * cpr;
            int char_end   = char_start + cpr;
            if (char_end > (int)expr.len) char_end = (int)expr.len;
            int seg_len = char_end - char_start;
            if (seg_len < 0) seg_len = 0;

            char row_buf[MAX_EXPR_LEN + 1];
            memcpy(row_buf, &expr.buf[char_start], (size_t)seg_len);
            row_buf[seg_len] = '\0';

            lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_GREY_LIGHT), 0);
            lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(disp_rows[row], row_buf);

            /* Place cursor on the sub-row that contains cursor_pos */
            if (expr_line == cursor_expr_row)
                cursor_update(disp_rows[row], (uint32_t)cursor_col);
        }
    }

    /* If the cursor's sub-row scrolled off-screen, hide the cursor */
    if (total_history_lines + cursor_expr_row < start && cursor_box != NULL)
        lv_obj_add_flag(cursor_box, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief LVGL timer callback — blinks the cursor every CURSOR_BLINK_MS.
 *
 * Called from lv_task_handler() — DefaultTask already holds the LVGL mutex.
 * Do NOT call lvgl_lock() here or it will deadlock.
 *
 * Updates the main screen cursor and, when an overlay input screen is active,
 * also blinks the corresponding overlay cursor box.
 */
static void cursor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    cursor_visible = !cursor_visible;
    ui_refresh_display();
    /* Blink the overlay-screen cursor based on visibility, not current_mode,
     * so it keeps blinking during transient modifier modes (MODE_2ND/ALPHA). */
    if (Graph_IsYeqScreenVisible())
        yeq_cursor_update();
    else if (Graph_IsRangeScreenVisible())
        range_cursor_update();
    else if (Graph_IsZoomFactorsScreenVisible())
        zoom_factors_cursor_update();
    else if (Matrix_IsEditScreenVisible())
        matrix_edit_cursor_update();
    else if (ui_prgm_editor_screen != NULL && !lv_obj_has_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_editor_cursor_update();
    else if (ui_prgm_new_screen != NULL && !lv_obj_has_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_new_cursor_update();
}

/**
 * @brief Refreshes the cursor on all screens to reflect the current mode.
 *        The cursor block color and inner character encode 2nd/ALPHA/STO state.
 *        Checks screen visibility directly because current_mode may already be
 *        MODE_2ND/MODE_ALPHA when called from Process_Hardware_Key.
 */
void ui_update_status_bar(void)
{
    cursor_visible = true;
    ui_refresh_display();
    if (Graph_IsYeqScreenVisible())
        yeq_cursor_update();
    else if (Graph_IsRangeScreenVisible())
        range_cursor_update();
    else if (Graph_IsZoomFactorsScreenVisible())
        zoom_factors_cursor_update();
    else if (Matrix_IsEditScreenVisible())
        matrix_edit_cursor_update();
    else if (ui_prgm_editor_screen != NULL && !lv_obj_has_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_editor_cursor_update();
    else if (ui_prgm_new_screen != NULL && !lv_obj_has_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_new_cursor_update();
}

/**
 * @brief Refreshes the display with the current expression and cursor.
 *        Called after every keypress that modifies the expression.
 */
void Update_Calculator_Display(void)
{
    if (disp_rows[0] == NULL) return;
    lvgl_lock();
    ui_refresh_display();
    lvgl_unlock();
}

/* expr_prepend_ans_if_empty, expr_insert_char, expr_insert_str,
 * expr_delete_at_cursor moved to ui_input.c; declared in ui_input.h. */

/**
 * @brief Refreshes the display after history changes.
 */
void ui_update_history(void)
{
    ui_refresh_display();
}

/** Write @p text directly to display row @p row_1based (1–8) without
 *  touching the history ring buffer.  Must be called under lvgl_lock(). */
void ui_output_row(uint8_t row_1based, const char *text)
{
    if (row_1based < 1 || row_1based > DISP_ROW_COUNT) return;
    if (disp_rows[0] == NULL) return;
    uint8_t row = row_1based - 1;
    lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(disp_rows[row], text);
}

/* ui_update_math_display(), ui_update_test_display(), math_menu_insert(),
 * test_menu_insert(), menu_handle_nav_keys(), handle_math_menu(), and
 * handle_test_menu() moved to ui_math_menu.c. */

/*---------------------------------------------------------------------------
 * Token execution
 *---------------------------------------------------------------------------*/

/**
 * @brief Generic cross-module helper that takes a menu item and inserts it
 *        into either the Y= editor or the normal calculator context, then
 *        returns context via pointers.
 */
void menu_insert_text(const char *ins, CalcMode_t *ret_mode)
{
    if (*ret_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        graph_ui_yeq_insert(ins);
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    *ret_mode = MODE_NORMAL;
}

/*---------------------------------------------------------------------------
 * Navigation helper functions
 *---------------------------------------------------------------------------*/

/* Hides every graph editor, menu overlay, and the graph canvas.
 * Must be called inside lvgl_lock(). */
void hide_all_screens(void)
{
    Graph_HideYeqScreen();
    ParamYeq_HideScreen();
    Graph_HideRangeScreen();
    Zoom_HideScreen();
    Graph_HideZoomFactorsScreen();
    Mode_HideScreen();
    Math_HideScreen();
    Test_HideScreen();
    Matrix_HideMenuScreen();
    Matrix_HideEditScreen();
    Stat_HideMenuScreen();
    Stat_HideEditScreen();
    Stat_HideResultsScreen();
    Draw_HideScreen();
    Vars_HideScreen();
    Yvars_HideScreen();
    hide_prgm_screens();
    Graph_SetVisible(false);
}

/* Opens a menu (MATH, TEST, or MATRIX) from any screen.
 * return_to: the mode to restore when the menu is closed.
 * Hides all screens first so no overlay leaks through. */
void menu_open(Token_t menu_token, CalcMode_t return_to)
{
    lvgl_lock();
    hide_all_screens();
    switch (menu_token) {
    case TOKEN_MATH:
        math_menu_open(return_to);
        break;
    case TOKEN_TEST:
        test_menu_open(return_to);
        break;
    case TOKEN_MATRX:
        matrix_menu_state.return_mode = return_to;
        matrix_menu_state.tab         = 0;
        matrix_menu_state.item_cursor = 0;
        current_mode       = MODE_MATRIX_MENU;
        Matrix_ShowMenuScreen();
        ui_update_matrix_display();
        break;
    case TOKEN_PRGM:
        prgm_menu_open(return_to);
        break;
    case TOKEN_STAT:
        stat_menu_state.return_mode  = return_to;
        stat_menu_state.tab          = 0;
        stat_menu_state.item_cursor  = 0;
        current_mode = MODE_STAT_MENU;
        Stat_ShowMenuScreen();
        ui_update_stat_display();
        break;
    case TOKEN_DRAW:
        draw_menu_state.return_mode  = return_to;
        draw_menu_state.item_cursor  = 0;
        current_mode = MODE_DRAW_MENU;
        Draw_ShowScreen();
        ui_update_draw_display();
        break;
    case TOKEN_VARS:
        vars_menu_state.return_mode = return_to;
        vars_menu_state.tab         = 0;
        vars_menu_state.cursor      = 0;
        vars_menu_state.scroll      = 0;
        current_mode = MODE_VARS_MENU;
        Vars_ShowScreen();
        ui_update_vars_display();
        break;
    case TOKEN_Y_VARS:
        yvars_menu_state.return_mode = return_to;
        yvars_menu_state.tab         = 0;
        yvars_menu_state.item_cursor = 0;
        current_mode = MODE_YVARS_MENU;
        Yvars_ShowScreen();
        ui_update_yvars_display();
        break;
    default:
        break;
    }
    lvgl_unlock();
}

/* Closes a menu and restores the calling screen.
 * Returns the restored CalcMode_t (MODE_NORMAL or MODE_GRAPH_YEQ).
 * Does NOT fall through; callers decide whether to return or break. */
CalcMode_t menu_close(Token_t menu_token)
{
    CalcMode_t ret;
    switch (menu_token) {
    case TOKEN_MATH:
        ret = math_menu_close();
        break;
    case TOKEN_TEST:
        ret = test_menu_close();
        break;
    case TOKEN_MATRX:
        ret                           = matrix_menu_state.return_mode;
        matrix_menu_state.return_mode = MODE_NORMAL;
        matrix_menu_state.tab         = 0;
        matrix_menu_state.item_cursor = 0;
        break;
    case TOKEN_PRGM:
        ret = prgm_menu_close();
        break;
    case TOKEN_STAT:
        ret                           = stat_menu_state.return_mode;
        stat_menu_state.return_mode   = MODE_NORMAL;
        stat_menu_state.tab           = 0;
        stat_menu_state.item_cursor   = 0;
        break;
    case TOKEN_DRAW:
        ret                          = draw_menu_state.return_mode;
        draw_menu_state.return_mode  = MODE_NORMAL;
        draw_menu_state.item_cursor  = 0;
        break;
    case TOKEN_VARS:
        ret                         = vars_menu_state.return_mode;
        vars_menu_state.return_mode = MODE_NORMAL;
        vars_menu_state.tab         = 0;
        vars_menu_state.cursor      = 0;
        vars_menu_state.scroll      = 0;
        break;
    case TOKEN_Y_VARS:
        ret                              = yvars_menu_state.return_mode;
        yvars_menu_state.return_mode     = MODE_NORMAL;
        yvars_menu_state.tab             = 0;
        yvars_menu_state.item_cursor     = 0;
        break;
    default:
        ret = MODE_NORMAL;
        break;
    }
    current_mode = ret;
    lvgl_lock();
    Math_HideScreen();
    Test_HideScreen();
    Matrix_HideMenuScreen();
    Stat_HideMenuScreen();
    Stat_HideEditScreen();
    Stat_HideResultsScreen();
    Draw_HideScreen();
    Vars_HideScreen();
    Yvars_HideScreen();
    hide_prgm_screens();
    if (ret == MODE_GRAPH_YEQ)
        Graph_ShowYeqScreen();
    lvgl_unlock();
    return ret;
}

/* Moves the active tab in a multi-tab menu left or right.
 * Resets item cursor and scroll offset on tab change. */
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll,
                     uint8_t tab_count, bool left, void (*update)(void))
{
    if (left) {
        if (*tab > 0) { (*tab)--; *cursor = 0; if (scroll) *scroll = 0; }
    } else {
        if (*tab < tab_count - 1) { (*tab)++; *cursor = 0; if (scroll) *scroll = 0; }
    }
    lvgl_lock(); update(); lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Per-mode token handlers (non-graph)
 * Each returns true if the token was fully handled (Execute_Token should
 * return), false if execution should fall through to the next handler.
 *---------------------------------------------------------------------------*/

/* handle_mode_screen() moved to ui_mode.c; declared in ui_mode.h.
 * handle_math_menu(), handle_test_menu(), menu_handle_nav_keys() moved to ui_math_menu.c.
 * handle_sto_pending, handle_digit_key, handle_arithmetic_op moved to ui_input.c. */

/**
 * @brief Write a completed evaluation into the next history slot and refresh the display.
 *
 * Stores @p expr and @p result_str into history[history_count % HISTORY_LINE_COUNT],
 * copies matrix data when @p r->has_matrix is set, advances history_count, and
 * triggers a UI update under the LVGL mutex.
 *
 * The caller is responsible for any expression-buffer reset and history_recall_offset
 * update that should happen after the commit.
 */
static void commit_history_entry(const char *expr, const char *result_str,
                                 const CalcResult_t *r)
{
    uint8_t idx = history_count % HISTORY_LINE_COUNT;
    if (history[idx].expression != expr) {
        strncpy(history[idx].expression, expr, MAX_EXPR_LEN - 1);
        history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
    }
    strncpy(history[idx].result, result_str, MAX_RESULT_LEN - 1);
    history[idx].result[MAX_RESULT_LEN - 1] = '\0';
    history[idx].has_matrix = false;
    if (r->has_matrix) {
        uint8_t slot = (uint8_t)(matrix_ring_write_count % MATRIX_RING_COUNT);
        matrix_ring[slot]           = calc_matrices[r->matrix_idx];
        matrix_ring_gen_table[slot] = matrix_ring_write_count;
        history[idx].has_matrix       = true;
        history[idx].matrix_ring_idx  = slot;
        history[idx].matrix_ring_gen  = matrix_ring_write_count;
        history[idx].matrix_rows_cache = calc_matrices[r->matrix_idx].rows;
        matrix_ring_write_count++;
        matrix_scroll_focus  = (int8_t)idx;
        matrix_scroll_offset = 0;
    } else {
        matrix_scroll_focus  = -1;
        matrix_scroll_offset = 0;
    }
    history_count++;
    lvgl_lock();
    ui_update_history();
    lvgl_unlock();
}

/**
 * @brief Resets the matrix history scroll state (called from ui_input.c after STO).
 */
void reset_matrix_scroll_focus(void)
{
    matrix_scroll_focus  = -1;
    matrix_scroll_offset = 0;
}

/* Load a history entry at the given scroll offset into the expression buffer. */
static void history_load_offset(uint8_t offset)
{
    uint8_t idx = (history_count - offset) % HISTORY_LINE_COUNT;
    strncpy(expr.buf, history[idx].expression, MAX_EXPR_LEN - 1);
    expr.buf[MAX_EXPR_LEN - 1] = '\0';
    expr.len    = (uint8_t)strlen(expr.buf);
    expr.cursor = expr.len;
    Update_Calculator_Display();
}

/* eval_draw_arg(), parse_draw_args(), try_execute_draw_command() moved to ui_draw.c.
 * try_execute_draw_command() is declared in ui_draw.h (already included above). */

/* Evaluate (or run) the current expression on TOKEN_ENTER.
 * Called only when expr_len > 0. */
static void history_enter_evaluate(void)
{
    /* prgmNAME expression: insert into history and run the program */
    if (strncmp(expr.buf, "prgm", 4) == 0) {
        int8_t slot = prgm_lookup_slot(expr.buf + 4);
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, expr.buf, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        history[hidx].result[0] = '\0';
        history_count++;
        ExprBuffer_Clear(&expr);
        history_recall_offset = 0;
        Update_Calculator_Display();
        if (slot >= 0)
            prgm_run_start((uint8_t)slot);
        return;
    }
#ifndef HOST_TEST
    /* DRAW commands execute as statements — display "Done", skip Calc_Evaluate */
    if (try_execute_draw_command()) {
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, expr.buf, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        strncpy(history[hidx].result, "Done", MAX_RESULT_LEN - 1);
        history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
        history[hidx].has_matrix = false;
        history_count++;
        ExprBuffer_Clear(&expr);
        history_recall_offset = 0;
        lvgl_lock();
        ui_update_history();
        lvgl_unlock();
        Update_Calculator_Display();
        return;
    }
#endif /* HOST_TEST */
    CalcResult_t result = Calc_Evaluate(expr.buf, ans, ans_is_matrix, angle_degrees);
    char result_str[MAX_RESULT_LEN];
    format_calc_result(&result, result_str, MAX_RESULT_LEN);
    commit_history_entry(expr.buf, result_str, &result);
    ExprBuffer_Clear(&expr);
    history_recall_offset = 0;
}

void handle_history_nav(Token_t t)
{
    switch (t) {

    case TOKEN_LEFT:
        if (expr.len == 0 && matrix_scroll_focus >= 0 &&
            history_get_matrix(&history[matrix_scroll_focus]) != NULL) {
            if (matrix_scroll_offset > 0) {
                matrix_scroll_offset--;
                Update_Calculator_Display();
            }
        } else if (expr.cursor > 0) {
            ExprBuffer_Left(&expr);
            Update_Calculator_Display();
        }
        break;

    case TOKEN_RIGHT:
        if (expr.len == 0 && matrix_scroll_focus >= 0) {
            const CalcMatrix_t *m = history_get_matrix(&history[matrix_scroll_focus]);
            if (m != NULL) {
                int total_w = matrix_row_total_width(m);
                int max_off = (total_w > (int)expr_chars_per_row)
                              ? (total_w - (int)expr_chars_per_row) : 0;
                if ((int)matrix_scroll_offset < max_off) {
                    matrix_scroll_offset++;
                    Update_Calculator_Display();
                }
            }
        } else if (expr.cursor < expr.len) {
            ExprBuffer_Right(&expr);
            Update_Calculator_Display();
        }
        break;

    case TOKEN_UP:
        if ((expr.len == 0 || history_recall_offset > 0) &&
            history_recall_offset < history_count &&
            history_recall_offset < HISTORY_LINE_COUNT) {
            history_recall_offset++;
            history_load_offset(history_recall_offset);
        }
        break;

    case TOKEN_DOWN:
        if (history_recall_offset > 0) {
            history_recall_offset--;
            if (history_recall_offset == 0) {
                ExprBuffer_Clear(&expr);
                Update_Calculator_Display();
            } else {
                history_load_offset(history_recall_offset);
            }
        }
        break;

    case TOKEN_ENTER:
        if (expr.len == 0 && history_count > 0) {
            /* Re-evaluate the last history entry */
            uint8_t last_idx = (history_count - 1) % HISTORY_LINE_COUNT;
            CalcResult_t result = Calc_Evaluate(history[last_idx].expression,
                                                ans, ans_is_matrix, angle_degrees);
            char result_str[MAX_RESULT_LEN];
            format_calc_result(&result, result_str, MAX_RESULT_LEN);
            commit_history_entry(history[last_idx].expression, result_str, &result);
            history_recall_offset = 0;
        } else if (expr.len > 0) {
            history_enter_evaluate();
        }
        break;

    case TOKEN_ENTRY:
        if (history_count > 0) {
            history_recall_offset = 1;
            history_load_offset(1);
        }
        break;

    default: break;
    }
}

/* handle_function_insert, handle_clear_key, handle_sto_key,
 * handle_normal_graph_nav, handle_normal_mode moved to ui_input.c. */

/**
 * @brief Processes a single calculator token from the keypad queue.
 * @param t  Token to execute.
 */
void Execute_Token(Token_t t)
{
    /*--- TOKEN_ON: save state (plain ON) or save + sleep (2nd+ON) ----------*/
    if (t == TOKEN_ON) {
        bool power_down = (current_mode == MODE_2ND);

        lvgl_lock();
        lv_obj_t *saving_lbl = lv_label_create(lv_scr_act());
        lv_label_set_text(saving_lbl, "Saving...");
        lv_obj_set_style_text_color(saving_lbl, lv_color_hex(COLOR_AMBER), 0);
        lv_obj_align(saving_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
        lvgl_unlock();
        osDelay(20);

        PersistBlock_t block;
        Calc_BuildPersistBlock(&block);
        Persist_Save(&block);
        Prgm_Save();

        current_mode       = MODE_NORMAL;
        return_mode        = MODE_NORMAL;
        sto_pending        = false;
        prgm_reset_execution_state();
        lvgl_lock();
        lv_obj_del(saving_lbl);
        hide_all_screens();
        ui_update_status_bar();
        lvgl_unlock();

        if (power_down) {
            Power_DisplayBlankAndMessage();
        }
        return;
    }

    /*--- TOKEN_QUIT: hard exit to main calculator screen ------------------*/
    if (t == TOKEN_QUIT) {
        current_mode = MODE_NORMAL;
        return_mode  = MODE_NORMAL;
        sto_pending  = false;
        prgm_reset_execution_state();
        lvgl_lock();
        hide_all_screens();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }

    /*--- TOKEN_MODE: always opens MODE screen from any mode ----------------*/
    if (t == TOKEN_MODE) {
        ui_mode_open();
        return;
    }

    if (current_mode == MODE_PRGM_RUNNING)        { handle_prgm_running(t); return; }

    if (current_mode == MODE_GRAPH_YEQ)          { if (handle_yeq_mode(t))          return; }
    if (current_mode == MODE_GRAPH_RANGE)         { if (handle_range_mode(t))         return; }
    if (current_mode == MODE_GRAPH_ZOOM)          { if (handle_zoom_mode(t))          return; }
    if (current_mode == MODE_GRAPH_ZOOM_FACTORS)  { if (handle_zoom_factors_mode(t))  return; }
    if (current_mode == MODE_GRAPH_ZBOX)          { if (handle_zbox_mode(t))          return; }
    if (current_mode == MODE_GRAPH_TRACE)         { if (handle_trace_mode(t))         return; }
    if (current_mode == MODE_GRAPH_PARAM_YEQ)    { if (handle_param_yeq_mode(t))     return; }
    if (current_mode == MODE_MODE_SCREEN)         { if (handle_mode_screen(t))                        return; }
    if (current_mode == MODE_MATH_MENU)           { if (handle_math_menu(t))                          return; }
    if (current_mode == MODE_TEST_MENU)           { if (handle_test_menu(t))                          return; }
    if (current_mode == MODE_MATRIX_MENU)         { if (handle_matrix_menu(t, &matrix_menu_state))  return; }
    if (current_mode == MODE_MATRIX_EDIT)         { handle_matrix_edit(t); return; }
    if (current_mode == MODE_STAT_MENU)           { if (handle_stat_menu(t, &stat_menu_state))      return; }
    if (current_mode == MODE_STAT_EDIT)           { if (handle_stat_edit(t))           return; }
    if (current_mode == MODE_STAT_RESULTS)        { if (handle_stat_results(t))        return; }
    if (current_mode == MODE_DRAW_MENU)           { if (handle_draw_menu(t))           return; }
    if (current_mode == MODE_VARS_MENU)           { if (handle_vars_menu(t))           return; }
    if (current_mode == MODE_YVARS_MENU)          { if (handle_yvars_menu(t))          return; }
    if (current_mode == MODE_PRGM_MENU)           { if (handle_prgm_menu(t))          return; }
    /* F5b: ALPHA_LOCK in name-entry — same pattern as A6 editor fix below */
    if (current_mode == MODE_PRGM_NEW_NAME ||
        (current_mode == MODE_ALPHA_LOCK && return_mode == MODE_PRGM_NEW_NAME))
                                                  { if (handle_prgm_new_name(t))      return; }
    /* A6: ALPHA_LOCK in editor — current_mode stays MODE_ALPHA_LOCK; route by return_mode */
    if (current_mode == MODE_PRGM_EDITOR ||
        (current_mode == MODE_ALPHA_LOCK && return_mode == MODE_PRGM_EDITOR))
                                                  { if (handle_prgm_editor(t))        return; }
    if (current_mode == MODE_PRGM_CTL_MENU)       { if (handle_prgm_ctl_menu(t))      return; }
    if (current_mode == MODE_PRGM_IO_MENU)        { if (handle_prgm_io_menu(t))       return; }
    if (current_mode == MODE_PRGM_EXEC_MENU)      { if (handle_prgm_exec_menu(t))     return; }

    if (sto_pending) { if (handle_sto_pending(t)) return; }

    handle_normal_mode(t);
}

/*---------------------------------------------------------------------------
 * Keypad event handler
 *---------------------------------------------------------------------------*/

/**
 * @brief Translates a hardware key ID into a token and posts it to the queue.
 * @param key_id  Raw hardware key identifier from Keypad_Scan().
 */
void Process_Hardware_Key(uint8_t key_id)
{
    if (key_id == 0)
        return;

    if (key_id >= TI81_LookupTable_Size)
        return;

    KeyDefinition_t key        = TI81_LookupTable[key_id];
    Token_t         token_to_send = TOKEN_NONE;

    if (current_mode == MODE_2ND) {
        token_to_send  = key.second;
        current_mode   = return_mode;   /* restore the mode that was active before 2nd */
        return_mode    = MODE_NORMAL;
        if (token_to_send == TOKEN_NONE) {
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
    } else if (current_mode == MODE_ALPHA) {
        token_to_send  = key.alpha;
        current_mode   = return_mode;   /* restore the mode that was active before ALPHA */
        return_mode    = MODE_NORMAL;
        if (token_to_send == TOKEN_NONE) {
            /* A2: fall back to normal function (e.g. ENTER/DEL/CLEAR in name-entry) */
            token_to_send = key.normal;
            if (token_to_send == TOKEN_NONE) {
                lvgl_lock();
                ui_update_status_bar();
                lvgl_unlock();
                return;
            }
        }
    } else if (current_mode == MODE_ALPHA_LOCK) {
        if (key.normal == TOKEN_ALPHA) {
            /* ALPHA pressed while locked — exit alpha lock */
            current_mode = return_mode;
            return_mode  = MODE_NORMAL;
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
        token_to_send = key.alpha;  /* stay locked — do not restore mode */
    } else if (sto_pending) {
        /* STO implicitly uses the alpha layer for the destination key */
        token_to_send = key.alpha;
    } else {
        token_to_send  = key.normal;
    }

    /* Handle sticky modifier keys — pressing again cancels the mode */
    if (token_to_send == TOKEN_2ND) {
        if (current_mode == MODE_2ND) {
            current_mode = return_mode;
            return_mode  = MODE_NORMAL;
        } else {
            return_mode  = current_mode;
            current_mode = MODE_2ND;
        }
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_ALPHA) {
        if (current_mode == MODE_ALPHA) {
            current_mode = return_mode;
            return_mode  = MODE_NORMAL;
        } else {
            return_mode  = current_mode;
            current_mode = MODE_ALPHA;
        }
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_A_LOCK) {
        return_mode  = current_mode;
        current_mode = MODE_ALPHA_LOCK;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }

    if (token_to_send != TOKEN_NONE) {
        /* F1: on CLEAR, abort any running program immediately from keypadTask so
         * prgm_run_loop() (on CalcCoreTask) exits on its next iteration check. */
        if (token_to_send == TOKEN_CLEAR)
            prgm_request_abort();  /* no-op if not running */
        if (xQueueSend(keypadQueueHandle, &token_to_send, 0) != pdPASS) {
            /* Queue full — keypress dropped */
        }
    }
}

/*---------------------------------------------------------------------------
 * FreeRTOS task
 *---------------------------------------------------------------------------*/

/**
 * @brief Calculator core task.
 *        Waits for LVGL initialisation, creates the UI, then processes
 *        keypad tokens from the queue indefinitely.
 */
void StartCalcCoreTask(void const *argument)
{
    (void)argument;
    xSemaphoreTake(xLVGL_Ready, portMAX_DELAY);

    lvgl_lock();
    ui_init_styles();
    ui_init_screen();
    ui_init_graph_screens();
    ui_mode_init();
    ui_init_math_screen();
    ui_init_test_screen();
    ui_init_matrix_screen();
    ui_init_stat_screen();
    ui_init_stat_edit_screen();
    ui_init_stat_results_screen();
    ui_init_draw_screen();
    ui_init_vars_screen();
    ui_init_yvars_screen();
    ui_init_prgm_screens();
    cursor_timer = lv_timer_create(cursor_timer_cb, CURSOR_BLINK_MS, NULL);
    ui_update_zoom_display();   /* populate ZOOM labels with initial scroll=0 (defined in graph_ui.c) */
    ui_update_mode_display();
    ui_update_math_display();
    ui_update_test_display();
    ui_update_matrix_display();
    ui_update_matrix_edit_display();
    ui_update_stat_display();
    ui_update_vars_display();
    ui_update_yvars_display();
    Calc_RegisterYEquations(
        Graph_GetState()->equations,
        GRAPH_NUM_EQ);
    /* Load programs from FLASH sector 11 before populating the PRGM menu */
    Prgm_Init();

    ui_refresh_display();

    /* Restore saved state from FLASH sector 10 if present and valid */
    {
        PersistBlock_t saved;
        if (Persist_Load(&saved)) {
            Calc_ApplyPersistBlock(&saved);
            ui_refresh_display();   /* show loaded ANS in expression display */
            /* Sync Y= labels with loaded equations */
            graph_ui_sync_yeq_labels();
            /* Sync MODE screen cursor/highlight with loaded s_mode.committed */
            ui_update_mode_display();
            /* Sync RANGE field labels with loaded graph_state values */
            ui_update_range_display();
            /* Sync ZOOM FACTORS labels with loaded s_zf.x_fact / s_zf.y_fact */
            ui_update_zoom_factors_display();
        }
    }

    lvgl_unlock();

    if (keypadQueueHandle == NULL) {
        vTaskDelete(NULL);
        return;
    }

    Token_t received_token;
    for (;;) {
        if (xQueueReceive(keypadQueueHandle, &received_token,
                          portMAX_DELAY) == pdPASS) {
            Execute_Token(received_token);
        }
    }
}