/**
 * @file    ui_prgm_exec.c
 * @brief   PRGM EXEC sub-menu implementation.
 *
 * Extracted from ui_prgm.c (Item 1 of INTERFACE_REFACTOR_PLAN.md).
 * Contains the EXEC sub-menu navigation state, LVGL screen initialization,
 * display-update helper, and token handler.
 *
 * The EXEC sub-menu is a subroutine slot picker (37 slots: 1–9, 0, A–Z, θ)
 * shown when PRGM→EXEC is pressed from inside the program editor.
 */
#include "calc_internal.h"
#include "ui_prgm.h"
#include "ui_prgm_exec.h"
#include "ui_palette.h"
#include <stdio.h>

/*---------------------------------------------------------------------------
 * Private state
 *---------------------------------------------------------------------------*/

lv_obj_t   *ui_prgm_exec_screen             = NULL;
static lv_obj_t   *prgm_sub_tab_labels_exec[3];
static lv_obj_t   *prgm_exec_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_exec_scroll_ind[2];
static uint8_t     prgm_exec_cursor         = 0;
static uint8_t     prgm_exec_scroll         = 0;

/*---------------------------------------------------------------------------
 * LVGL screen init
 *---------------------------------------------------------------------------*/

void ui_init_prgm_exec_screen(lv_obj_t *parent)
{
    ui_prgm_exec_screen = screen_create(parent);

    static const char * const sub_names[3] = {"CTL", "I/O", "EXEC"};
    static const int sub_x[3] = {4, 80, 156};
    for (int i = 0; i < 3; i++) {
        prgm_sub_tab_labels_exec[i] = lv_label_create(ui_prgm_exec_screen);
        lv_obj_set_pos(prgm_sub_tab_labels_exec[i], sub_x[i], 4);
        lv_obj_set_style_text_font(prgm_sub_tab_labels_exec[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_sub_tab_labels_exec[i],
            lv_color_hex(i == 2 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(prgm_sub_tab_labels_exec[i], sub_names[i]);
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_exec_labels[i] = lv_label_create(ui_prgm_exec_screen);
        lv_obj_set_pos(prgm_exec_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_exec_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_exec_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_exec_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        prgm_exec_scroll_ind[i] = lv_label_create(ui_prgm_exec_screen);
        lv_obj_set_pos(prgm_exec_scroll_ind[i], 4, 30 + row * 30);
        lv_obj_set_style_text_font(prgm_exec_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_exec_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(prgm_exec_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(prgm_exec_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(prgm_exec_scroll_ind[i], 0, 0);
        lv_label_set_text(prgm_exec_scroll_ind[i], i == 0 ? "\xE2\x86\x91" : "\xE2\x86\x93");
        lv_obj_add_flag(prgm_exec_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/*---------------------------------------------------------------------------
 * Display helpers
 *---------------------------------------------------------------------------*/

void ui_prgm_exec_reset_and_show(void)
{
    prgm_exec_cursor = 0;
    prgm_exec_scroll = 0;
    ui_update_prgm_exec_display();
}

void ui_update_prgm_exec_display(void)
{
    char buf[24];
    char id[3];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int slot = (int)prgm_exec_scroll + i;
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_slot_id_str((uint8_t)slot, id);
            const char *name = Prgm_GetName((uint8_t)slot);
            if (name[0] != '\0')
                snprintf(buf, sizeof(buf), "%s:Prgm%s  %s", id, id, name);
            else
                snprintf(buf, sizeof(buf), "%s:Prgm%s", id, id);
            lv_label_set_text(prgm_exec_labels[i], buf);
            lv_obj_set_style_text_color(prgm_exec_labels[i],
                lv_color_hex(i == (int)prgm_exec_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_exec_labels[i], "");
        }
    }
    if (prgm_exec_scroll > 0)
        lv_obj_clear_flag(prgm_exec_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_exec_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_exec_scroll + MENU_VISIBLE_ROWS) < PRGM_MAX_PROGRAMS)
        lv_obj_clear_flag(prgm_exec_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_exec_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

bool handle_prgm_exec_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_exec_cursor > 0)
            prgm_exec_cursor--;
        else if (prgm_exec_scroll > 0)
            prgm_exec_scroll--;
        lvgl_lock(); ui_update_prgm_exec_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(prgm_exec_scroll + prgm_exec_cursor) + 1 < PRGM_MAX_PROGRAMS) {
            if (prgm_exec_cursor < MENU_VISIBLE_ROWS - 1)
                prgm_exec_cursor++;
            else if ((int)(prgm_exec_scroll + MENU_VISIBLE_ROWS) < PRGM_MAX_PROGRAMS)
                prgm_exec_scroll++;
        }
        lvgl_lock(); ui_update_prgm_exec_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int slot = (int)prgm_exec_scroll + (int)prgm_exec_cursor;
        if (slot < PRGM_MAX_PROGRAMS) {
            char slot_id[3];
            prgm_slot_id_str((uint8_t)slot, slot_id);
            const char *uname = Prgm_GetName((uint8_t)slot);
            char ins[PRGM_NAME_LEN + 6]; /* "prgm" + name/id + NUL */
            snprintf(ins, sizeof(ins), "prgm%s", uname[0] != '\0' ? uname : slot_id);
            prgm_editor_insert_str(ins);
            prgm_flatten_to_store();
        }
        prgm_submenu_return_to_editor(ui_prgm_exec_screen);
        return true;
    }
    case TOKEN_1 ... TOKEN_9: {
        int slot = (int)(t - TOKEN_1);
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
                ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
            prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
            return handle_prgm_exec_menu(TOKEN_ENTER);
        }
        return true;
    }
    case TOKEN_0: {
        int slot = 9;
        prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
            ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
        prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
        return handle_prgm_exec_menu(TOKEN_ENTER);
    }
    case TOKEN_A ... TOKEN_Z: {
        int slot = 10 + (int)(t - TOKEN_A);
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
                ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
            prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
            return handle_prgm_exec_menu(TOKEN_ENTER);
        }
        return true;
    }
    case TOKEN_THETA: {
        int slot = 36;
        prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
            ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
        prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
        return handle_prgm_exec_menu(TOKEN_ENTER);
    }
    case TOKEN_CLEAR:
        prgm_submenu_return_to_editor(ui_prgm_exec_screen);
        return true;
    case TOKEN_LEFT:
        /* EXEC LEFT → I/O */
        prgm_submenu_tab_switch(ui_prgm_exec_screen, MODE_PRGM_IO_MENU);
        return true;
    case TOKEN_RIGHT:
        /* EXEC RIGHT → CTL (wrap) */
        prgm_submenu_tab_switch(ui_prgm_exec_screen, MODE_PRGM_CTL_MENU);
        return true;
    default:
        return true;
    }
}
