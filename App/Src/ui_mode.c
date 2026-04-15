/**
 * @file    ui_mode.c
 * @brief   MODE settings screen — state, LVGL init, display, and token handler.
 *
 * Extracted from calculator_core.c. Part of the UI super-module; may include
 * calc_internal.h freely.
 *
 * Rows and their committed[] index meanings:
 *   Row 0: Normal / Sci / Eng       (output format)
 *   Row 1: Float / 0–9              (decimal places)
 *   Row 2: Radian / Degree          (angle mode)
 *   Row 3: Func / Param             (graph type)
 *   Row 4: Connected / Dot          (graph style)
 *   Row 5: Sequential / Simul       (graph draw order)
 *   Row 6: Grid off / Grid on       (grid visibility)
 *   Row 7: Polar / Seq              (coord system)
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "calc_engine.h"
#  include "prgm_exec.h"
#  include "ui_mode.h"
#  include "calculator_core_test_stubs.h"
#  include <string.h>
#else
#  include "calc_internal.h"
#  include "graph.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Shared state (definitions)
 *---------------------------------------------------------------------------*/

/** Row 2 starts at 1 = Degree to match angle_degrees=true default. */
ModeScreenState_t s_mode = {
    .row_selected = 0,
    .cursor    = {0, 0, 1, 0, 0, 0, 0, 0},
    .committed = {0, 0, 1, 0, 0, 0, 0, 0},
};

/* ui_mode_screen: lv_obj_t* is defined in both builds — lv_obj_t is provided
 * by lvgl.h (firmware) or calculator_core_test_stubs.h (host test). */
lv_obj_t *ui_mode_screen = NULL;

#ifndef HOST_TEST

/*---------------------------------------------------------------------------
 * Private data
 *---------------------------------------------------------------------------*/

/* MODE screen option strings */
static const char * const mode_options[MODE_ROW_COUNT][MODE_MAX_COLS] = {
    {"Normal", "Sci", "Eng", NULL},
    {"Float", "0","1","2","3","4","5","6","7","8","9"},
    {"Radian", "Degree", NULL},
    {"Func",   "Param",  NULL},
    {"Connected", "Dot", NULL},
    {"Sequential","Simul",NULL},
    {"Grid off","Grid on",NULL},
    {"Polar",  "Seq",    NULL},
};

static const uint8_t mode_option_count[MODE_ROW_COUNT] = {3, 11, 2, 2, 2, 2, 2, 2};

/* x-pixel start position for each option within its row */
static const int16_t mode_option_x[MODE_ROW_COUNT][MODE_MAX_COLS] = {
    {4, 112, 215, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 80, 104, 128, 152, 176, 200, 224, 248, 272, 296},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static lv_obj_t *mode_option_labels[MODE_ROW_COUNT][MODE_MAX_COLS];

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/**
 * @brief Create all LVGL objects for the MODE settings screen.
 *        Called once from StartCalcCoreTask while the LVGL mutex is held.
 */
void ui_mode_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_mode_screen = screen_create(scr);

    memset(mode_option_labels, 0, sizeof(mode_option_labels));

    for (int r = 0; r < MODE_ROW_COUNT; r++) {
        int n = mode_option_count[r];
        for (int c = 0; c < n; c++) {
            lv_obj_t *lbl = lv_label_create(ui_mode_screen);
            lv_obj_set_pos(lbl, mode_option_x[r][c], r * 30);
            lv_obj_set_style_text_font(lbl, &jetbrains_mono_24, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_GREY_INACTIVE), 0);
            lv_label_set_text(lbl, mode_options[r][c]);
            mode_option_labels[r][c] = lbl;
        }
    }
}

/**
 * @brief Open the MODE screen: sync cursor to committed values, set mode,
 *        hide all overlays, and make the MODE screen visible.
 *        Replaces the inline TOKEN_MODE handler from calculator_core.c.
 */
void ui_mode_open(void)
{
    memcpy(s_mode.cursor, s_mode.committed, sizeof(s_mode.cursor));
    s_mode.row_selected = 0;
    current_mode = MODE_MODE_SCREEN;
    lvgl_lock();
    hide_all_screens();
    lv_obj_clear_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_mode_display();
    lvgl_unlock();
}

/**
 * @brief Redraws all MODE screen option labels with correct highlight colours.
 *        Must be called under lvgl_lock().
 */
void ui_update_mode_display(void)
{
    for (int r = 0; r < MODE_ROW_COUNT; r++) {
        int n = mode_option_count[r];
        for (int c = 0; c < n; c++) {
            lv_obj_t *lbl = mode_option_labels[r][c];
            if (lbl == NULL) continue;
            lv_color_t col;
            if (r == s_mode.row_selected && c == (int)s_mode.cursor[r])
                col = lv_color_hex(COLOR_YELLOW);
            else if (c == (int)s_mode.committed[r])
                col = lv_color_hex(COLOR_WHITE);
            else
                col = lv_color_hex(COLOR_GREY_INACTIVE);
            lv_obj_set_style_text_color(lbl, col, 0);
        }
    }
}

/**
 * @brief Token handler for MODE_MODE_SCREEN.
 * @return true if the token was consumed; false to fall through to normal mode.
 */
bool handle_mode_screen(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_mode.row_selected > 0) s_mode.row_selected--;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s_mode.row_selected < MODE_ROW_COUNT - 1) s_mode.row_selected++;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        if (s_mode.cursor[s_mode.row_selected] > 0)
            s_mode.cursor[s_mode.row_selected]--;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (s_mode.cursor[s_mode.row_selected] < mode_option_count[s_mode.row_selected] - 1)
            s_mode.cursor[s_mode.row_selected]++;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        s_mode.committed[s_mode.row_selected] = s_mode.cursor[s_mode.row_selected];
        if (s_mode.row_selected == 1)
            Calc_SetDecimalMode(s_mode.committed[1]);
        if (s_mode.row_selected == 2)
            angle_degrees = (s_mode.committed[2] == 1);
        if (s_mode.row_selected == 4) {
            graph_state.param_mode = (s_mode.committed[4] == 1);
            Graph_InvalidateCache();
        }
        if (s_mode.row_selected == 6)
            graph_state.grid_on = (s_mode.committed[6] == 1);
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
    case TOKEN_MODE:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        return true;
    default:
        /* Any other key exits MODE screen and is processed normally */
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}

#endif /* !HOST_TEST */
