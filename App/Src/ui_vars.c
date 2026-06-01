/**
 * @file    ui_vars.c
 * @brief   VARS menu UI (VARS key).
 *
 * Five-tab menu:
 *   XY  — n, x̄, Sx, σx, ȳ, Sy, σy          (7 items, from stat_results / stat_data)
 *   Σ   — Σx, Σx², Σy, Σy², Σxy             (5 items, from stat_results / stat_data)
 *   LR  — a, b, r, RegEQ                     (4 items, from stat_results)
 *   DIM — Arow, Acol, Brow, Bcol, Crow, Ccol (6 items, from calc_matrices)
 *   RNG — Xmin…Tstep                         (10 items, from graph_state; scrolls)
 *
 * ENTER inserts the current numeric value of the selected variable into the
 * active expression buffer (main expression or Y= editor).
 *
 * Font notes (see CLAUDE.md gotcha #14 and MENU_SPECS.md):
 *   x̄ = U+E000 PUA  → \xEE\x80\x80
 *   ȳ = U+0233       → \xC8\xB3
 *   Σ = U+03A3       → \xCE\xA3
 *   σ = U+03C3       → \xCF\x83
 *   ² = U+00B2       → \xC2\xB2
 */

#include "ui_vars.h"
#include "ui_menu_screen.h"
#include "ui_shared.h"
#include "calc_engine.h"
#include "calculator_core.h"
#include "graph.h"
#include "ui_stat.h"
#include "calc_stat.h"
#include "ui_palette.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define VARS_TAB_COUNT  5

static const char * const vars_tab_names[VARS_TAB_COUNT] = {
    "XY", "\xCE\xA3", "LR", "DIM", "RNG"
};

static const int vars_tab_x[VARS_TAB_COUNT] = {4, 48, 84, 120, 188};

/* Tab 0: XY — statistics summary */
static const char * const vars_xy_names[7] = {
    "1:n",
    "2:\xEE\x80\x80",         /* x̄ U+E000 */
    "3:Sx",
    "4:\xCF\x83x",            /* σx */
    "5:\xC8\xB3",             /* ȳ U+0233 */
    "6:Sy",
    "7:\xCF\x83y",            /* σy */
};

/* Tab 1: Σ — summation variables */
static const char * const vars_sigma_names[5] = {
    "1:\xCE\xA3x",            /* Σx */
    "2:\xCE\xA3x\xC2\xB2",   /* Σx² */
    "3:\xCE\xA3y",            /* Σy */
    "4:\xCE\xA3y\xC2\xB2",   /* Σy² */
    "5:\xCE\xA3xy",           /* Σxy */
};

/* Tab 2: LR — linear regression */
static const char * const vars_lr_names[4] = {
    "1:a", "2:b", "3:r", "4:RegEQ"
};

/* Tab 3: DIM — matrix dimensions + stat list length */
static const char * const vars_dim_names[7] = {
    "1:Arow", "2:Acol", "3:Brow", "4:Bcol", "5:Crow", "6:Ccol", "7:Dim{x}"
};

/* Tab 4: RNG — window range (10 items; item 10 digit-shortcut is TOKEN_0) */
static const char * const vars_rng_names[10] = {
    "1:Xmin", "2:Xmax", "3:Xscl",
    "4:Ymin", "5:Ymax", "6:Yscl",
    "7:Xres",
    "8:Tmin", "9:Tmax", "0:Tstep",
};

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_vars_ms;

/*---------------------------------------------------------------------------
 * Value formatting — returns the current value string for a given item
 *---------------------------------------------------------------------------*/

static void vars_format_value(uint8_t tab, uint8_t item, char *buf, size_t len)
{
    float v = 0.0f;
    const StatResults_t *sr = Stat_GetResults();

    switch (tab) {
    case 0: /* XY */
        if (!sr->valid) { snprintf(buf, len, "0"); return; }
        switch (item) {
        case 0: v = sr->n;       break;
        case 1: v = sr->mean_x;  break;
        case 2: v = sr->sx;      break;
        case 3: v = sr->sigma_x; break;
        case 4: v = CalcStat_MeanY(Stat_GetData());  break;
        case 5: v = CalcStat_SxY(Stat_GetData());    break;
        case 6: v = CalcStat_SigmaY(Stat_GetData()); break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 1: /* Σ */
        if (!sr->valid) { snprintf(buf, len, "0"); return; }
        switch (item) {
        case 0: v = sr->sum_x;  break;
        case 1: v = sr->sum_x2; break;
        case 2: v = CalcStat_SumY(Stat_GetData());  break;
        case 3: v = CalcStat_SumY2(Stat_GetData()); break;
        case 4: v = CalcStat_SumXY(Stat_GetData()); break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 2: /* LR */
        if (!sr->valid) { snprintf(buf, len, "0"); return; }
        if (item == 3) {
            /* RegEQ: model-aware string per guidebook p. 7-9 */
            char abuf[16], bbuf[16];
            Calc_FormatResult(sr->reg_a, abuf, (uint8_t)sizeof(abuf));
            Calc_FormatResult(sr->reg_b, bbuf, (uint8_t)sizeof(bbuf));
            switch (sr->last_model) {
            case 1:  snprintf(buf, len, "%s+%s*ln(X)", abuf, bbuf); break; /* LnReg */
            case 2:  snprintf(buf, len, "%s*%s^X",     abuf, bbuf); break; /* ExpReg */
            case 3:  snprintf(buf, len, "%s*X^%s",     abuf, bbuf); break; /* PwrReg */
            default: snprintf(buf, len, "%s+%sX",      abuf, bbuf); break; /* LinReg */
            }
            return;
        }
        switch (item) {
        case 0: v = sr->reg_a; break;
        case 1: v = sr->reg_b; break;
        case 2: v = sr->reg_r; break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 3: /* DIM */
        switch (item) {
        case 0: v = (float)calc_matrices[0].rows;       break;
        case 1: v = (float)calc_matrices[0].cols;       break;
        case 2: v = (float)calc_matrices[1].rows;       break;
        case 3: v = (float)calc_matrices[1].cols;       break;
        case 4: v = (float)calc_matrices[2].rows;       break;
        case 5: v = (float)calc_matrices[2].cols;       break;
        case 6: v = (float)Stat_GetData()->list_len;    break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 4: { /* RNG */
        const GraphState_t *gs = Graph_GetState();
        switch (item) {
        case 0: v = gs->x_min;  break;
        case 1: v = gs->x_max;  break;
        case 2: v = gs->x_scl;  break;
        case 3: v = gs->y_min;  break;
        case 4: v = gs->y_max;  break;
        case 5: v = gs->y_scl;  break;
        case 6: v = gs->x_res;  break;
        case 7: v = gs->t_min;  break;
        case 8: v = gs->t_max;  break;
        case 9: v = gs->t_step; break;
        default: snprintf(buf, len, "0"); return;
        }
        break;
    }

    default:
        snprintf(buf, len, "0");
        return;
    }

    Calc_FormatResult(v, buf, (uint8_t)(len < 255u ? len : 255u));
}

/*---------------------------------------------------------------------------
 * Insert action
 *---------------------------------------------------------------------------*/

static void vars_do_insert(uint8_t tab, uint8_t item)
{
    char buf[64];
    vars_format_value(tab, item, buf, sizeof(buf));
    lvgl_lock();
    lv_obj_add_flag(s_vars_ms.screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();
    menu_insert_text(buf, &s_vars_ms.nav.return_mode);
}

/*---------------------------------------------------------------------------
 * MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void vars_xy_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    vars_do_insert(0, (uint8_t)idx);
}

static void vars_sigma_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    vars_do_insert(1, (uint8_t)idx);
}

static void vars_lr_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    vars_do_insert(2, (uint8_t)idx);
}

static void vars_dim_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    vars_do_insert(3, (uint8_t)idx);
}

static void vars_rng_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    vars_do_insert(4, (uint8_t)idx);
}

static void vars_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    menu_close(TOKEN_VARS);
    Update_Calculator_Display();
}

/* TOKEN_VARS closes without reopening (return true); other unrecognised
 * tokens fall through to MenuScreen_DefaultExtra for graph-nav/menu-open handling. */
static bool vars_on_extra(Token_t t, MenuScreen_t *ms)
{
    if (t == TOKEN_VARS) {
        vars_on_cancel(NULL);
        return true;
    }
    return MenuScreen_DefaultExtra(t, ms);
}

static const MenuTabDesc_t s_tabs[VARS_TAB_COUNT] = {
    { 7,  vars_xy_names,    NULL, vars_xy_on_select    },
    { 5,  vars_sigma_names, NULL, vars_sigma_on_select },
    { 4,  vars_lr_names,    NULL, vars_lr_on_select    },
    { 7,  vars_dim_names,   NULL, vars_dim_on_select   },
    { 10, vars_rng_names,   NULL, vars_rng_on_select   },
};

static const MenuScreenDesc_t s_desc = {
    .tab_count      = VARS_TAB_COUNT,
    .tab_names      = vars_tab_names,
    .tab_x          = vars_tab_x,
    .default_tab    = 0,
    .wrap_tabs      = true,
    .title          = NULL,
    .tabs           = s_tabs,
    .left_mode      = 0,
    .right_mode     = 0,
    .on_cancel      = vars_on_cancel,
    .on_tab_switch  = NULL,
    .on_extra       = vars_on_extra,
};

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Vars_ShowScreen(void) { lv_obj_clear_flag(s_vars_ms.screen, LV_OBJ_FLAG_HIDDEN); }
void Vars_HideScreen(void) { lv_obj_add_flag  (s_vars_ms.screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_vars_screen(void)
{
    MenuScreen_Init(&s_vars_ms, &s_desc, lv_scr_act());
}

/*---------------------------------------------------------------------------
 * Display Update
 *---------------------------------------------------------------------------*/

void ui_update_vars_display(void)
{
    MenuScreen_UpdateDisplay(&s_vars_ms);
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

bool handle_vars_menu(Token_t t) { return MenuScreen_HandleToken(&s_vars_ms, t); }

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

void Vars_MenuOpen(CalcMode_t return_to)
{
    s_vars_ms.nav.return_mode = return_to;
    Calc_SetMode(MODE_VARS_MENU);
    lvgl_lock();
    lv_obj_clear_flag(s_vars_ms.screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_vars_ms);
    lvgl_unlock();
}

CalcMode_t Vars_MenuClose(void)
{
    CalcMode_t ret            = s_vars_ms.nav.return_mode;
    s_vars_ms.nav.return_mode = MODE_NORMAL;
    return ret;
}
