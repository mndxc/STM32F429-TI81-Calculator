/**
 * @file    graph.c
 * @brief   Graphing subsystem — renders Y= equations using LVGL canvas.
 */
#include "graph.h"
#include "app_common.h"
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
static lv_obj_t *graph_lbl_eq  = NULL;  /* Equation label at top */
static lv_obj_t *graph_lbl_xy  = NULL;  /* X/Y readout at bottom for trace */

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
    lv_color_t axis_color = lv_color_hex(0x888888);

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
 * @brief Draws tick marks along the axes at x_scl and y_scl intervals.
 */
static void draw_ticks(void)
{
    lv_color_t tick_color = lv_color_hex(0x555555);
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

/*---------------------------------------------------------------------------
 * Public functions
 *--------------------------------------------------------------------------*/

void Graph_Init(lv_obj_t *parent)
{
    /* Container screen */
    graph_screen = lv_obj_create(parent);
    lv_obj_set_size(graph_screen, GRAPH_W, GRAPH_H + 20);
    lv_obj_set_pos(graph_screen, 0, 0);
    lv_obj_set_style_bg_color(graph_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(graph_screen, 0, 0);
    lv_obj_set_style_pad_all(graph_screen, 0, 0);
    lv_obj_add_flag(graph_screen, LV_OBJ_FLAG_HIDDEN);

    /* Equation label at top */
    graph_lbl_eq = lv_label_create(graph_screen);
    lv_obj_set_pos(graph_lbl_eq, 2, 2);
    lv_obj_set_style_text_font(graph_lbl_eq, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(graph_lbl_eq, lv_color_hex(0x888888), 0);
    lv_label_set_text(graph_lbl_eq, "Y=");

    /* Canvas */
    graph_canvas = lv_canvas_create(graph_screen);
    lv_canvas_set_buffer(graph_canvas, graph_buf, GRAPH_W, GRAPH_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(graph_canvas, 0, 20);

    /* X/Y trace label at bottom */
    graph_lbl_xy = lv_label_create(graph_screen);
    lv_obj_set_pos(graph_lbl_xy, 2, GRAPH_H + 2);
    lv_obj_set_style_text_font(graph_lbl_xy, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(graph_lbl_xy, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(graph_lbl_xy, "");
}

void Graph_Render(bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    /* Clear canvas to black */
    lv_canvas_fill_bg(graph_canvas, lv_color_hex(0x000000), LV_OPA_COVER);

    /* Draw axes and ticks */
    draw_axes();
    draw_ticks();

    /* One color per Y= slot */
    static const uint32_t eq_palette[GRAPH_NUM_EQ] = {
        0xFFFFFF,   /* Y1 — white   */
        0x00FFFF,   /* Y2 — cyan    */
        0xFFFF00,   /* Y3 — yellow  */
        0xFF80FF,   /* Y4 — magenta */
    };

    /* Build label from active slots and plot each curve */
    char eq_label[24] = "";
    const char *eq_names[] = { "Y1 ", "Y2 ", "Y3 ", "Y4 " };

    for (uint8_t eq = 0; eq < GRAPH_NUM_EQ; eq++) {
        const char *eqstr = graph_state.equations[eq];
        if (strlen(eqstr) == 0) continue;

        strncat(eq_label, eq_names[eq], sizeof(eq_label) - strlen(eq_label) - 1);

        lv_color_t curve_color = lv_color_hex(eq_palette[eq]);
        int32_t prev_py    = -1;
        bool    prev_valid = false;

        for (int32_t px = 0; px < GRAPH_W; px++) {
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
                int32_t y_start = prev_py < py ? prev_py : py;
                int32_t y_end   = prev_py < py ? py : prev_py;
                for (int32_t y = y_start; y <= y_end; y++)
                    lv_canvas_set_px(graph_canvas, px, y, curve_color, LV_OPA_COVER);
            } else {
                lv_canvas_set_px(graph_canvas, px, py, curve_color, LV_OPA_COVER);
            }

            prev_py    = py;
            prev_valid = true;
        }
    }

    /* Cache the clean frame so Graph_DrawTrace can restore it without re-rendering */
    memcpy(graph_buf_clean, graph_buf, (size_t)GRAPH_W * GRAPH_H * 2);
    graph_clean_valid = true;

    lv_label_set_text(graph_lbl_eq, eq_label);
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

    char x_buf[16], y_buf[16], label_buf[48];

    if (strlen(eqstr) == 0) {
        Calc_FormatResult(x, x_buf, sizeof(x_buf));
        snprintf(label_buf, sizeof(label_buf), "X=%s", x_buf);
        lv_label_set_text(graph_lbl_xy, label_buf);
        return;
    }

    CalcResult_t r = Calc_EvaluateAt(eqstr, x, 0.0f, angle_degrees);

    Calc_FormatResult(x, x_buf, sizeof(x_buf));

    if (r.error != CALC_OK || isnan(r.value) || isinf(r.value)) {
        snprintf(label_buf, sizeof(label_buf), "X=%s  Y=undef", x_buf);
        lv_label_set_text(graph_lbl_xy, label_buf);
        return;
    }

    Calc_FormatResult(r.value, y_buf, sizeof(y_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%-12s Y=%s", x_buf, y_buf);
    lv_label_set_text(graph_lbl_xy, label_buf);

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
    if (graph_lbl_xy != NULL)
        lv_label_set_text(graph_lbl_xy, "");
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

    lv_color_t box_color = lv_color_hex(0xFFFFFF);  /* white rectangle */
    lv_color_t cur_color = lv_color_hex(0xFFFF00);  /* yellow crosshair */

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
    char x_buf[16], y_buf[16], label_buf[48];
    Calc_FormatResult(px_to_math_x(px), x_buf, sizeof(x_buf));
    Calc_FormatResult(px_to_math_y(py), y_buf, sizeof(y_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%-12s Y=%s", x_buf, y_buf);
    lv_label_set_text(graph_lbl_xy, label_buf);
}