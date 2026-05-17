/**
 * @file    ui_math_menu.c
 * @brief   MATH (MATH/NUM/HYP/PRB) and TEST menu UI.
 *
 * Extracted from calculator_core.c (UI super-module Phase 3).
 * Part of the calculator UI super-module.
 */

#include "ui_math_menu.h"
#include "ui_menu_screen.h"
#include <stdio.h>
#ifndef HOST_TEST
#  include "ui_shared.h"
#  include "calculator_core.h"
#  include "ui_input.h"
#  include "ui_prgm.h"
#  include "prgm_editor.h"
#  include "graph_ui.h"
#  include "ui_palette.h"
#endif

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define MATH_TAB_COUNT   4   /* MATH NUM HYP PRB */
#define TEST_ITEM_COUNT  6   /* = ≠ > ≥ < ≤    */

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/

typedef struct {
    const char *display;
    const char *insert;
} MenuItem_t;

/*---------------------------------------------------------------------------
 * Private data — MATH/NUM/HYP/PRB
 *---------------------------------------------------------------------------*/

static const char * const math_tab_names[MATH_TAB_COUNT] = {"MATH", "NUM", "HYP", "PRB"};
static const uint8_t math_tab_item_count[MATH_TAB_COUNT] = {8, 4, 6, 3};
static const int math_tab_x[MATH_TAB_COUNT] = {4, 80, 140, 205};

/* Merged display+insert data for each MATH menu item */
static const MenuItem_t math_menu_items[MATH_TAB_COUNT][8] = {
    { /* MATH tab */
        {"R>P(",    "R>P("},
        {"P>R(",    "P>R("},
        {"\xC2\xB3",                    "^3"},       /* ³  U+00B3  — display only; engine reads ^3 */
        {"\xC2\xB3\xE2\x88\x9A(",      "^(1/3)"},   /* ³√( U+00B3+U+221A — display only */
        {"!",       "!"},
        {"deg",     "\xC2\xB0"},
        {"rad",     "r"},
        {"nDeriv(", "nDeriv("},
    },
    { /* NUM tab */
        {"Round(",  "round("},
        {"IPart(",  "iPart("},
        {"FPart(",  "fPart("},
        {"Int(",    "int("},
        {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL},
    },
    { /* HYP tab */
        {"sinh(",   "sinh("},
        {"cosh(",   "cosh("},
        {"tanh(",   "tanh("},
        {"sinh\xEE\x80\x81(",  "asinh("},   /* sinh⁻¹( — display; engine reads asinh( */
        {"cosh\xEE\x80\x81(",  "acosh("},   /* cosh⁻¹( — display; engine reads acosh( */
        {"tanh\xEE\x80\x81(",  "atanh("},   /* tanh⁻¹( — display; engine reads atanh( */
        {NULL, NULL}, {NULL, NULL},
    },
    { /* PRB tab */
        {"Rand",    "rand"},
        {"nPr",     " nPr "},
        {"nCr",     " nCr "},
        {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL},
    },
};

/*---------------------------------------------------------------------------
 * Private data — TEST
 *---------------------------------------------------------------------------*/

static const MenuItem_t test_menu_items[TEST_ITEM_COUNT] = {
    {"=",             "="},
    {"\xE2\x89\xA0",  "\xE2\x89\xA0"},   /* U+2260 ≠ */
    {">",             ">"},
    {"\xE2\x89\xA5",  "\xE2\x89\xA5"},   /* U+2265 ≥ */
    {"<",             "<"},
    {"\xE2\x89\xA4",  "\xE2\x89\xA4"},   /* U+2264 ≤ */
};

/* Pre-formatted "N:symbol" display labels for MenuTabDesc_t. */
static const char * const test_display_labels[TEST_ITEM_COUNT] = {
    "1:=",
    "2:\xE2\x89\xA0",   /* 2:≠ */
    "3:>",
    "4:\xE2\x89\xA5",   /* 4:≥ */
    "5:<",
    "6:\xE2\x89\xA4",   /* 6:≤ */
};

#ifndef HOST_TEST

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_math_ms;
static MenuScreen_t s_test_ms;

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Math_ShowScreen(void) { lv_obj_clear_flag(s_math_ms.screen, LV_OBJ_FLAG_HIDDEN); }
void Math_HideScreen(void) { lv_obj_add_flag  (s_math_ms.screen, LV_OBJ_FLAG_HIDDEN); }
void Test_ShowScreen(void) { lv_obj_clear_flag(s_test_ms.screen, LV_OBJ_FLAG_HIDDEN); }
void Test_HideScreen(void) { lv_obj_add_flag  (s_test_ms.screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * MATH insert helper (preserves PRGM_EDITOR / Y= / normal routing)
 *---------------------------------------------------------------------------*/

static void math_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(s_math_ms.screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_math_ms.nav.return_mode == MODE_PRGM_EDITOR) {
        PrgmEditor_MenuInsert(ins);
    } else if (s_math_ms.nav.return_mode == MODE_GRAPH_YEQ) {
        Calc_SetMode(MODE_GRAPH_YEQ);
        graph_ui_yeq_insert(ins);
    } else {
        Calc_SetMode(MODE_NORMAL);
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    s_math_ms.nav.return_mode = MODE_NORMAL;
}

/*---------------------------------------------------------------------------
 * MATH MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void math_tab0_get_label(int idx, char *buf, size_t len) { snprintf(buf, len, "%d:%s", idx + 1, math_menu_items[0][idx].display); }
static void math_tab1_get_label(int idx, char *buf, size_t len) { snprintf(buf, len, "%d:%s", idx + 1, math_menu_items[1][idx].display); }
static void math_tab2_get_label(int idx, char *buf, size_t len) { snprintf(buf, len, "%d:%s", idx + 1, math_menu_items[2][idx].display); }
static void math_tab3_get_label(int idx, char *buf, size_t len) { snprintf(buf, len, "%d:%s", idx + 1, math_menu_items[3][idx].display); }

static void math_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    int tab = (int)s_math_ms.active_tab;
    if (tab < MATH_TAB_COUNT && idx >= 0 && idx < (int)math_tab_item_count[tab]) {
        const char *ins = math_menu_items[tab][idx].insert;
        if (ins) math_menu_insert(ins);
    }
}

static void math_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    menu_close(TOKEN_MATH);
    Update_Calculator_Display();
}

/* TOKEN_MATH closes without reopening (return true); graph-nav and other
 * menu-opening keys fall through to handle_normal_mode via DefaultExtra. */
static bool math_on_extra(Token_t t, MenuScreen_t *ms)
{
    if (t == TOKEN_MATH) {
        math_on_cancel(NULL);
        return true;
    }
    return MenuScreen_DefaultExtra(t, ms);
}

static const MenuTabDesc_t s_math_tabs[MATH_TAB_COUNT] = {
    { 8, NULL, math_tab0_get_label, math_on_select },
    { 4, NULL, math_tab1_get_label, math_on_select },
    { 6, NULL, math_tab2_get_label, math_on_select },
    { 3, NULL, math_tab3_get_label, math_on_select },
};

static const MenuScreenDesc_t s_math_desc = {
    .tab_count      = MATH_TAB_COUNT,
    .tab_names      = math_tab_names,
    .tab_x          = math_tab_x,
    .default_tab    = 0,
    .wrap_tabs      = false,
    .title          = NULL,
    .tabs           = s_math_tabs,
    .left_mode      = 0,
    .right_mode     = 0,
    .on_cancel      = math_on_cancel,
    .on_tab_switch  = NULL,
    .on_extra       = math_on_extra,
};

/*---------------------------------------------------------------------------
 * TEST MenuScreen_t descriptor and callbacks
 *---------------------------------------------------------------------------*/

static void test_on_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    if (idx < 0 || idx >= TEST_ITEM_COUNT) return;
    const char *ins = test_menu_items[idx].insert;
    if (!ins) return;

    lvgl_lock();
    lv_obj_add_flag(s_test_ms.screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_test_ms.nav.return_mode == MODE_PRGM_EDITOR) {
        PrgmEditor_MenuInsert(ins);
    } else if (s_test_ms.nav.return_mode == MODE_GRAPH_YEQ) {
        Calc_SetMode(MODE_GRAPH_YEQ);
        graph_ui_yeq_insert(ins);
    } else {
        Calc_SetMode(MODE_NORMAL);
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    s_test_ms.nav.return_mode = MODE_NORMAL;
}

static void test_on_cancel(lv_obj_t *screen)
{
    (void)screen;
    menu_close(TOKEN_TEST);
}

static const MenuTabDesc_t s_test_tab = {
    TEST_ITEM_COUNT, test_display_labels, NULL, test_on_select
};

static const MenuScreenDesc_t s_test_desc = {
    .tab_count      = 0,
    .tab_names      = NULL,
    .tab_x          = NULL,
    .default_tab    = 0,
    .wrap_tabs      = false,
    .title          = "TEST",
    .tabs           = &s_test_tab,
    .left_mode      = 0,
    .right_mode     = 0,
    .on_cancel      = test_on_cancel,
    .on_tab_switch  = NULL,
    .on_extra       = MenuScreen_DefaultExtra,
};

/*---------------------------------------------------------------------------
 * Screen initialisation
 *---------------------------------------------------------------------------*/

void ui_init_math_screen(void)
{
    MenuScreen_Init(&s_math_ms, &s_math_desc, lv_scr_act());
}

void ui_init_test_screen(void)
{
    MenuScreen_Init(&s_test_ms, &s_test_desc, lv_scr_act());
}

/*---------------------------------------------------------------------------
 * Display update
 *---------------------------------------------------------------------------*/

void ui_update_math_display(void)
{
    MenuScreen_UpdateDisplay(&s_math_ms);
}

/*---------------------------------------------------------------------------
 * Token handlers
 *---------------------------------------------------------------------------*/

bool handle_math_menu(Token_t t) { return MenuScreen_HandleToken(&s_math_ms, t); }
bool handle_test_menu(Token_t t) { return MenuScreen_HandleToken(&s_test_ms, t); }

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

void math_menu_open(CalcMode_t return_to)
{
    s_math_ms.nav.return_mode = return_to;
    Calc_SetMode(MODE_MATH_MENU);
    lvgl_lock();
    lv_obj_clear_flag(s_math_ms.screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_math_ms);
    lvgl_unlock();
}

void test_menu_open(CalcMode_t return_to)
{
    s_test_ms.nav.return_mode = return_to;
    Calc_SetMode(MODE_TEST_MENU);
    lvgl_lock();
    lv_obj_clear_flag(s_test_ms.screen, LV_OBJ_FLAG_HIDDEN);
    MenuScreen_ResetAndShow(&s_test_ms);
    lvgl_unlock();
}

CalcMode_t math_menu_close(void)
{
    CalcMode_t ret            = s_math_ms.nav.return_mode;
    s_math_ms.nav.return_mode = MODE_NORMAL;
    return ret;
}

CalcMode_t test_menu_close(void)
{
    CalcMode_t ret            = s_test_ms.nav.return_mode;
    s_test_ms.nav.return_mode = MODE_NORMAL;
    return ret;
}

#else /* HOST_TEST */

/*---------------------------------------------------------------------------
 * HOST_TEST stubs — keep the translation unit non-empty
 *---------------------------------------------------------------------------*/

void ui_init_math_screen(void)    {}
void ui_init_test_screen(void)    {}
void ui_update_math_display(void) {}
bool handle_math_menu(Token_t t)  { (void)t; return false; }
bool handle_test_menu(Token_t t)  { (void)t; return false; }
void math_menu_open(CalcMode_t r) { (void)r; }
void test_menu_open(CalcMode_t r) { (void)r; }
CalcMode_t math_menu_close(void)  { return MODE_NORMAL; }
CalcMode_t test_menu_close(void)  { return MODE_NORMAL; }

#endif /* HOST_TEST */
