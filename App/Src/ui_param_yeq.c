/**
 * @file    ui_param_yeq.c
 * @brief   Parametric Y= editor screen (X₁t/Y₁t … X₃t/Y₃t).
 *
 * Extracted from graph_ui.c.  Part of the calculator UI super-module —
 * includes calc_internal.h for shared state and LVGL helpers.
 *
 * Public surface: param_yeq_init_screen, param_yeq_nav_enter,
 *                 param_yeq_cursor_update, handle_param_yeq_mode.
 */
#include "ui_param_yeq.h"
#include "calc_internal.h"
#include "graph.h"
#include "graph_ui.h"       /* nav_to, ui_update_status_bar */
#include "expr_util.h"
#include "ui_palette.h"
#include "lvgl.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Private types
 *--------------------------------------------------------------------------*/

typedef struct {
    uint8_t  selected;    /* Which row (0–PARAM_YEQ_ROW_COUNT-1) is active */
    uint8_t  cursor_pos;  /* Byte offset of insertion point within the equation */
    bool     on_equal;    /* True if cursor is on the '=' sign */
} ParamYeqState_t;

/*---------------------------------------------------------------------------
 * Private state and LVGL objects
 *--------------------------------------------------------------------------*/

lv_obj_t *ui_param_yeq_screen          = NULL;

static lv_obj_t *ui_lbl_param_name[PARAM_YEQ_ROW_COUNT];
static lv_obj_t *ui_lbl_param_equal[PARAM_YEQ_ROW_COUNT];
static lv_obj_t *ui_lbl_param_eq[PARAM_YEQ_ROW_COUNT];
static lv_obj_t *param_cursor_box   = NULL;
static lv_obj_t *param_cursor_inner = NULL;

static ParamYeqState_t s_param_yeq = {0};

/*---------------------------------------------------------------------------
 * Private helpers
 *--------------------------------------------------------------------------*/

/* Returns pointer to the active parametric equation string for row i.
 * Even rows (0,2,4) → param_x[i/2]; odd rows (1,3,5) → param_y[i/2]. */
static char *param_eq_ptr(uint8_t row)
{
    uint8_t pair = row / 2;
    return (row % 2 == 0) ? Graph_GetParamEquationXBuf(pair) : Graph_GetParamEquationYBuf(pair);
}

static void param_yeq_update_highlight(void)
{
    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        bool sel = (i == s_param_yeq.selected);
        lv_color_t c = lv_color_hex(sel ? COLOR_YELLOW : COLOR_WHITE);
        if (ui_lbl_param_name[i])  lv_obj_set_style_text_color(ui_lbl_param_name[i],  c, 0);
        if (ui_lbl_param_equal[i]) lv_obj_set_style_text_color(ui_lbl_param_equal[i], c, 0);
        if (ui_lbl_param_eq[i])    lv_obj_set_style_text_color(ui_lbl_param_eq[i],    c, 0);
    }
}

static void param_yeq_reflow_rows(void)
{
    lv_obj_update_layout(ui_param_yeq_screen);
    int32_t y = 4;
    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        lv_obj_set_pos(ui_lbl_param_name[i],  4,  y);
        lv_obj_set_pos(ui_lbl_param_equal[i], 44, y);
        lv_obj_set_pos(ui_lbl_param_eq[i],    57, y);
        int32_t h = lv_obj_get_height(ui_lbl_param_eq[i]);
        if (h < 26) h = 26;
        y += h + 2;
    }
}

/* Delete the character immediately before the cursor in the active param equation. */
static void param_yeq_del_at_cursor(void)
{
    char *eq       = param_eq_ptr(s_param_yeq.selected);
    uint8_t eq_len = (uint8_t)strlen(eq);
    if (s_param_yeq.cursor_pos == 0) return;
    uint8_t prev = s_param_yeq.cursor_pos;
    do { prev--; }
    while (prev > 0 && ((uint8_t)eq[prev] & 0xC0) == 0x80);
    memmove(&eq[prev], &eq[s_param_yeq.cursor_pos], eq_len - s_param_yeq.cursor_pos + 1);
    s_param_yeq.cursor_pos = prev;
    lvgl_lock();
    lv_label_set_text(ui_lbl_param_eq[s_param_yeq.selected], eq);
    param_yeq_reflow_rows();
    param_yeq_cursor_update();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

void param_yeq_init_screen(lv_obj_t *parent)
{
    /* Row labels: X₁t, Y₁t, X₂t, Y₂t, X₃t, Y₃t */
    static const char * const param_row_names[PARAM_YEQ_ROW_COUNT] = {
        "X\xe2\x82\x81""t", "Y\xe2\x82\x81""t",
        "X\xe2\x82\x82""t", "Y\xe2\x82\x82""t",
        "X\xe2\x82\x83""t", "Y\xe2\x82\x83""t",
    };

    ui_param_yeq_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_param_yeq_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_param_yeq_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_param_yeq_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_param_yeq_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_param_yeq_screen, 0, 0);
    lv_obj_clear_flag(ui_param_yeq_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_param_yeq_screen, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        int32_t row_y = 4 + i * 26;

        ui_lbl_param_name[i] = lv_label_create(ui_param_yeq_screen);
        lv_obj_set_pos(ui_lbl_param_name[i], 4, row_y);
        lv_obj_set_style_text_font(ui_lbl_param_name[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_param_name[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_param_name[i], param_row_names[i]);

        ui_lbl_param_equal[i] = lv_label_create(ui_param_yeq_screen);
        lv_obj_set_pos(ui_lbl_param_equal[i], 44, row_y);
        lv_obj_set_style_text_font(ui_lbl_param_equal[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_param_equal[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_param_equal[i], "=");

        ui_lbl_param_eq[i] = lv_label_create(ui_param_yeq_screen);
        lv_obj_set_pos(ui_lbl_param_eq[i], 57, row_y);
        lv_obj_set_width(ui_lbl_param_eq[i], DISPLAY_W - 61);
        lv_obj_set_style_text_font(ui_lbl_param_eq[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_param_eq[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_long_mode(ui_lbl_param_eq[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(ui_lbl_param_eq[i], "");
    }

    cursor_box_create(ui_param_yeq_screen, true, &param_cursor_box, &param_cursor_inner);
}

void param_yeq_nav_enter(void)
{
    /* Called from nav_to() while lvgl_lock() is already held. */
    s_param_yeq.on_equal   = false;
    s_param_yeq.cursor_pos = (uint8_t)strlen(param_eq_ptr(s_param_yeq.selected));
    lv_obj_clear_flag(ui_param_yeq_screen, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        lv_label_set_text(ui_lbl_param_eq[i], param_eq_ptr((uint8_t)i));
    }
    param_yeq_update_highlight();
    param_yeq_reflow_rows();
    param_yeq_cursor_update();
}

void param_yeq_cursor_update(void)
{
    if (param_cursor_box == NULL) return;
    if (s_param_yeq.on_equal) {
        cursor_render(param_cursor_box, param_cursor_inner,
                      ui_lbl_param_equal[s_param_yeq.selected], 0,
                      cursor_visible, current_mode, insert_mode);
    } else {
        if (ui_lbl_param_eq[s_param_yeq.selected] == NULL) return;
        const char *txt = lv_label_get_text(ui_lbl_param_eq[s_param_yeq.selected]);
        uint32_t glyph_pos = ExprUtil_Utf8ByteToGlyph(txt, s_param_yeq.cursor_pos);
        cursor_render(param_cursor_box, param_cursor_inner,
                      ui_lbl_param_eq[s_param_yeq.selected], glyph_pos,
                      cursor_visible, current_mode, insert_mode);
    }
}

bool handle_param_yeq_mode(Token_t t)
{
    char *eq     = param_eq_ptr(s_param_yeq.selected);
    uint8_t pair = s_param_yeq.selected / 2;

    /* Cursor sanitise */
    uint8_t eq_len = (uint8_t)strlen(eq);
    if (s_param_yeq.cursor_pos > eq_len) s_param_yeq.cursor_pos = eq_len;

    /* Navigation keys */
    switch (t) {
    case TOKEN_GRAPH:    nav_to(MODE_NORMAL);            return true;
    case TOKEN_RANGE:    nav_to(MODE_GRAPH_RANGE);       return true;
    case TOKEN_ZOOM:     nav_to(MODE_GRAPH_ZOOM);        return true;
    case TOKEN_TRACE:    nav_to(MODE_GRAPH_TRACE);       return true;
    case TOKEN_Y_EQUALS:
        current_mode = MODE_NORMAL;
        lvgl_lock(); hide_all_screens(); lvgl_unlock();
        return true;

    case TOKEN_CLEAR:
        eq[0] = '\0'; s_param_yeq.cursor_pos = 0;
        lvgl_lock();
        lv_label_set_text(ui_lbl_param_eq[s_param_yeq.selected], eq);
        param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_DEL:
        param_yeq_del_at_cursor();
        return true;

    case TOKEN_INS:
        insert_mode = !insert_mode;
        lvgl_lock(); param_yeq_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_LEFT:
        if (s_param_yeq.on_equal) return true;
        if (s_param_yeq.cursor_pos > 0) {
            do { s_param_yeq.cursor_pos--; }
            while (s_param_yeq.cursor_pos > 0 &&
                   ((uint8_t)eq[s_param_yeq.cursor_pos] & 0xC0) == 0x80);
        } else {
            s_param_yeq.on_equal = true;
        }
        lvgl_lock(); param_yeq_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_RIGHT:
        if (s_param_yeq.on_equal) {
            s_param_yeq.on_equal   = false;
            s_param_yeq.cursor_pos = 0;
        } else if (s_param_yeq.cursor_pos < eq_len) {
            uint8_t step = ExprUtil_Utf8CharSize(&eq[s_param_yeq.cursor_pos]);
            s_param_yeq.cursor_pos += step ? step : 1;
            if (s_param_yeq.cursor_pos > eq_len) s_param_yeq.cursor_pos = eq_len;
        }
        lvgl_lock(); param_yeq_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_UP:
        if (s_param_yeq.selected > 0) s_param_yeq.selected--;
        if (!s_param_yeq.on_equal)
            s_param_yeq.cursor_pos = strlen(param_eq_ptr(s_param_yeq.selected));
        lvgl_lock();
        param_yeq_update_highlight(); param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER:
        if (s_param_yeq.on_equal) {
            /* Toggle enable for this pair */
            Graph_SetParamEnabled(pair, !Graph_GetState()->param_enabled[pair]);
            lvgl_lock(); param_yeq_update_highlight(); lvgl_unlock();
            return true;
        }
        __attribute__((fallthrough));
    case TOKEN_DOWN:
        if (s_param_yeq.selected < PARAM_YEQ_ROW_COUNT - 1) s_param_yeq.selected++;
        if (!s_param_yeq.on_equal)
            s_param_yeq.cursor_pos = strlen(param_eq_ptr(s_param_yeq.selected));
        lvgl_lock();
        param_yeq_update_highlight(); param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
        return true;

    default:
        break;
    }

    /* Character insertion */
    const char *append = NULL;
    char num_buf[2] = {0, 0};

    switch (t) {
    case TOKEN_X_T:   append = "T";      break;  /* T is the parametric free variable */
    case TOKEN_0 ... TOKEN_9:
        num_buf[0] = (char)((t - TOKEN_0) + '0');
        append = num_buf;
        break;
    case TOKEN_DECIMAL: append = ".";     break;
    case TOKEN_NEG:     append = "-";     break;
    case TOKEN_ADD:     append = "+";     break;
    case TOKEN_SUB:     append = "-";     break;
    case TOKEN_MULT:    append = "*";     break;
    case TOKEN_DIV:     append = "/";     break;
    case TOKEN_POWER:   append = "^";     break;
    case TOKEN_L_PAR:   append = "(";     break;
    case TOKEN_R_PAR:   append = ")";     break;
    case TOKEN_PI:      append = "\xCF\x80"; break;  /* π UTF-8 */
    case TOKEN_SIN:     append = "sin(";  break;
    case TOKEN_COS:     append = "cos(";  break;
    case TOKEN_TAN:     append = "tan(";  break;
    case TOKEN_ASIN:    append = "sin\xEE\x80\x81("; break;
    case TOKEN_ACOS:    append = "cos\xEE\x80\x81("; break;
    case TOKEN_ATAN:    append = "tan\xEE\x80\x81("; break;
    case TOKEN_LN:      append = "ln(";   break;
    case TOKEN_LOG:     append = "log(";  break;
    case TOKEN_SQRT:    append = "\xE2\x88\x9A("; break;
    case TOKEN_ABS:     append = "abs(";  break;
    case TOKEN_E_X:     append = "exp(";  break;
    case TOKEN_EE:      append = "*10^";  break;
    case TOKEN_ANS:     append = "ANS";   break;
    default:
        lvgl_lock(); ui_update_status_bar(); param_yeq_cursor_update(); lvgl_unlock();
        return true;
    }

    if (append && !s_param_yeq.on_equal) {
        ExprUtil_InsertStr(eq, &eq_len, &s_param_yeq.cursor_pos,
                           (uint8_t)(GRAPH_EQUATION_BUF_LEN - 1), append);
        lvgl_lock();
        lv_label_set_text(ui_lbl_param_eq[s_param_yeq.selected], eq);
        param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
    }

    lvgl_lock(); ui_update_status_bar(); param_yeq_cursor_update(); lvgl_unlock();
    return true;
}
