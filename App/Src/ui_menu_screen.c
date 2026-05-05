/**
 * @file    ui_menu_screen.c
 * @brief   Generic scrolling menu screen module — see ui_menu_screen.h.
 */

#include "ui_menu_screen.h"
#include "ui_palette.h"
#include <string.h>

void MenuScreen_Init(MenuScreen_t *ms, const MenuScreenDesc_t *desc,
                     lv_obj_t *parent)
{
    memset(ms, 0, sizeof(*ms));
    ms->desc   = desc;
    ms->screen = screen_create(parent);

    for (int i = 0; i < (int)desc->tab_count && i < MENU_SCREEN_MAX_TABS; i++) {
        ms->tab_labels[i] = lv_label_create(ms->screen);
        lv_obj_set_pos(ms->tab_labels[i], desc->tab_x[i], 4);
        lv_obj_set_style_text_font(ms->tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ms->tab_labels[i],
            lv_color_hex(i == (int)desc->active_tab
                         ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(ms->tab_labels[i], desc->tab_names[i]);
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        ms->item_labels[i] = lv_label_create(ms->screen);
        lv_obj_set_pos(ms->item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(ms->item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ms->item_labels[i],
                                    lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ms->item_labels[i], "");
    }

    static const char * const k_arrows[2] = {
        "\xE2\x86\x91", "\xE2\x86\x93"
    };
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        ms->scroll_ind[i] = lv_label_create(ms->screen);
        lv_obj_set_pos(ms->scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(ms->scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ms->scroll_ind[i],
                                    lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(ms->scroll_ind[i],
                                  lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(ms->scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(ms->scroll_ind[i], 0, 0);
        lv_label_set_text(ms->scroll_ind[i], k_arrows[i]);
        lv_obj_add_flag(ms->scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void MenuScreen_ResetAndShow(MenuScreen_t *ms)
{
    ms->nav.cursor = 0;
    ms->nav.scroll = 0;
    MenuScreen_UpdateDisplay(ms);
}

void MenuScreen_UpdateDisplay(MenuScreen_t *ms)
{
    const MenuScreenDesc_t *d = ms->desc;
    char buf[MENU_SCREEN_LABEL_MAX];

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)ms->nav.scroll + i;
        if (idx < (int)d->item_count) {
            const char *text;
            if (d->display_labels) {
                text = d->display_labels[idx];
            } else {
                d->get_label(idx, buf, sizeof(buf));
                text = buf;
            }
            lv_label_set_text(ms->item_labels[i], text);
            lv_obj_set_style_text_color(ms->item_labels[i],
                lv_color_hex(i == (int)ms->nav.cursor
                             ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(ms->item_labels[i], "");
        }
    }

    if (ms->nav.scroll > 0)
        lv_obj_clear_flag(ms->scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ms->scroll_ind[0], LV_OBJ_FLAG_HIDDEN);

    if ((int)(ms->nav.scroll + MENU_VISIBLE_ROWS) < (int)d->item_count)
        lv_obj_clear_flag(ms->scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ms->scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

bool MenuScreen_HandleToken(MenuScreen_t *ms, Token_t t)
{
    const MenuScreenDesc_t *d = ms->desc;
    switch (t) {
    case TOKEN_UP:
        MenuState_MoveUp(&ms->nav, d->item_count, MENU_VISIBLE_ROWS);
        lvgl_lock(); MenuScreen_UpdateDisplay(ms); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        MenuState_MoveDown(&ms->nav, d->item_count, MENU_VISIBLE_ROWS);
        lvgl_lock(); MenuScreen_UpdateDisplay(ms); lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        d->on_select(MenuState_AbsoluteIndex(&ms->nav), ms->screen);
        return true;
    case TOKEN_1 ... TOKEN_9:
    case TOKEN_0: {
        int idx = MenuState_DigitToIndex(t, d->item_count);
        if (idx >= 0)
            d->on_select(idx, ms->screen);
        return true;
    }
    case TOKEN_CLEAR:
        d->on_cancel(ms->screen);
        return true;
    case TOKEN_LEFT:
        if (d->left_mode)
            d->on_tab_switch(ms->screen, d->left_mode);
        return true;
    case TOKEN_RIGHT:
        if (d->right_mode)
            d->on_tab_switch(ms->screen, d->right_mode);
        return true;
    default:
        if (d->on_extra)
            return d->on_extra(t, ms);
        return true;
    }
}
