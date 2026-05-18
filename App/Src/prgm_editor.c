/**
 * @file    prgm_editor.c
 * @brief   Program line editor — LVGL screen, cursor, and token handler.
 *
 * Extracted from ui_prgm.c.  Owns all editor LVGL objects, the working
 * line buffer, and the editor token handler.  Cross-module coordination
 * (navigate-up-to-new-name, return-to-menu) uses registered callbacks so
 * this module does not include ui_prgm.h, keeping the dependency graph
 * acyclic.
 *
 * Dependency summary:
 *   prgm_editor.c → prgm_editor.h, prgm_exec.h, ui_shared.h, ui_palette.h,
 *                   calculator_core.h, ui_prgm_ctl.h, ui_prgm_io.h,
 *                   ui_prgm_exec.h, ui_prgm_mode.h
 *   ui_prgm.c     → prgm_editor.h   (one-way; no cycle)
 */
#include "prgm_editor.h"
#include "prgm_exec.h"
#include "ui_shared.h"
#include "ui_palette.h"
#include "calculator_core.h"
#include "ui_prgm_ctl.h"
#include "ui_prgm_io.h"
#include "ui_prgm_exec.h"
#include "ui_prgm_mode.h"
#include <stdio.h>
#include <string.h>

/* Geometry constants shared with ui_prgm.c */
#define PRGM_EDITOR_VISIBLE  7   /* visible editor rows */

/*===========================================================================
 * Editor state — all private to this module
 *===========================================================================*/

static lv_obj_t  *s_editor_screen       = NULL;
static lv_obj_t  *s_title_lbl           = NULL;
static lv_obj_t  *s_line_labels[PRGM_EDITOR_VISIBLE];
static lv_obj_t  *s_scroll_up           = NULL;
static lv_obj_t  *s_scroll_down         = NULL;
static lv_obj_t  *s_cursor_box          = NULL;
static lv_obj_t  *s_cursor_inner        = NULL;

static uint8_t    s_edit_idx            = 0;   /* slot being edited */
static uint8_t    s_edit_line           = 0;   /* current line (0-based) */
static uint8_t    s_edit_scroll         = 0;   /* first visible line */
static uint8_t    s_edit_col            = 0;   /* cursor byte-offset in current line */
static uint8_t    s_edit_num_lines      = 0;   /* total lines in active program */
static bool       s_from_new            = false; /* editor opened from new-name screen */

/* Working line buffer — plain .bss */
static char s_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];

/* Cross-module callbacks set by ui_prgm.c */
static void (*s_nav_up_cb)(void)            = NULL;
static void (*s_return_to_menu_cb)(void)    = NULL;

/*===========================================================================
 * Public accessors used by prgm_exec.c (declared in ui_prgm.h for compat)
 *===========================================================================*/

const char *Prgm_GetLine(uint8_t ln)     { return s_edit_lines[ln]; }
uint8_t     Prgm_GetNumLines(void)       { return s_edit_num_lines; }

/* Parses program body from FLASH store into s_edit_lines working buffer. */
void prgm_parse_from_store(uint8_t idx)
{
    s_edit_num_lines = 0;
    memset(s_edit_lines, 0, sizeof(s_edit_lines));
    const char *body = Prgm_GetBody(idx);
    if (body[0] == '\0') {
        s_edit_num_lines = 1;
        return;
    }
    const char *p = body;
    while (*p && s_edit_num_lines < PRGM_MAX_LINES) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= PRGM_MAX_LINE_LEN) len = PRGM_MAX_LINE_LEN - 1;
        memcpy(s_edit_lines[s_edit_num_lines], p, len);
        s_edit_lines[s_edit_num_lines][len] = '\0';
        s_edit_num_lines++;
        if (!nl) break;
        p = nl + 1;
    }
    if (s_edit_num_lines == 0)
        s_edit_num_lines = 1;
}

/*===========================================================================
 * Private helpers
 *===========================================================================*/

/* Reassembles s_edit_lines and writes to the FLASH store. */
static void flatten_to_store_internal(void)
{
    char body[PRGM_BODY_LEN];
    size_t off = 0;
    for (int i = 0; i < (int)s_edit_num_lines; i++) {
        size_t len = strlen(s_edit_lines[i]);
        if (off + len + 2 >= PRGM_BODY_LEN) break;
        memcpy(body + off, s_edit_lines[i], len);
        off += len;
        if (i < (int)s_edit_num_lines - 1)
            body[off++] = '\n';
    }
    body[off] = '\0';
    Prgm_SetBody(s_edit_idx, body);
}

/* Returns true if the current line already has a Lbl/Goto label char (B4). */
static bool prgm_label_full(void)
{
    const char *line = s_edit_lines[s_edit_line];
    return (strncmp(line, "Lbl ",  4) == 0 && strlen(line) > 4) ||
           (strncmp(line, "Goto ", 5) == 0 && strlen(line) > 5);
}

/* Adjusts scroll so s_edit_line is within the visible window. */
static void scroll_to_line(void)
{
    if ((int)s_edit_line < (int)s_edit_scroll)
        s_edit_scroll = s_edit_line;
    else if ((int)s_edit_line >= (int)s_edit_scroll + PRGM_EDITOR_VISIBLE)
        s_edit_scroll = (uint8_t)(s_edit_line - PRGM_EDITOR_VISIBLE + 1);
}

/* prgm_slot_id_str is defined in ui_prgm.c and declared in ui_prgm.h.
 * Forward-declare here to avoid including ui_prgm.h (which would create a
 * mutual-include dependency since ui_prgm.c includes prgm_editor.h). */
void prgm_slot_id_str(uint8_t slot, char *out);

/* Redraws all editor labels, scroll indicators, and cursor.
 * Must be called under lvgl_lock(). */
static void update_display_locked(void)
{
    char id[3];
    prgm_slot_id_str(s_edit_idx, id);
    char title[20];
    const char *ename = Prgm_GetName(s_edit_idx);
    if (ename[0] != '\0')
        snprintf(title, sizeof(title), "Prgm%s  %s", id, ename);
    else
        snprintf(title, sizeof(title), "Prgm%s", id);
    lv_label_set_text(s_title_lbl, title);

    for (int i = 0; i < PRGM_EDITOR_VISIBLE; i++) {
        int line = (int)s_edit_scroll + i;
        if (line < (int)s_edit_num_lines) {
            char buf[PRGM_MAX_LINE_LEN + 2];
            snprintf(buf, sizeof(buf), ":%s", s_edit_lines[line]);
            lv_label_set_text(s_line_labels[i], buf);
            lv_obj_set_style_text_color(s_line_labels[i],
                lv_color_hex(line == (int)s_edit_line ? COLOR_YELLOW : COLOR_WHITE), 0);
        } else {
            lv_label_set_text(s_line_labels[i], "");
        }
    }

    if (s_edit_scroll > 0)
        lv_obj_clear_flag(s_scroll_up, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_scroll_up, LV_OBJ_FLAG_HIDDEN);
    if ((int)(s_edit_scroll + PRGM_EDITOR_VISIBLE) < (int)s_edit_num_lines)
        lv_obj_clear_flag(s_scroll_down, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_scroll_down, LV_OBJ_FLAG_HIDDEN);

    /* Inline cursor update (s_cursor_box != NULL guaranteed after init) */
    int vis = (int)s_edit_line - (int)s_edit_scroll;
    if (vis < 0 || vis >= PRGM_EDITOR_VISIBLE) {
        lv_obj_add_flag(s_cursor_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_t *lbl = s_line_labels[vis];
    /* +1 for the ":" prefix rendered in the label */
    cursor_render(s_cursor_box, s_cursor_inner, lbl,
                  (uint32_t)(s_edit_col + 1),
                  Calc_GetCursorVisible(), Calc_GetMode(), Calc_GetInsertMode());
}

/* Token-to-string lookup table for editor insertions. */
static const struct { Token_t tok; const char *str; } k_prgm_tok_strs[] = {
    { TOKEN_ADD,     "+"                  },
    { TOKEN_SUB,     "-"                  },
    { TOKEN_MULT,    "*"                  },
    { TOKEN_DIV,     "/"                  },
    { TOKEN_POWER,   "^"                  },
    { TOKEN_L_PAR,   "("                  },
    { TOKEN_R_PAR,   ")"                  },
    { TOKEN_DECIMAL, "."                  },
    { TOKEN_NEG,     "(-"                 },
    { TOKEN_STO,     "->"                 },
    { TOKEN_SPACE,   " "                  },
    { TOKEN_QUOTES,  "\""                 },
    { TOKEN_COMMA,   ","                  },
    { TOKEN_QSTN_M,  "?"                  },
    { TOKEN_ANS,     "ANS"                },
    { TOKEN_SIN,     "sin("               },
    { TOKEN_COS,     "cos("               },
    { TOKEN_TAN,     "tan("               },
    { TOKEN_ASIN,    "sin\xEE\x80\x81("  }, /* sin⁻¹( */
    { TOKEN_ACOS,    "cos\xEE\x80\x81("  }, /* cos⁻¹( */
    { TOKEN_ATAN,    "tan\xEE\x80\x81("  }, /* tan⁻¹( */
    { TOKEN_ABS,     "abs("               },
    { TOKEN_LN,      "ln("                },
    { TOKEN_LOG,     "log("               },
    { TOKEN_SQRT,    "sqrt("              },
    { TOKEN_EE,      "*10^"               },
    { TOKEN_E_X,     "exp("               },
    { TOKEN_TEN_X,   "10^("               },
    { TOKEN_SQUARE,  "^2"                 },
    { TOKEN_X_INV,   "\xEE\x80\x81"      }, /* ⁻¹ U+E001 */
    { TOKEN_MTRX_A,  "[A]"                },
    { TOKEN_MTRX_B,  "[B]"                },
    { TOKEN_MTRX_C,  "[C]"                },
    { TOKEN_X_T,     "X"                  },
    { TOKEN_PI,      "pi"                 },
};

static const char *prgm_token_to_str(Token_t t)
{
    for (size_t i = 0; i < sizeof(k_prgm_tok_strs) / sizeof(k_prgm_tok_strs[0]); i++) {
        if (k_prgm_tok_strs[i].tok == t) return k_prgm_tok_strs[i].str;
    }
    return NULL;
}

/*===========================================================================
 * Navigation, deletion, insertion sub-handlers
 *===========================================================================*/

static void editor_handle_nav(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_edit_line > 0) {
            s_edit_line--;
            s_edit_col = 0;
            scroll_to_line();
            lvgl_lock(); update_display_locked(); lvgl_unlock();
        } else if (s_edit_col == 0 && s_from_new) {
            /* F10: navigate back up to the name-entry title */
            flatten_to_store_internal();
            Calc_SetMode(MODE_ALPHA_LOCK);
            Calc_SetReturnMode(MODE_PRGM_NEW_NAME);
            lvgl_lock();
            lv_obj_add_flag(s_editor_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            s_from_new = false;
            if (s_nav_up_cb) s_nav_up_cb();
        }
        break;
    case TOKEN_DOWN:
    case TOKEN_ENTER:
        if (s_edit_line + 1 < s_edit_num_lines) {
            s_edit_line++;
        } else if (s_edit_num_lines < PRGM_MAX_LINES) {
            s_edit_lines[s_edit_num_lines][0] = '\0';
            s_edit_num_lines++;
            s_edit_line++;
            flatten_to_store_internal();
        }
        s_edit_col = 0;
        scroll_to_line();
        lvgl_lock(); update_display_locked(); lvgl_unlock();
        break;
    case TOKEN_LEFT:
        if (s_edit_col > 0) {
            s_edit_col--;
            lvgl_lock(); PrgmEditor_CursorUpdate(); lvgl_unlock();
        }
        break;
    case TOKEN_RIGHT: {
        uint8_t len = (uint8_t)strlen(s_edit_lines[s_edit_line]);
        if (s_edit_col < len) {
            s_edit_col++;
            lvgl_lock(); PrgmEditor_CursorUpdate(); lvgl_unlock();
        }
        break;
    }
    default: break;
    }
}

static void editor_handle_del_clear(Token_t t)
{
    char *line = s_edit_lines[s_edit_line];
    switch (t) {
    case TOKEN_DEL: {
        uint8_t len = (uint8_t)strlen(line);
        if (s_edit_col > 0) {
            memmove(line + s_edit_col - 1,
                    line + s_edit_col,
                    len - s_edit_col + 1);
            s_edit_col--;
            flatten_to_store_internal();
            lvgl_lock(); update_display_locked(); lvgl_unlock();
        } else if (len == 0 && s_edit_num_lines > 1) {
            for (int i = (int)s_edit_line; i < (int)s_edit_num_lines - 1; i++)
                memcpy(s_edit_lines[i], s_edit_lines[i + 1], PRGM_MAX_LINE_LEN);
            s_edit_num_lines--;
            if (s_edit_line > 0) s_edit_line--;
            s_edit_col = (uint8_t)strlen(s_edit_lines[s_edit_line]);
            scroll_to_line();
            flatten_to_store_internal();
            lvgl_lock(); update_display_locked(); lvgl_unlock();
        }
        break;
    }
    case TOKEN_CLEAR:
        if (strlen(line) > 0) {
            line[0] = '\0';
            s_edit_col = 0;
            flatten_to_store_internal();
            lvgl_lock(); update_display_locked(); lvgl_unlock();
        } else if (s_edit_num_lines == 1) {
            Calc_SetMode(MODE_PRGM_MENU);
            lvgl_lock();
            lv_obj_add_flag(s_editor_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            if (s_return_to_menu_cb) s_return_to_menu_cb();
        }
        break;
    default: break;
    }
}

static void editor_handle_insert(Token_t t)
{
    switch (t) {
    case TOKEN_0 ... TOKEN_9: {
        if (prgm_label_full()) return;
        char c[2] = {(char)('0' + (t - TOKEN_0)), '\0'};
        PrgmEditor_InsertStr(c);
        flatten_to_store_internal();
        lvgl_lock(); update_display_locked(); lvgl_unlock();
        break;
    }
    case TOKEN_A ... TOKEN_Z: {
        if (prgm_label_full()) return;
        char c[2] = {(char)('A' + (t - TOKEN_A)), '\0'};
        PrgmEditor_InsertStr(c);
        flatten_to_store_internal();
        lvgl_lock(); update_display_locked(); lvgl_unlock();
        break;
    }
    default: {
        const char *s = prgm_token_to_str(t);
        if (s) {
            PrgmEditor_InsertStr(s);
            flatten_to_store_internal();
            lvgl_lock(); update_display_locked(); lvgl_unlock();
        }
        break;
    }
    }
}

/*===========================================================================
 * Public API
 *===========================================================================*/

void PrgmEditor_InitScreen(lv_obj_t *parent_scr)
{
    s_editor_screen = screen_create(parent_scr);

    s_title_lbl = lv_label_create(s_editor_screen);
    lv_obj_set_pos(s_title_lbl, 4, 4);
    lv_obj_set_style_text_font(s_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(s_title_lbl, "PRGM");

    for (int i = 0; i < PRGM_EDITOR_VISIBLE; i++) {
        s_line_labels[i] = lv_label_create(s_editor_screen);
        lv_obj_set_pos(s_line_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(s_line_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(s_line_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(s_line_labels[i], "");
    }

    /* Editor lines are ":<content>" with ':' at X=4.
     * Indicators sit at X=4 with opaque bg to replace the colon visually. */
    s_scroll_down = lv_label_create(s_editor_screen);
    lv_obj_set_pos(s_scroll_down, 4, 30 + (PRGM_EDITOR_VISIBLE - 1) * 30);
    lv_obj_set_style_text_font(s_scroll_down, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_scroll_down, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_color(s_scroll_down, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(s_scroll_down, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scroll_down, 0, 0);
    lv_label_set_text(s_scroll_down, "\xE2\x86\x93");
    lv_obj_add_flag(s_scroll_down, LV_OBJ_FLAG_HIDDEN);

    s_scroll_up = lv_label_create(s_editor_screen);
    lv_obj_set_pos(s_scroll_up, 4, 30);
    lv_obj_set_style_text_font(s_scroll_up, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(s_scroll_up, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_set_style_bg_color(s_scroll_up, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(s_scroll_up, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scroll_up, 0, 0);
    lv_label_set_text(s_scroll_up, "\xE2\x86\x91");
    lv_obj_add_flag(s_scroll_up, LV_OBJ_FLAG_HIDDEN);

    cursor_box_create(s_editor_screen, true, &s_cursor_box, &s_cursor_inner);
}

void PrgmEditor_Open(uint8_t slot, bool from_new)
{
    s_edit_idx    = slot;
    s_edit_line   = 0;
    s_edit_scroll = 0;
    s_edit_col    = 0;
    s_from_new    = from_new;
    prgm_parse_from_store(slot);
    Calc_SetInsertMode(false);  /* D1: editor always opens in overwrite mode */
    Calc_SetMode(MODE_PRGM_EDITOR);
    lvgl_lock();
    lv_obj_clear_flag(s_editor_screen, LV_OBJ_FLAG_HIDDEN);
    update_display_locked();
    lvgl_unlock();
}

bool PrgmEditor_HandleToken(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
    case TOKEN_DOWN:
    case TOKEN_ENTER:
    case TOKEN_LEFT:
    case TOKEN_RIGHT:
        editor_handle_nav(t);
        return true;
    case TOKEN_DEL:
    case TOKEN_CLEAR:
        editor_handle_del_clear(t);
        return true;
    case TOKEN_INS:
        Calc_SetInsertMode(!Calc_GetInsertMode());
        lvgl_lock(); PrgmEditor_CursorUpdate(); lvgl_unlock();
        return true;
    case TOKEN_TEST:
        menu_open(TOKEN_TEST, MODE_PRGM_EDITOR);
        return true;
    case TOKEN_MATH:
        menu_open(TOKEN_MATH, MODE_PRGM_EDITOR);
        return true;
    case TOKEN_PRGM:
        Calc_SetMode(MODE_PRGM_CTL_MENU);
        lvgl_lock();
        lv_obj_add_flag(s_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_prgm_ctl_reset_and_show();
        lvgl_unlock();
        return true;
    case TOKEN_MODE:
        Calc_SetMode(MODE_PRGM_MODE_NUMBER);
        lvgl_lock();
        lv_obj_add_flag(s_editor_screen, LV_OBJ_FLAG_HIDDEN);
        ui_prgm_mode_num_reset_and_show();
        lvgl_unlock();
        return true;
    case TOKEN_VARS:
        menu_open(TOKEN_VARS, MODE_PRGM_EDITOR);
        return true;
    case TOKEN_MATRX:
        menu_open(TOKEN_MATRX, MODE_PRGM_EDITOR);
        return true;
    case TOKEN_Y_VARS:
        menu_open(TOKEN_Y_VARS, MODE_PRGM_EDITOR);
        return true;
    case TOKEN_STAT:
        menu_open(TOKEN_STAT, MODE_PRGM_EDITOR);
        return true;
    case TOKEN_DRAW:
        menu_open(TOKEN_DRAW, MODE_PRGM_EDITOR);
        return true;
    default:
        editor_handle_insert(t);
        return true;
    }
}

void PrgmEditor_InsertStr(const char *s)
{
    if (!s || !*s) return;
    char *line = s_edit_lines[s_edit_line];
    uint8_t len  = (uint8_t)strlen(line);
    uint8_t slen = (uint8_t)strlen(s);
    if ((int)len + (int)slen >= PRGM_MAX_LINE_LEN) return;
    memmove(line + s_edit_col + slen,
            line + s_edit_col,
            len - s_edit_col + 1);
    memcpy(line + s_edit_col, s, slen);
    s_edit_col = (uint8_t)(s_edit_col + slen);
}

void PrgmEditor_FlattenToStore(void)
{
    flatten_to_store_internal();
}

void PrgmEditor_RefreshDisplay(void)
{
    lvgl_lock();
    update_display_locked();
    lvgl_unlock();
}

void PrgmEditor_CursorUpdate(void)
{
    if (s_cursor_box == NULL) return;
    int vis = (int)s_edit_line - (int)s_edit_scroll;
    if (vis < 0 || vis >= PRGM_EDITOR_VISIBLE) {
        lv_obj_add_flag(s_cursor_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_t *lbl = s_line_labels[vis];
    cursor_render(s_cursor_box, s_cursor_inner, lbl,
                  (uint32_t)(s_edit_col + 1),
                  Calc_GetCursorVisible(), Calc_GetMode(), Calc_GetInsertMode());
}

void PrgmEditor_MenuInsert(const char *s)
{
    PrgmEditor_InsertStr(s);
    flatten_to_store_internal();
    lvgl_lock();
    lv_obj_clear_flag(s_editor_screen, LV_OBJ_FLAG_HIDDEN);
    update_display_locked();
    lvgl_unlock();
    Calc_SetMode(MODE_PRGM_EDITOR);
}

lv_obj_t *PrgmEditor_GetScreen(void)
{
    return s_editor_screen;
}

uint8_t PrgmEditor_GetSlot(void)
{
    return s_edit_idx;
}

void PrgmEditor_SetNavUpCallback(void (*cb)(void))
{
    s_nav_up_cb = cb;
}

void PrgmEditor_SetReturnToMenuCallback(void (*cb)(void))
{
    s_return_to_menu_cb = cb;
}
