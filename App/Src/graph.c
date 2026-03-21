/**
 * @file    graph.c
 * @brief   Graphing subsystem — renders Y= equations using LVGL canvas.
 */
#include "graph.h"
#include "app_common.h"
#include "ui_palette.h"
#include "calc_engine.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Private variables
 *--------------------------------------------------------------------------*/

/* Pixel buffer for the canvas — RGB565, one uint16_t per pixel */
static uint16_t * const graph_buf       = (uint16_t *)0xD0025800;

/* Clean-frame cache for trace: SDRAM immediately after graph_buf */
static uint16_t * const graph_buf_clean = (uint16_t *)(0xD0025800 + GRAPH_W * GRAPH_H * 2);
static bool graph_clean_valid = false;

static lv_obj_t *graph_screen  = NULL;
static lv_obj_t *graph_canvas  = NULL;
static lv_obj_t *graph_lbl_x   = NULL;  /* X= readout, bottom-left of canvas */
static lv_obj_t *graph_lbl_y   = NULL;  /* Y= readout, bottom-right of canvas */

/*---------------------------------------------------------------------------
 * Private helpers
 *--------------------------------------------------------------------------*/

/**
 * @brief Maps a math x coordinate to a canvas pixel column.
 */
static int32_t math_x_to_px(float x)
{
    return (int32_t)((x - graph_state.x_min) /
                     (graph_state.x_max - graph_state.x_min) * (GRAPH_W - 1));
}

/**
 * @brief Maps a math y coordinate to a canvas pixel row.
 *        Note: pixel y increases downward, math y increases upward.
 */
static int32_t math_y_to_px(float y)
{
    return (int32_t)((graph_state.y_max - y) /
                     (graph_state.y_max - graph_state.y_min) * (GRAPH_H - 1));
}

/**
 * @brief Maps a canvas pixel column to a math x coordinate (inverse of math_x_to_px).
 */
static float px_to_math_x(int32_t px)
{
    return graph_state.x_min +
           (float)px / (float)(GRAPH_W - 1) *
           (graph_state.x_max - graph_state.x_min);
}

/**
 * @brief Maps a canvas pixel row to a math y coordinate (inverse of math_y_to_px).
 */
static float px_to_math_y(int32_t py)
{
    return graph_state.y_max -
           (float)py / (float)(GRAPH_H - 1) *
           (graph_state.y_max - graph_state.y_min);
}

/**
 * @brief Draws the X and Y axes if they fall within the current window.
 */
static void draw_axes(void)
{
    lv_color_t axis_color = lv_color_hex(COLOR_GREY_MED);

    /* Y axis — vertical line at x=0 */
    if (graph_state.x_min <= 0.0f && graph_state.x_max >= 0.0f) {
        int32_t px = math_x_to_px(0.0f);
        for (int32_t y = 0; y < GRAPH_H; y++) {
            lv_canvas_set_px(graph_canvas, px, y, axis_color, LV_OPA_COVER);
        }
    }

    /* X axis — horizontal line at y=0 */
    if (graph_state.y_min <= 0.0f && graph_state.y_max >= 0.0f) {
        int32_t py = math_y_to_px(0.0f);
        for (int32_t x = 0; x < GRAPH_W; x++) {
            lv_canvas_set_px(graph_canvas, x, py, axis_color, LV_OPA_COVER);
        }
    }
}

/**
 * @brief Draws grid dots at every (x_scl, y_scl) intersection (TI-81 style).
 *        Only drawn when graph_state.grid_on is true.
 */
static void draw_grid(void)
{
    lv_color_t grid_color = lv_color_hex(COLOR_GREY_DARK);

    /* Anchor at multiples of x_scl/y_scl (same as draw_ticks) so dots stay
     * aligned with tick marks after zooming. */
    float gx_start = ceilf(graph_state.x_min / graph_state.x_scl) * graph_state.x_scl;
    float gy_start = ceilf(graph_state.y_min / graph_state.y_scl) * graph_state.y_scl;

    for (float gy = gy_start; gy <= graph_state.y_max + 1e-6f; gy += graph_state.y_scl) {
        int32_t py = math_y_to_px(gy);
        if (py < 0 || py >= GRAPH_H) continue;
        for (float gx = gx_start; gx <= graph_state.x_max + 1e-6f; gx += graph_state.x_scl) {
            int32_t px = math_x_to_px(gx);
            if (px < 0 || px >= GRAPH_W) continue;
            lv_canvas_set_px(graph_canvas, px, py, grid_color, LV_OPA_COVER);
        }
    }
}

/**
 * @brief Draws tick marks along the axes at x_scl and y_scl intervals.
 */
static void draw_ticks(void)
{
    lv_color_t tick_color = lv_color_hex(COLOR_GREY_TICK);
    const int32_t TICK_LEN = 3;

    /* X axis ticks */
    if (graph_state.y_min <= 0.0f && graph_state.y_max >= 0.0f) {
        int32_t py = math_y_to_px(0.0f);
        for (float x = 0.0f; x <= graph_state.x_max; x += graph_state.x_scl) {
            int32_t px = math_x_to_px(x);
            for (int32_t t = -TICK_LEN; t <= TICK_LEN; t++)
                lv_canvas_set_px(graph_canvas, px, py + t, tick_color, LV_OPA_COVER);
        }
        for (float x = 0.0f; x >= graph_state.x_min; x -= graph_state.x_scl) {
            int32_t px = math_x_to_px(x);
            for (int32_t t = -TICK_LEN; t <= TICK_LEN; t++)
                lv_canvas_set_px(graph_canvas, px, py + t, tick_color, LV_OPA_COVER);
        }
    }

    /* Y axis ticks */
    if (graph_state.x_min <= 0.0f && graph_state.x_max >= 0.0f) {
        int32_t px = math_x_to_px(0.0f);
        for (float y = 0.0f; y <= graph_state.y_max; y += graph_state.y_scl) {
            int32_t py = math_y_to_px(y);
            for (int32_t t = -TICK_LEN; t <= TICK_LEN; t++)
                lv_canvas_set_px(graph_canvas, px + t, py, tick_color, LV_OPA_COVER);
        }
        for (float y = 0.0f; y >= graph_state.y_min; y -= graph_state.y_scl) {
            int32_t py = math_y_to_px(y);
            for (int32_t t = -TICK_LEN; t <= TICK_LEN; t++)
                lv_canvas_set_px(graph_canvas, px + t, py, tick_color, LV_OPA_COVER);
        }
    }
}

/**
 * @brief Formats a graph coordinate value for the on-screen readout.
 *
 * Guarantees the output string is at most 10 characters, so the full
 * "X=<value>" or "Y=<value>" label never exceeds 12 chars (144 px at
 * 12 px/char with JetBrains Mono 20) and cannot overlap the sibling label.
 *
 * Strategy:
 *   |val| in [0.001, 1000)  →  "%.4f" with trailing-zero trim  (max 9 chars)
 *   everything else         →  "%.3e" with leading-zero stripped from exponent
 *                              (max 10 chars, e.g. "-1.490e+38")
 */
static void format_graph_coord(float val, char *buf, size_t len)
{
    if (val == 0.0f) {
        snprintf(buf, len, "0");
        return;
    }

    float absval = fabsf(val);

    if (absval >= 0.001f && absval < 1000.0f) {
        /* Near-integer: no decimal point needed */
        float rounded = roundf(val);
        if (fabsf(val - rounded) < 5e-5f) {
            snprintf(buf, len, "%d", (int)rounded);
            return;
        }
        snprintf(buf, len, "%.4f", val);
        char *dot = strchr(buf, '.');
        if (dot != NULL) {
            char *end = buf + strlen(buf) - 1;
            while (end > dot && *end == '0') *end-- = '\0';
            if (*end == '.') *end = '\0';
        }
    } else {
        snprintf(buf, len, "%.3e", val);
        /* Strip leading zero from exponent: "e+08" → "e+8", "e-08" → "e-8" */
        char *e = strchr(buf, 'e');
        if (e != NULL && (e[1] == '+' || e[1] == '-') && e[2] == '0' && e[3] != '\0')
            memmove(&e[2], &e[3], strlen(&e[3]) + 1);
    }
}

/*---------------------------------------------------------------------------
 * Public functions
 *--------------------------------------------------------------------------*/

void Graph_Init(lv_obj_t *parent)
{
    /* Container screen */
    graph_screen = lv_obj_create(parent);
    lv_obj_set_size(graph_screen, GRAPH_W, GRAPH_H);
    lv_obj_set_pos(graph_screen, 0, 0);
    lv_obj_set_style_bg_color(graph_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(graph_screen, 0, 0);
    lv_obj_set_style_pad_all(graph_screen, 0, 0);
    lv_obj_add_flag(graph_screen, LV_OBJ_FLAG_HIDDEN);

    /* Canvas — full height from top, no top bar */
    graph_canvas = lv_canvas_create(graph_screen);
    lv_canvas_set_buffer(graph_canvas, graph_buf, GRAPH_W, GRAPH_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(graph_canvas, 0, 0);

    /* X= and Y= coordinate readouts — overlaid side by side at bottom of canvas */
    graph_lbl_x = lv_label_create(graph_screen);
    lv_obj_set_pos(graph_lbl_x, 4, GRAPH_H - 22);
    lv_obj_set_style_text_font(graph_lbl_x, &jetbrains_mono_20, 0);
    lv_obj_set_style_text_color(graph_lbl_x, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_bg_color(graph_lbl_x, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(graph_lbl_x, LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(graph_lbl_x, 3, 0);
    lv_label_set_text(graph_lbl_x, "");

    graph_lbl_y = lv_label_create(graph_screen);
    lv_obj_set_pos(graph_lbl_y, GRAPH_W / 2, GRAPH_H - 22);
    lv_obj_set_style_text_font(graph_lbl_y, &jetbrains_mono_20, 0);
    lv_obj_set_style_text_color(graph_lbl_y, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_bg_color(graph_lbl_y, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(graph_lbl_y, LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(graph_lbl_y, 3, 0);
    lv_label_set_text(graph_lbl_y, "");
}

void Graph_Render(bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    /* Clear canvas to black */
    lv_canvas_fill_bg(graph_canvas, lv_color_hex(COLOR_BLACK), LV_OPA_COVER);

    /* Draw grid dots (if enabled), then axes and ticks on top */
    if (graph_state.grid_on) draw_grid();
    draw_axes();
    draw_ticks();

    /* One color per Y= slot */
    static const uint32_t eq_palette[GRAPH_NUM_EQ] = {
        COLOR_CURVE_Y1,   /* Y1 — white   */
        COLOR_CURVE_Y2,   /* Y2 — cyan    */
        COLOR_CURVE_Y3,   /* Y3 — yellow  */
        COLOR_CURVE_Y4,   /* Y4 — magenta */
    };

    /* Plot each active curve */
    for (uint8_t eq = 0; eq < GRAPH_NUM_EQ; eq++) {
        const char *eqstr = graph_state.equations[eq];
        if (strlen(eqstr) == 0) continue;

        lv_color_t curve_color = lv_color_hex(eq_palette[eq]);
        int32_t prev_py    = -1;
        int32_t prev_px    = -1;
        bool    prev_valid = false;

        int32_t step = (int32_t)graph_state.x_res;
        if (step < 1) step = 1;

        for (int32_t px = 0; px < GRAPH_W; px += step) {
            float x = graph_state.x_min +
                      (float)px / (float)(GRAPH_W - 1) *
                      (graph_state.x_max - graph_state.x_min);

            CalcResult_t r = Calc_EvaluateAt(eqstr, x, 0.0f, angle_degrees);

            if (r.error != CALC_OK || isnan(r.value) || isinf(r.value)) {
                prev_valid = false;
                continue;
            }

            int32_t py = math_y_to_px(r.value);

            if (py < 0 || py >= GRAPH_H) {
                prev_valid = false;
                continue;
            }

            if (prev_valid) {
                /* Interpolate across all columns from prev_px+1 to px */
                int32_t span = px - prev_px;
                int32_t last_interp = prev_py;
                for (int32_t cx = prev_px + 1; cx <= px; cx++) {
                    int32_t cur_interp = prev_py + (int32_t)((float)(cx - prev_px) / (float)span * (py - prev_py));
                    int32_t y_start = last_interp < cur_interp ? last_interp : cur_interp;
                    int32_t y_end   = last_interp < cur_interp ? cur_interp : last_interp;
                    for (int32_t y = y_start; y <= y_end; y++)
                        if (y >= 0 && y < GRAPH_H)
                            lv_canvas_set_px(graph_canvas, cx, y, curve_color, LV_OPA_COVER);
                    last_interp = cur_interp;
                }
            } else {
                lv_canvas_set_px(graph_canvas, px, py, curve_color, LV_OPA_COVER);
            }

            prev_py    = py;
            prev_px    = px;
            prev_valid = true;
        }
    }

    /* Cache the clean frame so Graph_DrawTrace can restore it without re-rendering */
    memcpy(graph_buf_clean, graph_buf, (size_t)GRAPH_W * GRAPH_H * 2);
    graph_clean_valid = true;
}

void Graph_SetVisible(bool visible)
{
    if (graph_screen == NULL) return;
    graph_state.active = visible;
    if (visible) {
        lv_obj_clear_flag(graph_screen, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(graph_screen, LV_OBJ_FLAG_HIDDEN);
        graph_clean_valid = false;
    }
}

void Graph_DrawTrace(float x, uint8_t eq_idx, bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    /* Restore the clean frame — avoids a full 320-column re-render on every step */
    if (graph_clean_valid) {
        memcpy(graph_buf, graph_buf_clean, (size_t)GRAPH_W * GRAPH_H * 2);
    } else {
        Graph_Render(angle_degrees);
    }

    const char *eqstr = graph_state.equations[eq_idx];

    char x_buf[16], y_buf[16], label_buf[20];

    format_graph_coord(x, x_buf, sizeof(x_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%s", x_buf);
    lv_label_set_text(graph_lbl_x, label_buf);

    if (strlen(eqstr) == 0) {
        lv_label_set_text(graph_lbl_y, "");
        return;
    }

    CalcResult_t r = Calc_EvaluateAt(eqstr, x, 0.0f, angle_degrees);

    if (r.error != CALC_OK || isnan(r.value) || isinf(r.value)) {
        lv_label_set_text(graph_lbl_y, "Y=undef");
        return;
    }

    format_graph_coord(r.value, y_buf, sizeof(y_buf));
    snprintf(label_buf, sizeof(label_buf), "Y=%s", y_buf);
    lv_label_set_text(graph_lbl_y, label_buf);

    int32_t px = math_x_to_px(x);
    int32_t py = math_y_to_px(r.value);

    /* Draw crosshair in bright green — visible against all curve colors */
    lv_color_t cur = lv_color_hex(0x00FF00);
    const int32_t ARM = 5;

    for (int32_t dx = -ARM; dx <= ARM; dx++) {
        int32_t cx = px + dx;
        if (cx >= 0 && cx < GRAPH_W && py >= 0 && py < GRAPH_H)
            lv_canvas_set_px(graph_canvas, cx, py, cur, LV_OPA_COVER);
    }
    for (int32_t dy = -ARM; dy <= ARM; dy++) {
        int32_t cy = py + dy;
        if (cy >= 0 && cy < GRAPH_H && px >= 0 && px < GRAPH_W)
            lv_canvas_set_px(graph_canvas, px, cy, cur, LV_OPA_COVER);
    }
}

void Graph_ClearTrace(void)
{
    if (graph_lbl_x != NULL) lv_label_set_text(graph_lbl_x, "");
    if (graph_lbl_y != NULL) lv_label_set_text(graph_lbl_y, "");
}

void Graph_DrawZBox(int32_t px, int32_t py,
                    int32_t px1, int32_t py1,
                    bool corner1_set, bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    /* Restore the clean frame — avoids a full re-render on every cursor step */
    if (graph_clean_valid) {
        memcpy(graph_buf, graph_buf_clean, (size_t)GRAPH_W * GRAPH_H * 2);
    } else {
        Graph_Render(angle_degrees);
    }

    lv_color_t box_color = lv_color_hex(COLOR_WHITE);  /* white rectangle */
    lv_color_t cur_color = lv_color_hex(COLOR_YELLOW);  /* yellow crosshair */

    if (corner1_set) {
        /* Normalise corners so x0 <= x1 and y0 <= y1 */
        int32_t x0 = px1 < px ? px1 : px;
        int32_t x1c = px1 < px ? px  : px1;
        int32_t y0 = py1 < py ? py1 : py;
        int32_t y1c = py1 < py ? py  : py1;

        /* Top and bottom edges */
        for (int32_t x = x0; x <= x1c; x++) {
            if (x >= 0 && x < GRAPH_W) {
                if (y0 >= 0 && y0 < GRAPH_H)
                    lv_canvas_set_px(graph_canvas, x, y0, box_color, LV_OPA_COVER);
                if (y1c >= 0 && y1c < GRAPH_H)
                    lv_canvas_set_px(graph_canvas, x, y1c, box_color, LV_OPA_COVER);
            }
        }
        /* Left and right edges */
        for (int32_t y = y0; y <= y1c; y++) {
            if (y >= 0 && y < GRAPH_H) {
                if (x0 >= 0 && x0 < GRAPH_W)
                    lv_canvas_set_px(graph_canvas, x0, y, box_color, LV_OPA_COVER);
                if (x1c >= 0 && x1c < GRAPH_W)
                    lv_canvas_set_px(graph_canvas, x1c, y, box_color, LV_OPA_COVER);
            }
        }
    }

    /* Draw crosshair at the moving cursor */
    const int32_t ARM = 4;
    for (int32_t dx = -ARM; dx <= ARM; dx++) {
        int32_t cx = px + dx;
        if (cx >= 0 && cx < GRAPH_W && py >= 0 && py < GRAPH_H)
            lv_canvas_set_px(graph_canvas, cx, py, cur_color, LV_OPA_COVER);
    }
    for (int32_t dy = -ARM; dy <= ARM; dy++) {
        int32_t cy = py + dy;
        if (cy >= 0 && cy < GRAPH_H && px >= 0 && px < GRAPH_W)
            lv_canvas_set_px(graph_canvas, px, cy, cur_color, LV_OPA_COVER);
    }

    /* Update X/Y readout with math coordinates of current cursor */
    char x_buf[16], y_buf[16], label_buf[20];
    format_graph_coord(px_to_math_x(px), x_buf, sizeof(x_buf));
    format_graph_coord(px_to_math_y(py), y_buf, sizeof(y_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%s", x_buf);
    lv_label_set_text(graph_lbl_x, label_buf);
    snprintf(label_buf, sizeof(label_buf), "Y=%s", y_buf);
    lv_label_set_text(graph_lbl_y, label_buf);
}