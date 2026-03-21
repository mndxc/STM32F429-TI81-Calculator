/**
 * @file    calc_internal.h
 * @brief   Shared internal states and helpers for calculator UI modules.
 */

#ifndef APP_CALC_INTERNAL_H
#define APP_CALC_INTERNAL_H

#include "app_common.h"
#include "lvgl.h"

/* LVGL Fonts */
extern const lv_font_t jetbrains_mono_24;
extern const lv_font_t jetbrains_mono_20;

/* Global Calculator States */
extern CalcMode_t current_mode;
extern CalcMode_t return_mode;
extern bool insert_mode;
extern float ans;
extern bool ans_is_matrix;
extern bool angle_degrees;

/* Menu visible rows constant */
#define MENU_VISIBLE_ROWS 7

/* Main Display / Screens */
extern lv_obj_t *ui_matrix_screen;
extern lv_obj_t *ui_matrix_edit_screen;

/* Shared UI functions */
void cursor_place(lv_obj_t *cbox, lv_obj_t *cinner, lv_obj_t *row_label, uint32_t char_pos);
void cursor_box_create(lv_obj_t *parent, bool is_overlay, lv_obj_t **out_box, lv_obj_t **out_inner);
void hide_all_screens(void);
void nav_to(CalcMode_t target);
void Update_Calculator_Display(void);
CalcMode_t menu_close(Token_t menu_token);
void lvgl_lock(void);
void lvgl_unlock(void);
lv_obj_t *screen_create(lv_obj_t *parent);
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll, uint8_t tab_count, bool left, void (*update)(void));
void menu_insert_text(const char *ins, CalcMode_t *ret_mode);

#endif /* APP_CALC_INTERNAL_H */
