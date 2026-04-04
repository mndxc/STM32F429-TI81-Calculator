/**
 * @file    graph_ui.c
 * @brief   Graph screen UI handlers and helpers (Y=, RANGE, ZOOM, TRACE, ZBox).
 *
 * Extracted from calculator_core.c following the same pattern as ui_matrix.c
 * and ui_prgm.c. Zero behavioral changes — purely a file organisation refactor.
 *
 * All LVGL calls must be made under lvgl_lock()/lvgl_unlock() except from
 * cursor_timer_cb (which runs inside lv_task_handler — mutex already held).
 */

#include "graph_ui.h"
#include "calc_internal.h"
#include "app_common.h"
#include "graph.h"
#include "expr_util.h"
#include "calc_engine.h"
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
    float   x;          /* Current trace x position */
    uint8_t eq_idx;     /* Which equation the crosshair is on */
} TraceState_t;

typedef struct {
    int32_t  px,  py;           /* Current cursor pixel position */
    int32_t  px1, py1;          /* First corner (valid once corner1_set) */
    bool     corner1_set;
} ZBoxState_t;

typedef struct {
    float    x_fact, y_fact;    /* XFact / YFact zoom multipliers (persisted) */
    uint8_t  field;             /* 0=XFact, 1=YFact */
    char     buf[16];           /* In-progress edit buffer */
    uint8_t  len;
    uint8_t  cursor;
} ZoomFactorsState_t;

typedef struct {
    uint8_t  scroll_offset;
    uint8_t  item_cursor;       /* Visible-row index of highlight */
} ZoomMenuState_t;

typedef struct {
    uint8_t  selected;          /* Which Y= row is active */
    uint8_t  cursor_pos;        /* Byte offset of insertion point within the equation */
    bool     on_equal;          /* True if cursor is on the '=' sign */
} YeqEditorState_t;

/*---------------------------------------------------------------------------
 * Forward declarations
 *---------------------------------------------------------------------------*/

static float range_field_value(uint8_t field);

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define ZOOM_ITEM_COUNT       8
#define RANGE_ROW_COUNT_FUNC  7
#define RANGE_ROW_COUNT_PARAM 9

static const char * const zoom_item_names[ZOOM_ITEM_COUNT] = {
    "Box", "Zoom In", "Zoom Out", "Set Factors",
    "Square", "Standard", "Trig", "Integer"
};

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
 * LVGL object pointers — screen pointers are non-static (extern in headers)
 *---------------------------------------------------------------------------*/

/* Screen pointers — non-static so hide_all_screens() and menu_close() can reach them */
lv_obj_t *ui_graph_yeq_screen          = NULL;
lv_obj_t *ui_graph_range_screen        = NULL;
lv_obj_t *ui_graph_zoom_screen         = NULL;
lv_obj_t *ui_graph_zoom_factors_screen = NULL;
lv_obj_t *ui_param_yeq_screen          = NULL;

/* Y= editor labels and cursor */
static lv_obj_t *ui_lbl_yeq_name[GRAPH_NUM_EQ];
static lv_obj_t *ui_lbl_yeq_equal[GRAPH_NUM_EQ];
static lv_obj_t *ui_lbl_yeq_eq[GRAPH_NUM_EQ];
static lv_obj_t *yeq_cursor_box   = NULL;
static lv_obj_t *yeq_cursor_inner = NULL;

/* Parametric Y= editor labels and cursor — 6 rows: X₁t,Y₁t,X₂t,Y₂t,X₃t,Y₃t */
#define PARAM_YEQ_ROW_COUNT 6
static lv_obj_t *ui_lbl_param_name[PARAM_YEQ_ROW_COUNT];
static lv_obj_t *ui_lbl_param_equal[PARAM_YEQ_ROW_COUNT];
static lv_obj_t *ui_lbl_param_eq[PARAM_YEQ_ROW_COUNT];
static lv_obj_t *param_cursor_box   = NULL;
static lv_obj_t *param_cursor_inner = NULL;

/* RANGE editor labels and cursor — 9 rows max (7 func, 9 param) */
static lv_obj_t *ui_lbl_range_rows[RANGE_ROW_COUNT_PARAM];
static lv_obj_t *range_cursor_box    = NULL;
static lv_obj_t *range_cursor_inner  = NULL;

/* ZOOM menu labels */
static lv_obj_t *zoom_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *zoom_scroll_ind[2];   /* [0]=top(↑), [1]=bottom(↓) */

/* ZOOM FACTORS labels and cursor */
static lv_obj_t *ui_lbl_zoom_factors_rows[2];
static lv_obj_t *zoom_factors_cursor_box   = NULL;
static lv_obj_t *zoom_factors_cursor_inner = NULL;

/*---------------------------------------------------------------------------
 * State instances
 *---------------------------------------------------------------------------*/

static YeqEditorState_t   s_yeq   = {0};
static YeqEditorState_t   s_param_yeq = {0}; /* parametric Y= editor state */
static RangeEditorState_t s_range = {0};
static TraceState_t       s_trace = {0};
static ZBoxState_t        s_zbox  = { .px = GRAPH_W / 2, .py = GRAPH_H / 2 };
static ZoomMenuState_t    s_zoom  = {0};
static ZoomFactorsState_t s_zf    = { .x_fact = 4.0f, .y_fact = 4.0f };

/*---------------------------------------------------------------------------
 * Forward declarations for static helpers
 * (called by zoom_execute_item before their definitions)
 *---------------------------------------------------------------------------*/

static void zoom_factors_reset(void);
static void zoom_factors_load_field(void);
void ui_update_zoom_factors_display(void);
static void zoom_factors_update_highlight(void);
void zoom_factors_cursor_update(void);
static bool yeq_cursor_move(Token_t t);
static bool yeq_row_switch(Token_t t);
static void yeq_del_at_cursor(void);
static void param_yeq_update_highlight(void);
static void param_yeq_reflow_rows(void);

/*---------------------------------------------------------------------------
 * Initialisation helpers (one per screen)
 *---------------------------------------------------------------------------*/

static void ui_init_yeq_screen(lv_obj_t *parent)
{
    ui_graph_yeq_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_graph_yeq_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_yeq_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_yeq_screen,
                               lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_graph_yeq_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_yeq_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);

    const char *eq_row_names[] = { "Y\xe2\x82\x81", "Y\xe2\x82\x82", "Y\xe2\x82\x83", "Y\xe2\x82\x84" };
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        int32_t row_y = 4 + i * 26;
        ui_lbl_yeq_name[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_name[i], 4, row_y);
        lv_obj_set_style_text_font(ui_lbl_yeq_name[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_yeq_name[i], eq_row_names[i]);

        ui_lbl_yeq_equal[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_equal[i], 31, row_y); /* Shifted right from name */
        lv_obj_set_style_text_font(ui_lbl_yeq_equal[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_equal[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_yeq_equal[i], "=");

        ui_lbl_yeq_eq[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_eq[i], 44, row_y);
        lv_obj_set_width(ui_lbl_yeq_eq[i], DISPLAY_W - 48);
        lv_obj_set_style_text_font(ui_lbl_yeq_eq[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_eq[i],
                                     lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_long_mode(ui_lbl_yeq_eq[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(ui_lbl_yeq_eq[i], "");
    }

    cursor_box_create(ui_graph_yeq_screen, true, &yeq_cursor_box, &yeq_cursor_inner);
}

static void ui_init_param_yeq_screen(lv_obj_t *parent)
{
    /* Row labels: X₁t, Y₁t, X₂t, Y₂t, X₃t, Y₃t */
    static const char * const param_row_names[PARAM_YEQ_ROW_COUNT] = {
        "X\xe2\x82\x81""t", "Y\xe2\x82\x81""t",
        "X\xe2\x82\x82""t", "Y\xe2\x82\x82""t",
        "X\xe2\x82\x83""t", "Y\xe2\x82\x83""t",
    };

    ui_param_yeq_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_param_yeq_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_param_yeq_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_param_yeq_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_param_yeq_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_param_yeq_screen, 0, 0);
    lv_obj_clear_flag(ui_param_yeq_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_param_yeq_screen, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        int32_t row_y = 4 + i * 26;

        ui_lbl_param_name[i] = lv_label_create(ui_param_yeq_screen);
        lv_obj_set_pos(ui_lbl_param_name[i], 4, row_y);
        lv_obj_set_style_text_font(ui_lbl_param_name[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_param_name[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_param_name[i], param_row_names[i]);

        ui_lbl_param_equal[i] = lv_label_create(ui_param_yeq_screen);
        lv_obj_set_pos(ui_lbl_param_equal[i], 44, row_y);
        lv_obj_set_style_text_font(ui_lbl_param_equal[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_param_equal[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(ui_lbl_param_equal[i], "=");

        ui_lbl_param_eq[i] = lv_label_create(ui_param_yeq_screen);
        lv_obj_set_pos(ui_lbl_param_eq[i], 57, row_y);
        lv_obj_set_width(ui_lbl_param_eq[i], DISPLAY_W - 61);
        lv_obj_set_style_text_font(ui_lbl_param_eq[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_param_eq[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_long_mode(ui_lbl_param_eq[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(ui_lbl_param_eq[i], "");
    }

    cursor_box_create(ui_param_yeq_screen, true, &param_cursor_box, &param_cursor_inner);
}

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

static void ui_init_zoom_screen(lv_obj_t *parent)
{
    ui_graph_zoom_screen = lv_obj_create(parent);
    lv_obj_set_size(ui_graph_zoom_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_zoom_screen, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_zoom_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_zoom_title = lv_label_create(ui_graph_zoom_screen);
    lv_obj_set_pos(lbl_zoom_title, 4, 4);
    lv_obj_set_style_text_font(lbl_zoom_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_zoom_title, lv_color_hex(COLOR_WHITE), 0);
    lv_label_set_text(lbl_zoom_title, "ZOOM");

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        zoom_item_labels[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(zoom_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(zoom_item_labels[i], "");
    }

    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        zoom_scroll_ind[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(zoom_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(zoom_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
        lv_obj_set_style_bg_opa(zoom_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(zoom_scroll_ind[i], 0, 0);
        lv_label_set_text(zoom_scroll_ind[i], "");
        lv_obj_add_flag(zoom_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
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

/*---------------------------------------------------------------------------
 * Initialisation
 *---------------------------------------------------------------------------*/

/**
 * @brief Creates the Y=, RANGE, ZOOM, ZOOM FACTORS screens and graph canvas.
 *        Called once from StartCalcCoreTask under lvgl_lock().
 */
void ui_init_graph_screens(void)
{
    lv_obj_t *scr = lv_scr_act();

    ui_init_yeq_screen(scr);
    ui_init_param_yeq_screen(scr);
    ui_init_range_screen(scr);
    ui_init_zoom_screen(scr);
    ui_init_zoom_factors_screen(scr);

    /* Init graph canvas */
    Graph_Init(scr);
}

/*---------------------------------------------------------------------------
 * Display update helpers (some non-static — called from calculator_core.c)
 *---------------------------------------------------------------------------*/

/**
 * @brief Refreshes all seven RANGE row labels from the current graph_state values.
 */
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

/**
 * @brief Reflows Y= row positions after equation text changes.
 *        Must be called under lvgl_lock().
 */
void yeq_reflow_rows(void)
{
    lv_obj_update_layout(ui_graph_yeq_screen);
    int32_t y = 4;
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        lv_obj_set_pos(ui_lbl_yeq_name[i],  4,  y);
        lv_obj_set_pos(ui_lbl_yeq_equal[i], 31, y);
        lv_obj_set_pos(ui_lbl_yeq_eq[i],     44, y);
        int32_t h = lv_obj_get_height(ui_lbl_yeq_eq[i]);
        if (h < 26) h = 26;
        y += h + 2;
    }
}

/**
 * @brief Positions the Y= equation editor cursor.
 *        Called from cursor_timer_cb — must NOT call lvgl_lock().
 */
void yeq_cursor_update(void)
{
    if (yeq_cursor_box == NULL) return;
    if (s_yeq.on_equal) {
        cursor_render(yeq_cursor_box, yeq_cursor_inner,
                      ui_lbl_yeq_equal[s_yeq.selected], 0,
                      cursor_visible, current_mode, insert_mode);
    } else {
        if (ui_lbl_yeq_eq[s_yeq.selected] == NULL) return;
        const char *txt = lv_label_get_text(ui_lbl_yeq_eq[s_yeq.selected]);
        uint32_t glyph_pos = ExprUtil_Utf8ByteToGlyph(txt, s_yeq.cursor_pos);
        cursor_render(yeq_cursor_box, yeq_cursor_inner,
                      ui_lbl_yeq_eq[s_yeq.selected], glyph_pos,
                      cursor_visible, current_mode, insert_mode);
    }
}

/* Positions the RANGE field editor cursor.
 * Called from cursor_timer_cb — must NOT call lvgl_lock(). */
void range_cursor_update(void)
{
    if (range_cursor_box == NULL || ui_lbl_range_rows[s_range.field] == NULL) return;
    uint32_t char_pos = (uint32_t)strlen(range_field_names[s_range.field])
                      + s_range.cursor;
    cursor_render(range_cursor_box, range_cursor_inner,
                  ui_lbl_range_rows[s_range.field], char_pos,
                  cursor_visible, current_mode, insert_mode);
}

/* Highlights the active Y= row label in yellow; all others are white. */
static void yeq_update_highlight(void)
{
    for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
        /* Name is yellow if selected row, white otherwise */
        lv_color_t name_col = (i == s_yeq.selected) ? lv_color_hex(COLOR_YELLOW)
                                                   : lv_color_hex(COLOR_WHITE);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i], name_col, 0);

        /* Equal sign is amber if enabled, inactive grey if disabled */
        lv_color_t eq_col = graph_state.enabled[i] ? lv_color_hex(COLOR_AMBER)
                                                 : lv_color_hex(COLOR_GREY_INACTIVE);
        lv_obj_set_style_text_color(ui_lbl_yeq_equal[i], eq_col, 0);
    }
}

/*---------------------------------------------------------------------------
 * Zoom preset helper
 *---------------------------------------------------------------------------*/

static void apply_zoom_preset(uint8_t preset)
{
    switch (preset) {
    case 1: /* ZStandard */
        graph_state.x_min = -10.0f; graph_state.x_max =  10.0f; graph_state.x_scl = 1.0f;
        graph_state.y_min = -10.0f; graph_state.y_max =  10.0f; graph_state.y_scl = 1.0f;
        break;
    case 2: /* ZTrig */
        graph_state.x_min = -6.2832f; graph_state.x_max = 6.2832f; graph_state.x_scl = 1.5708f;
        graph_state.y_min =  -4.0f;   graph_state.y_max = 4.0f;    graph_state.y_scl = 1.0f;
        break;
    case 3: /* ZDecimal */
        graph_state.x_min = -4.7f; graph_state.x_max = 4.7f; graph_state.x_scl = 0.5f;
        graph_state.y_min = -3.1f; graph_state.y_max = 3.1f; graph_state.y_scl = 0.5f;
        break;
    case 4: /* ZSquare */
        {
            float xs = (graph_state.x_max - graph_state.x_min) / GRAPH_W;
            float ys = (graph_state.y_max - graph_state.y_min) / GRAPH_H;
            if (xs > ys) {
                float yc = (graph_state.y_max + graph_state.y_min) * 0.5f;
                float yh = xs * GRAPH_H * 0.5f;
                graph_state.y_min = yc - yh;
                graph_state.y_max = yc + yh;
            } else {
                float xc = (graph_state.x_max + graph_state.x_min) * 0.5f;
                float xh = ys * GRAPH_W * 0.5f;
                graph_state.x_min = xc - xh;
                graph_state.x_max = xc + xh;
            }
        }
        break;
    case 5: /* ZInteger */
        graph_state.x_min = -160.0f; graph_state.x_max = 159.0f; graph_state.x_scl = 10.0f;
        graph_state.y_min = -110.0f; graph_state.y_max = 109.0f; graph_state.y_scl = 10.0f;
        break;
    default:
        graph_state.x_min = -10.0f; graph_state.x_max =  10.0f; graph_state.x_scl = 1.0f;
        graph_state.y_min = -10.0f; graph_state.y_max =  10.0f; graph_state.y_scl = 1.0f;
        break;
    }
}

/*---------------------------------------------------------------------------
 * ZOOM display helper
 *---------------------------------------------------------------------------*/

void ui_update_zoom_display(void)
{
    lv_obj_add_flag(zoom_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(zoom_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = s_zoom.scroll_offset + i;
        if (idx >= ZOOM_ITEM_COUNT) {
            lv_label_set_text(zoom_item_labels[i], "");
            continue;
        }
        bool more_below = (s_zoom.scroll_offset + MENU_VISIBLE_ROWS < ZOOM_ITEM_COUNT)
                          && (i == MENU_VISIBLE_ROWS - 1);
        bool more_above = (s_zoom.scroll_offset > 0) && (i == 0);
        char buf[32];
        if (more_below) {
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, zoom_item_names[idx]);
            lv_label_set_text(zoom_scroll_ind[1], "\xE2\x86\x93");
            lv_obj_clear_flag(zoom_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        } else if (more_above) {
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, zoom_item_names[idx]);
            lv_label_set_text(zoom_scroll_ind[0], "\xE2\x86\x91");
            lv_obj_clear_flag(zoom_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        } else {
            snprintf(buf, sizeof(buf), "%d:%s", idx + 1, zoom_item_names[idx]);
        }
        lv_color_t col = (i == (int)s_zoom.item_cursor)
            ? lv_color_hex(COLOR_YELLOW)
            : lv_color_hex(COLOR_WHITE);
        lv_obj_set_style_text_color(zoom_item_labels[i], col, 0);
        lv_label_set_text(zoom_item_labels[i], buf);
    }
}

/*---------------------------------------------------------------------------
 * Graph/menu state helpers
 *---------------------------------------------------------------------------*/

static uint8_t find_first_active_eq(void)
{
    for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
        if (strlen(graph_state.equations[i]) > 0 && graph_state.enabled[i])
            return i;
    }
    return 0;
}

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

static void zoom_menu_reset(void)
{
    s_zoom.scroll_offset = 0;
    s_zoom.item_cursor   = 0;
}

/*---------------------------------------------------------------------------
 * ZOOM action executor helpers
 *---------------------------------------------------------------------------*/

/* Hide zoom menu, switch to normal mode, show graph canvas, and redraw. */
static void zoom_show_graph(void)
{
    current_mode = MODE_NORMAL;
    lvgl_lock();
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
    Graph_SetVisible(true);
    Graph_Render(angle_degrees);
    lvgl_unlock();
}

/* Enter ZBox rubber-band selection mode. */
static void zoom_enter_zbox(void)
{
    s_zbox.px = GRAPH_W / 2; s_zbox.py = GRAPH_H / 2; s_zbox.corner1_set = false;
    current_mode = MODE_GRAPH_ZBOX;
    lvgl_lock();
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
    Graph_SetVisible(true);
    Graph_DrawZBox(s_zbox.px, s_zbox.py, 0, 0, false, angle_degrees);
    lvgl_unlock();
}

/* Scale the graph window by (xf, yf) around its centre.
 * xf < 1 zooms in on X; xf > 1 zooms out. Same convention for yf. */
static void zoom_scale_view(float xf, float yf)
{
    float xc = (graph_state.x_min + graph_state.x_max) * 0.5f;
    float yc = (graph_state.y_min + graph_state.y_max) * 0.5f;
    float xh = (graph_state.x_max - graph_state.x_min) * xf / 2.0f;
    float yh = (graph_state.y_max - graph_state.y_min) * yf / 2.0f;
    graph_state.x_min = xc - xh; graph_state.x_max = xc + xh;
    graph_state.y_min = yc - yh; graph_state.y_max = yc + yh;
}

/* Open the Zoom Factors editor screen. */
static void zoom_enter_factors(void)
{
    zoom_factors_reset();
    zoom_factors_load_field();
    current_mode = MODE_GRAPH_ZOOM_FACTORS;
    lvgl_lock();
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_zoom_factors_display();
    zoom_factors_update_highlight();
    zoom_factors_cursor_update();
    lvgl_unlock();
}

static void zoom_execute_item(uint8_t item_num)
{
    zoom_menu_reset();
    switch (item_num) {
    case 1: zoom_enter_zbox();                                          break;
    case 2: zoom_scale_view(1.0f / s_zf.x_fact, 1.0f / s_zf.y_fact);
            zoom_show_graph();                                          break;
    case 3: zoom_scale_view(s_zf.x_fact, s_zf.y_fact);
            zoom_show_graph();                                          break;
    case 4: zoom_enter_factors();                                       break;
    case 5: apply_zoom_preset(4); zoom_show_graph();                   break;
    case 6: apply_zoom_preset(1); zoom_show_graph();                   break;
    case 7: apply_zoom_preset(2); zoom_show_graph();                   break;
    case 8: apply_zoom_preset(5); zoom_show_graph();                   break;
    default:
        current_mode = MODE_NORMAL;
        lvgl_lock(); lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN); lvgl_unlock();
        break;
    }
}

/*---------------------------------------------------------------------------
 * RANGE editor helpers
 *---------------------------------------------------------------------------*/

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
 * ZOOM FACTORS editor helpers
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

/**
 * @brief Positions the cursor over the active ZOOM FACTORS field.
 *        Called from cursor_timer_cb — must NOT call lvgl_lock().
 */
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
 * Y= helpers called from calculator_core.c
 *---------------------------------------------------------------------------*/

/** Sync all Y= equation labels from graph_state after a persist load. */
void graph_ui_sync_yeq_labels(void)
{
    for (int i = 0; i < GRAPH_NUM_EQ; i++)
        lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
    yeq_update_highlight();
}

/**
 * @brief Insert @p ins into the active Y= equation at the cursor, restore
 *        the Y= screen, and update the display.  Called from menu_insert_text,
 *        math_menu_insert, and test_menu_insert in calculator_core.c when the
 *        return mode is MODE_GRAPH_YEQ.
 *
 * Sets current_mode = MODE_GRAPH_YEQ and acquires lvgl_lock() internally.
 */
void graph_ui_yeq_insert(const char *ins)
{
    current_mode = MODE_GRAPH_YEQ;
    char *eq = graph_state.equations[s_yeq.selected];
    size_t ins_len = strlen(ins);
    size_t eq_len  = strlen(eq);
    if (eq_len + ins_len < 63) {
        memmove(&eq[s_yeq.cursor_pos + ins_len], &eq[s_yeq.cursor_pos],
                eq_len - s_yeq.cursor_pos + 1);
        memcpy(&eq[s_yeq.cursor_pos], ins, ins_len);
        s_yeq.cursor_pos += (uint8_t)ins_len;
    }
    lvgl_lock();
    lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
    yeq_reflow_rows();
    yeq_cursor_update();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * nav_to — navigates to a graph-related screen from any current mode
 *---------------------------------------------------------------------------*/

/**
 * @brief Navigates to a graph-related screen from any current mode.
 *
 * Handles all hide/show/state-reset logic in one place.
 * Pass MODE_NORMAL to press GRAPH: renders the graph canvas.
 */
void nav_to(CalcMode_t target)
{
    lvgl_lock();
    hide_all_screens();
    current_mode = target;

    switch (target) {
    case MODE_GRAPH_YEQ:
        graph_state.active = false;
        s_yeq.on_equal   = false;
        s_yeq.cursor_pos = (uint8_t)strlen(graph_state.equations[s_yeq.selected]);
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
        yeq_update_highlight();
        yeq_reflow_rows();
        yeq_cursor_update();
        break;

    case MODE_GRAPH_RANGE:
        graph_state.active = false;
        range_sync_names();
        range_field_reset();
        range_load_field();
        lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_range_display();
        range_update_highlight();
        range_cursor_update();
        break;

    case MODE_GRAPH_ZOOM:
        graph_state.active = false;
        zoom_menu_reset();
        lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_zoom_display();
        break;

    case MODE_NORMAL:  /* TOKEN_GRAPH — show graph canvas and render */
        graph_state.active = true;
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        break;

    case MODE_GRAPH_TRACE:
        graph_state.active = true;
        if (graph_state.param_mode) {
            /* Find first enabled parametric pair */
            s_trace.eq_idx = 0;
            for (uint8_t i = 0; i < GRAPH_NUM_PARAM; i++) {
                if (graph_state.param_enabled[i] &&
                    strlen(graph_state.param_x[i]) > 0 &&
                    strlen(graph_state.param_y[i]) > 0) {
                    s_trace.eq_idx = i;
                    break;
                }
            }
            /* Start trace at midpoint of T range */
            s_trace.x = (graph_state.t_min + graph_state.t_max) * 0.5f;
        } else {
            s_trace.eq_idx = find_first_active_eq();
            s_trace.x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
        }
        Graph_SetVisible(true);
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        break;

    case MODE_GRAPH_PARAM_YEQ:
        graph_state.active = false;
        s_param_yeq.on_equal   = false;
        s_param_yeq.cursor_pos = (uint8_t)strlen(
            (s_param_yeq.selected % 2 == 0)
                ? graph_state.param_x[s_param_yeq.selected / 2]
                : graph_state.param_y[s_param_yeq.selected / 2]);
        lv_obj_clear_flag(ui_param_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
            uint8_t pair = (uint8_t)(i / 2);
            const char *eq = (i % 2 == 0)
                ? graph_state.param_x[pair]
                : graph_state.param_y[pair];
            lv_label_set_text(ui_lbl_param_eq[i], eq);
        }
        param_yeq_update_highlight();
        param_yeq_reflow_rows();
        param_yeq_cursor_update();
        break;

    default:
        break;
    }
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Per-mode token handlers
 *---------------------------------------------------------------------------*/

/**
 * @brief Handles cursor navigation tokens within the Y= editor.
 *
 * Covers: LEFT, RIGHT, UP, DOWN, DEL, CLEAR, ENTER, INS, and graph
 * navigation keys (RANGE, ZOOM, TRACE, GRAPH, Y_EQUALS).
 *
 * @return true if the token was consumed, false otherwise.
 */
/* Cursor movement within the current equation: LEFT, RIGHT, INS. */
static bool yeq_cursor_move(Token_t t)
{
    char *eq       = graph_state.equations[s_yeq.selected];
    uint8_t eq_len = (uint8_t)strlen(eq);
    switch (t) {
    case TOKEN_LEFT:
        if (s_yeq.on_equal) return true;
        if (s_yeq.cursor_pos > 0) {
            if (ExprUtil_MatrixTokenSizeBefore(eq, s_yeq.cursor_pos) == 3) {
                s_yeq.cursor_pos -= 3;
            } else {
                do { s_yeq.cursor_pos--; }
                while (s_yeq.cursor_pos > 0 &&
                       ((uint8_t)eq[s_yeq.cursor_pos] & 0xC0) == 0x80);
            }
        } else {
            s_yeq.on_equal = true;
        }
        break;
    case TOKEN_RIGHT:
        if (s_yeq.on_equal) {
            s_yeq.on_equal   = false;
            s_yeq.cursor_pos = 0;
        } else if (s_yeq.cursor_pos < eq_len) {
            uint8_t mat = ExprUtil_MatrixTokenSizeAt(eq, s_yeq.cursor_pos, eq_len);
            if (mat) {
                s_yeq.cursor_pos += mat;
            } else {
                uint8_t step = ExprUtil_Utf8CharSize(&eq[s_yeq.cursor_pos]);
                s_yeq.cursor_pos += step ? step : 1;
            }
            if (s_yeq.cursor_pos > eq_len) s_yeq.cursor_pos = eq_len;
        }
        break;
    case TOKEN_INS:
        insert_mode = !insert_mode;
        break;
    default:
        return false;
    }
    lvgl_lock();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

/* Row switching between Y1–Y4 equations: UP, DOWN, ENTER. */
static bool yeq_row_switch(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_yeq.selected > 0) s_yeq.selected--;
        if (!s_yeq.on_equal)
            s_yeq.cursor_pos = strlen(graph_state.equations[s_yeq.selected]);
        break;
    case TOKEN_ENTER:
        if (s_yeq.on_equal) {
            graph_state.enabled[s_yeq.selected] = !graph_state.enabled[s_yeq.selected];
            lvgl_lock();
            yeq_update_highlight();
            lvgl_unlock();
            return true;
        }
        __attribute__((fallthrough));
    case TOKEN_DOWN:
        if (s_yeq.selected < GRAPH_NUM_EQ - 1) s_yeq.selected++;
        if (!s_yeq.on_equal)
            s_yeq.cursor_pos = strlen(graph_state.equations[s_yeq.selected]);
        break;
    default:
        return false;
    }
    lvgl_lock();
    yeq_update_highlight();
    yeq_reflow_rows();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

/* Delete the character immediately before the cursor in the current equation. */
static void yeq_del_at_cursor(void)
{
    char *eq       = graph_state.equations[s_yeq.selected];
    uint8_t eq_len = (uint8_t)strlen(eq);
    if (s_yeq.cursor_pos == 0) return;
    uint8_t prev;
    uint8_t mat = ExprUtil_MatrixTokenSizeBefore(eq, s_yeq.cursor_pos);
    if (mat) {
        prev = s_yeq.cursor_pos - mat;
    } else {
        prev = s_yeq.cursor_pos;
        do { prev--; }
        while (prev > 0 && ((uint8_t)eq[prev] & 0xC0) == 0x80);
    }
    memmove(&eq[prev], &eq[s_yeq.cursor_pos], eq_len - s_yeq.cursor_pos + 1);
    s_yeq.cursor_pos = prev;
    lvgl_lock();
    lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
    yeq_reflow_rows();
    yeq_cursor_update();
    lvgl_unlock();
}

static bool handle_yeq_navigation(Token_t t)
{
    char *eq = graph_state.equations[s_yeq.selected];
    switch (t) {
    case TOKEN_GRAPH:    nav_to(MODE_NORMAL);                      return true;
    case TOKEN_RANGE:    nav_to(MODE_GRAPH_RANGE);                 return true;
    case TOKEN_ZOOM:     zoom_menu_reset(); nav_to(MODE_GRAPH_ZOOM); return true;
    case TOKEN_TRACE:    nav_to(MODE_GRAPH_TRACE);                 return true;
    case TOKEN_Y_EQUALS:
        current_mode = MODE_NORMAL;
        lvgl_lock(); hide_all_screens(); lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
        eq[0] = '\0'; s_yeq.cursor_pos = 0;
        lvgl_lock();
        lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
        yeq_reflow_rows(); yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_DEL:      yeq_del_at_cursor();                      return true;
    default:
        return yeq_cursor_move(t) || yeq_row_switch(t);
    }
}

/**
 * @brief Handles character and token insertion tokens within the Y= editor.
 *
 * Covers: digit keys, operator keys, function keys (sin/cos/etc.), letter
 * keys (ALPHA layer), and other tokens that append text to the equation.
 *
 * @return true if the token was consumed, false otherwise.
 */
static bool handle_yeq_insertion(Token_t t)
{
    char *eq     = graph_state.equations[s_yeq.selected];
    uint8_t eq_len = (uint8_t)strlen(eq);
    const char *append = NULL;
    char num_buf[2] = {0, 0};

    switch (t) {
    case TOKEN_X_T:   append = "x";     break;
    case TOKEN_0 ... TOKEN_9:
        num_buf[0] = (char)((t - TOKEN_0) + '0');
        append = num_buf;
        break;
    case TOKEN_DECIMAL: append = ".";     break;
    case TOKEN_EE:      append = "*10^";  break;
    case TOKEN_E_X:     append = "exp(";  break;
    case TOKEN_TEN_X:   append = "10^(";  break;
    case TOKEN_ADD:     append = "+";     break;
    case TOKEN_SUB:     append = "-";     break;
    case TOKEN_MULT:    append = "*";     break;
    case TOKEN_DIV:     append = "/";     break;
    case TOKEN_POWER:   append = "^";     break;
    case TOKEN_L_PAR:   append = "(";     break;
    case TOKEN_R_PAR:   append = ")";     break;
    case TOKEN_SIN:     append = "sin(";  break;
    case TOKEN_COS:     append = "cos(";  break;
    case TOKEN_TAN:     append = "tan(";  break;
    case TOKEN_ASIN:    append = "sin\xEE\x80\x81("; break;   /* sin⁻¹( */
    case TOKEN_ACOS:    append = "cos\xEE\x80\x81("; break;   /* cos⁻¹( */
    case TOKEN_ATAN:    append = "tan\xEE\x80\x81("; break;   /* tan⁻¹( */
    case TOKEN_LN:      append = "ln(";   break;
    case TOKEN_LOG:     append = "log(";  break;
    case TOKEN_SQRT:    append = "\xE2\x88\x9A("; break;
    case TOKEN_ABS:     append = "abs(";  break;
    case TOKEN_SQUARE:  append = "^2";    break;
    case TOKEN_PI:      append = "π";     break;
    case TOKEN_NEG:     append = "-";     break;
    case TOKEN_X_INV:   append = "\xEE\x80\x81";   break;  /* ⁻¹ U+E001 */
    case TOKEN_ANS:     append = "ANS";   break;
    case TOKEN_MATRX:
    case TOKEN_MTRX_A:
    case TOKEN_MTRX_B:
    case TOKEN_MTRX_C:
        return true;
    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z: {
        static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        num_buf[0] = alpha_chars[t - TOKEN_A];
        append = num_buf;
        break;
    }
    default: return false;
    }

    if (append != NULL && !s_yeq.on_equal) {
        size_t slen = strlen(append);
        uint8_t eq_len_u8 = (uint8_t)eq_len;
        if (slen == 1) {
            ExprUtil_InsertChar(eq, &eq_len_u8, &s_yeq.cursor_pos, 63, insert_mode, append[0]);
        } else if (eq_len_u8 + (uint8_t)slen < 63) {
            ExprUtil_InsertStr(eq, &eq_len_u8, &s_yeq.cursor_pos, 63, append);
        } else {
            return true;
        }
        lvgl_lock();
        lv_label_set_text(ui_lbl_yeq_eq[s_yeq.selected], eq);
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
    }
    return true;
}

bool handle_yeq_mode(Token_t t)
{
    if (s_yeq.cursor_pos > (uint8_t)strlen(graph_state.equations[s_yeq.selected]))
        s_yeq.cursor_pos = (uint8_t)strlen(graph_state.equations[s_yeq.selected]);

    if (handle_yeq_navigation(t)) {
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    }

    /* Menu-open tokens handled inline */
    if (t == TOKEN_MATH) {
        menu_open(TOKEN_MATH, MODE_GRAPH_YEQ);
        return true;
    }
    if (t == TOKEN_TEST) {
        menu_open(TOKEN_TEST, MODE_GRAPH_YEQ);
        return true;
    }

    if (handle_yeq_insertion(t)) {
        lvgl_lock();
        ui_update_status_bar();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    }

    lvgl_lock();
    ui_update_status_bar();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

/*---------------------------------------------------------------------------
 * Parametric Y= editor helpers and handler
 *---------------------------------------------------------------------------*/

/* Returns pointer to the active parametric equation string for row i.
 * Even rows (0,2,4) → param_x[i/2]; odd rows (1,3,5) → param_y[i/2]. */
static char *param_eq_ptr(uint8_t row)
{
    uint8_t pair = row / 2;
    return (row % 2 == 0) ? graph_state.param_x[pair] : graph_state.param_y[pair];
}

static void param_yeq_update_highlight(void)
{
    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        bool sel = (i == s_param_yeq.selected);
        lv_color_t c = lv_color_hex(sel ? COLOR_YELLOW : COLOR_WHITE);
        if (ui_lbl_param_name[i])  lv_obj_set_style_text_color(ui_lbl_param_name[i],  c, 0);
        if (ui_lbl_param_equal[i]) lv_obj_set_style_text_color(ui_lbl_param_equal[i], c, 0);
        if (ui_lbl_param_eq[i])    lv_obj_set_style_text_color(ui_lbl_param_eq[i],    c, 0);
    }
}

static void param_yeq_reflow_rows(void)
{
    lv_obj_update_layout(ui_param_yeq_screen);
    int32_t y = 4;
    for (int i = 0; i < PARAM_YEQ_ROW_COUNT; i++) {
        lv_obj_set_pos(ui_lbl_param_name[i],  4,  y);
        lv_obj_set_pos(ui_lbl_param_equal[i], 44, y);
        lv_obj_set_pos(ui_lbl_param_eq[i],    57, y);
        int32_t h = lv_obj_get_height(ui_lbl_param_eq[i]);
        if (h < 26) h = 26;
        y += h + 2;
    }
}

void param_yeq_cursor_update(void)
{
    if (param_cursor_box == NULL) return;
    if (s_param_yeq.on_equal) {
        cursor_render(param_cursor_box, param_cursor_inner,
                      ui_lbl_param_equal[s_param_yeq.selected], 0,
                      cursor_visible, current_mode, insert_mode);
    } else {
        if (ui_lbl_param_eq[s_param_yeq.selected] == NULL) return;
        const char *txt = lv_label_get_text(ui_lbl_param_eq[s_param_yeq.selected]);
        uint32_t glyph_pos = ExprUtil_Utf8ByteToGlyph(txt, s_param_yeq.cursor_pos);
        cursor_render(param_cursor_box, param_cursor_inner,
                      ui_lbl_param_eq[s_param_yeq.selected], glyph_pos,
                      cursor_visible, current_mode, insert_mode);
    }
}

/* Delete the character immediately before the cursor in the active param equation. */
static void param_yeq_del_at_cursor(void)
{
    char *eq       = param_eq_ptr(s_param_yeq.selected);
    uint8_t eq_len = (uint8_t)strlen(eq);
    if (s_param_yeq.cursor_pos == 0) return;
    uint8_t prev = s_param_yeq.cursor_pos;
    do { prev--; }
    while (prev > 0 && ((uint8_t)eq[prev] & 0xC0) == 0x80);
    memmove(&eq[prev], &eq[s_param_yeq.cursor_pos], eq_len - s_param_yeq.cursor_pos + 1);
    s_param_yeq.cursor_pos = prev;
    lvgl_lock();
    lv_label_set_text(ui_lbl_param_eq[s_param_yeq.selected], eq);
    param_yeq_reflow_rows();
    param_yeq_cursor_update();
    lvgl_unlock();
}

bool handle_param_yeq_mode(Token_t t)
{
    char *eq     = param_eq_ptr(s_param_yeq.selected);
    uint8_t pair = s_param_yeq.selected / 2;

    /* Cursor sanitise */
    uint8_t eq_len = (uint8_t)strlen(eq);
    if (s_param_yeq.cursor_pos > eq_len) s_param_yeq.cursor_pos = eq_len;

    /* Navigation keys */
    switch (t) {
    case TOKEN_GRAPH:    nav_to(MODE_NORMAL);            return true;
    case TOKEN_RANGE:    nav_to(MODE_GRAPH_RANGE);       return true;
    case TOKEN_ZOOM:     zoom_menu_reset(); nav_to(MODE_GRAPH_ZOOM); return true;
    case TOKEN_TRACE:    nav_to(MODE_GRAPH_TRACE);       return true;
    case TOKEN_Y_EQUALS:
        current_mode = MODE_NORMAL;
        lvgl_lock(); hide_all_screens(); lvgl_unlock();
        return true;

    case TOKEN_CLEAR:
        eq[0] = '\0'; s_param_yeq.cursor_pos = 0;
        lvgl_lock();
        lv_label_set_text(ui_lbl_param_eq[s_param_yeq.selected], eq);
        param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_DEL:
        param_yeq_del_at_cursor();
        return true;

    case TOKEN_INS:
        insert_mode = !insert_mode;
        lvgl_lock(); param_yeq_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_LEFT:
        if (s_param_yeq.on_equal) return true;
        if (s_param_yeq.cursor_pos > 0) {
            do { s_param_yeq.cursor_pos--; }
            while (s_param_yeq.cursor_pos > 0 &&
                   ((uint8_t)eq[s_param_yeq.cursor_pos] & 0xC0) == 0x80);
        } else {
            s_param_yeq.on_equal = true;
        }
        lvgl_lock(); param_yeq_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_RIGHT:
        if (s_param_yeq.on_equal) {
            s_param_yeq.on_equal   = false;
            s_param_yeq.cursor_pos = 0;
        } else if (s_param_yeq.cursor_pos < eq_len) {
            uint8_t step = ExprUtil_Utf8CharSize(&eq[s_param_yeq.cursor_pos]);
            s_param_yeq.cursor_pos += step ? step : 1;
            if (s_param_yeq.cursor_pos > eq_len) s_param_yeq.cursor_pos = eq_len;
        }
        lvgl_lock(); param_yeq_cursor_update(); lvgl_unlock();
        return true;

    case TOKEN_UP:
        if (s_param_yeq.selected > 0) s_param_yeq.selected--;
        if (!s_param_yeq.on_equal)
            s_param_yeq.cursor_pos = strlen(param_eq_ptr(s_param_yeq.selected));
        lvgl_lock();
        param_yeq_update_highlight(); param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER:
        if (s_param_yeq.on_equal) {
            /* Toggle enable for this pair */
            graph_state.param_enabled[pair] = !graph_state.param_enabled[pair];
            lvgl_lock(); param_yeq_update_highlight(); lvgl_unlock();
            return true;
        }
        __attribute__((fallthrough));
    case TOKEN_DOWN:
        if (s_param_yeq.selected < PARAM_YEQ_ROW_COUNT - 1) s_param_yeq.selected++;
        if (!s_param_yeq.on_equal)
            s_param_yeq.cursor_pos = strlen(param_eq_ptr(s_param_yeq.selected));
        lvgl_lock();
        param_yeq_update_highlight(); param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
        return true;

    default:
        break;
    }

    /* Character insertion — similar to handle_yeq_insertion but writes to param_x/y */
    const char *append = NULL;
    char num_buf[2] = {0, 0};

    switch (t) {
    case TOKEN_X_T:   append = "T";      break;  /* T is the parametric free variable */
    case TOKEN_0 ... TOKEN_9:
        num_buf[0] = (char)((t - TOKEN_0) + '0');
        append = num_buf;
        break;
    case TOKEN_DECIMAL: append = ".";     break;
    case TOKEN_NEG:     append = "-";     break;
    case TOKEN_ADD:     append = "+";     break;
    case TOKEN_SUB:     append = "-";     break;
    case TOKEN_MULT:    append = "*";     break;
    case TOKEN_DIV:     append = "/";     break;
    case TOKEN_POWER:   append = "^";     break;
    case TOKEN_L_PAR:   append = "(";     break;
    case TOKEN_R_PAR:   append = ")";     break;
    case TOKEN_PI:      append = "\xCF\x80"; break;  /* π UTF-8 */
    case TOKEN_SIN:     append = "sin(";  break;
    case TOKEN_COS:     append = "cos(";  break;
    case TOKEN_TAN:     append = "tan(";  break;
    case TOKEN_ASIN:    append = "sin\xEE\x80\x81("; break;
    case TOKEN_ACOS:    append = "cos\xEE\x80\x81("; break;
    case TOKEN_ATAN:    append = "tan\xEE\x80\x81("; break;
    case TOKEN_LN:      append = "ln(";   break;
    case TOKEN_LOG:     append = "log(";  break;
    case TOKEN_SQRT:    append = "\xE2\x88\x9A("; break;
    case TOKEN_ABS:     append = "abs(";  break;
    case TOKEN_E_X:     append = "exp(";  break;
    case TOKEN_EE:      append = "*10^";  break;
    case TOKEN_ANS:     append = "ANS";   break;
    default:
        lvgl_lock(); ui_update_status_bar(); param_yeq_cursor_update(); lvgl_unlock();
        return true;
    }

    if (append && !s_param_yeq.on_equal) {
        ExprUtil_InsertStr(eq, &eq_len, &s_param_yeq.cursor_pos,
                           (uint8_t)(sizeof(graph_state.param_x[0]) - 1), append);
        lvgl_lock();
        lv_label_set_text(ui_lbl_param_eq[s_param_yeq.selected], eq);
        param_yeq_reflow_rows(); param_yeq_cursor_update();
        lvgl_unlock();
    }

    lvgl_lock(); ui_update_status_bar(); param_yeq_cursor_update(); lvgl_unlock();
    return true;
}

/*---------------------------------------------------------------------------
 * Shared field-editor helper (P21)
 * Used by handle_range_mode and handle_zoom_factors_mode.
 *---------------------------------------------------------------------------*/

typedef struct {
    char    *buf;
    uint8_t  cap;        /* sizeof(buf) */
    uint8_t *len;
    uint8_t *cursor;
    void   (*update)(void);      /* refresh display labels */
    void   (*cursor_fn)(void);   /* reposition cursor widget */
} FieldEditor_t;

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

bool handle_zoom_mode(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_zoom.item_cursor > 0) {
            s_zoom.item_cursor--;
        } else if (s_zoom.scroll_offset > 0) {
            s_zoom.scroll_offset--;
        }
        lvgl_lock();
        ui_update_zoom_display();
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(s_zoom.scroll_offset + s_zoom.item_cursor) + 1 < ZOOM_ITEM_COUNT) {
            if (s_zoom.item_cursor < MENU_VISIBLE_ROWS - 1)
                s_zoom.item_cursor++;
            else if (s_zoom.scroll_offset + MENU_VISIBLE_ROWS < ZOOM_ITEM_COUNT)
                s_zoom.scroll_offset++;
        }
        lvgl_lock();
        ui_update_zoom_display();
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        zoom_execute_item((uint8_t)(s_zoom.scroll_offset + s_zoom.item_cursor + 1));
        return true;
    case TOKEN_CLEAR:
    case TOKEN_ZOOM:
        zoom_menu_reset();
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return true;
    case TOKEN_Y_EQUALS:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_GRAPH:
        zoom_menu_reset();
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_TRACE:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_TRACE);
        return true;
    case TOKEN_1 ... TOKEN_9: {
        uint8_t item = (uint8_t)(t - TOKEN_0);
        if (item <= ZOOM_ITEM_COUNT)
            zoom_execute_item(item);
        return true;
    }
    default:
        zoom_menu_reset();
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
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

bool handle_zbox_mode(Token_t t)
{
    switch (t) {
    case TOKEN_LEFT:
        if (s_zbox.px > 0) s_zbox.px--;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (s_zbox.px < GRAPH_W - 1) s_zbox.px++;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_UP:
        if (s_zbox.py > 0) s_zbox.py--;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s_zbox.py < GRAPH_H - 1) s_zbox.py++;
        lvgl_lock();
        Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        if (!s_zbox.corner1_set) {
            s_zbox.px1         = s_zbox.px;
            s_zbox.py1         = s_zbox.py;
            s_zbox.corner1_set = true;
            lvgl_lock();
            Graph_DrawZBox(s_zbox.px, s_zbox.py, s_zbox.px1, s_zbox.py1, s_zbox.corner1_set, angle_degrees);
            lvgl_unlock();
        } else {
            int32_t x_lo = s_zbox.px1 < s_zbox.px ? s_zbox.px1 : s_zbox.px;
            int32_t x_hi = s_zbox.px1 < s_zbox.px ? s_zbox.px  : s_zbox.px1;
            int32_t y_lo = s_zbox.py1 < s_zbox.py ? s_zbox.py1 : s_zbox.py;
            int32_t y_hi = s_zbox.py1 < s_zbox.py ? s_zbox.py  : s_zbox.py1;
            if (x_hi > x_lo && y_hi > y_lo) {
                float x_range   = graph_state.x_max - graph_state.x_min;
                float y_range   = graph_state.y_max - graph_state.y_min;
                float new_x_min = graph_state.x_min + (float)x_lo / (float)(GRAPH_W - 1) * x_range;
                float new_x_max = graph_state.x_min + (float)x_hi / (float)(GRAPH_W - 1) * x_range;
                float new_y_max = graph_state.y_max - (float)y_lo / (float)(GRAPH_H - 1) * y_range;
                float new_y_min = graph_state.y_max - (float)y_hi / (float)(GRAPH_H - 1) * y_range;
                graph_state.x_min = new_x_min;
                graph_state.x_max = new_x_max;
                graph_state.y_min = new_y_min;
                graph_state.y_max = new_y_max;
            }
            current_mode       = MODE_NORMAL;
            s_zbox.corner1_set = false;
            lvgl_lock();
            Graph_ClearTrace();
            Graph_Render(angle_degrees);
            lvgl_unlock();
        }
        return true;
    case TOKEN_CLEAR:
        s_zbox.corner1_set = false;
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return true;
    case TOKEN_ZOOM:
        s_zbox.corner1_set = false;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_GRAPH:
        s_zbox.corner1_set = false;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_Y_EQUALS:
        s_zbox.corner1_set = false;
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        s_zbox.corner1_set = false;
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_TRACE:
        s_zbox.corner1_set = false;
        nav_to(MODE_GRAPH_TRACE);
        return true;
    default:
        s_zbox.corner1_set = false;
        nav_to(MODE_NORMAL);
        return false; /* fall through to main switch */
    }
}

bool handle_trace_mode(Token_t t)
{
    float step = graph_state.param_mode
        ? graph_state.t_step
        : (graph_state.x_max - graph_state.x_min) / (float)(GRAPH_W - 1);
    if (step <= 0.0f) step = 0.1309f;

    switch (t) {
    case TOKEN_LEFT:
        if (graph_state.param_mode) {
            if (s_trace.x > graph_state.t_min) s_trace.x -= step;
        } else {
            if (s_trace.x > graph_state.x_min) s_trace.x -= step;
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (graph_state.param_mode) {
            if (s_trace.x < graph_state.t_max) s_trace.x += step;
        } else {
            if (s_trace.x < graph_state.x_max) s_trace.x += step;
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_UP:
        if (graph_state.param_mode) {
            /* Cycle backward through enabled parametric pairs */
            for (uint8_t i = 1; i <= GRAPH_NUM_PARAM; i++) {
                uint8_t idx = (s_trace.eq_idx + GRAPH_NUM_PARAM - i) % GRAPH_NUM_PARAM;
                if (graph_state.param_enabled[idx] &&
                    strlen(graph_state.param_x[idx]) > 0 &&
                    strlen(graph_state.param_y[idx]) > 0) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        } else {
            for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
                uint8_t idx = (s_trace.eq_idx + GRAPH_NUM_EQ - i) % GRAPH_NUM_EQ;
                if (strlen(graph_state.equations[idx]) > 0 && graph_state.enabled[idx]) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (graph_state.param_mode) {
            for (uint8_t i = 1; i <= GRAPH_NUM_PARAM; i++) {
                uint8_t idx = (s_trace.eq_idx + i) % GRAPH_NUM_PARAM;
                if (graph_state.param_enabled[idx] &&
                    strlen(graph_state.param_x[idx]) > 0 &&
                    strlen(graph_state.param_y[idx]) > 0) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        } else {
            for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
                uint8_t idx = (s_trace.eq_idx + i) % GRAPH_NUM_EQ;
                if (strlen(graph_state.equations[idx]) > 0 && graph_state.enabled[idx]) {
                    s_trace.eq_idx = idx;
                    break;
                }
            }
        }
        lvgl_lock();
        Graph_DrawTrace(s_trace.x, s_trace.eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_TRACE:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        Graph_ClearTrace();
        Graph_Render(angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_Y_EQUALS:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_GRAPH:
        lvgl_lock(); Graph_ClearTrace(); lvgl_unlock();
        nav_to(MODE_NORMAL);
        return true;
    default:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}
