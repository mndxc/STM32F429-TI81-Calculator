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
 * Cross-module calls: zoom_execute_item() calls functions defined in graph_ui.c
 * and declared in graph_ui.h — zoom_enter_zbox() (case 1) and
 * zoom_enter_cursor_pick() (cases 2, 3, 8).
 */

#include "ui_shared.h"
#include "calculator_core.h"
#include "calc_engine.h"
#include "graph_ui.h"
#include "ui_graph_zoom.h"
#include "ui_menu_screen.h"
#include "graph.h"
#include "graph_ui_range.h"
#include "ui_palette.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define ZOOM_ITEM_COUNT       8

static const char * const zoom_display_labels[ZOOM_ITEM_COUNT] = {
    "1:Box", "2:Zoom In", "3:Zoom Out", "4:Set Factors",
    "5:Square", "6:Standard", "7:Trig", "8:Integer"
};

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_zoom_ms;

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Zoom_ShowScreen(void) { lv_obj_clear_flag(s_zoom_ms.screen, LV_OBJ_FLAG_HIDDEN); }
void Zoom_HideScreen(void) { lv_obj_add_flag  (s_zoom_ms.screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * ZOOM action executor helpers
 *---------------------------------------------------------------------------*/

/* Hide zoom menu, switch to normal mode, show graph canvas, and redraw. */
static void zoom_show_graph(void)
{
    Calc_SetMode(MODE_NORMAL);
    lvgl_lock();
    lv_obj_add_flag(s_zoom_ms.screen, LV_OBJ_FLAG_HIDDEN);
    Graph_SetVisible(true);
    Graph_Render();
    lvgl_unlock();
}

/* Open the Zoom Factors editor screen. */
static void zoom_enter_factors(void)
{
    Calc_SetMode(MODE_GRAPH_ZOOM_FACTORS);
    lvgl_lock();
    lv_obj_add_flag(s_zoom_ms.screen, LV_OBJ_FLAG_HIDDEN);
    zoom_factors_nav_enter();
    lvgl_unlock();
}

static void apply_zoom_preset(uint8_t preset)
{
    const GraphState_t *gs = Graph_GetState();
    switch (preset) {
    case 1: /* ZStandard */
        Graph_SetWindow(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 1.0f, gs->x_res);
        if (gs->param_mode) Graph_SetParamWindow(0.0f, 6.2832f, 0.10472f);
        break;
    case 2: /* ZTrig */
        Graph_SetWindow(-6.2832f, 6.2832f, -3.0f, 3.0f, 1.5708f, 0.25f, gs->x_res);
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
    case 2: zoom_enter_cursor_pick(1);                                    break; /* Zoom In */
    case 3: zoom_enter_cursor_pick(2);                                    break; /* Zoom Out */
    case 4: zoom_enter_factors();                                         break;
    case 5: apply_zoom_preset(4); zoom_show_graph();                      break;
    case 6: apply_zoom_preset(1); zoom_show_graph();                      break;
    case 7: apply_zoom_preset(2); zoom_show_graph();                      break;
    case 8: zoom_enter_cursor_pick(3);                                    break; /* Integer */
    default:
        Calc_SetMode(MODE_NORMAL);
        lvgl_lock(); lv_obj_add_flag(s_zoom_ms.screen, LV_OBJ_FLAG_HIDDEN); lvgl_unlock();
        break;
    }
}

/*---------------------------------------------------------------------------
 * MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void zoom_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    zoom_execute_item((uint8_t)(idx + 1));
}

static void zoom_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    zoom_menu_reset();
    Calc_SetMode(MODE_NORMAL);
    lvgl_lock();
    hide_all_screens();
    lvgl_unlock();
}

/* TOKEN_ZOOM closes without reopening; graph-nav and other menu keys fall
 * through to handle_normal_mode via DefaultExtra. */
static bool zoom_on_extra(Token_t t, MenuScreen_t *ms)
{
    if (t == TOKEN_ZOOM) {
        zoom_on_cancel(NULL);
        return true;
    }
    return MenuScreen_DefaultExtra(t, ms);
}

static const MenuTabDesc_t s_tab = {
    ZOOM_ITEM_COUNT, zoom_display_labels, NULL, zoom_on_select
};

static const MenuScreenDesc_t s_desc = {
    .tab_count      = 0,
    .tab_names      = NULL,
    .tab_x          = NULL,
    .default_tab    = 0,
    .wrap_tabs      = false,
    .title          = "ZOOM",
    .tabs           = &s_tab,
    .left_mode      = 0,
    .right_mode     = 0,
    .on_cancel      = zoom_on_cancel,
    .on_tab_switch  = NULL,
    .on_extra       = zoom_on_extra,
};

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/

void ui_init_zoom_screen(lv_obj_t *parent)
{
    MenuScreen_Init(&s_zoom_ms, &s_desc, parent);
}

/*---------------------------------------------------------------------------
 * Display helper
 *---------------------------------------------------------------------------*/

void ui_update_zoom_display(void)
{
    MenuScreen_UpdateDisplay(&s_zoom_ms);
}

/*---------------------------------------------------------------------------
 * State helper
 *---------------------------------------------------------------------------*/

void zoom_menu_reset(void)
{
    s_zoom_ms.nav.cursor = 0;
    s_zoom_ms.nav.scroll = 0;
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

bool handle_zoom_mode(Token_t t) { return MenuScreen_HandleToken(&s_zoom_ms, t); }
