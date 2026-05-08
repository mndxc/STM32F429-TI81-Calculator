/**
 * @file    prgm_editor.h
 * @brief   Program line editor — LVGL screen, state, and token handler.
 *
 * Extracted from ui_prgm.c.  Owns all LVGL objects for the editor screen,
 * the working line buffer, and the editor token handler.
 * ui_prgm.c retains only the slot browser (EXEC/EDIT/ERASE tabs).
 *
 * Cross-module coordination (navigate-up-to-new-name, return-to-menu) uses
 * registered callbacks so prgm_editor.c does not need to include ui_prgm.h,
 * avoiding a circular dependency.
 */
#ifndef PRGM_EDITOR_H
#define PRGM_EDITOR_H

#include "app_common.h"
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

/* Lifecycle — called from ui_init_prgm_screens */
void PrgmEditor_InitScreen(lv_obj_t *parent_scr);

/**
 * Open the editor for @p slot.  Caller must hide the previous screen
 * (prgm_menu or prgm_new) before calling this function.
 * If @p from_new is true, UP on line 0 col 0 invokes the nav-up callback
 * registered via PrgmEditor_SetNavUpCallback().
 */
void PrgmEditor_Open(uint8_t slot, bool from_new);

/* Token handler — registered in the CalcMode_t dispatch table */
bool PrgmEditor_HandleToken(Token_t t);

/* Buffer mutations — called by sub-menu on_select callbacks */
void PrgmEditor_InsertStr(const char *s);
void PrgmEditor_FlattenToStore(void);

/* Display refresh — called by prgm_submenu_return_to_editor */
void PrgmEditor_RefreshDisplay(void);
void PrgmEditor_CursorUpdate(void);

/**
 * Insert @p s into the current editor line, flatten to store, refresh the
 * editor display, and restore MODE_PRGM_EDITOR.  Called by math_menu_insert
 * / test_menu_insert when return_mode == MODE_PRGM_EDITOR.
 */
void PrgmEditor_MenuInsert(const char *s);

/* Accessors */
lv_obj_t *PrgmEditor_GetScreen(void);
uint8_t   PrgmEditor_GetSlot(void);

/**
 * Register a callback invoked when the user navigates UP from the first
 * editor line when the editor was opened via from_new=true.
 * Set to NULL to clear.  Used by ui_prgm.c to show the new-name screen.
 */
void PrgmEditor_SetNavUpCallback(void (*cb)(void));

/**
 * Register a callback invoked when CLEAR is pressed on an empty single-line
 * program (return to PRGM menu browser).  Set once in ui_init_prgm_screens.
 */
void PrgmEditor_SetReturnToMenuCallback(void (*cb)(void));

#endif /* PRGM_EDITOR_H */
