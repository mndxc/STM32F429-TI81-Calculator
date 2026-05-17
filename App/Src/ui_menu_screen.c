/**
 * @file    ui_menu_screen.c
 * @brief   Generic scrolling menu screen module — see ui_menu_screen.h.
 */

#include "ui_menu_screen.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * HOST_TEST: stub out all LVGL-dependent functions so the navigation logic
 * (HandleToken, IsMenuOpeningKey, DefaultExtra) can be unit-tested on the host.
 * ------------------------------------------------------------------------- */

#ifdef HOST_TEST

void lvgl_lock(void);
void lvgl_unlock(void);

void MenuScreen_Init(MenuScreen_t *ms, const MenuScreenDesc_t *desc,
                     lv_obj_t *parent)
{
    (void)parent;
    memset(ms, 0, sizeof(*ms));
    ms->desc       = desc;
    ms->active_tab = desc->default_tab;
}

void MenuScreen_SetTab(MenuScreen_t *ms, uint8_t idx)
{
    ms->active_tab    = idx;
    ms->nav.cursor    = 0;
    ms->nav.scroll    = 0;
}

void MenuScreen_ResetAndShow(MenuScreen_t *ms)
{
    MenuScreen_SetTab(ms, ms->desc->default_tab);
}

void MenuScreen_UpdateDisplay(MenuScreen_t *ms) { (void)ms; }

#else /* !HOST_TEST — full LVGL implementation */

#include "ui_palette.h"

void MenuScreen_Init(MenuScreen_t *ms, const MenuScreenDesc_t *desc,
                     lv_obj_t *parent)
{
    memset(ms, 0, sizeof(*ms));
    ms->desc       = desc;
    ms->active_tab = desc->default_tab;
    ms->screen     = screen_create(parent);

    /* Title label (shown when tab_count == 0 and title is set) */
    if (desc->tab_count == 0 && desc->title) {
        ms->title_label = lv_label_create(ms->screen);
        lv_obj_set_pos(ms->title_label, 4, 4);
        lv_obj_set_style_text_font(ms->title_label, &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ms->title_label,
                                    lv_color_hex(COLOR_YELLOW), 0);
        lv_label_set_text(ms->title_label, desc->title);
    }

    /* Tab bar */
    for (int i = 0; i < (int)desc->tab_count && i < MENU_SCREEN_MAX_TABS; i++) {
        ms->tab_labels[i] = lv_label_create(ms->screen);
        lv_obj_set_pos(ms->tab_labels[i], desc->tab_x[i], 4);
        lv_obj_set_style_text_font(ms->tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ms->tab_labels[i],
            lv_color_hex(i == (int)ms->active_tab
                         ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(ms->tab_labels[i], desc->tab_names[i]);
    }

    /* Item rows */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        ms->item_labels[i] = lv_label_create(ms->screen);
        lv_obj_set_pos(ms->item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(ms->item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ms->item_labels[i],
                                    lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ms->item_labels[i], "");
    }

    /* Scroll indicators */
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

void MenuScreen_SetTab(MenuScreen_t *ms, uint8_t idx)
{
    ms->active_tab = idx;
    ms->nav.cursor = 0;
    ms->nav.scroll = 0;
    for (int i = 0; i < ms->desc->tab_count && i < MENU_SCREEN_MAX_TABS; i++) {
        if (ms->tab_labels[i]) {
            lv_obj_set_style_text_color(ms->tab_labels[i],
                lv_color_hex(i == (int)idx
                             ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        }
    }
    MenuScreen_UpdateDisplay(ms);
}

void MenuScreen_ResetAndShow(MenuScreen_t *ms)
{
    MenuScreen_SetTab(ms, ms->desc->default_tab);
}

void MenuScreen_UpdateDisplay(MenuScreen_t *ms)
{
    const MenuScreenDesc_t *d   = ms->desc;
    const MenuTabDesc_t    *tab = &d->tabs[ms->active_tab];
    char buf[MENU_SCREEN_LABEL_MAX];

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)ms->nav.scroll + i;
        if (idx < (int)tab->item_count) {
            const char *text;
            if (tab->display_labels) {
                text = tab->display_labels[idx];
            } else {
                tab->get_label(idx, buf, sizeof(buf));
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

    if ((int)(ms->nav.scroll + MENU_VISIBLE_ROWS) < (int)tab->item_count)
        lv_obj_clear_flag(ms->scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ms->scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

#endif /* HOST_TEST */

/* -------------------------------------------------------------------------
 * Navigation logic — shared between HOST_TEST and production builds.
 * No LVGL calls here except lvgl_lock/unlock wrappers around SetTab/UpdateDisplay.
 * ------------------------------------------------------------------------- */

bool MenuScreen_HandleToken(MenuScreen_t *ms, Token_t t)
{
    const MenuScreenDesc_t *d   = ms->desc;
    const MenuTabDesc_t    *tab = &d->tabs[ms->active_tab];

    switch (t) {
    case TOKEN_UP:
        MenuState_MoveUp(&ms->nav, tab->item_count, MENU_VISIBLE_ROWS);
        lvgl_lock(); MenuScreen_UpdateDisplay(ms); lvgl_unlock();
        return true;

    case TOKEN_DOWN:
        MenuState_MoveDown(&ms->nav, tab->item_count, MENU_VISIBLE_ROWS);
        lvgl_lock(); MenuScreen_UpdateDisplay(ms); lvgl_unlock();
        return true;

    case TOKEN_ENTER:
        if (tab->on_select)
            tab->on_select(MenuState_AbsoluteIndex(&ms->nav), ms->screen);
        return true;

    case TOKEN_1 ... TOKEN_9:
    case TOKEN_0: {
        int idx = MenuState_DigitToIndex(t, tab->item_count);
        if (idx >= 0 && tab->on_select)
            tab->on_select(idx, ms->screen);
        return true;
    }

    case TOKEN_CLEAR:
        if (d->on_cancel)
            d->on_cancel(ms->screen);
        return true;

    case TOKEN_LEFT:
        if (d->on_tab_switch && d->left_mode) {
            d->on_tab_switch(ms->screen, d->left_mode);
        } else if (d->tab_count > 0) {
            uint8_t next;
            if (ms->active_tab > 0)
                next = ms->active_tab - 1;
            else if (d->wrap_tabs)
                next = (uint8_t)(d->tab_count - 1);
            else
                return true;
            lvgl_lock(); MenuScreen_SetTab(ms, next); lvgl_unlock();
        }
        return true;

    case TOKEN_RIGHT:
        if (d->on_tab_switch && d->right_mode) {
            d->on_tab_switch(ms->screen, d->right_mode);
        } else if (d->tab_count > 0) {
            uint8_t next;
            if ((uint8_t)(ms->active_tab + 1) < d->tab_count)
                next = (uint8_t)(ms->active_tab + 1);
            else if (d->wrap_tabs)
                next = 0;
            else
                return true;
            lvgl_lock(); MenuScreen_SetTab(ms, next); lvgl_unlock();
        }
        return true;

    default:
        if (d->on_extra)
            return d->on_extra(t, ms);
        return MenuScreen_DefaultExtra(t, ms);
    }
}

bool MenuScreen_IsMenuOpeningKey(Token_t t)
{
    switch (t) {
    case TOKEN_MATH:
    case TOKEN_TEST:
    case TOKEN_VARS:
    case TOKEN_MATRX:
    case TOKEN_PRGM:
    case TOKEN_Y_VARS:
    case TOKEN_STAT:
    case TOKEN_DRAW:
        return true;
    default:
        return false;
    }
}

bool MenuScreen_DefaultExtra(Token_t t, MenuScreen_t *ms)
{
    bool is_graph_nav = (t == TOKEN_Y_EQUALS || t == TOKEN_RANGE ||
                         t == TOKEN_ZOOM     || t == TOKEN_GRAPH  ||
                         t == TOKEN_TRACE);
    if (is_graph_nav || MenuScreen_IsMenuOpeningKey(t)) {
        if (ms->desc->on_cancel)
            ms->desc->on_cancel(ms->screen);
        return false;
    }
    return false;
}
