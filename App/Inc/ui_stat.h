/**
 * @file    ui_stat.h
 * @brief   STAT menu, DATA editor and results screen UI — initialization,
 *          display-update, and token handler declarations.
 */

#ifndef APP_UI_STAT_H
#define APP_UI_STAT_H

#include "app_common.h"
#include "lvgl.h"

/*---------------------------------------------------------------------------
 * Menu navigation state
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    tab;          /* 0=CALC, 1=DRAW, 2=DATA */
    uint8_t    item_cursor;  /* Visible-row highlight within current tab */
    CalcMode_t return_mode;  /* Mode to restore on CLEAR */
} StatMenuState_t;

/*---------------------------------------------------------------------------
 * Externally visible state
 *---------------------------------------------------------------------------*/

extern StatMenuState_t stat_menu_state;

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

/** Initialises STAT menu state and shows the screen. Called under lvgl_lock(). */
void       Stat_MenuOpen(CalcMode_t return_to);

/** Resets STAT menu state and returns the saved return mode. Called from menu_close(). */
CalcMode_t Stat_MenuClose(void);

/** Token handler for MODE_STAT_MENU.  Returns true to consume token. */
bool handle_stat_menu(Token_t t, StatMenuState_t *s);

/** Token handler for MODE_STAT_EDIT.  Returns true to consume token. */
bool handle_stat_edit(Token_t t);

/** Token handler for MODE_STAT_RESULTS.  Returns true to consume token. */
bool handle_stat_results(Token_t t);

#endif /* APP_UI_STAT_H */
