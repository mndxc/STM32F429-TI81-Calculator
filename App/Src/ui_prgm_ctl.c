/**
 * @file    ui_prgm_ctl.c
 * @brief   PRGM CTL sub-menu implementation.
 *
 * Extracted from ui_prgm.c (Item 1 of INTERFACE_REFACTOR_PLAN.md).
 * Contains the CTL sub-menu string tables, navigation state, LVGL screen
 * initialization, display-update helper, and token handler.
 *
 * Supported CTL commands: Lbl, Goto, If, IS>(, DS<(, Pause, End, Stop.
 */
#include "calc_internal.h"
#include "ui_prgm.h"
#include "ui_prgm_ctl.h"
#include "ui_palette.h"

/*---------------------------------------------------------------------------
 * Constants and string tables
 *---------------------------------------------------------------------------*/

#define PRGM_CTL_ITEM_COUNT  8

static const char * const prgm_ctl_display[PRGM_CTL_ITEM_COUNT] = {
    "1:Lbl ",   "2:Goto ",  "3:If ",   "4:IS>(",
    "5:DS<(",   "6:Pause",  "7:End",   "8:Stop",
};
static const char * const prgm_ctl_insert[PRGM_CTL_ITEM_COUNT] = {
    "Lbl ",     "Goto ",    "If ",     "IS>(",
    "DS<(",     "Pause",    "End",     "Stop",
};

/*---------------------------------------------------------------------------
 * Private state
 *---------------------------------------------------------------------------*/

lv_obj_t   *ui_prgm_ctl_screen           = NULL;
static uint8_t     prgm_ctl_cursor       = 0;
static uint8_t     prgm_ctl_scroll       = 0;
static lv_obj_t   *prgm_ctl_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_ctl_scroll_ind[2];
static lv_obj_t   *prgm_sub_tab_labels_ctl[3];

/*---------------------------------------------------------------------------
 * LVGL screen init
 *---------------------------------------------------------------------------*/

void ui_init_prgm_ctl_screen(lv_obj_t *parent)
{
    ui_prgm_ctl_screen = screen_create(parent);

    static const char * const sub_names[3] = {"CTL", "I/O", "EXEC"};
    static const int sub_x[3] = {4, 80, 156};
    for (int i = 0; i < 3; i++) {
        prgm_sub_tab_labels_ctl[i] = lv_label_create(ui_prgm_ctl_screen);
        lv_obj_set_pos(prgm_sub_tab_labels_ctl[i], sub_x[i], 4);
        lv_obj_set_style_text_font(prgm_sub_tab_labels_ctl[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_sub_tab_labels_ctl[i],
            lv_color_hex(i == 0 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(prgm_sub_tab_labels_ctl[i], sub_names[i]);
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_ctl_labels[i] = lv_label_create(ui_prgm_ctl_screen);
        lv_obj_set_pos(prgm_ctl_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_ctl_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_ctl_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_ctl_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        prgm_ctl_scroll_ind[i] = lv_label_create(ui_prgm_ctl_screen);
        lv_obj_set_pos(prgm_ctl_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(prgm_ctl_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_ctl_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(prgm_ctl_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(prgm_ctl_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(prgm_ctl_scroll_ind[i], 0, 0);
        lv_label_set_text(prgm_ctl_scroll_ind[i], i == 0 ? "\xE2\x86\x91" : "\xE2\x86\x93");
        lv_obj_add_flag(prgm_ctl_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/*---------------------------------------------------------------------------
 * Display helpers
 *---------------------------------------------------------------------------*/

void ui_prgm_ctl_reset_and_show(void)
{
    prgm_ctl_cursor = 0;
    prgm_ctl_scroll = 0;
    ui_update_prgm_ctl_display();
}

void ui_update_prgm_ctl_display(void)
{
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)prgm_ctl_scroll + i;
        if (idx < PRGM_CTL_ITEM_COUNT) {
            lv_label_set_text(prgm_ctl_labels[i], prgm_ctl_display[idx]);
            lv_obj_set_style_text_color(prgm_ctl_labels[i],
                lv_color_hex(i == (int)prgm_ctl_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_ctl_labels[i], "");
        }
    }
    if (prgm_ctl_scroll > 0)
        lv_obj_clear_flag(prgm_ctl_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_ctl_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_ctl_scroll + MENU_VISIBLE_ROWS) < PRGM_CTL_ITEM_COUNT)
        lv_obj_clear_flag(prgm_ctl_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_ctl_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

bool handle_prgm_ctl_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_ctl_cursor > 0)
            prgm_ctl_cursor--;
        else if (prgm_ctl_scroll > 0)
            prgm_ctl_scroll--;
        lvgl_lock(); ui_update_prgm_ctl_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(prgm_ctl_scroll + prgm_ctl_cursor) + 1 < PRGM_CTL_ITEM_COUNT) {
            if (prgm_ctl_cursor < MENU_VISIBLE_ROWS - 1)
                prgm_ctl_cursor++;
            else if ((int)(prgm_ctl_scroll + MENU_VISIBLE_ROWS) < PRGM_CTL_ITEM_COUNT)
                prgm_ctl_scroll++;
        }
        lvgl_lock(); ui_update_prgm_ctl_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)prgm_ctl_scroll + (int)prgm_ctl_cursor;
        if (idx < PRGM_CTL_ITEM_COUNT) {
            prgm_editor_insert_str(prgm_ctl_insert[idx]);
            prgm_flatten_to_store();
        }
        prgm_submenu_return_to_editor(ui_prgm_ctl_screen);
        return true;
    }
    case TOKEN_1 ... TOKEN_9: {
        int idx = (int)(t - TOKEN_1);
        if (idx < PRGM_CTL_ITEM_COUNT) {
            prgm_editor_insert_str(prgm_ctl_insert[idx]);
            prgm_flatten_to_store();
        }
        prgm_submenu_return_to_editor(ui_prgm_ctl_screen);
        return true;
    }
    case TOKEN_CLEAR:
        prgm_submenu_return_to_editor(ui_prgm_ctl_screen);
        return true;
    case TOKEN_RIGHT:
        /* CTL RIGHT → I/O */
        prgm_submenu_tab_switch(ui_prgm_ctl_screen, MODE_PRGM_IO_MENU);
        return true;
    case TOKEN_LEFT:
        /* CTL LEFT → EXEC (wrap) */
        prgm_submenu_tab_switch(ui_prgm_ctl_screen, MODE_PRGM_EXEC_MENU);
        return true;
    default:
        return true;
    }
}
