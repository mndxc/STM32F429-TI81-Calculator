/**
 * @file    ui_prgm_io.c
 * @brief   PRGM I/O sub-menu implementation.
 *
 * Extracted from ui_prgm.c (Item 1 of INTERFACE_REFACTOR_PLAN.md).
 * Contains the I/O sub-menu string tables, navigation state, LVGL screen
 * initialization, display-update helper, and token handler.
 *
 * Supported I/O commands: Disp, Input, DispHome, DispGraph, ClrHome.
 */
#include "calc_internal.h"
#include "ui_prgm.h"
#include "ui_prgm_io.h"
#include "ui_palette.h"

/*---------------------------------------------------------------------------
 * Constants and string tables
 *---------------------------------------------------------------------------*/

#define PRGM_IO_ITEM_COUNT  5

static const char * const prgm_io_display[PRGM_IO_ITEM_COUNT] = {
    "1:Disp ",  "2:Input ", "3:DispHome", "4:DispGraph", "5:ClrHome",
};
static const char * const prgm_io_insert[PRGM_IO_ITEM_COUNT] = {
    "Disp ",    "Input ",   "DispHome",   "DispGraph",   "ClrHome",
};

/*---------------------------------------------------------------------------
 * Private state
 *---------------------------------------------------------------------------*/

lv_obj_t   *ui_prgm_io_screen            = NULL;
static uint8_t     prgm_io_cursor        = 0;
static lv_obj_t   *prgm_io_labels[PRGM_IO_ITEM_COUNT];
static lv_obj_t   *prgm_sub_tab_labels_io[3];

/*---------------------------------------------------------------------------
 * LVGL screen init
 *---------------------------------------------------------------------------*/

void ui_init_prgm_io_screen(lv_obj_t *parent)
{
    ui_prgm_io_screen = screen_create(parent);

    static const char * const sub_names[3] = {"CTL", "I/O", "EXEC"};
    static const int sub_x[3] = {4, 80, 156};
    for (int i = 0; i < 3; i++) {
        prgm_sub_tab_labels_io[i] = lv_label_create(ui_prgm_io_screen);
        lv_obj_set_pos(prgm_sub_tab_labels_io[i], sub_x[i], 4);
        lv_obj_set_style_text_font(prgm_sub_tab_labels_io[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_sub_tab_labels_io[i],
            lv_color_hex(i == 1 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(prgm_sub_tab_labels_io[i], sub_names[i]);
    }

    for (int i = 0; i < PRGM_IO_ITEM_COUNT; i++) {
        prgm_io_labels[i] = lv_label_create(ui_prgm_io_screen);
        lv_obj_set_pos(prgm_io_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_io_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_io_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_io_labels[i], "");
    }
}

/*---------------------------------------------------------------------------
 * Display helpers
 *---------------------------------------------------------------------------*/

void ui_prgm_io_reset_and_show(void)
{
    prgm_io_cursor = 0;
    ui_update_prgm_io_display();
}

void ui_update_prgm_io_display(void)
{
    for (int i = 0; i < PRGM_IO_ITEM_COUNT; i++) {
        lv_label_set_text(prgm_io_labels[i], prgm_io_display[i]);
        lv_obj_set_style_text_color(prgm_io_labels[i],
            lv_color_hex(i == (int)prgm_io_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
    }
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

bool handle_prgm_io_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_io_cursor > 0) prgm_io_cursor--;
        lvgl_lock(); ui_update_prgm_io_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (prgm_io_cursor < PRGM_IO_ITEM_COUNT - 1) prgm_io_cursor++;
        lvgl_lock(); ui_update_prgm_io_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)prgm_io_cursor;
        prgm_editor_insert_str(prgm_io_insert[idx]);
        prgm_flatten_to_store();
        prgm_submenu_return_to_editor(ui_prgm_io_screen);
        return true;
    }
    case TOKEN_1 ... TOKEN_5: {
        int idx = (int)(t - TOKEN_1);
        if (idx < PRGM_IO_ITEM_COUNT) {
            prgm_editor_insert_str(prgm_io_insert[idx]);
            prgm_flatten_to_store();
        }
        prgm_submenu_return_to_editor(ui_prgm_io_screen);
        return true;
    }
    case TOKEN_CLEAR:
        prgm_submenu_return_to_editor(ui_prgm_io_screen);
        return true;
    case TOKEN_LEFT:
        /* I/O LEFT → CTL */
        prgm_submenu_tab_switch(ui_prgm_io_screen, MODE_PRGM_CTL_MENU);
        return true;
    case TOKEN_RIGHT:
        /* I/O RIGHT → EXEC (wrap) */
        prgm_submenu_tab_switch(ui_prgm_io_screen, MODE_PRGM_EXEC_MENU);
        return true;
    default:
        return true;
    }
}
