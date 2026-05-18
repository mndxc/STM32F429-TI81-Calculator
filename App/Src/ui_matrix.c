/**
 * @file  ui_matrix.c
 * @brief Matrix editor UI — slot browser (MATRX tab) and cell editor (EDIT tab).
 *
 * Two sub-state machines: matrix_menu (browse/select) and matrix_edit (dimension
 * change, cell navigation, and value entry). Both run inside handle_matrix_menu()
 * and handle_matrix_edit() respectively.
 */

#include "ui_matrix.h"
#include "ui_menu_screen.h"
#include "ui_palette.h"
#include "ui_shared.h"
#include "calculator_core.h"
#include "calc_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Owns:     Matrix cell editor state (matrix_edit_cursor, matrix_edit_dim_field,
 *           LVGL cell grid) and key dispatch for matrix editor modes.
 * Not owns: CalcMatrix_t data arrays (owned by calculator_core.c / persist).
 * Locks:    All LVGL calls require lvgl_lock() from the caller.
 *
 * Invariant: matrix_edit_cursor ranges [-1, rows*cols-1]; -1 means cursor is
 *            on the dimension row.
 */

/*---------------------------------------------------------------------------
 * Matrix UI State
 *---------------------------------------------------------------------------*/
static lv_obj_t *ui_matrix_edit_screen = NULL;

static MenuScreen_t s_matrix_ms;

static uint8_t    matrix_edit_idx        = 0;   /* 0=[A], 1=[B], 2=[C] */
static int16_t    matrix_edit_cursor     = 0;   /* flat cell index; -1 = dim mode */
static int16_t    matrix_edit_scroll     = 0;   /* first visible cell */
static uint8_t    matrix_edit_dim_field  = 0;   /* 0=rows, 1=cols (dim mode only) */
static char       matrix_edit_buf[16]    = {0};
static uint8_t    matrix_edit_len        = 0;
static uint8_t    matrix_edit_val_cursor = 0;

static lv_obj_t *matrix_edit_title_lbl  = NULL;
static lv_obj_t *matrix_list_labels[7]; /* MATRIX_LIST_VISIBLE is 7 */
static lv_obj_t *matrix_edit_up_lbl     = NULL;
static lv_obj_t *matrix_edit_down_lbl   = NULL;

static lv_obj_t *matrix_edit_cursor_box    = NULL;
static lv_obj_t *matrix_edit_cursor_inner  = NULL;

/* Matrix dim-editor cursor column layout:
 *   col 0–1: row-label glyph ("R " / "C ")
 *   col 2–3: current value digits
 *   col 4:   cursor sits here for ROWS field
 *   col 5–6: '×' separator + next digit
 *   col 6:   cursor sits here for COLS field            */
#define MATRIX_DIM_CURSOR_ROWS_COL  4u
#define MATRIX_DIM_CURSOR_COLS_COL  6u
/* Cell value editor starts at col 4 (same layout as dim-row digit columns) */
#define MATRIX_CELL_VALUE_COL_BASE  4u

/* Strings / Constants */
static const char * const matrix_matrx_labels[6] = {
    "1:rowSwap(", "2:row+(", "3:*row(", "4:*row+(", "5:det(", "6:T"
};
static const char * const matrix_op_insert[6] = {
    "rowSwap(", "row+(", "*row(", "*row+(", "det(", "^T"
};
static const char * const matrix_edit_item_names[3] = {"[A]", "[B]", "[C]"};

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/
/* Must NOT acquire lvgl_lock() — called from LVGL timer; lock already held. */
void matrix_edit_cursor_update(void)
{
    if (matrix_edit_cursor_box == NULL) return;

    if (matrix_edit_cursor == -1) {
        uint32_t char_pos = (matrix_edit_dim_field == 0) ? MATRIX_DIM_CURSOR_ROWS_COL : MATRIX_DIM_CURSOR_COLS_COL;
        cursor_render(matrix_edit_cursor_box, matrix_edit_cursor_inner,
                      matrix_edit_title_lbl, char_pos,
                      Calc_GetCursorVisible(), Calc_GetMode(), false);
    } else {
        int vis_idx = (int)matrix_edit_cursor - (int)matrix_edit_scroll;
        if (vis_idx < 0 || vis_idx >= 7) {
            lv_obj_add_flag(matrix_edit_cursor_box, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        uint32_t char_pos = MATRIX_CELL_VALUE_COL_BASE + (uint32_t)matrix_edit_val_cursor;
        cursor_render(matrix_edit_cursor_box, matrix_edit_cursor_inner,
                      matrix_list_labels[vis_idx], char_pos,
                      Calc_GetCursorVisible(), Calc_GetMode(), false);
    }
}

static void matrix_edit_load_cell(void)
{
    if (matrix_edit_cursor < 0) return;
    CalcMatrix_t *m = &calc_matrices[matrix_edit_idx];
    int r = (int)matrix_edit_cursor / (int)m->cols;
    int c = (int)matrix_edit_cursor % (int)m->cols;
    Calc_FormatResult(m->data[r][c], matrix_edit_buf, sizeof(matrix_edit_buf));
    matrix_edit_len        = (uint8_t)strlen(matrix_edit_buf);
    matrix_edit_val_cursor = 0;
}

/*---------------------------------------------------------------------------
 * MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void matrix_matrx_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    const char *ins = matrix_op_insert[idx];
    if (ins != NULL) {
        lvgl_lock();
        lv_obj_add_flag(s_matrix_ms.screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        menu_insert_text(ins, &s_matrix_ms.nav.return_mode);
    }
}

static void matrix_edit_get_label(int idx, char *buf, size_t bufsz)
{
    snprintf(buf, bufsz, "%d:%s %dx%d",
             idx + 1, matrix_edit_item_names[idx],
             calc_matrices[idx].rows, calc_matrices[idx].cols);
}

static void matrix_edit_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    matrix_edit_idx       = (uint8_t)idx;
    matrix_edit_cursor    = 0;
    matrix_edit_scroll    = 0;
    matrix_edit_dim_field = 0;
    matrix_edit_len       = 0;
    matrix_edit_buf[0]    = '\0';
    Calc_SetMode(MODE_MATRIX_EDIT);
    lvgl_lock();
    lv_obj_add_flag(s_matrix_ms.screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_matrix_edit_display();
    lvgl_unlock();
}

static void matrix_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    menu_close(TOKEN_MATRX);
}

static bool matrix_on_extra(Token_t t, MenuScreen_t *ms)
{
    if (t == TOKEN_MATRX) {
        menu_close(TOKEN_MATRX);
        return true;
    }
    return MenuScreen_DefaultExtra(t, ms);
}

static const char * const matrix_tab_names[2] = {"MATRX", "EDIT"};
static const int           matrix_tab_x[2]    = {4, 100};

static const MenuTabDesc_t matrix_tabs[2] = {
    { 6, matrix_matrx_labels, NULL,                  matrix_matrx_on_select },
    { 3, NULL,                 matrix_edit_get_label, matrix_edit_on_select  },
};

static const MenuScreenDesc_t matrix_desc = {
    .tab_count     = 2,
    .tab_names     = matrix_tab_names,
    .tab_x         = matrix_tab_x,
    .default_tab   = 0,
    .wrap_tabs     = false,
    .title         = NULL,
    .tabs          = matrix_tabs,
    .left_mode     = 0,
    .right_mode    = 0,
    .on_cancel     = matrix_on_cancel,
    .on_tab_switch = NULL,
    .on_extra      = matrix_on_extra,
};

/*---------------------------------------------------------------------------
 * Screen show/hide/visibility
 *---------------------------------------------------------------------------*/

/* Caller must hold lvgl_lock(). */
void Matrix_ShowMenuScreen(void) { lv_obj_clear_flag(s_matrix_ms.screen,    LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
void Matrix_HideMenuScreen(void) { lv_obj_add_flag  (s_matrix_ms.screen,    LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
void Matrix_ShowEditScreen(void) { lv_obj_clear_flag(ui_matrix_edit_screen,  LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
void Matrix_HideEditScreen(void) { lv_obj_add_flag  (ui_matrix_edit_screen,  LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
bool Matrix_IsEditScreenVisible(void)
{
    return ui_matrix_edit_screen != NULL &&
           !lv_obj_has_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/
void ui_init_matrix_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    MenuScreen_Init(&s_matrix_ms, &matrix_desc, scr);

    ui_matrix_edit_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_matrix_edit_screen, 320, 240); /* DISPLAY_W, DISPLAY_H */
    lv_obj_set_pos(ui_matrix_edit_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_matrix_edit_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_matrix_edit_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_matrix_edit_screen, 0, 0);
    lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);

    matrix_edit_title_lbl = lv_label_create(ui_matrix_edit_screen);
    lv_obj_set_pos(matrix_edit_title_lbl, 4, 4);
    lv_obj_set_style_text_font(matrix_edit_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(matrix_edit_title_lbl, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(matrix_edit_title_lbl, "[A] 3x3");

    for (int i = 0; i < 7; i++) {
        matrix_list_labels[i] = lv_label_create(ui_matrix_edit_screen);
        lv_obj_set_pos(matrix_list_labels[i], 4, 34 + i * 30);
        lv_obj_set_style_text_font(matrix_list_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(matrix_list_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(matrix_list_labels[i], "");
    }

    matrix_edit_down_lbl = lv_label_create(ui_matrix_edit_screen);
    lv_obj_set_pos(matrix_edit_down_lbl, 46, 34 + (7 - 1) * 30);
    lv_obj_set_style_text_font(matrix_edit_down_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(matrix_edit_down_lbl, lv_color_hex(COLOR_AMBER), 0);
    lv_label_set_text(matrix_edit_down_lbl, "\xE2\x86\x93");
    lv_obj_add_flag(matrix_edit_down_lbl, LV_OBJ_FLAG_HIDDEN);

    matrix_edit_up_lbl = lv_label_create(ui_matrix_edit_screen);
    lv_obj_set_pos(matrix_edit_up_lbl, 46, 34);
    lv_obj_set_style_text_font(matrix_edit_up_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(matrix_edit_up_lbl, lv_color_hex(COLOR_AMBER), 0);
    lv_label_set_text(matrix_edit_up_lbl, "\xE2\x86\x91");
    lv_obj_add_flag(matrix_edit_up_lbl, LV_OBJ_FLAG_HIDDEN);

    cursor_box_create(ui_matrix_edit_screen, true,
                      &matrix_edit_cursor_box, &matrix_edit_cursor_inner);
}

/*---------------------------------------------------------------------------
 * Display Updates
 *---------------------------------------------------------------------------*/
/* Caller must hold lvgl_lock(). */
void ui_update_matrix_display(void)
{
    MenuScreen_UpdateDisplay(&s_matrix_ms);
}

/* Caller must hold lvgl_lock(). */
void ui_update_matrix_edit_display(void)
{
    CalcMatrix_t *m = &calc_matrices[matrix_edit_idx];
    int total_cells = (int)m->rows * (int)m->cols;

    char title_buf[24];
    snprintf(title_buf, sizeof(title_buf), "%s %dx%d",
             matrix_edit_item_names[matrix_edit_idx], m->rows, m->cols);
    lv_obj_set_style_text_color(matrix_edit_title_lbl,
        (matrix_edit_cursor == -1) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(matrix_edit_title_lbl, title_buf);

    lv_obj_add_flag(matrix_edit_up_lbl,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(matrix_edit_down_lbl, LV_OBJ_FLAG_HIDDEN);

    bool more_above = (matrix_edit_cursor >= 0 && matrix_edit_scroll > 0);
    bool more_below = ((int)matrix_edit_scroll + 7 < total_cells);

    for (int i = 0; i < 7; i++) {
        int cell_idx = (int)matrix_edit_scroll + i;
        if (cell_idx >= total_cells) {
            lv_label_set_text(matrix_list_labels[i], "");
            lv_obj_set_style_text_color(matrix_list_labels[i], lv_color_hex(COLOR_WHITE), 0);
            continue;
        }
        int row_1b = cell_idx / (int)m->cols + 1;
        int col_1b = cell_idx % (int)m->cols + 1;
        bool is_cursor = (matrix_edit_cursor >= 0 && cell_idx == (int)matrix_edit_cursor);
        char val_str[16];
        if (is_cursor && matrix_edit_len > 0) {
            snprintf(val_str, sizeof(val_str), "%s", matrix_edit_buf);
        } else {
            Calc_FormatResult(m->data[row_1b - 1][col_1b - 1], val_str, sizeof(val_str));
        }

        bool show_down = (more_below && i == 6);
        bool show_up   = (more_above && i == 0);
        char sep = (show_down || show_up) ? ' ' : '=';

        char row_buf[32];
        snprintf(row_buf, sizeof(row_buf), "%d,%d%c%s", row_1b, col_1b, sep, val_str);
        lv_label_set_text(matrix_list_labels[i], row_buf);
        lv_obj_set_style_text_color(matrix_list_labels[i],
            is_cursor ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);

        if (show_down) lv_obj_clear_flag(matrix_edit_down_lbl, LV_OBJ_FLAG_HIDDEN);
        if (show_up)   lv_obj_clear_flag(matrix_edit_up_lbl,   LV_OBJ_FLAG_HIDDEN);
    }

    matrix_edit_cursor_update();
}

/*---------------------------------------------------------------------------
 * Token Handlers
 *---------------------------------------------------------------------------*/
bool handle_matrix_menu(Token_t t)
{
    return MenuScreen_HandleToken(&s_matrix_ms, t);
}

void handle_matrix_edit(Token_t t)
{
    CalcMatrix_t *m = &calc_matrices[matrix_edit_idx];

#define MXEDIT_COMMIT() do { \
    if (matrix_edit_cursor >= 0) { \
        int _r = matrix_edit_cursor / (int)m->cols; \
        int _c = matrix_edit_cursor % (int)m->cols; \
        if (matrix_edit_len > 0) \
            m->data[_r][_c] = strtof(matrix_edit_buf, NULL); \
        matrix_edit_len = 0; matrix_edit_val_cursor = 0; matrix_edit_buf[0] = '\0'; \
    } \
} while(0)

#define MXEDIT_SCROLL() do { \
    if (matrix_edit_cursor >= 0) { \
        if (matrix_edit_cursor < (int)matrix_edit_scroll) \
            matrix_edit_scroll = (int16_t)matrix_edit_cursor; \
        if (matrix_edit_cursor >= (int)matrix_edit_scroll + 7) \
            matrix_edit_scroll = (int16_t)(matrix_edit_cursor - 7 + 1); \
    } else { \
        matrix_edit_scroll = 0; \
    } \
} while(0)

    int total_cells = (int)m->rows * (int)m->cols;

    if (matrix_edit_cursor == -1) {
        switch (t) {
        case TOKEN_1 ... TOKEN_6: {
            int new_dim = (int)(t - TOKEN_0);
            if (matrix_edit_dim_field == 0) {
                m->rows = (uint8_t)new_dim;
            } else {
                m->cols = (uint8_t)new_dim;
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        }
        case TOKEN_LEFT:
            matrix_edit_dim_field = 0;
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            matrix_edit_dim_field = 1;
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_ENTER:
        case TOKEN_DOWN:
            matrix_edit_cursor = 0;
            matrix_edit_scroll = 0;
            matrix_edit_load_cell();
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_CLEAR:
        case TOKEN_MATRX:
            Calc_SetMode(MODE_MATRIX_MENU);
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_matrix_ms.screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_matrix_display();
            lvgl_unlock();
            return;
        default:
            return;
        }
    }

    switch (t) {
    case TOKEN_0 ... TOKEN_9: {
        char ch = (char)((t - TOKEN_0) + '0');
        if (!Calc_GetInsertMode() && matrix_edit_val_cursor < matrix_edit_len) {
            matrix_edit_buf[matrix_edit_val_cursor++] = ch;
        } else if (matrix_edit_len < (uint8_t)(sizeof(matrix_edit_buf) - 1)) {
            memmove(&matrix_edit_buf[matrix_edit_val_cursor + 1],
                    &matrix_edit_buf[matrix_edit_val_cursor],
                    matrix_edit_len - matrix_edit_val_cursor + 1);
            matrix_edit_buf[matrix_edit_val_cursor++] = ch;
            matrix_edit_len++;
        }
        lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        return;
    }
    case TOKEN_DECIMAL: {
        if (strchr(matrix_edit_buf, '.') == NULL) {
            char ch = '.';
            if (!Calc_GetInsertMode() && matrix_edit_val_cursor < matrix_edit_len) {
                matrix_edit_buf[matrix_edit_val_cursor++] = ch;
            } else if (matrix_edit_len < (uint8_t)(sizeof(matrix_edit_buf) - 1)) {
                memmove(&matrix_edit_buf[matrix_edit_val_cursor + 1],
                        &matrix_edit_buf[matrix_edit_val_cursor],
                        matrix_edit_len - matrix_edit_val_cursor + 1);
                matrix_edit_buf[matrix_edit_val_cursor++] = ch;
                matrix_edit_len++;
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        }
        return;
    }
    case TOKEN_NEG:
        if (matrix_edit_len > 0 && matrix_edit_buf[0] == '-') {
            memmove(matrix_edit_buf, matrix_edit_buf + 1, matrix_edit_len);
            matrix_edit_len--;
            if (matrix_edit_val_cursor > 0) matrix_edit_val_cursor--;
        } else if (matrix_edit_len < (uint8_t)(sizeof(matrix_edit_buf) - 1)) {
            memmove(matrix_edit_buf + 1, matrix_edit_buf, matrix_edit_len + 1);
            matrix_edit_buf[0] = '-';
            matrix_edit_len++;
            matrix_edit_val_cursor++;
        }
        lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        return;
    case TOKEN_DEL:
        if (matrix_edit_val_cursor > 0) {
            memmove(&matrix_edit_buf[matrix_edit_val_cursor - 1],
                    &matrix_edit_buf[matrix_edit_val_cursor],
                    matrix_edit_len - matrix_edit_val_cursor + 1);
            matrix_edit_len--;
            matrix_edit_val_cursor--;
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        }
        return;
    case TOKEN_ENTER:
    case TOKEN_DOWN: {
        MXEDIT_COMMIT();
        if (matrix_edit_cursor < total_cells - 1)
            matrix_edit_cursor++;
        MXEDIT_SCROLL();
        matrix_edit_load_cell();
        lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        return;
    }
    case TOKEN_UP: {
        MXEDIT_COMMIT();
        if (matrix_edit_cursor > 0) {
            matrix_edit_cursor--;
            MXEDIT_SCROLL();
            matrix_edit_load_cell();
        } else {
            matrix_edit_cursor     = -1;
            matrix_edit_dim_field  = 0;
            matrix_edit_scroll     = 0;
            matrix_edit_len        = 0;
            matrix_edit_val_cursor = 0;
            matrix_edit_buf[0]     = '\0';
        }
        lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        return;
    }
    case TOKEN_RIGHT:
        if (matrix_edit_val_cursor < matrix_edit_len)
            matrix_edit_val_cursor++;
        lvgl_lock(); matrix_edit_cursor_update(); lvgl_unlock();
        return;
    case TOKEN_LEFT:
        if (matrix_edit_val_cursor > 0)
            matrix_edit_val_cursor--;
        lvgl_lock(); matrix_edit_cursor_update(); lvgl_unlock();
        return;
    case TOKEN_CLEAR:
        if (matrix_edit_len > 0) {
            matrix_edit_len        = 0;
            matrix_edit_val_cursor = 0;
            matrix_edit_buf[0]     = '\0';
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
        } else {
            Calc_SetMode(MODE_MATRIX_MENU);
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_matrix_ms.screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_matrix_display();
            lvgl_unlock();
        }
        return;
    case TOKEN_MATRX:
        MXEDIT_COMMIT();
        Calc_SetMode(MODE_MATRIX_MENU);
        lvgl_lock();
        lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_matrix_ms.screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_matrix_display();
        lvgl_unlock();
        return;
    default:
        return;
    }

#undef MXEDIT_COMMIT
#undef MXEDIT_SCROLL
}

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

/* Caller must hold lvgl_lock(). */
void Matrix_MenuOpen(CalcMode_t return_to)
{
    s_matrix_ms.nav.return_mode = return_to;
    Calc_SetMode(MODE_MATRIX_MENU);
    lv_obj_clear_flag(s_matrix_ms.screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_matrix_ms);
}

CalcMode_t Matrix_MenuClose(void)
{
    CalcMode_t ret              = s_matrix_ms.nav.return_mode;
    s_matrix_ms.nav.return_mode = MODE_NORMAL;
    return ret;
}
