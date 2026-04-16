/**
 * @file    ui_yvars.c
 * @brief   Y-VARS menu UI (2nd+VARS key).
 *
 * Three-tab menu:
 *   Y   — 4 items: Y₁–Y₄; selecting one inserts an equation reference string
 *          into the expression buffer (or Y= editor if opened from there).
 *   ON  — 5 items: All-On, Y₁-On … Y₄-On; sets graph_state.enabled[] true.
 *   OFF — 5 items: All-Off, Y₁-Off … Y₄-Off; sets graph_state.enabled[] false.
 *
 * Parametric pairs (X₁t/Y₁t etc.) are deferred until parametric Y-VARS
 * support is added.
 *
 * Font notes (see CLAUDE.md gotcha #14 and MENU_SPECS.md):
 *   ₁₂₃₄ = U+2081–2084 → \xE2\x82\x81 … \xE2\x82\x84
 */

/* TODO: Navigation state uses bespoke variables. Migrate to MenuState_t from
 * menu_state.h — see INTERFACE_REFACTOR_PLAN.md Item 3 (ui_vars.c proof-of-concept). */

#include "ui_yvars.h"
#include "calc_internal.h"
#include "ui_palette.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define YVARS_TAB_COUNT     3
#define YVARS_Y_ITEMS       4   /* Y₁–Y₄ */
#define YVARS_ON_ITEMS      5   /* All-On, Y₁-On … Y₄-On */
#define YVARS_OFF_ITEMS     5   /* All-Off, Y₁-Off … Y₄-Off */

static const char * const yvars_tab_names[YVARS_TAB_COUNT] = {
    "Y", "ON", "OFF"
};

/* Tab 0: Y — equation reference insert strings (displayed in menu) */
static const char * const yvars_y_names[YVARS_Y_ITEMS] = {
    "1:Y\xE2\x82\x81",   /* Y₁ */
    "2:Y\xE2\x82\x82",   /* Y₂ */
    "3:Y\xE2\x82\x83",   /* Y₃ */
    "4:Y\xE2\x82\x84",   /* Y₄ */
};

/* Insert strings for Y tab — "Y₁" through "Y₄" (4 bytes each) */
static const char * const yvars_y_insert[YVARS_Y_ITEMS] = {
    "Y\xE2\x82\x81",
    "Y\xE2\x82\x82",
    "Y\xE2\x82\x83",
    "Y\xE2\x82\x84",
};

/* Tab 1: ON */
static const char * const yvars_on_names[YVARS_ON_ITEMS] = {
    "1:All-On",
    "2:Y\xE2\x82\x81-On",
    "3:Y\xE2\x82\x82-On",
    "4:Y\xE2\x82\x83-On",
    "5:Y\xE2\x82\x84-On",
};

/* Tab 2: OFF */
static const char * const yvars_off_names[YVARS_OFF_ITEMS] = {
    "1:All-Off",
    "2:Y\xE2\x82\x81-Off",
    "3:Y\xE2\x82\x82-Off",
    "4:Y\xE2\x82\x83-Off",
    "5:Y\xE2\x82\x84-Off",
};

static const uint8_t yvars_tab_item_count[YVARS_TAB_COUNT] = {
    YVARS_Y_ITEMS, YVARS_ON_ITEMS, YVARS_OFF_ITEMS
};

/* Tab bar x positions — tuned for 3 short labels at 24px mono font */
static const int16_t yvars_tab_x[YVARS_TAB_COUNT] = { 4, 40, 92 };

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

YVarsMenuState_t yvars_menu_state = { 0, 0, MODE_NORMAL };

lv_obj_t *ui_yvars_screen = NULL;

static lv_obj_t *yvars_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *yvars_tab_labels[YVARS_TAB_COUNT];

/*---------------------------------------------------------------------------
 * Actions
 *---------------------------------------------------------------------------*/

/** Y tab: insert equation reference string into the active editor. */
static void yvars_do_y_insert(uint8_t idx)
{
    lvgl_lock();
    lv_obj_add_flag(ui_yvars_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();
    menu_insert_text(yvars_y_insert[idx], &yvars_menu_state.return_mode);
}

/** ON/OFF tab: set enabled state for item idx; idx 0 = All. */
static void yvars_do_enable(uint8_t idx, bool enable)
{
    if (idx == 0) {
        /* All-On / All-Off */
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            graph_state.enabled[i] = enable;
    } else {
        graph_state.enabled[idx - 1] = enable;
    }

    /* Close menu and return to normal mode */
    lvgl_lock();
    lv_obj_add_flag(ui_yvars_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    current_mode = yvars_menu_state.return_mode;
    yvars_menu_state.return_mode = MODE_NORMAL;

    Update_Calculator_Display();
}

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
}

/*---------------------------------------------------------------------------
 * Display Update
 *---------------------------------------------------------------------------*/

void ui_update_yvars_display(void)
{
    uint8_t tab    = yvars_menu_state.tab;
    uint8_t cursor = yvars_menu_state.item_cursor;
    uint8_t total  = yvars_tab_item_count[tab];

    /* Tab labels */
    for (int i = 0; i < YVARS_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(yvars_tab_labels[i],
            (i == (int)tab) ? lv_color_hex(COLOR_YELLOW)
                            : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    /* Item rows — all items fit in MENU_VISIBLE_ROWS (no scroll needed) */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        if (i >= (int)total) {
            lv_label_set_text(yvars_item_labels[i], "");
            lv_obj_set_style_text_color(yvars_item_labels[i],
                lv_color_hex(COLOR_WHITE), 0);
            continue;
        }

        const char *name = "";
        switch (tab) {
        case 0: name = yvars_y_names[i];   break;
        case 1: name = yvars_on_names[i];  break;
        case 2: name = yvars_off_names[i]; break;
        default: break;
        }

        lv_label_set_text(yvars_item_labels[i], name);
        lv_obj_set_style_text_color(yvars_item_labels[i],
            (i == (int)cursor) ? lv_color_hex(COLOR_YELLOW)
                               : lv_color_hex(COLOR_WHITE), 0);
    }
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

bool handle_yvars_menu(Token_t t)
{
    YVarsMenuState_t *s = &yvars_menu_state;
    uint8_t total = yvars_tab_item_count[s->tab];

    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s->tab, &s->item_cursor, NULL,
                 YVARS_TAB_COUNT, true, ui_update_yvars_display);
        return true;

    case TOKEN_RIGHT:
        tab_move(&s->tab, &s->item_cursor, NULL,
                 YVARS_TAB_COUNT, false, ui_update_yvars_display);
        return true;

    case TOKEN_UP:
        if (s->item_cursor > 0)
            s->item_cursor--;
        lvgl_lock();
        ui_update_yvars_display();
        lvgl_unlock();
        return true;

    case TOKEN_DOWN:
        if ((int)s->item_cursor + 1 < (int)total)
            s->item_cursor++;
        lvgl_lock();
        ui_update_yvars_display();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER: {
        uint8_t idx = s->item_cursor;
        if (idx < total) {
            switch (s->tab) {
            case 0: yvars_do_y_insert(idx);        break;
            case 1: yvars_do_enable(idx, true);    break;
            case 2: yvars_do_enable(idx, false);   break;
            }
        }
        return true;
    }

    /* Digit shortcuts 1–4 for Y tab; 1–5 for ON/OFF tabs */
    case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4: case TOKEN_5: {
        int idx = (int)(t - TOKEN_1);   /* 0-based */
        if (idx < (int)total) {
            s->item_cursor = (uint8_t)idx;
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
