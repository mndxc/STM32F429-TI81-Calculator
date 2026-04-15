/**
 * @file    graph_ui_range.c
 * @brief   RANGE field editor and ZOOM FACTORS editor screens.
 *
 * Extracted from graph_ui.c; part of the calculator UI super-module.
 * Zero behavioral changes — purely a file organisation refactor.
 *
 * Both editors share the generic FieldEditor_t / field_editor_handle()
 * infrastructure defined here.
 *
 * All LVGL calls must be made under lvgl_lock()/lvgl_unlock() except from
 * cursor_timer_cb (which runs inside lv_task_handler — mutex already held).
 */

#include "graph_ui_range.h"
#include "calc_internal.h"
#include "graph_ui.h"       /* zoom_menu_reset(), ui_graph_zoom_screen, ui_update_zoom_display() */
#include "ui_palette.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t  field;     /* 0=Xmin 1=Xmax 2=Xscl 3=Ymin 4=Ymax 5=Yscl 6=Xres */
    char     buf[16];   /* In-progress edit buffer */
    uint8_t  len;       /* Current length of buf */
    uint8_t  cursor;    /* Insertion point within buf */
} RangeEditorState_t;

typedef struct {
    float    x_fact, y_fact;    /* XFact / YFact zoom multipliers (persisted) */
    uint8_t  field;             /* 0=XFact, 1=YFact */
    char     buf[16];           /* In-progress edit buffer */
    uint8_t  len;
    uint8_t  cursor;
} ZoomFactorsState_t;

typedef struct {
    char    *buf;
    uint8_t  cap;        /* sizeof(buf) */
    uint8_t *len;
    uint8_t *cursor;
    void   (*update)(void);      /* refresh display labels */
    void   (*cursor_fn)(void);   /* reposition cursor widget */
} FieldEditor_t;

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define RANGE_ROW_COUNT_FUNC  7
#define RANGE_ROW_COUNT_PARAM 9

static const char * const zoom_factors_names[2] = {"XFact=", "YFact="};

static const char * const range_field_names_func[RANGE_ROW_COUNT_FUNC] = {
    "Xmin=", "Xmax=", "Xscl=", "Ymin=", "Ymax=", "Yscl=", "Xres="
};
static const char * const range_field_names_param[RANGE_ROW_COUNT_PARAM] = {
    "Tmin=", "Tmax=", "Tstep=", "Xmin=", "Xmax=", "Xscl=", "Ymin=", "Ymax=", "Yscl="
};
/* Alias pointing to the active name set — updated when param_mode changes */
static const char * const *range_field_names = range_field_names_func;

/*---------------------------------------------------------------------------
 * LVGL object pointers
 *---------------------------------------------------------------------------*/

lv_obj_t *ui_graph_range_screen        = NULL;
lv_obj_t *ui_graph_zoom_factors_screen = NULL;

/* RANGE editor labels and cursor — 9 rows max (7 func, 9 param) */
static lv_obj_t *ui_lbl_range_rows[RANGE_ROW_COUNT_PARAM];
static lv_obj_t *range_cursor_box    = NULL;
static lv_obj_t *range_cursor_inner  = NULL;

/* ZOOM FACTORS labels and cursor */
static lv_obj_t *ui_lbl_zoom_factors_rows[2];
static lv_obj_t *zoom_factors_cursor_box   = NULL;
static lv_obj_t *zoom_factors_cursor_inner = NULL;

/*---------------------------------------------------------------------------
 * State instances
 *---------------------------------------------------------------------------*/

static RangeEditorState_t s_range = {0};
static ZoomFactorsState_t s_zf    = { .x_fact = 4.0f, .y_fact = 4.0f };

/*---------------------------------------------------------------------------
 * Forward declarations
 *---------------------------------------------------------------------------*/

static float range_field_value(uint8_t field);
static void zoom_factors_reset(void);
static void zoom_factors_load_field(void);
static void zoom_factors_update_highlight(void);

/*---------------------------------------------------------------------------
 * Initialisation helpers
 *---------------------------------------------------------------------------*/

static void ui_init_range_screen(lv_obj_t *parent)
{
    ui_graph_range_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_graph_range_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_range_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_range_screen,
                               lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_graph_range_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_range_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_range_title = lv_label_create(ui_graph_range_screen);
    lv_obj_set_pos(lbl_range_title, 4, 4);
    lv_obj_set_style_text_font(lbl_range_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_range_title,
                                 lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(lbl_range_title, "RANGE");

    for (int i = 0; i < RANGE_ROW_COUNT_PARAM; i++) {
        ui_lbl_range_rows[i] = lv_label_create(ui_graph_range_screen);
        lv_obj_set_pos(ui_lbl_range_rows[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(ui_lbl_range_rows[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_range_rows[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        /* Rows 7-8 start off hidden (only visible in parametric mode) */
        if (i >= RANGE_ROW_COUNT_FUNC)
            lv_obj_add_flag(ui_lbl_range_rows[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_lbl_range_rows[i],
                          (i < RANGE_ROW_COUNT_FUNC) ? range_field_names_func[i] : "");
    }

    cursor_box_create(ui_graph_range_screen, true, &range_cursor_box, &range_cursor_inner);
}

static void ui_init_zoom_factors_screen(lv_obj_t *parent)
{
    ui_graph_zoom_factors_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_graph_zoom_factors_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_zoom_factors_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_zoom_factors_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_graph_zoom_factors_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_zoom_factors_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_zf_title = lv_label_create(ui_graph_zoom_factors_screen);
    lv_obj_set_pos(lbl_zf_title, 4, 4);
    lv_obj_set_style_text_font(lbl_zf_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_zf_title, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(lbl_zf_title, "ZOOM FACTORS");

    for (int i = 0; i < 2; i++) {
        ui_lbl_zoom_factors_rows[i] = lv_label_create(ui_graph_zoom_factors_screen);
        lv_obj_set_pos(ui_lbl_zoom_factors_rows[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(ui_lbl_zoom_factors_rows[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_zoom_factors_rows[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_zoom_factors_rows[i], zoom_factors_names[i]);
    }

    cursor_box_create(ui_graph_zoom_factors_screen, true,
                      &zoom_factors_cursor_box, &zoom_factors_cursor_inner);
}

void graph_ui_range_init_screens(lv_obj_t *parent)
{
    ui_init_range_screen(parent);
    ui_init_zoom_factors_screen(parent);
}

/*---------------------------------------------------------------------------
 * RANGE state helpers
 *---------------------------------------------------------------------------*/

static uint8_t range_field_max(void)
{
    return graph_state.param_mode ? (RANGE_ROW_COUNT_PARAM - 1) : (RANGE_ROW_COUNT_FUNC - 1);
}

static void range_sync_names(void)
{
    /* Update the alias and show/hide extra rows to match current mode */
    bool param = graph_state.param_mode;
    range_field_names = param ? range_field_names_param : range_field_names_func;
    uint8_t show_count = param ? RANGE_ROW_COUNT_PARAM : RANGE_ROW_COUNT_FUNC;
    for (int i = 0; i < RANGE_ROW_COUNT_PARAM; i++) {
        if (ui_lbl_range_rows[i] == NULL) continue;
        if (i < show_count)
            lv_obj_clear_flag(ui_lbl_range_rows[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(ui_lbl_range_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void range_field_reset(void)
{
    s_range.field   = 0;
    s_range.len     = 0;
    s_range.buf[0]  = '\0';
    s_range.cursor  = 0;
}

static float range_field_value(uint8_t field)
{
    if (graph_state.param_mode) {
        switch (field) {
        case 0: return graph_state.t_min;
        case 1: return graph_state.t_max;
        case 2: return graph_state.t_step;
        case 3: return graph_state.x_min;
        case 4: return graph_state.x_max;
        case 5: return graph_state.x_scl;
        case 6: return graph_state.y_min;
        case 7: return graph_state.y_max;
        case 8: return graph_state.y_scl;
        }
    } else {
        float vals[RANGE_ROW_COUNT_FUNC] = {
            graph_state.x_min, graph_state.x_max, graph_state.x_scl,
            graph_state.y_min, graph_state.y_max, graph_state.y_scl,
            graph_state.x_res
        };
        if (field < RANGE_ROW_COUNT_FUNC) return vals[field];
    }
    return 0.0f;
}

static void range_load_field(void)
{
    snprintf(s_range.buf, sizeof(s_range.buf), "%.4g", range_field_value(s_range.field));
    s_range.len    = (uint8_t)strlen(s_range.buf);
    s_range.cursor = 0;
}

static void range_commit_field(void)
{
    if (s_range.len == 0)
        return;
    float val = strtof(s_range.buf, NULL);
    if (graph_state.param_mode) {
        switch (s_range.field) {
        case 0: graph_state.t_min = val; break;
        case 1: graph_state.t_max = val; break;
        case 2: if (val > 0.0f) graph_state.t_step = val; break;
        case 3: graph_state.x_min = val; break;
        case 4: graph_state.x_max = val; break;
        case 5: if (val > 0.0f) graph_state.x_scl = val; break;
        case 6: graph_state.y_min = val; break;
        case 7: graph_state.y_max = val; break;
        case 8: if (val > 0.0f) graph_state.y_scl = val; break;
        }
    } else {
        switch (s_range.field) {
        case 0: graph_state.x_min = val; break;
        case 1: graph_state.x_max = val; break;
        case 2: if (val > 0.0f) graph_state.x_scl = val; break;
        case 3: graph_state.y_min = val; break;
        case 4: graph_state.y_max = val; break;
        case 5: if (val > 0.0f) graph_state.y_scl = val; break;
        case 6: { int32_t iv = (int32_t)val; if (iv >= 1 && iv <= 8) graph_state.x_res = (float)iv; } break;
        }
    }
}

static void range_update_highlight(void)
{
    uint8_t count = graph_state.param_mode ? RANGE_ROW_COUNT_PARAM : RANGE_ROW_COUNT_FUNC;
    for (uint8_t i = 0; i < count; i++) {
        lv_obj_t *lbl = ui_lbl_range_rows[i];
        if (lbl == NULL) continue;
        lv_obj_set_style_text_color(lbl,
            (i == s_range.field) ? lv_color_hex(COLOR_YELLOW)
                                 : lv_color_hex(COLOR_WHITE),
            0);
    }
}

/*---------------------------------------------------------------------------
 * RANGE display and cursor
 *---------------------------------------------------------------------------*/

void ui_update_range_display(void)
{
    uint8_t count = graph_state.param_mode ? RANGE_ROW_COUNT_PARAM : RANGE_ROW_COUNT_FUNC;
    char row_buf[32];
    char val_buf[16];
    for (int i = 0; i < count; i++) {
        if (ui_lbl_range_rows[i] == NULL) continue;
        if (i == (int)s_range.field && s_range.len > 0) {
            snprintf(row_buf, sizeof(row_buf), "%s%s",
                     range_field_names[i], s_range.buf);
        } else {
            snprintf(val_buf, sizeof(val_buf), "%.4g", range_field_value((uint8_t)i));
            snprintf(row_buf, sizeof(row_buf), "%s%s",
                     range_field_names[i], val_buf);
        }
        lv_label_set_text(ui_lbl_range_rows[i], row_buf);
    }
}

void range_cursor_update(void)
{
    if (range_cursor_box == NULL || ui_lbl_range_rows[s_range.field] == NULL) return;
    uint32_t char_pos = (uint32_t)strlen(range_field_names[s_range.field])
                      + s_range.cursor;
    cursor_render(range_cursor_box, range_cursor_inner,
                  ui_lbl_range_rows[s_range.field], char_pos,
                  cursor_visible, current_mode, insert_mode);
}

/*---------------------------------------------------------------------------
 * Nav entry point
 *---------------------------------------------------------------------------*/

void range_nav_enter(void)
{
    range_sync_names();
    range_field_reset();
    range_load_field();
    lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_range_display();
    range_update_highlight();
    range_cursor_update();
}

/*---------------------------------------------------------------------------
 * ZOOM FACTORS state helpers
 *---------------------------------------------------------------------------*/

static void zoom_factors_reset(void)
{
    s_zf.field  = 0;
    s_zf.len    = 0;
    s_zf.buf[0] = '\0';
    s_zf.cursor = 0;
}

static void zoom_factors_load_field(void)
{
    float val = (s_zf.field == 0) ? s_zf.x_fact : s_zf.y_fact;
    snprintf(s_zf.buf, sizeof(s_zf.buf), "%.4g", val);
    s_zf.len    = (uint8_t)strlen(s_zf.buf);
    s_zf.cursor = 0;
}

static void zoom_factors_commit_field(void)
{
    if (s_zf.len == 0) return;
    float val = strtof(s_zf.buf, NULL);
    if (val <= 0.0f) return;
    if (s_zf.field == 0) s_zf.x_fact = val;
    else                 s_zf.y_fact = val;
}

static void zoom_factors_update_highlight(void)
{
    for (int i = 0; i < 2; i++) {
        if (ui_lbl_zoom_factors_rows[i] == NULL) continue;
        lv_obj_set_style_text_color(ui_lbl_zoom_factors_rows[i],
            (i == (int)s_zf.field) ? lv_color_hex(COLOR_YELLOW)
                                   : lv_color_hex(COLOR_WHITE),
            0);
    }
}

/*---------------------------------------------------------------------------
 * ZOOM FACTORS display and cursor
 *---------------------------------------------------------------------------*/

void ui_update_zoom_factors_display(void)
{
    char buf[32];
    float vals[2] = { s_zf.x_fact, s_zf.y_fact };
    for (int i = 0; i < 2; i++) {
        if (ui_lbl_zoom_factors_rows[i] == NULL) continue;
        if (i == (int)s_zf.field && s_zf.len > 0) {
            snprintf(buf, sizeof(buf), "%s%s", zoom_factors_names[i], s_zf.buf);
        } else {
            char val_str[16];
            snprintf(val_str, sizeof(val_str), "%.4g", vals[i]);
            snprintf(buf, sizeof(buf), "%s%s", zoom_factors_names[i], val_str);
        }
        lv_label_set_text(ui_lbl_zoom_factors_rows[i], buf);
    }
}

void zoom_factors_cursor_update(void)
{
    if (zoom_factors_cursor_box == NULL) return;
    if (ui_lbl_zoom_factors_rows[s_zf.field] == NULL) return;
    uint32_t char_pos = (uint32_t)strlen(zoom_factors_names[s_zf.field])
                      + s_zf.cursor;
    cursor_render(zoom_factors_cursor_box, zoom_factors_cursor_inner,
                  ui_lbl_zoom_factors_rows[s_zf.field], char_pos,
                  cursor_visible, current_mode, insert_mode);
}

/*---------------------------------------------------------------------------
 * Nav entry point
 *---------------------------------------------------------------------------*/

void zoom_factors_nav_enter(void)
{
    zoom_factors_reset();
    zoom_factors_load_field();
    lv_obj_clear_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_zoom_factors_display();
    zoom_factors_update_highlight();
    zoom_factors_cursor_update();
}

/*---------------------------------------------------------------------------
 * Persist accessors
 *---------------------------------------------------------------------------*/

float graph_ui_get_zoom_x_fact(void) { return s_zf.x_fact; }
float graph_ui_get_zoom_y_fact(void) { return s_zf.y_fact; }

void graph_ui_set_zoom_facts(float x_fact, float y_fact)
{
    s_zf.x_fact = x_fact;
    s_zf.y_fact = y_fact;
}

/*---------------------------------------------------------------------------
 * Shared field-editor helper
 * Used by handle_range_mode and handle_zoom_factors_mode.
 *---------------------------------------------------------------------------*/

/* Handle character-editing tokens common to all numeric field editors:
 * TOKEN_0-9, TOKEN_DECIMAL, TOKEN_DEL, TOKEN_LEFT, TOKEN_RIGHT, TOKEN_INS.
 * Acquires lvgl_lock() internally when state changes.
 * Returns true if the token was consumed, false for any other token. */
static bool field_editor_handle(Token_t t, const FieldEditor_t *fe)
{
    char    *buf    = fe->buf;
    uint8_t *len    = fe->len;
    uint8_t *cursor = fe->cursor;
    bool display_dirty = false;

    switch (t) {
    case TOKEN_0 ... TOKEN_9: {
        char ch = (char)((t - TOKEN_0) + '0');
        if (!insert_mode && *cursor < *len) {
            buf[(*cursor)++] = ch;
        } else if (*len < fe->cap - 1) {
            memmove(&buf[*cursor + 1], &buf[*cursor], *len - *cursor + 1);
            buf[(*cursor)++] = ch;
            (*len)++;
        }
        display_dirty = true;
        break;
    }
    case TOKEN_DECIMAL:
        if (strchr(buf, '.') == NULL) {
            char ch = '.';
            if (!insert_mode && *cursor < *len) {
                buf[(*cursor)++] = ch;
            } else if (*len < fe->cap - 1) {
                memmove(&buf[*cursor + 1], &buf[*cursor], *len - *cursor + 1);
                buf[(*cursor)++] = ch;
                (*len)++;
            }
            display_dirty = true;
        }
        break;
    case TOKEN_DEL:
        if (*cursor > 0) {
            memmove(&buf[*cursor - 1], &buf[*cursor], *len - *cursor + 1);
            (*len)--;
            (*cursor)--;
            display_dirty = true;
        } else {
            return true; /* consumed but nothing to update */
        }
        break;
    case TOKEN_LEFT:
        if (*cursor == 0) return true;
        (*cursor)--;
        break;
    case TOKEN_RIGHT:
        if (*cursor >= *len) return true;
        (*cursor)++;
        break;
    case TOKEN_INS:
        insert_mode = !insert_mode;
        break;
    default:
        return false;
    }

    lvgl_lock();
    if (display_dirty) fe->update();
    fe->cursor_fn();
    lvgl_unlock();
    return true;
}

/*---------------------------------------------------------------------------
 * Token handlers
 *---------------------------------------------------------------------------*/

bool handle_range_mode(Token_t t)
{
    FieldEditor_t fe = {
        .buf       = s_range.buf,
        .cap       = sizeof(s_range.buf),
        .len       = &s_range.len,
        .cursor    = &s_range.cursor,
        .update    = ui_update_range_display,
        .cursor_fn = range_cursor_update,
    };
    if (field_editor_handle(t, &fe)) return true;

    switch (t) {
    case TOKEN_NEG:
        if (s_range.len > 0 && s_range.buf[0] == '-') {
            memmove(s_range.buf, s_range.buf + 1, s_range.len);
            s_range.len--;
        } else if (s_range.len < sizeof(s_range.buf) - 1) {
            memmove(s_range.buf + 1, s_range.buf, s_range.len + 1);
            s_range.buf[0] = '-';
            s_range.len++;
        }
        s_range.cursor = s_range.len;
        lvgl_lock();
        ui_update_range_display();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER:
    case TOKEN_DOWN:
        range_commit_field();
        if (s_range.field < range_field_max()) s_range.field++;
        range_load_field();
        lvgl_lock();
        ui_update_range_display();
        range_update_highlight();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_UP:
        range_commit_field();
        if (s_range.field > 0) s_range.field--;
        range_load_field();
        lvgl_lock();
        ui_update_range_display();
        range_update_highlight();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ZOOM:
        range_commit_field();
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;

    case TOKEN_GRAPH:
        range_commit_field();
        nav_to(MODE_NORMAL);
        return true;

    case TOKEN_RANGE:
        current_mode = MODE_NORMAL;
        range_field_reset();
        lvgl_lock();
        hide_all_screens();
        ui_update_range_display();
        lvgl_unlock();
        return true;

    case TOKEN_CLEAR:
        if (s_range.len > 0) {
            s_range.len    = 0;
            s_range.buf[0] = '\0';
            s_range.cursor = 0;
            lvgl_lock();
            ui_update_range_display();
            range_cursor_update();
            lvgl_unlock();
        } else {
            current_mode = MODE_NORMAL;
            range_field_reset();
            lvgl_lock();
            ui_update_range_display();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
        }
        return true;

    case TOKEN_Y_EQUALS:
        range_commit_field();
        nav_to(graph_state.param_mode ? MODE_GRAPH_PARAM_YEQ : MODE_GRAPH_YEQ);
        return true;

    case TOKEN_TRACE:
        range_commit_field();
        nav_to(MODE_GRAPH_TRACE);
        return true;

    default:
        lvgl_lock();
        ui_update_status_bar();
        range_cursor_update();
        lvgl_unlock();
        return true;
    }
}

bool handle_zoom_factors_mode(Token_t t)
{
    FieldEditor_t fe = {
        .buf       = s_zf.buf,
        .cap       = sizeof(s_zf.buf),
        .len       = &s_zf.len,
        .cursor    = &s_zf.cursor,
        .update    = ui_update_zoom_factors_display,
        .cursor_fn = zoom_factors_cursor_update,
    };
    if (field_editor_handle(t, &fe)) return true;

    switch (t) {
    case TOKEN_ENTER:
    case TOKEN_DOWN:
        zoom_factors_commit_field();
        if (s_zf.field < 1) s_zf.field++;
        zoom_factors_load_field();
        lvgl_lock();
        ui_update_zoom_factors_display();
        zoom_factors_update_highlight();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_UP:
        zoom_factors_commit_field();
        if (s_zf.field > 0) s_zf.field--;
        zoom_factors_load_field();
        lvgl_lock();
        ui_update_zoom_factors_display();
        zoom_factors_update_highlight();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ZOOM:
        zoom_factors_commit_field();
        zoom_factors_reset();
        zoom_menu_reset();
        current_mode = MODE_GRAPH_ZOOM;
        lvgl_lock();
        hide_all_screens();
        lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_zoom_display();
        lvgl_unlock();
        return true;

    case TOKEN_CLEAR:
        if (s_zf.len > 0) {
            s_zf.len    = 0;
            s_zf.buf[0] = '\0';
            s_zf.cursor = 0;
            lvgl_lock();
            ui_update_zoom_factors_display();
            zoom_factors_cursor_update();
            lvgl_unlock();
        } else {
            zoom_factors_reset();
            current_mode = MODE_GRAPH_ZOOM;
            zoom_menu_reset();
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_zoom_display();
            lvgl_unlock();
        }
        return true;

    case TOKEN_Y_EQUALS:
        zoom_factors_commit_field();
        zoom_factors_reset();
        nav_to(MODE_GRAPH_YEQ);
        return true;

    case TOKEN_RANGE:
        zoom_factors_commit_field();
        zoom_factors_reset();
        nav_to(MODE_GRAPH_RANGE);
        return true;

    case TOKEN_GRAPH:
        zoom_factors_commit_field();
        zoom_factors_reset();
        nav_to(MODE_NORMAL);
        return true;

    case TOKEN_TRACE:
        zoom_factors_commit_field();
        zoom_factors_reset();
        nav_to(MODE_GRAPH_TRACE);
        return true;

    default:
        lvgl_lock();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;
    }
}
