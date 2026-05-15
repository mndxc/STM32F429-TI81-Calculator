/**
 * @file    ui_yvars.c
 * @brief   Y-VARS menu UI (2nd+VARS key).
 *
 * Three-tab menu:
 *   Y   — 10 items: Y₁–Y₄, X₁t, Y₁t, X₂t, Y₂t, X₃t, Y₃t; selecting one
 *          inserts an equation reference string into the expression buffer
 *          (or Y= editor if opened from there).  Scrolls when > 7 items are
 *          visible (overflow indicator ↓/↑ at row 7 / row 1).
 *   ON  —  8 items: All-On, Y₁-On … Y₄-On, X₁t-On … X₃t-On; sets enabled
 *          flags on function equations and/or parametric pairs.
 *   OFF —  8 items: All-Off, Y₁-Off … Y₄-Off, X₁t-Off … X₃t-Off.
 *
 * Font notes (see CLAUDE.md gotcha #14 and MENU_SPECS.md):
 *   ₁₂₃₄ = U+2081–2084 → \xE2\x82\x81 … \xE2\x82\x84
 */

#include "ui_yvars.h"
#include "menu_state.h"
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
#define YVARS_ON_ITEMS       8  /* All-On, Y₁-On…Y₄-On, X₁t-On…X₃t-On */
#define YVARS_OFF_ITEMS      8  /* All-Off, Y₁-Off…Y₄-Off, X₁t-Off…X₃t-Off */

static const char * const yvars_tab_names[YVARS_TAB_COUNT] = {
    "Y", "ON", "OFF"
};

/* Tab 0: Y — displayed item names (number prefix + label) */
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

/* Insert strings for Y tab */
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

/* Tab 1: ON */
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

/* Tab 2: OFF */
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

static const uint8_t yvars_tab_item_count[YVARS_TAB_COUNT] = {
    YVARS_Y_ITEMS, YVARS_ON_ITEMS, YVARS_OFF_ITEMS
};

/* Tab bar x positions — tuned for 3 short labels at 24px mono font */
static const int16_t yvars_tab_x[YVARS_TAB_COUNT] = { 4, 40, 92 };

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

static MenuState_t yvars_menu_state = {0};

lv_obj_t *ui_yvars_screen = NULL;

/* STO→Yn context: set by Yvars_OpenForSto before opening the menu */
static bool s_sto_context          = false;
static char s_sto_expr[MAX_EXPR_LEN];

static lv_obj_t *yvars_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *yvars_tab_labels[YVARS_TAB_COUNT];
static lv_obj_t *yvars_scroll_ind[2]; /* [0]=top(↑)  [1]=bottom(↓) */

/*---------------------------------------------------------------------------
 * Actions
 *---------------------------------------------------------------------------*/

/** Y tab: insert equation reference string, or in STO context store expr to Y= slot. */
static void yvars_do_y_insert(uint8_t idx)
{
    lvgl_lock();
    lv_obj_add_flag(ui_yvars_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_sto_context && idx < 4) {
        /* STO→Yn: write saved expression string to the selected Y= slot */
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
        menu_insert_text(yvars_y_insert[idx], &yvars_menu_state.return_mode);
    }
}

/**
 * ON/OFF tab: set enabled state for absolute item idx.
 *   idx 0        → All (all Y= equations + all parametric pairs)
 *   idx 1–4      → Y₁–Y₄ (function equation idx-1)
 *   idx 5–7      → X₁t–X₃t (parametric pair idx-5)
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
    lv_obj_add_flag(ui_yvars_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    Calc_SetMode(yvars_menu_state.return_mode);
    yvars_menu_state.return_mode = MODE_NORMAL;

    Update_Calculator_Display();
}

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Yvars_ShowScreen(void) { lv_obj_clear_flag(ui_yvars_screen, LV_OBJ_FLAG_HIDDEN); }
void Yvars_HideScreen(void) { lv_obj_add_flag(ui_yvars_screen,   LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_yvars_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_yvars_screen = screen_create(scr);

    /* Tab bar */
    for (int i = 0; i < YVARS_TAB_COUNT; i++) {
        yvars_tab_labels[i] = lv_label_create(ui_yvars_screen);
        lv_obj_set_pos(yvars_tab_labels[i], yvars_tab_x[i], 4);
        lv_obj_set_style_text_font(yvars_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(yvars_tab_labels[i],
            lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(yvars_tab_labels[i], yvars_tab_names[i]);
    }

    /* Item list */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        yvars_item_labels[i] = lv_label_create(ui_yvars_screen);
        lv_obj_set_pos(yvars_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(yvars_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(yvars_item_labels[i],
            lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(yvars_item_labels[i], "");
    }

    /* Scroll indicators — [0] top row, [1] bottom row; overlay the ':' char */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        yvars_scroll_ind[i] = lv_label_create(ui_yvars_screen);
        lv_obj_set_pos(yvars_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(yvars_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(yvars_scroll_ind[i],
            lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(yvars_scroll_ind[i],
            lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(yvars_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(yvars_scroll_ind[i], 0, 0);
        lv_label_set_text(yvars_scroll_ind[i], "");
        lv_obj_add_flag(yvars_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/*---------------------------------------------------------------------------
 * Display Update
 *---------------------------------------------------------------------------*/

void ui_update_yvars_display(void)
{
    uint8_t tab    = yvars_menu_state.tab;
    uint8_t cursor = yvars_menu_state.cursor;
    uint8_t scroll = yvars_menu_state.scroll;
    uint8_t total  = yvars_tab_item_count[tab];

    /* Tab labels */
    for (int i = 0; i < YVARS_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(yvars_tab_labels[i],
            (i == (int)tab) ? lv_color_hex(COLOR_YELLOW)
                            : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    /* Hide scroll indicators; re-show below if needed */
    lv_obj_add_flag(yvars_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(yvars_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    /* Item rows */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)scroll + i;
        if (idx >= (int)total) {
            lv_label_set_text(yvars_item_labels[i], "");
            lv_obj_set_style_text_color(yvars_item_labels[i],
                lv_color_hex(COLOR_WHITE), 0);
            continue;
        }

        const char *name = "";
        switch (tab) {
        case 0: name = yvars_y_names[idx];   break;
        case 1: name = yvars_on_names[idx];  break;
        case 2: name = yvars_off_names[idx]; break;
        default: break;
        }

        lv_label_set_text(yvars_item_labels[i], name);
        lv_obj_set_style_text_color(yvars_item_labels[i],
            (i == (int)cursor) ? lv_color_hex(COLOR_YELLOW)
                               : lv_color_hex(COLOR_WHITE), 0);

        if ((scroll > 0) && (i == 0)) {
            lv_label_set_text(yvars_scroll_ind[0], "\xE2\x86\x91");
            lv_obj_clear_flag(yvars_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        }
        if (((int)scroll + MENU_VISIBLE_ROWS < (int)total) && (i == MENU_VISIBLE_ROWS - 1)) {
            lv_label_set_text(yvars_scroll_ind[1], "\xE2\x86\x93");
            lv_obj_clear_flag(yvars_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

bool handle_yvars_menu(Token_t t)
{
    MenuState_t *s = &yvars_menu_state;
    uint8_t total = yvars_tab_item_count[s->tab];

    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s->tab, &s->cursor, &s->scroll,
                 YVARS_TAB_COUNT, true, ui_update_yvars_display);
        return true;

    case TOKEN_RIGHT:
        tab_move(&s->tab, &s->cursor, &s->scroll,
                 YVARS_TAB_COUNT, false, ui_update_yvars_display);
        return true;

    case TOKEN_UP:
        MenuState_MoveUp(s, total, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_yvars_display();
        lvgl_unlock();
        return true;

    case TOKEN_DOWN:
        MenuState_MoveDown(s, total, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_yvars_display();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER: {
        uint8_t idx = MenuState_AbsoluteIndex(s);
        if ((int)idx < (int)total) {
            switch (s->tab) {
            case 0: yvars_do_y_insert(idx);        break;
            case 1: yvars_do_enable(idx, true);    break;
            case 2: yvars_do_enable(idx, false);   break;
            }
        }
        return true;
    }

    /* Digit shortcuts: 1–9 → idx 0–8; 0 → idx 9 */
    case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4: case TOKEN_5:
    case TOKEN_6: case TOKEN_7: case TOKEN_8: case TOKEN_9: case TOKEN_0: {
        int idx = MenuState_DigitToIndex(t, total);
        if (idx >= 0) {
            if (idx < MENU_VISIBLE_ROWS) {
                s->scroll = 0;
                s->cursor = (uint8_t)idx;
            } else {
                s->scroll = (uint8_t)(idx - MENU_VISIBLE_ROWS + 1);
                s->cursor = (uint8_t)(MENU_VISIBLE_ROWS - 1);
            }
            switch (s->tab) {
            case 0: yvars_do_y_insert((uint8_t)idx);        break;
            case 1: yvars_do_enable((uint8_t)idx, true);    break;
            case 2: yvars_do_enable((uint8_t)idx, false);   break;
            }
        }
        return true;
    }

    case TOKEN_CLEAR:
    case TOKEN_Y_VARS:
        menu_close(TOKEN_Y_VARS);
        Update_Calculator_Display();
        return true;

    default:
        return false;
    }
}

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
    yvars_menu_state.return_mode = return_to;
    yvars_menu_state.tab         = 0;
    yvars_menu_state.cursor      = 0;
    yvars_menu_state.scroll      = 0;
    Calc_SetMode(MODE_YVARS_MENU);
    Yvars_ShowScreen();
    ui_update_yvars_display();
}

CalcMode_t Yvars_MenuClose(void)
{
    CalcMode_t ret               = yvars_menu_state.return_mode;
    yvars_menu_state.return_mode = MODE_NORMAL;
    yvars_menu_state.tab         = 0;
    yvars_menu_state.cursor      = 0;
    yvars_menu_state.scroll      = 0;
    return ret;
}
