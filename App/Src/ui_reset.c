/**
 * @file    ui_reset.c
 * @brief   RESET confirmation screen (2nd + +).
 *
 * Screen layout (guidebook p. 1-28):
 *   MEMORY
 *   Sts: XX pts        — stat data point count
 *   Pgm: XX stored     — occupied program slots
 *   1:No
 *   2:Reset
 *
 * The user presses 1 (or CLEAR) to cancel, or 2 to reset.
 * After reset the home screen shows "Mem cleared".
 */

#include "ui_reset.h"
#include "ui_shared.h"
#include "calculator_core.h"
#include "persist.h"
#include "prgm_exec.h"
#include "ui_stat.h"
#include "ui_palette.h"
#include "calc_history.h"
#include <stdlib.h>   /* srand */
#include <stdio.h>

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

static lv_obj_t *ui_reset_screen = NULL;

static CalcMode_t s_return_mode = MODE_NORMAL;
static lv_obj_t  *s_lbl_sts    = NULL;
static lv_obj_t  *s_lbl_pgm    = NULL;

/*---------------------------------------------------------------------------
 * Show / hide
 *---------------------------------------------------------------------------*/

void Reset_ShowScreen(void)  { lv_obj_clear_flag(ui_reset_screen, LV_OBJ_FLAG_HIDDEN); }
void Reset_HideScreen(void)  { lv_obj_add_flag(ui_reset_screen,   LV_OBJ_FLAG_HIDDEN); }
bool Reset_IsVisible(void)   { return ui_reset_screen != NULL && !lv_obj_has_flag(ui_reset_screen, LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialisation
 *---------------------------------------------------------------------------*/

void ui_init_reset_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_reset_screen = screen_create(scr);

    lv_obj_t *title = lv_label_create(ui_reset_screen);
    lv_obj_set_pos(title, 4, 4);
    lv_obj_set_style_text_font(title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(title, "MEMORY");

    s_lbl_sts = lv_label_create(ui_reset_screen);
    lv_obj_set_pos(s_lbl_sts, 4, 40);
    lv_obj_set_style_text_font(s_lbl_sts, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_lbl_sts, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(s_lbl_sts, "Sts:  0 pts");

    s_lbl_pgm = lv_label_create(ui_reset_screen);
    lv_obj_set_pos(s_lbl_pgm, 4, 70);
    lv_obj_set_style_text_font(s_lbl_pgm, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_lbl_pgm, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(s_lbl_pgm, "Pgm:  0 stored");

    lv_obj_t *item1 = lv_label_create(ui_reset_screen);
    lv_obj_set_pos(item1, 4, 110);
    lv_obj_set_style_text_font(item1, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(item1, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(item1, "1:No");

    lv_obj_t *item2 = lv_label_create(ui_reset_screen);
    lv_obj_set_pos(item2, 4, 140);
    lv_obj_set_style_text_font(item2, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(item2, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(item2, "2:Reset");
}

/*---------------------------------------------------------------------------
 * Open helper
 *---------------------------------------------------------------------------*/

static void reset_update_memory_display(void)
{
    char buf[32];

    uint8_t stat_pts = Stat_GetData()->list_len;
    snprintf(buf, sizeof(buf), "Sts: %3u pts", (unsigned)stat_pts);
    lv_label_set_text(s_lbl_sts, buf);

    uint8_t pgm_count = 0;
    for (uint8_t i = 0; i < PRGM_MAX_PROGRAMS; i++) {
        if (Prgm_IsSlotOccupied(i)) pgm_count++;
    }
    snprintf(buf, sizeof(buf), "Pgm: %3u stored", (unsigned)pgm_count);
    lv_label_set_text(s_lbl_pgm, buf);
}

void Reset_MenuOpen(CalcMode_t return_to)
{
    s_return_mode = return_to;
    Calc_SetMode(MODE_RESET_CONFIRM);
    lvgl_lock();
    reset_update_memory_display();
    Reset_ShowScreen();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

static void reset_cancel(void)
{
    /* TOKEN_RESET already hid all screens; the safest cancel is to return
     * to the home screen rather than attempting to restore arbitrarily complex
     * editor state (Y=, RANGE, graph canvas, etc.). */
    Calc_SetMode(MODE_NORMAL);
    Calc_SetReturnMode(MODE_NORMAL);
    lvgl_lock();
    Reset_HideScreen();
    lvgl_unlock();
    Update_Calculator_Display();
}

static void reset_confirm_and_apply(void)
{
    /* Reset calc state + persist to FLASH */
    Persist_Reset();

    /* Erase all programs */
    Prgm_Init();
    Prgm_Save();

    /* Reset rand seed to factory value 0 (guidebook p. 1-28) */
    srand(0);

    /* Return to home screen, showing "Mem cleared" */
    Calc_ResetInputState();
    prgm_reset_execution_state();
    CalcHistory_Commit("", "Mem cleared", false, 0, 0, 0);

    Calc_SetMode(MODE_NORMAL);
    Calc_SetReturnMode(MODE_NORMAL);
    lvgl_lock();
    Reset_HideScreen();
    hide_all_screens();
    ui_update_status_bar();
    lvgl_unlock();
    Update_Calculator_Display();
}

bool handle_reset_confirm(Token_t t)
{
    switch (t) {
    case TOKEN_1:
    case TOKEN_CLEAR:
        reset_cancel();
        return true;

    case TOKEN_2:
        reset_confirm_and_apply();
        return true;

    default:
        return true; /* swallow all other keys while confirmation is open */
    }
}
