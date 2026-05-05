/**
 * @file    ui_menu_screen.h
 * @brief   Generic scrolling menu screen module for PRGM sub-menus.
 *
 * MenuScreen_t owns a MenuState_t (cursor + scroll), a set of LVGL label
 * objects (optional tab bar, item rows, scroll indicators), and a descriptor
 * that supplies the item labels and navigation callbacks.
 *
 * Each PRGM sub-menu file keeps one static MenuScreen_t and a
 * MenuScreenDesc_t; all navigation and display logic lives in this module.
 *
 * LVGL threading rules:
 *   MenuScreen_Init()         — must be called under lvgl_lock().
 *   MenuScreen_ResetAndShow() — must be called under lvgl_lock().
 *   MenuScreen_UpdateDisplay()— must be called under lvgl_lock().
 *   MenuScreen_HandleToken()  — acquires/releases lvgl_lock() internally
 *                               for up/down; callbacks manage their own locking.
 */

#ifndef UI_MENU_SCREEN_H
#define UI_MENU_SCREEN_H

#include "ui_shared.h"
#include "menu_state.h"

/* Maximum tab labels rendered in the decorative tab bar. */
#define MENU_SCREEN_MAX_TABS   3

/* Buffer size for dynamic item labels produced by get_label(). */
#define MENU_SCREEN_LABEL_MAX  32

/* Forward declaration for on_extra callback signature. */
typedef struct MenuScreen MenuScreen_t;

/**
 * @brief Descriptor that parameterises a MenuScreen_t.
 *
 * All pointer members must remain valid for the lifetime of the MenuScreen_t.
 * Set left_mode / right_mode to 0 (MODE_NORMAL) to disable that direction.
 */
typedef struct {
    /* Tab bar — set tab_count = 0 to omit. */
    uint8_t             tab_count;      /**< Number of sibling tabs to display. */
    const char * const *tab_names;      /**< Array of tab_count label strings.  */
    const int          *tab_x;          /**< Array of tab_count x positions.    */
    uint8_t             active_tab;     /**< Index of the highlighted tab.       */

    /* Items. */
    uint8_t             item_count;     /**< Total items in the list.            */
    /** Static display labels.  Set NULL and provide get_label() instead. */
    const char * const *display_labels;
    /** Dynamic label generator; NULL when display_labels is set.
        buf is MENU_SCREEN_LABEL_MAX bytes. */
    void (*get_label)(int idx, char *buf, size_t bufsz);

    /* Navigation targets for LEFT / RIGHT (0 = disabled). */
    CalcMode_t          left_mode;
    CalcMode_t          right_mode;

    /** Called on ENTER or a valid digit shortcut with the 0-based item index.
        Typically inserts text and returns to the program editor. */
    void (*on_select)(int idx, lv_obj_t *screen);

    /** Called on CLEAR.  Typically prgm_submenu_return_to_editor(screen). */
    void (*on_cancel)(lv_obj_t *screen);

    /** Called on LEFT / RIGHT with the target mode.
        Typically prgm_submenu_tab_switch(screen, mode). */
    void (*on_tab_switch)(lv_obj_t *screen, CalcMode_t mode);

    /** Extra token handling — NULL if not needed.
        Called for any token not handled by the generic navigation logic.
        Return true to consume the token. */
    bool (*on_extra)(Token_t t, MenuScreen_t *ms);
} MenuScreenDesc_t;

/** Generic scrolling menu screen instance. */
struct MenuScreen {
    const MenuScreenDesc_t *desc;
    MenuState_t             nav;    /**< cursor + scroll (tab field unused). */
    lv_obj_t               *screen;
    lv_obj_t               *item_labels[MENU_VISIBLE_ROWS];
    lv_obj_t               *scroll_ind[2];
    lv_obj_t               *tab_labels[MENU_SCREEN_MAX_TABS];
};

/**
 * @brief Initialise a MenuScreen_t, creating all LVGL objects.
 *        Must be called under lvgl_lock().
 */
void MenuScreen_Init(MenuScreen_t *ms, const MenuScreenDesc_t *desc,
                     lv_obj_t *parent);

/**
 * @brief Reset cursor / scroll to 0 and refresh the display.
 *        Must be called under lvgl_lock().
 *        Does NOT show or hide the screen — the caller manages visibility.
 */
void MenuScreen_ResetAndShow(MenuScreen_t *ms);

/**
 * @brief Redraw item labels and scroll indicators from the current nav state.
 *        Must be called under lvgl_lock().
 */
void MenuScreen_UpdateDisplay(MenuScreen_t *ms);

/**
 * @brief Dispatch a token to the menu's navigation and callback logic.
 * @return true (always consumes the token).
 */
bool MenuScreen_HandleToken(MenuScreen_t *ms, Token_t t);

#endif /* UI_MENU_SCREEN_H */
