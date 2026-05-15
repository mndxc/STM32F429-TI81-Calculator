/**
 * @file ui_prgm.c
 * @brief Program (PRGM) slot browser, new-name entry, and output adapter.
 *
 * Owns the EXEC/EDIT/ERASE tab browser (37 slots), the new-program name-entry
 * screen, the runtime Menu( overlay, and the PrgmOutput_t hardware adapter.
 * The line editor is now in prgm_editor.c.
 *
 * Supported CTL commands: If (single-line), Goto, Lbl, IS>(, DS<(, Stop, prgm (subroutine).
 * Supported I/O commands: Disp, Input, ClrHome, Pause, DispHome, DispGraph.
 * Removed per TI-81 spec: Then/Else/While/For/Return/Prompt/Output(/Menu(.
 * Remaining: hardware validation (P10). Command reference: docs/PRGM_COMMANDS.md
 */
#include "ui_prgm.h"
#include "prgm_editor.h"
#include "ui_prgm_ctl.h"
#include "ui_prgm_io.h"
#include "ui_prgm_exec.h"
#include "ui_prgm_mode.h"
#include "ui_palette.h"
#include "ui_shared.h"
#include "calculator_core.h"
#include "expr_editor.h"
#include "calc_history.h"
#include "expr_util.h"
#include "ui_input.h"
#include "prgm_exec.h"
#include "calc_engine.h"
#include "graph.h"
#include "graph_ui.h"
#include <stdio.h>
#include <string.h>

/* PRGM menu/editor geometry */
#define PRGM_TAB_COUNT          3   /* EXEC, EDIT, NEW */
/* PRGM_MAX_LINES and PRGM_MAX_LINE_LEN are defined in prgm_exec.h (via ui_prgm.h) */

/* PRGM menu state */
static lv_obj_t   *ui_prgm_screen            = NULL;
static uint8_t     prgm_tab                  = 0;   /* 0=EXEC, 1=EDIT, 2=ERASE */
static uint8_t     prgm_item_cursor          = 0;
static uint8_t     prgm_scroll_offset        = 0;
static CalcMode_t  prgm_return_mode          = MODE_NORMAL;
static lv_obj_t   *prgm_tab_labels[PRGM_TAB_COUNT];
static lv_obj_t   *prgm_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_scroll_ind[2];         /* [0]=up, [1]=down */

/* PRGM NEW name entry state */
lv_obj_t   *ui_prgm_new_screen        = NULL;
static char        prgm_new_name[PRGM_NAME_LEN + 1] = {0};
static uint8_t     prgm_new_name_len          = 0;
static uint8_t     prgm_new_name_cursor       = 0;   /* insertion point within name [0,len] */
static uint8_t     prgm_new_slot              = 0;   /* slot index being created */
static lv_obj_t   *prgm_new_title_lbl        = NULL; /* shows "PrgmX:typed_name" */
static lv_obj_t   *prgm_new_cursor_box       = NULL;
static lv_obj_t   *prgm_new_cursor_inner     = NULL;

/* PRGM ERASE confirmation state */
static bool        prgm_erase_confirm        = false;
static uint8_t     prgm_erase_confirm_slot   = 0;   /* actual slot index to erase */
static uint8_t     prgm_erase_confirm_choice = 0;   /* 0=do not erase, 1=erase */

/* PRGM runtime Menu( screen — shown during program execution */
static lv_obj_t   *ui_prgm_menu_screen         = NULL;
static lv_obj_t   *prgm_menu_title_lbl          = NULL;
static lv_obj_t   *prgm_menu_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_menu_scroll_ind[2];

/* PRGM menu / editor static data */
static const char * const prgm_tab_names[PRGM_TAB_COUNT] = {"EXEC", "EDIT", "ERASE"};

/*===========================================================================
 * PRGM — program editor callbacks registered with prgm_editor.c
 *===========================================================================*/

/* Called by prgm_editor.c when the user navigates UP from line 0 col 0
 * in an editor opened from the new-name screen. */
static void nav_up_to_new_name(void)
{
    prgm_new_name_cursor = prgm_new_name_len; /* cursor at end of name */
    lvgl_lock();
    lv_obj_clear_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_prgm_new_display();
    lvgl_unlock();
}

/* Called by prgm_editor.c when CLEAR is pressed on an empty single-line
 * program — return to the PRGM menu browser on the EDIT tab. */
static void editor_return_to_menu(void)
{
    prgm_tab           = 1;  /* return to EDIT tab */
    prgm_item_cursor   = 0;
    prgm_scroll_offset = 0;
    lvgl_lock();
    lv_obj_clear_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_prgm_display();
    lvgl_unlock();
}

/*===========================================================================
 * PRGM — screen init
 *===========================================================================*/

/* Creates the PRGM main menu screen (hidden at startup). */
static void ui_init_prgm_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_prgm_screen = screen_create(scr);

    static const int16_t tab_x[PRGM_TAB_COUNT] = {4, 80, 157};
    for (int i = 0; i < PRGM_TAB_COUNT; i++) {
        prgm_tab_labels[i] = lv_label_create(ui_prgm_screen);
        lv_obj_set_pos(prgm_tab_labels[i], tab_x[i], 4);
        lv_obj_set_style_text_font(prgm_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_tab_labels[i], lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(prgm_tab_labels[i], prgm_tab_names[i]);
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_item_labels[i] = lv_label_create(ui_prgm_screen);
        lv_obj_set_pos(prgm_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_item_labels[i], "");
    }

    /* Scroll indicators — opaque bg covers the colon in items beneath */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        prgm_scroll_ind[i] = lv_label_create(ui_prgm_screen);
        lv_obj_set_pos(prgm_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(prgm_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(prgm_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(prgm_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(prgm_scroll_ind[i], 0, 0);
        lv_label_set_text(prgm_scroll_ind[i], i == 0 ? "\xE2\x86\x91" : "\xE2\x86\x93");
        lv_obj_add_flag(prgm_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* Creates the PRGM NEW name-entry screen (hidden at startup).
 * Layout matches original TI-81: "PrgmX:typed_name" on row 0 with cursor,
 * then ":" on row 1 as the first (empty) code line stub. */
static void ui_init_prgm_new_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_prgm_new_screen = screen_create(scr);

    /* Row 0: "PrgmX:name" — updated dynamically in ui_update_prgm_new_display */
    prgm_new_title_lbl = lv_label_create(ui_prgm_new_screen);
    lv_obj_set_pos(prgm_new_title_lbl, 4, 4);
    lv_obj_set_style_text_font(prgm_new_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(prgm_new_title_lbl, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(prgm_new_title_lbl, "Prgm1:");

    /* Row 1: first code line stub */
    lv_obj_t *code_stub = lv_label_create(ui_prgm_new_screen);
    lv_obj_set_pos(code_stub, 4, 34);
    lv_obj_set_style_text_font(code_stub, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(code_stub, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(code_stub, ":");

    cursor_box_create(ui_prgm_new_screen, true,
                      &prgm_new_cursor_box, &prgm_new_cursor_inner);
}

/* Creates the runtime Menu( overlay screen (hidden at startup). */
static void ui_init_prgm_menu_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_prgm_menu_screen = screen_create(scr);

    prgm_menu_title_lbl = lv_label_create(ui_prgm_menu_screen);
    lv_obj_set_pos(prgm_menu_title_lbl, 4, 4);
    lv_obj_set_style_text_font(prgm_menu_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(prgm_menu_title_lbl, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(prgm_menu_title_lbl, "");

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_menu_item_labels[i] = lv_label_create(ui_prgm_menu_screen);
        lv_obj_set_pos(prgm_menu_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_menu_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_menu_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_menu_item_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        prgm_menu_scroll_ind[i] = lv_label_create(ui_prgm_menu_screen);
        lv_obj_set_pos(prgm_menu_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(prgm_menu_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_menu_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(prgm_menu_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(prgm_menu_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(prgm_menu_scroll_ind[i], 0, 0);
        lv_label_set_text(prgm_menu_scroll_ind[i], i == 0 ? "\xE2\x86\x91" : "\xE2\x86\x93");
        lv_obj_add_flag(prgm_menu_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/*===========================================================================
 * Slot & display utilities
 *===========================================================================*/

/* Returns the display identifier string for a program slot (0-based index).
 * out must have room for 3 bytes.  Mapping: 0-8→'1'-'9', 9→'0', 10-35→'A'-'Z', 36→θ. */
void prgm_slot_id_str(uint8_t slot, char *out)
{
    if (slot <= 8)       { out[0] = (char)('1' + slot); out[1] = '\0'; }
    else if (slot == 9)  { out[0] = '0';                out[1] = '\0'; }
    else if (slot <= 35) { out[0] = (char)('A' + (slot - 10)); out[1] = '\0'; }
    else                 { out[0] = '\xCE'; out[1] = '\xB8'; out[2] = '\0'; } /* θ U+03B8 */
}

/* Returns true if the slot has a program (name is non-empty). */
bool prgm_slot_is_used(uint8_t slot)
{
    return Prgm_IsSlotOccupied(slot);
}

/* Updates PRGM menu labels and tab highlights.  Must be called under lvgl_lock. */
void ui_update_prgm_display(void)
{
    /* Tab highlights */
    for (int i = 0; i < PRGM_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(prgm_tab_labels[i],
            lv_color_hex(i == (int)prgm_tab ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
    }

    /* buf for "<id>:Prgm<id>  NNNNNNNN\0" — 2+5+2+2+8+1=20 bytes max */
    char buf[24];
    char id[3];

    /* ERASE confirmation dialog overrides list view */
    if (prgm_erase_confirm) {
        prgm_slot_id_str(prgm_erase_confirm_slot, id);
        char title[20];
        const char *cname = Prgm_GetName(prgm_erase_confirm_slot);
        if (cname[0] != '\0')
            snprintf(title, sizeof(title), "Prgm%s  %s", id, cname);
        else
            snprintf(title, sizeof(title), "Prgm%s", id);
        lv_label_set_text(prgm_item_labels[0], title);
        lv_obj_set_style_text_color(prgm_item_labels[0], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_item_labels[1], "1:Do not erase");
        lv_obj_set_style_text_color(prgm_item_labels[1],
            lv_color_hex(prgm_erase_confirm_choice == 0 ? COLOR_YELLOW : COLOR_WHITE), 0);
        lv_label_set_text(prgm_item_labels[2], "2:Erase");
        lv_obj_set_style_text_color(prgm_item_labels[2],
            lv_color_hex(prgm_erase_confirm_choice == 1 ? COLOR_YELLOW : COLOR_WHITE), 0);
        for (int i = 3; i < MENU_VISIBLE_ROWS; i++)
            lv_label_set_text(prgm_item_labels[i], "");
        lv_obj_add_flag(prgm_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(prgm_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* All tabs — all 37 slots */
    int total = PRGM_MAX_PROGRAMS;
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int slot = (int)prgm_scroll_offset + i;
        if (slot < total) {
            prgm_slot_id_str((uint8_t)slot, id);
            const char *name = Prgm_GetName((uint8_t)slot);
            if (name[0] != '\0')
                snprintf(buf, sizeof(buf), "%s:Prgm%s  %s", id, id, name);
            else
                snprintf(buf, sizeof(buf), "%s:Prgm%s", id, id);
            lv_label_set_text(prgm_item_labels[i], buf);
            lv_obj_set_style_text_color(prgm_item_labels[i],
                lv_color_hex(i == (int)prgm_item_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_item_labels[i], "");
        }
    }

    /* Scroll indicators */
    if (prgm_scroll_offset > 0)
        lv_obj_clear_flag(prgm_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_scroll_offset + MENU_VISIBLE_ROWS) < total)
        lv_obj_clear_flag(prgm_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

/*===========================================================================
 * New-name screen cursor/display
 *===========================================================================*/

/* Positions the new-name cursor box without updating the label text. */
void prgm_new_cursor_update(void)
{
    if (prgm_new_cursor_box == NULL || prgm_new_title_lbl == NULL) return;
    cursor_render(prgm_new_cursor_box, prgm_new_cursor_inner,
                  prgm_new_title_lbl, (uint32_t)(6 + prgm_new_name_cursor),
                  Calc_GetCursorVisible(), Calc_GetMode(), false);
}

/* Updates the new-program name-entry label and cursor. */
void ui_update_prgm_new_display(void)
{
    /* Build "PrgmX:typed_name" in one buffer; cursor glyph index = 6 + name_len.
     * θ (slot 36) is 2 UTF-8 bytes but 1 glyph, so "Prgmθ:" is always 6 glyphs. */
    char id[3];
    prgm_slot_id_str(prgm_new_slot, id);
    char buf[4 + 2 + 1 + PRGM_NAME_LEN + 1]; /* "Prgm" + id(≤2) + ":" + name + NUL */
    snprintf(buf, sizeof(buf), "Prgm%s:%s", id, prgm_new_name);
    lv_label_set_text(prgm_new_title_lbl, buf);
    cursor_render(prgm_new_cursor_box, prgm_new_cursor_inner,
                  prgm_new_title_lbl, (uint32_t)(6 + prgm_new_name_cursor),
                  Calc_GetCursorVisible(), Calc_GetMode(), false);
}

/*===========================================================================
 * prgm_flatten_to_store — declared in ui_prgm.h for sub-menu backward compat.
 * Delegates to prgm_editor.c.
 *===========================================================================*/

void prgm_flatten_to_store(void)
{
    PrgmEditor_FlattenToStore();
}

/*===========================================================================
 * PRGM slot browser — menu token handlers
 *===========================================================================*/

/* Helper: return the total number of items in the current prgm_tab view. */
static int prgm_menu_total(void)
{
    return PRGM_MAX_PROGRAMS;  /* all tabs show all 37 slots */
}

/* Handle keys while the ERASE confirmation dialog is active. */
static bool handle_erase_confirm(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
    case TOKEN_DOWN:
        prgm_erase_confirm_choice ^= 1;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_1:
        /* Immediately cancel (A4) */
        prgm_erase_confirm = false;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_2:
        /* Immediately erase (A4) */
        Prgm_ClearSlot(prgm_erase_confirm_slot);
        prgm_erase_confirm = false;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        if (prgm_erase_confirm_choice == 1)
            Prgm_ClearSlot(prgm_erase_confirm_slot);
        prgm_erase_confirm = false;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
        prgm_erase_confirm = false;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    default:
        return true; /* absorb all other keys during confirmation */
    }
}

/* ENTER on the EXEC tab: insert prgmNAME into the calculator expression (C1). */
static void enter_exec_tab(int abs_pos)
{
    if (abs_pos >= PRGM_MAX_PROGRAMS) return;
    char slot_id[3];
    prgm_slot_id_str((uint8_t)abs_pos, slot_id);
    const char *uname = Prgm_GetName((uint8_t)abs_pos);
    char tmp[MAX_EXPR_LEN];
    snprintf(tmp, MAX_EXPR_LEN, "prgm%s",
             uname[0] != '\0' ? uname : slot_id);
    ExprEditor_LoadStr(tmp);
    CalcMode_t exec_ret = prgm_return_mode;
    prgm_return_mode   = MODE_NORMAL;
    prgm_tab           = 0;
    prgm_item_cursor   = 0;
    prgm_scroll_offset = 0;
    Calc_SetMode(exec_ret);
    lvgl_lock();
    lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();
    Update_Calculator_Display();
}

/* ENTER on the EDIT tab: open editor for used slot, name-entry for empty slot. */
static void enter_edit_tab(int abs_pos)
{
    if (abs_pos >= PRGM_MAX_PROGRAMS) return;
    bool has_name = prgm_slot_is_used((uint8_t)abs_pos);
    bool has_body = (Prgm_GetBody((uint8_t)abs_pos)[0] != '\0');
    if (has_name || has_body) {
        /* D3: body-only slot opens editor directly (no name-entry) */
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        PrgmEditor_Open((uint8_t)abs_pos, false);
    } else {
        /* Empty slot — show name-entry screen; auto-engage ALPHA */
        prgm_new_slot        = (uint8_t)abs_pos;
        prgm_new_name_len    = 0;
        prgm_new_name_cursor = 0;
        memset(prgm_new_name, 0, sizeof(prgm_new_name));
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_screen,       LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_new_display();
        lvgl_unlock();
        Calc_SetMode(MODE_ALPHA);
        Calc_SetReturnMode(MODE_PRGM_NEW_NAME);
    }
}

/* ENTER on the ERASE tab: show confirmation dialog for selected slot (A1). */
static void enter_erase_tab(int abs_pos)
{
    if (abs_pos >= PRGM_MAX_PROGRAMS) return;
    prgm_erase_confirm        = true;
    prgm_erase_confirm_slot   = (uint8_t)abs_pos;
    prgm_erase_confirm_choice = 0;
    lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
}

bool handle_prgm_menu(Token_t t)
{
    if (prgm_erase_confirm)
        return handle_erase_confirm(t);

    int total = prgm_menu_total();
    switch (t) {
    case TOKEN_LEFT:
        prgm_erase_confirm = false;
        /* D2: wrap — LEFT at EXEC(0) wraps to ERASE(2) */
        prgm_tab           = (prgm_tab == 0) ? (PRGM_TAB_COUNT - 1) : (prgm_tab - 1);
        prgm_item_cursor   = 0;
        prgm_scroll_offset = 0;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        prgm_erase_confirm = false;
        /* D2: wrap — RIGHT at ERASE(2) wraps to EXEC(0) */
        prgm_tab           = (prgm_tab + 1) % PRGM_TAB_COUNT;
        prgm_item_cursor   = 0;
        prgm_scroll_offset = 0;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_UP:
        if (prgm_item_cursor > 0)
            prgm_item_cursor--;
        else if (prgm_scroll_offset > 0)
            prgm_scroll_offset--;
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(prgm_scroll_offset + prgm_item_cursor) + 1 < total) {
            if (prgm_item_cursor < MENU_VISIBLE_ROWS - 1)
                prgm_item_cursor++;
            else if ((int)(prgm_scroll_offset + MENU_VISIBLE_ROWS) < total)
                prgm_scroll_offset++;
        }
        lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int abs_pos = (int)prgm_scroll_offset + (int)prgm_item_cursor;
        if (prgm_tab == 0)      enter_exec_tab(abs_pos);
        else if (prgm_tab == 1) enter_edit_tab(abs_pos);
        else                    enter_erase_tab(abs_pos);
        return true;
    }
    case TOKEN_1: case TOKEN_2: case TOKEN_3: case TOKEN_4: case TOKEN_5:
    case TOKEN_6: case TOKEN_7: case TOKEN_8: case TOKEN_9: case TOKEN_0: {
        /* A3/F7: direct slot shortcut for all three tabs */
        int slot = (t == TOKEN_0) ? 9 : (int)(t - TOKEN_1);
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_scroll_offset = (slot >= MENU_VISIBLE_ROWS)
                ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
            prgm_item_cursor   = (uint8_t)(slot - (int)prgm_scroll_offset);
            return handle_prgm_menu(TOKEN_ENTER);
        }
        return true;
    }
    case TOKEN_A ... TOKEN_Z: {
        /* F6: ALPHA+letter slot shortcut — slots 10 (A) through 35 (Z) */
        int slot = 10 + (int)(t - TOKEN_A);
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_scroll_offset = (slot >= MENU_VISIBLE_ROWS)
                ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
            prgm_item_cursor   = (uint8_t)(slot - (int)prgm_scroll_offset);
            return handle_prgm_menu(TOKEN_ENTER);
        }
        return true;
    }
    case TOKEN_THETA: {
        /* F6: ALPHA+θ slot shortcut — slot 36 */
        int slot = 36;
        prgm_scroll_offset = (slot >= MENU_VISIBLE_ROWS)
            ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
        prgm_item_cursor   = (uint8_t)(slot - (int)prgm_scroll_offset);
        return handle_prgm_menu(TOKEN_ENTER);
    }
    case TOKEN_CLEAR:
    case TOKEN_PRGM: {
        prgm_erase_confirm   = false;
        CalcMode_t ret = prgm_return_mode;
        prgm_return_mode     = MODE_NORMAL;
        prgm_tab             = 0;
        prgm_item_cursor     = 0;
        prgm_scroll_offset   = 0;
        Calc_SetMode(ret);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        return true;
    }
    default: {
        /* Any other key: close menu, fall through */
        prgm_erase_confirm = false;
        CalcMode_t ret = prgm_return_mode;
        prgm_return_mode   = MODE_NORMAL;
        prgm_item_cursor   = 0;
        prgm_scroll_offset = 0;
        Calc_SetMode(ret);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        return false;
    }
    }
    return true;
}

bool handle_prgm_new_name(Token_t t)
{
    switch (t) {
    case TOKEN_A ... TOKEN_Z: {
        if (prgm_new_name_len < PRGM_NAME_LEN) {
            char ch = (char)('A' + (t - TOKEN_A));
            memmove(prgm_new_name + prgm_new_name_cursor + 1,
                    prgm_new_name + prgm_new_name_cursor,
                    prgm_new_name_len - prgm_new_name_cursor + 1);
            prgm_new_name[prgm_new_name_cursor] = ch;
            prgm_new_name_cursor++;
            prgm_new_name_len++;
            lvgl_lock(); ui_update_prgm_new_display(); lvgl_unlock();
        }
        /* Re-engage ALPHA so the next keypress is also a letter */
        Calc_SetReturnMode(MODE_PRGM_NEW_NAME);
        Calc_SetMode(MODE_ALPHA);
        return true;
    }
    case TOKEN_0 ... TOKEN_9: {
        if (prgm_new_name_len < PRGM_NAME_LEN) {
            char ch = (char)('0' + (t - TOKEN_0));
            memmove(prgm_new_name + prgm_new_name_cursor + 1,
                    prgm_new_name + prgm_new_name_cursor,
                    prgm_new_name_len - prgm_new_name_cursor + 1);
            prgm_new_name[prgm_new_name_cursor] = ch;
            prgm_new_name_cursor++;
            prgm_new_name_len++;
            lvgl_lock(); ui_update_prgm_new_display(); lvgl_unlock();
        }
        /* Re-engage ALPHA so the next keypress can still be a letter */
        Calc_SetReturnMode(MODE_PRGM_NEW_NAME);
        Calc_SetMode(MODE_ALPHA);
        return true;
    }
    case TOKEN_DEL:
        if (prgm_new_name_cursor > 0) {
            memmove(prgm_new_name + prgm_new_name_cursor - 1,
                    prgm_new_name + prgm_new_name_cursor,
                    prgm_new_name_len - prgm_new_name_cursor + 1);
            prgm_new_name_cursor--;
            prgm_new_name_len--;
            lvgl_lock(); ui_update_prgm_new_display(); lvgl_unlock();
        }
        /* Re-engage ALPHA after DEL so the next keypress is still a letter */
        Calc_SetReturnMode(MODE_PRGM_NEW_NAME);
        Calc_SetMode(MODE_ALPHA);
        return true;
    case TOKEN_LEFT:
        if (prgm_new_name_cursor > 0) {
            prgm_new_name_cursor--;
            lvgl_lock(); prgm_new_cursor_update(); lvgl_unlock();
        }
        return true;
    case TOKEN_RIGHT:
        if (prgm_new_name_cursor < prgm_new_name_len) {
            prgm_new_name_cursor++;
            lvgl_lock(); prgm_new_cursor_update(); lvgl_unlock();
        }
        return true;
    case TOKEN_DOWN:
        /* Navigate into editor body — save name first */
        if (prgm_new_name_len > 0)
            Prgm_SetName(prgm_new_slot, prgm_new_name);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        PrgmEditor_SetNavUpCallback(nav_up_to_new_name);
        PrgmEditor_Open(prgm_new_slot, true);
        return true;
    case TOKEN_ENTER:
        /* Save user name if typed; open editor regardless (name is optional) */
        if (prgm_new_name_len > 0)
            Prgm_SetName(prgm_new_slot, prgm_new_name);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        PrgmEditor_SetNavUpCallback(nav_up_to_new_name);
        PrgmEditor_Open(prgm_new_slot, true);
        return true;
    case TOKEN_CLEAR:
    case TOKEN_PRGM:
        /* Cancel — return to PRGM menu */
        Calc_SetMode(MODE_PRGM_MENU);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_screen,   LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_display();
        lvgl_unlock();
        return true;
    default:
        return true;  /* absorb all other keys in name entry */
    }
}

bool handle_prgm_editor(Token_t t)
{
    return PrgmEditor_HandleToken(t);
}

void prgm_submenu_return_to_editor(lv_obj_t *hide_screen)
{
    Calc_SetMode(MODE_PRGM_EDITOR);
    lvgl_lock();
    lv_obj_add_flag(hide_screen,                    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(PrgmEditor_GetScreen(),       LV_OBJ_FLAG_HIDDEN);
    PrgmEditor_RefreshDisplay();
    lvgl_unlock();
}

void prgm_submenu_tab_switch(lv_obj_t *hide_screen, CalcMode_t to_mode)
{
    Calc_SetMode(to_mode);
    lvgl_lock();
    lv_obj_add_flag(hide_screen, LV_OBJ_FLAG_HIDDEN);
    if (to_mode == MODE_PRGM_CTL_MENU)
        ui_prgm_ctl_reset_and_show();
    else if (to_mode == MODE_PRGM_IO_MENU)
        ui_prgm_io_reset_and_show();
    else if (to_mode == MODE_PRGM_MODE_NUMBER)
        ui_prgm_mode_num_reset_and_show();
    else if (to_mode == MODE_PRGM_MODE_GRAPH)
        ui_prgm_mode_gph_reset_and_show();
    else
        ui_prgm_exec_reset_and_show();
    lvgl_unlock();
}


/** Show (or refresh) the runtime Menu( overlay.  Must be called under lvgl_lock(). */
void ui_prgm_menu_show(const char *title,
                        const char texts[][PRGM_MAX_LINE_LEN],
                        uint8_t count, uint8_t cursor, uint8_t scroll)
{
    lv_label_set_text(prgm_menu_title_lbl, title);
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)scroll + i;
        if (idx < (int)count) {
            char buf[PRGM_MAX_LINE_LEN + 4];
            snprintf(buf, sizeof(buf), "%d:%s", idx + 1, texts[idx]);
            lv_label_set_text(prgm_menu_item_labels[i], buf);
            lv_obj_set_style_text_color(prgm_menu_item_labels[i],
                lv_color_hex(idx == (int)cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_menu_item_labels[i], "");
        }
    }
    if (scroll > 0)
        lv_obj_clear_flag(prgm_menu_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_menu_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(scroll + MENU_VISIBLE_ROWS) < (int)count)
        lv_obj_clear_flag(prgm_menu_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_menu_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_prgm_menu_screen, LV_OBJ_FLAG_HIDDEN);
}

/** Hide the runtime Menu( overlay.  Must be called under lvgl_lock(). */
void ui_prgm_menu_hide(void)
{
    if (ui_prgm_menu_screen)
        lv_obj_add_flag(ui_prgm_menu_screen, LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------------------
 * Embedded PrgmOutput_t adapter — translates executor callbacks into LVGL/
 * graph calls.  Registered via Prgm_SetOutput() in ui_init_prgm_screens().
 *---------------------------------------------------------------------------*/

static void hw_disp_text(const char *expr, const char *result)
{
    CalcHistory_Commit(expr, result, false, 0, 0, 0);
    lvgl_lock(); CalcHistory_UpdateDisplay(); lvgl_unlock();
}

static void hw_prog_done(void)
{
    CalcHistory_Commit("", "Done", false, 0, 0, 0);
    lvgl_lock(); CalcHistory_UpdateDisplay(); lvgl_unlock();
}

static void hw_clr_home(void)
{
    lvgl_lock(); CalcHistory_UpdateDisplay(); lvgl_unlock();
}

static void hw_disp_graph(void)
{
    lvgl_lock();
    hide_all_screens();
    Graph_SetVisible(true);
    lvgl_unlock();
    osDelay(20);
    Graph_Render();
}

static void hw_show_home(void)
{
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
}

static void hw_input_ready(void)
{
    Update_Calculator_Display();
}

/* Called when the user presses ENTER after exploring the graph via a no-arg
 * Input command.  Stores cursor (X, Y) into the corresponding calc variables,
 * returns to the home screen, and resumes program execution. */
static void prgm_graph_resume(float x, float y)
{
    calc_variables['X' - 'A'] = x;
    calc_variables['Y' - 'A'] = y;
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
    Calc_SetMode(MODE_PRGM_RUNNING);
    Prgm_ClearInputWait();
    Prgm_RunLoop();
}

/* Called when the user presses CLEAR during graph-exploration Input.
 * Aborts the program and returns to the home screen in normal mode. */
static void prgm_graph_abort(void)
{
    Prgm_ResetExecutionState();
    ExprEditor_Clear();
    Calc_SetMode(MODE_NORMAL);
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
}

static void hw_input_graph(void)
{
    Graph_StartPrgmInput(prgm_graph_resume, prgm_graph_abort);
}

static const PrgmOutput_t k_hw_output = {
    .disp_text   = hw_disp_text,
    .prog_done   = hw_prog_done,
    .clr_home    = hw_clr_home,
    .disp_graph  = hw_disp_graph,
    .show_home   = hw_show_home,
    .input_ready = hw_input_ready,
    .input_graph = hw_input_graph,
};

void ui_init_prgm_screens(void)
{
    Prgm_SetOutput(&k_hw_output);
    ui_init_prgm_screen();
    ui_init_prgm_new_screen();
    ui_init_prgm_menu_screen();

    PrgmEditor_SetReturnToMenuCallback(editor_return_to_menu);
    lv_obj_t *scr = lv_scr_act();
    PrgmEditor_InitScreen(scr);

    /* CTL/I/O/EXEC/MODE sub-menus — init order must follow PrgmEditor_InitScreen */
    ui_init_prgm_ctl_screen(scr);
    ui_init_prgm_io_screen(scr);
    ui_init_prgm_exec_screen(scr);
    ui_init_prgm_mode_screens(scr);
}

bool Prgm_IsEditorScreenVisible(void)
{
    lv_obj_t *s = PrgmEditor_GetScreen();
    return s != NULL && !lv_obj_has_flag(s, LV_OBJ_FLAG_HIDDEN);
}

bool Prgm_IsNewScreenVisible(void)
{
    return ui_prgm_new_screen != NULL &&
           !lv_obj_has_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
}

void hide_prgm_screens(void)
{
    if (ui_prgm_screen)         lv_obj_add_flag(ui_prgm_screen,         LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_new_screen)     lv_obj_add_flag(ui_prgm_new_screen,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *es = PrgmEditor_GetScreen();
    if (es)                     lv_obj_add_flag(es,                      LV_OBJ_FLAG_HIDDEN);
    ui_prgm_ctl_hide();
    ui_prgm_io_hide();
    ui_prgm_exec_hide();
    ui_prgm_mode_num_hide();
    ui_prgm_mode_gph_hide();
    if (ui_prgm_menu_screen)    lv_obj_add_flag(ui_prgm_menu_screen,    LV_OBJ_FLAG_HIDDEN);
}

void prgm_reset_state(CalcMode_t target_mode) {
    if (target_mode == MODE_PRGM_MENU) {
        prgm_return_mode = Calc_GetMode();
        prgm_tab = 0;
        prgm_item_cursor = 0;
        prgm_scroll_offset = 0;
        Calc_SetMode(MODE_PRGM_MENU);
        lv_obj_clear_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_display();
    } else if (target_mode == MODE_PRGM_RUNNING) {
        prgm_return_mode = Calc_GetMode();
        prgm_tab = 0;
        prgm_item_cursor = 0;
        prgm_scroll_offset = 0;
        Calc_SetMode(MODE_PRGM_RUNNING);
        Prgm_RunStart(PrgmEditor_GetSlot());
    }
}

void prgm_menu_open(CalcMode_t return_to) {
    prgm_return_mode = return_to;
    prgm_tab = 0;
    prgm_item_cursor = 0;
    prgm_scroll_offset = 0;
    Calc_SetMode(MODE_PRGM_MENU);
    lv_obj_clear_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_prgm_display();
}

CalcMode_t prgm_menu_close(void) {
    CalcMode_t ret = prgm_return_mode;
    prgm_tab = 0;
    prgm_item_cursor = 0;
    prgm_scroll_offset = 0;
    return ret;
}

/**
 * @brief Token handler for MODE_PRGM_RUNNING.
 *
 * Intercepts tokens while a program is executing (waiting at Pause/Input/
 * Prompt).  CLEAR aborts; ENTER resumes; other tokens feed the expression
 * input buffer while waiting for Input/Prompt.
 *
 * Moved here from prgm_exec.c: declared in ui_prgm.h; belongs in the UI
 * super-module where handle_normal_mode() and the shared expression state
 * are accessible.
 */
bool handle_prgm_running(Token_t t)
{
    if (Prgm_IsWaitingInput()) {
        if (t == TOKEN_ENTER) {
            char input_var = Prgm_GetInputVar();
            if (input_var != 0) {
                /* Evaluate and store to the target variable */
                const char *ebuf = ExprEditor_GetBuf();
                CalcResult_t r = Calc_Evaluate(ebuf, Calc_GetAns(),
                                               Calc_GetAnsIsMatrix(), Calc_GetAngleDegrees());
                char res_buf[MAX_RESULT_LEN];
                format_calc_result(&r, res_buf, MAX_RESULT_LEN);
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[input_var - 'A'] = r.value;
                    /* ans already updated by format_calc_result */
                }
                /* Append expression + result to history */
                CalcHistory_Commit(ebuf, res_buf, false, 0, 0, 0);
            }
            ExprEditor_Clear();
            Prgm_ClearInputWait();
            lvgl_lock(); CalcHistory_UpdateDisplay(); lvgl_unlock();
            Prgm_RunLoop();  /* resume execution */
            return true;
        }
        if (t == TOKEN_DEL) {
            expr_delete_at_cursor();
            Update_Calculator_Display();
            return true;
        }
        if (t == TOKEN_CLEAR) {
            if (ExprEditor_GetLen() > 0) {
                ExprEditor_Clear();
                Update_Calculator_Display();
            } else {
                /* Abort on CLEAR with empty expression */
                Prgm_ResetExecutionState();
                Calc_SetMode(MODE_NORMAL);
                lvgl_lock(); ui_refresh_display(); lvgl_unlock();
            }
            return true;
        }
        /* Block keys that would navigate away or change mode */
        switch (t) {
        case TOKEN_GRAPH: case TOKEN_Y_EQUALS: case TOKEN_RANGE:
        case TOKEN_ZOOM:  case TOKEN_TRACE:
        case TOKEN_MATH:  case TOKEN_TEST:    case TOKEN_MATRX:
        case TOKEN_PRGM:  case TOKEN_STO:     case TOKEN_INS:
        case TOKEN_LEFT:  case TOKEN_RIGHT:
        case TOKEN_UP:    case TOKEN_DOWN:
        case TOKEN_2ND:   case TOKEN_ALPHA:   case TOKEN_A_LOCK:
        case TOKEN_QUIT:
            return true; /* consume silently */
        default: {
            /* Route expression tokens through the normal-mode handler.
             * Safe subset: expression-building keys never change current_mode. */
            CalcMode_t saved = Calc_GetMode();
            Calc_SetMode(MODE_NORMAL);
            handle_normal_mode(t);
            Calc_SetMode(saved);
            return true;
        }
        }
    }

    /* Not waiting for input — abort on CLEAR, consume everything else */
    if (t == TOKEN_CLEAR) {
        Prgm_ResetExecutionState();
        ExprEditor_Clear();
        Calc_SetMode(MODE_NORMAL);
        lvgl_lock(); ui_refresh_display(); lvgl_unlock();
        return true;
    }
    return true; /* consume all tokens while running */
}
