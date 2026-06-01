/**
 * @file  ui_main_display.c
 * @brief Main screen LVGL layout, cursor rendering, and history display.
 *
 * Owns two groups of responsibility extracted from calculator_core.c:
 *   1. LVGL UI object creation and layout — main screen, display rows, cursor.
 *   2. History entry rendering and matrix display formatting helpers.
 *
 * Block-4 state (current_mode, ans, angle_degrees, etc.) remains in
 * calculator_core.c and is accessed here through the Calc_* API.
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "calc_history.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "expr_util.h"
#  include "ui_palette.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "calculator_core_test_stubs.h"
#  include "expr_editor.h"
#  include "calculator_core.h"
#  include "calc_mode_topology.h"
#else
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "graph.h"
#  include "graph_draw.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "ui_shared.h"
#  include "calc_history.h"
#  include "calculator_core.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "ui_math_menu.h"
#  include "ui_matrix.h"
#  include "ui_prgm.h"
#  include "prgm_editor.h"
#  include "ui_prgm_ctl.h"
#  include "ui_prgm_io.h"
#  include "ui_prgm_exec.h"
#  include "ui_prgm_mode.h"
#  include "ui_stat.h"
#  include "ui_draw.h"
#  include "ui_vars.h"
#  include "ui_yvars.h"
#  include "ui_reset.h"
#  include "ui_error.h"
#  include "graph_ui.h"
#  include "graph_ui_range.h"
#  include "ui_graph_zoom.h"
#  include "ui_palette.h"
#  include "expr_util.h"
#  include "expr_editor.h"
#  include "cmsis_os.h"
#  include "lvgl.h"
#  include "main.h"
#  include "calc_mode_topology.h"
#endif
#include "ui_main_display.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define MATRIX_MAX_DIM      CALC_MATRIX_MAX_DIM

/*---------------------------------------------------------------------------
 * Private variables — display objects
 *---------------------------------------------------------------------------*/

static lv_obj_t *disp_rows[DISP_ROW_COUNT];
static lv_timer_t *cursor_timer = NULL;
static lv_style_t style_bg;
static uint8_t expr_chars_per_row = 22;

/*---------------------------------------------------------------------------
 * Private variables — matrix history ring
 *
 * The matrix ring stores CalcMatrix_t values for history entries so that
 * render_result_row can format and scroll them without re-evaluating.
 * Owned here (display layer) because only rendering code reads the ring;
 * calculator_core.c writes it through UiDisplay_PushMatrixToRing().
 *---------------------------------------------------------------------------*/

static CalcMatrix_t matrix_ring[MATRIX_RING_COUNT];
static uint8_t      matrix_ring_gen_table[MATRIX_RING_COUNT];
static uint8_t      matrix_ring_write_count = 0;

/*---------------------------------------------------------------------------
 * Matrix ring write API (called by calculator_core.c commit functions)
 *---------------------------------------------------------------------------*/

void UiDisplay_PushMatrixToRing(uint8_t mat_idx,
                                uint8_t *ring_idx_out,
                                uint8_t *ring_gen_out,
                                uint8_t *rows_cache_out)
{
    uint8_t ring_idx = (uint8_t)(matrix_ring_write_count % MATRIX_RING_COUNT);
    matrix_ring[ring_idx]           = calc_matrices[mat_idx];
    matrix_ring_gen_table[ring_idx] = matrix_ring_write_count;
    *ring_idx_out   = ring_idx;
    *ring_gen_out   = matrix_ring_write_count;
    *rows_cache_out = calc_matrices[mat_idx].rows;
    matrix_ring_write_count++;
}

uint8_t UiDisplay_GetExprCharsPerRow(void)
{
    return expr_chars_per_row;
}

const CalcMatrix_t *UiDisplay_GetHistoryMatrix(const HistoryEntry_t *e)
{
    if (!e->has_matrix) return NULL;
    uint8_t slot = e->matrix_ring_idx;
    if (matrix_ring_gen_table[slot] != e->matrix_ring_gen) return NULL;
    return &matrix_ring[slot];
}

/*---------------------------------------------------------------------------
 * UI initialisation
 *---------------------------------------------------------------------------*/

static void ui_init_styles(void)
{
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

static void ui_init_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &style_bg, 0);
    lv_obj_set_size(scr, DISPLAY_W, DISPLAY_H);

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

    ExprEditor_Init(scr);

    uint16_t glyph_w = lv_font_get_glyph_width(&jetbrains_mono_24, 'X', 0);
    if (glyph_w > 0)
        expr_chars_per_row = (uint8_t)((DISPLAY_W - 8) / glyph_w);
}

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

/*---------------------------------------------------------------------------
 * Cursor rendering
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
            inner_text = "";
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

/*---------------------------------------------------------------------------
 * Result formatting
 *---------------------------------------------------------------------------*/

/**
 * @brief Format a CalcResult_t into a displayable string.
 *
 * For scalar results: delegates to Calc_FormatResultStr.
 * For matrix results: produces a newline-separated grid "[a b c]\n[d e f]\n[g h i]".
 * Updates ANS via Calc_SetAnsScalar / Calc_SetAnsMatrix on successful results.
 */
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size)
{
    Calc_FormatResultStr(r, buf, buf_size);
    if (r->error == CALC_OK) {
        if (r->has_matrix) {
            Calc_SetAnsMatrix((float)r->matrix_idx);
        } else {
            Calc_SetAnsScalar(r->value);
        }
    }
}

/*---------------------------------------------------------------------------
 * History display helpers
 *---------------------------------------------------------------------------*/

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
        else return false;
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
        for (int p = 0; p < cw && pos < (int)sizeof(full) - 1; p++)
            full[pos++] = (p < cl) ? cell[p] : ' ';
    }
    if (pos < (int)sizeof(full) - 1) full[pos++] = ']';
    full[pos] = '\0';
    int full_len = pos;

    bool clip_left  = (scroll_offset > 0);
    bool clip_right = (scroll_offset + display_cols < full_len);

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

/**
 * @brief Render one history result row onto @p label.
 *
 * Sets the label colour to COLOR_WHITE, then either formats a matrix row
 * (column-aligned, left-aligned, with horizontal scroll applied) or a scalar
 * result line (right-aligned), and sets the label text.
 */
static void render_result_row(lv_obj_t *label, const HistoryEntry_t *entry,
                               int entry_idx, int result_line)
{
    char rbuf[MAX_RESULT_LEN];
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_WHITE), 0);
    if (entry->has_matrix) {
        const CalcMatrix_t *m = UiDisplay_GetHistoryMatrix(entry);
        if (m != NULL) {
            int off = (CalcHistory_GetMatrixScrollFocus() == (int8_t)entry_idx)
                      ? (int)CalcHistory_GetMatrixScrollOffset() : 0;
            matrix_format_row(m, result_line,
                              off, (int)expr_chars_per_row, rbuf, sizeof(rbuf));
        } else {
            get_result_line(entry->result, result_line, rbuf, sizeof(rbuf));
        }
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    } else {
        get_result_line(entry->result, result_line, rbuf, sizeof(rbuf));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_label_set_text(label, rbuf);
}

/*---------------------------------------------------------------------------
 * Main display refresh
 *---------------------------------------------------------------------------*/

/**
 * @brief Redraws all DISP_ROW_COUNT display rows from the history buffer
 *        and the current input expression.
 *
 * Each history entry occupies ceil(expr_len/cpr) expression sub-rows + 1 result
 * row, so long committed expressions wrap just as the active expression does.
 * Current expression sub-rows follow immediately after all history rows.
 */
void ui_refresh_display(void)
{
    if (disp_rows[0] == NULL) return;

    int cpr = (int)expr_chars_per_row;
    int expr_len = ExprEditor_GetLen();
    int expr_rows = (expr_len == 0) ? 1 : (expr_len + cpr - 1) / cpr;

    int cnt = (int)CalcHistory_GetCount();
    int num_entries = (cnt < HISTORY_LINE_COUNT) ? cnt : HISTORY_LINE_COUNT;

    int total_history_lines = 0;
    for (int d = 0; d < num_entries; d++) {
        int idx = (int)((cnt - num_entries + d) % HISTORY_LINE_COUNT);
        const HistoryEntry_t *e = CalcHistory_GetEntry((uint8_t)idx);
        int elen = (int)strlen(e->expression);
        int erows = (elen + cpr - 1) / cpr;
        int rlines = e->has_matrix
                     ? (int)e->matrix_rows_cache
                     : count_result_lines(e->result);
        total_history_lines += erows + rlines;
    }

    int total = total_history_lines + expr_rows;
    int start = (total > DISP_ROW_COUNT) ? (total - DISP_ROW_COUNT) : 0;

    int expr_cursor     = ExprEditor_GetCursor();
    int cursor_expr_row = expr_cursor / cpr;
    int cursor_col      = expr_cursor % cpr;

    struct { int entry_idx; int sub_row; bool is_result; } line_map[DISP_ROW_COUNT];
    int map_len = 0;
    int line    = 0;

    for (int d = 0; d < num_entries; d++) {
        int idx = (int)((cnt - num_entries + d) % HISTORY_LINE_COUNT);
        const HistoryEntry_t *e = CalcHistory_GetEntry((uint8_t)idx);
        int elen  = (int)strlen(e->expression);
        int erows = (elen + cpr - 1) / cpr;
        int rlines = e->has_matrix
                     ? (int)e->matrix_rows_cache
                     : count_result_lines(e->result);

        for (int er = 0; er < erows; er++, line++) {
            if (line >= start && map_len < DISP_ROW_COUNT) {
                line_map[map_len].entry_idx = idx;
                line_map[map_len].sub_row   = er;
                line_map[map_len].is_result = false;
                map_len++;
            }
        }
        for (int rl = 0; rl < rlines; rl++, line++) {
            if (line >= start && map_len < DISP_ROW_COUNT) {
                line_map[map_len].entry_idx = idx;
                line_map[map_len].sub_row   = rl;
                line_map[map_len].is_result = true;
                map_len++;
            }
        }
    }
    for (int el = 0; el < expr_rows; el++, line++) {
        if (line >= start && map_len < DISP_ROW_COUNT) {
            line_map[map_len].entry_idx = -1;
            line_map[map_len].sub_row   = el;
            line_map[map_len].is_result = false;
            map_len++;
        }
    }

    CalcMode_t cur_mode  = Calc_GetMode();
    bool       ins_mode  = Calc_GetInsertMode();

    for (int row = 0; row < DISP_ROW_COUNT; row++) {
        if (row >= map_len) {
            lv_label_set_text(disp_rows[row], "");
            continue;
        }

        int  eidx      = line_map[row].entry_idx;
        int  sub_row   = line_map[row].sub_row;
        bool is_result = line_map[row].is_result;

        if (eidx >= 0 && !is_result) {
            const HistoryEntry_t *e = CalcHistory_GetEntry((uint8_t)eidx);
            int elen = (int)strlen(e->expression);
            int char_start = sub_row * cpr;
            int char_end   = char_start + cpr;
            if (char_end > elen) char_end = elen;
            int seg_len = char_end - char_start;
            if (seg_len < 0) seg_len = 0;
            char row_buf[MAX_EXPR_LEN + 1];
            memcpy(row_buf, e->expression + char_start, (size_t)seg_len);
            row_buf[seg_len] = '\0';
            lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_GREY_MED), 0);
            lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(disp_rows[row], row_buf);
        } else if (eidx >= 0) {
            render_result_row(disp_rows[row], CalcHistory_GetEntry((uint8_t)eidx), eidx, sub_row);
        } else {
            int char_start = sub_row * cpr;
            int char_end   = char_start + cpr;
            if (char_end > expr_len) char_end = expr_len;
            int seg_len = char_end - char_start;
            if (seg_len < 0) seg_len = 0;
            char row_buf[MAX_EXPR_LEN + 1];
            memcpy(row_buf, ExprEditor_GetBuf() + char_start, (size_t)seg_len);
            row_buf[seg_len] = '\0';
            lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_GREY_LIGHT), 0);
            lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(disp_rows[row], row_buf);
            if (sub_row == cursor_expr_row)
                ExprEditor_CursorUpdate(disp_rows[row], (uint32_t)cursor_col,
                                        cur_mode, ins_mode);
        }
    }

    if (total_history_lines + cursor_expr_row < start)
        ExprEditor_CursorHide();
}

static void update_overlay_cursor(void)
{
    if (Graph_IsYeqScreenVisible())
        yeq_cursor_update();
    else if (Graph_IsRangeScreenVisible())
        range_cursor_update();
    else if (Graph_IsZoomFactorsScreenVisible())
        zoom_factors_cursor_update();
    else if (Matrix_IsEditScreenVisible())
        matrix_edit_cursor_update();
    else if (Prgm_IsEditorScreenVisible())
        PrgmEditor_CursorUpdate();
    else if (Prgm_IsNewScreenVisible())
        prgm_new_cursor_update();
}

/**
 * @brief LVGL timer callback — blinks the cursor every CURSOR_BLINK_MS.
 *
 * Called from lv_task_handler() — DefaultTask already holds the LVGL mutex.
 * Do NOT call lvgl_lock() here or it will deadlock.
 */
static void cursor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ExprEditor_SetCursorVisible(!ExprEditor_GetCursorVisible());
    ui_refresh_display();
    update_overlay_cursor();
}

/**
 * @brief Refreshes the cursor on all screens to reflect the current mode.
 */
void ui_update_status_bar(void)
{
    ExprEditor_SetCursorVisible(true);
    ui_refresh_display();
    update_overlay_cursor();
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

/*---------------------------------------------------------------------------
 * Top-level init — called from StartCalcCoreTask (mode_dispatcher.c)
 *---------------------------------------------------------------------------*/

void UiMainDisplay_Init(void)
{
    ui_init_styles();
    ui_init_screen();
    CalcHistory_RegisterDisplayCallback(ui_refresh_display);
    cursor_timer = lv_timer_create(cursor_timer_cb, CURSOR_BLINK_MS, NULL);
}
