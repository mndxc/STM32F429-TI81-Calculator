/**
 * @file    ui_math_menu.h
 * @brief   MATH (MATH/NUM/HYP/PRB) and TEST menu UI — initialization,
 *          display-update, and token handler declarations.
 *
 * Both menus are part of the calculator UI super-module and share state
 * via calc_internal.h (which includes this header).
 */

#ifndef APP_UI_MATH_MENU_H
#define APP_UI_MATH_MENU_H

#include "app_common.h"
#include "lvgl.h"

/*---------------------------------------------------------------------------
 * Screen show/hide API (caller holds lvgl_lock)
 *---------------------------------------------------------------------------*/
void Math_ShowScreen(void);
void Math_HideScreen(void);
void Test_ShowScreen(void);
void Test_HideScreen(void);

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/** Creates the MATH/NUM/HYP/PRB menu screen (hidden at start). */
void ui_init_math_screen(void);

/** Creates the TEST menu screen (hidden at start). */
void ui_init_test_screen(void);

/** Redraws the MATH menu display from current state. Must be called under lvgl_lock(). */
void ui_update_math_display(void);

/** Redraws the TEST menu display from current state. Must be called under lvgl_lock(). */
void ui_update_test_display(void);

/** Token handler for MODE_MATH_MENU. Returns true to consume token. */
bool handle_math_menu(Token_t t);

/** Token handler for MODE_TEST_MENU. Returns true to consume token. */
bool handle_test_menu(Token_t t);

/**
 * @brief Initialises MATH menu state and shows the screen.
 * Called from menu_open() while already holding lvgl_lock().
 */
void math_menu_open(CalcMode_t return_to);

/**
 * @brief Initialises TEST menu state and shows the screen.
 * Called from menu_open() while already holding lvgl_lock().
 */
void test_menu_open(CalcMode_t return_to);

/**
 * @brief Resets MATH menu state and returns the saved return mode.
 * Called from menu_close(). Does not touch LVGL visibility.
 */
CalcMode_t math_menu_close(void);

/**
 * @brief Resets TEST menu state and returns the saved return mode.
 * Called from menu_close(). Does not touch LVGL visibility.
 */
CalcMode_t test_menu_close(void);

#endif /* APP_UI_MATH_MENU_H */
