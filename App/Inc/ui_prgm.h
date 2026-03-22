/**
 * @file ui_prgm.h
 * @brief Program (PRGM) Menu and Editor UI Module
 *
 * *************************************************************************
 * WARNING: PARTIALLY COMPLETED MODULE
 * *************************************************************************
 * This module was extracted from `calculator_core.c` to adhere to the UI
 * Extensibility Pattern. Currently, ONLY the UI rendering and tab navigation
 * (EXEC, EDIT, NEW, CTL, I/O) are fully implemented.
 *
 * SIGNIFICANT BACKEND WORK REMAINS:
 *   - The PRGM execution engine must be properly bridged to calc_engine.c.
 *   - Tokenization logic inside the editor is incomplete.
 *   - Program storage (prgm_flatten_to_store) and memory management must be finalized.
 *   - I/O handling and control flow loops (If, While, For) are non-functional.
 * *************************************************************************
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
