/**
 * @file    ui_shared.h
 * @brief   Lightweight shared UI constants and utility declarations.
 *
 * Shared UI constants, utility declarations, and cursor-rendering helpers for
 * all calculator UI modules.  Replaces the former calc_internal.h re-export
 * umbrella — each module now includes this header directly together with
 * whatever targeted headers it needs (calculator_core.h, calc_history.h, etc.).
 */

#ifndef APP_UI_SHARED_H
#define APP_UI_SHARED_H

#include "app_common.h"  /* CalcMode_t, Token_t, uint8_t … */
#include "lvgl.h"

/* Fonts */
extern const lv_font_t jetbrains_mono_24;
extern const lv_font_t jetbrains_mono_20;

/* Layout constants */
#define MENU_VISIBLE_ROWS   7
#define DISPLAY_W           320
#define DISPLAY_H           240
#define DISP_ROW_COUNT      8
#define DISP_ROW_H          30
#define CURSOR_BLINK_MS     530

/* LVGL mutex wrappers */
void lvgl_lock(void);
void lvgl_unlock(void);

/* Screen creation */
lv_obj_t *screen_create(lv_obj_t *parent);

/* Navigation */
void hide_all_screens(void);
void nav_to(CalcMode_t target);

/* Display refresh */
void Update_Calculator_Display(void);
void ui_update_status_bar(void);

/* Menu lifecycle */
CalcMode_t menu_close(Token_t menu_token);
void       menu_open(Token_t menu_token, CalcMode_t return_to);
void       menu_insert_text(const char *ins, CalcMode_t *ret_mode);

/* Tab/scroll navigation */
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll,
              uint8_t tab_count, bool left, void (*update)(void));

/* Cursor rendering — shared across all expression-editor screens */
void cursor_box_create(lv_obj_t *parent, bool start_hidden,
                       lv_obj_t **out_box, lv_obj_t **out_inner);
void cursor_render(lv_obj_t *box, lv_obj_t *inner, lv_obj_t *parent_label,
                   uint32_t glyph_pos, bool visible, CalcMode_t mode, bool insert);

#endif /* APP_UI_SHARED_H */
