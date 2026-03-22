/**
 * @file ui_prgm.h
 * @brief Program (PRGM) Menu and Editor UI Module
 *
 * Handles PRGM menu UI (EXEC/EDIT/ERASE tabs, 37 slots), name-entry screen,
 * program line editor (CTL/I/O sub-menus), and editor ↔ FLASH store round-trip.
 * Execution is delegated to prgm_exec.c.
 *
 * Implementation status (as of 2026-03-22):
 *   - UI (menus, editor, CTL/I/O sub-menus): fully implemented.
 *   - Executor (prgm_exec.c): If/Then/Else/While/For/Goto/Lbl/Disp/Input/
 *     Prompt/ClrHome/Pause/Stop/Return/prgm/STO all implemented.
 *   - All commands implemented. Remaining: hardware validation (P10).
 *     Command reference: docs/PRGM_COMMANDS.md
 */
#ifndef UI_PRGM_H
#define UI_PRGM_H

#include "app_common.h"
#include "lvgl.h"
#include "prgm_exec.h"
#include <stdint.h>
#include <stdbool.h>

extern lv_obj_t *ui_prgm_editor_screen;
extern lv_obj_t *ui_prgm_new_screen;

/* Shared editor working buffer — also used by the execution engine */
extern char    prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];
extern uint8_t prgm_edit_num_lines;

/* Helpers used by both the editor and the execution engine */
void prgm_parse_from_store(uint8_t idx);
void prgm_slot_id_str(uint8_t slot, char *out);
bool prgm_slot_is_used(uint8_t slot);

void ui_init_prgm_screens(void);
/* prgm_reset_execution_state, prgm_run_start, prgm_run_loop declared in prgm_exec.h */
void hide_prgm_screens(void);
void ui_prgm_menu_show(const char *title, const char texts[][PRGM_MAX_LINE_LEN],
                        uint8_t count, uint8_t cursor, uint8_t scroll);
void ui_prgm_menu_hide(void);
void prgm_reset_state(CalcMode_t target_mode);

bool handle_prgm_menu(Token_t t);
bool handle_prgm_new_name(Token_t t);
bool handle_prgm_editor(Token_t t);
bool handle_prgm_ctl_menu(Token_t t);
bool handle_prgm_io_menu(Token_t t);
bool handle_prgm_running(Token_t t);

void prgm_menu_open(CalcMode_t return_to);
CalcMode_t prgm_menu_close(void);
void prgm_editor_cursor_update(void);
void prgm_new_cursor_update(void);

#endif
