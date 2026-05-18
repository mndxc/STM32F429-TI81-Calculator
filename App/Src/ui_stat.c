/**
 * @file    ui_stat.c
 * @brief   STAT menu, DATA list editor, and results screen UI.
 *
 * Three modes:
 *   MODE_STAT_MENU    — three-tab menu (CALC / DRAW / DATA)
 *   MODE_STAT_EDIT    — two-column list editor for (x,y) data pairs
 *   MODE_STAT_RESULTS — multi-line readout of last statistical computation
 */

#include "ui_stat.h"
#include "ui_menu_screen.h"
#include "ui_shared.h"
#include "calc_engine.h"
#include "calculator_core.h"
#include "calc_stat.h"
#include "graph.h"
#include "ui_palette.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Owns:     s_stat_ms (MenuScreen_t for STAT), stat display labels, and key
 *           dispatch for MODE_STAT.
 * Not owns: stat_data[] and stat_results (owned by calc_stat.c).
 * Locks:    All label-update calls require lvgl_lock() from the caller.
 */

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define STAT_TAB_COUNT      3   /* CALC, DRAW, DATA */

static const char * const stat_tab_names[STAT_TAB_COUNT] = {"CALC", "DRAW", "DATA"};

static const char * const stat_calc_names[5] = {
    "1:1-Var", "2:LinReg", "3:LnReg", "4:ExpReg", "5:PwrReg"
};
static const char * const stat_draw_names[3] = {
    "1:Hist", "2:Scatter", "3:xyLine"
};
static const char * const stat_data_names[4] = {
    "1:Edit", "2:ClrStat", "3:xSort", "4:ySort"
};

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

/* ui_stat_results_screen is public (referenced externally via Stat_HideResultsScreen) */
lv_obj_t *ui_stat_results_screen = NULL;

/* MenuScreen_t for the 3-tab STAT menu */
static MenuScreen_t s_stat_ms;

/* Module-private stat data and results — accessed via Stat_GetData/GetResults/SetData */
static StatData_t    stat_data    = {{0}, {0}, 0};
static StatResults_t stat_results = {0};

const StatData_t    *Stat_GetData(void)    { return &stat_data; }
const StatResults_t *Stat_GetResults(void) { return &stat_results; }
void                 Stat_SetData(const StatData_t *src) { memcpy(&stat_data, src, sizeof(stat_data)); }

/* LVGL objects — results screen */
static lv_obj_t *stat_results_lbl = NULL;

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

/** Format a stat value for display in the DATA editor (up to 10 chars). */
static void stat_fmt(float v, char *buf, size_t len)
{
    Calc_FormatResult(v, buf, (uint8_t)(len < 255 ? len : 255));
}

/** Run a CALC tab computation and transition to MODE_STAT_RESULTS. */
static void stat_run_calc(uint8_t item)
{
    bool ok = false;
    switch (item) {
    case 0: CalcStat_Compute1Var(&stat_data, &stat_results); ok = stat_results.valid; break;
    case 1: ok = CalcStat_ComputeLinReg(&stat_data, &stat_results); break;
    case 2: ok = CalcStat_ComputeLnReg(&stat_data, &stat_results);  break;
    case 3: ok = CalcStat_ComputeExpReg(&stat_data, &stat_results); break;
    case 4: ok = CalcStat_ComputePwrReg(&stat_data, &stat_results); break;
    }
    (void)ok;
    Calc_SetMode(MODE_STAT_RESULTS);
    lvgl_lock();
    Stat_HideMenuScreen();
    lv_obj_clear_flag(ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_stat_results_display();
    lvgl_unlock();
}

/** Run a DRAW tab plot operation. */
static void stat_run_draw(uint8_t item)
{
    CalcMode_t ret_mode       = s_stat_ms.nav.return_mode;
    s_stat_ms.nav.return_mode = MODE_NORMAL;

    lvgl_lock();
    Stat_HideMenuScreen();
    lvgl_unlock();

    Graph_SetVisible(true);
    switch (item) {
    case 0: Graph_DrawHistogram(&stat_data); break;
    case 1: Graph_DrawScatter(&stat_data);   break;
    case 2: Graph_DrawXYLine(&stat_data);    break;
    }

    Calc_SetMode(ret_mode);
}

/*---------------------------------------------------------------------------
 * MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void stat_calc_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    stat_run_calc((uint8_t)idx);
}

static void stat_draw_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    stat_run_draw((uint8_t)idx);
}

static void stat_data_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    switch (idx) {
    case 0:
        Stat_EditOpen();
        break;
    case 1:
        CalcStat_Clear(&stat_data);
        lvgl_lock(); MenuScreen_UpdateDisplay(&s_stat_ms); lvgl_unlock();
        break;
    case 2:
        CalcStat_SortX(&stat_data);
        lvgl_lock(); MenuScreen_UpdateDisplay(&s_stat_ms); lvgl_unlock();
        break;
    case 3:
        CalcStat_SortY(&stat_data);
        lvgl_lock(); MenuScreen_UpdateDisplay(&s_stat_ms); lvgl_unlock();
        break;
    }
}

static void stat_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    menu_close(TOKEN_STAT);
    Update_Calculator_Display();
}

static const int s_tab_x[STAT_TAB_COUNT] = {4, 90, 190};

static const MenuTabDesc_t s_tabs[STAT_TAB_COUNT] = {
    { 5, stat_calc_names, NULL, stat_calc_on_select },
    { 3, stat_draw_names, NULL, stat_draw_on_select },
    { 4, stat_data_names, NULL, stat_data_on_select },
};

static const MenuScreenDesc_t s_desc = {
    .tab_count      = STAT_TAB_COUNT,
    .tab_names      = stat_tab_names,
    .tab_x          = s_tab_x,
    .default_tab    = 0,
    .wrap_tabs      = false,
    .title          = NULL,
    .tabs           = s_tabs,
    .left_mode      = 0,
    .right_mode     = 0,
    .on_cancel      = stat_on_cancel,
    .on_tab_switch  = NULL,
    .on_extra       = NULL,
};

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

/* Caller must hold lvgl_lock(). */
void Stat_ShowMenuScreen(void)    { lv_obj_clear_flag(s_stat_ms.screen,       LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
void Stat_HideMenuScreen(void)    { lv_obj_add_flag  (s_stat_ms.screen,       LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
void Stat_ShowResultsScreen(void) { lv_obj_clear_flag(ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN); }
/* Caller must hold lvgl_lock(). */
void Stat_HideResultsScreen(void) { lv_obj_add_flag  (ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_stat_screen(void)
{
    MenuScreen_Init(&s_stat_ms, &s_desc, lv_scr_act());
}

void ui_init_stat_results_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_stat_results_screen = screen_create(scr);

    stat_results_lbl = lv_label_create(ui_stat_results_screen);
    lv_obj_set_pos(stat_results_lbl, 4, 4);
    lv_obj_set_style_text_font(stat_results_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(stat_results_lbl,
        lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(stat_results_lbl, "");
}

/*---------------------------------------------------------------------------
 * Display Updates
 *---------------------------------------------------------------------------*/

/* Caller must hold lvgl_lock(). */
void ui_update_stat_display(void)
{
    MenuScreen_UpdateDisplay(&s_stat_ms);
}

/* Caller must hold lvgl_lock(). */
void ui_update_stat_results_display(void)
{
    char buf[256];
    int  pos = 0;

    if (!stat_results.valid) {
        lv_label_set_text(stat_results_lbl, "ERR:no data");
        return;
    }

    /* 1-Var results are always present when valid */
    if (stat_results.n > 0.0f) {
        char tmp[24];
        stat_fmt(stat_results.n,       tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "n=%s\n", tmp);
        stat_fmt(stat_results.mean_x,  tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "x\xCC\x84=%s\n", tmp);
        stat_fmt(stat_results.sx,      tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Sx=%s\n", tmp);
        stat_fmt(stat_results.sigma_x, tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\xCF\x83x=%s\n", tmp);
        stat_fmt(stat_results.sum_x,   tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\xCE\xA3x=%s\n", tmp);
        stat_fmt(stat_results.sum_x2,  tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\xCE\xA3x\xC2\xB2=%s", tmp);
    }

    /* Regression results (reg_a non-zero = regression was run) */
    if (stat_results.reg_a != 0.0f || stat_results.reg_b != 0.0f) {
        char tmp[24];
        stat_fmt(stat_results.reg_a, tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\na=%s", tmp);
        stat_fmt(stat_results.reg_b, tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\nb=%s", tmp);
        stat_fmt(stat_results.reg_r, tmp, sizeof(tmp));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\nr=%s", tmp);
    }

    if (pos == 0) buf[0] = '\0';
    lv_label_set_text(stat_results_lbl, buf);
}

/*---------------------------------------------------------------------------
 * Token Handlers
 *---------------------------------------------------------------------------*/

bool handle_stat_menu(Token_t t)
{
    return MenuScreen_HandleToken(&s_stat_ms, t);
}

bool handle_stat_results(Token_t t)
{
    switch (t) {
    case TOKEN_CLEAR:
        menu_close(TOKEN_STAT);
        Update_Calculator_Display();
        return true;
    default:
        /* Any other key returns to menu */
        Calc_SetMode(MODE_STAT_MENU);
        lvgl_lock();
        lv_obj_add_flag(ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN);
        Stat_ShowMenuScreen();
        MenuScreen_UpdateDisplay(&s_stat_ms);
        lvgl_unlock();
        return true;
    }
}

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

/* Caller must hold lvgl_lock(). */
void Stat_MenuOpen(CalcMode_t return_to)
{
    s_stat_ms.nav.return_mode = return_to;
    Calc_SetMode(MODE_STAT_MENU);
    lv_obj_clear_flag(s_stat_ms.screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_stat_ms);
}

CalcMode_t Stat_MenuClose(void)
{
    CalcMode_t ret            = s_stat_ms.nav.return_mode;
    s_stat_ms.nav.return_mode = MODE_NORMAL;
    return ret;
}
