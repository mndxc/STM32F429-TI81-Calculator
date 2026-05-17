/**
 * @file    ui_menu_screen.h
 * @brief   Generic scrolling menu screen module.
 *
 * MenuScreen_t owns a MenuState_t (cursor + scroll), a set of LVGL label
 * objects (optional tab bar, item rows, scroll indicators), and a descriptor
 * that supplies item labels and navigation callbacks.
 *
 * Supports two navigation models:
 *  - Sibling-menu navigation: LEFT/RIGHT call on_tab_switch to switch to a
 *    different CalcMode (e.g. PRGM CTL → I/O → EXEC sub-menus).
 *  - Within-menu tab navigation: LEFT/RIGHT call MenuScreen_SetTab to flip
 *    between tabs on the same screen (e.g. VARS XY/Σ/LR/DIM/RNG).
 *
 * LVGL threading rules:
 *   MenuScreen_Init()         — must be called under lvgl_lock().
 *   MenuScreen_ResetAndShow() — must be called under lvgl_lock().
 *   MenuScreen_UpdateDisplay()— must be called under lvgl_lock().
 *   MenuScreen_SetTab()       — must be called under lvgl_lock().
 *   MenuScreen_HandleToken()  — acquires/releases lvgl_lock() internally
 *                               for UP/DOWN and within-menu tab switches;
 *                               callbacks manage their own locking.
 */

#ifndef UI_MENU_SCREEN_H
#define UI_MENU_SCREEN_H

#ifdef HOST_TEST
#  include "app_common.h"
#  include "menu_state.h"
#  include <stdint.h>
#  include <stdbool.h>
#  include <stddef.h>
typedef struct { int dummy; } lv_obj_t;
#  ifndef MENU_VISIBLE_ROWS
#    define MENU_VISIBLE_ROWS  7
#  endif
#else
#  include "ui_shared.h"
#  include "menu_state.h"
#endif

/* Maximum tabs rendered in the visual tab bar. */
#define MENU_SCREEN_MAX_TABS   5

/* Buffer size for dynamic item labels produced by get_label(). */
#define MENU_SCREEN_LABEL_MAX  32

/* Forward declaration for on_extra callback signature. */
typedef struct MenuScreen MenuScreen_t;

/**
 * @brief Per-tab item descriptor.
 *
 * Each MenuTabDesc_t describes one tab's worth of items.  For single-tab
 * menus or sibling-mode menus, the tabs[] array has one entry per visual tab
 * but only the entry at default_tab is populated with items.
 */
typedef struct {
    uint8_t             item_count;     /**< Number of items in this tab (0 = empty placeholder). */
    /** Static display labels.  Set NULL and provide get_label() instead. */
    const char * const *display_labels;
    /** Dynamic label generator; NULL when display_labels is set. */
    void (*get_label)(int idx, char *buf, size_t bufsz);
    /** Called on ENTER or a valid digit shortcut with the 0-based item index. */
    void (*on_select)(int idx, lv_obj_t *screen);
} MenuTabDesc_t;

/**
 * @brief Descriptor that parameterises a MenuScreen_t.
 *
 * All pointer members must remain valid for the lifetime of the MenuScreen_t.
 */
typedef struct {
    /* Tab bar — set tab_count = 0 to omit tab bar and show title instead. */
    uint8_t             tab_count;      /**< Number of visual tabs (= length of tabs[]). */
    const char * const *tab_names;      /**< Array of tab_count label strings. NULL when tab_count=0. */
    const int          *tab_x;          /**< Array of tab_count x positions.   NULL when tab_count=0. */
    uint8_t             default_tab;    /**< Initial active tab index.           */
    bool                wrap_tabs;      /**< Within-menu LEFT wraps to last / RIGHT wraps to 0. */

    /** Optional title shown (yellow) when tab_count == 0. */
    const char         *title;

    /**
     * Per-tab item data.  Must have tab_count entries (or 1 for single-tab
     * menus).  For sibling-mode navigation, only tabs[default_tab] needs items;
     * other entries may be zero-initialised placeholders.
     */
    const MenuTabDesc_t *tabs;

    /* Navigation targets for LEFT / RIGHT sibling-mode (0 = disabled). */
    CalcMode_t          left_mode;
    CalcMode_t          right_mode;

    /** Called on CLEAR.  Typically menu_close() + hide screen. */
    void (*on_cancel)(lv_obj_t *screen);

    /** Called on LEFT / RIGHT when sibling-mode navigation is active. */
    void (*on_tab_switch)(lv_obj_t *screen, CalcMode_t mode);

    /** Extra token handling — NULL if not needed.
        Called for any token not handled by the generic navigation logic.
        Return true to consume the token, false to fall through. */
    bool (*on_extra)(Token_t t, MenuScreen_t *ms);
} MenuScreenDesc_t;

/** Generic scrolling menu screen instance. */
struct MenuScreen {
    const MenuScreenDesc_t *desc;
    MenuState_t             nav;         /**< cursor + scroll state. */
    uint8_t                 active_tab;  /**< Index into desc->tabs[] and tab bar. */
    lv_obj_t               *screen;
    lv_obj_t               *title_label; /**< Yellow title label (tab_count == 0). */
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
 * @brief Reset cursor/scroll/active_tab to defaults and refresh the display.
 *        Must be called under lvgl_lock().
 *        Does NOT show or hide the screen — the caller manages visibility.
 */
void MenuScreen_ResetAndShow(MenuScreen_t *ms);

/**
 * @brief Switch to tab idx: update active_tab, reset cursor/scroll, recolor
 *        the tab bar, and refresh item display.
 *        Must be called under lvgl_lock().
 */
void MenuScreen_SetTab(MenuScreen_t *ms, uint8_t idx);

/**
 * @brief Redraw item labels and scroll indicators from the current nav state.
 *        Must be called under lvgl_lock().
 */
void MenuScreen_UpdateDisplay(MenuScreen_t *ms);

/**
 * @brief Dispatch a token to the menu's navigation and callback logic.
 * @return true if the token was consumed, false to fall through.
 */
bool MenuScreen_HandleToken(MenuScreen_t *ms, Token_t t);

/**
 * @brief Returns true if t is a menu-opening key
 *        (TOKEN_MATH/TEST/VARS/MATRX/PRGM/Y_VARS/STAT/DRAW).
 */
bool MenuScreen_IsMenuOpeningKey(Token_t t);

/**
 * @brief Default on_extra handler: closes the current menu (via on_cancel)
 *        and returns false for graph-nav keys (Y_EQUALS/RANGE/ZOOM/GRAPH/TRACE)
 *        and menu-opening keys, allowing handle_normal_mode to process them.
 *        Returns false for all other tokens (fall-through).
 *
 * Assign to desc->on_extra for any menu that should pass graph-nav and
 * menu-switching keys through to the normal-mode dispatcher.
 */
bool MenuScreen_DefaultExtra(Token_t t, MenuScreen_t *ms);

#endif /* UI_MENU_SCREEN_H */
