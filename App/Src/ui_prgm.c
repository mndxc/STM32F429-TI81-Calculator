/**
 * @file ui_prgm.c
 * @brief Program (PRGM) Menu and Editor UI Module Implementation
 *
 * Handles PRGM menu UI (EXEC/EDIT/ERASE tabs, 37 slots), name-entry screen,
 * program line editor (CTL/I/O sub-menus), and editor ↔ FLASH store round-trip.
 * Execution is delegated to prgm_exec.c.
 *
 * Supported CTL commands: If (single-line), Goto, Lbl, IS>(, DS<(, Stop, prgm (subroutine).
 * Supported I/O commands: Disp, Input, ClrHome, Pause, DispHome, DispGraph.
 * Removed per TI-81 spec: Then/Else/While/For/Return/Prompt/Output(/Menu(.
 * Remaining: hardware validation (P10). Command reference: docs/PRGM_COMMANDS.md
 */
#include "ui_prgm.h"
#include "ui_palette.h"
#include "calc_internal.h"
#include "prgm_exec.h"
#include "calc_engine.h"
#include <stdio.h>
#include <string.h>

/* PRGM menu/editor geometry */
#define PRGM_TAB_COUNT          3   /* EXEC, EDIT, NEW */
#define PRGM_CTL_ITEM_COUNT     8   /* CTL sub-menu items */
#define PRGM_IO_ITEM_COUNT      5   /* I/O sub-menu items */
#define PRGM_EDITOR_VISIBLE     7   /* Visible editor rows (matches MENU_VISIBLE_ROWS) */
/* PRGM_MAX_LINES and PRGM_MAX_LINE_LEN are defined in prgm_exec.h (via ui_prgm.h) */

/* PRGM_CTRL_DEPTH, PRGM_CALL_DEPTH, PRGM_MAX_LINES, PRGM_MAX_LINE_LEN,
 * CtrlType_t, CtrlFrame_t, and CallFrame_t are defined in prgm_exec.h. */
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
static bool        prgm_editor_from_new       = false; /* true when editor opened from name entry */
static lv_obj_t   *prgm_new_title_lbl        = NULL; /* shows "PrgmX:typed_name" */
static lv_obj_t   *prgm_new_cursor_box       = NULL;
static lv_obj_t   *prgm_new_cursor_inner     = NULL;

/* PRGM ERASE confirmation state */
static bool        prgm_erase_confirm        = false;
static uint8_t     prgm_erase_confirm_slot   = 0;   /* actual slot index to erase */
static uint8_t     prgm_erase_confirm_choice = 0;   /* 0=do not erase, 1=erase */

/* PRGM editor state */
lv_obj_t   *ui_prgm_editor_screen     = NULL;
static uint8_t     prgm_edit_idx             = 0;   /* which program is being edited */
static uint8_t     prgm_edit_line            = 0;   /* current line (0-based) */
static uint8_t     prgm_edit_scroll          = 0;   /* first visible line */
static uint8_t     prgm_edit_col             = 0;   /* cursor byte-offset within current line */
uint8_t            prgm_edit_num_lines       = 0;   /* total lines in active program */
static lv_obj_t   *prgm_edit_title_lbl       = NULL;
static lv_obj_t   *prgm_edit_line_labels[PRGM_EDITOR_VISIBLE];
static lv_obj_t   *prgm_edit_scroll_up       = NULL;
static lv_obj_t   *prgm_edit_scroll_down     = NULL;
static lv_obj_t   *prgm_edit_cursor_box      = NULL;
static lv_obj_t   *prgm_edit_cursor_inner    = NULL;
/* Working line buffer for active program — plain .bss; shared with prgm_exec.c */
char               prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];

/* PRGM CTL sub-menu state */
static lv_obj_t   *ui_prgm_ctl_screen        = NULL;
static uint8_t     prgm_ctl_cursor           = 0;
static uint8_t     prgm_ctl_scroll           = 0;
static lv_obj_t   *prgm_ctl_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_ctl_scroll_ind[2];

/* PRGM I/O sub-menu state */
static lv_obj_t   *ui_prgm_io_screen        = NULL;
static lv_obj_t *prgm_sub_tab_labels_ctl[3];   /* CTL, I/O, EXEC */
static lv_obj_t *prgm_sub_tab_labels_io[3];    /* CTL, I/O, EXEC */

/* PRGM EXEC sub-menu (subroutine slot picker from inside editor) */
static lv_obj_t   *ui_prgm_exec_screen        = NULL;
static lv_obj_t   *prgm_sub_tab_labels_exec[3]; /* CTL, I/O, EXEC */
static lv_obj_t   *prgm_exec_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_exec_scroll_ind[2];
static uint8_t     prgm_exec_cursor           = 0;
static uint8_t     prgm_exec_scroll           = 0;

/* PRGM runtime Menu( screen — shown during program execution */
static lv_obj_t   *ui_prgm_menu_screen         = NULL;
static lv_obj_t   *prgm_menu_title_lbl          = NULL;
static lv_obj_t   *prgm_menu_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_menu_scroll_ind[2];

static uint8_t     prgm_io_cursor            = 0;
static lv_obj_t   *prgm_io_labels[PRGM_IO_ITEM_COUNT];

/* PRGM executor state — defined in prgm_exec.c */

/* PRGM menu / editor static data */
static const char * const prgm_tab_names[PRGM_TAB_COUNT] = {"EXEC", "EDIT", "ERASE"};
/* CTL items: display name | text to insert into program line */
static const char * const prgm_ctl_display[PRGM_CTL_ITEM_COUNT] = {
    "1:Lbl ",   "2:Goto ",  "3:If ",   "4:IS>(",
    "5:DS<(",   "6:Pause",  "7:End",   "8:Stop",
};
static const char * const prgm_ctl_insert[PRGM_CTL_ITEM_COUNT] = {
    "Lbl ",     "Goto ",    "If ",     "IS>(",
    "DS<(",     "Pause",    "End",     "Stop",
};
static const char * const prgm_io_display[PRGM_IO_ITEM_COUNT] = {
    "1:Disp ",  "2:Input ", "3:DispHome", "4:DispGraph", "5:ClrHome",
};
static const char * const prgm_io_insert[PRGM_IO_ITEM_COUNT] = {
    "Disp ",    "Input ",   "DispHome",   "DispGraph",   "ClrHome",
};

/*===========================================================================
 * PRGM — program editor, sub-menus, and menu
 *==========================================================================*/

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

/* Creates the PRGM line editor, CTL sub-menu, and I/O sub-menu screens. */
static void ui_init_prgm_editor_screen(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* --- Program line editor --- */
    ui_prgm_editor_screen = screen_create(scr);

    prgm_edit_title_lbl = lv_label_create(ui_prgm_editor_screen);
    lv_obj_set_pos(prgm_edit_title_lbl, 4, 4);
    lv_obj_set_style_text_font(prgm_edit_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(prgm_edit_title_lbl, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(prgm_edit_title_lbl, "PRGM");

    for (int i = 0; i < PRGM_EDITOR_VISIBLE; i++) {
        prgm_edit_line_labels[i] = lv_label_create(ui_prgm_editor_screen);
        lv_obj_set_pos(prgm_edit_line_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_edit_line_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_edit_line_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_edit_line_labels[i], "");
    }

    /* Editor lines are ":<content>" with ':' at X=4 (glyph 0).
     * Indicators sit at X=4 with opaque bg to replace the colon visually. */
    prgm_edit_scroll_down = lv_label_create(ui_prgm_editor_screen);
    lv_obj_set_pos(prgm_edit_scroll_down, 4,
                   30 + (PRGM_EDITOR_VISIBLE - 1) * 30);
    lv_obj_set_style_text_font(prgm_edit_scroll_down, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(prgm_edit_scroll_down, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_color(prgm_edit_scroll_down, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(prgm_edit_scroll_down, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(prgm_edit_scroll_down, 0, 0);
    lv_label_set_text(prgm_edit_scroll_down, "\xE2\x86\x93");
    lv_obj_add_flag(prgm_edit_scroll_down, LV_OBJ_FLAG_HIDDEN);

    prgm_edit_scroll_up = lv_label_create(ui_prgm_editor_screen);
    lv_obj_set_pos(prgm_edit_scroll_up, 4, 30);
    lv_obj_set_style_text_font(prgm_edit_scroll_up, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(prgm_edit_scroll_up, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_color(prgm_edit_scroll_up, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(prgm_edit_scroll_up, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(prgm_edit_scroll_up, 0, 0);
    lv_label_set_text(prgm_edit_scroll_up, "\xE2\x86\x91");
    lv_obj_add_flag(prgm_edit_scroll_up, LV_OBJ_FLAG_HIDDEN);

    cursor_box_create(ui_prgm_editor_screen, true,
                      &prgm_edit_cursor_box, &prgm_edit_cursor_inner);

    /* --- CTL sub-menu --- */
    ui_prgm_ctl_screen = screen_create(scr);

    {
        static const char * const sub_names[3] = {"CTL", "I/O", "EXEC"};
        static const int sub_x[3] = {4, 80, 156};
        for (int i = 0; i < 3; i++) {
            prgm_sub_tab_labels_ctl[i] = lv_label_create(ui_prgm_ctl_screen);
            lv_obj_set_pos(prgm_sub_tab_labels_ctl[i], sub_x[i], 4);
            lv_obj_set_style_text_font(prgm_sub_tab_labels_ctl[i], &jetbrains_mono_24, 0);
            lv_obj_set_style_text_color(prgm_sub_tab_labels_ctl[i],
                lv_color_hex(i == 0 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
            lv_label_set_text(prgm_sub_tab_labels_ctl[i], sub_names[i]);
        }
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_ctl_labels[i] = lv_label_create(ui_prgm_ctl_screen);
        lv_obj_set_pos(prgm_ctl_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_ctl_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_ctl_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_ctl_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        prgm_ctl_scroll_ind[i] = lv_label_create(ui_prgm_ctl_screen);
        lv_obj_set_pos(prgm_ctl_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(prgm_ctl_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_ctl_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(prgm_ctl_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(prgm_ctl_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(prgm_ctl_scroll_ind[i], 0, 0);
        lv_label_set_text(prgm_ctl_scroll_ind[i], i == 0 ? "\xE2\x86\x91" : "\xE2\x86\x93");
        lv_obj_add_flag(prgm_ctl_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* --- I/O sub-menu --- */
    ui_prgm_io_screen = screen_create(scr);

    {
        static const char * const sub_names[3] = {"CTL", "I/O", "EXEC"};
        static const int sub_x[3] = {4, 80, 156};
        for (int i = 0; i < 3; i++) {
            prgm_sub_tab_labels_io[i] = lv_label_create(ui_prgm_io_screen);
            lv_obj_set_pos(prgm_sub_tab_labels_io[i], sub_x[i], 4);
            lv_obj_set_style_text_font(prgm_sub_tab_labels_io[i], &jetbrains_mono_24, 0);
            lv_obj_set_style_text_color(prgm_sub_tab_labels_io[i],
                lv_color_hex(i == 1 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
            lv_label_set_text(prgm_sub_tab_labels_io[i], sub_names[i]);
        }
    }

    for (int i = 0; i < PRGM_IO_ITEM_COUNT; i++) {
        prgm_io_labels[i] = lv_label_create(ui_prgm_io_screen);
        lv_obj_set_pos(prgm_io_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_io_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_io_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_io_labels[i], "");
    }

    /* --- EXEC sub-menu (subroutine slot picker) --- */
    ui_prgm_exec_screen = screen_create(scr);

    {
        static const char * const sub_names[3] = {"CTL", "I/O", "EXEC"};
        static const int sub_x[3] = {4, 80, 156};
        for (int i = 0; i < 3; i++) {
            prgm_sub_tab_labels_exec[i] = lv_label_create(ui_prgm_exec_screen);
            lv_obj_set_pos(prgm_sub_tab_labels_exec[i], sub_x[i], 4);
            lv_obj_set_style_text_font(prgm_sub_tab_labels_exec[i], &jetbrains_mono_24, 0);
            lv_obj_set_style_text_color(prgm_sub_tab_labels_exec[i],
                lv_color_hex(i == 2 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
            lv_label_set_text(prgm_sub_tab_labels_exec[i], sub_names[i]);
        }
    }

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        prgm_exec_labels[i] = lv_label_create(ui_prgm_exec_screen);
        lv_obj_set_pos(prgm_exec_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_exec_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_exec_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_exec_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        prgm_exec_scroll_ind[i] = lv_label_create(ui_prgm_exec_screen);
        lv_obj_set_pos(prgm_exec_scroll_ind[i], 4, 30 + row * 30);
        lv_obj_set_style_text_font(prgm_exec_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_exec_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(prgm_exec_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(prgm_exec_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(prgm_exec_scroll_ind[i], 0, 0);
        lv_label_set_text(prgm_exec_scroll_ind[i], i == 0 ? "\xE2\x86\x91" : "\xE2\x86\x93");
        lv_obj_add_flag(prgm_exec_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

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
    return g_prgm_store.names[slot][0] != '\0';
}

/* Updates PRGM menu labels and tab highlights.  Must be called under lvgl_lock. */
static void ui_update_prgm_display(void)
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
        const char *cname = g_prgm_store.names[prgm_erase_confirm_slot];
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
            const char *name = g_prgm_store.names[slot];
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

/* Parses program body into prgm_edit_lines working buffer. */
void prgm_parse_from_store(uint8_t idx)
{
    prgm_edit_num_lines = 0;
    memset(prgm_edit_lines, 0, sizeof(prgm_edit_lines));
    const char *body = g_prgm_store.bodies[idx];
    if (body[0] == '\0') {
        prgm_edit_num_lines = 1;
        return;
    }
    const char *p = body;
    while (*p && prgm_edit_num_lines < PRGM_MAX_LINES) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= PRGM_MAX_LINE_LEN) len = PRGM_MAX_LINE_LEN - 1;
        memcpy(prgm_edit_lines[prgm_edit_num_lines], p, len);
        prgm_edit_lines[prgm_edit_num_lines][len] = '\0';
        prgm_edit_num_lines++;
        if (!nl) break;
        p = nl + 1;
    }
    if (prgm_edit_num_lines == 0)
        prgm_edit_num_lines = 1;
}

/* Reassembles g_prgm_store body from prgm_edit_lines. */
static void prgm_flatten_to_store(void)
{
    char *body = g_prgm_store.bodies[prgm_edit_idx];
    size_t off = 0;
    for (int i = 0; i < (int)prgm_edit_num_lines; i++) {
        size_t len = strlen(prgm_edit_lines[i]);
        if (off + len + 2 >= PRGM_BODY_LEN) break;
        memcpy(body + off, prgm_edit_lines[i], len);
        off += len;
        if (i < (int)prgm_edit_num_lines - 1)
            body[off++] = '\n';
    }
    body[off] = '\0';
}

/* Positions the new-name cursor box without updating the label text. */
void prgm_new_cursor_update(void)
{
    if (prgm_new_cursor_box == NULL || prgm_new_title_lbl == NULL) return;
    cursor_render(prgm_new_cursor_box, prgm_new_cursor_inner,
                  prgm_new_title_lbl, (uint32_t)(6 + prgm_new_name_cursor),
                  cursor_visible, current_mode, false);
}

/* Positions the editor cursor box on the current line. */
void prgm_editor_cursor_update(void)
{
    if (prgm_edit_cursor_box == NULL) return;
    int vis = (int)prgm_edit_line - (int)prgm_edit_scroll;
    if (vis < 0 || vis >= PRGM_EDITOR_VISIBLE) {
        lv_obj_add_flag(prgm_edit_cursor_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_t *lbl = prgm_edit_line_labels[vis];
    /* +1 for the ":" prefix rendered in the label */
    cursor_render(prgm_edit_cursor_box, prgm_edit_cursor_inner,
                  lbl, (uint32_t)(prgm_edit_col + 1),
                  cursor_visible, current_mode, insert_mode);
}

/* Updates all PRGM editor line labels, scroll indicators, and cursor.
 * Must be called under lvgl_lock. */
static void ui_update_prgm_editor_display(void)
{
    char id[3];
    prgm_slot_id_str(prgm_edit_idx, id);
    char title[20]; /* "Prgm" + id(2) + "  " + name(8) + NUL = 17 max */
    const char *ename = g_prgm_store.names[prgm_edit_idx];
    if (ename[0] != '\0')
        snprintf(title, sizeof(title), "Prgm%s  %s", id, ename);
    else
        snprintf(title, sizeof(title), "Prgm%s", id);
    lv_label_set_text(prgm_edit_title_lbl, title);

    for (int i = 0; i < PRGM_EDITOR_VISIBLE; i++) {
        int line = (int)prgm_edit_scroll + i;
        if (line < (int)prgm_edit_num_lines) {
            char buf[PRGM_MAX_LINE_LEN + 2];
            snprintf(buf, sizeof(buf), ":%s", prgm_edit_lines[line]);
            lv_label_set_text(prgm_edit_line_labels[i], buf);
            lv_obj_set_style_text_color(prgm_edit_line_labels[i],
                lv_color_hex(line == (int)prgm_edit_line ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_edit_line_labels[i], "");
        }
    }

    /* Scroll indicators */
    if (prgm_edit_scroll > 0)
        lv_obj_clear_flag(prgm_edit_scroll_up, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_edit_scroll_up, LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_edit_scroll + PRGM_EDITOR_VISIBLE) < (int)prgm_edit_num_lines)
        lv_obj_clear_flag(prgm_edit_scroll_down, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_edit_scroll_down, LV_OBJ_FLAG_HIDDEN);

    prgm_editor_cursor_update();
}

/* Updates CTL sub-menu labels and cursor highlight. */
static void ui_update_prgm_ctl_display(void)
{
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)prgm_ctl_scroll + i;
        if (idx < PRGM_CTL_ITEM_COUNT) {
            lv_label_set_text(prgm_ctl_labels[i], prgm_ctl_display[idx]);
            lv_obj_set_style_text_color(prgm_ctl_labels[i],
                lv_color_hex(i == (int)prgm_ctl_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_ctl_labels[i], "");
        }
    }
    if (prgm_ctl_scroll > 0)
        lv_obj_clear_flag(prgm_ctl_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_ctl_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_ctl_scroll + MENU_VISIBLE_ROWS) < PRGM_CTL_ITEM_COUNT)
        lv_obj_clear_flag(prgm_ctl_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_ctl_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

/* Updates I/O sub-menu labels and cursor highlight. */
static void ui_update_prgm_io_display(void)
{
    for (int i = 0; i < PRGM_IO_ITEM_COUNT; i++) {
        lv_label_set_text(prgm_io_labels[i], prgm_io_display[i]);
        lv_obj_set_style_text_color(prgm_io_labels[i],
            lv_color_hex(i == (int)prgm_io_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
    }
}

/* Updates EXEC sub-menu (slot picker) labels, cursor highlight, and scroll indicators.
 * Must be called under lvgl_lock. */
static void ui_update_prgm_exec_display(void)
{
    char buf[24];
    char id[3];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int slot = (int)prgm_exec_scroll + i;
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_slot_id_str((uint8_t)slot, id);
            const char *name = g_prgm_store.names[slot];
            if (name[0] != '\0')
                snprintf(buf, sizeof(buf), "%s:Prgm%s  %s", id, id, name);
            else
                snprintf(buf, sizeof(buf), "%s:Prgm%s", id, id);
            lv_label_set_text(prgm_exec_labels[i], buf);
            lv_obj_set_style_text_color(prgm_exec_labels[i],
                lv_color_hex(i == (int)prgm_exec_cursor ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(prgm_exec_labels[i], "");
        }
    }
    if (prgm_exec_scroll > 0)
        lv_obj_clear_flag(prgm_exec_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_exec_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    if ((int)(prgm_exec_scroll + MENU_VISIBLE_ROWS) < PRGM_MAX_PROGRAMS)
        lv_obj_clear_flag(prgm_exec_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(prgm_exec_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
}

/* Updates the new-program name-entry label and cursor. */
static void ui_update_prgm_new_display(void)
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
                  cursor_visible, current_mode, false);
}

/* Adjusts editor scroll to keep prgm_edit_line visible. */
static void prgm_editor_scroll_to_line(void)
{
    if ((int)prgm_edit_line < (int)prgm_edit_scroll)
        prgm_edit_scroll = prgm_edit_line;
    else if ((int)prgm_edit_line >= (int)prgm_edit_scroll + PRGM_EDITOR_VISIBLE)
        prgm_edit_scroll = (uint8_t)(prgm_edit_line - PRGM_EDITOR_VISIBLE + 1);
}

/* Inserts string at current cursor position in the current editor line. */
static void prgm_editor_insert_str(const char *s)
{
    if (!s || !*s) return;
    char *line = prgm_edit_lines[prgm_edit_line];
    uint8_t len = (uint8_t)strlen(line);
    uint8_t slen = (uint8_t)strlen(s);
    if ((int)len + (int)slen >= PRGM_MAX_LINE_LEN) return;
    memmove(line + prgm_edit_col + slen,
            line + prgm_edit_col,
            len - prgm_edit_col + 1);
    memcpy(line + prgm_edit_col, s, slen);
    prgm_edit_col = (uint8_t)(prgm_edit_col + slen);
}

/* F4: Insert string into the editor on behalf of a MATH/TEST menu selection.
 * Called from math_menu_insert / test_menu_insert when return_mode == PRGM_EDITOR.
 * Restores the editor screen and sets current_mode back to MODE_PRGM_EDITOR. */
void prgm_editor_menu_insert(const char *s)
{
    prgm_editor_insert_str(s);
    prgm_flatten_to_store();
    lvgl_lock();
    lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_prgm_editor_display();
    lvgl_unlock();
    current_mode = MODE_PRGM_EDITOR;
}

/* Maps a token to its text representation for program editing.
 * Returns NULL for tokens that are not valid in the program editor. */
static const char *prgm_token_to_str(Token_t t)
{
    switch (t) {
    case TOKEN_ADD:     return "+";
    case TOKEN_SUB:     return "-";
    case TOKEN_MULT:    return "*";
    case TOKEN_DIV:     return "/";
    case TOKEN_POWER:   return "^";
    case TOKEN_L_PAR:   return "(";
    case TOKEN_R_PAR:   return ")";
    case TOKEN_DECIMAL: return ".";
    case TOKEN_NEG:     return "(-";
    case TOKEN_STO:     return "->";
    case TOKEN_SPACE:   return " ";
    case TOKEN_QUOTES:  return "\"";
    case TOKEN_COMMA:   return ",";
    case TOKEN_QSTN_M:  return "?";
    case TOKEN_ANS:     return "ANS";
    case TOKEN_SIN:     return "sin(";
    case TOKEN_COS:     return "cos(";
    case TOKEN_TAN:     return "tan(";
    case TOKEN_ASIN:    return "sin\xEE\x80\x81(";   /* sin⁻¹( */
    case TOKEN_ACOS:    return "cos\xEE\x80\x81(";   /* cos⁻¹( */
    case TOKEN_ATAN:    return "tan\xEE\x80\x81(";   /* tan⁻¹( */
    case TOKEN_ABS:     return "abs(";
    case TOKEN_LN:      return "ln(";
    case TOKEN_LOG:     return "log(";
    case TOKEN_SQRT:    return "sqrt(";
    case TOKEN_EE:      return "*10^";
    case TOKEN_E_X:     return "exp(";
    case TOKEN_TEN_X:   return "10^(";
    case TOKEN_SQUARE:  return "^2";
    case TOKEN_X_INV:   return "^-1";
    case TOKEN_MTRX_A:  return "[A]";
    case TOKEN_MTRX_B:  return "[B]";
    case TOKEN_MTRX_C:  return "[C]";
    case TOKEN_X_T:     return "X";
    case TOKEN_PI:      return "pi";
    default:            return NULL;
    }
}

/* Returns true if the current editor line already has a Lbl/Goto label char (B4). */
static bool prgm_label_full(void)
{
    const char *line = prgm_edit_lines[prgm_edit_line];
    return (strncmp(line, "Lbl ",  4) == 0 && strlen(line) > 4) ||
           (strncmp(line, "Goto ", 5) == 0 && strlen(line) > 5);
}

/* Opens the program editor for the given program index. */
static void prgm_open_editor(uint8_t idx)
{
    prgm_edit_idx        = idx;
    prgm_edit_line       = 0;
    prgm_edit_scroll     = 0;
    prgm_edit_col        = 0;
    prgm_parse_from_store(idx);
    insert_mode  = false;  /* D1: editor always opens in overwrite mode */
    current_mode = MODE_PRGM_EDITOR;
    lvgl_lock();
    lv_obj_add_flag(ui_prgm_screen,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_prgm_editor_display();
    lvgl_unlock();
}

/* prgm_skip_to_target, prgm_execute_line, prgm_run_loop, prgm_run_start,
 * handle_prgm_running, and prgm_reset_execution_state have been moved to
 * prgm_exec.c.  Their public declarations remain in prgm_exec.h / ui_prgm.h. */

/*---------------------------------------------------------------------------
 * PRGM token handlers
 *---------------------------------------------------------------------------*/

/* Helper: return the total number of items in the current prgm_tab view. */
static int prgm_menu_total(void)
{
    return PRGM_MAX_PROGRAMS;  /* all tabs show all 37 slots */
}

bool handle_prgm_menu(Token_t t)
{
    /* --- Erase confirmation dialog intercepts most keys --- */
    if (prgm_erase_confirm) {
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
            memset(g_prgm_store.names[prgm_erase_confirm_slot],  0, PRGM_NAME_LEN + 1);
            memset(g_prgm_store.bodies[prgm_erase_confirm_slot], 0, PRGM_BODY_LEN);
            prgm_erase_confirm = false;
            lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
            return true;
        case TOKEN_ENTER:
            if (prgm_erase_confirm_choice == 1) {
                memset(g_prgm_store.names[prgm_erase_confirm_slot],  0, PRGM_NAME_LEN + 1);
                memset(g_prgm_store.bodies[prgm_erase_confirm_slot], 0, PRGM_BODY_LEN);
            }
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
        if (prgm_tab == 0) {
            /* EXEC (C1): insert prgmNAME into calculator expression */
            if (abs_pos < PRGM_MAX_PROGRAMS) {
                char slot_id[3];
                prgm_slot_id_str((uint8_t)abs_pos, slot_id);
                const char *uname = g_prgm_store.names[abs_pos];
                snprintf(expression, MAX_EXPR_LEN, "prgm%s",
                         uname[0] != '\0' ? uname : slot_id);
                expr_len   = (uint8_t)strlen(expression);
                cursor_pos = expr_len;
                CalcMode_t exec_ret = prgm_return_mode;
                prgm_return_mode   = MODE_NORMAL;
                prgm_tab           = 0;
                prgm_item_cursor   = 0;
                prgm_scroll_offset = 0;
                current_mode = exec_ret;
                lvgl_lock();
                lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
                lvgl_unlock();
                Update_Calculator_Display();
            }
        } else if (prgm_tab == 1) {
            /* EDIT: slot index == abs_pos directly */
            if (abs_pos < PRGM_MAX_PROGRAMS) {
                bool has_name = prgm_slot_is_used((uint8_t)abs_pos);
                bool has_body = (g_prgm_store.bodies[abs_pos][0] != '\0');
                if (has_name || has_body) {
                    /* D3: body-only slot opens editor directly (no name-entry) */
                    prgm_editor_from_new = false;
                    prgm_open_editor((uint8_t)abs_pos);
                } else {
                    /* Empty slot — show name-entry screen; auto-engage ALPHA */
                    prgm_new_slot        = (uint8_t)abs_pos;
                    prgm_new_name_len    = 0;
                    prgm_new_name_cursor = 0;
                    memset(prgm_new_name, 0, sizeof(prgm_new_name));
                    prgm_editor_from_new = false;
                    lvgl_lock();
                    lv_obj_add_flag(ui_prgm_screen,       LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
                    ui_update_prgm_new_display();
                    lvgl_unlock();
                    current_mode = MODE_ALPHA;
                    return_mode  = MODE_PRGM_NEW_NAME;
                }
            }
        } else {
            /* ERASE: show confirmation for selected slot (A1: all 37 slots) */
            if (abs_pos < PRGM_MAX_PROGRAMS) {
                prgm_erase_confirm        = true;
                prgm_erase_confirm_slot   = (uint8_t)abs_pos;
                prgm_erase_confirm_choice = 0;
                lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
            }
        }
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
        current_mode = ret;
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
        current_mode = ret;
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
        return_mode  = MODE_PRGM_NEW_NAME;
        current_mode = MODE_ALPHA;
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
        return_mode  = MODE_PRGM_NEW_NAME;
        current_mode = MODE_ALPHA;
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
        return_mode  = MODE_PRGM_NEW_NAME;
        current_mode = MODE_ALPHA;
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
            memcpy(g_prgm_store.names[prgm_new_slot], prgm_new_name, prgm_new_name_len + 1);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        prgm_editor_from_new = true;
        prgm_open_editor(prgm_new_slot);
        return true;
    case TOKEN_ENTER:
        /* Save user name if typed; open editor regardless (name is optional) */
        if (prgm_new_name_len > 0)
            memcpy(g_prgm_store.names[prgm_new_slot], prgm_new_name, prgm_new_name_len + 1);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        prgm_editor_from_new = true;
        prgm_open_editor(prgm_new_slot);
        return true;
    case TOKEN_CLEAR:
    case TOKEN_PRGM:
        /* Cancel — return to PRGM menu */
        current_mode = MODE_PRGM_MENU;
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
    char *line = prgm_edit_lines[prgm_edit_line];

    switch (t) {
    case TOKEN_UP:
        if (prgm_edit_line > 0) {
            prgm_edit_line--;
            prgm_edit_col = 0;
            prgm_editor_scroll_to_line();
            lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        } else if (prgm_edit_col == 0 && prgm_editor_from_new) {
            /* F10: navigate back up to the name-entry title */
            prgm_flatten_to_store();
            prgm_new_name_cursor = prgm_new_name_len; /* cursor at end of name */
            current_mode = MODE_ALPHA;
            return_mode  = MODE_PRGM_NEW_NAME;
            lvgl_lock();
            lv_obj_add_flag(ui_prgm_editor_screen,  LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_prgm_new_screen,   LV_OBJ_FLAG_HIDDEN);
            ui_update_prgm_new_display();
            lvgl_unlock();
        }
        return true;
    case TOKEN_DOWN:
    case TOKEN_ENTER:
        if (prgm_edit_line + 1 < prgm_edit_num_lines) {
            prgm_edit_line++;
        } else if (prgm_edit_num_lines < PRGM_MAX_LINES) {
            /* Add new empty line */
            prgm_edit_lines[prgm_edit_num_lines][0] = '\0';
            prgm_edit_num_lines++;
            prgm_edit_line++;
            prgm_flatten_to_store();
        }
        prgm_edit_col = 0;
        prgm_editor_scroll_to_line();
        lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        if (prgm_edit_col > 0) {
            prgm_edit_col--;
            lvgl_lock(); prgm_editor_cursor_update(); lvgl_unlock();
        }
        return true;
    case TOKEN_RIGHT: {
        uint8_t len = (uint8_t)strlen(line);
        if (prgm_edit_col < len) {
            prgm_edit_col++;
            lvgl_lock(); prgm_editor_cursor_update(); lvgl_unlock();
        }
        return true;
    }
    case TOKEN_DEL: {
        uint8_t len = (uint8_t)strlen(line);
        if (prgm_edit_col > 0) {
            /* Delete char before cursor */
            memmove(line + prgm_edit_col - 1,
                    line + prgm_edit_col,
                    len - prgm_edit_col + 1);
            prgm_edit_col--;
            prgm_flatten_to_store();
            lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        } else if (len == 0 && prgm_edit_num_lines > 1) {
            /* Delete empty line — merge with previous */
            for (int i = (int)prgm_edit_line; i < (int)prgm_edit_num_lines - 1; i++)
                memcpy(prgm_edit_lines[i], prgm_edit_lines[i + 1], PRGM_MAX_LINE_LEN);
            prgm_edit_num_lines--;
            if (prgm_edit_line > 0) prgm_edit_line--;
            prgm_edit_col = (uint8_t)strlen(prgm_edit_lines[prgm_edit_line]);
            prgm_editor_scroll_to_line();
            prgm_flatten_to_store();
            lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        }
        return true;
    }
    case TOKEN_CLEAR:
        if (strlen(line) > 0) {
            /* Clear current line */
            line[0] = '\0';
            prgm_edit_col = 0;
            prgm_flatten_to_store();
            lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        } else if (prgm_edit_num_lines == 1) {
            /* Empty program — return to PRGM menu */
            current_mode = MODE_PRGM_MENU;
            prgm_tab = 1;  /* return to EDIT tab */
            prgm_item_cursor = 0;
            prgm_scroll_offset = 0;
            lvgl_lock();
            lv_obj_add_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_prgm_screen,      LV_OBJ_FLAG_HIDDEN);
            ui_update_prgm_display();
            lvgl_unlock();
        }
        return true;
    case TOKEN_INS:
        insert_mode = !insert_mode;
        lvgl_lock(); prgm_editor_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_TEST:
        /* F4: open TEST menu; selections will insert into editor via
         * test_menu_insert() checking return_mode == MODE_PRGM_EDITOR */
        menu_open(TOKEN_TEST, MODE_PRGM_EDITOR);
        return true;

    case TOKEN_MATH:
        /* F4: open MATH menu; selections will insert into editor via
         * math_menu_insert() checking return_mode == MODE_PRGM_EDITOR */
        menu_open(TOKEN_MATH, MODE_PRGM_EDITOR);
        return true;

    case TOKEN_PRGM:
        /* Open CTL sub-menu */
        prgm_ctl_cursor = 0;
        prgm_ctl_scroll = 0;
        current_mode = MODE_PRGM_CTL_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_ctl_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_ctl_display();
        lvgl_unlock();
        return true;

    case TOKEN_0 ... TOKEN_9: {
        if (prgm_label_full()) return true;  /* B4: single-char label constraint */
        char c[2] = {(char)('0' + (t - TOKEN_0)), '\0'};
        prgm_editor_insert_str(c);
        prgm_flatten_to_store();
        lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        return true;
    }
    case TOKEN_A ... TOKEN_Z: {
        if (prgm_label_full()) return true;  /* B4: single-char label constraint */
        char c[2] = {(char)('A' + (t - TOKEN_A)), '\0'};
        prgm_editor_insert_str(c);
        prgm_flatten_to_store();
        lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        return true;
    }
    default: {
        /* Try generic token-to-string mapping */
        const char *s = prgm_token_to_str(t);
        if (s) {
            prgm_editor_insert_str(s);
            prgm_flatten_to_store();
            lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        }
        return true;
    }
    }
}

bool handle_prgm_ctl_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_ctl_cursor > 0)
            prgm_ctl_cursor--;
        else if (prgm_ctl_scroll > 0)
            prgm_ctl_scroll--;
        lvgl_lock(); ui_update_prgm_ctl_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(prgm_ctl_scroll + prgm_ctl_cursor) + 1 < PRGM_CTL_ITEM_COUNT) {
            if (prgm_ctl_cursor < MENU_VISIBLE_ROWS - 1)
                prgm_ctl_cursor++;
            else if ((int)(prgm_ctl_scroll + MENU_VISIBLE_ROWS) < PRGM_CTL_ITEM_COUNT)
                prgm_ctl_scroll++;
        }
        lvgl_lock(); ui_update_prgm_ctl_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)prgm_ctl_scroll + (int)prgm_ctl_cursor;
        if (idx < PRGM_CTL_ITEM_COUNT) {
            prgm_editor_insert_str(prgm_ctl_insert[idx]);
            prgm_flatten_to_store();
        }
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_ctl_screen,       LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_1 ... TOKEN_9: {
        int idx = (int)(t - TOKEN_1);
        if (idx < PRGM_CTL_ITEM_COUNT) {
            prgm_editor_insert_str(prgm_ctl_insert[idx]);
            prgm_flatten_to_store();
        }
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_ctl_screen,       LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_CLEAR:
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_ctl_screen,      LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        /* CTL RIGHT → I/O */
        prgm_io_cursor = 0;
        current_mode = MODE_PRGM_IO_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_ctl_screen,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_io_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_io_display();
        lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        /* CTL LEFT → EXEC (wrap) */
        prgm_exec_cursor = 0;
        prgm_exec_scroll = 0;
        current_mode = MODE_PRGM_EXEC_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_ctl_screen,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_exec_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_exec_display();
        lvgl_unlock();
        return true;
    default:
        return true;
    }
}

bool handle_prgm_io_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_io_cursor > 0) prgm_io_cursor--;
        lvgl_lock(); ui_update_prgm_io_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (prgm_io_cursor < PRGM_IO_ITEM_COUNT - 1) prgm_io_cursor++;
        lvgl_lock(); ui_update_prgm_io_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)prgm_io_cursor;
        prgm_editor_insert_str(prgm_io_insert[idx]);
        prgm_flatten_to_store();
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_io_screen,       LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_1 ... TOKEN_5: {
        int idx = (int)(t - TOKEN_1);
        if (idx < PRGM_IO_ITEM_COUNT) {
            prgm_editor_insert_str(prgm_io_insert[idx]);
            prgm_flatten_to_store();
        }
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_io_screen,       LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_CLEAR:
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_io_screen,       LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        /* I/O LEFT → CTL */
        prgm_ctl_cursor = 0;
        prgm_ctl_scroll = 0;
        current_mode = MODE_PRGM_CTL_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_io_screen,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_ctl_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_ctl_display();
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        /* I/O RIGHT → EXEC (wrap) */
        prgm_exec_cursor = 0;
        prgm_exec_scroll = 0;
        current_mode = MODE_PRGM_EXEC_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_io_screen,      LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_exec_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_exec_display();
        lvgl_unlock();
        return true;
    default:
        return true;
    }
}

bool handle_prgm_exec_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (prgm_exec_cursor > 0)
            prgm_exec_cursor--;
        else if (prgm_exec_scroll > 0)
            prgm_exec_scroll--;
        lvgl_lock(); ui_update_prgm_exec_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(prgm_exec_scroll + prgm_exec_cursor) + 1 < PRGM_MAX_PROGRAMS) {
            if (prgm_exec_cursor < MENU_VISIBLE_ROWS - 1)
                prgm_exec_cursor++;
            else if ((int)(prgm_exec_scroll + MENU_VISIBLE_ROWS) < PRGM_MAX_PROGRAMS)
                prgm_exec_scroll++;
        }
        lvgl_lock(); ui_update_prgm_exec_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int slot = (int)prgm_exec_scroll + (int)prgm_exec_cursor;
        if (slot < PRGM_MAX_PROGRAMS) {
            char slot_id[3];
            prgm_slot_id_str((uint8_t)slot, slot_id);
            const char *uname = g_prgm_store.names[slot];
            char ins[PRGM_NAME_LEN + 6]; /* "prgm" + name/id + NUL */
            snprintf(ins, sizeof(ins), "prgm%s", uname[0] != '\0' ? uname : slot_id);
            prgm_editor_insert_str(ins);
            prgm_flatten_to_store();
        }
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_exec_screen,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    }
    case TOKEN_1 ... TOKEN_9: {
        int slot = (int)(t - TOKEN_1);
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
                ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
            prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
            return handle_prgm_exec_menu(TOKEN_ENTER);
        }
        return true;
    }
    case TOKEN_0: {
        int slot = 9;
        prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
            ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
        prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
        return handle_prgm_exec_menu(TOKEN_ENTER);
    }
    case TOKEN_A ... TOKEN_Z: {
        int slot = 10 + (int)(t - TOKEN_A);
        if (slot < PRGM_MAX_PROGRAMS) {
            prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
                ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
            prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
            return handle_prgm_exec_menu(TOKEN_ENTER);
        }
        return true;
    }
    case TOKEN_THETA: {
        int slot = 36;
        prgm_exec_scroll = (slot >= MENU_VISIBLE_ROWS)
            ? (uint8_t)(slot - MENU_VISIBLE_ROWS + 1) : 0;
        prgm_exec_cursor = (uint8_t)(slot - (int)prgm_exec_scroll);
        return handle_prgm_exec_menu(TOKEN_ENTER);
    }
    case TOKEN_CLEAR:
        current_mode = MODE_PRGM_EDITOR;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_exec_screen,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_editor_display();
        lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        /* EXEC LEFT → I/O */
        prgm_io_cursor = 0;
        current_mode = MODE_PRGM_IO_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_exec_screen,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_io_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_io_display();
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        /* EXEC RIGHT → CTL (wrap) */
        prgm_ctl_cursor = 0;
        prgm_ctl_scroll = 0;
        current_mode = MODE_PRGM_CTL_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_exec_screen,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_ctl_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_ctl_display();
        lvgl_unlock();
        return true;
    default:
        return true;
    }
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

void ui_init_prgm_screens(void)
{
    ui_init_prgm_screen();
    ui_init_prgm_new_screen();
    ui_init_prgm_menu_screen();
    ui_init_prgm_editor_screen();
}

/* prgm_reset_execution_state is defined in prgm_exec.c */

void hide_prgm_screens(void) {
    if (ui_prgm_screen) lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_new_screen) lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_editor_screen) lv_obj_add_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_ctl_screen) lv_obj_add_flag(ui_prgm_ctl_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_io_screen) lv_obj_add_flag(ui_prgm_io_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_exec_screen) lv_obj_add_flag(ui_prgm_exec_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_menu_screen) lv_obj_add_flag(ui_prgm_menu_screen, LV_OBJ_FLAG_HIDDEN);
}

void prgm_reset_state(CalcMode_t target_mode) {
    if (target_mode == MODE_PRGM_MENU) {
        prgm_return_mode = current_mode;
        prgm_tab = 0;
        prgm_item_cursor = 0;
        prgm_scroll_offset = 0;
        current_mode = MODE_PRGM_MENU;
        lv_obj_clear_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_display();
    } else if (target_mode == MODE_PRGM_RUNNING) {
        prgm_return_mode = current_mode;
        prgm_tab = 0;
        prgm_item_cursor = 0;
        prgm_scroll_offset = 0;
        current_mode = MODE_PRGM_RUNNING;
        prgm_run_start(prgm_edit_idx);
    }
}

void prgm_menu_open(CalcMode_t return_to) {
    prgm_return_mode = return_to;
    prgm_tab = 0;
    prgm_item_cursor = 0;
    prgm_scroll_offset = 0;
    current_mode = MODE_PRGM_MENU;
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
