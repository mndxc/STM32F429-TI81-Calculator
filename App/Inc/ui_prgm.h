/**
 * @file ui_prgm.h
 * @brief Program (PRGM) slot browser, new-name entry, and output adapter.
 *
 * Handles PRGM menu UI (EXEC/EDIT/ERASE tabs, 37 slots), name-entry screen,
 * and the PrgmOutput_t hardware adapter.  The line editor is in prgm_editor.h.
 *
 * Implementation status (as of 2026-05-07):
 *   - UI (menus, editor, CTL/I/O sub-menus): fully implemented.
 *   - Executor (prgm_exec.c): fully implemented.
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

bool Prgm_IsEditorScreenVisible(void);
bool Prgm_IsNewScreenVisible(void);

/* Accessors for the editor working buffer — used by the execution engine.
 * Declarations live here for backward compatibility with prgm_exec.c which
 * includes ui_prgm.h; implementations are in prgm_editor.c. */
const char *Prgm_GetLine(uint8_t ln);
uint8_t     Prgm_GetNumLines(void);
void        prgm_parse_from_store(uint8_t idx);

/* Helpers used by the slot browser, sub-menus, and execution engine */
void prgm_slot_id_str(uint8_t slot, char *out);
bool prgm_slot_is_used(uint8_t slot);

/* Thin wrapper — delegates to PrgmEditor_FlattenToStore().
 * Kept for sub-menu files that include ui_prgm.h but have not yet been
 * migrated to include prgm_editor.h directly. */
void prgm_flatten_to_store(void);

void ui_init_prgm_screens(void);
void hide_prgm_screens(void);
void ui_prgm_menu_show(const char *title, const char texts[][PRGM_MAX_LINE_LEN],
                        uint8_t count, uint8_t cursor, uint8_t scroll);
void ui_prgm_menu_hide(void);
void prgm_reset_state(CalcMode_t target_mode);

bool handle_prgm_menu(Token_t t);
bool handle_prgm_new_name(Token_t t);
bool handle_prgm_editor(Token_t t);
bool handle_prgm_running(Token_t t);

/* Sub-menu shared helpers — called by ui_prgm_ctl.c, ui_prgm_io.c,
 * ui_prgm_exec.c, ui_prgm_mode.c.  Defined in ui_prgm.c. */
void prgm_submenu_return_to_editor(lv_obj_t *hide_screen);
void prgm_submenu_tab_switch(lv_obj_t *hide_screen, CalcMode_t to_mode);

void prgm_menu_open(CalcMode_t return_to);
CalcMode_t prgm_menu_close(void);
void prgm_new_cursor_update(void);
void ui_update_prgm_display(void);
void ui_update_prgm_new_display(void);

#endif
