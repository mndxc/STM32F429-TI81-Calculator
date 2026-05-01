/**
 * @file    ui_prgm_mode.c
 * @brief   PRGM MODE sub-menu — NUMBER and GRAPH tabs.
 *
 * Accessed via [MODE] from the program editor.  Each tab inserts a command
 * string into the current program line; prgm_exec.c handles execution.
 *
 * Guidebook: pp. 8-16, 8-18 — "Setting Modes from a Program".
 */
#include "ui_shared.h"
#include "ui_prgm.h"
#include "ui_prgm_mode.h"
#include "ui_palette.h"

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
 * Tab bar geometry
 *---------------------------------------------------------------------------*/

static const char * const mode_tab_names[2] = {"NUMBER", "GRAPH"};
static const int           mode_tab_x[2]    = {4, 110};

/*---------------------------------------------------------------------------
 * Private state — NUMBER screen
 *---------------------------------------------------------------------------*/

lv_obj_t   *ui_prgm_mode_num_screen         = NULL;
static uint8_t    prgm_mode_num_cursor       = 0;
static lv_obj_t  *prgm_mode_num_labels[PRGM_MODE_NUM_COUNT];
static lv_obj_t  *prgm_mode_num_tab_lbl[2];

/*---------------------------------------------------------------------------
 * Private state — GRAPH screen
 *---------------------------------------------------------------------------*/

lv_obj_t   *ui_prgm_mode_gph_screen         = NULL;
static uint8_t    prgm_mode_gph_cursor       = 0;
static uint8_t    prgm_mode_gph_scroll       = 0;
static lv_obj_t  *prgm_mode_gph_labels[MENU_VISIBLE_ROWS];
static lv_obj_t  *prgm_mode_gph_scroll_ind[2];
static lv_obj_t  *prgm_mode_gph_tab_lbl[2];

/*---------------------------------------------------------------------------
 * LVGL helpers — shared scroll indicator setup
 *---------------------------------------------------------------------------*/

static void make_scroll_ind(lv_obj_t *parent, lv_obj_t **out, int row, const char *arrow)
{
    *out = lv_label_create(parent);
    lv_obj_set_pos(*out, 18, 30 + row * 30);
    lv_obj_set_style_text_font(*out, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(*out, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_color(*out, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(*out, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(*out, 0, 0);
    lv_label_set_text(*out, arrow);
    lv_obj_add_flag(*out, LV_OBJ_FLAG_HIDDEN);
}

static void make_tab_bar(lv_obj_t *parent, lv_obj_t *tab_lbls[2], int active_tab)
{
    for (int i = 0; i < 2; i++) {
        tab_lbls[i] = lv_label_create(parent);
        lv_obj_set_pos(tab_lbls[i], mode_tab_x[i], 4);
        lv_obj_set_style_text_font(tab_lbls[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(tab_lbls[i],
            lv_color_hex(i == active_tab ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(tab_lbls[i], mode_tab_names[i]);
    }
}

/*---------------------------------------------------------------------------
 * LVGL screen init
 *---------------------------------------------------------------------------*/

void ui_init_prgm_mode_screens(lv_obj_t *parent)
{
    /* --- NUMBER screen --- */
    ui_prgm_mode_num_screen = screen_create(parent);
    make_tab_bar(ui_prgm_mode_num_screen, prgm_mode_num_tab_lbl, 0);

    for (int i = 0; i < PRGM_MODE_NUM_COUNT; i++) {
        prgm_mode_num_labels[i] = lv_label_create(ui_prgm_mode_num_screen);
        lv_obj_set_pos(prgm_mode_num_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_mode_num_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_mode_num_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_mode_num_labels[i], prgm_mode_num_display[i]);
    }

    /* --- GRAPH screen --- */
    ui_prgm_mode_gph_screen = screen_create(parent);
    make_tab_bar(ui_prgm_mode_gph_screen, prgm_mode_gph_tab_lbl, 1);

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_mode_gph_labels[i] = lv_label_create(ui_prgm_mode_gph_screen);
        lv_obj_set_pos(prgm_mode_gph_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_mode_gph_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_mode_gph_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_mode_gph_labels[i], "");
    }

    make_scroll_ind(ui_prgm_mode_gph_screen, &prgm_mode_gph_scroll_ind[0],
                    0, "\xE2\x86\x91");
    make_scroll_ind(ui_prgm_mode_gph_screen, &prgm_mode_gph_scroll_ind[1],
                    MENU_VISIBLE_ROWS - 1, "\xE2\x86\x93");
}

/*---------------------------------------------------------------------------
 * Display update helpers
 *---------------------------------------------------------------------------*/

static void ui_update_prgm_mode_num_display(void)
{
    for (int i = 0; i < PRGM_MODE_NUM_COUNT; i++) {
        lv_label_set_text(prgm_mode_num_labels[i], prgm_mode_num_display[i]);
        lv_obj_set_style_text_color(prgm_mode_num_labels[i],
            lv_color_hex(i == (int)prgm_mode_num_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
    }
}

static void ui_update_prgm_mode_gph_display(void)
{
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)prgm_mode_gph_scroll + i;
        if (idx < PRGM_MODE_GPH_COUNT) {
            lv_label_set_text(prgm_mode_gph_labels[i], prgm_mode_gph_display[idx]);
            lv_obj_set_style_text_color(prgm_mode_gph_labels[i],
                lv_color_hex(i == (int)prgm_mode_gph_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_mode_gph_labels[i], "");
        }
    }
    if (prgm_mode_gph_scroll > 0)
        lv_obj_clear_flag(prgm_mode_gph_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_mode_gph_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_mode_gph_scroll + MENU_VISIBLE_ROWS) < PRGM_MODE_GPH_COUNT)
        lv_obj_clear_flag(prgm_mode_gph_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_mode_gph_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

void ui_prgm_mode_num_reset_and_show(void)
{
    prgm_mode_num_cursor = 0;
    ui_update_prgm_mode_num_display();
}

void ui_prgm_mode_gph_reset_and_show(void)
{
    prgm_mode_gph_cursor = 0;
    prgm_mode_gph_scroll = 0;
    ui_update_prgm_mode_gph_display();
}

/*---------------------------------------------------------------------------
 * Token handlers
 *---------------------------------------------------------------------------*/

bool handle_prgm_mode_number(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_mode_num_cursor > 0) prgm_mode_num_cursor--;
        lvgl_lock(); ui_update_prgm_mode_num_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (prgm_mode_num_cursor < PRGM_MODE_NUM_COUNT - 1) prgm_mode_num_cursor++;
        lvgl_lock(); ui_update_prgm_mode_num_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)prgm_mode_num_cursor;
        prgm_editor_insert_str(prgm_mode_num_insert[idx]);
        prgm_flatten_to_store();
        prgm_submenu_return_to_editor(ui_prgm_mode_num_screen);
        return true;
    }
    case TOKEN_1 ... TOKEN_7: {
        int idx = (int)(t - TOKEN_1);
        prgm_editor_insert_str(prgm_mode_num_insert[idx]);
        prgm_flatten_to_store();
        prgm_submenu_return_to_editor(ui_prgm_mode_num_screen);
        return true;
    }
    case TOKEN_CLEAR:
        prgm_submenu_return_to_editor(ui_prgm_mode_num_screen);
        return true;
    case TOKEN_RIGHT:
        prgm_submenu_tab_switch(ui_prgm_mode_num_screen, MODE_PRGM_MODE_GRAPH);
        return true;
    default:
        return true;
    }
}

bool handle_prgm_mode_graph(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_mode_gph_cursor > 0)
            prgm_mode_gph_cursor--;
        else if (prgm_mode_gph_scroll > 0)
            prgm_mode_gph_scroll--;
        lvgl_lock(); ui_update_prgm_mode_gph_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(prgm_mode_gph_scroll + prgm_mode_gph_cursor) + 1 < PRGM_MODE_GPH_COUNT) {
            if (prgm_mode_gph_cursor < MENU_VISIBLE_ROWS - 1)
                prgm_mode_gph_cursor++;
            else if ((int)(prgm_mode_gph_scroll + MENU_VISIBLE_ROWS) < PRGM_MODE_GPH_COUNT)
                prgm_mode_gph_scroll++;
        }
        lvgl_lock(); ui_update_prgm_mode_gph_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)prgm_mode_gph_scroll + (int)prgm_mode_gph_cursor;
        if (idx < PRGM_MODE_GPH_COUNT) {
            prgm_editor_insert_str(prgm_mode_gph_insert[idx]);
            prgm_flatten_to_store();
        }
        prgm_submenu_return_to_editor(ui_prgm_mode_gph_screen);
        return true;
    }
    case TOKEN_1 ... TOKEN_9: {
        int idx = (int)(t - TOKEN_1);
        if (idx < PRGM_MODE_GPH_COUNT) {
            prgm_editor_insert_str(prgm_mode_gph_insert[idx]);
            prgm_flatten_to_store();
        }
        prgm_submenu_return_to_editor(ui_prgm_mode_gph_screen);
        return true;
    }
    case TOKEN_0:
        /* "O:Polar" — item index 9 */
        prgm_editor_insert_str(prgm_mode_gph_insert[9]);
        prgm_flatten_to_store();
        prgm_submenu_return_to_editor(ui_prgm_mode_gph_screen);
        return true;
    case TOKEN_CLEAR:
        prgm_submenu_return_to_editor(ui_prgm_mode_gph_screen);
        return true;
    case TOKEN_LEFT:
        prgm_submenu_tab_switch(ui_prgm_mode_gph_screen, MODE_PRGM_MODE_NUMBER);
        return true;
    default:
        return true;
    }
}
