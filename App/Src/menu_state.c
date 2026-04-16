/**
 * @file    menu_state.c
 * @brief   Shared menu navigation helpers for MenuState_t.
 *
 * No LVGL / HAL / RTOS dependencies — fully host-testable.
 */

#include "menu_state.h"

void MenuState_MoveUp(MenuState_t *s, uint8_t total, uint8_t visible)
{
    (void)total;
    (void)visible;
    if (s->cursor > 0) {
        s->cursor--;
    } else if (s->scroll > 0) {
        s->scroll--;
    }
}

void MenuState_MoveDown(MenuState_t *s, uint8_t total, uint8_t visible)
{
    uint8_t absolute = MenuState_AbsoluteIndex(s);
    if ((int)absolute + 1 < (int)total) {
        if ((int)s->cursor + 1 < (int)visible)
            s->cursor++;
        else
            s->scroll++;
    }
}

void MenuState_PrevTab(MenuState_t *s, uint8_t tab_count)
{
    (void)tab_count;
    if (s->tab > 0)
        s->tab--;
    s->cursor = 0;
    s->scroll = 0;
}

void MenuState_NextTab(MenuState_t *s, uint8_t tab_count)
{
    if ((int)s->tab + 1 < (int)tab_count)
        s->tab++;
    s->cursor = 0;
    s->scroll = 0;
}

int MenuState_DigitToIndex(Token_t t, uint8_t total)
{
    int idx;
    if (t == TOKEN_0)
        idx = 9;
    else if (t >= TOKEN_1 && t <= TOKEN_9)
        idx = (int)(t - TOKEN_1);
    else
        return -1;
    if (idx >= (int)total)
        return -1;
    return idx;
}
