/**
 * @file    ui_prgm_exec.c
 * @brief   PRGM EXEC sub-menu — subroutine slot picker (37 slots: 1–9,0,A–Z,θ).
 *
 * All navigation, display, and LVGL boilerplate is owned by MenuScreen_t
 * in ui_menu_screen.c.  This file adds a get_label() callback for dynamic
 * slot labels and an on_extra() callback for the letter/theta shortcuts.
 */

#include "ui_shared.h"
#include "ui_prgm.h"
#include "ui_prgm_exec.h"
#include "ui_palette.h"
#include "ui_menu_screen.h"
#include <stdio.h>

/*---------------------------------------------------------------------------
 * Descriptor callbacks
 *---------------------------------------------------------------------------*/

static const char * const s_tab_names[3] = {"CTL", "I/O", "EXEC"};
static const int           s_tab_x[3]    = {4, 80, 156};

static void exec_get_label(int slot, char *buf, size_t bufsz)
{
    char id[3];
    prgm_slot_id_str((uint8_t)slot, id);
    const char *name = Prgm_GetName((uint8_t)slot);
    if (name[0] != '\0')
        snprintf(buf, bufsz, "%s:Prgm%s  %s", id, id, name);
    else
        snprintf(buf, bufsz, "%s:Prgm%s", id, id);
}

static void exec_on_select(int slot, lv_obj_t *screen)
{
    if (slot < PRGM_MAX_PROGRAMS) {
        char slot_id[3];
        prgm_slot_id_str((uint8_t)slot, slot_id);
        const char *uname = Prgm_GetName((uint8_t)slot);
        char ins[PRGM_NAME_LEN + 6];  /* "prgm" + name/id + NUL */
        snprintf(ins, sizeof(ins), "prgm%s",
                 uname[0] != '\0' ? uname : slot_id);
        prgm_editor_insert_str(ins);
        prgm_flatten_to_store();
    }
    prgm_submenu_return_to_editor(screen);
}

static bool exec_on_extra(Token_t t, MenuScreen_t *ms)
{
    int slot = -1;
    if (t >= TOKEN_A && t <= TOKEN_Z)
        slot = 10 + (int)(t - TOKEN_A);
    else if (t == TOKEN_THETA)
        slot = 36;
    if (slot >= 0 && slot < PRGM_MAX_PROGRAMS)
        exec_on_select(slot, ms->screen);
    return true;
}

static const MenuScreenDesc_t s_exec_desc = {
    .tab_count      = 3,
    .tab_names      = s_tab_names,
    .tab_x          = s_tab_x,
    .active_tab     = 2,
    .item_count     = PRGM_MAX_PROGRAMS,
    .display_labels = NULL,
    .get_label      = exec_get_label,
    .left_mode      = MODE_PRGM_IO_MENU,
    .right_mode     = MODE_PRGM_CTL_MENU,
    .on_select      = exec_on_select,
    .on_cancel      = prgm_submenu_return_to_editor,
    .on_tab_switch  = prgm_submenu_tab_switch,
    .on_extra       = exec_on_extra,
};

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

static MenuScreen_t s_exec;
lv_obj_t *ui_prgm_exec_screen = NULL;

void ui_init_prgm_exec_screen(lv_obj_t *parent)
{
    MenuScreen_Init(&s_exec, &s_exec_desc, parent);
    ui_prgm_exec_screen = s_exec.screen;
}

void ui_prgm_exec_reset_and_show(void)  { MenuScreen_ResetAndShow(&s_exec); }
void ui_update_prgm_exec_display(void)  { MenuScreen_UpdateDisplay(&s_exec); }
bool handle_prgm_exec_menu(Token_t t)   { return MenuScreen_HandleToken(&s_exec, t); }
