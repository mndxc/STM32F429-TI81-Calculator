/**
 * @file    ui_cursor.c
 * @brief   UICursor_t — shared blink-timer cursor lifecycle for UI modules.
 */

#include "ui_cursor.h"

void UICursor_Init(UICursor_t *c)
{
    c->timer   = NULL;
    c->visible = false;
}

void UICursor_Start(UICursor_t *c, lv_timer_cb_t cb, uint32_t period_ms)
{
    if (c->timer) {
        lv_timer_delete(c->timer);
    }
    c->timer   = lv_timer_create(cb, period_ms, NULL);
    c->visible = true;
}

void UICursor_Stop(UICursor_t *c)
{
    if (c->timer) {
        lv_timer_delete(c->timer);
        c->timer = NULL;
    }
    c->visible = false;
}

void UICursor_Reset(UICursor_t *c)
{
    if (c->timer) {
        lv_timer_reset(c->timer);
    }
}
