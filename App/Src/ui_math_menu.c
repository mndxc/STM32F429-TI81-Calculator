/**
 * @file    ui_math_menu.c
 * @brief   MATH (MATH/NUM/HYP/PRB) and TEST menu UI.
 *
 * Extracted from calculator_core.c (UI super-module Phase 3).
 * Includes calc_internal.h as a full super-module member.
 */

#include "ui_math_menu.h"
#ifndef HOST_TEST
#  include "calc_internal.h"
#  include "ui_prgm.h"
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

typedef struct {
    uint8_t    tab;             /* 0=MATH 1=NUM 2=HYP 3=PRB */
    uint8_t    item_cursor;     /* Visible-row index of highlight */
    uint8_t    scroll_offset;
    CalcMode_t return_mode;     /* Mode to restore after selection */
} MathMenuState_t;

typedef struct {
    uint8_t    item_cursor;
    CalcMode_t return_mode;
} TestMenuState_t;

/*---------------------------------------------------------------------------
 * Private variables — MATH menu
 *---------------------------------------------------------------------------*/

lv_obj_t *ui_math_screen = NULL;
static MathMenuState_t s_math = {0};
static lv_obj_t *math_tab_labels[MATH_TAB_COUNT];
static lv_obj_t *math_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *math_scroll_ind[2];   /* [0]=top(↑), [1]=bottom(↓) — amber overlay */

static const char * const math_tab_names[MATH_TAB_COUNT] = {"MATH", "NUM", "HYP", "PRB"};
static const uint8_t math_tab_item_count[MATH_TAB_COUNT] = {8, 4, 6, 3};

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
 * Private variables — TEST menu
 *---------------------------------------------------------------------------*/

lv_obj_t  *ui_test_screen = NULL;
static TestMenuState_t s_test = {0};
static lv_obj_t  *test_title_label                  = NULL;
static lv_obj_t  *test_item_labels[TEST_ITEM_COUNT];

static const MenuItem_t test_menu_items[TEST_ITEM_COUNT] = {
    {"=",             "="},
    {"\xE2\x89\xA0",  "\xE2\x89\xA0"},   /* U+2260 ≠ */
    {">",             ">"},
    {"\xE2\x89\xA5",  "\xE2\x89\xA5"},   /* U+2265 ≥ */
    {"<",             "<"},
    {"\xE2\x89\xA4",  "\xE2\x89\xA4"},   /* U+2264 ≤ */
};

#ifndef HOST_TEST

/*---------------------------------------------------------------------------
 * Screen initialisation
 *---------------------------------------------------------------------------*/

/* Creates the MATH/NUM/HYP/PRB menu screen (hidden at startup). */
void ui_init_math_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_math_screen = screen_create(scr);

    /* Tab bar: 4 tab names at fixed x positions */
    static const int16_t tab_x[MATH_TAB_COUNT] = {4, 80, 140, 205};
    for (int i = 0; i < MATH_TAB_COUNT; i++) {
        math_tab_labels[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_tab_labels[i], tab_x[i], 4);
        lv_obj_set_style_text_font(math_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_tab_labels[i], lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(math_tab_labels[i], math_tab_names[i]);
    }

    /* MENU_VISIBLE_ROWS dynamic item labels — text set by ui_update_math_display() */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        math_item_labels[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(math_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(math_item_labels[i], "");
    }

    /* Scroll indicator overlays — amber arrow, opaque bg covers colon beneath */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        math_scroll_ind[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(math_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(math_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(math_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(math_scroll_ind[i], 0, 0);
        lv_label_set_text(math_scroll_ind[i], "");
        lv_obj_add_flag(math_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* Creates the TEST menu screen (hidden at startup). */
void ui_init_test_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_test_screen = screen_create(scr);

    /* "TEST" title at the top row */
    test_title_label = lv_label_create(ui_test_screen);
    lv_obj_set_pos(test_title_label, 4, 4);
    lv_obj_set_style_text_font(test_title_label, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(test_title_label, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(test_title_label, "TEST");

    /* Item labels — text set by ui_update_test_display() */
    for (int i = 0; i < TEST_ITEM_COUNT; i++) {
        test_item_labels[i] = lv_label_create(ui_test_screen);
        lv_obj_set_pos(test_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(test_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(test_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(test_item_labels[i], "");
    }
}

/*---------------------------------------------------------------------------
 * Display update
 *---------------------------------------------------------------------------*/

/* Redraws tab bar and visible item rows for the current MATH menu state.
 * Must be called under lvgl_lock(). */
void ui_update_math_display(void)
{
    /* Tab labels */
    for (int i = 0; i < MATH_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(math_tab_labels[i],
            (i == (int)s_math.tab) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    /* Hide both scroll indicators; re-shown below if needed */
    lv_obj_add_flag(math_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(math_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    int total = (int)math_tab_item_count[s_math.tab];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)s_math.scroll_offset + i;
        if (idx >= total) {
            lv_label_set_text(math_item_labels[i], "");
            continue;
        }
        bool more_below = (s_math.scroll_offset + MENU_VISIBLE_ROWS < (uint8_t)total)
                          && (i == MENU_VISIBLE_ROWS - 1);
        bool more_above = (s_math.scroll_offset > 0) && (i == 0);
        char buf[40];
        const char *name = math_menu_items[s_math.tab][idx].display;
        if (more_below) {
            /* Space holds the arrow's slot; amber ↓ overlay drawn on top */
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, name);
            lv_label_set_text(math_scroll_ind[1], "\xE2\x86\x93");
            lv_obj_clear_flag(math_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        } else if (more_above) {
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, name);
            lv_label_set_text(math_scroll_ind[0], "\xE2\x86\x91");
            lv_obj_clear_flag(math_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        } else {
            snprintf(buf, sizeof(buf), "%d:%s", idx + 1, name);
        }

        lv_obj_set_style_text_color(math_item_labels[i],
            (i == (int)s_math.item_cursor) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(math_item_labels[i], buf);
    }
}

/* Refreshes the TEST menu item labels based on s_test.item_cursor. */
void ui_update_test_display(void)
{
    for (int i = 0; i < TEST_ITEM_COUNT; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%s", i + 1, test_menu_items[i].display);
        lv_obj_set_style_text_color(test_item_labels[i],
            (i == (int)s_test.item_cursor) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(test_item_labels[i], buf);
    }
}

/*---------------------------------------------------------------------------
 * Private insert helpers
 *---------------------------------------------------------------------------*/

static void math_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_math.return_mode == MODE_PRGM_EDITOR) {
        prgm_editor_menu_insert(ins);
    } else if (s_math.return_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        graph_ui_yeq_insert(ins);
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    s_math.return_mode = MODE_NORMAL;
}

static void test_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_test.return_mode == MODE_PRGM_EDITOR) {
        prgm_editor_menu_insert(ins);
    } else if (s_test.return_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        graph_ui_yeq_insert(ins);
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    s_test.return_mode = MODE_NORMAL;
}

/*---------------------------------------------------------------------------
 * Private navigation helper (shared by both handlers)
 *---------------------------------------------------------------------------*/

/* Shared nav-key handler for menus that can jump to graph screens.
 * Resets ret_mode and cursor (and scroll if non-NULL) then calls nav_to().
 * Returns true if the token was a nav key, false otherwise. */
static bool menu_handle_nav_keys(Token_t t, CalcMode_t *ret_mode,
                                  uint8_t *cursor, uint8_t *scroll)
{
    CalcMode_t target;
    switch (t) {
    case TOKEN_Y_EQUALS: target = MODE_GRAPH_YEQ;   break;
    case TOKEN_RANGE:    target = MODE_GRAPH_RANGE;  break;
    case TOKEN_ZOOM:     target = MODE_GRAPH_ZOOM;   break;
    case TOKEN_GRAPH:    target = MODE_NORMAL;        break;
    case TOKEN_TRACE:    target = MODE_GRAPH_TRACE;  break;
    default:             return false;
    }
    *ret_mode = MODE_NORMAL;
    *cursor   = 0;
    if (scroll) *scroll = 0;
    nav_to(target);
    return true;
}

/*---------------------------------------------------------------------------
 * Token handlers
 *---------------------------------------------------------------------------*/

bool handle_math_menu(Token_t t)
{
    int total = (int)math_tab_item_count[s_math.tab];
    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s_math.tab, &s_math.item_cursor, &s_math.scroll_offset,
                 MATH_TAB_COUNT, true, ui_update_math_display);
        return true;
    case TOKEN_RIGHT:
        tab_move(&s_math.tab, &s_math.item_cursor, &s_math.scroll_offset,
                 MATH_TAB_COUNT, false, ui_update_math_display);
        return true;
    case TOKEN_UP:
        if (s_math.item_cursor > 0) {
            s_math.item_cursor--;
        } else if (s_math.scroll_offset > 0) {
            s_math.scroll_offset--;
        }
        lvgl_lock(); ui_update_math_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(s_math.scroll_offset + s_math.item_cursor) + 1 < total) {
            if (s_math.item_cursor < MENU_VISIBLE_ROWS - 1)
                s_math.item_cursor++;
            else if ((int)(s_math.scroll_offset + MENU_VISIBLE_ROWS) < total)
                s_math.scroll_offset++;
        }
        lvgl_lock(); ui_update_math_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)s_math.scroll_offset + (int)s_math.item_cursor;
        if (idx < total) {
            const char *ins = math_menu_items[s_math.tab][idx].insert;
            if (ins != NULL) { math_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_1 ... TOKEN_9: {
        int idx = (int)(t - TOKEN_0) - 1;
        if (idx < total) {
            const char *ins = math_menu_items[s_math.tab][idx].insert;
            if (ins != NULL) { math_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_CLEAR:
    case TOKEN_MATH:
        menu_close(TOKEN_MATH);
        return true;
    default:
        if (menu_handle_nav_keys(t, &s_math.return_mode,
                                 &s_math.item_cursor, &s_math.scroll_offset))
            return true;
    {
        CalcMode_t ret = menu_close(TOKEN_MATH);
        if (ret == MODE_GRAPH_YEQ)
            return true;
        return false; /* fall through to main switch */
    }
    }
    /* Execution reaches here only from ENTER/number when item not found */
    return true;
}

bool handle_test_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_test.item_cursor > 0) s_test.item_cursor--;
        lvgl_lock(); ui_update_test_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s_test.item_cursor < TEST_ITEM_COUNT - 1) s_test.item_cursor++;
        lvgl_lock(); ui_update_test_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        const char *ins = test_menu_items[s_test.item_cursor].insert;
        if (ins != NULL) { test_menu_insert(ins); return true; }
        break;
    }
    case TOKEN_1 ... TOKEN_6: {
        int idx = (int)(t - TOKEN_0) - 1;
        if (idx >= 0 && idx < TEST_ITEM_COUNT) {
            const char *ins = test_menu_items[idx].insert;
            if (ins != NULL) { test_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_CLEAR:
    case TOKEN_TEST:
        menu_close(TOKEN_TEST);
        return true;
    default:
        if (menu_handle_nav_keys(t, &s_test.return_mode,
                                 &s_test.item_cursor, NULL))
            return true;
    {
        CalcMode_t ret = menu_close(TOKEN_TEST);
        if (ret == MODE_GRAPH_YEQ)
            return true;
        return false; /* fall through to main switch */
    }
    }
    return true;
}

/*---------------------------------------------------------------------------
 * Open / close helpers (called from menu_open / menu_close in calculator_core.c)
 *---------------------------------------------------------------------------*/

void math_menu_open(CalcMode_t return_to)
{
    s_math.return_mode   = return_to;
    s_math.tab           = 0;
    s_math.item_cursor   = 0;
    s_math.scroll_offset = 0;
    current_mode         = MODE_MATH_MENU;
    lv_obj_clear_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_math_display();
}

void test_menu_open(CalcMode_t return_to)
{
    s_test.return_mode  = return_to;
    s_test.item_cursor  = 0;
    current_mode        = MODE_TEST_MENU;
    lv_obj_clear_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_test_display();
}

CalcMode_t math_menu_close(void)
{
    CalcMode_t ret     = s_math.return_mode;
    s_math.return_mode   = MODE_NORMAL;
    s_math.item_cursor   = 0;
    s_math.scroll_offset = 0;
    return ret;
}

CalcMode_t test_menu_close(void)
{
    CalcMode_t ret    = s_test.return_mode;
    s_test.return_mode = MODE_NORMAL;
    s_test.item_cursor = 0;
    return ret;
}

#else /* HOST_TEST */

/*---------------------------------------------------------------------------
 * HOST_TEST stubs — keep the translation unit non-empty
 *---------------------------------------------------------------------------*/

lv_obj_t *ui_math_screen = NULL;
lv_obj_t *ui_test_screen = NULL;

void ui_init_math_screen(void)         {}
void ui_init_test_screen(void)         {}
void ui_update_math_display(void)      {}
void ui_update_test_display(void)      {}
bool handle_math_menu(Token_t t)       { (void)t; return false; }
bool handle_test_menu(Token_t t)       { (void)t; return false; }
void math_menu_open(CalcMode_t r)      { (void)r; }
void test_menu_open(CalcMode_t r)      { (void)r; }
CalcMode_t math_menu_close(void)       { return MODE_NORMAL; }
CalcMode_t test_menu_close(void)       { return MODE_NORMAL; }

#endif /* HOST_TEST */
