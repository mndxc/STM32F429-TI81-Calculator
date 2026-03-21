/**
 * @file ui_prgm.c
 * @brief Program (PRGM) Menu and Editor UI Module Implementation
 *
 * *************************************************************************
 * WARNING: PARTIALLY COMPLETED MODULE
 * *************************************************************************
 * This module was extracted from `calculator_core.c` to adhere to the UI
 * Extensibility Pattern. Currently, ONLY the UI rendering and tab navigation
 * (EXEC, EDIT, NEW, CTL, I/O) are fully implemented.
 *
 * SIGNIFICANT BACKEND WORK REMAINS:
 *   - The PRGM execution engine must be properly bridged to calc_engine.c.
 *   - Tokenization logic inside the editor is incomplete.
 *   - Program storage (prgm_flatten_to_store) and memory management must be finalized.
 *   - I/O handling and control flow loops (If, While, For) are non-functional.
 *   - Do not assume execution functions actually process tokens yet!
 * *************************************************************************
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
#define PRGM_CTL_ITEM_COUNT    12   /* CTL sub-menu items */
#define PRGM_IO_ITEM_COUNT      4   /* I/O sub-menu items */
#define PRGM_EDITOR_VISIBLE     7   /* Visible editor rows (matches MENU_VISIBLE_ROWS) */
#define PRGM_MAX_LINES         64   /* Max lines in one program */
#define PRGM_MAX_LINE_LEN      48   /* Max chars per line (incl null) */

/* PRGM executor limits */
#define PRGM_CTRL_DEPTH         8   /* Max nested If/While/For depth */
#define PRGM_CALL_DEPTH         4   /* Max subroutine call depth */

typedef enum { CF_IF = 0, CF_WHILE, CF_FOR } CtrlType_t;

typedef struct {
    CtrlType_t  type;
    uint16_t    origin_line;   /* While: condition line; For: For( line */
    float       for_limit;
    float       for_step;
    char        for_var;
} CtrlFrame_t;

typedef struct {
    uint8_t  idx;        /* caller program index in g_prgm_store */
    uint16_t pc;         /* return address (line after the prgm call) */
    uint8_t  num_lines;  /* caller's total line count */
} CallFrame_t;
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
static uint8_t     prgm_new_slot              = 0;   /* slot index being created */
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
static uint8_t     prgm_edit_num_lines       = 0;   /* total lines in active program */
static lv_obj_t   *prgm_edit_title_lbl       = NULL;
static lv_obj_t   *prgm_edit_line_labels[PRGM_EDITOR_VISIBLE];
static lv_obj_t   *prgm_edit_scroll_up       = NULL;
static lv_obj_t   *prgm_edit_scroll_down     = NULL;
static lv_obj_t   *prgm_edit_cursor_box      = NULL;
static lv_obj_t   *prgm_edit_cursor_inner    = NULL;
/* Working line buffer for active program — plain .bss */
static char        prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];

/* PRGM CTL sub-menu state */
static lv_obj_t   *ui_prgm_ctl_screen        = NULL;
static uint8_t     prgm_ctl_cursor           = 0;
static uint8_t     prgm_ctl_scroll           = 0;
static lv_obj_t   *prgm_ctl_labels[MENU_VISIBLE_ROWS];
static lv_obj_t   *prgm_ctl_scroll_ind[2];

/* PRGM I/O sub-menu state */
static lv_obj_t   *ui_prgm_io_screen        = NULL;
static lv_obj_t *prgm_sub_tab_labels_ctl[2];
static lv_obj_t *prgm_sub_tab_labels_io[2];

static uint8_t     prgm_io_cursor            = 0;
static lv_obj_t   *prgm_io_labels[PRGM_IO_ITEM_COUNT];

/* PRGM executor state */
static CtrlFrame_t prgm_ctrl_stack[PRGM_CTRL_DEPTH];
static uint8_t     prgm_ctrl_top      = 0;
static CallFrame_t prgm_call_stack[PRGM_CALL_DEPTH];
static uint8_t     prgm_call_top      = 0;
static uint8_t     prgm_run_idx       = 0;   /* program index being executed */
static uint16_t    prgm_run_pc        = 0;   /* current line 0-based */
static uint8_t     prgm_run_num_lines = 0;   /* total lines in running program */
static bool        prgm_run_active    = false;
static bool        prgm_waiting_input = false; /* true when paused at Pause/Input/Prompt */
static char        prgm_input_var     = 0;    /* 'A'–'Z' for Input/Prompt, 0 for Pause */

/* PRGM menu / editor static data */
static const char * const prgm_tab_names[PRGM_TAB_COUNT] = {"EXEC", "EDIT", "ERASE"};
/* CTL items: display name | text to insert into program line */
static const char * const prgm_ctl_display[PRGM_CTL_ITEM_COUNT] = {
    "1:If ",    "2:Then",   "3:Else",   "4:End",
    "5:While ", "6:For(",   "7:Goto ",  "8:Lbl ",
    "9:Pause",  "10:Stop",  "11:Return","12:prgm",
};
static const char * const prgm_ctl_insert[PRGM_CTL_ITEM_COUNT] = {
    "If ",    "Then",   "Else",   "End",
    "While ", "For(",   "Goto ",  "Lbl ",
    "Pause",  "Stop",   "Return", "prgm",
};
static const char * const prgm_io_display[PRGM_IO_ITEM_COUNT] = {
    "1:Input ", "2:Prompt ", "3:Disp ", "4:ClrHome",
};
static const char * const prgm_io_insert[PRGM_IO_ITEM_COUNT] = {
    "Input ", "Prompt ", "Disp ", "ClrHome",
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

    for (int i = 0; i < 2; i++) {
        prgm_sub_tab_labels_ctl[i] = lv_label_create(ui_prgm_ctl_screen);
        lv_obj_set_pos(prgm_sub_tab_labels_ctl[i], i == 0 ? 4 : 80, 4);
        lv_obj_set_style_text_font(prgm_sub_tab_labels_ctl[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_sub_tab_labels_ctl[i], lv_color_hex(i == 0 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(prgm_sub_tab_labels_ctl[i], i == 0 ? "CTL" : "I/O");
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

    for (int i = 0; i < 2; i++) {
        prgm_sub_tab_labels_io[i] = lv_label_create(ui_prgm_io_screen);
        lv_obj_set_pos(prgm_sub_tab_labels_io[i], i == 0 ? 4 : 80, 4);
        lv_obj_set_style_text_font(prgm_sub_tab_labels_io[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_sub_tab_labels_io[i], lv_color_hex(i == 1 ? COLOR_YELLOW : COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(prgm_sub_tab_labels_io[i], i == 0 ? "CTL" : "I/O");
    }

    for (int i = 0; i < PRGM_IO_ITEM_COUNT; i++) {
        prgm_io_labels[i] = lv_label_create(ui_prgm_io_screen);
        lv_obj_set_pos(prgm_io_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(prgm_io_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(prgm_io_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(prgm_io_labels[i], "");
    }
}

/* Returns the display identifier string for a program slot (0-based index).
 * out must have room for 3 bytes.  Mapping: 0-8→'1'-'9', 9→'0', 10-35→'A'-'Z', 36→θ. */
static void prgm_slot_id_str(uint8_t slot, char *out)
{
    if (slot <= 8)       { out[0] = (char)('1' + slot); out[1] = '\0'; }
    else if (slot == 9)  { out[0] = '0';                out[1] = '\0'; }
    else if (slot <= 35) { out[0] = (char)('A' + (slot - 10)); out[1] = '\0'; }
    else                 { out[0] = '\xCE'; out[1] = '\xB8'; out[2] = '\0'; } /* θ U+03B8 */
}

/* Returns true if the slot has a program (name is non-empty). */
static bool prgm_slot_is_used(uint8_t slot)
{
    return g_prgm_store.names[slot][0] != '\0';
}

/* Fills out[] with occupied slot indices; returns count. */
static uint8_t prgm_build_occupied(uint8_t *out)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PRGM_MAX_PROGRAMS; i++) {
        if (prgm_slot_is_used(i)) out[n++] = i;
    }
    return n;
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

    int total;
    if (prgm_tab == 2) {
        /* ERASE — occupied slots only */
        uint8_t occupied[PRGM_MAX_PROGRAMS];
        uint8_t occ_count = prgm_build_occupied(occupied);
        total = (int)occ_count;
        for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
            int vi = (int)prgm_scroll_offset + i;
            if (vi < total) {
                uint8_t slot = occupied[vi];
                prgm_slot_id_str(slot, id);
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
    } else {
        /* EXEC (tab 0) and EDIT (tab 1) — all 37 slots */
        total = PRGM_MAX_PROGRAMS;
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
static void prgm_parse_from_store(uint8_t idx)
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
    cursor_place(prgm_new_cursor_box, prgm_new_cursor_inner,
                 prgm_new_title_lbl, (uint32_t)(6 + prgm_new_name_len));
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
    cursor_place(prgm_edit_cursor_box, prgm_edit_cursor_inner,
                 lbl, (uint32_t)(prgm_edit_col + 1));
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
    cursor_place(prgm_new_cursor_box, prgm_new_cursor_inner,
                 prgm_new_title_lbl, (uint32_t)(6 + prgm_new_name_len));
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
    case TOKEN_ASIN:    return "asin(";
    case TOKEN_ACOS:    return "acos(";
    case TOKEN_ATAN:    return "atan(";
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

/* Opens the program editor for the given program index. */
static void prgm_open_editor(uint8_t idx)
{
    prgm_edit_idx        = idx;
    prgm_edit_line       = 0;
    prgm_edit_scroll     = 0;
    prgm_edit_col        = 0;
    prgm_parse_from_store(idx);
    current_mode = MODE_PRGM_EDITOR;
    lvgl_lock();
    lv_obj_add_flag(ui_prgm_screen,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_prgm_editor_display();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * PRGM executor
 *---------------------------------------------------------------------------*/

/**
 * @brief Skip forward to the line after the matching Else (if want_else) or End.
 *
 * Call with prgm_run_pc pointing to the first line to scan (not already past
 * it).  On return prgm_run_pc is the first line to execute after the target.
 * Counts nested block-openers so that nested If/While/For structures are
 * handled correctly.
 */
static void prgm_skip_to_target(bool want_else)
{
    int depth = 0;
    while (prgm_run_pc < (uint16_t)prgm_run_num_lines) {
        const char *ln = prgm_edit_lines[prgm_run_pc];
        if (strncmp(ln, "If ", 3) == 0) {
            /* Only a block-If (next line is Then) increments depth */
            uint16_t nxt = prgm_run_pc + 1;
            if (nxt < (uint16_t)prgm_run_num_lines &&
                strncmp(prgm_edit_lines[nxt], "Then", 4) == 0)
                depth++;
        } else if (strncmp(ln, "While ", 6) == 0 || strncmp(ln, "For(", 4) == 0) {
            depth++;
        } else if (strcmp(ln, "Else") == 0 && depth == 0 && want_else) {
            prgm_run_pc++; /* execute from line after Else */
            return;
        } else if (strcmp(ln, "End") == 0) {
            if (depth == 0) {
                prgm_run_pc++; /* continue from line after End */
                return;
            }
            depth--;
        }
        prgm_run_pc++;
    }
    /* No matching End found — fall off end of program */
}
/** Execute the program line at index @p ln. prgm_run_pc is already ln+1. */
static void prgm_execute_line(uint16_t ln)
{
    const char *line = prgm_edit_lines[ln];

    /* --- Lbl (no-op during sequential execution) ------------------------- */
    if (strncmp(line, "Lbl ", 4) == 0) return;

    /* --- Goto ------------------------------------------------------------- */
    if (strncmp(line, "Goto ", 5) == 0) {
        const char *lbl = line + 5;
        for (uint16_t i = 0; i < (uint16_t)prgm_run_num_lines; i++) {
            if (strncmp(prgm_edit_lines[i], "Lbl ", 4) == 0 &&
                strcmp(prgm_edit_lines[i] + 4, lbl) == 0) {
                prgm_run_pc = i + 1; /* execute line after Lbl */
                return;
            }
        }
        prgm_run_active = false; /* label not found */
        return;
    }

    /* --- If --------------------------------------------------------------- */
    if (strncmp(line, "If ", 3) == 0) {
        CalcResult_t r = Calc_Evaluate(line + 3, ans, ans_is_matrix, angle_degrees);
        bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
        if (!cond) {
            if (prgm_run_pc < (uint16_t)prgm_run_num_lines &&
                strncmp(prgm_edit_lines[prgm_run_pc], "Then", 4) == 0) {
                /* Block If: skip past Then, find Else or End */
                prgm_run_pc++;            /* skip Then */
                prgm_skip_to_target(true);
            } else {
                prgm_run_pc++;            /* single-line If: skip one statement */
            }
        }
        return;
    }

    /* --- Then ------------------------------------------------------------- */
    if (strcmp(line, "Then") == 0) {
        /* Push CF_IF so End knows it belongs to an If block */
        if (prgm_ctrl_top < PRGM_CTRL_DEPTH) {
            prgm_ctrl_stack[prgm_ctrl_top].type        = CF_IF;
            prgm_ctrl_stack[prgm_ctrl_top].origin_line = ln;
            prgm_ctrl_top++;
        }
        return;
    }

    /* --- Else ------------------------------------------------------------- */
    if (strcmp(line, "Else") == 0) {
        /* We are in the true block — pop CF_IF and skip to matching End */
        if (prgm_ctrl_top > 0 && prgm_ctrl_stack[prgm_ctrl_top - 1].type == CF_IF)
            prgm_ctrl_top--;
        /* prgm_run_pc == ln+1: scan from next line to End (no Else wanted) */
        prgm_skip_to_target(false);
        return;
    }

    /* --- While ------------------------------------------------------------ */
    if (strncmp(line, "While ", 6) == 0) {
        CalcResult_t r = Calc_Evaluate(line + 6, ans, ans_is_matrix, angle_degrees);
        bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
        if (cond) {
            if (prgm_ctrl_top < PRGM_CTRL_DEPTH) {
                prgm_ctrl_stack[prgm_ctrl_top].type        = CF_WHILE;
                prgm_ctrl_stack[prgm_ctrl_top].origin_line = ln;
                prgm_ctrl_top++;
            }
        } else {
            prgm_skip_to_target(false);
        }
        return;
    }

    /* --- For( ------------------------------------------------------------- */
    if (strncmp(line, "For(", 4) == 0) {
        const char *args = line + 4;
        /* Variable must be a single letter followed by comma */
        if (args[0] < 'A' || args[0] > 'Z' || args[1] != ',') return;
        char var = args[0];
        const char *rest = args + 2;

        /* Split rest at depth-0 commas → up to 3 args (begin, end, step) */
        char parts[3][MAX_EXPR_LEN];
        int n = 0, depth = 0, j = 0;
        for (const char *p = rest; *p && n < 3; p++) {
            if (*p == '(' || *p == '[')       depth++;
            else if (*p == ')') { if (depth == 0) break; depth--; }
            else if (*p == ']') { if (depth > 0)  depth--; }
            else if (*p == ',' && depth == 0) {
                parts[n][j] = '\0'; n++; j = 0; continue;
            }
            if (j < MAX_EXPR_LEN - 1) parts[n][j++] = *p;
        }
        if (j > 0) { parts[n][j] = '\0'; n++; }
        if (n < 2) return;

        CalcResult_t rb = Calc_Evaluate(parts[0], ans, ans_is_matrix, angle_degrees);
        CalcResult_t re = Calc_Evaluate(parts[1], ans, ans_is_matrix, angle_degrees);
        if (rb.error != CALC_OK || rb.has_matrix) return;
        if (re.error != CALC_OK || re.has_matrix) return;
        float begin_v = rb.value, end_v = re.value, step_v = 1.0f;
        if (n >= 3) {
            CalcResult_t rs = Calc_Evaluate(parts[2], ans, ans_is_matrix, angle_degrees);
            if (rs.error == CALC_OK && !rs.has_matrix) step_v = rs.value;
        }
        if (step_v == 0.0f) return; /* infinite loop guard */

        calc_variables[var - 'A'] = begin_v;
        bool run = (step_v > 0.0f) ? (begin_v <= end_v) : (begin_v >= end_v);
        if (run) {
            if (prgm_ctrl_top < PRGM_CTRL_DEPTH) {
                prgm_ctrl_stack[prgm_ctrl_top].type        = CF_FOR;
                prgm_ctrl_stack[prgm_ctrl_top].origin_line = ln;
                prgm_ctrl_stack[prgm_ctrl_top].for_limit   = end_v;
                prgm_ctrl_stack[prgm_ctrl_top].for_step    = step_v;
                prgm_ctrl_stack[prgm_ctrl_top].for_var     = var;
                prgm_ctrl_top++;
            }
        } else {
            prgm_skip_to_target(false);
        }
        return;
    }

    /* --- End -------------------------------------------------------------- */
    if (strcmp(line, "End") == 0) {
        if (prgm_ctrl_top == 0) return; /* unmatched End */
        CtrlFrame_t *frame = &prgm_ctrl_stack[prgm_ctrl_top - 1];
        if (frame->type == CF_IF) {
            prgm_ctrl_top--;
        } else if (frame->type == CF_WHILE) {
            /* Re-evaluate While condition */
            const char *wcond = prgm_edit_lines[frame->origin_line] + 6;
            CalcResult_t r = Calc_Evaluate(wcond, ans, ans_is_matrix, angle_degrees);
            bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
            if (cond) {
                prgm_run_pc = frame->origin_line + 1; /* re-execute body */
            } else {
                prgm_ctrl_top--;
            }
        } else if (frame->type == CF_FOR) {
            char fvar = frame->for_var;
            calc_variables[fvar - 'A'] += frame->for_step;
            float val = calc_variables[fvar - 'A'];
            bool still = (frame->for_step > 0.0f) ? (val <= frame->for_limit)
                                                   : (val >= frame->for_limit);
            if (still) {
                prgm_run_pc = frame->origin_line + 1; /* first body line */
            } else {
                prgm_ctrl_top--;
            }
        }
        return;
    }

    /* --- Pause ------------------------------------------------------------ */
    if (strcmp(line, "Pause") == 0) {
        prgm_waiting_input = true;
        prgm_input_var     = 0;
        return;
    }

    /* --- Stop ------------------------------------------------------------- */
    if (strcmp(line, "Stop") == 0) {
        prgm_run_active = false;
        return;
    }

    /* --- Return ----------------------------------------------------------- */
    if (strcmp(line, "Return") == 0) {
        if (prgm_call_top > 0) {
            prgm_call_top--;
            prgm_run_idx       = prgm_call_stack[prgm_call_top].idx;
            prgm_run_pc        = prgm_call_stack[prgm_call_top].pc;
            prgm_run_num_lines = prgm_call_stack[prgm_call_top].num_lines;
            prgm_parse_from_store(prgm_run_idx);
        } else {
            prgm_run_active = false;
        }
        return;
    }

    /* --- prgm (subroutine call) ------------------------------------------ */
    if (strncmp(line, "prgm", 4) == 0) {
        /* Look up by slot identifier (e.g. "prgmA" → slot 10) */
        const char *id_str = line + 4;
        for (uint8_t i = 0; i < PRGM_MAX_PROGRAMS; i++) {
            if (!prgm_slot_is_used(i)) continue;
            char slot_id[3];
            prgm_slot_id_str(i, slot_id);
            if (strcmp(slot_id, id_str) == 0) {
                if (prgm_call_top < PRGM_CALL_DEPTH) {
                    prgm_call_stack[prgm_call_top].idx       = prgm_run_idx;
                    prgm_call_stack[prgm_call_top].pc        = prgm_run_pc;
                    prgm_call_stack[prgm_call_top].num_lines = prgm_run_num_lines;
                    prgm_call_top++;
                    prgm_run_idx       = i;
                    prgm_parse_from_store(i);
                    prgm_run_num_lines = prgm_edit_num_lines;
                    prgm_run_pc        = 0;
                }
                return;
            }
        }
        return; /* program not found — continue */
    }

    /* --- ClrHome ---------------------------------------------------------- */
    if (strcmp(line, "ClrHome") == 0) {
        history_count         = 0;
        history_recall_offset = 0;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        return;
    }

    /* --- Disp ------------------------------------------------------------- */
    if (strncmp(line, "Disp ", 5) == 0) {
        const char *arg = line + 5;
        char disp_buf[MAX_RESULT_LEN];
        if (*arg == '"') {
            /* String literal */
            const char *s   = arg + 1;
            const char *end = strchr(s, '"');
            size_t len = end ? (size_t)(end - s) : strlen(s);
            if (len >= (size_t)(MAX_RESULT_LEN - 1)) len = (size_t)(MAX_RESULT_LEN - 2);
            strncpy(disp_buf, s, len);
            disp_buf[len] = '\0';
        } else {
            CalcResult_t r = Calc_Evaluate(arg, ans, ans_is_matrix, angle_degrees);
            format_calc_result(&r, disp_buf, MAX_RESULT_LEN, &ans);
            if (r.error == CALC_OK && !r.has_matrix) {
                ans = r.value; ans_is_matrix = false;
            } else if (r.error == CALC_OK && r.has_matrix) {
                ans = (float)r.matrix_idx; ans_is_matrix = true;
            }
        }
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        history[hidx].expression[0] = '\0';
        strncpy(history[hidx].result, disp_buf, MAX_RESULT_LEN - 1);
        history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
        history_count++;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        return;
    }

    /* --- Input ------------------------------------------------------------ */
    if (strncmp(line, "Input ", 6) == 0) {
        const char *arg = line + 6;
        char var = (*arg >= 'A' && *arg <= 'Z') ? *arg : 0;
        prgm_input_var = var;
        char prompt[12];
        snprintf(prompt, sizeof(prompt), var ? "%c=?" : "?", var);
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, prompt, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        history[hidx].result[0] = '\0';
        history_count++;
        expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
        prgm_waiting_input = true;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        Update_Calculator_Display();
        return;
    }

    /* --- Prompt ----------------------------------------------------------- */
    if (strncmp(line, "Prompt ", 7) == 0) {
        const char *arg = line + 7;
        char var = (*arg >= 'A' && *arg <= 'Z') ? *arg : 0;
        prgm_input_var = var;
        char prompt[12];
        snprintf(prompt, sizeof(prompt), var ? "%c=?" : "?", var);
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, prompt, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        history[hidx].result[0] = '\0';
        history_count++;
        expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
        prgm_waiting_input = true;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        Update_Calculator_Display();
        return;
    }

    /* --- STO (expr->VAR) -------------------------------------------------- */
    const char *sto_arrow = strstr(line, "->");
    if (sto_arrow) {
        size_t llen = (size_t)(sto_arrow - line);
        if (llen > 0 && llen < (size_t)(MAX_EXPR_LEN - 1)) {
            char left[MAX_EXPR_LEN];
            strncpy(left, line, llen);
            left[llen] = '\0';
            const char *varname = sto_arrow + 2;
            if (*varname >= 'A' && *varname <= 'Z') {
                CalcResult_t r = Calc_Evaluate(left, ans, ans_is_matrix, angle_degrees);
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[*varname - 'A'] = r.value;
                    ans = r.value;
                    ans_is_matrix = false;
                }
            }
        }
        return;
    }

    /* --- General expression line ----------------------------------------- */
    {
        CalcResult_t r = Calc_Evaluate(line, ans, ans_is_matrix, angle_degrees);
        if (r.error == CALC_OK && !r.has_matrix) {
            ans = r.value;
            ans_is_matrix = false;
        } else if (r.error == CALC_OK && r.has_matrix) {
            ans = (float)r.matrix_idx;
            ans_is_matrix = true;
        }
    }
}

/** Main synchronous execution loop.  Runs lines from prgm_run_pc until a
 *  pause point or end of program.  Re-entered via handle_prgm_running on
 *  ENTER after Pause/Input/Prompt. */
static void prgm_run_loop(void)
{
    prgm_run_active = true;

restart:
    while (prgm_run_pc < (uint16_t)prgm_run_num_lines
           && prgm_run_active && !prgm_waiting_input) {
        uint16_t ln = prgm_run_pc++;
        prgm_execute_line(ln);
    }

    if (!prgm_run_active || prgm_waiting_input)
        return;

    /* End of lines — implicit return from subroutine if call stack not empty */
    if (prgm_call_top > 0) {
        prgm_call_top--;
        prgm_run_idx       = prgm_call_stack[prgm_call_top].idx;
        prgm_run_pc        = prgm_call_stack[prgm_call_top].pc;
        prgm_run_num_lines = prgm_call_stack[prgm_call_top].num_lines;
        prgm_parse_from_store(prgm_run_idx);
        goto restart;
    }

    /* Program done */
    prgm_run_active = false;
    current_mode    = MODE_NORMAL;
    lvgl_lock();
    ui_refresh_display();
    lvgl_unlock();
}

/** Initialise executor state and start running program @p idx. */
void prgm_run_start(uint8_t idx)
{
    prgm_run_idx       = idx;
    prgm_run_pc        = 0;
    prgm_ctrl_top      = 0;
    prgm_call_top      = 0;
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_input_var     = 0;
    expression[0]      = '\0';
    expr_len           = 0;
    cursor_pos         = 0;
    prgm_parse_from_store(idx);
    prgm_run_num_lines = prgm_edit_num_lines;
    current_mode       = MODE_PRGM_RUNNING;
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
    prgm_run_loop();
}

/**
 * @brief Token handler for MODE_PRGM_RUNNING.
 *
 * Intercepts tokens while a program is executing (waiting at Pause/Input/
 * Prompt).  CLEAR aborts; ENTER resumes; other tokens feed the expression
 * input buffer while waiting for Input/Prompt.
 */
bool handle_prgm_running(Token_t t)
{
    if (prgm_waiting_input) {
        if (t == TOKEN_ENTER) {
            if (prgm_input_var != 0) {
                /* Evaluate and store to the target variable */
                CalcResult_t r = Calc_Evaluate(expression, ans, ans_is_matrix,
                                               angle_degrees);
                char res_buf[MAX_RESULT_LEN];
                format_calc_result(&r, res_buf, MAX_RESULT_LEN, &ans);
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[prgm_input_var - 'A'] = r.value;
                    ans           = r.value;
                    ans_is_matrix = false;
                }
                /* Append expression + result to history */
                uint8_t hidx = history_count % HISTORY_LINE_COUNT;
                strncpy(history[hidx].expression, expression, MAX_EXPR_LEN - 1);
                history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
                strncpy(history[hidx].result, res_buf, MAX_RESULT_LEN - 1);
                history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
                history_count++;
            }
            expression[0]     = '\0';
            expr_len          = 0;
            cursor_pos        = 0;
            prgm_waiting_input = false;
            prgm_input_var    = 0;
            lvgl_lock(); ui_update_history(); lvgl_unlock();
            prgm_run_loop();  /* resume execution */
            return true;
        }
        if (t == TOKEN_DEL) {
            expr_delete_at_cursor();
            Update_Calculator_Display();
            return true;
        }
        if (t == TOKEN_CLEAR) {
            if (expr_len > 0) {
                expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
                Update_Calculator_Display();
            } else {
                /* Abort on CLEAR with empty expression */
                prgm_run_active    = false;
                prgm_waiting_input = false;
                prgm_ctrl_top      = 0;
                prgm_call_top      = 0;
                current_mode       = MODE_NORMAL;
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
            CalcMode_t saved = current_mode;
            current_mode = MODE_NORMAL;
            handle_normal_mode(t);
            current_mode = saved;
            return true;
        }
        }
    }

    /* Not waiting for input — abort on CLEAR, consume everything else */
    if (t == TOKEN_CLEAR) {
        prgm_run_active    = false;
        prgm_waiting_input = false;
        prgm_ctrl_top      = 0;
        prgm_call_top      = 0;
        expression[0]      = '\0';
        expr_len           = 0;
        cursor_pos         = 0;
        current_mode       = MODE_NORMAL;
        lvgl_lock(); ui_refresh_display(); lvgl_unlock();
        return true;
    }
    return true; /* consume all tokens while running */
}

/*---------------------------------------------------------------------------
 * PRGM token handlers
 *---------------------------------------------------------------------------*/

/* Helper: return the total number of items in the current prgm_tab view. */
static int prgm_menu_total(void)
{
    if (prgm_tab == 2) {   /* ERASE — occupied slots only */
        uint8_t occ[PRGM_MAX_PROGRAMS];
        return (int)prgm_build_occupied(occ);
    }
    return PRGM_MAX_PROGRAMS;  /* EXEC and EDIT — all 37 slots */
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
            prgm_erase_confirm_choice = 0;
            /* fall through to ENTER */
            /* FALLTHROUGH */
        case TOKEN_ENTER:
            if (prgm_erase_confirm_choice == 1) {
                /* Erase: zero out the slot */
                memset(g_prgm_store.names[prgm_erase_confirm_slot],  0, PRGM_NAME_LEN + 1);
                memset(g_prgm_store.bodies[prgm_erase_confirm_slot], 0, PRGM_BODY_LEN);
                if (prgm_item_cursor > 0) prgm_item_cursor--;
                if (prgm_scroll_offset > 0) {
                    uint8_t occ[PRGM_MAX_PROGRAMS];
                    int remaining = (int)prgm_build_occupied(occ);
                    if ((int)(prgm_scroll_offset + prgm_item_cursor) >= remaining)
                        prgm_scroll_offset = 0;
                }
            }
            prgm_erase_confirm = false;
            lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
            return true;
        case TOKEN_2:
            prgm_erase_confirm_choice = 1;
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
        tab_move(&prgm_tab, &prgm_item_cursor, &prgm_scroll_offset,
                 PRGM_TAB_COUNT, true, ui_update_prgm_display);
        return true;
    case TOKEN_RIGHT:
        prgm_erase_confirm = false;
        tab_move(&prgm_tab, &prgm_item_cursor, &prgm_scroll_offset,
                 PRGM_TAB_COUNT, false, ui_update_prgm_display);
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
            /* EXEC: run the selected slot by index directly */
            if (abs_pos < PRGM_MAX_PROGRAMS) {
                lvgl_lock();
                lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
                lvgl_unlock();
                prgm_run_start((uint8_t)abs_pos);
            }
        } else if (prgm_tab == 1) {
            /* EDIT: slot index == abs_pos directly */
            if (abs_pos < PRGM_MAX_PROGRAMS) {
                if (prgm_slot_is_used((uint8_t)abs_pos)) {
                    prgm_open_editor((uint8_t)abs_pos);
                } else {
                    /* Empty slot — show name-entry screen; auto-engage ALPHA */
                    prgm_new_slot     = (uint8_t)abs_pos;
                    prgm_new_name_len = 0;
                    memset(prgm_new_name, 0, sizeof(prgm_new_name));
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
            /* ERASE: show confirmation for selected occupied slot */
            uint8_t occupied[PRGM_MAX_PROGRAMS];
            uint8_t occ_count = prgm_build_occupied(occupied);
            if (abs_pos < (int)occ_count) {
                prgm_erase_confirm        = true;
                prgm_erase_confirm_slot   = occupied[abs_pos];
                prgm_erase_confirm_choice = 0;
                lvgl_lock(); ui_update_prgm_display(); lvgl_unlock();
            }
        }
        return true;
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
            prgm_new_name[prgm_new_name_len++] = (char)('A' + (t - TOKEN_A));
            prgm_new_name[prgm_new_name_len]   = '\0';
            lvgl_lock(); ui_update_prgm_new_display(); lvgl_unlock();
        }
        /* Re-engage ALPHA so the next keypress is also a letter */
        return_mode  = MODE_PRGM_NEW_NAME;
        current_mode = MODE_ALPHA;
        return true;
    }
    case TOKEN_0 ... TOKEN_9: {
        if (prgm_new_name_len < PRGM_NAME_LEN) {
            prgm_new_name[prgm_new_name_len++] = (char)('0' + (t - TOKEN_0));
            prgm_new_name[prgm_new_name_len]   = '\0';
            lvgl_lock(); ui_update_prgm_new_display(); lvgl_unlock();
        }
        /* Re-engage ALPHA so the next keypress can still be a letter */
        return_mode  = MODE_PRGM_NEW_NAME;
        current_mode = MODE_ALPHA;
        return true;
    }
    case TOKEN_DEL:
        if (prgm_new_name_len > 0) {
            prgm_new_name[--prgm_new_name_len] = '\0';
            lvgl_lock(); ui_update_prgm_new_display(); lvgl_unlock();
        }
        return true;
    case TOKEN_ENTER:
        /* Save user name if typed; open editor regardless (name is optional) */
        if (prgm_new_name_len > 0)
            memcpy(g_prgm_store.names[prgm_new_slot], prgm_new_name, prgm_new_name_len + 1);
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
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
        char c[2] = {(char)('0' + (t - TOKEN_0)), '\0'};
        prgm_editor_insert_str(c);
        prgm_flatten_to_store();
        lvgl_lock(); ui_update_prgm_editor_display(); lvgl_unlock();
        return true;
    }
    case TOKEN_A ... TOKEN_Z: {
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
    case TOKEN_LEFT:
        prgm_io_cursor = 0;
        current_mode = MODE_PRGM_IO_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_ctl_screen,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_io_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_io_display();
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
    case TOKEN_1 ... TOKEN_4: {
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
    case TOKEN_RIGHT:
        prgm_ctl_cursor = 0;
        prgm_ctl_scroll = 0;
        current_mode = MODE_PRGM_CTL_MENU;
        lvgl_lock();
        lv_obj_add_flag(ui_prgm_io_screen,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_prgm_ctl_screen,  LV_OBJ_FLAG_HIDDEN);
        ui_update_prgm_ctl_display();
        lvgl_unlock();
        return true;
    default:
        return true;
    }
}









void ui_init_prgm_screens(void)
{
    ui_init_prgm_screen();
    ui_init_prgm_new_screen();
    ui_init_prgm_editor_screen();
}

void prgm_reset_execution_state(void)
{
    prgm_run_active = false;
    prgm_waiting_input = false;
    prgm_ctrl_top = 0;
    prgm_call_top = 0;
}

void hide_prgm_screens(void) {
    if (ui_prgm_screen) lv_obj_add_flag(ui_prgm_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_new_screen) lv_obj_add_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_editor_screen) lv_obj_add_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_ctl_screen) lv_obj_add_flag(ui_prgm_ctl_screen, LV_OBJ_FLAG_HIDDEN);
    if (ui_prgm_io_screen) lv_obj_add_flag(ui_prgm_io_screen, LV_OBJ_FLAG_HIDDEN);
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
