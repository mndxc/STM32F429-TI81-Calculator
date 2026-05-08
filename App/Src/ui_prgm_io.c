/**
 * @file    ui_prgm_io.c
 * @brief   PRGM I/O sub-menu — Disp/Input/DispHome/DispGraph/ClrHome.
 *
 * All navigation, display, and LVGL boilerplate is owned by MenuScreen_t
 * in ui_menu_screen.c.  This file contains only the string tables,
 * descriptor, and thin public-API wrappers.
 */

#include "ui_shared.h"
#include "ui_prgm.h"
#include "ui_prgm_io.h"
#include "prgm_editor.h"
#include "ui_palette.h"
#include "ui_menu_screen.h"

/*---------------------------------------------------------------------------
 * String tables
 *---------------------------------------------------------------------------*/

#define PRGM_IO_ITEM_COUNT  5

static const char * const prgm_io_display[PRGM_IO_ITEM_COUNT] = {
    "1:Disp ",  "2:Input ", "3:DispHome", "4:DispGraph", "5:ClrHome",
};
static const char * const prgm_io_insert[PRGM_IO_ITEM_COUNT] = {
    "Disp ",    "Input ",   "DispHome",   "DispGraph",   "ClrHome",
};

/*---------------------------------------------------------------------------
 * Descriptor
 *---------------------------------------------------------------------------*/

static const char * const s_tab_names[3] = {"CTL", "I/O", "EXEC"};
static const int           s_tab_x[3]    = {4, 80, 156};

static void io_on_select(int idx, lv_obj_t *screen)
{
    PrgmEditor_InsertStr(prgm_io_insert[idx]);
    PrgmEditor_FlattenToStore();
    prgm_submenu_return_to_editor(screen);
}

static const MenuScreenDesc_t s_io_desc = {
    .tab_count      = 3,
    .tab_names      = s_tab_names,
    .tab_x          = s_tab_x,
    .active_tab     = 1,
    .item_count     = PRGM_IO_ITEM_COUNT,
    .display_labels = prgm_io_display,
    .get_label      = NULL,
    .left_mode      = MODE_PRGM_CTL_MENU,
    .right_mode     = MODE_PRGM_EXEC_MENU,
    .on_select      = io_on_select,
    .on_cancel      = prgm_submenu_return_to_editor,
    .on_tab_switch  = prgm_submenu_tab_switch,
    .on_extra       = NULL,
};

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_io;
lv_obj_t *ui_prgm_io_screen = NULL;

void ui_init_prgm_io_screen(lv_obj_t *parent)
{
    MenuScreen_Init(&s_io, &s_io_desc, parent);
    ui_prgm_io_screen = s_io.screen;
}

void ui_prgm_io_reset_and_show(void)  { MenuScreen_ResetAndShow(&s_io); }
void ui_update_prgm_io_display(void)  { MenuScreen_UpdateDisplay(&s_io); }
bool handle_prgm_io_menu(Token_t t)   { return MenuScreen_HandleToken(&s_io, t); }
