/* TODO: Navigation state uses bespoke variables. Migrate to MenuState_t from
 * menu_state.h — see INTERFACE_REFACTOR_PLAN.md Item 3 (ui_vars.c proof-of-concept). */

#include "ui_matrix.h"
#include "ui_palette.h"
#include "calc_internal.h"
#include "calc_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Matrix UI State
 *---------------------------------------------------------------------------*/
lv_obj_t *ui_matrix_screen      = NULL;
lv_obj_t *ui_matrix_edit_screen = NULL;

MatrixMenuState_t matrix_menu_state = {0};

uint8_t    matrix_edit_idx        = 0;   /* 0=[A], 1=[B], 2=[C] */
int16_t    matrix_edit_cursor     = 0;   /* flat cell index; -1 = dim mode */
int16_t    matrix_edit_scroll     = 0;   /* first visible cell */
uint8_t    matrix_edit_dim_field  = 0;   /* 0=rows, 1=cols (dim mode only) */
char       matrix_edit_buf[16]    = {0};
uint8_t    matrix_edit_len        = 0;
uint8_t    matrix_edit_val_cursor = 0;

static lv_obj_t *matrix_tab_labels[2];
static lv_obj_t *matrix_item_labels[MENU_VISIBLE_ROWS];

static lv_obj_t *matrix_edit_title_lbl  = NULL;
static lv_obj_t *matrix_list_labels[7]; // MATRIX_LIST_VISIBLE is 7
static lv_obj_t *matrix_edit_up_lbl     = NULL;
static lv_obj_t *matrix_edit_down_lbl   = NULL;

static lv_obj_t *matrix_edit_cursor_box    = NULL;
static lv_obj_t *matrix_edit_cursor_inner  = NULL;

/* Strings / Constants */
static const char * const matrix_tab_names[2]     = {"MATRX", "EDIT"};
static const uint8_t matrix_tab_item_count[2]     = {6, 3};
static const char * const matrix_op_names[6]      = {
    "det(", "T", "dim(", "fill(", "identity(", "randM("
};
static const char * const matrix_op_insert[6]     = {
    "det(", "^T", "dim(", "fill(", "identity(", "randM("
};
static const char * const matrix_edit_item_names[3] = {"[A]", "[B]", "[C]"};

/* Reference to externally defined menu insertion helper */
extern void menu_insert_text(const char *ins, CalcMode_t *ret_mode);
extern void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll,
                     uint8_t tab_count, bool left, void (*update)(void));

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/
void matrix_edit_cursor_update(void)
{
    if (matrix_edit_cursor_box == NULL) return;

    if (matrix_edit_cursor == -1) {
        uint32_t char_pos = (matrix_edit_dim_field == 0) ? 4u : 6u;
        cursor_render(matrix_edit_cursor_box, matrix_edit_cursor_inner,
                      matrix_edit_title_lbl, char_pos,
                      cursor_visible, current_mode, false);
    } else {
        int vis_idx = (int)matrix_edit_cursor - (int)matrix_edit_scroll;
        if (vis_idx < 0 || vis_idx >= 7) {
            lv_obj_add_flag(matrix_edit_cursor_box, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        uint32_t char_pos = 4u + (uint32_t)matrix_edit_val_cursor;
        cursor_render(matrix_edit_cursor_box, matrix_edit_cursor_inner,
                      matrix_list_labels[vis_idx], char_pos,
                      cursor_visible, current_mode, false);
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
 * UI Initialization
 *---------------------------------------------------------------------------*/
void ui_init_matrix_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_matrix_screen = screen_create(scr);

    static const int16_t matrix_tab_x[2] = {4, 100};
    for (int i = 0; i < 2; i++) {
        matrix_tab_labels[i] = lv_label_create(ui_matrix_screen);
        lv_obj_set_pos(matrix_tab_labels[i], matrix_tab_x[i], 4);
        lv_obj_set_style_text_font(matrix_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(matrix_tab_labels[i], lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(matrix_tab_labels[i], matrix_tab_names[i]);
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        matrix_item_labels[i] = lv_label_create(ui_matrix_screen);
        lv_obj_set_pos(matrix_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(matrix_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(matrix_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(matrix_item_labels[i], "");
    }

    ui_matrix_edit_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_matrix_edit_screen, 320, 240); // DISPLAY_W, DISPLAY_H
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
void ui_update_matrix_display(void)
{
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_text_color(matrix_tab_labels[i],
            (i == (int)matrix_menu_state.tab) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    uint8_t item_count = matrix_tab_item_count[matrix_menu_state.tab];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        if (i < (int)item_count) {
            char buf[32];
            if (matrix_menu_state.tab == 0) {
                snprintf(buf, sizeof(buf), "%d:%s", i + 1, matrix_op_names[i]);
            } else {
                snprintf(buf, sizeof(buf), "%d:%s %dx%d",
                         i + 1, matrix_edit_item_names[i],
                         calc_matrices[i].rows, calc_matrices[i].cols);
            }
            lv_obj_set_style_text_color(matrix_item_labels[i],
                (i == (int)matrix_menu_state.item_cursor) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
            lv_label_set_text(matrix_item_labels[i], buf);
        } else {
            lv_label_set_text(matrix_item_labels[i], "");
        }
    }
}

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
bool handle_matrix_menu(Token_t t, MatrixMenuState_t *s)
{
    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s->tab, &s->item_cursor, NULL, 2, true, ui_update_matrix_display);
        return true;
    case TOKEN_RIGHT:
        tab_move(&s->tab, &s->item_cursor, NULL, 2, false, ui_update_matrix_display);
        return true;
    case TOKEN_UP:
        if (s->item_cursor > 0) s->item_cursor--;
        lvgl_lock(); ui_update_matrix_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s->item_cursor < matrix_tab_item_count[s->tab] - 1) s->item_cursor++;
        lvgl_lock(); ui_update_matrix_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        if (s->tab == 0) {
            const char *ins = matrix_op_insert[s->item_cursor];
            if (ins != NULL) {
                lvgl_lock();
                lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
                lvgl_unlock();
                menu_insert_text(ins, &s->return_mode);
            }
        } else {
            matrix_edit_idx    = s->item_cursor;
            matrix_edit_cursor     = 0;
            matrix_edit_scroll     = 0;
            matrix_edit_dim_field  = 0;
            current_mode = MODE_MATRIX_EDIT;
            matrix_edit_load_cell();
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_matrix_edit_display();
            lvgl_unlock();
        }
        return true;
    }
    case TOKEN_1 ... TOKEN_6: {
        int idx = (int)(t - TOKEN_0) - 1;
        if (idx >= 0 && idx < (int)matrix_tab_item_count[s->tab]) {
            s->item_cursor = (uint8_t)idx;
            if (s->tab == 0) {
                const char *ins = matrix_op_insert[idx];
                if (ins != NULL) {
                    lvgl_lock();
                    lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
                    lvgl_unlock();
                    menu_insert_text(ins, &s->return_mode);
                }
            } else {
                matrix_edit_idx    = (uint8_t)idx;
                matrix_edit_cursor = 0;
                matrix_edit_scroll = 0;
                matrix_edit_dim_field = 0;
                matrix_edit_len    = 0;
                matrix_edit_buf[0] = '\0';
                current_mode = MODE_MATRIX_EDIT;
                lvgl_lock();
                lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
                ui_update_matrix_edit_display();
                lvgl_unlock();
            }
        }
        return true;
    }
    case TOKEN_CLEAR:
    case TOKEN_MATRX:
        menu_close(TOKEN_MATRX);
        return true;
    case TOKEN_Y_EQUALS:
        s->return_mode = MODE_NORMAL;
        s->tab         = 0;
        s->item_cursor = 0;
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        s->return_mode = MODE_NORMAL;
        s->tab         = 0;
        s->item_cursor = 0;
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        s->return_mode = MODE_NORMAL;
        s->tab         = 0;
        s->item_cursor = 0;
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_GRAPH:
        s->return_mode = MODE_NORMAL;
        s->tab         = 0;
        s->item_cursor = 0;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_TRACE:
        s->return_mode = MODE_NORMAL;
        s->tab         = 0;
        s->item_cursor = 0;
        nav_to(MODE_GRAPH_TRACE);
        return true;
    default: {
        CalcMode_t ret = menu_close(TOKEN_MATRX);
        if (ret == MODE_GRAPH_YEQ) return true;
        return false; /* fall through to main switch */
    }
    }
    return true;
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
            current_mode = MODE_MATRIX_MENU;
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
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
        if (!insert_mode && matrix_edit_val_cursor < matrix_edit_len) {
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
            if (!insert_mode && matrix_edit_val_cursor < matrix_edit_len) {
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
            current_mode = MODE_MATRIX_MENU;
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_matrix_display();
            lvgl_unlock();
        }
        return;
    case TOKEN_MATRX:
        MXEDIT_COMMIT();
        current_mode = MODE_MATRIX_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_matrix_display();
        lvgl_unlock();
        return;
    default:
        return;
    }

#undef MXEDIT_COMMIT
#undef MXEDIT_SCROLL
}
