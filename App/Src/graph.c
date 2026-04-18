/**
 * @file    graph.c
 * @brief   Graphing subsystem — renders Y= equations using LVGL canvas.
 */
#include "graph.h"
/* Note: app_common.h is included transitively via graph.h */
#include "graph_draw.h"
#include "ui_palette.h"
#include "calc_engine.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Graph state — owned here; external access via Graph_GetState() / setters
 *--------------------------------------------------------------------------*/

/* Sole definition; all external access goes through Graph_GetState() and the
 * write accessors declared in graph.h. */
static GraphState_t graph_state = {
    .equations  = {{0}},
    .x_min      = -10.0f,
    .x_max      =  10.0f,
    .y_min      = -10.0f,
    .y_max      =  10.0f,
    .x_scl      =   1.0f,
    .y_scl      =   1.0f,
    .x_res      =   1.0f,
    .active     = false,
    .t_min      =   0.0f,
    .t_max      =   6.2832f,   /* 2π */
    .t_step     =   0.1309f,   /* π/24 */
    .param_mode = false,
};

/*---------------------------------------------------------------------------
 * Graph state accessor implementations
 *--------------------------------------------------------------------------*/

const GraphState_t *Graph_GetState(void) { return &graph_state; }

void Graph_SetEquationEnabled(uint8_t idx, bool enabled)
{
    if (idx < GRAPH_NUM_EQ) graph_state.enabled[idx] = enabled;
}

void Graph_SetWindow(float xmin, float xmax, float ymin, float ymax,
                     float xscl, float yscl, float xres)
{
    graph_state.x_min = xmin;
    graph_state.x_max = xmax;
    graph_state.y_min = ymin;
    graph_state.y_max = ymax;
    graph_state.x_scl = xscl;
    graph_state.y_scl = yscl;
    graph_state.x_res = xres;
}

void Graph_SetParamEnabled(uint8_t idx, bool enabled)
{
    if (idx < GRAPH_NUM_PARAM) graph_state.param_enabled[idx] = enabled;
}

void Graph_SetParamWindow(float tmin, float tmax, float tstep)
{
    graph_state.t_min  = tmin;
    graph_state.t_max  = tmax;
    graph_state.t_step = tstep;
}

void Graph_SetParamMode(bool param) { graph_state.param_mode = param; }
void Graph_SetGridOn(bool on)       { graph_state.grid_on    = on;    }
void Graph_SetActive(bool active)   { graph_state.active     = active; }

char *Graph_GetEquationBuf(uint8_t idx)
{
    if (idx < GRAPH_NUM_EQ) return graph_state.equations[idx];
    return NULL;
}

char *Graph_GetParamEquationXBuf(uint8_t pair)
{
    if (pair < GRAPH_NUM_PARAM) return graph_state.param_x[pair];
    return NULL;
}

char *Graph_GetParamEquationYBuf(uint8_t pair)
{
    if (pair < GRAPH_NUM_PARAM) return graph_state.param_y[pair];
    return NULL;
}

/*---------------------------------------------------------------------------
 * Private variables
 *--------------------------------------------------------------------------*/

/* SDRAM base address for graph pixel buffers — two consecutive GRAPH_W × GRAPH_H
 * RGB565 frames (graph_buf and graph_buf_clean) are reserved starting here.
 * See linker/BSP memory map; FLASH_SECTOR_10 is at 0x080C0000 (unrelated). */
#define GRAPH_BUF_ADDR 0xD0025800UL

/* Pixel buffer for the canvas — RGB565, one uint16_t per pixel */
static uint16_t * const graph_buf       = (uint16_t *)GRAPH_BUF_ADDR;

/* Clean-frame cache for trace: SDRAM immediately after graph_buf */
static uint16_t * const graph_buf_clean = (uint16_t *)(GRAPH_BUF_ADDR + (size_t)GRAPH_W * GRAPH_H * 2);
static bool graph_clean_valid = false;


static lv_obj_t *graph_screen  = NULL;
static lv_obj_t *graph_canvas  = NULL;
static lv_obj_t *graph_lbl_x   = NULL;  /* X= readout, bottom-left of canvas */
static lv_obj_t *graph_lbl_y   = NULL;  /* Y= readout, bottom-right of canvas */
static lv_obj_t *graph_lbl_t   = NULL;  /* T= readout, bottom-center (param mode) */

/* Per-equation postfix cache — avoids re-parsing on every pixel column */
static GraphEquation_t eq_postfix[GRAPH_NUM_EQ];
static char            eq_postfix_str[GRAPH_NUM_EQ][64];
static bool            eq_postfix_valid[GRAPH_NUM_EQ];

/* Per-pair parametric postfix caches */
static GraphEquation_t param_postfix_x[GRAPH_NUM_PARAM];
static GraphEquation_t param_postfix_y[GRAPH_NUM_PARAM];
static char            param_postfix_x_str[GRAPH_NUM_PARAM][64];
static char            param_postfix_y_str[GRAPH_NUM_PARAM][64];
static bool            param_postfix_valid[GRAPH_NUM_PARAM];

/*---------------------------------------------------------------------------
 * Private helpers
 *--------------------------------------------------------------------------*/



/**
 * @brief Maps a math y coordinate to a canvas pixel row.
 *        Note: pixel y increases downward, math y increases upward.
 */
static int32_t math_y_to_px(float y)
{
    float range = graph_state.y_max - graph_state.y_min;
    if (fabsf(range) < 1e-9f) return 0;
    return (int32_t)((graph_state.y_max - y) / range * (GRAPH_H - 1));
}

static int32_t math_x_to_px(float x)
{
    float range = graph_state.x_max - graph_state.x_min;
    if (fabsf(range) < 1e-9f) return 0;
    return (int32_t)((x - graph_state.x_min) / range * (GRAPH_W - 1));
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
    if (graph_state.x_scl > 0.0f && graph_state.y_min <= 0.0f && graph_state.y_max >= 0.0f) {
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
    if (graph_state.y_scl > 0.0f && graph_state.x_min <= 0.0f && graph_state.x_max >= 0.0f) {
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


/**
 * @brief Clears the canvas to black and draws grid, axes, and ticks.
 *        Shared setup called at the start of both Graph_Render() and
 *        Graph_RenderParametric().
 */
static void graph_render_setup(void)
{
    lv_canvas_fill_bg(graph_canvas, lv_color_hex(COLOR_BLACK), LV_OPA_COVER);
    if (graph_state.grid_on) draw_grid();
    draw_axes();
    draw_ticks();
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
    lv_obj_set_pos(graph_lbl_y, 160, GRAPH_H - 22);
    lv_obj_set_style_text_font(graph_lbl_y, &jetbrains_mono_20, 0);
    lv_obj_set_style_text_color(graph_lbl_y, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_bg_color(graph_lbl_y, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(graph_lbl_y, LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(graph_lbl_y, 3, 0);
    lv_label_set_text(graph_lbl_y, "");

    /* T= readout — centred at bottom, shown only in parametric trace mode */
    graph_lbl_t = lv_label_create(graph_screen);
    lv_obj_set_pos(graph_lbl_t, 82, GRAPH_H - 22);
    lv_obj_set_style_text_font(graph_lbl_t, &jetbrains_mono_20, 0);
    lv_obj_set_style_text_color(graph_lbl_t, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_bg_color(graph_lbl_t, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(graph_lbl_t, LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(graph_lbl_t, 3, 0);
    lv_label_set_text(graph_lbl_t, "");

    /* Default all equations to enabled on first init */
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        graph_state.enabled[i] = true;
    }
}

void Graph_Render(bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    /* Dispatch to parametric renderer when in parametric mode */
    if (graph_state.param_mode) {
        Graph_RenderParametric(angle_degrees);
        return;
    }

    graph_render_setup();

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
        if (strlen(eqstr) == 0 || !graph_state.enabled[eq]) continue;

        /* Refresh postfix cache if the equation string has changed */
        if (!eq_postfix_valid[eq] ||
            strncmp(eqstr, eq_postfix_str[eq], sizeof(eq_postfix_str[eq])) != 0) {
            if (Calc_PrepareGraphEquation(eqstr, 0.0f, &eq_postfix[eq]) == CALC_OK) {
                strncpy(eq_postfix_str[eq], eqstr, sizeof(eq_postfix_str[eq]) - 1);
                eq_postfix_str[eq][sizeof(eq_postfix_str[eq]) - 1] = '\0';
                eq_postfix_valid[eq] = true;
            } else {
                eq_postfix_valid[eq] = false;
                continue; /* Skip uncompilable equation */
            }
        }

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

            CalcResult_t r = Calc_EvalGraphEquation(&eq_postfix[eq], x, angle_degrees);

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

                    /* Clamp to canvas bounds to prevent massive loops on singularities */
                    if (y_start < 0) y_start = 0;
                    if (y_end >= GRAPH_H) y_end = GRAPH_H - 1;

                    for (int32_t y = y_start; y <= y_end; y++)
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

    /* Blend user draw layer on top before caching */
    Graph_ApplyDrawLayer(graph_buf);

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

void Graph_InvalidateCache(void)
{
    for (int i = 0; i < GRAPH_NUM_EQ; i++)
        eq_postfix_valid[i] = false;
    for (int i = 0; i < GRAPH_NUM_PARAM; i++)
        param_postfix_valid[i] = false;
}

void Graph_RenderParametric(bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    graph_render_setup();

    static const uint32_t pair_palette[GRAPH_NUM_PARAM] = {
        COLOR_CURVE_Y1,   /* Pair 1 — white  */
        COLOR_CURVE_Y2,   /* Pair 2 — cyan   */
        COLOR_CURVE_Y3,   /* Pair 3 — yellow */
    };

    for (uint8_t p = 0; p < GRAPH_NUM_PARAM; p++) {
        const char *xstr = graph_state.param_x[p];
        const char *ystr = graph_state.param_y[p];
        if (strlen(xstr) == 0 || strlen(ystr) == 0 || !graph_state.param_enabled[p])
            continue;

        /* Rebuild X(t) postfix cache if stale */
        if (!param_postfix_valid[p] ||
            strncmp(xstr, param_postfix_x_str[p], sizeof(param_postfix_x_str[p])) != 0) {
            if (Calc_PrepareParamEquation(xstr, 0.0f, &param_postfix_x[p]) == CALC_OK) {
                strncpy(param_postfix_x_str[p], xstr, sizeof(param_postfix_x_str[p]) - 1);
                param_postfix_x_str[p][sizeof(param_postfix_x_str[p]) - 1] = '\0';
            } else {
                param_postfix_valid[p] = false;
                continue;
            }
        }
        /* Rebuild Y(t) postfix cache if stale */
        if (!param_postfix_valid[p] ||
            strncmp(ystr, param_postfix_y_str[p], sizeof(param_postfix_y_str[p])) != 0) {
            if (Calc_PrepareParamEquation(ystr, 0.0f, &param_postfix_y[p]) == CALC_OK) {
                strncpy(param_postfix_y_str[p], ystr, sizeof(param_postfix_y_str[p]) - 1);
                param_postfix_y_str[p][sizeof(param_postfix_y_str[p]) - 1] = '\0';
                param_postfix_valid[p] = true;
            } else {
                param_postfix_valid[p] = false;
                continue;
            }
        }

        lv_color_t curve_color = lv_color_hex(pair_palette[p]);
        int32_t prev_px = -1, prev_py = -1;
        bool    prev_valid = false;

        float t_step = graph_state.t_step;
        if (t_step <= 0.0f) t_step = 0.1309f;

        for (float t = graph_state.t_min;
             t <= graph_state.t_max + t_step * 0.5f;
             t += t_step) {

            CalcResult_t rx = Calc_EvalParamEquation(&param_postfix_x[p], t, angle_degrees);
            CalcResult_t ry = Calc_EvalParamEquation(&param_postfix_y[p], t, angle_degrees);

            if (rx.error != CALC_OK || ry.error != CALC_OK ||
                isnan(rx.value) || isinf(rx.value) ||
                isnan(ry.value) || isinf(ry.value)) {
                prev_valid = false;
                continue;
            }

            int32_t px = math_x_to_px(rx.value);
            int32_t py = math_y_to_px(ry.value);

            if (px < 0 || px >= GRAPH_W || py < 0 || py >= GRAPH_H) {
                prev_valid = false;
                continue;
            }

            if (prev_valid && (px - prev_px < GRAPH_W / 2) && (prev_px - px < GRAPH_W / 2)) {
                /* Interpolate vertically between prev and current pixel columns */
                int32_t span = px - prev_px;
                if (span == 0) {
                    int32_t y_lo = prev_py < py ? prev_py : py;
                    int32_t y_hi = prev_py < py ? py : prev_py;
                    for (int32_t y = y_lo; y <= y_hi; y++)
                        lv_canvas_set_px(graph_canvas, px, y, curve_color, LV_OPA_COVER);
                } else {
                    int32_t last = prev_py;
                    for (int32_t cx = prev_px + (span > 0 ? 1 : -1);
                         cx != px + (span > 0 ? 1 : -1);
                         cx += (span > 0 ? 1 : -1)) {
                        int32_t cur = prev_py + (int32_t)((float)(cx - prev_px) / (float)span * (float)(py - prev_py));
                        int32_t y_lo = last < cur ? last : cur;
                        int32_t y_hi = last < cur ? cur : last;
                        if (y_lo < 0) y_lo = 0;
                        if (y_hi >= GRAPH_H) y_hi = GRAPH_H - 1;
                        for (int32_t y = y_lo; y <= y_hi; y++)
                            lv_canvas_set_px(graph_canvas, cx, y, curve_color, LV_OPA_COVER);
                        last = cur;
                    }
                }
            } else {
                lv_canvas_set_px(graph_canvas, px, py, curve_color, LV_OPA_COVER);
            }

            prev_px    = px;
            prev_py    = py;
            prev_valid = true;
        }
    }

    /* Blend user draw layer on top before caching */
    Graph_ApplyDrawLayer(graph_buf);

    memcpy(graph_buf_clean, graph_buf, (size_t)GRAPH_W * GRAPH_H * 2);
    graph_clean_valid = true;
}

/* Parametric trace helper — draws T=/X=/Y= readouts and crosshair for one
 * T value on the given pair index.  Called only from Graph_DrawTrace(). */
static void graph_draw_trace_param(float t, uint8_t pair, bool angle_degrees)
{
    char t_buf[16], xv_buf[16], yv_buf[16], label_buf[20];
    format_graph_coord(t, t_buf, sizeof(t_buf));
    snprintf(label_buf, sizeof(label_buf), "T=%s", t_buf);
    lv_label_set_text(graph_lbl_t, label_buf);

    const char *xstr = graph_state.param_x[pair];
    const char *ystr = graph_state.param_y[pair];

    if (strlen(xstr) == 0 || strlen(ystr) == 0 || !param_postfix_valid[pair]) {
        lv_label_set_text(graph_lbl_x, "");
        lv_label_set_text(graph_lbl_y, "");
        return;
    }

    CalcResult_t rx = Calc_EvalParamEquation(&param_postfix_x[pair], t, angle_degrees);
    CalcResult_t ry = Calc_EvalParamEquation(&param_postfix_y[pair], t, angle_degrees);

    if (rx.error != CALC_OK || isnan(rx.value) || isinf(rx.value)) {
        lv_label_set_text(graph_lbl_x, "X=undef");
        lv_label_set_text(graph_lbl_y, "");
        return;
    }
    if (ry.error != CALC_OK || isnan(ry.value) || isinf(ry.value)) {
        format_graph_coord(rx.value, xv_buf, sizeof(xv_buf));
        snprintf(label_buf, sizeof(label_buf), "X=%s", xv_buf);
        lv_label_set_text(graph_lbl_x, label_buf);
        lv_label_set_text(graph_lbl_y, "Y=undef");
        return;
    }

    format_graph_coord(rx.value, xv_buf, sizeof(xv_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%s", xv_buf);
    lv_label_set_text(graph_lbl_x, label_buf);
    format_graph_coord(ry.value, yv_buf, sizeof(yv_buf));
    snprintf(label_buf, sizeof(label_buf), "Y=%s", yv_buf);
    lv_label_set_text(graph_lbl_y, label_buf);

    int32_t px = math_x_to_px(rx.value);
    int32_t py = math_y_to_px(ry.value);

    if (px >= 0 && px < GRAPH_W && py >= 0 && py < GRAPH_H) {
        lv_color_t cur = lv_color_hex(0x00FF00);
        const int32_t ARM = 5;
        for (int32_t dx = -ARM; dx <= ARM; dx++) {
            int32_t cx = px + dx;
            if (cx >= 0 && cx < GRAPH_W)
                lv_canvas_set_px(graph_canvas, cx, py, cur, LV_OPA_COVER);
        }
        for (int32_t dy = -ARM; dy <= ARM; dy++) {
            int32_t cy = py + dy;
            if (cy >= 0 && cy < GRAPH_H)
                lv_canvas_set_px(graph_canvas, px, cy, cur, LV_OPA_COVER);
        }
    }
}

/* Function-mode trace helper — draws X=/Y= readouts and crosshair at x on
 * the given equation index.  Called only from Graph_DrawTrace(). */
static void graph_draw_trace_func(float x, uint8_t eq_idx, bool angle_degrees)
{
    const char *eqstr = graph_state.equations[eq_idx];
    char x_buf[16], y_buf[16], label_buf[20];

    format_graph_coord(x, x_buf, sizeof(x_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%s", x_buf);
    lv_label_set_text(graph_lbl_x, label_buf);
    lv_label_set_text(graph_lbl_t, "");

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

void Graph_DrawTrace(float x, uint8_t eq_idx, bool angle_degrees)
{
    if (graph_canvas == NULL) return;

    /* Restore the clean frame — avoids a full re-render on every step */
    if (graph_clean_valid) {
        memcpy(graph_buf, graph_buf_clean, (size_t)GRAPH_W * GRAPH_H * 2);
    } else {
        Graph_Render(angle_degrees);
    }

    /* Parametric trace: x = t value, eq_idx = pair index (0-2) */
    if (graph_state.param_mode)
        graph_draw_trace_param(x, eq_idx < GRAPH_NUM_PARAM ? eq_idx : 0, angle_degrees);
    else
        graph_draw_trace_func(x, eq_idx, angle_degrees);
}

void Graph_ClearTrace(void)
{
    if (graph_lbl_x != NULL) lv_label_set_text(graph_lbl_x, "");
    if (graph_lbl_y != NULL) lv_label_set_text(graph_lbl_y, "");
    if (graph_lbl_t != NULL) lv_label_set_text(graph_lbl_t, "");
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

    /* Precompute RGB565 pixel values — avoids per-pixel LVGL call overhead in the loops */
    uint16_t box_px = lv_color_to_u16(lv_color_hex(COLOR_WHITE));
    uint16_t cur_px = lv_color_to_u16(lv_color_hex(COLOR_YELLOW));

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
                    graph_buf[y0 * GRAPH_W + x] = box_px;
                if (y1c >= 0 && y1c < GRAPH_H)
                    graph_buf[y1c * GRAPH_W + x] = box_px;
            }
        }
        /* Left and right edges */
        for (int32_t y = y0; y <= y1c; y++) {
            if (y >= 0 && y < GRAPH_H) {
                if (x0 >= 0 && x0 < GRAPH_W)
                    graph_buf[y * GRAPH_W + x0] = box_px;
                if (x1c >= 0 && x1c < GRAPH_W)
                    graph_buf[y * GRAPH_W + x1c] = box_px;
            }
        }
    }

    /* Draw crosshair at the moving cursor */
    const int32_t ARM = 4;
    for (int32_t dx = -ARM; dx <= ARM; dx++) {
        int32_t cx = px + dx;
        if (cx >= 0 && cx < GRAPH_W && py >= 0 && py < GRAPH_H)
            graph_buf[py * GRAPH_W + cx] = cur_px;
    }
    for (int32_t dy = -ARM; dy <= ARM; dy++) {
        int32_t cy = py + dy;
        if (cy >= 0 && cy < GRAPH_H && px >= 0 && px < GRAPH_W)
            graph_buf[cy * GRAPH_W + px] = cur_px;
    }

    /* Mark canvas dirty once after all pixel writes — replaces per-pixel invalidation */
    lv_obj_invalidate(graph_canvas);

    /* Update X/Y readout with math coordinates of current cursor */
    char x_buf[16], y_buf[16], label_buf[20];
    format_graph_coord(px_to_math_x(px), x_buf, sizeof(x_buf));
    format_graph_coord(px_to_math_y(py), y_buf, sizeof(y_buf));
    snprintf(label_buf, sizeof(label_buf), "X=%s", x_buf);
    lv_label_set_text(graph_lbl_x, label_buf);
    snprintf(label_buf, sizeof(label_buf), "Y=%s", y_buf);
    lv_label_set_text(graph_lbl_y, label_buf);
}

/*---------------------------------------------------------------------------
 * STAT plot helpers (Graph_DrawScatter / XYLine / Histogram)
 *---------------------------------------------------------------------------*/

/** Bresenham line between two canvas pixel coordinates. */
static void draw_line_px(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         lv_color_t color)
{
    int32_t dx = x1 - x0, dy = y1 - y0;
    int32_t ax = dx < 0 ? -dx : dx;
    int32_t ay = dy < 0 ? -dy : dy;
    int32_t sx = dx >= 0 ? 1 : -1;
    int32_t sy = dy >= 0 ? 1 : -1;
    int32_t err = ax - ay;
    int32_t cx = x0, cy = y0;
    while (1) {
        if (cx >= 0 && cx < GRAPH_W && cy >= 0 && cy < GRAPH_H)
            lv_canvas_set_px(graph_canvas, cx, cy, color, LV_OPA_COVER);
        if (cx == x1 && cy == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -ay) { err -= ay; cx += sx; }
        if (e2 <  ax) { err += ax; cy += sy; }
    }
}

/** Prepare canvas: clear, draw axes/ticks, make graph visible. */
static void stat_plot_prepare(void)
{
    if (graph_canvas == NULL) return;
    lv_canvas_fill_bg(graph_canvas, lv_color_hex(COLOR_BLACK), LV_OPA_COVER);
    if (graph_state.grid_on) draw_grid();
    draw_axes();
    draw_ticks();
    graph_clean_valid = false;
}

void Graph_DrawScatter(const StatData_t *d)
{
    stat_plot_prepare();
    if (d->list_len == 0) { Graph_SetVisible(true); return; }

    lv_color_t c = lv_color_hex(COLOR_CURVE_Y1);
    for (uint8_t i = 0; i < d->list_len; i++) {
        int32_t px = math_x_to_px(d->list_x[i]);
        int32_t py = math_y_to_px(d->list_y[i]);
        /* 3×3 cross */
        for (int32_t dx = -1; dx <= 1; dx++) {
            int32_t ex = px + dx;
            if (ex >= 0 && ex < GRAPH_W && py >= 0 && py < GRAPH_H)
                lv_canvas_set_px(graph_canvas, ex, py, c, LV_OPA_COVER);
        }
        for (int32_t dy = -1; dy <= 1; dy++) {
            int32_t ey = py + dy;
            if (px >= 0 && px < GRAPH_W && ey >= 0 && ey < GRAPH_H)
                lv_canvas_set_px(graph_canvas, px, ey, c, LV_OPA_COVER);
        }
    }
    Graph_SetVisible(true);
}

void Graph_DrawXYLine(const StatData_t *d)
{
    stat_plot_prepare();
    if (d->list_len == 0) { Graph_SetVisible(true); return; }

    lv_color_t c = lv_color_hex(COLOR_CURVE_Y1);
    /* Draw scatter crosses */
    for (uint8_t i = 0; i < d->list_len; i++) {
        int32_t px = math_x_to_px(d->list_x[i]);
        int32_t py = math_y_to_px(d->list_y[i]);
        for (int32_t dx = -1; dx <= 1; dx++) {
            int32_t ex = px + dx;
            if (ex >= 0 && ex < GRAPH_W && py >= 0 && py < GRAPH_H)
                lv_canvas_set_px(graph_canvas, ex, py, c, LV_OPA_COVER);
        }
        for (int32_t dy = -1; dy <= 1; dy++) {
            int32_t ey = py + dy;
            if (px >= 0 && px < GRAPH_W && ey >= 0 && ey < GRAPH_H)
                lv_canvas_set_px(graph_canvas, px, ey, c, LV_OPA_COVER);
        }
    }
    /* Connect consecutive points */
    for (uint8_t i = 1; i < d->list_len; i++) {
        int32_t x0 = math_x_to_px(d->list_x[i - 1]);
        int32_t y0 = math_y_to_px(d->list_y[i - 1]);
        int32_t x1 = math_x_to_px(d->list_x[i]);
        int32_t y1 = math_y_to_px(d->list_y[i]);
        draw_line_px(x0, y0, x1, y1, c);
    }
    Graph_SetVisible(true);
}

void Graph_DrawHistogram(const StatData_t *d)
{
    stat_plot_prepare();
    if (d->list_len == 0) { Graph_SetVisible(true); return; }

    float range = graph_state.x_max - graph_state.x_min;
    if (fabsf(range) < 1e-9f) { Graph_SetVisible(true); return; }

#define HIST_BINS 10
    int32_t counts[HIST_BINS] = {0};
    float   bin_width = range / (float)HIST_BINS;
    if (bin_width <= 0.0f) { Graph_SetVisible(true); return; }

    for (uint8_t i = 0; i < d->list_len; i++) {
        float x = d->list_x[i];
        int bin = (int)((x - graph_state.x_min) / bin_width);
        if (bin < 0) bin = 0;
        if (bin >= HIST_BINS) bin = HIST_BINS - 1;
        counts[bin]++;
    }

    /* Find max count for y scaling */
    int32_t max_count = 1;
    for (int b = 0; b < HIST_BINS; b++)
        if (counts[b] > max_count) max_count = counts[b];

    lv_color_t c = lv_color_hex(COLOR_CURVE_Y1);
    int32_t baseline_py = math_y_to_px(graph_state.y_min);
    if (baseline_py < 0)          baseline_py = 0;
    if (baseline_py >= GRAPH_H)   baseline_py = GRAPH_H - 1;

    /* Each bin occupies GRAPH_W / HIST_BINS columns */
    int32_t bin_px_w = GRAPH_W / HIST_BINS;

    for (int b = 0; b < HIST_BINS; b++) {
        if (counts[b] == 0) continue;
        float bar_top_y = graph_state.y_min +
            (float)counts[b] / (float)max_count * (graph_state.y_max - graph_state.y_min);
        int32_t bar_top_py = math_y_to_px(bar_top_y);
        if (bar_top_py < 0) bar_top_py = 0;
        if (bar_top_py > GRAPH_H - 1) bar_top_py = GRAPH_H - 1;

        int32_t x_start = b * bin_px_w;
        int32_t x_end   = x_start + bin_px_w - 1;
        if (x_end >= GRAPH_W) x_end = GRAPH_W - 1;

        int32_t y_top    = bar_top_py;
        int32_t y_bottom = baseline_py;
        if (y_top > y_bottom) { int32_t tmp = y_top; y_top = y_bottom; y_bottom = tmp; }

        for (int32_t px = x_start; px <= x_end; px++) {
            for (int32_t py = y_top; py <= y_bottom; py++) {
                lv_canvas_set_px(graph_canvas, px, py, c, LV_OPA_COVER);
            }
        }
    }
#undef HIST_BINS
    Graph_SetVisible(true);
}


bool Graph_IsVisible(void)
{
    return graph_state.active;
}

int32_t Graph_MathXToPx(float x)
{
    int32_t px = math_x_to_px(x);
    if (px < 0) px = 0;
    if (px >= GRAPH_W) px = GRAPH_W - 1;
    return px;
}

int32_t Graph_MathYToPx(float y)
{
    int32_t py = math_y_to_px(y);
    if (py < 0) py = 0;
    if (py >= GRAPH_H) py = GRAPH_H - 1;
    return py;
}