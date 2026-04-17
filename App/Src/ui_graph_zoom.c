/**
 * @file    ui_graph_zoom.c
 * @brief   ZOOM menu screen handler.
 *
 * Extracted from graph_ui.c (INTERFACE_REFACTOR_PLAN Item 4). Zero behavioral
 * changes — purely a file organisation refactor.
 *
 * All LVGL calls must be made under lvgl_lock()/lvgl_unlock() except from
 * cursor_timer_cb (which runs inside lv_task_handler — mutex already held).
 *
 * Cross-module call: zoom_execute_item() case 1 calls zoom_enter_zbox() which
 * is defined in graph_ui.c (it resets s_zbox, owned by the ZBox handler).
 * The declaration is in calc_internal.h.
 */

#include "calc_internal.h"
#include "ui_graph_zoom.h"
#include "graph.h"
#include "graph_ui_range.h"
#include "ui_palette.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define ZOOM_ITEM_COUNT       8

static const char * const zoom_item_names[ZOOM_ITEM_COUNT] = {
    "Box", "Zoom In", "Zoom Out", "Set Factors",
    "Square", "Standard", "Trig", "Integer"
};

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t  scroll_offset;
    uint8_t  item_cursor;       /* Visible-row index of highlight */
} ZoomMenuState_t;

/*---------------------------------------------------------------------------
 * LVGL object pointers
 *---------------------------------------------------------------------------*/

/* Screen pointer — non-static so hide_all_screens() and menu_close() can reach it */
lv_obj_t *ui_graph_zoom_screen = NULL;

/* ZOOM menu labels */
static lv_obj_t *zoom_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *zoom_scroll_ind[2];   /* [0]=top(↑), [1]=bottom(↓) */

/*---------------------------------------------------------------------------
 * State
 *---------------------------------------------------------------------------*/

static ZoomMenuState_t s_zoom = {0};

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/

void ui_init_zoom_screen(lv_obj_t *parent)
{
    ui_graph_zoom_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_graph_zoom_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_zoom_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_zoom_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_zoom_title = lv_label_create(ui_graph_zoom_screen);
    lv_obj_set_pos(lbl_zoom_title, 4, 4);
    lv_obj_set_style_text_font(lbl_zoom_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_zoom_title, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(lbl_zoom_title, "ZOOM");

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        zoom_item_labels[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(zoom_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(zoom_item_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        zoom_scroll_ind[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(zoom_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(zoom_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(zoom_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(zoom_scroll_ind[i], 0, 0);
        lv_label_set_text(zoom_scroll_ind[i], "");
        lv_obj_add_flag(zoom_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/*---------------------------------------------------------------------------
 * Display helper
 *---------------------------------------------------------------------------*/

void ui_update_zoom_display(void)
{
    lv_obj_add_flag(zoom_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(zoom_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = s_zoom.scroll_offset + i;
        if (idx >= ZOOM_ITEM_COUNT) {
            lv_label_set_text(zoom_item_labels[i], "");
            continue;
        }
        bool more_below = (s_zoom.scroll_offset + MENU_VISIBLE_ROWS < ZOOM_ITEM_COUNT)
                          && (i == MENU_VISIBLE_ROWS - 1);
        bool more_above = (s_zoom.scroll_offset > 0) && (i == 0);
        char buf[32];
        if (more_below) {
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, zoom_item_names[idx]);
            lv_label_set_text(zoom_scroll_ind[1], "\xE2\x86\x93");
            lv_obj_clear_flag(zoom_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        } else if (more_above) {
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, zoom_item_names[idx]);
            lv_label_set_text(zoom_scroll_ind[0], "\xE2\x86\x91");
            lv_obj_clear_flag(zoom_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        } else {
            snprintf(buf, sizeof(buf), "%d:%s", idx + 1, zoom_item_names[idx]);
        }
        lv_color_t col = (i == (int)s_zoom.item_cursor)
            ? lv_color_hex(COLOR_YELLOW)
            : lv_color_hex(COLOR_WHITE);
        lv_obj_set_style_text_color(zoom_item_labels[i], col, 0);
        lv_label_set_text(zoom_item_labels[i], buf);
    }
}

/*---------------------------------------------------------------------------
 * State helper
 *---------------------------------------------------------------------------*/

void zoom_menu_reset(void)
{
    s_zoom.scroll_offset = 0;
    s_zoom.item_cursor   = 0;
}

/*---------------------------------------------------------------------------
 * ZOOM action executor helpers
 *---------------------------------------------------------------------------*/

/* Hide zoom menu, switch to normal mode, show graph canvas, and redraw. */
static void zoom_show_graph(void)
{
    current_mode = MODE_NORMAL;
    lvgl_lock();
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
    Graph_SetVisible(true);
    Graph_Render(angle_degrees);
    lvgl_unlock();
}

/* Scale the graph window by (xf, yf) around its centre.
 * xf < 1 zooms in on X; xf > 1 zooms out. Same convention for yf. */
static void zoom_scale_view(float xf, float yf)
{
    const GraphState_t *gs = Graph_GetState();
    float xc = (gs->x_min + gs->x_max) * 0.5f;
    float yc = (gs->y_min + gs->y_max) * 0.5f;
    float xh = (gs->x_max - gs->x_min) * xf / 2.0f;
    float yh = (gs->y_max - gs->y_min) * yf / 2.0f;
    Graph_SetWindow(xc - xh, xc + xh, yc - yh, yc + yh,
                    gs->x_scl, gs->y_scl, gs->x_res);
}

/* Open the Zoom Factors editor screen. */
static void zoom_enter_factors(void)
{
    current_mode = MODE_GRAPH_ZOOM_FACTORS;
    lvgl_lock();
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
    zoom_factors_nav_enter();
    lvgl_unlock();
}

static void apply_zoom_preset(uint8_t preset)
{
    const GraphState_t *gs = Graph_GetState();
    switch (preset) {
    case 1: /* ZStandard */
        Graph_SetWindow(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 1.0f, gs->x_res);
        break;
    case 2: /* ZTrig */
        Graph_SetWindow(-6.2832f, 6.2832f, -4.0f, 4.0f, 1.5708f, 1.0f, gs->x_res);
        break;
    case 3: /* ZDecimal */
        Graph_SetWindow(-4.7f, 4.7f, -3.1f, 3.1f, 0.5f, 0.5f, gs->x_res);
        break;
    case 4: /* ZSquare */
        {
            float xs = (gs->x_max - gs->x_min) / GRAPH_W;
            float ys = (gs->y_max - gs->y_min) / GRAPH_H;
            if (xs > ys) {
                float yc = (gs->y_max + gs->y_min) * 0.5f;
                float yh = xs * GRAPH_H * 0.5f;
                Graph_SetWindow(gs->x_min, gs->x_max, yc - yh, yc + yh,
                                gs->x_scl, gs->y_scl, gs->x_res);
            } else {
                float xc = (gs->x_max + gs->x_min) * 0.5f;
                float xh = ys * GRAPH_W * 0.5f;
                Graph_SetWindow(xc - xh, xc + xh, gs->y_min, gs->y_max,
                                gs->x_scl, gs->y_scl, gs->x_res);
            }
        }
        break;
    case 5: /* ZInteger */
        Graph_SetWindow(-160.0f, 159.0f, -110.0f, 109.0f, 10.0f, 10.0f, gs->x_res);
        break;
    default:
        Graph_SetWindow(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 1.0f, gs->x_res);
        break;
    }
}

static void zoom_execute_item(uint8_t item_num)
{
    zoom_menu_reset();
    switch (item_num) {
    case 1: zoom_enter_zbox();                                            break;
    case 2: zoom_scale_view(1.0f / graph_ui_get_zoom_x_fact(), 1.0f / graph_ui_get_zoom_y_fact());
            zoom_show_graph();                                            break;
    case 3: zoom_scale_view(graph_ui_get_zoom_x_fact(), graph_ui_get_zoom_y_fact());
            zoom_show_graph();                                            break;
    case 4: zoom_enter_factors();                                         break;
    case 5: apply_zoom_preset(4); zoom_show_graph();                     break;
    case 6: apply_zoom_preset(1); zoom_show_graph();                     break;
    case 7: apply_zoom_preset(2); zoom_show_graph();                     break;
    case 8: apply_zoom_preset(5); zoom_show_graph();                     break;
    default:
        current_mode = MODE_NORMAL;
        lvgl_lock(); lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN); lvgl_unlock();
        break;
    }
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

bool handle_zoom_mode(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_zoom.item_cursor > 0) {
            s_zoom.item_cursor--;
        } else if (s_zoom.scroll_offset > 0) {
            s_zoom.scroll_offset--;
        }
        lvgl_lock();
        ui_update_zoom_display();
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(s_zoom.scroll_offset + s_zoom.item_cursor) + 1 < ZOOM_ITEM_COUNT) {
            if (s_zoom.item_cursor < MENU_VISIBLE_ROWS - 1)
                s_zoom.item_cursor++;
            else if (s_zoom.scroll_offset + MENU_VISIBLE_ROWS < ZOOM_ITEM_COUNT)
                s_zoom.scroll_offset++;
        }
        lvgl_lock();
        ui_update_zoom_display();
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        zoom_execute_item((uint8_t)(s_zoom.scroll_offset + s_zoom.item_cursor + 1));
        return true;
    case TOKEN_CLEAR:
    case TOKEN_ZOOM:
        zoom_menu_reset();
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return true;
    case TOKEN_Y_EQUALS:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_GRAPH:
        zoom_menu_reset();
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_TRACE:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_TRACE);
        return true;
    case TOKEN_1 ... TOKEN_9: {
        uint8_t item = (uint8_t)(t - TOKEN_0);
        if (item <= ZOOM_ITEM_COUNT)
            zoom_execute_item(item);
        return true;
    }
    default:
        zoom_menu_reset();
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}
