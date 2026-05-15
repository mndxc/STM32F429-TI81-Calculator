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
#include "graph_ui.h"
#include "ui_shared.h"
#include "calculator_core.h"
#include "calc_engine.h"
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

static MenuState_t draw_menu_state = {0};

lv_obj_t *ui_draw_screen = NULL;

/* Item list labels */
static lv_obj_t *draw_item_labels[DRAW_ITEM_COUNT];

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Draw_ShowScreen(void) { lv_obj_clear_flag(ui_draw_screen, LV_OBJ_FLAG_HIDDEN); }
void Draw_HideScreen(void) { lv_obj_add_flag(ui_draw_screen,   LV_OBJ_FLAG_HIDDEN); }

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
            (i == (int)draw_menu_state.cursor)
            ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
    }
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

/* Returns true when the DRAW menu was opened from an on-graph mode, meaning
 * the user wants interactive cursor-pick rather than expression-buffer insertion. */
static bool is_graph_context(CalcMode_t m)
{
    return m == MODE_GRAPH_FREE_CURSOR
        || m == MODE_GRAPH_TRACE
        || m == MODE_GRAPH_ZBOX
        || m == MODE_GRAPH_ZOOM_CURSOR
        || m == MODE_GRAPH_DRAW_CURSOR;
}

/** Execute or insert the item at draw_menu_state.cursor. */
static void draw_menu_select(void)
{
    uint8_t item = draw_menu_state.cursor;

    if (item == 0) {
        /* ClrDraw — immediate: clear layer, re-render if graph was visible */
        Graph_DrawLayerClear();
        if (Graph_IsVisible()) {
            Graph_Render();
        }
        menu_close(TOKEN_DRAW);
        Update_Calculator_Display();
        return;
    }

    /* Items 2–5 (Line(, PT-On(, PT-Off(, PT-Chg(): enter interactive cursor-pick
     * when the menu was opened from a graph canvas mode (guidebook p. 5-2/5-5/5-6).
     * op encoding: item 1→op 2 (Line(), item 2→op 3 (PT-On(), etc. */
    if (item <= 4 && is_graph_context(draw_menu_state.return_mode)) {
        draw_enter_cursor_pick((uint8_t)(item + 1));
        return;
    }

    /* All other cases (items 6–7, or items 2–5 from expression editor):
     * insert token text and return to the calling editor. */
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

/* Extract one raw argument string from *p (depth-aware, stops at top-level
 * comma or ')').  Trims surrounding whitespace, writes to out, advances *p
 * past the argument and its trailing comma if present.
 * Returns true on success, false if the argument is empty or overflows out. */
static bool shade_extract_str(const char **p, char *out, size_t out_size)
{
    while (**p == ' ') (*p)++;
    const char *start = *p;
    uint8_t depth = 0;
    while (**p) {
        if (**p == '(') { depth++; (*p)++; continue; }
        if (**p == ')') { if (depth == 0) break; depth--; (*p)++; continue; }
        if (**p == ',' && depth == 0) break;
        (*p)++;
    }
    const char *end = *p;
    while (end > start && *(end - 1) == ' ') end--;
    size_t len = (size_t)(end - start);
    if (len == 0 || len >= out_size) return false;
    memcpy(out, start, len);
    out[len] = '\0';
    if (**p == ',') (*p)++;
    return true;
}

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
    CalcResult_t r = Calc_Evaluate(buf, Calc_GetAns(), Calc_GetAnsIsMatrix(), Calc_GetAngleDegrees());
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
    if (strcmp(Calc_GetExprBuf(), "ClrDraw") == 0) {
        Graph_DrawLayerClear();
        if (Graph_IsVisible())
            Graph_Render();
        return true;
    }

    /* Line(x1,y1,x2,y2) */
    if (strncmp(Calc_GetExprBuf(), "Line(", 5) == 0) {
        const char *p = Calc_GetExprBuf() + 4;
        float args[4] = {0};
        if (parse_draw_args(&p, args, 4) == 4) {
            int32_t px1 = Graph_MathXToPx(args[0]);
            int32_t py1 = Graph_MathYToPx(args[1]);
            int32_t px2 = Graph_MathXToPx(args[2]);
            int32_t py2 = Graph_MathYToPx(args[3]);
            Graph_DrawLayerLine(px1, py1, px2, py2, draw_white);
            if (Graph_IsVisible())
                Graph_Render();
        }
        return true;
    }

    /* PT-On(x,y) */
    if (strncmp(Calc_GetExprBuf(), "PT-On(", 6) == 0) {
        const char *p = Calc_GetExprBuf() + 5;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            Graph_DrawLayerSetPixel(Graph_MathXToPx(args[0]),
                                    Graph_MathYToPx(args[1]), draw_white);
            if (Graph_IsVisible())
                Graph_Render();
        }
        return true;
    }

    /* PT-Off(x,y) */
    if (strncmp(Calc_GetExprBuf(), "PT-Off(", 7) == 0) {
        const char *p = Calc_GetExprBuf() + 6;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            Graph_DrawLayerSetPixel(Graph_MathXToPx(args[0]),
                                    Graph_MathYToPx(args[1]), 0x0000);
            if (Graph_IsVisible())
                Graph_Render();
        }
        return true;
    }

    /* PT-Chg(x,y) */
    if (strncmp(Calc_GetExprBuf(), "PT-Chg(", 7) == 0) {
        const char *p = Calc_GetExprBuf() + 6;
        float args[2] = {0};
        if (parse_draw_args(&p, args, 2) >= 2) {
            int32_t px = Graph_MathXToPx(args[0]);
            int32_t py = Graph_MathYToPx(args[1]);
            uint16_t cur = Graph_DrawLayerGetPixel(px, py);
            Graph_DrawLayerSetPixel(px, py, cur ? 0x0000 : draw_white);
            if (Graph_IsVisible())
                Graph_Render();
        }
        return true;
    }

    /* DrawF <expr> */
    if (strncmp(Calc_GetExprBuf(), "DrawF ", 6) == 0) {
        const char *expr_part = Calc_GetExprBuf() + 6;
        if (strlen(expr_part) > 0) {
            Graph_DrawF(expr_part, draw_white);
            if (Graph_IsVisible())
                Graph_Render();
        }
        return true;
    }

    /* Shade(lowerfunc, upperfunc [, resolution, Xbeg, Xend]) */
    if (strncmp(Calc_GetExprBuf(), "Shade(", 6) == 0) {
        const char *p = Calc_GetExprBuf() + 6;   /* point past '(' */
        char func_lo[64], func_hi[64];
        if (shade_extract_str(&p, func_lo, sizeof(func_lo)) &&
            shade_extract_str(&p, func_hi, sizeof(func_hi))) {
            /* Default optional scalars from graph state */
            const GraphState_t *gs = Graph_GetState();
            float scalar_args[3] = { gs->x_res, gs->x_min, gs->x_max };
            uint8_t ns = 0;
            while (*p && *p != ')' && ns < 3) {
                const char *start = p;
                uint8_t depth = 0;
                while (*p) {
                    if (*p == '(') { depth++; p++; continue; }
                    if (*p == ')') { if (depth == 0) break; depth--; p++; continue; }
                    if (*p == ',' && depth == 0) break;
                    p++;
                }
                scalar_args[ns++] = eval_draw_arg(start, p);
                if (*p == ',') p++;
            }
            int resolution = (int)scalar_args[0];
            if (resolution < 1) resolution = 1;
            if (resolution > 8) resolution = 8;
            Graph_Shade(func_lo, func_hi, resolution,
                        scalar_args[1], scalar_args[2],
                        shade_grey);
            if (Graph_IsVisible())
                Graph_Render();
        }
        return true;
    }

    return false;
}

bool handle_draw_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        MenuState_MoveUp(&draw_menu_state, DRAW_ITEM_COUNT, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_draw_display();
        lvgl_unlock();
        return true;

    case TOKEN_DOWN:
        MenuState_MoveDown(&draw_menu_state, DRAW_ITEM_COUNT, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_draw_display();
        lvgl_unlock();
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
                draw_menu_state.cursor = (uint8_t)i;
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

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

void Draw_MenuOpen(CalcMode_t return_to)
{
    draw_menu_state.return_mode = return_to;
    draw_menu_state.cursor = 0;
    Calc_SetMode(MODE_DRAW_MENU);
    Draw_ShowScreen();
    ui_update_draw_display();
}

CalcMode_t Draw_MenuClose(void)
{
    CalcMode_t ret              = draw_menu_state.return_mode;
    draw_menu_state.return_mode = MODE_NORMAL;
    draw_menu_state.cursor = 0;
    return ret;
}
