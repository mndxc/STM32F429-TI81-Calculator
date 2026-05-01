/**
 * @file    ui_stat_edit.c
 * @brief   STAT DATA list editor — screen init, display update, and token handler.
 *
 * Extracted from ui_stat.c; manages MODE_STAT_EDIT and the associated LVGL screen.
 * Data is accessed via Stat_GetData() / the shared stat_data pointer in ui_stat.c.
 *
 * Column states:
 *   col=0  editing X value of current row
 *   col=1  editing Y value of current row
 *   col=2  "= cursor" row-select mode (TI-81 guidebook p. 7-5) — pressing
 *          INS inserts a new (0,0) row before the cursor; DEL deletes the row
 */

#include "ui_stat.h"
#include "calc_stat.h"
#include "calc_engine.h"
#include "calculator_core.h"
#include "ui_shared.h"
#include "ui_palette.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STAT_EDIT_VISIBLE   7   /* Visible rows in the DATA editor */

/*---------------------------------------------------------------------------
 * Screen object — non-static so test stubs can supply a NULL definition.
 *---------------------------------------------------------------------------*/

lv_obj_t *ui_stat_edit_screen = NULL;

void Stat_ShowEditScreen(void) { lv_obj_clear_flag(ui_stat_edit_screen, LV_OBJ_FLAG_HIDDEN); }
void Stat_HideEditScreen(void) { lv_obj_add_flag  (ui_stat_edit_screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * Editor state
 *---------------------------------------------------------------------------*/

static uint8_t  stat_edit_row    = 0;
static uint8_t  stat_edit_col    = 0;   /* 0=X, 1=Y, 2=row-select (= cursor) */
static uint8_t  stat_edit_scroll = 0;
static char     stat_edit_buf[20];
static uint8_t  stat_edit_len    = 0;

/* LVGL objects */
static lv_obj_t *stat_edit_title_lbl   = NULL;
static lv_obj_t *stat_edit_row_labels[STAT_EDIT_VISIBLE];
static lv_obj_t *stat_edit_up_lbl      = NULL;
static lv_obj_t *stat_edit_down_lbl    = NULL;

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

static void stat_fmt(float v, char *buf, size_t len)
{
    Calc_FormatResult(v, buf, (uint8_t)(len < 255 ? len : 255));
}

/** Load the value at (row, col) into stat_edit_buf.
 *  At col=2 (row-select mode) the buffer is always cleared. */
static void stat_edit_load_cell(void)
{
    if (stat_edit_col == 2) {
        stat_edit_buf[0] = '\0';
        stat_edit_len    = 0;
        return;
    }
    const StatData_t *d = Stat_GetData();
    if (stat_edit_row < d->list_len) {
        float v = (stat_edit_col == 0)
                  ? d->list_x[stat_edit_row]
                  : d->list_y[stat_edit_row];
        stat_fmt(v, stat_edit_buf, sizeof(stat_edit_buf));
    } else {
        stat_edit_buf[0] = '\0';
    }
    stat_edit_len = (uint8_t)strlen(stat_edit_buf);
}

/** Commit stat_edit_buf to the data list at (row, col).
 *  Extends list_len if writing to the new-row slot. */
static void stat_edit_commit(void)
{
    if (stat_edit_len == 0) return;

    float v = strtof(stat_edit_buf, NULL);

    /* Write via SetData to keep data encapsulated in ui_stat.c */
    StatData_t tmp;
    memcpy(&tmp, Stat_GetData(), sizeof(tmp));

    if (stat_edit_col == 0) {
        tmp.list_x[stat_edit_row] = v;
    } else {
        tmp.list_y[stat_edit_row] = v;
    }

    if (stat_edit_row >= tmp.list_len) {
        if (stat_edit_row < STAT_MAX_POINTS) {
            tmp.list_len = (uint8_t)(stat_edit_row + 1);
            if (stat_edit_col == 0) tmp.list_y[stat_edit_row] = 0.0f;
            else                    tmp.list_x[stat_edit_row] = 0.0f;
        }
    }

    Stat_SetData(&tmp);
}

/** Clamp stat_edit_scroll so the cursor row is always visible. */
static void stat_edit_fix_scroll(void)
{
    if ((int)stat_edit_row < (int)stat_edit_scroll)
        stat_edit_scroll = stat_edit_row;
    if ((int)stat_edit_row >= (int)stat_edit_scroll + STAT_EDIT_VISIBLE)
        stat_edit_scroll = (uint8_t)(stat_edit_row - STAT_EDIT_VISIBLE + 1);
}

/*---------------------------------------------------------------------------
 * Screen initialisation
 *---------------------------------------------------------------------------*/

void ui_init_stat_edit_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_stat_edit_screen = screen_create(scr);

    /* Title row */
    stat_edit_title_lbl = lv_label_create(ui_stat_edit_screen);
    lv_obj_set_pos(stat_edit_title_lbl, 4, 4);
    lv_obj_set_style_text_font(stat_edit_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(stat_edit_title_lbl,
        lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(stat_edit_title_lbl, "L1  X           Y");

    /* Visible data rows */
    for (int i = 0; i < STAT_EDIT_VISIBLE; i++) {
        stat_edit_row_labels[i] = lv_label_create(ui_stat_edit_screen);
        lv_obj_set_pos(stat_edit_row_labels[i], 4, 34 + i * 30);
        lv_obj_set_style_text_font(stat_edit_row_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(stat_edit_row_labels[i],
            lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(stat_edit_row_labels[i], "");
    }

    /* Up / down overflow arrows */
    stat_edit_up_lbl = lv_label_create(ui_stat_edit_screen);
    lv_obj_set_pos(stat_edit_up_lbl, 4, 34);
    lv_obj_set_style_text_font(stat_edit_up_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(stat_edit_up_lbl,
        lv_color_hex(COLOR_AMBER), 0);
    lv_label_set_text(stat_edit_up_lbl, "\xE2\x86\x91");
    lv_obj_add_flag(stat_edit_up_lbl, LV_OBJ_FLAG_HIDDEN);

    stat_edit_down_lbl = lv_label_create(ui_stat_edit_screen);
    lv_obj_set_pos(stat_edit_down_lbl, 4, 34 + (STAT_EDIT_VISIBLE - 1) * 30);
    lv_obj_set_style_text_font(stat_edit_down_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(stat_edit_down_lbl,
        lv_color_hex(COLOR_AMBER), 0);
    lv_label_set_text(stat_edit_down_lbl, "\xE2\x86\x93");
    lv_obj_add_flag(stat_edit_down_lbl, LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------------------
 * Display update
 *---------------------------------------------------------------------------*/

void ui_update_stat_edit_display(void)
{
    const StatData_t *d = Stat_GetData();
    uint8_t total = (d->list_len < STAT_MAX_POINTS)
                    ? d->list_len + 1u
                    : STAT_MAX_POINTS;

    bool more_above = (stat_edit_scroll > 0);
    bool more_below = ((int)stat_edit_scroll + STAT_EDIT_VISIBLE < (int)total);

    if (more_above) lv_obj_clear_flag(stat_edit_up_lbl, LV_OBJ_FLAG_HIDDEN);
    else            lv_obj_add_flag  (stat_edit_up_lbl, LV_OBJ_FLAG_HIDDEN);

    if (more_below) lv_obj_clear_flag(stat_edit_down_lbl, LV_OBJ_FLAG_HIDDEN);
    else            lv_obj_add_flag  (stat_edit_down_lbl, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < STAT_EDIT_VISIBLE; i++) {
        int row = (int)stat_edit_scroll + i;
        if (row >= (int)total) {
            lv_label_set_text(stat_edit_row_labels[i], "");
            lv_obj_set_style_text_color(stat_edit_row_labels[i],
                lv_color_hex(COLOR_WHITE), 0);
            continue;
        }

        char xbuf[20], ybuf[20];
        if (row < (int)d->list_len) {
            stat_fmt(d->list_x[row], xbuf, sizeof(xbuf));
            stat_fmt(d->list_y[row], ybuf, sizeof(ybuf));
        } else {
            xbuf[0] = '\0';
            ybuf[0] = '\0';
        }

        bool is_cursor_row = (row == (int)stat_edit_row);

        /* Override the active cell with the edit buffer (col 0 or 1 only). */
        if (is_cursor_row) {
            if (stat_edit_col == 0)
                snprintf(xbuf, sizeof(xbuf), "%s", stat_edit_buf);
            else if (stat_edit_col == 1)
                snprintf(ybuf, sizeof(ybuf), "%s", stat_edit_buf);
            /* col == 2: show stored values unchanged */
        }

        /* In row-select mode the '>' prefix replaces the leading space so the
         * user can see the cursor is on the row-level (= sign) position. */
        char line[50];
        bool is_row_select = is_cursor_row && (stat_edit_col == 2);
        if (is_row_select)
            snprintf(line, sizeof(line), ">%2d: %-10s %-10s",
                     row + 1, xbuf, ybuf);
        else
            snprintf(line, sizeof(line), " %2d: %-10s %-10s",
                     row + 1, xbuf, ybuf);

        lv_obj_set_style_text_color(stat_edit_row_labels[i],
            is_cursor_row ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(stat_edit_row_labels[i], line);
    }
}

/*---------------------------------------------------------------------------
 * Open (called from handle_stat_menu DATA→Edit)
 *---------------------------------------------------------------------------*/

void Stat_EditOpen(void)
{
    stat_edit_row    = 0;
    stat_edit_col    = 0;
    stat_edit_scroll = 0;
    stat_edit_load_cell();
    Calc_SetMode(MODE_STAT_EDIT);
    lvgl_lock();
    Stat_HideMenuScreen();
    Stat_ShowEditScreen();
    ui_update_stat_edit_display();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Token handler for MODE_STAT_EDIT
 *---------------------------------------------------------------------------*/

bool handle_stat_edit(Token_t t)
{
    const StatData_t *d = Stat_GetData();
    uint8_t total = (d->list_len < STAT_MAX_POINTS)
                    ? d->list_len + 1u
                    : STAT_MAX_POINTS;

    /* Pressing a digit/decimal/neg key while in row-select mode (col=2)
     * silently transitions to X-editing with a fresh empty buffer, matching
     * the "original value is cleared automatically when you begin typing"
     * behaviour described on guidebook p. 7-5. */
    bool is_input_key = ((t >= TOKEN_0 && t <= TOKEN_9) ||
                         t == TOKEN_DECIMAL || t == TOKEN_NEG);
    if (stat_edit_col == 2 && is_input_key) {
        stat_edit_col = 0;
        stat_edit_len = 0;
        stat_edit_buf[0] = '\0';
    }

    switch (t) {
    /* Digit and decimal input */
    case TOKEN_0: case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4:
    case TOKEN_5: case TOKEN_6: case TOKEN_7: case TOKEN_8: case TOKEN_9: {
        if (stat_edit_len < (uint8_t)(sizeof(stat_edit_buf) - 1)) {
            static const char digit_ch[10] = "0123456789";
            int idx = (int)t - (int)TOKEN_0;
            if (idx >= 0 && idx < 10) {
                stat_edit_buf[stat_edit_len++] = digit_ch[idx];
                stat_edit_buf[stat_edit_len]   = '\0';
            }
            lvgl_lock();
            ui_update_stat_edit_display();
            lvgl_unlock();
        }
        return true;
    }
    case TOKEN_DECIMAL:
        if (stat_edit_len < (uint8_t)(sizeof(stat_edit_buf) - 1)) {
            bool has_dot = false;
            for (uint8_t i = 0; i < stat_edit_len; i++)
                if (stat_edit_buf[i] == '.') { has_dot = true; break; }
            if (!has_dot) {
                stat_edit_buf[stat_edit_len++] = '.';
                stat_edit_buf[stat_edit_len]   = '\0';
                lvgl_lock();
                ui_update_stat_edit_display();
                lvgl_unlock();
            }
        }
        return true;
    case TOKEN_NEG:
        if (stat_edit_len == 0 ||
            (stat_edit_len == 1 && stat_edit_buf[0] == '-')) {
            if (stat_edit_buf[0] == '-') {
                memmove(stat_edit_buf, stat_edit_buf + 1, (size_t)stat_edit_len);
                stat_edit_len--;
            } else {
                memmove(stat_edit_buf + 1, stat_edit_buf, (size_t)stat_edit_len + 1);
                stat_edit_buf[0] = '-';
                stat_edit_len++;
            }
        } else if (stat_edit_buf[0] != '-') {
            if (stat_edit_len < (uint8_t)(sizeof(stat_edit_buf) - 1)) {
                memmove(stat_edit_buf + 1, stat_edit_buf, (size_t)stat_edit_len + 1);
                stat_edit_buf[0] = '-';
                stat_edit_len++;
            }
        }
        lvgl_lock();
        ui_update_stat_edit_display();
        lvgl_unlock();
        return true;

    /* DEL — character delete when editing a cell; row delete in row-select mode */
    case TOKEN_DEL:
        if (stat_edit_col == 2) {
            /* Row-level delete (guidebook p. 7-5) */
            if (stat_edit_row < d->list_len) {
                StatData_t tmp;
                memcpy(&tmp, d, sizeof(tmp));
                CalcStat_DeleteRow(&tmp, stat_edit_row);
                Stat_SetData(&tmp);
                /* Clamp cursor after deletion */
                d = Stat_GetData();
                if (d->list_len > 0 && stat_edit_row >= d->list_len)
                    stat_edit_row = (uint8_t)(d->list_len - 1);
                stat_edit_fix_scroll();
                stat_edit_load_cell();
                lvgl_lock();
                ui_update_stat_edit_display();
                lvgl_unlock();
            }
        } else if (stat_edit_len > 0) {
            stat_edit_len--;
            stat_edit_buf[stat_edit_len] = '\0';
            lvgl_lock();
            ui_update_stat_edit_display();
            lvgl_unlock();
        }
        return true;

    /* INS — insert a new (0,0) row before the cursor in row-select mode */
    case TOKEN_INS:
        if (stat_edit_col == 2) {
            StatData_t tmp;
            memcpy(&tmp, d, sizeof(tmp));
            if (CalcStat_InsertRow(&tmp, stat_edit_row)) {
                Stat_SetData(&tmp);
                stat_edit_col = 0;
                stat_edit_fix_scroll();
                stat_edit_load_cell();
                lvgl_lock();
                ui_update_stat_edit_display();
                lvgl_unlock();
            }
        }
        return true;

    case TOKEN_ENTER:
    case TOKEN_DOWN: {
        if (stat_edit_col == 2) {
            /* In row-select mode: advance one row, stay in row-select */
            d     = Stat_GetData();
            total = (d->list_len < STAT_MAX_POINTS)
                    ? d->list_len + 1u : STAT_MAX_POINTS;
            if ((int)stat_edit_row + 1 < (int)total)
                stat_edit_row++;
        } else {
            stat_edit_commit();
            if (stat_edit_col == 0) {
                stat_edit_col = 1;
            } else {
                stat_edit_col = 0;
                if ((int)stat_edit_row + 1 < (int)STAT_MAX_POINTS) {
                    stat_edit_row++;
                    d     = Stat_GetData();
                    total = (d->list_len < STAT_MAX_POINTS)
                            ? d->list_len + 1u : STAT_MAX_POINTS;
                    if (stat_edit_row >= total) stat_edit_row = (uint8_t)(total - 1);
                }
            }
        }
        stat_edit_fix_scroll();
        stat_edit_load_cell();
        lvgl_lock();
        ui_update_stat_edit_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_UP: {
        if (stat_edit_col == 2) {
            /* In row-select mode: move up one row, stay in row-select */
            if (stat_edit_row > 0) stat_edit_row--;
        } else {
            stat_edit_commit();
            if (stat_edit_col == 1) {
                stat_edit_col = 0;
            } else {
                if (stat_edit_row > 0) {
                    stat_edit_row--;
                    stat_edit_col = 1;
                }
            }
        }
        stat_edit_fix_scroll();
        stat_edit_load_cell();
        lvgl_lock();
        ui_update_stat_edit_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_LEFT:
        stat_edit_commit();
        if (stat_edit_col == 1) {
            stat_edit_col = 0;
        } else if (stat_edit_col == 0) {
            stat_edit_col = 2;          /* enter row-select (= cursor) */
        } else {                        /* col == 2 */
            if (stat_edit_row > 0) stat_edit_row--;
            /* stay in col=2 */
        }
        stat_edit_fix_scroll();
        stat_edit_load_cell();
        lvgl_lock();
        ui_update_stat_edit_display();
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        stat_edit_commit();
        if (stat_edit_col == 2) {
            stat_edit_col = 0;          /* exit row-select, edit X */
        } else if (stat_edit_col == 0) {
            stat_edit_col = 1;
        } else if (stat_edit_row + 1 < total) {
            stat_edit_row++;
            stat_edit_col = 0;
        }
        stat_edit_fix_scroll();
        stat_edit_load_cell();
        lvgl_lock();
        ui_update_stat_edit_display();
        lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
        if (stat_edit_col == 2 || stat_edit_len == 0) {
            if (stat_edit_col != 2) stat_edit_commit();
            Calc_SetMode(MODE_STAT_MENU);
            lvgl_lock();
            Stat_HideEditScreen();
            Stat_ShowMenuScreen();
            ui_update_stat_display();
            lvgl_unlock();
        } else {
            stat_edit_len    = 0;
            stat_edit_buf[0] = '\0';
            lvgl_lock();
            ui_update_stat_edit_display();
            lvgl_unlock();
        }
        return true;
    default:
        return false;
    }
}
