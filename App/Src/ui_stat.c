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
#include "calc_internal.h"
#include "calc_stat.h"
#include "graph.h"
#include "ui_palette.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define STAT_TAB_COUNT      3   /* CALC, DRAW, DATA */

static const char * const stat_tab_names[STAT_TAB_COUNT] = {"CALC", "DRAW", "DATA"};
static const uint8_t stat_tab_item_count[STAT_TAB_COUNT] = {5, 3, 4};

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

/* Public objects */
lv_obj_t *ui_stat_screen         = NULL;
lv_obj_t *ui_stat_results_screen = NULL;

MenuState_t stat_menu_state = {0};

/* Module-private stat data and results — accessed via Stat_GetData/GetResults/SetData */
static StatData_t    stat_data    = {{0}, {0}, 0};
static StatResults_t stat_results = {0};

const StatData_t    *Stat_GetData(void)    { return &stat_data; }
const StatResults_t *Stat_GetResults(void) { return &stat_results; }
void                 Stat_SetData(const StatData_t *src) { memcpy(&stat_data, src, sizeof(stat_data)); }

/* LVGL objects — menu screen */
static lv_obj_t *stat_tab_labels[STAT_TAB_COUNT];
static lv_obj_t *stat_item_labels[MENU_VISIBLE_ROWS];

/* LVGL objects — results screen */
static lv_obj_t *stat_results_lbl = NULL;

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

/** Format a stat value for display in the DATA editor (up to 10 chars). */
static void stat_fmt(float v, char *buf, size_t len)
{
    /* Use the engine's formatter for consistency */
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
    lv_obj_add_flag(ui_stat_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_stat_results_display();
    lvgl_unlock();
}

/** Run a DRAW tab plot operation. */
static void stat_run_draw(uint8_t item)
{
    lvgl_lock();
    lv_obj_add_flag(ui_stat_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    Graph_SetVisible(true);
    switch (item) {
    case 0: Graph_DrawHistogram(&stat_data); break;
    case 1: Graph_DrawScatter(&stat_data);   break;
    case 2: Graph_DrawXYLine(&stat_data);    break;
    }

    /* Return to normal mode so keypad works on the graph screen */
    Calc_SetMode(stat_menu_state.return_mode);
    stat_menu_state.return_mode = MODE_NORMAL;
}

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Stat_ShowMenuScreen(void)    { lv_obj_clear_flag(ui_stat_screen,         LV_OBJ_FLAG_HIDDEN); }
void Stat_HideMenuScreen(void)    { lv_obj_add_flag  (ui_stat_screen,         LV_OBJ_FLAG_HIDDEN); }
void Stat_ShowResultsScreen(void) { lv_obj_clear_flag(ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN); }
void Stat_HideResultsScreen(void) { lv_obj_add_flag  (ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_stat_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_stat_screen = screen_create(scr);

    /* Tab labels — CALC at x=4, DRAW at x=90, DATA at x=190 */
    static const int16_t tab_x[STAT_TAB_COUNT] = {4, 90, 190};
    for (int i = 0; i < STAT_TAB_COUNT; i++) {
        stat_tab_labels[i] = lv_label_create(ui_stat_screen);
        lv_obj_set_pos(stat_tab_labels[i], tab_x[i], 4);
        lv_obj_set_style_text_font(stat_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(stat_tab_labels[i],
            lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(stat_tab_labels[i], stat_tab_names[i]);
    }

    /* Item list labels */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        stat_item_labels[i] = lv_label_create(ui_stat_screen);
        lv_obj_set_pos(stat_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(stat_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(stat_item_labels[i],
            lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(stat_item_labels[i], "");
    }
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

void ui_update_stat_display(void)
{
    uint8_t tab = stat_menu_state.tab;
    for (int i = 0; i < STAT_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(stat_tab_labels[i],
            (i == (int)tab) ? lv_color_hex(COLOR_YELLOW)
                            : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    uint8_t count = stat_tab_item_count[tab];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        const char *text = "";
        if (i < (int)count) {
            if      (tab == 0) text = stat_calc_names[i];
            else if (tab == 1) text = stat_draw_names[i];
            else               text = stat_data_names[i];
        }
        lv_obj_set_style_text_color(stat_item_labels[i],
            (i == (int)stat_menu_state.cursor)
            ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(stat_item_labels[i], text);
    }
}

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

bool handle_stat_menu(Token_t t, MenuState_t *s)
{
    uint8_t tab_count  = STAT_TAB_COUNT;
    uint8_t item_count = stat_tab_item_count[s->tab];

    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s->tab, &s->cursor, NULL, tab_count, true,
                 ui_update_stat_display);
        return true;
    case TOKEN_RIGHT:
        tab_move(&s->tab, &s->cursor, NULL, tab_count, false,
                 ui_update_stat_display);
        return true;
    case TOKEN_UP:
        MenuState_MoveUp(s, item_count, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_stat_display();
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        MenuState_MoveDown(s, item_count, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_stat_display();
        lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        uint8_t item = s->cursor;
        if (s->tab == 0) {
            /* CALC */
            stat_run_calc(item);
        } else if (s->tab == 1) {
            /* DRAW */
            stat_run_draw(item);
        } else {
            /* DATA */
            switch (item) {
            case 0: /* Edit */
                Stat_EditOpen();
                break;
            case 1: /* ClrStat */
                CalcStat_Clear(&stat_data);
                lvgl_lock();
                ui_update_stat_display();
                lvgl_unlock();
                break;
            case 2: /* xSort */
                CalcStat_SortX(&stat_data);
                lvgl_lock();
                ui_update_stat_display();
                lvgl_unlock();
                break;
            case 3: /* ySort */
                CalcStat_SortY(&stat_data);
                lvgl_lock();
                ui_update_stat_display();
                lvgl_unlock();
                break;
            }
        }
        return true;
    }
    /* Digit shortcuts: 1–5 dispatch item at that slot */
    case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4: case TOKEN_5: {
        static const Token_t digit_tokens[5] = {
            TOKEN_1, TOKEN_2, TOKEN_3, TOKEN_4, TOKEN_5
        };
        for (int i = 0; i < 5; i++) {
            if (t == digit_tokens[i] && i < (int)item_count) {
                s->cursor = (uint8_t)i;
                handle_stat_menu(TOKEN_ENTER, s);
                return true;
            }
        }
        return true;
    }
    case TOKEN_CLEAR:
        menu_close(TOKEN_STAT);
        Update_Calculator_Display();
        return true;
    default:
        return false;
    }
}

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

void Stat_MenuOpen(CalcMode_t return_to)
{
    stat_menu_state.return_mode = return_to;
    stat_menu_state.tab         = 0;
    stat_menu_state.cursor      = 0;
    Calc_SetMode(MODE_STAT_MENU);
    Stat_ShowMenuScreen();
    ui_update_stat_display();
}

CalcMode_t Stat_MenuClose(void)
{
    CalcMode_t ret              = stat_menu_state.return_mode;
    stat_menu_state.return_mode = MODE_NORMAL;
    stat_menu_state.tab         = 0;
    stat_menu_state.cursor      = 0;
    return ret;
}

bool handle_stat_results(Token_t t)
{
    switch (t) {
    case TOKEN_CLEAR:
        /* CLEAR returns to normal */
        menu_close(TOKEN_STAT);
        Update_Calculator_Display();
        return true;
    default:
        /* Any other key returns to menu */
        Calc_SetMode(MODE_STAT_MENU);
        lvgl_lock();
        lv_obj_add_flag(ui_stat_results_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_stat_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_stat_display();
        lvgl_unlock();
        return true;
    }
}
