/**
 * @file    ui_vars.c
 * @brief   VARS menu UI (VARS key).
 *
 * Five-tab menu:
 *   XY  — n, x̄, Sx, σx, ȳ, Sy, σy          (7 items, from stat_results / stat_data)
 *   Σ   — Σx, Σx², Σy, Σy², Σxy             (5 items, from stat_results / stat_data)
 *   LR  — a, b, r, RegEQ                     (4 items, from stat_results)
 *   DIM — Arow, Acol, Brow, Bcol, Crow, Ccol (6 items, from calc_matrices)
 *   RNG — Xmin…Tstep                         (10 items, from graph_state; scrolls)
 *
 * ENTER inserts the current numeric value of the selected variable into the
 * active expression buffer (main expression or Y= editor).
 *
 * Font notes (see CLAUDE.md gotcha #14 and MENU_SPECS.md):
 *   x̄ = U+E000 PUA  → \xEE\x80\x80
 *   ȳ = U+0233       → \xC8\xB3
 *   Σ = U+03A3       → \xCE\xA3
 *   σ = U+03C3       → \xCF\x83
 *   ² = U+00B2       → \xC2\xB2
 */

#include "ui_vars.h"
#include "menu_state.h"
#include "calc_internal.h"
#include "graph.h"
#include "ui_palette.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define VARS_TAB_COUNT  5

static const char * const vars_tab_names[VARS_TAB_COUNT] = {
    "XY", "\xCE\xA3", "LR", "DIM", "RNG"
};

static const uint8_t vars_tab_item_count[VARS_TAB_COUNT] = { 7, 5, 4, 6, 10 };

/* Tab 0: XY — statistics summary */
static const char * const vars_xy_names[7] = {
    "1:n",
    "2:\xEE\x80\x80",         /* x̄ U+E000 */
    "3:Sx",
    "4:\xCF\x83x",            /* σx */
    "5:\xC8\xB3",             /* ȳ U+0233 */
    "6:Sy",
    "7:\xCF\x83y",            /* σy */
};

/* Tab 1: Σ — summation variables */
static const char * const vars_sigma_names[5] = {
    "1:\xCE\xA3x",            /* Σx */
    "2:\xCE\xA3x\xC2\xB2",   /* Σx² */
    "3:\xCE\xA3y",            /* Σy */
    "4:\xCE\xA3y\xC2\xB2",   /* Σy² */
    "5:\xCE\xA3xy",           /* Σxy */
};

/* Tab 2: LR — linear regression */
static const char * const vars_lr_names[4] = {
    "1:a", "2:b", "3:r", "4:RegEQ"
};

/* Tab 3: DIM — matrix dimensions */
static const char * const vars_dim_names[6] = {
    "1:Arow", "2:Acol", "3:Brow", "4:Bcol", "5:Crow", "6:Ccol"
};

/* Tab 4: RNG — window range (10 items; item 10 digit-shortcut is TOKEN_0) */
static const char * const vars_rng_names[10] = {
    "1:Xmin", "2:Xmax", "3:Xscl",
    "4:Ymin", "5:Ymax", "6:Yscl",
    "7:Xres",
    "8:Tmin", "9:Tmax", "0:Tstep",
};

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

MenuState_t vars_menu_state = {0, 0, 0, MODE_NORMAL};

lv_obj_t *ui_vars_screen = NULL;

/* Item list labels */
static lv_obj_t *vars_item_labels[MENU_VISIBLE_ROWS];

/* Tab bar labels */
static lv_obj_t *vars_tab_labels[VARS_TAB_COUNT];

/* Scroll indicator overlays — amber, opaque bg (same pattern as MATH menu) */
static lv_obj_t *vars_scroll_ind[2];  /* [0]=top(↑) [1]=bottom(↓) */

/* Tab bar x positions — tuned for 5 tabs at 24px mono font (~14px/char) */
static const int16_t vars_tab_x[VARS_TAB_COUNT] = {4, 48, 84, 120, 188};

/*---------------------------------------------------------------------------
 * Internal helpers — y-statistic computations from raw data
 *
 * The StatResults_t struct holds only x-statistics.  These helpers derive
 * the equivalent y-statistics on-the-fly from stat_data.
 *---------------------------------------------------------------------------*/

static float vars_sum_y(void)
{
    float s = 0.0f;
    for (int i = 0; i < (int)stat_data.list_len; i++)
        s += stat_data.list_y[i];
    return s;
}

static float vars_sum_y2(void)
{
    float s = 0.0f;
    for (int i = 0; i < (int)stat_data.list_len; i++)
        s += stat_data.list_y[i] * stat_data.list_y[i];
    return s;
}

static float vars_sum_xy(void)
{
    float s = 0.0f;
    for (int i = 0; i < (int)stat_data.list_len; i++)
        s += stat_data.list_x[i] * stat_data.list_y[i];
    return s;
}

static float vars_mean_y(void)
{
    if (stat_data.list_len == 0) return 0.0f;
    return vars_sum_y() / (float)stat_data.list_len;
}

static float vars_sy(void)
{
    int n = (int)stat_data.list_len;
    if (n < 2) return 0.0f;
    float my = vars_mean_y();
    float v = (vars_sum_y2() - (float)n * my * my) / (float)(n - 1);
    return sqrtf(v < 0.0f ? 0.0f : v);
}

static float vars_sigma_y(void)
{
    int n = (int)stat_data.list_len;
    if (n < 1) return 0.0f;
    float my = vars_mean_y();
    float v = (vars_sum_y2() - (float)n * my * my) / (float)n;
    return sqrtf(v < 0.0f ? 0.0f : v);
}

/*---------------------------------------------------------------------------
 * Value formatting — returns the current value string for a given item
 *---------------------------------------------------------------------------*/

static void vars_format_value(uint8_t tab, uint8_t item, char *buf, size_t len)
{
    float v = 0.0f;

    switch (tab) {
    case 0: /* XY */
        if (!stat_results.valid) { snprintf(buf, len, "0"); return; }
        switch (item) {
        case 0: v = stat_results.n;       break;
        case 1: v = stat_results.mean_x;  break;
        case 2: v = stat_results.sx;      break;
        case 3: v = stat_results.sigma_x; break;
        case 4: v = vars_mean_y();         break;
        case 5: v = vars_sy();             break;
        case 6: v = vars_sigma_y();        break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 1: /* Σ */
        if (!stat_results.valid) { snprintf(buf, len, "0"); return; }
        switch (item) {
        case 0: v = stat_results.sum_x;  break;
        case 1: v = stat_results.sum_x2; break;
        case 2: v = vars_sum_y();         break;
        case 3: v = vars_sum_y2();        break;
        case 4: v = vars_sum_xy();        break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 2: /* LR */
        if (!stat_results.valid) { snprintf(buf, len, "0"); return; }
        if (item == 3) {
            /* RegEQ: build "aX+b" from regression coefficients */
            char abuf[16], bbuf[16];
            Calc_FormatResult(stat_results.reg_a, abuf, (uint8_t)sizeof(abuf));
            Calc_FormatResult(stat_results.reg_b, bbuf, (uint8_t)sizeof(bbuf));
            snprintf(buf, len, "%sX+%s", abuf, bbuf);
            return;
        }
        switch (item) {
        case 0: v = stat_results.reg_a; break;
        case 1: v = stat_results.reg_b; break;
        case 2: v = stat_results.reg_r; break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 3: /* DIM */
        switch (item) {
        case 0: v = (float)calc_matrices[0].rows; break;
        case 1: v = (float)calc_matrices[0].cols; break;
        case 2: v = (float)calc_matrices[1].rows; break;
        case 3: v = (float)calc_matrices[1].cols; break;
        case 4: v = (float)calc_matrices[2].rows; break;
        case 5: v = (float)calc_matrices[2].cols; break;
        default: snprintf(buf, len, "0"); return;
        }
        break;

    case 4: { /* RNG */
        const GraphState_t *gs = Graph_GetState();
        switch (item) {
        case 0: v = gs->x_min;  break;
        case 1: v = gs->x_max;  break;
        case 2: v = gs->x_scl;  break;
        case 3: v = gs->y_min;  break;
        case 4: v = gs->y_max;  break;
        case 5: v = gs->y_scl;  break;
        case 6: v = gs->x_res;  break;
        case 7: v = gs->t_min;  break;
        case 8: v = gs->t_max;  break;
        case 9: v = gs->t_step; break;
        default: snprintf(buf, len, "0"); return;
        }
        break;
    }

    default:
        snprintf(buf, len, "0");
        return;
    }

    Calc_FormatResult(v, buf, (uint8_t)(len < 255u ? len : 255u));
}

/*---------------------------------------------------------------------------
 * Insert action
 *---------------------------------------------------------------------------*/

/** Format the value for (tab, item) and insert it into the active editor. */
static void vars_do_insert(uint8_t tab, uint8_t item)
{
    char buf[64];
    vars_format_value(tab, item, buf, sizeof(buf));
    lvgl_lock();
    lv_obj_add_flag(ui_vars_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();
    menu_insert_text(buf, &vars_menu_state.return_mode);
}

/*---------------------------------------------------------------------------
 * Screen show/hide
 *---------------------------------------------------------------------------*/

void Vars_ShowScreen(void) { lv_obj_clear_flag(ui_vars_screen, LV_OBJ_FLAG_HIDDEN); }
void Vars_HideScreen(void) { lv_obj_add_flag(ui_vars_screen,   LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialization
 *---------------------------------------------------------------------------*/

void ui_init_vars_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_vars_screen = screen_create(scr);

    /* Tab bar */
    for (int i = 0; i < VARS_TAB_COUNT; i++) {
        vars_tab_labels[i] = lv_label_create(ui_vars_screen);
        lv_obj_set_pos(vars_tab_labels[i], vars_tab_x[i], 4);
        lv_obj_set_style_text_font(vars_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(vars_tab_labels[i],
            lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(vars_tab_labels[i], vars_tab_names[i]);
    }

    /* Item list */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        vars_item_labels[i] = lv_label_create(ui_vars_screen);
        lv_obj_set_pos(vars_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(vars_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(vars_item_labels[i],
            lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(vars_item_labels[i], "");
    }

    /* Scroll indicator overlays (amber, opaque bg — same as MATH menu) */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        vars_scroll_ind[i] = lv_label_create(ui_vars_screen);
        lv_obj_set_pos(vars_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(vars_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(vars_scroll_ind[i],
            lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(vars_scroll_ind[i],
            lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(vars_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(vars_scroll_ind[i], 0, 0);
        lv_label_set_text(vars_scroll_ind[i], "");
        lv_obj_add_flag(vars_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/*---------------------------------------------------------------------------
 * Display Update
 *---------------------------------------------------------------------------*/

void ui_update_vars_display(void)
{
    uint8_t tab    = vars_menu_state.tab;
    uint8_t cursor = vars_menu_state.cursor;
    uint8_t scroll = vars_menu_state.scroll;
    uint8_t total  = vars_tab_item_count[tab];

    /* Tab labels */
    for (int i = 0; i < VARS_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(vars_tab_labels[i],
            (i == (int)tab) ? lv_color_hex(COLOR_YELLOW)
                            : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    /* Hide scroll indicators; re-show below if needed */
    lv_obj_add_flag(vars_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(vars_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    /* Item rows */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)scroll + i;
        if (idx >= (int)total) {
            lv_label_set_text(vars_item_labels[i], "");
            lv_obj_set_style_text_color(vars_item_labels[i],
                lv_color_hex(COLOR_WHITE), 0);
            continue;
        }

        bool more_above = (scroll > 0)                            && (i == 0);
        bool more_below = ((int)scroll + MENU_VISIBLE_ROWS < (int)total)
                          && (i == MENU_VISIBLE_ROWS - 1);

        /* Pick display name for this item */
        const char *name = "";
        switch (tab) {
        case 0: name = vars_xy_names[idx];    break;
        case 1: name = vars_sigma_names[idx]; break;
        case 2: name = vars_lr_names[idx];    break;
        case 3: name = vars_dim_names[idx];   break;
        case 4: name = vars_rng_names[idx];   break;
        }

        lv_label_set_text(vars_item_labels[i], name);
        lv_obj_set_style_text_color(vars_item_labels[i],
            (i == (int)cursor) ? lv_color_hex(COLOR_YELLOW)
                               : lv_color_hex(COLOR_WHITE), 0);

        if (more_above) {
            lv_label_set_text(vars_scroll_ind[0], "\xE2\x86\x91");
            lv_obj_clear_flag(vars_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        }
        if (more_below) {
            lv_label_set_text(vars_scroll_ind[1], "\xE2\x86\x93");
            lv_obj_clear_flag(vars_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/*---------------------------------------------------------------------------
 * Token Handler
 *---------------------------------------------------------------------------*/

bool handle_vars_menu(Token_t t)
{
    MenuState_t *s = &vars_menu_state;
    uint8_t total = vars_tab_item_count[s->tab];

    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s->tab, &s->cursor, &s->scroll,
                 VARS_TAB_COUNT, true, ui_update_vars_display);
        return true;

    case TOKEN_RIGHT:
        tab_move(&s->tab, &s->cursor, &s->scroll,
                 VARS_TAB_COUNT, false, ui_update_vars_display);
        return true;

    case TOKEN_UP:
        MenuState_MoveUp(s, total, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_vars_display();
        lvgl_unlock();
        return true;

    case TOKEN_DOWN:
        MenuState_MoveDown(s, total, MENU_VISIBLE_ROWS);
        lvgl_lock();
        ui_update_vars_display();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER: {
        uint8_t actual = MenuState_AbsoluteIndex(s);
        if ((int)actual < (int)total)
            vars_do_insert(s->tab, actual);
        return true;
    }

    /* Digit shortcuts 1–9: item at that 1-based index; 0: item 10 (RNG Tstep) */
    case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4: case TOKEN_5:
    case TOKEN_6: case TOKEN_7: case TOKEN_8: case TOKEN_9: case TOKEN_0: {
        int idx = MenuState_DigitToIndex(t, total);
        if (idx >= 0) {
            /* Scroll so the chosen item is visible, place cursor on it */
            if (idx < MENU_VISIBLE_ROWS) {
                s->scroll = 0;
                s->cursor = (uint8_t)idx;
            } else {
                s->scroll = (uint8_t)(idx - MENU_VISIBLE_ROWS + 1);
                s->cursor = (uint8_t)(MENU_VISIBLE_ROWS - 1);
            }
            vars_do_insert(s->tab, (uint8_t)idx);
        }
        return true;
    }

    case TOKEN_CLEAR:
    case TOKEN_VARS:
        menu_close(TOKEN_VARS);
        Update_Calculator_Display();
        return true;

    default:
        return false;
    }
}
