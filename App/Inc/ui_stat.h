/**
 * @file    ui_stat.h
 * @brief   STAT menu, DATA editor and results screen UI — initialization,
 *          display-update, and token handler declarations.
 */

#ifndef APP_UI_STAT_H
#define APP_UI_STAT_H

#include "app_common.h"
#include "lvgl.h"
#include "menu_state.h"

/* StatMenuState_t replaced by shared MenuState_t. */

/*---------------------------------------------------------------------------
 * Stat data / results accessors
 *---------------------------------------------------------------------------*/

/** Returns a read-only pointer to the current stat data list. */
const StatData_t    *Stat_GetData(void);

/** Returns a read-only pointer to the last computed stat results. */
const StatResults_t *Stat_GetResults(void);

/** Overwrites the stat data list from *src (used by persist restore). */
void Stat_SetData(const StatData_t *src);

/*---------------------------------------------------------------------------
 * Screen show/hide API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Stat_ShowMenuScreen(void);
void Stat_HideMenuScreen(void);
void Stat_ShowEditScreen(void);
void Stat_HideEditScreen(void);
void Stat_ShowResultsScreen(void);
void Stat_HideResultsScreen(void);

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates the STAT tab-menu screen (hidden at start). */
void ui_init_stat_screen(void);

/** Creates the DATA list-editor screen (hidden at start). */
void ui_init_stat_edit_screen(void);

/** Creates the results readout screen (hidden at start). */
void ui_init_stat_results_screen(void);

/** Redraws the STAT tab-menu display from current state. */
void ui_update_stat_display(void);

/** Redraws the DATA editor display from current state. */
void ui_update_stat_edit_display(void);

/** Redraws the results display from stat_results. */
void ui_update_stat_results_display(void);

/** Resets DATA editor state and transitions to MODE_STAT_EDIT. */
void Stat_EditOpen(void);

/** Initialises STAT menu state and shows the screen. Called under lvgl_lock(). */
void       Stat_MenuOpen(CalcMode_t return_to);

/** Resets STAT menu state and returns the saved return mode. Called from menu_close(). */
CalcMode_t Stat_MenuClose(void);

/** Token handler for MODE_STAT_MENU.
 *  Consumes: TOKEN_UP/DOWN (tab navigation), TOKEN_ENTER (open selected item),
 *  TOKEN_CLEAR / TOKEN_2ND_QUIT (close menu, return to return_mode).
 *  Returns true to consume token, false to let the caller handle it. */
bool handle_stat_menu(Token_t t);

/** Token handler for MODE_STAT_EDIT (DATA list editor).
 *  Consumes: TOKEN_UP/DOWN (row navigation), TOKEN_LEFT/RIGHT (x/y column),
 *  digit/decimal keys (cell entry), TOKEN_ENTER (commit cell), TOKEN_DEL (backspace),
 *  TOKEN_CLEAR / TOKEN_2ND_QUIT (exit editor).
 *  Returns true to consume token. */
bool handle_stat_edit(Token_t t);

/** Token handler for MODE_STAT_RESULTS.
 *  Consumes: TOKEN_CLEAR / TOKEN_2ND_QUIT (return to STAT menu).
 *  All other tokens return false (not consumed).
 *  Returns true to consume token. */
bool handle_stat_results(Token_t t);

#endif /* APP_UI_STAT_H */
