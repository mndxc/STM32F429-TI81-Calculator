/**
 * @file    ui_prgm_mode.c
 * @brief   PRGM MODE sub-menu — NUMBER and GRAPH tabs.
 *
 * All navigation, display, and LVGL boilerplate is owned by MenuScreen_t
 * in ui_menu_screen.c.  This file contains only the string tables,
 * descriptors, and thin public-API wrappers.
 *
 * Guidebook: pp. 8-16, 8-18 — "Setting Modes from a Program".
 */

#include "ui_shared.h"
#include "ui_prgm.h"
#include "ui_prgm_mode.h"
#include "prgm_editor.h"
#include "ui_palette.h"
#include "ui_menu_screen.h"

/*---------------------------------------------------------------------------
 * NUMBER tab — 7 items, no scroll required
 *---------------------------------------------------------------------------*/

#define PRGM_MODE_NUM_COUNT  7

static const char * const prgm_mode_num_display[PRGM_MODE_NUM_COUNT] = {
    "1:Norm",  "2:Sci",   "3:Eng",
    "4:Fix",   "5:Float", "6:Rad",  "7:Deg",
};
static const char * const prgm_mode_num_insert[PRGM_MODE_NUM_COUNT] = {
    "Norm",    "Sci",     "Eng",
    "Fix ",    "Float",   "Rad",    "Deg",
};

/*---------------------------------------------------------------------------
 * GRAPH tab — 10 items, scroll required (MENU_VISIBLE_ROWS = 7)
 *---------------------------------------------------------------------------*/

#define PRGM_MODE_GPH_COUNT  10

static const char * const prgm_mode_gph_display[PRGM_MODE_GPH_COUNT] = {
    "1:Function",  "2:Param",    "3:Connected",
    "4:Dot",       "5:Sequence", "6:Simul",
    "7:Grid Off",  "8:Grid On",  "9:Rect",
    "O:Polar",
};
static const char * const prgm_mode_gph_insert[PRGM_MODE_GPH_COUNT] = {
    "Function",    "Param",      "Connected",
    "Dot",         "Sequence",   "Simul",
    "Grid Off",    "Grid On",    "Rect",
    "Polar",
};

/*---------------------------------------------------------------------------
 * Descriptors
 *---------------------------------------------------------------------------*/

static const char * const s_tab_names[2] = {"NUMBER", "GRAPH"};
static const int           s_tab_x[2]    = {4, 110};

static void num_on_select(int idx, lv_obj_t *screen)
{
    PrgmEditor_InsertStr(prgm_mode_num_insert[idx]);
    PrgmEditor_FlattenToStore();
    prgm_submenu_return_to_editor(screen);
}

static void gph_on_select(int idx, lv_obj_t *screen)
{
    if (idx < PRGM_MODE_GPH_COUNT) {
        PrgmEditor_InsertStr(prgm_mode_gph_insert[idx]);
        PrgmEditor_FlattenToStore();
    }
    prgm_submenu_return_to_editor(screen);
}

static const MenuScreenDesc_t s_num_desc = {
    .tab_count      = 2,
    .tab_names      = s_tab_names,
    .tab_x          = s_tab_x,
    .active_tab     = 0,
    .item_count     = PRGM_MODE_NUM_COUNT,
    .display_labels = prgm_mode_num_display,
    .get_label      = NULL,
    .left_mode      = 0,                   /* no LEFT in NUMBER tab */
    .right_mode     = MODE_PRGM_MODE_GRAPH,
    .on_select      = num_on_select,
    .on_cancel      = prgm_submenu_return_to_editor,
    .on_tab_switch  = prgm_submenu_tab_switch,
    .on_extra       = NULL,
};

static const MenuScreenDesc_t s_gph_desc = {
    .tab_count      = 2,
    .tab_names      = s_tab_names,
    .tab_x          = s_tab_x,
    .active_tab     = 1,
    .item_count     = PRGM_MODE_GPH_COUNT,
    .display_labels = prgm_mode_gph_display,
    .get_label      = NULL,
    .left_mode      = MODE_PRGM_MODE_NUMBER,
    .right_mode     = 0,                   /* no RIGHT in GRAPH tab */
    .on_select      = gph_on_select,
    .on_cancel      = prgm_submenu_return_to_editor,
    .on_tab_switch  = prgm_submenu_tab_switch,
    .on_extra       = NULL,
};

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_num;
static MenuScreen_t s_gph;
static lv_obj_t *ui_prgm_mode_num_screen = NULL;
static lv_obj_t *ui_prgm_mode_gph_screen = NULL;

void ui_init_prgm_mode_screens(lv_obj_t *parent)
{
    MenuScreen_Init(&s_num, &s_num_desc, parent);
    ui_prgm_mode_num_screen = s_num.screen;
    MenuScreen_Init(&s_gph, &s_gph_desc, parent);
    ui_prgm_mode_gph_screen = s_gph.screen;
}

void ui_prgm_mode_num_reset_and_show(void)
{
    lv_obj_clear_flag(ui_prgm_mode_num_screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_num);
}
void ui_prgm_mode_gph_reset_and_show(void)
{
    lv_obj_clear_flag(ui_prgm_mode_gph_screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_gph);
}
void ui_prgm_mode_num_hide(void) { if (ui_prgm_mode_num_screen) lv_obj_add_flag(ui_prgm_mode_num_screen, LV_OBJ_FLAG_HIDDEN); }
void ui_prgm_mode_gph_hide(void) { if (ui_prgm_mode_gph_screen) lv_obj_add_flag(ui_prgm_mode_gph_screen, LV_OBJ_FLAG_HIDDEN); }
bool handle_prgm_mode_number(Token_t t)    { return MenuScreen_HandleToken(&s_num, t); }
bool handle_prgm_mode_graph(Token_t t)     { return MenuScreen_HandleToken(&s_gph, t); }
