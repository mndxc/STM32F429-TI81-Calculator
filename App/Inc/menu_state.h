/**
 * @file    menu_state.h
 * @brief   Shared navigation state and helpers for single-list and tabbed menus.
 *
 * MenuState_t is a drop-in replacement for the ad-hoc per-module state
 * structs (VarsMenuState_t, StatMenuState_t, etc.). Modules should be
 * migrated one at a time; see INTERFACE_REFACTOR_PLAN.md Item 3.
 *
 * Proof-of-concept: ui_vars.c was the first module migrated (Item 3).
 * Remaining: ui_stat.c, ui_matrix.c, ui_yvars.c, ui_draw.c, ui_math_menu.c.
 *
 * All helpers are pure (no LVGL / HAL dependencies) and host-testable.
 */

#ifndef MENU_STATE_H
#define MENU_STATE_H

#include "app_common.h"

/*---------------------------------------------------------------------------
 * Shared navigation state
 *---------------------------------------------------------------------------*/

/**
 * @brief Common navigation state shared by all single-list and tabbed menus.
 */
typedef struct {
    uint8_t    tab;          /**< Active tab index (0 if single-list) */
    uint8_t    cursor;       /**< Highlighted row within the visible window */
    uint8_t    scroll;       /**< Top-of-window item index */
    CalcMode_t return_mode;  /**< Mode to restore on CLEAR */
} MenuState_t;

/*---------------------------------------------------------------------------
 * Navigation helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief Move the menu cursor up by one row, scrolling if needed.
 * @param s        Menu state to update.
 * @param total    Total item count in the active tab/list.
 * @param visible  Number of visible rows (typically MENU_VISIBLE_ROWS).
 */
void MenuState_MoveUp(MenuState_t *s, uint8_t total, uint8_t visible);

/**
 * @brief Move the menu cursor down by one row, scrolling if needed.
 * @param s        Menu state to update.
 * @param total    Total item count in the active tab/list.
 * @param visible  Number of visible rows (typically MENU_VISIBLE_ROWS).
 */
void MenuState_MoveDown(MenuState_t *s, uint8_t total, uint8_t visible);

/**
 * @brief Move to the previous tab (stops at tab 0). Resets cursor and scroll.
 * @param s          Menu state to update.
 * @param tab_count  Total number of tabs.
 */
void MenuState_PrevTab(MenuState_t *s, uint8_t tab_count);

/**
 * @brief Move to the next tab (stops at tab_count-1). Resets cursor and scroll.
 * @param s          Menu state to update.
 * @param tab_count  Total number of tabs.
 */
void MenuState_NextTab(MenuState_t *s, uint8_t tab_count);

/**
 * @brief Map a digit token to a 0-based item index.
 *
 * TOKEN_1..TOKEN_9 → 0..8; TOKEN_0 → 9 (matches TI-81 digit-shortcut convention).
 *
 * @param t      Incoming token.
 * @param total  Number of items in the current tab/list.
 * @return       0-based index, or -1 if @p t is not a digit token or the
 *               resulting index is out of range.
 */
int MenuState_DigitToIndex(Token_t t, uint8_t total);

/**
 * @brief Returns the absolute item index (scroll + cursor).
 */
static inline uint8_t MenuState_AbsoluteIndex(const MenuState_t *s)
{
    return (uint8_t)(s->scroll + s->cursor);
}

#endif /* MENU_STATE_H */
