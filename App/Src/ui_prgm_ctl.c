/**
 * @file    ui_prgm_ctl.c
 * @brief   PRGM CTL sub-menu — Lbl/Goto/If/IS>/DS</Pause/End/Stop.
 *
 * All navigation, display, and LVGL boilerplate is owned by MenuScreen_t
 * in ui_menu_screen.c.  This file contains only the string tables,
 * descriptor, and thin public-API wrappers.
 */

#include "ui_shared.h"
#include "ui_prgm.h"
#include "ui_prgm_ctl.h"
#include "ui_palette.h"
#include "ui_menu_screen.h"

/*---------------------------------------------------------------------------
 * String tables
 *---------------------------------------------------------------------------*/

#define PRGM_CTL_ITEM_COUNT  8

static const char * const prgm_ctl_display[PRGM_CTL_ITEM_COUNT] = {
    "1:Lbl ",   "2:Goto ",  "3:If ",   "4:IS>(",
    "5:DS<(",   "6:Pause",  "7:End",   "8:Stop",
};
static const char * const prgm_ctl_insert[PRGM_CTL_ITEM_COUNT] = {
    "Lbl ",     "Goto ",    "If ",     "IS>(",
    "DS<(",     "Pause",    "End",     "Stop",
};

/*---------------------------------------------------------------------------
 * Descriptor
 *---------------------------------------------------------------------------*/

static const char * const s_tab_names[3] = {"CTL", "I/O", "EXEC"};
static const int           s_tab_x[3]    = {4, 80, 156};

static void ctl_on_select(int idx, lv_obj_t *screen)
{
    if (idx < PRGM_CTL_ITEM_COUNT) {
        prgm_editor_insert_str(prgm_ctl_insert[idx]);
        prgm_flatten_to_store();
    }
    prgm_submenu_return_to_editor(screen);
}

static const MenuScreenDesc_t s_ctl_desc = {
    .tab_count      = 3,
    .tab_names      = s_tab_names,
    .tab_x          = s_tab_x,
    .active_tab     = 0,
    .item_count     = PRGM_CTL_ITEM_COUNT,
    .display_labels = prgm_ctl_display,
    .get_label      = NULL,
    .left_mode      = MODE_PRGM_EXEC_MENU,
    .right_mode     = MODE_PRGM_IO_MENU,
    .on_select      = ctl_on_select,
    .on_cancel      = prgm_submenu_return_to_editor,
    .on_tab_switch  = prgm_submenu_tab_switch,
    .on_extra       = NULL,
};

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_ctl;
lv_obj_t *ui_prgm_ctl_screen = NULL;

void ui_init_prgm_ctl_screen(lv_obj_t *parent)
{
    MenuScreen_Init(&s_ctl, &s_ctl_desc, parent);
    ui_prgm_ctl_screen = s_ctl.screen;
}

void ui_prgm_ctl_reset_and_show(void)  { MenuScreen_ResetAndShow(&s_ctl); }
void ui_update_prgm_ctl_display(void)  { MenuScreen_UpdateDisplay(&s_ctl); }
bool handle_prgm_ctl_menu(Token_t t)   { return MenuScreen_HandleToken(&s_ctl, t); }
