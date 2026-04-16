/**
 * @file    ui_draw.c
 * @brief   DRAW menu UI (2nd+PRGM).
 *
 * Single-list menu, 7 items, no tabs:
 *   1:ClrDraw  — executes immediately (clears draw layer, re-renders graph)
 *   2:Line(    — inserts "Line(" into expression buffer
 *   3:PT-On(   — inserts "PT-On("
 *   4:PT-Off(  — inserts "PT-Off("
 *   5:PT-Chg(  — inserts "PT-Chg("
 *   6:DrawF    — inserts "DrawF "
 *   7:Shade(   — inserts "Shade("
 *
 * Items 2–7 return the user to the expression editor (or Y= editor if opened
 * from there) to complete the argument list before pressing ENTER.
 */

#include "ui_draw.h"
#include "calc_internal.h"
#include "graph.h"
#include "graph_draw.h"
#include "ui_palette.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define DRAW_ITEM_COUNT  7

static const char * const draw_item_names[DRAW_ITEM_COUNT] = {
    "1:ClrDraw",
    "2:Line(",
    "3:PT-On(",
    "4:PT-Off(",
    "5:PT-Chg(",
    "6:DrawF",
    "7:Shade(",
};

/* Text inserted into the expression buffer for items 2–7 (item 0 = ClrDraw,
 * executed immediately, so no insertion string). */
static const char * const draw_item_insert[DRAW_ITEM_COUNT] = {
    NULL,        /* 1:ClrDraw — immediate action */
    "Line(",
    "PT-On(",
    "PT-Off(",
    "PT-Chg(",
    "DrawF ",
    "Shade(",
};

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

DrawMenuState_t draw_menu_state = {0, MODE_NORMAL};

lv_obj_t *ui_draw_screen = NULL;

/* Item list labels */
static lv_obj_t *draw_item_labels[DRAW_ITEM_COUNT];

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_draw_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_draw_screen = screen_create(scr);

    /* Title label */
    lv_obj_t *title = lv_label_create(ui_draw_screen);
    lv_obj_set_pos(title, 4, 4);
    lv_obj_set_style_text_font(title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(title, "DRAW");

    /* Item list */
    for (int i = 0; i < DRAW_ITEM_COUNT; i++) {
        draw_item_labels[i] = lv_label_create(ui_draw_screen);
        lv_obj_set_pos(draw_item_labels[i], 4, 34 + i * 30);
        lv_obj_set_style_text_font(draw_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(draw_item_labels[i],
            lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(draw_item_labels[i], draw_item_names[i]);
    }
}

/*---------------------------------------------------------------------------
 * Display Update
 *---------------------------------------------------------------------------*/

void ui_update_draw_display(void)
{
    for (int i = 0; i < DRAW_ITEM_COUNT; i++) {
        lv_obj_set_style_text_color(draw_item_labels[i],
            (i == (int)draw_menu_state.item_cursor)
            ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
    }
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

/** Execute or insert the item at draw_menu_state.item_cursor. */
static void draw_menu_select(void)
{
    uint8_t item = draw_menu_state.item_cursor;

    if (item == 0) {
        /* ClrDraw — immediate: clear layer, re-render if graph was visible */
        Graph_DrawLayerClear();
        if (Graph_IsVisible()) {
            Graph_Render(angle_degrees);
        }
        menu_close(TOKEN_DRAW);
        Update_Calculator_Display();
        return;
    }

    /* Items 2–7: insert text then close menu */
    const char *ins = draw_item_insert[item];
    lvgl_lock();
    lv_obj_add_flag(ui_draw_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();
    menu_insert_text(ins, &draw_menu_state.return_mode);
}

/*---------------------------------------------------------------------------
 * DRAW command execution (from expression buffer, on TOKEN_ENTER)
 * Called by history_enter_evaluate() in calculator_core.c.
 *---------------------------------------------------------------------------*/

/* Evaluate a sub-expression string [start, end) as a float.
 * Used to evaluate individual arguments of DRAW commands. */
static float eval_draw_arg(const char *start, const char *end)
{
    char buf[32];
    /* Trim leading whitespace */
    while (start < end && *start == ' ') start++;
    size_t len = (size_t)(end - start);
    if (len == 0 || len >= sizeof(buf)) return 0.0f;
    memcpy(buf, start, len);
    buf[len] = '\0';
    CalcResult_t r = Calc_Evaluate(buf, ans, ans_is_matrix, angle_degrees);
    return (r.error == CALC_OK && !r.has_matrix) ? r.value : 0.0f;
}

/* Parse up to max_args comma-separated arguments from "(arg0,arg1,...)" at *p.
 * Handles nested parentheses so expressions like "sin(X)" work as arguments.
 * Advances *p past the closing ')'. Returns number of args parsed. */
static uint8_t parse_draw_args(const char **p, float *args, uint8_t max_args)
{
    if (**p != '(') return 0;
    (*p)++;
    uint8_t count = 0;
    while (**p && **p != ')' && count < max_args) {
        const char *start = *p;
        uint8_t depth = 0;
        while (**p) {
            if (**p == '(') { depth++; (*p)++; continue; }
            if (**p == ')') { if (depth == 0) break; depth--; (*p)++; continue; }
            if (**p == ',' && depth == 0) break;
            (*p)++;
        }
        args[count++] = eval_draw_arg(start, *p);
        if (**p == ',') (*p)++;
    }
    if (**p == ')') (*p)++;
    return count;
}

/* Try to execute a DRAW command in the expression buffer.
 * Returns true if the expression is a recognised DRAW command (caller shows
 * "Done" and clears the expression buffer); false if not a DRAW command. */
bool try_execute_draw_command(void)
{
    const uint16_t draw_white  = 0xFFFF;
    const uint16_t shade_grey  = 0x8410; /* mid-grey 50 % */

    /* ClrDraw */
    if (strcmp(expr.buf, "ClrDraw") == 0) {
        Graph_DrawLayerClear();
        if (Graph_IsVisible())
            Graph_Render(angle_degrees);
        return true;
    }

    /* Line(x1,y1,x2,y2) */
    if (strncmp(expr.buf, "Line(", 5) == 0) {
        const char *p = expr.buf + 4;
        float args[4] = {0};
        if (parse_draw_args(&p, args, 4) == 4) {
            int32_t px1 = Graph_MathXToPx(args[0]);
            int32_t py1 = Graph_MathYToPx(args[1]);
            int32_t px2 = Graph_MathXToPx(args[2]);
            int32_t py2 = Graph_MathYToPx(args[3]);
            Graph_DrawLayerLine(px1, py1, px2, py2, draw_white);
            if (Graph_IsVisible())
                Graph_Render(angle_degrees);
        }
        return true;
    }

    /* PT-On(x,y) */
    if (strncmp(expr.buf, "PT-On(", 6) == 0) {
        const char *p = expr.buf + 5;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            Graph_DrawLayerSetPixel(Graph_MathXToPx(args[0]),
                                    Graph_MathYToPx(args[1]), draw_white);
            if (Graph_IsVisible())
                Graph_Render(angle_degrees);
        }
        return true;
    }

    /* PT-Off(x,y) */
    if (strncmp(expr.buf, "PT-Off(", 7) == 0) {
        const char *p = expr.buf + 6;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            Graph_DrawLayerSetPixel(Graph_MathXToPx(args[0]),
                                    Graph_MathYToPx(args[1]), 0x0000);
            if (Graph_IsVisible())
                Graph_Render(angle_degrees);
        }
        return true;
    }

    /* PT-Chg(x,y) */
    if (strncmp(expr.buf, "PT-Chg(", 7) == 0) {
        const char *p = expr.buf + 6;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            int32_t px = Graph_MathXToPx(args[0]);
            int32_t py = Graph_MathYToPx(args[1]);
            uint16_t cur = Graph_DrawLayerGetPixel(px, py);
            Graph_DrawLayerSetPixel(px, py, cur ? 0x0000 : draw_white);
            if (Graph_IsVisible())
                Graph_Render(angle_degrees);
        }
        return true;
    }

    /* DrawF <expr> */
    if (strncmp(expr.buf, "DrawF ", 6) == 0) {
        const char *expr_part = expr.buf + 6;
        if (strlen(expr_part) > 0) {
            Graph_DrawF(expr_part, draw_white, angle_degrees);
            if (Graph_IsVisible())
                Graph_Render(angle_degrees);
        }
        return true;
    }

    /* Shade(yLow,yHigh) */
    if (strncmp(expr.buf, "Shade(", 6) == 0) {
        const char *p = expr.buf + 5;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            Graph_Shade(args[0], args[1], shade_grey);
            if (Graph_IsVisible())
                Graph_Render(angle_degrees);
        }
        return true;
    }

    return false;
}

bool handle_draw_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (draw_menu_state.item_cursor > 0) {
            draw_menu_state.item_cursor--;
            lvgl_lock();
            ui_update_draw_display();
            lvgl_unlock();
        }
        return true;

    case TOKEN_DOWN:
        if (draw_menu_state.item_cursor + 1 < DRAW_ITEM_COUNT) {
            draw_menu_state.item_cursor++;
            lvgl_lock();
            ui_update_draw_display();
            lvgl_unlock();
        }
        return true;

    case TOKEN_ENTER:
        draw_menu_select();
        return true;

    /* Digit shortcuts: 1–7 jump to that item and select it */
    case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4:
    case TOKEN_5: case TOKEN_6: case TOKEN_7: {
        static const Token_t digit_tok[7] = {
            TOKEN_1, TOKEN_2, TOKEN_3, TOKEN_4, TOKEN_5, TOKEN_6, TOKEN_7
        };
        for (int i = 0; i < DRAW_ITEM_COUNT; i++) {
            if (t == digit_tok[i]) {
                draw_menu_state.item_cursor = (uint8_t)i;
                draw_menu_select();
                break;
            }
        }
        return true;
    }

    case TOKEN_CLEAR:
        menu_close(TOKEN_DRAW);
        Update_Calculator_Display();
        return true;

    default:
        return false;
    }
}
