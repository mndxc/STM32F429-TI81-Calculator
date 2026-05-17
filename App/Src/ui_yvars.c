/**
 * @file    ui_yvars.c
 * @brief   Y-VARS menu UI (2nd+VARS key).
 *
 * Three-tab menu (guidebook p. 3-19 — Y is the centre/default tab):
 *   OFF —  8 items: All-Off, Y₁-Off … Y₄-Off, X₁t-Off … X₃t-Off.
 *   Y   — 10 items: Y₁–Y₄, X₁t, Y₁t, X₂t, Y₂t, X₃t, Y₃t; selecting one
 *          inserts an equation reference string into the expression buffer
 *          (or Y= editor if opened from there).  Scrolls when > 7 items are
 *          visible (overflow indicator ↓/↑ at row 7 / row 1).
 *   ON  —  8 items: All-On, Y₁-On … Y₄-On, X₁t-On … X₃t-On; sets enabled
 *          flags on function equations and/or parametric pairs.
 *
 * Font notes (see CLAUDE.md gotcha #14 and MENU_SPECS.md):
 *   ₁₂₃₄ = U+2081–2084 → \xE2\x82\x81 … \xE2\x82\x84
 */

#include "ui_yvars.h"
#include "ui_menu_screen.h"
#include "ui_shared.h"
#include "calculator_core.h"
#include "calc_history.h"
#include "graph.h"
#include "ui_palette.h"
#include <stdio.h>
#include <string.h>
#include "app_common.h"

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define YVARS_TAB_COUNT     3
#define YVARS_Y_ITEMS       10  /* Y₁–Y₄, X₁t, Y₁t, X₂t, Y₂t, X₃t, Y₃t */
#define YVARS_ON_ITEMS       8  /* All-On,  Y₁-On…Y₄-On,  X₁t-On…X₃t-On  */
#define YVARS_OFF_ITEMS      8  /* All-Off, Y₁-Off…Y₄-Off, X₁t-Off…X₃t-Off */

/* Tab order per guidebook p. 3-19: OFF (left) | Y (centre/default) | ON (right) */
static const char * const yvars_tab_names[YVARS_TAB_COUNT] = {
    "OFF", "Y", "ON"
};

/* Tab bar x positions — OFF left, Y centre, ON right */
static const int yvars_tab_x[YVARS_TAB_COUNT] = { 4, 52, 92 };

/* Tab 0: OFF */
static const char * const yvars_off_names[YVARS_OFF_ITEMS] = {
    "1:All-Off",
    "2:Y\xE2\x82\x81-Off",         /* Y₁-Off */
    "3:Y\xE2\x82\x82-Off",         /* Y₂-Off */
    "4:Y\xE2\x82\x83-Off",         /* Y₃-Off */
    "5:Y\xE2\x82\x84-Off",         /* Y₄-Off */
    "6:X\xE2\x82\x81t-Off",        /* X₁t-Off */
    "7:X\xE2\x82\x82t-Off",        /* X₂t-Off */
    "8:X\xE2\x82\x83t-Off",        /* X₃t-Off */
};

/* Tab 1: Y — displayed item names */
static const char * const yvars_y_names[YVARS_Y_ITEMS] = {
    "1:Y\xE2\x82\x81",             /* Y₁ */
    "2:Y\xE2\x82\x82",             /* Y₂ */
    "3:Y\xE2\x82\x83",             /* Y₃ */
    "4:Y\xE2\x82\x84",             /* Y₄ */
    "5:X\xE2\x82\x81t",            /* X₁t */
    "6:Y\xE2\x82\x81t",            /* Y₁t */
    "7:X\xE2\x82\x82t",            /* X₂t */
    "8:Y\xE2\x82\x82t",            /* Y₂t */
    "9:X\xE2\x82\x83t",            /* X₃t */
    "0:Y\xE2\x82\x83t",            /* Y₃t */
};

/* Insert strings for the Y tab */
static const char * const yvars_y_insert[YVARS_Y_ITEMS] = {
    "Y\xE2\x82\x81",               /* Y₁ */
    "Y\xE2\x82\x82",               /* Y₂ */
    "Y\xE2\x82\x83",               /* Y₃ */
    "Y\xE2\x82\x84",               /* Y₄ */
    "X\xE2\x82\x81t",              /* X₁t */
    "Y\xE2\x82\x81t",              /* Y₁t */
    "X\xE2\x82\x82t",              /* X₂t */
    "Y\xE2\x82\x82t",              /* Y₂t */
    "X\xE2\x82\x83t",              /* X₃t */
    "Y\xE2\x82\x83t",              /* Y₃t */
};

/* Tab 2: ON */
static const char * const yvars_on_names[YVARS_ON_ITEMS] = {
    "1:All-On",
    "2:Y\xE2\x82\x81-On",          /* Y₁-On */
    "3:Y\xE2\x82\x82-On",          /* Y₂-On */
    "4:Y\xE2\x82\x83-On",          /* Y₃-On */
    "5:Y\xE2\x82\x84-On",          /* Y₄-On */
    "6:X\xE2\x82\x81t-On",         /* X₁t-On */
    "7:X\xE2\x82\x82t-On",         /* X₂t-On */
    "8:X\xE2\x82\x83t-On",         /* X₃t-On */
};

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_yvars_ms;

/* STO→Yn context: set by Yvars_OpenForSto before opening the menu */
static bool s_sto_context       = false;
static char s_sto_expr[MAX_EXPR_LEN];

/*---------------------------------------------------------------------------
 * Actions
 *---------------------------------------------------------------------------*/

/** Y tab: insert equation reference, or in STO context write expr to Y= slot. */
static void yvars_do_y_insert(uint8_t idx)
{
    lvgl_lock();
    lv_obj_add_flag(s_yvars_ms.screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_sto_context && idx < 4) {
        static const char * const ynames[4] = {
            "Y\xE2\x82\x81", "Y\xE2\x82\x82",
            "Y\xE2\x82\x83", "Y\xE2\x82\x84"
        };
        char *eq_buf = Graph_GetEquationBuf(idx);
        strncpy(eq_buf, s_sto_expr, 63);
        eq_buf[63] = '\0';

        char expr_hist[MAX_EXPR_LEN + 12];
        snprintf(expr_hist, sizeof(expr_hist), "\"%s\"->%s", s_sto_expr, ynames[idx]);
        CalcHistory_Commit(expr_hist, "Done", false, 0, 0, 0);
        CalcHistory_ResetRecallOffset();

        s_sto_context = false;
        Calc_SetMode(MODE_NORMAL);
        lvgl_lock();
        CalcHistory_UpdateDisplay();
        ui_update_status_bar();
        lvgl_unlock();
        Update_Calculator_Display();
    } else {
        s_sto_context = false;
        menu_insert_text(yvars_y_insert[idx], &s_yvars_ms.nav.return_mode);
    }
}

/**
 * ON/OFF tab: set enabled state for absolute item idx.
 *   idx 0      → All (all Y= equations + all parametric pairs)
 *   idx 1–4    → Y₁–Y₄ (function equation idx-1)
 *   idx 5–7    → X₁t–X₃t (parametric pair idx-5)
 */
static void yvars_do_enable(uint8_t idx, bool enable)
{
    if (idx == 0) {
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            Graph_SetEquationEnabled((uint8_t)i, enable);
        for (int i = 0; i < GRAPH_NUM_PARAM; i++)
            Graph_SetParamEnabled((uint8_t)i, enable);
    } else if (idx <= 4) {
        Graph_SetEquationEnabled((uint8_t)(idx - 1), enable);
    } else {
        Graph_SetParamEnabled((uint8_t)(idx - 5), enable);
    }

    lvgl_lock();
    lv_obj_add_flag(s_yvars_ms.screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    Calc_SetMode(s_yvars_ms.nav.return_mode);
    s_yvars_ms.nav.return_mode = MODE_NORMAL;

    Update_Calculator_Display();
}

/*---------------------------------------------------------------------------
 * MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void yvars_off_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    yvars_do_enable((uint8_t)idx, false);
}

static void yvars_y_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    yvars_do_y_insert((uint8_t)idx);
}

static void yvars_on_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    yvars_do_enable((uint8_t)idx, true);
}

static void yvars_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    menu_close(TOKEN_Y_VARS);
    Update_Calculator_Display();
}

/* TOKEN_Y_VARS closes without reopening (return true); other unrecognised
 * tokens fall through to MenuScreen_DefaultExtra. */
static bool yvars_on_extra(Token_t t, MenuScreen_t *ms)
{
    if (t == TOKEN_Y_VARS) {
        yvars_on_cancel(NULL);
        return true;
    }
    return MenuScreen_DefaultExtra(t, ms);
}

static const MenuTabDesc_t s_tabs[YVARS_TAB_COUNT] = {
    { YVARS_OFF_ITEMS, yvars_off_names, NULL, yvars_off_on_select },
    { YVARS_Y_ITEMS,   yvars_y_names,   NULL, yvars_y_on_select   },
    { YVARS_ON_ITEMS,  yvars_on_names,  NULL, yvars_on_on_select  },
};

static const MenuScreenDesc_t s_desc = {
    .tab_count      = YVARS_TAB_COUNT,
    .tab_names      = yvars_tab_names,
    .tab_x          = yvars_tab_x,
    .default_tab    = 1,            /* Y is the default tab per guidebook p. 3-19 */
    .wrap_tabs      = true,
    .title          = NULL,
    .tabs           = s_tabs,
    .left_mode      = 0,
    .right_mode     = 0,
    .on_cancel      = yvars_on_cancel,
    .on_tab_switch  = NULL,
    .on_extra       = yvars_on_extra,
};

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Yvars_ShowScreen(void) { lv_obj_clear_flag(s_yvars_ms.screen, LV_OBJ_FLAG_HIDDEN); }
void Yvars_HideScreen(void) { lv_obj_add_flag  (s_yvars_ms.screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_yvars_screen(void)
{
    MenuScreen_Init(&s_yvars_ms, &s_desc, lv_scr_act());
}

/*---------------------------------------------------------------------------
 * Display Update
 *---------------------------------------------------------------------------*/

void ui_update_yvars_display(void)
{
    MenuScreen_UpdateDisplay(&s_yvars_ms);
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

bool handle_yvars_menu(Token_t t) { return MenuScreen_HandleToken(&s_yvars_ms, t); }

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

void Yvars_OpenForSto(const char *expr_to_store)
{
    if (expr_to_store) {
        strncpy(s_sto_expr, expr_to_store, MAX_EXPR_LEN - 1);
        s_sto_expr[MAX_EXPR_LEN - 1] = '\0';
        s_sto_context = true;
    } else {
        s_sto_context = false;
    }
    Yvars_MenuOpen(MODE_NORMAL);
}

void Yvars_MenuOpen(CalcMode_t return_to)
{
    s_yvars_ms.nav.return_mode = return_to;
    Calc_SetMode(MODE_YVARS_MENU);
    lvgl_lock();
    lv_obj_clear_flag(s_yvars_ms.screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_yvars_ms);
    lvgl_unlock();
}

CalcMode_t Yvars_MenuClose(void)
{
    CalcMode_t ret             = s_yvars_ms.nav.return_mode;
    s_yvars_ms.nav.return_mode = MODE_NORMAL;
    return ret;
}
