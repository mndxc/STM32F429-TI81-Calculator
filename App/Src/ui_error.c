/**
 * @file    ui_error.c
 * @brief   TI-81 error overlay screen (guidebook pp. 1-26, B-4).
 *
 * Screen layout:
 *   ERROR nn type      — e.g. "ERROR 06 SYNTAX"
 *   1:Goto Error       — (omitted when can_goto=false, e.g. RANGE/ZOOM errors)
 *   2:Quit
 *
 * Pressing 1 restores the faulting expression in the editor with the cursor
 * at the byte offset of the fault, then returns to MODE_NORMAL.
 * Pressing 2 or CLEAR dismisses and returns to a clean home screen.
 */

#include "ui_error.h"
#include "ui_shared.h"
#include "ui_palette.h"
#include "calculator_core.h"
#include "calc_history.h"
#include "expr_util.h"
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * Module state
 *---------------------------------------------------------------------------*/

lv_obj_t *ui_error_screen = NULL;

static lv_obj_t  *s_lbl_title    = NULL;
static lv_obj_t  *s_lbl_goto     = NULL;

static char      s_saved_expr[MAX_EXPR_LEN];
static uint16_t  s_saved_offset;
static bool      s_can_goto;

/*---------------------------------------------------------------------------
 * Show / hide
 *---------------------------------------------------------------------------*/

void Error_ShowScreen(void) { lv_obj_clear_flag(ui_error_screen, LV_OBJ_FLAG_HIDDEN); }
void Error_HideScreen(void) { lv_obj_add_flag(ui_error_screen,   LV_OBJ_FLAG_HIDDEN); }

/*---------------------------------------------------------------------------
 * UI Initialisation
 *---------------------------------------------------------------------------*/

void ui_init_error_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_error_screen = screen_create(scr);

    s_lbl_title = lv_label_create(ui_error_screen);
    lv_obj_set_pos(s_lbl_title, 4, 4);
    lv_obj_set_style_text_font(s_lbl_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(s_lbl_title, "ERROR");

    s_lbl_goto = lv_label_create(ui_error_screen);
    lv_obj_set_pos(s_lbl_goto, 4, 44);
    lv_obj_set_style_text_font(s_lbl_goto, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_lbl_goto, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(s_lbl_goto, "1:Goto Error");

    lv_obj_t *item_quit = lv_label_create(ui_error_screen);
    lv_obj_set_pos(item_quit, 4, 74);
    lv_obj_set_style_text_font(item_quit, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(item_quit, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(item_quit, "2:Quit");
}

/*---------------------------------------------------------------------------
 * Open helper
 *---------------------------------------------------------------------------*/

void Error_Open(CalcError_t err, const char *expr, uint16_t offset, bool can_goto)
{
    s_saved_offset = offset;
    s_can_goto     = can_goto;
    strncpy(s_saved_expr, expr ? expr : "", MAX_EXPR_LEN - 1);
    s_saved_expr[MAX_EXPR_LEN - 1] = '\0';

    Calc_SetMode(MODE_ERROR_SCREEN);
    lvgl_lock();
    lv_label_set_text(s_lbl_title, Calc_GetErrorString(err));
    if (can_goto)
        lv_obj_clear_flag(s_lbl_goto, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_lbl_goto, LV_OBJ_FLAG_HIDDEN);
    Error_ShowScreen();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Token handler
 *---------------------------------------------------------------------------*/

static void error_goto(void)
{
    ExprBuffer_t *e = Calc_GetExpr();
    strncpy(e->buf, s_saved_expr, MAX_EXPR_LEN - 1);
    e->buf[MAX_EXPR_LEN - 1] = '\0';
    e->len    = (uint8_t)strlen(e->buf);
    e->cursor = (s_saved_offset <= e->len) ? (uint8_t)s_saved_offset : e->len;
    CalcHistory_ResetRecallOffset();

    Calc_SetMode(MODE_NORMAL);
    Calc_SetReturnMode(MODE_NORMAL);
    lvgl_lock();
    Error_HideScreen();
    lvgl_unlock();
    Update_Calculator_Display();
}

static void error_quit(void)
{
    Calc_SetMode(MODE_NORMAL);
    Calc_SetReturnMode(MODE_NORMAL);
    lvgl_lock();
    Error_HideScreen();
    lvgl_unlock();
    Update_Calculator_Display();
}

bool handle_error_screen(Token_t t)
{
    switch (t) {
    case TOKEN_1:
        if (s_can_goto) { error_goto(); return true; }
        return true; /* Swallow — no Goto Error available */

    case TOKEN_2:
    case TOKEN_CLEAR:
        error_quit();
        return true;

    default:
        return true; /* Swallow all other keys while error screen is shown */
    }
}
