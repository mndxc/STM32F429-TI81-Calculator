/**
 * @file    calculator_core.c
 * @brief   Calculator logic, UI management, and FreeRTOS task implementation.
 *
 * This module handles:
 *  - LVGL UI creation and updates
 *  - Token processing from the keypad queue
 *  - Calculator input buffer management
 */

#include "app_common.h"
#include "app_init.h"
#include "calc_engine.h"
#include "graph.h"
#include "persist.h"
#include "prgm.h"
#include "calc_internal.h"
#include "ui_matrix.h"
#include "ui_prgm.h"
#include "cmsis_os.h"
#include "lvgl.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define DISPLAY_W           320
#define DISPLAY_H           240

#define DISP_ROW_COUNT      8           /* Visible text rows on the main screen */
#define DISP_ROW_H          30          /* Pixels per row */
#define CURSOR_BLINK_MS     530         /* Cursor blink interval */

#define HISTORY_LINE_COUNT  32          /* Expression+result pairs stored in history */
#define MAX_EXPR_LEN        96          /* Supports up to 4 wrapped display rows */
#define MAX_RESULT_LEN      96   /* 32 for scalars; up to ~80 for 3×3 matrix rows */

/* Scrollable menu geometry */
#define ZOOM_ITEM_COUNT     8           /* Total ZOOM items */
#define MENU_VISIBLE_ROWS   7           /* Item rows visible below tab bar (8 rows - 1 tab) */
#define MODE_ROW_COUNT      8           /* Rows in the MODE screen */
#define MODE_MAX_COLS       11          /* Max options per MODE row (row 1 has 11) */
#define MATH_TAB_COUNT      4           /* MATH menu tabs: MATH NUM HYP PRB */
#define TEST_ITEM_COUNT     6           /* TEST menu items: = ≠ > ≥ < ≤ */

#define MATRIX_COUNT        3           /* Number of matrices: [A], [B], [C] */
#define MATRIX_MAX_DIM      CALC_MATRIX_MAX_DIM  /* alias — actual size in calc_engine.h */
#define MATRIX_LIST_VISIBLE 7                    /* visible cell rows in the list editor */
#define MATRIX_TAB_COUNT    2           /* MATRX and EDIT tabs */
#define MATRIX_MATRX_ITEMS  6           /* Items in the MATRX operations tab */
#define MATRIX_EDIT_ITEMS   3           /* Items in the EDIT tab */


/* Color scheme */
#define COLOR_BG            0x1A1A1A    /* Near black background */
#define COLOR_HISTORY_EXPR  0x888888    /* Grey for committed expressions */
#define COLOR_HISTORY_RES   0xFFFFFF    /* White for results */
#define COLOR_EXPR          0xCCCCCC    /* Light grey for current expression */
#define COLOR_2ND           0xF5A623    /* Amber for 2nd mode indicator */
#define COLOR_ALPHA         0x7ED321    /* Green for alpha mode indicator */

/*---------------------------------------------------------------------------
 * External references
 *---------------------------------------------------------------------------*/

extern const uint32_t TI81_LookupTable_Size;

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/


typedef struct {
    const char *display;
    const char *insert;
} MenuItem_t;

/*---------------------------------------------------------------------------
 * Private variables
 *---------------------------------------------------------------------------*/

/* LVGL objects — main display */
static lv_obj_t *disp_rows[DISP_ROW_COUNT]; /* Full-width text rows (Montserrat 24) */

/* Cursor blink state */
bool        cursor_visible = true;
static lv_timer_t *cursor_timer   = NULL;
static lv_obj_t   *cursor_box     = NULL;  /* Filled-block cursor rectangle */
static lv_obj_t   *cursor_inner   = NULL;  /* Character label inside cursor_box */

/* Per-screen cursor boxes (children of their respective overlay panels) */
static lv_obj_t   *yeq_cursor_box    = NULL;
static lv_obj_t   *yeq_cursor_inner  = NULL;
static lv_obj_t   *range_cursor_box  = NULL;
static lv_obj_t   *range_cursor_inner = NULL;

/* Graph screens */
static lv_obj_t *ui_graph_yeq_screen   = NULL;
static lv_obj_t *ui_lbl_yeq_name[GRAPH_NUM_EQ]; /* "Y1=" ... "Y4=" row labels */
static lv_obj_t *ui_lbl_yeq_eq[GRAPH_NUM_EQ];   /* Equation content per row */
static uint8_t   yeq_selected           = 0;    /* Which Y= row is being edited */
static lv_obj_t *ui_graph_zoom_screen   = NULL;
static lv_obj_t *ui_graph_range_screen  = NULL;
static lv_obj_t *ui_lbl_range_rows[7]   = {NULL}; /* One combined "Name=value" label per row */

/* Field names in TI-81 display order: Xmin Xmax Xscl Ymin Ymax Yscl Xres */
static const char * const range_field_names[7] = {
    "Xmin=", "Xmax=", "Xscl=", "Ymin=", "Ymax=", "Yscl=", "Xres="
};
static uint8_t   yeq_cursor_pos         = 0;    /* Insertion point in the Y= equation */


/* Styles */
static lv_style_t style_bg;

/* Calculator state */
char         expression[MAX_EXPR_LEN];
uint8_t      expr_len       = 0;
uint8_t      cursor_pos     = 0;  /* Insertion point, 0–expr_len */
static uint8_t      expr_chars_per_row = 22; /* Chars that fit on one display row; set at init */
bool         insert_mode            = false; /* false=overwrite (default), true=insert */
CalcMode_t   current_mode           = MODE_NORMAL;
CalcMode_t   return_mode            = MODE_NORMAL;
bool         angle_degrees          = true;

float ans = 0.0f;
bool         ans_is_matrix  = false; /* true when ans holds a matrix slot index */
static bool         sto_pending    = false;  /* True after STO — next alpha stores ans */

HistoryEntry_t history[HISTORY_LINE_COUNT];
uint8_t        history_count = 0;
    int8_t         history_recall_offset = 0; /* 0=not recalling; N=Nth-most-recent entry */

/* Graph state */
GraphState_t graph_state = {
    .equations = {{0}},
    .x_min    = -10.0f,
    .x_max    =  10.0f,
    .y_min    = -10.0f,
    .y_max    =  10.0f,
    .x_scl    =   1.0f,
    .y_scl    =   1.0f,
    .x_res    =   1.0f,
    .active   = false,
};

/* RANGE editor state */
static uint8_t  range_field_selected   = 0;   /* 0=Xmin 1=Xmax 2=Xscl 3=Ymin 4=Ymax 5=Yscl 6=Xres */
static char     range_field_buf[16]    = {0};
static uint8_t  range_field_len        = 0;
static uint8_t  range_field_cursor_pos = 0;   /* Insertion point within range_field_buf */

/* TRACE cursor state */
static float   trace_x      = 0.0f;
static uint8_t trace_eq_idx = 0;

/* ZBOX rubber-band zoom state */
static int32_t zbox_px          = GRAPH_W / 2;
static int32_t zbox_py          = GRAPH_H / 2;
static int32_t zbox_px1         = 0;
static int32_t zbox_py1         = 0;
static bool    zbox_corner1_set = false;

/* Mode to restore after a 2nd/ALPHA modifier is consumed */
// return_mode moved to global Calc state

/* ZOOM menu scroll state */
static uint8_t   zoom_scroll_offset = 0;
static uint8_t   zoom_item_cursor   = 0;  /* visible row of highlight cursor */
static lv_obj_t *zoom_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *zoom_scroll_ind[2];   /* [0]=top(↑), [1]=bottom(↓) — amber overlay */

/* ZOOM FACTORS sub-screen state */
static lv_obj_t *ui_graph_zoom_factors_screen = NULL;
static lv_obj_t *ui_lbl_zoom_factors_rows[2];
static lv_obj_t *zoom_factors_cursor_box   = NULL;
static lv_obj_t *zoom_factors_cursor_inner = NULL;
static float     zoom_x_fact          = 4.0f;  /* XFact default */
static float     zoom_y_fact          = 4.0f;  /* YFact default */
static uint8_t   zoom_factors_field   = 0;     /* 0=XFact, 1=YFact */
static char      zoom_factors_buf[16] = {0};
static uint8_t   zoom_factors_len     = 0;
static uint8_t   zoom_factors_cursor  = 0;
static const char * const zoom_factors_names[2] = {"XFact=", "YFact="};

/* ZOOM item data */
static const char * const zoom_item_names[ZOOM_ITEM_COUNT] = {
    "Box", "Zoom In", "Zoom Out", "Set Factors",
    "Square", "Standard", "Trig", "Integer"
};

/* MODE screen state */
lv_obj_t *ui_mode_screen                          = NULL;
static uint8_t   mode_row_selected                        = 0;
/* cursor position per row (which option is highlighted by arrow keys) */
static uint8_t   mode_cursor[MODE_ROW_COUNT]              = {0, 0, 1, 0, 0, 0, 0, 0};
/* committed selection per row (row 2 starts at 1 = Degree to match angle_degrees=true) */
static uint8_t   mode_committed[MODE_ROW_COUNT]           = {0, 0, 1, 0, 0, 0, 0, 0};

/* MODE screen option strings and positions */
static const char * const mode_options[MODE_ROW_COUNT][MODE_MAX_COLS] = {
    {"Normal", "Sci", "Eng", NULL},
    {"Float", "0","1","2","3","4","5","6","7","8","9"},
    {"Radian", "Degree", NULL},
    {"Func",   "Param",  NULL},
    {"Connected", "Dot", NULL},
    {"Sequential","Simul",NULL},
    {"Grid off","Grid on",NULL},
    {"Polar",  "Seq",    NULL},
};
static const uint8_t mode_option_count[MODE_ROW_COUNT] = {3, 11, 2, 2, 2, 2, 2, 2};
/* x-pixel start position for each option within its row */
static const int16_t mode_option_x[MODE_ROW_COUNT][MODE_MAX_COLS] = {
    {4, 112, 215, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 80, 104, 128, 152, 176, 200, 224, 248, 272, 296},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 165, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static lv_obj_t *mode_option_labels[MODE_ROW_COUNT][MODE_MAX_COLS];

/* MATH menu state */
static lv_obj_t *ui_math_screen                    = NULL;
static uint8_t   math_tab                          = 0;  /* 0=MATH 1=NUM 2=HYP 3=PRB */
static uint8_t   math_item_cursor                  = 0;  /* visible row of cursor */
static uint8_t   math_scroll_offset                = 0;
static CalcMode_t math_return_mode                 = MODE_NORMAL; /* mode to restore after MATH selection */
static lv_obj_t *math_tab_labels[MATH_TAB_COUNT];
static lv_obj_t *math_item_labels[MENU_VISIBLE_ROWS];
static lv_obj_t *math_scroll_ind[2];   /* [0]=top(↑), [1]=bottom(↓) — amber overlay */

/* MATH menu data */
static const char * const math_tab_names[MATH_TAB_COUNT] = {"MATH", "NUM", "HYP", "PRB"};
static const uint8_t math_tab_item_count[MATH_TAB_COUNT] = {8, 4, 6, 3};
/* Merged display+insert data for each MATH menu item */
static const MenuItem_t math_menu_items[MATH_TAB_COUNT][8] = {
    { /* MATH tab */
        {"R>P(",    "R>P("},
        {"P>R(",    "P>R("},
        {"^3",      "^3"},
        {"^(1/3)",  "^(1/3)"},
        {"!",       "!"},
        {"deg",     "\xC2\xB0"},
        {"rad",     "r"},
        {"nDeriv(", "nDeriv("},
    },
    { /* NUM tab */
        {"Round(",  "round("},
        {"IPart(",  "iPart("},
        {"FPart(",  "fPart("},
        {"Int(",    "int("},
        {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL},
    },
    { /* HYP tab */
        {"sinh(",   "sinh("},
        {"cosh(",   "cosh("},
        {"tanh(",   "tanh("},
        {"asinh(",  "asinh("},
        {"acosh(",  "acosh("},
        {"atanh(",  "atanh("},
        {NULL, NULL}, {NULL, NULL},
    },
    { /* PRB tab */
        {"Rand",    "rand"},
        {"nPr",     " nPr "},
        {"nCr",     " nCr "},
        {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL},
    },
};

/* TEST menu state */
static lv_obj_t  *ui_test_screen                   = NULL;
static uint8_t    test_item_cursor                  = 0;
static CalcMode_t test_return_mode                  = MODE_NORMAL;
static lv_obj_t  *test_title_label                  = NULL;
static lv_obj_t  *test_item_labels[TEST_ITEM_COUNT];

/* TEST menu data */
static const MenuItem_t test_menu_items[TEST_ITEM_COUNT] = {
    {"=",             "="},
    {"\xE2\x89\xA0",  "\xE2\x89\xA0"},   /* U+2260 ≠ */
    {">",             ">"},
    {"\xE2\x89\xA5",  "\xE2\x89\xA5"},   /* U+2265 ≥ */
    {"<",             "<"},
    {"\xE2\x89\xA4",  "\xE2\x89\xA4"},   /* U+2264 ≤ */
};

/* Matrix data lives in calc_matrices[] (calc_engine.c) — accessed via extern. */

/* MATRIX menu state */

static uint8_t      matrix_tab             = 0;   /* 0=MATRX, 1=EDIT */
static uint8_t      matrix_item_cursor     = 0;
static CalcMode_t   matrix_return_mode     = MODE_NORMAL;

/* MATRIX EDIT sub-screen state */




/*---------------------------------------------------------------------------
 * Forward declarations for display helpers defined later in this file
 *---------------------------------------------------------------------------*/

static void ui_update_zoom_display(void);
static void ui_update_mode_display(void);
static void ui_update_math_display(void);
static void ui_update_test_display(void);
static void zoom_factors_reset(void);
static void zoom_factors_load_field(void);
static void ui_update_zoom_factors_display(void);
static void zoom_factors_update_highlight(void);
static void zoom_factors_cursor_update(void);
static void matrix_edit_cursor_update(void);

/* Per-mode token handler forward declarations */
static bool handle_yeq_mode(Token_t t);
static bool handle_range_mode(Token_t t);
static bool handle_zoom_mode(Token_t t);
static bool handle_zoom_factors_mode(Token_t t);
static bool handle_zbox_mode(Token_t t);
static bool handle_trace_mode(Token_t t);
static bool handle_mode_screen(Token_t t);
static bool handle_math_menu(Token_t t);
static bool handle_test_menu(Token_t t);static bool handle_sto_pending(Token_t t);

/*---------------------------------------------------------------------------
 * Persistent storage helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief  Snapshot all saveable calculator state into @p out.
 *
 * Reads static-local variables directly (same translation unit).
 * graph_state.active is intentionally excluded — always boot with graph
 * hidden.
 */
void Calc_BuildPersistBlock(PersistBlock_t *out)
{
    memcpy(out->calc_variables, calc_variables, sizeof(calc_variables));
    out->ans = ans;
    memcpy(out->mode_committed, mode_committed, sizeof(mode_committed));
    out->zoom_x_fact = zoom_x_fact;
    out->zoom_y_fact = zoom_y_fact;

    /* Graph state — copy fields individually (skip active) */
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(out->equations[i], graph_state.equations[i],
               sizeof(graph_state.equations[i]));
    }
    out->x_min   = graph_state.x_min;
    out->x_max   = graph_state.x_max;
    out->y_min   = graph_state.y_min;
    out->y_max   = graph_state.y_max;
    out->x_scl   = graph_state.x_scl;
    out->y_scl   = graph_state.y_scl;
    out->x_res   = graph_state.x_res;
    out->grid_on = graph_state.grid_on ? 1u : 0u;

    /* Matrices [A], [B], [C] — save dimensions and flatten 6×6 data arrays */
    for (int m = 0; m < 3; m++) {
        out->matrix_rows[m] = calc_matrices[m].rows;
        out->matrix_cols[m] = calc_matrices[m].cols;
        memcpy(out->matrix_data[m], calc_matrices[m].data,
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }
}

/**
 * @brief  Restore calculator state from a previously loaded block.
 *
 * Re-derives calc_decimal_mode and angle_degrees from mode_committed via
 * their existing setters so behaviour is consistent with ENTER on MODE.
 */
void Calc_ApplyPersistBlock(const PersistBlock_t *in)
{
    memcpy(calc_variables, in->calc_variables, sizeof(calc_variables));
    ans = in->ans;
    memcpy(mode_committed, in->mode_committed, sizeof(mode_committed));

    /* Re-derives state that is computed from mode_committed */
    angle_degrees = (in->mode_committed[2] == 1);
    Calc_SetDecimalMode(in->mode_committed[1]);

    zoom_x_fact = in->zoom_x_fact;
    zoom_y_fact = in->zoom_y_fact;

    /* Restore graph state — leave active = false */
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(graph_state.equations[i], in->equations[i],
               sizeof(graph_state.equations[i]));
    }
    graph_state.x_min   = in->x_min;
    graph_state.x_max   = in->x_max;
    graph_state.y_min   = in->y_min;
    graph_state.y_max   = in->y_max;
    graph_state.x_scl   = in->x_scl;
    graph_state.y_scl   = in->y_scl;
    graph_state.x_res   = in->x_res;
    graph_state.grid_on = (in->grid_on != 0);

    /* Restore matrices [A], [B], [C] — dimensions and 6×6 data */
    for (int m = 0; m < 3; m++) {
        uint8_t rows = in->matrix_rows[m];
        uint8_t cols = in->matrix_cols[m];
        /* Clamp to valid range in case of corrupt data */
        calc_matrices[m].rows = (rows >= 1 && rows <= CALC_MATRIX_MAX_DIM) ? rows : 3;
        calc_matrices[m].cols = (cols >= 1 && cols <= CALC_MATRIX_MAX_DIM) ? cols : 3;
        memcpy(calc_matrices[m].data, in->matrix_data[m],
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }
}

/*---------------------------------------------------------------------------
 * LVGL thread safety helpers
 *---------------------------------------------------------------------------*/

void lvgl_lock(void) {
    if (xLVGL_Mutex != NULL)
        xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY);
}

void lvgl_unlock(void) {
    if (xLVGL_Mutex != NULL)
        xSemaphoreGive(xLVGL_Mutex);
}

/*---------------------------------------------------------------------------
 * UI initialisation
 *---------------------------------------------------------------------------*/

/* Initialises the three shared LVGL styles used across the calculator UI. */
static void ui_init_styles(void)
{
    /* Background */
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(COLOR_BG));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_pad_all(&style_bg, 0);
}

/* Creates a block cursor box (14×26 px) with an inner label child.
 * All cursor objects across every screen are built through this single function.
 * Change the size, font, or style properties here and all cursors update at once. */
void cursor_box_create(lv_obj_t *parent, bool start_hidden,
                               lv_obj_t **out_box, lv_obj_t **out_inner)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 14, 26);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *inner = lv_label_create(box);
    lv_obj_set_style_text_font(inner, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(inner, lv_color_hex(COLOR_BG), 0);
    lv_obj_center(inner);
    lv_label_set_text(inner, "");

    if (start_hidden)
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);

    *out_box   = box;
    *out_inner = inner;
}

/* Creates the main calculator screen: history rows, angle label, and cursor. */
static void ui_init_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &style_bg, 0);
    lv_obj_set_size(scr, DISPLAY_W, DISPLAY_H);

    /* DISP_ROW_COUNT full-width text rows — Montserrat 24, DISP_ROW_H px each */
    for (int i = 0; i < DISP_ROW_COUNT; i++) {
        disp_rows[i] = lv_label_create(scr);
        lv_obj_set_pos(disp_rows[i], 4, i * DISP_ROW_H + 2);
        lv_obj_set_width(disp_rows[i], DISPLAY_W - 8);
        lv_obj_set_style_text_font(disp_rows[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(disp_rows[i],
                                    lv_color_hex(COLOR_HISTORY_EXPR), 0);
        lv_label_set_long_mode(disp_rows[i], LV_LABEL_LONG_CLIP);
        lv_label_set_text(disp_rows[i], "");
    }

    /* Cursor block — filled rectangle that overlays the insertion point. */
    cursor_box_create(scr, false, &cursor_box, &cursor_inner);

    /* Measure how many monospaced characters fit on one display row. */
    uint16_t glyph_w = lv_font_get_glyph_width(&jetbrains_mono_24, 'X', 0);
    if (glyph_w > 0)
        expr_chars_per_row = (uint8_t)((DISPLAY_W - 8) / glyph_w);
}

/*---------------------------------------------------------------------------
 * Graph screen initialisation
 *---------------------------------------------------------------------------*/

/**
 * @brief Refreshes all seven RANGE row labels from the current graph_state values.
 *        Each label shows the combined "Name=value" text.  If a row is currently
 *        being edited (range_field_len > 0) its in-progress buffer is shown instead.
 *        Called whenever graph_state is modified or field selection changes.
 */
static void ui_update_range_display(void)
{
    const float committed[7] = {
        graph_state.x_min, graph_state.x_max, graph_state.x_scl,
        graph_state.y_min, graph_state.y_max, graph_state.y_scl,
        graph_state.x_res
    };
    char row_buf[32];
    char val_buf[16];
    for (int i = 0; i < 7; i++) {
        if (ui_lbl_range_rows[i] == NULL) continue;
        if (i == (int)range_field_selected && range_field_len > 0) {
            snprintf(row_buf, sizeof(row_buf), "%s%s",
                     range_field_names[i], range_field_buf);
        } else {
            snprintf(val_buf, sizeof(val_buf), "%.4g", committed[i]);
            snprintf(row_buf, sizeof(row_buf), "%s%s",
                     range_field_names[i], val_buf);
        }
        lv_label_set_text(ui_lbl_range_rows[i], row_buf);
    }
}

/* Creates a full-screen opaque black LVGL panel, hidden by default.
 * Used as the base for all overlay screens (MODE, MATH, TEST, MATRIX). */
lv_obj_t *screen_create(lv_obj_t *parent)
{
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_set_size(scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);
    return scr;
}

/* Creates the Y=, RANGE, ZOOM, and graph canvas screens (all hidden at startup). */
static void ui_init_graph_screens(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* --- Y= editor screen --- */
    ui_graph_yeq_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_graph_yeq_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_yeq_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_yeq_screen,
                               lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_graph_yeq_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_yeq_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);

    /* Four Y= equation rows — no heading, rows start at top (matches TI-81 original) */
    const char *eq_row_names[] = { "Y1=", "Y2=", "Y3=", "Y4=" };
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        int32_t row_y = 4 + i * 26;
        ui_lbl_yeq_name[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_name[i], 4, row_y);
        lv_obj_set_style_text_font(ui_lbl_yeq_name[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i],
                                     lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(ui_lbl_yeq_name[i], eq_row_names[i]);

        ui_lbl_yeq_eq[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_eq[i], 44, row_y);
        lv_obj_set_width(ui_lbl_yeq_eq[i], DISPLAY_W - 48);
        lv_obj_set_style_text_font(ui_lbl_yeq_eq[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_yeq_eq[i],
                                     lv_color_hex(0xFFFFFF), 0);
        lv_label_set_long_mode(ui_lbl_yeq_eq[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(ui_lbl_yeq_eq[i], "");
    }

    /* Y= cursor block — hidden until Y= screen is active */
    cursor_box_create(ui_graph_yeq_screen, true, &yeq_cursor_box, &yeq_cursor_inner);

    /* --- RANGE screen --- */
    ui_graph_range_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_graph_range_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_range_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_range_screen,
                               lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_graph_range_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_range_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);

    /* Tab bar — same font/style as items below, single "RANGE" tab (TI-81 style) */
    lv_obj_t *lbl_range_title = lv_label_create(ui_graph_range_screen);
    lv_obj_set_pos(lbl_range_title, 4, 4);
    lv_obj_set_style_text_font(lbl_range_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_range_title,
                                 lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_range_title, "RANGE");

    /* Seven field rows — combined "Name=value" labels, text set by ui_update_range_display() */
    for (int i = 0; i < 7; i++) {
        ui_lbl_range_rows[i] = lv_label_create(ui_graph_range_screen);
        lv_obj_set_pos(ui_lbl_range_rows[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(ui_lbl_range_rows[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_range_rows[i],
                                     lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(ui_lbl_range_rows[i], range_field_names[i]);
    }

    /* RANGE cursor block — hidden until RANGE screen is active */
    cursor_box_create(ui_graph_range_screen, true, &range_cursor_box, &range_cursor_inner);

    /* --- ZOOM menu screen --- */
    ui_graph_zoom_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_graph_zoom_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_zoom_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_zoom_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);

    /* ZOOM tab bar — same font as items per TI-81 style */
    lv_obj_t *lbl_zoom_title = lv_label_create(ui_graph_zoom_screen);
    lv_obj_set_pos(lbl_zoom_title, 4, 4);
    lv_obj_set_style_text_font(lbl_zoom_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_zoom_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_zoom_title, "ZOOM");

    /* MENU_VISIBLE_ROWS dynamic item labels — text set by ui_update_zoom_display() */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        zoom_item_labels[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(zoom_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_item_labels[i], lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(zoom_item_labels[i], "");
    }

    /* Scroll indicator overlays — amber arrow shown over the digit's colon slot */
    /* x=18 = 4(left margin) + 14(one char advance at 24px monospaced).
     * Opaque black bg covers the colon of the item beneath the arrow. */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        zoom_scroll_ind[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(zoom_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_scroll_ind[i], lv_color_hex(0xFFAA00), 0);
        lv_obj_set_style_bg_color(zoom_scroll_ind[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(zoom_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(zoom_scroll_ind[i], 0, 0);
        lv_label_set_text(zoom_scroll_ind[i], "");
        lv_obj_add_flag(zoom_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* --- ZOOM FACTORS sub-screen --- */
    ui_graph_zoom_factors_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_graph_zoom_factors_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_zoom_factors_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_zoom_factors_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_graph_zoom_factors_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_zoom_factors_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_zf_title = lv_label_create(ui_graph_zoom_factors_screen);
    lv_obj_set_pos(lbl_zf_title, 4, 4);
    lv_obj_set_style_text_font(lbl_zf_title, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(lbl_zf_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_zf_title, "ZOOM FACTORS");

    for (int i = 0; i < 2; i++) {
        ui_lbl_zoom_factors_rows[i] = lv_label_create(ui_graph_zoom_factors_screen);
        lv_obj_set_pos(ui_lbl_zoom_factors_rows[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(ui_lbl_zoom_factors_rows[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(ui_lbl_zoom_factors_rows[i], lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(ui_lbl_zoom_factors_rows[i], zoom_factors_names[i]);
    }

    cursor_box_create(ui_graph_zoom_factors_screen, true,
                      &zoom_factors_cursor_box, &zoom_factors_cursor_inner);

    /* Init graph canvas */
    Graph_Init(scr);
}

/* Creates the MODE settings screen (hidden at startup). */
static void ui_init_mode_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_mode_screen = screen_create(scr);

    memset(mode_option_labels, 0, sizeof(mode_option_labels));

    for (int r = 0; r < MODE_ROW_COUNT; r++) {
        int n = mode_option_count[r];
        for (int c = 0; c < n; c++) {
            lv_obj_t *lbl = lv_label_create(ui_mode_screen);
            lv_obj_set_pos(lbl, mode_option_x[r][c], r * 30);
            lv_obj_set_style_text_font(lbl, &jetbrains_mono_24, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);
            lv_label_set_text(lbl, mode_options[r][c]);
            mode_option_labels[r][c] = lbl;
        }
    }
}

/* Creates the MATH/NUM/HYP/PRB menu screen (hidden at startup). */
static void ui_init_math_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_math_screen = screen_create(scr);

    /* Tab bar: 4 tab names at fixed x positions */
    static const int16_t tab_x[MATH_TAB_COUNT] = {4, 80, 140, 205};
    for (int i = 0; i < MATH_TAB_COUNT; i++) {
        math_tab_labels[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_tab_labels[i], tab_x[i], 4);
        lv_obj_set_style_text_font(math_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_tab_labels[i], lv_color_hex(0x666666), 0);
        lv_label_set_text(math_tab_labels[i], math_tab_names[i]);
    }

    /* MENU_VISIBLE_ROWS dynamic item labels — text set by ui_update_math_display() */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        math_item_labels[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(math_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_item_labels[i], lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(math_item_labels[i], "");
    }

    /* Scroll indicator overlays — amber arrow, opaque bg covers colon beneath */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        math_scroll_ind[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(math_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_scroll_ind[i], lv_color_hex(0xFFAA00), 0);
        lv_obj_set_style_bg_color(math_scroll_ind[i], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(math_scroll_ind[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(math_scroll_ind[i], 0, 0);
        lv_label_set_text(math_scroll_ind[i], "");
        lv_obj_add_flag(math_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* Creates the TEST menu screen (hidden at startup). */
static void ui_init_test_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_test_screen = screen_create(scr);

    /* "TEST" title at the top row */
    test_title_label = lv_label_create(ui_test_screen);
    lv_obj_set_pos(test_title_label, 4, 4);
    lv_obj_set_style_text_font(test_title_label, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(test_title_label, lv_color_hex(0xFFFF00), 0);
    lv_label_set_text(test_title_label, "TEST");

    /* Item labels — text set by ui_update_test_display() */
    for (int i = 0; i < TEST_ITEM_COUNT; i++) {
        test_item_labels[i] = lv_label_create(ui_test_screen);
        lv_obj_set_pos(test_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(test_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(test_item_labels[i], lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(test_item_labels[i], "");
    }
}


/*---------------------------------------------------------------------------
 * UI update functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Generic block-cursor placement.
 *
 * Determines cursor visibility, color, and inner character from global
 * calculator state (current_mode, sto_pending, insert_mode, cursor_visible),
 * then moves cbox to the pixel position of char_pos within row_label.
 *
 * Cursor inner-char key:
 *   STO pending  → green 'A'
 *   MODE_2ND     → amber '^'
 *   MODE_ALPHA/LOCK → green 'A'
 *   insert_mode  → grey 'I'  (overwrite = default: blank grey block)
 *
 * @param cbox       The cursor rectangle LVGL object to move/show/hide.
 * @param cinner     The label child of cbox that shows the inner character.
 * @param row_label  The LVGL label whose text provides the reference position.
 * @param char_pos   Character index within row_label at which to place cbox.
 */
void cursor_place(lv_obj_t *cbox, lv_obj_t *cinner,
                         lv_obj_t *row_label, uint32_t char_pos)
{
    if (cbox == NULL) return;

    bool show;
    lv_color_t box_color;
    const char *inner_text;

    if (sto_pending) {
        show       = cursor_visible;
        box_color  = lv_color_hex(COLOR_ALPHA);
        inner_text = "A";
    } else switch (current_mode) {
        case MODE_2ND:
            show       = cursor_visible;
            box_color  = lv_color_hex(COLOR_2ND);
            inner_text = "^";
            break;
        case MODE_ALPHA:
        case MODE_ALPHA_LOCK:
            show       = cursor_visible;
            box_color  = lv_color_hex(COLOR_ALPHA);
            inner_text = "A";
            break;
        default:
            show       = cursor_visible;
            box_color  = lv_color_hex(0xCCCCCC);
            inner_text = insert_mode ? "I" : "";
            break;
    }

    if (!show) {
        lv_obj_add_flag(cbox, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_point_t pos;
    lv_label_get_letter_pos(row_label, char_pos, &pos);

    int32_t lx = lv_obj_get_x(row_label);
    int32_t ly = lv_obj_get_y(row_label);
    lv_obj_set_pos(cbox, lx + pos.x, ly + pos.y);

    lv_obj_set_style_bg_color(cbox, box_color, 0);
    lv_label_set_text(cinner, inner_text);
    lv_obj_clear_flag(cbox, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Positions the main calculator screen block cursor.
 *
 * @param row_label  The LVGL label containing the current expression.
 * @param char_pos   Character index at which to place the cursor.
 */
static void cursor_update(lv_obj_t *row_label, uint32_t char_pos)
{
    cursor_place(cursor_box, cursor_inner, row_label, char_pos);
}

/**
 * @brief Returns the byte length of the UTF-8 character starting at s[0].
 *        Returns 1 for ASCII or any invalid/continuation byte so iteration
 *        never gets stuck.  Returns 0 only at the null terminator.
 */
/**
 * @brief Returns 3 if the 3 bytes immediately before pos in buf form "[A]",
 *        "[B]", or "[C]" (a matrix token); otherwise returns 0.
 */
static uint8_t matrix_token_size_before(const char *buf, uint8_t pos)
{
    if (pos < 3) return 0;
    if (buf[pos - 3] == '[' && buf[pos - 1] == ']' &&
        (buf[pos - 2] == 'A' || buf[pos - 2] == 'B' || buf[pos - 2] == 'C'))
        return 3;
    return 0;
}

/**
 * @brief Returns 3 if the 3 bytes at pos in buf form "[A]", "[B]", or "[C]"
 *        (a matrix token); otherwise returns 0.
 */
static uint8_t matrix_token_size_at(const char *buf, uint8_t pos, uint8_t len)
{
    if (pos + 3 > len) return 0;
    if (buf[pos] == '[' && buf[pos + 2] == ']' &&
        (buf[pos + 1] == 'A' || buf[pos + 1] == 'B' || buf[pos + 1] == 'C'))
        return 3;
    return 0;
}

static uint8_t utf8_char_size(const char *s)
{
    uint8_t c = (uint8_t)s[0];
    if (c == 0)    return 0;
    if (c < 0x80)  return 1;   /* ASCII */
    if (c < 0xC0)  return 1;   /* Continuation byte — skip safely */
    if (c < 0xE0)  return 2;   /* 2-byte sequence (e.g. π U+03C0) */
    if (c < 0xF0)  return 3;   /* 3-byte sequence (e.g. √ U+221A) */
    return 4;                   /* 4-byte sequence */
}

/**
 * @brief Converts a byte cursor position to a glyph (character) index.
 *        lv_label_get_letter_pos expects a glyph index; yeq_cursor_pos is
 *        stored as a byte offset so this conversion is required whenever
 *        the equation may contain multi-byte UTF-8 characters (e.g. √, π).
 */
static uint32_t utf8_byte_to_glyph(const char *s, uint32_t byte_idx)
{
    uint32_t glyph = 0;
    uint32_t i = 0;
    while (i < byte_idx && s[i] != '\0') {
        uint8_t sz = utf8_char_size(&s[i]);
        i += sz;
        glyph++;
    }
    return glyph;
}

/**
 * @brief Reflows Y= row positions after equation text changes.
 *        Rows are stacked top-to-bottom; each row's height expands when its
 *        equation wraps to multiple lines.  Must be called under lvgl_lock().
 */
static void yeq_reflow_rows(void)
{
    lv_obj_update_layout(ui_graph_yeq_screen);
    int32_t y = 4;
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        lv_obj_set_pos(ui_lbl_yeq_name[i], 4,  y);
        lv_obj_set_pos(ui_lbl_yeq_eq[i],  44,  y);
        int32_t h = lv_obj_get_height(ui_lbl_yeq_eq[i]);
        if (h < 26) h = 26; /* minimum one-line height */
        y += h + 2;
    }
}

/**
 * @brief Positions the Y= equation editor cursor over the active equation row.
 *        Must be called under lvgl_lock() (or from cursor_timer_cb).
 */
static void yeq_cursor_update(void)
{
    if (yeq_cursor_box == NULL || ui_lbl_yeq_eq[yeq_selected] == NULL) return;
    const char *txt = lv_label_get_text(ui_lbl_yeq_eq[yeq_selected]);
    uint32_t glyph_pos = utf8_byte_to_glyph(txt, yeq_cursor_pos);
    cursor_place(yeq_cursor_box, yeq_cursor_inner,
                 ui_lbl_yeq_eq[yeq_selected], glyph_pos);
}

/**
 * @brief Positions the RANGE field editor cursor over the active field label.
 *        The cursor sits at range_field_cursor_pos characters after the field
 *        name prefix (e.g., "Xmin=").
 *        Must be called under lvgl_lock() (or from cursor_timer_cb).
 */
static void range_cursor_update(void)
{
    if (range_cursor_box == NULL || ui_lbl_range_rows[range_field_selected] == NULL) return;
    uint32_t char_pos = (uint32_t)strlen(range_field_names[range_field_selected])
                      + range_field_cursor_pos;
    cursor_place(range_cursor_box, range_cursor_inner,
                 ui_lbl_range_rows[range_field_selected], char_pos);
}

/**
 * @brief Redraws all DISP_ROW_COUNT display rows from the history buffer
 *        and the current input expression.
 *
 * Each history entry occupies ceil(expr_len/cpr) expression sub-rows + 1 result
 * row, so long committed expressions wrap just as the active expression does.
 * Current expression sub-rows follow immediately after all history rows.
 */
/**
 * @brief Format a CalcResult_t into a displayable string.
 *
 * For scalar results: delegates to Calc_FormatResult.
 * For matrix results: produces a newline-separated grid "[a b c]\n[d e f]\n[g h i]".
 * Updates *ans_out only on successful scalar results.
 */
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size,
                                float *ans_out)
{
    memset(buf, 0, (size_t)buf_size);
    if (r->error != CALC_OK) {
        strncpy(buf, r->error_msg, (size_t)(buf_size - 1));
        buf[buf_size - 1] = '\0';
        return;
    }
    if (r->has_matrix) {
        const CalcMatrix_t *m = &calc_matrices[r->matrix_idx];
        int pos = 0;
        for (int row = 0; row < (int)m->rows && pos < buf_size - 2; row++) {
            if (row > 0 && pos < buf_size - 1) buf[pos++] = '\n';
            if (pos < buf_size - 1) buf[pos++] = '[';
            for (int col = 0; col < (int)m->cols; col++) {
                char cell[12];
                Calc_FormatResult(m->data[row][col], cell, sizeof(cell));
                /* Limit cell width to 8 chars to keep lines short */
                cell[8] = '\0';
                if (col > 0 && pos < buf_size - 1) buf[pos++] = ' ';
                int cl = (int)strlen(cell);
                if (pos + cl < buf_size - 1) {
                    memcpy(&buf[pos], cell, (size_t)cl);
                    pos += cl;
                }
            }
            if (pos < buf_size - 1) buf[pos++] = ']';
        }
        buf[pos] = '\0';
        ans_is_matrix = true;
        if (ans_out) *ans_out = (float)r->matrix_idx;
    } else {
        Calc_FormatResult(r->value, buf, (uint8_t)buf_size);
        ans_is_matrix = false;
        if (ans_out) *ans_out = r->value;
    }
}

/** Returns the number of display lines a result string occupies (newline-separated). */
static int count_result_lines(const char *result)
{
    int n = 1;
    for (; *result; result++)
        if (*result == '\n') n++;
    return n;
}

/**
 * @brief Copy line @p line_idx (0-based) from a newline-separated string into buf.
 *
 * Returns false if line_idx exceeds the number of lines in src.
 */
static bool get_result_line(const char *src, int line_idx, char *buf, int buf_size)
{
    const char *p = src;
    for (int k = 0; k < line_idx; k++) {
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        else return false;  /* line_idx out of range */
    }
    int len = 0;
    while (p[len] && p[len] != '\n' && len < buf_size - 1) len++;
    memcpy(buf, p, (size_t)len);
    buf[len] = '\0';
    return true;
}

void ui_refresh_display(void)
{
    if (disp_rows[0] == NULL) return;

    int cpr = (int)expr_chars_per_row;
    int expr_rows = (expr_len == 0) ? 1 : (expr_len + cpr - 1) / cpr;

    /* Number of history entries visible (circular buffer cap) */
    int num_entries = ((int)history_count < HISTORY_LINE_COUNT)
                      ? (int)history_count : HISTORY_LINE_COUNT;

    /* Total display lines consumed by history (variable per entry).
       Matrix results occupy multiple result lines (one per matrix row). */
    int total_history_lines = 0;
    for (int d = 0; d < num_entries; d++) {
        int idx = (int)((history_count - num_entries + d) % HISTORY_LINE_COUNT);
        int elen = (int)strlen(history[idx].expression);
        int erows = (elen + cpr - 1) / cpr;
        int rlines = count_result_lines(history[idx].result);
        total_history_lines += erows + rlines;
    }

    int total = total_history_lines + expr_rows;
    int start = (total > DISP_ROW_COUNT) ? (total - DISP_ROW_COUNT) : 0;

    /* Which expression sub-row holds the cursor, and column within that row */
    int cursor_expr_row = (int)cursor_pos / cpr;
    int cursor_col      = (int)cursor_pos % cpr;

    for (int row = 0; row < DISP_ROW_COUNT; row++) {
        int li = start + row;

        if (li >= total) {
            lv_label_set_text(disp_rows[row], "");
            continue;
        }

        if (li < total_history_lines) {
            /* Walk history entries to find which owns line li */
            int line = 0;
            for (int d = 0; d < num_entries; d++) {
                int idx = (int)((history_count - num_entries + d) % HISTORY_LINE_COUNT);
                int elen  = (int)strlen(history[idx].expression);
                int erows = (elen + cpr - 1) / cpr;
                int rlines = count_result_lines(history[idx].result);

                if (li < line + erows) {
                    /* Expression sub-row */
                    int er = li - line;
                    int char_start = er * cpr;
                    int char_end = char_start + cpr;
                    if (char_end > elen) char_end = elen;
                    int seg_len = char_end - char_start;
                    if (seg_len < 0) seg_len = 0;
                    char row_buf[MAX_EXPR_LEN + 1];
                    memcpy(row_buf, history[idx].expression + char_start, (size_t)seg_len);
                    row_buf[seg_len] = '\0';
                    lv_obj_set_style_text_color(disp_rows[row],
                                                lv_color_hex(COLOR_HISTORY_EXPR), 0);
                    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
                    lv_label_set_text(disp_rows[row], row_buf);
                    break;
                }
                line += erows;

                if (li < line + rlines) {
                    /* Result line for this history entry (may be multi-line for matrices) */
                    int result_line = li - line;
                    char rbuf[MAX_RESULT_LEN];
                    get_result_line(history[idx].result, result_line, rbuf, sizeof(rbuf));
                    lv_obj_set_style_text_color(disp_rows[row],
                                                lv_color_hex(COLOR_HISTORY_RES), 0);
                    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_RIGHT, 0);
                    lv_label_set_text(disp_rows[row], rbuf);
                    break;
                }
                line += rlines;
            }
        } else {
            /* Current expression sub-row */
            int expr_line  = li - total_history_lines;
            int char_start = expr_line * cpr;
            int char_end   = char_start + cpr;
            if (char_end > (int)expr_len) char_end = (int)expr_len;
            int seg_len = char_end - char_start;
            if (seg_len < 0) seg_len = 0;

            char row_buf[MAX_EXPR_LEN + 1];
            memcpy(row_buf, &expression[char_start], (size_t)seg_len);
            row_buf[seg_len] = '\0';

            lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_EXPR), 0);
            lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(disp_rows[row], row_buf);

            /* Place cursor on the sub-row that contains cursor_pos */
            if (expr_line == cursor_expr_row)
                cursor_update(disp_rows[row], (uint32_t)cursor_col);
        }
    }

    /* If the cursor's sub-row scrolled off-screen, hide the cursor */
    if (total_history_lines + cursor_expr_row < start && cursor_box != NULL)
        lv_obj_add_flag(cursor_box, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief LVGL timer callback — blinks the cursor every CURSOR_BLINK_MS.
 *
 * Called from lv_task_handler() — DefaultTask already holds the LVGL mutex.
 * Do NOT call lvgl_lock() here or it will deadlock.
 *
 * Updates the main screen cursor and, when an overlay input screen is active,
 * also blinks the corresponding overlay cursor box.
 */
static void cursor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    cursor_visible = !cursor_visible;
    ui_refresh_display();
    /* Blink the overlay-screen cursor based on visibility, not current_mode,
     * so it keeps blinking during transient modifier modes (MODE_2ND/ALPHA). */
    if (ui_graph_yeq_screen != NULL &&
        !lv_obj_has_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN))
        yeq_cursor_update();
    else if (ui_graph_range_screen != NULL &&
             !lv_obj_has_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN))
        range_cursor_update();
    else if (ui_graph_zoom_factors_screen != NULL &&
             !lv_obj_has_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN))
        zoom_factors_cursor_update();
    else if (ui_matrix_edit_screen != NULL &&
             !lv_obj_has_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN))
        matrix_edit_cursor_update();
    else if (ui_prgm_editor_screen != NULL && !lv_obj_has_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_editor_cursor_update();
    else if (ui_prgm_new_screen != NULL && !lv_obj_has_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_new_cursor_update();
}

/**
 * @brief Refreshes the cursor on all screens to reflect the current mode.
 *        The cursor block color and inner character encode 2nd/ALPHA/STO state.
 *        Checks screen visibility directly because current_mode may already be
 *        MODE_2ND/MODE_ALPHA when called from Process_Hardware_Key.
 */
static void ui_update_status_bar(void)
{
    cursor_visible = true;
    ui_refresh_display();
    if (ui_graph_yeq_screen != NULL &&
        !lv_obj_has_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN))
        yeq_cursor_update();
    else if (ui_graph_range_screen != NULL &&
             !lv_obj_has_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN))
        range_cursor_update();
    else if (ui_graph_zoom_factors_screen != NULL &&
             !lv_obj_has_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN))
        zoom_factors_cursor_update();
    else if (ui_matrix_edit_screen != NULL &&
             !lv_obj_has_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN))
        matrix_edit_cursor_update();
    else if (ui_prgm_editor_screen != NULL && !lv_obj_has_flag(ui_prgm_editor_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_editor_cursor_update();
    else if (ui_prgm_new_screen != NULL && !lv_obj_has_flag(ui_prgm_new_screen, LV_OBJ_FLAG_HIDDEN))
        prgm_new_cursor_update();
}

/**
 * @brief Refreshes the display with the current expression and cursor.
 *        Called after every keypress that modifies the expression.
 */
void Update_Calculator_Display(void)
{
    if (disp_rows[0] == NULL) return;
    lvgl_lock();
    ui_refresh_display();
    lvgl_unlock();
}

/**
 * @brief If the expression is empty, prepend "ANS" and set cursor_pos to 3.
 *
 * Mirrors TI-81 behaviour where pressing a binary operator on a fresh line
 * automatically inserts ANS as the left-hand operand.  cursor_pos is also
 * advanced so subsequent insertions append after "ANS" rather than before it.
 */
static void expr_prepend_ans_if_empty(void)
{
    if (expr_len == 0 && MAX_EXPR_LEN > 3) {
        memcpy(expression, "ANS", 4);
        expr_len   = 3;
        cursor_pos = 3;
    }
}

/**
 * @brief Inserts or overwrites a single character at cursor_pos.
 *
 * In overwrite mode (insert_mode == false) and cursor is not at the end of
 * the expression, the character at cursor_pos is replaced and the cursor
 * advances.  In insert mode, or when at the end, characters shift right.
 */
static void expr_insert_char(char c)
{
    if (!insert_mode && cursor_pos < expr_len) {
        /* Overwrite: remove all bytes of the current char, then write c.
         * Treat [A]/[B]/[C] as atomic (3 bytes); also handle multi-byte UTF-8
         * (e.g. ≥) to avoid orphaned continuation bytes. */
        uint8_t cur_size = matrix_token_size_at(expression, cursor_pos, expr_len);
        if (!cur_size) cur_size = utf8_char_size(&expression[cursor_pos]);
        memmove(&expression[cursor_pos + 1],
                &expression[cursor_pos + cur_size],
                expr_len - cursor_pos - cur_size + 1);
        expression[cursor_pos] = c;
        expr_len = expr_len - cur_size + 1;
        cursor_pos++;
    } else {
        /* Insert: shift tail right, then write */
        if (expr_len + 1 > MAX_EXPR_LEN) return; // Check for buffer overflow
        memmove(&expression[cursor_pos + 1], &expression[cursor_pos],
                expr_len - cursor_pos + 1);
        expression[cursor_pos] = c;
        expr_len++;
        cursor_pos++;
    }
}

/**
 * @brief Inserts a string at cursor_pos and advances the cursor by its length.
 */
static void expr_insert_str(const char *s)
{
    uint8_t slen = (uint8_t)strlen(s);
    if (expr_len + slen >= MAX_EXPR_LEN) return;
    memmove(&expression[cursor_pos + slen], &expression[cursor_pos],
            expr_len - cursor_pos + 1);
    memcpy(&expression[cursor_pos], s, slen);
    expr_len   += slen;
    cursor_pos += slen;
}

/**
 * @brief Deletes the character immediately before cursor_pos (backspace).
 */
void expr_delete_at_cursor(void)
{
    if (cursor_pos == 0) return;
    /* Treat [A]/[B]/[C] as atomic — delete all 3 bytes at once. */
    uint8_t mat = matrix_token_size_before(expression, cursor_pos);
    if (mat) {
        uint8_t start = cursor_pos - mat;
        memmove(&expression[start], &expression[cursor_pos],
                expr_len - cursor_pos + 1);
        expr_len  -= mat;
        cursor_pos = start;
        return;
    }
    /* Find the start byte of the UTF-8 character that ends just before cursor.
     * Without this, deleting a 3-byte sequence (e.g. ≥) would only remove the
     * last byte, leaving two orphaned continuation bytes in the expression. */
    uint8_t start = cursor_pos - 1;
    while (start > 0 && ((uint8_t)expression[start] & 0xC0) == 0x80)
        start--;
    uint8_t char_bytes = cursor_pos - start;
    memmove(&expression[start], &expression[cursor_pos],
            expr_len - cursor_pos + 1);
    expr_len  -= char_bytes;
    cursor_pos = start;
}

/**
 * @brief Refreshes the display after history changes.
 */
void ui_update_history(void)
{
    ui_refresh_display();
}

/*---------------------------------------------------------------------------
 * Y= editor helpers
 *---------------------------------------------------------------------------*/

/* Highlights the active Y= row label in yellow; all others are white. */
static void yeq_update_highlight(void)
{
    for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
        lv_color_t col = (i == yeq_selected) ? lv_color_hex(0xFFFF00)
                                             : lv_color_hex(0xFFFFFF);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i], col, 0);
    }
}

/*---------------------------------------------------------------------------
 * Zoom preset helper
 *---------------------------------------------------------------------------*/

/* Applies one of the five fixed TI-81 ZOOM presets to graph_state.
 * preset: 1=ZStandard, 2=ZTrig, 3=ZDecimal, 4=ZSquare, 5=ZInteger.
 * ZBox (the sixth ZOOM option) is handled separately in Execute_Token(). */
static void apply_zoom_preset(uint8_t preset)
{
    switch (preset) {
    case 1: /* ZStandard */
        graph_state.x_min = -10.0f; graph_state.x_max =  10.0f; graph_state.x_scl = 1.0f;
        graph_state.y_min = -10.0f; graph_state.y_max =  10.0f; graph_state.y_scl = 1.0f;
        break;
    case 2: /* ZTrig — ±2π x, ±4 y, x scale = π/2 */
        graph_state.x_min = -6.2832f; graph_state.x_max = 6.2832f; graph_state.x_scl = 1.5708f;
        graph_state.y_min =  -4.0f;   graph_state.y_max = 4.0f;    graph_state.y_scl = 1.0f;
        break;
    case 3: /* ZDecimal — each pixel = 0.1 unit */
        graph_state.x_min = -4.7f; graph_state.x_max = 4.7f; graph_state.x_scl = 0.5f;
        graph_state.y_min = -3.1f; graph_state.y_max = 3.1f; graph_state.y_scl = 0.5f;
        break;
    case 4: /* ZSquare — expand shorter axis so pixels are square */
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
    case 5: /* ZInteger — each pixel = 1 unit */
        graph_state.x_min = -160.0f; graph_state.x_max = 159.0f; graph_state.x_scl = 10.0f;
        graph_state.y_min = -110.0f; graph_state.y_max = 109.0f; graph_state.y_scl = 10.0f;
        break;
    default: /* Unknown preset — apply ZStandard as safe fallback */
        graph_state.x_min = -10.0f; graph_state.x_max =  10.0f; graph_state.x_scl = 1.0f;
        graph_state.y_min = -10.0f; graph_state.y_max =  10.0f; graph_state.y_scl = 1.0f;
        break;
    }
}

/*---------------------------------------------------------------------------
 * ZOOM display helper
 *---------------------------------------------------------------------------*/

/* Redraws the 7 visible ZOOM item rows based on zoom_scroll_offset.
 * Must be called under lvgl_lock(). */
static void ui_update_zoom_display(void)
{
    /* Hide both scroll indicators; re-shown below if needed */
    lv_obj_add_flag(zoom_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(zoom_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = zoom_scroll_offset + i;
        if (idx >= ZOOM_ITEM_COUNT) {
            lv_label_set_text(zoom_item_labels[i], "");
            continue;
        }
        bool more_below = (zoom_scroll_offset + MENU_VISIBLE_ROWS < ZOOM_ITEM_COUNT)
                          && (i == MENU_VISIBLE_ROWS - 1);
        bool more_above = (zoom_scroll_offset > 0) && (i == 0);
        char buf[32];
        if (more_below) {
            /* Space holds the arrow's slot; amber ↓ overlay drawn on top */
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
        lv_color_t col = (i == (int)zoom_item_cursor)
            ? lv_color_hex(0xFFFF00)
            : lv_color_hex(0xFFFFFF);
        lv_obj_set_style_text_color(zoom_item_labels[i], col, 0);
        lv_label_set_text(zoom_item_labels[i], buf);
    }
}

/*---------------------------------------------------------------------------
 * Graph/menu state helpers
 *---------------------------------------------------------------------------*/

/* Returns the index of the first non-empty Y= equation, or 0 if all are empty. */
static uint8_t find_first_active_eq(void)
{
    for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
        if (strlen(graph_state.equations[i]) > 0)
            return i;
    }
    return 0;
}

/* Resets the RANGE editor input buffer and field state to defaults. */
static void range_field_reset(void)
{
    range_field_selected   = 0;
    range_field_len        = 0;
    range_field_buf[0]     = '\0';
    range_field_cursor_pos = 0;
}

/* Pre-populates range_field_buf from the stored graph_state value for the
 * currently selected field so LEFT/RIGHT can edit the existing value. */
static void range_load_field(void)
{
    float vals[7] = {
        graph_state.x_min, graph_state.x_max, graph_state.x_scl,
        graph_state.y_min, graph_state.y_max, graph_state.y_scl,
        graph_state.x_res
    };
    snprintf(range_field_buf, sizeof(range_field_buf), "%.4g", vals[range_field_selected]);
    range_field_len        = (uint8_t)strlen(range_field_buf);
    range_field_cursor_pos = 0;
}

/* Resets the ZOOM menu scroll and cursor position to defaults. */
static void zoom_menu_reset(void)
{
    zoom_scroll_offset = 0;
    zoom_item_cursor   = 0;
}

/*---------------------------------------------------------------------------
 * ZOOM action executor
 *---------------------------------------------------------------------------*/

/* Executes ZOOM menu item item_num (1-indexed).
 * Hides the ZOOM screen and performs the associated action. */
static void zoom_execute_item(uint8_t item_num)
{
    zoom_menu_reset();
    switch (item_num) {
    case 1: /* Box — enter rubber-band zoom */
        zbox_px = GRAPH_W / 2; zbox_py = GRAPH_H / 2; zbox_corner1_set = false;
        current_mode = MODE_GRAPH_ZBOX;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_DrawZBox(zbox_px, zbox_py, 0, 0, false, angle_degrees);
        lvgl_unlock();
        break;
    case 2: /* Zoom In — shrink window by XFact/YFact around centre */
        {
            float xc = (graph_state.x_min + graph_state.x_max) * 0.5f;
            float yc = (graph_state.y_min + graph_state.y_max) * 0.5f;
            float xh = (graph_state.x_max - graph_state.x_min) / (2.0f * zoom_x_fact);
            float yh = (graph_state.y_max - graph_state.y_min) / (2.0f * zoom_y_fact);
            graph_state.x_min = xc - xh; graph_state.x_max = xc + xh;
            graph_state.y_min = yc - yh; graph_state.y_max = yc + yh;
        }
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;
    case 3: /* Zoom Out — expand window by XFact/YFact around centre */
        {
            float xc = (graph_state.x_min + graph_state.x_max) * 0.5f;
            float yc = (graph_state.y_min + graph_state.y_max) * 0.5f;
            float xh = (graph_state.x_max - graph_state.x_min) * zoom_x_fact / 2.0f;
            float yh = (graph_state.y_max - graph_state.y_min) * zoom_y_fact / 2.0f;
            graph_state.x_min = xc - xh; graph_state.x_max = xc + xh;
            graph_state.y_min = yc - yh; graph_state.y_max = yc + yh;
        }
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;
    case 4: /* Set Factors — open ZOOM FACTORS sub-screen */
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
        break;
    case 5: /* Square */
        apply_zoom_preset(4);
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;
    case 6: /* Standard */
        apply_zoom_preset(1);
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;
    case 7: /* Trig */
        apply_zoom_preset(2);
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;
    case 8: /* Integer */
        apply_zoom_preset(5);
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;
    default:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        break;
    }
}

/*---------------------------------------------------------------------------
 * MODE screen display helper
 *---------------------------------------------------------------------------*/

/* Redraws all MODE screen option labels with correct highlight colours.
 * Must be called under lvgl_lock(). */
static void ui_update_mode_display(void)
{
    for (int r = 0; r < MODE_ROW_COUNT; r++) {
        int n = mode_option_count[r];
        for (int c = 0; c < n; c++) {
            lv_obj_t *lbl = mode_option_labels[r][c];
            if (lbl == NULL) continue;
            lv_color_t col;
            if (r == mode_row_selected && c == (int)mode_cursor[r])
                col = lv_color_hex(0xFFFF00);
            else if (c == (int)mode_committed[r])
                col = lv_color_hex(0xFFFFFF);   /* white — committed */
            else
                col = lv_color_hex(0x666666);   /* dim — not selected */
            lv_obj_set_style_text_color(lbl, col, 0);
        }
    }
}

/*---------------------------------------------------------------------------
 * MATH menu display helper
 *---------------------------------------------------------------------------*/

/* Redraws tab bar and visible item rows for the current MATH menu state.
 * Must be called under lvgl_lock(). */
static void ui_update_math_display(void)
{
    /* Tab labels */
    for (int i = 0; i < MATH_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(math_tab_labels[i],
            (i == (int)math_tab) ? lv_color_hex(0xFFFF00) : lv_color_hex(0x666666), 0);
    }

    /* Hide both scroll indicators; re-shown below if needed */
    lv_obj_add_flag(math_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(math_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    int total = (int)math_tab_item_count[math_tab];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)math_scroll_offset + i;
        if (idx >= total) {
            lv_label_set_text(math_item_labels[i], "");
            continue;
        }
        bool more_below = (math_scroll_offset + MENU_VISIBLE_ROWS < (uint8_t)total)
                          && (i == MENU_VISIBLE_ROWS - 1);
        bool more_above = (math_scroll_offset > 0) && (i == 0);
        char buf[40];
        const char *name = math_menu_items[math_tab][idx].display;
        if (more_below) {
            /* Space holds the arrow's slot; amber ↓ overlay drawn on top */
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, name);
            lv_label_set_text(math_scroll_ind[1], "\xE2\x86\x93");
            lv_obj_clear_flag(math_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);
        } else if (more_above) {
            snprintf(buf, sizeof(buf), "%d %s", idx + 1, name);
            lv_label_set_text(math_scroll_ind[0], "\xE2\x86\x91");
            lv_obj_clear_flag(math_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
        } else {
            snprintf(buf, sizeof(buf), "%d:%s", idx + 1, name);
        }

        lv_obj_set_style_text_color(math_item_labels[i],
            (i == (int)math_item_cursor) ? lv_color_hex(0xFFFF00) : lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(math_item_labels[i], buf);
    }
}

/* Refreshes the TEST menu item labels based on test_item_cursor. */
static void ui_update_test_display(void)
{
    for (int i = 0; i < TEST_ITEM_COUNT; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%s", i + 1, test_menu_items[i].display);
        lv_obj_set_style_text_color(test_item_labels[i],
            (i == (int)test_item_cursor) ? lv_color_hex(0xFFFF00) : lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(test_item_labels[i], buf);
    }
}


/**
 * @brief Loads the stored value of the current cell into matrix_edit_buf.
 *
/*---------------------------------------------------------------------------
 * RANGE editor helpers
 *---------------------------------------------------------------------------*/

/* Parses range_field_buf and writes it into the appropriate graph_state field.
 * Scale/res fields reject zero or negative values. */
static void range_commit_field(void)
{
    if (range_field_len == 0)
        return;
    float val = strtof(range_field_buf, NULL);
    switch (range_field_selected) {
    case 0: graph_state.x_min = val; break;
    case 1: graph_state.x_max = val; break;
    case 2: if (val > 0.0f) graph_state.x_scl = val; break;  /* must be positive */
    case 3: graph_state.y_min = val; break;
    case 4: graph_state.y_max = val; break;
    case 5: if (val > 0.0f) graph_state.y_scl = val; break;  /* must be positive */
    case 6: { int32_t iv = (int32_t)val; if (iv >= 1 && iv <= 8) graph_state.x_res = (float)iv; } break;  /* integer 1–8 */
    }
}

/* Highlights the active RANGE field label in yellow; all others are white. */
static void range_update_highlight(void)
{
    for (uint8_t i = 0; i < 7; i++) {
        lv_obj_t *lbl = ui_lbl_range_rows[i];
        if (lbl == NULL)
            continue;
        lv_obj_set_style_text_color(lbl,
            (i == range_field_selected) ? lv_color_hex(0xFFFF00)
                                        : lv_color_hex(0xFFFFFF),
            0);
    }
}

/*---------------------------------------------------------------------------
 * ZOOM FACTORS editor helpers
 *---------------------------------------------------------------------------*/

/* Resets the ZOOM FACTORS input buffer and field state. */
static void zoom_factors_reset(void)
{
    zoom_factors_field  = 0;
    zoom_factors_len    = 0;
    zoom_factors_buf[0] = '\0';
    zoom_factors_cursor = 0;
}

/* Pre-populates zoom_factors_buf from the stored zoom_x_fact / zoom_y_fact
 * for the currently selected field. */
static void zoom_factors_load_field(void)
{
    float val = (zoom_factors_field == 0) ? zoom_x_fact : zoom_y_fact;
    snprintf(zoom_factors_buf, sizeof(zoom_factors_buf), "%.4g", val);
    zoom_factors_len    = (uint8_t)strlen(zoom_factors_buf);
    zoom_factors_cursor = 0;
}

/* Parses zoom_factors_buf and writes it to zoom_x_fact or zoom_y_fact.
 * Rejects zero and negative values. */
static void zoom_factors_commit_field(void)
{
    if (zoom_factors_len == 0) return;
    float val = strtof(zoom_factors_buf, NULL);
    if (val <= 0.0f) return;
    if (zoom_factors_field == 0) zoom_x_fact = val;
    else                         zoom_y_fact = val;
}

/* Refreshes both ZOOM FACTORS row labels from current state. */
static void ui_update_zoom_factors_display(void)
{
    char buf[32];
    float vals[2] = { zoom_x_fact, zoom_y_fact };
    for (int i = 0; i < 2; i++) {
        if (ui_lbl_zoom_factors_rows[i] == NULL) continue;
        if (i == (int)zoom_factors_field && zoom_factors_len > 0) {
            snprintf(buf, sizeof(buf), "%s%s", zoom_factors_names[i], zoom_factors_buf);
        } else {
            /* Format value — strip trailing zeros like %.4g */
            char val_str[16];
            snprintf(val_str, sizeof(val_str), "%.4g", vals[i]);
            snprintf(buf, sizeof(buf), "%s%s", zoom_factors_names[i], val_str);
        }
        lv_label_set_text(ui_lbl_zoom_factors_rows[i], buf);
    }
}

/* Highlights the active ZOOM FACTORS field in yellow; the other in white. */
static void zoom_factors_update_highlight(void)
{
    for (int i = 0; i < 2; i++) {
        if (ui_lbl_zoom_factors_rows[i] == NULL) continue;
        lv_obj_set_style_text_color(ui_lbl_zoom_factors_rows[i],
            (i == (int)zoom_factors_field) ? lv_color_hex(0xFFFF00)
                                           : lv_color_hex(0xFFFFFF),
            0);
    }
}

/* Positions the cursor over the active ZOOM FACTORS field. */
static void zoom_factors_cursor_update(void)
{
    if (zoom_factors_cursor_box == NULL) return;
    if (ui_lbl_zoom_factors_rows[zoom_factors_field] == NULL) return;
    uint32_t char_pos = (uint32_t)strlen(zoom_factors_names[zoom_factors_field])
                      + zoom_factors_cursor;
    cursor_place(zoom_factors_cursor_box, zoom_factors_cursor_inner,
                 ui_lbl_zoom_factors_rows[zoom_factors_field], char_pos);
}

/**
 * @brief Positions the flashing cursor in the MATRIX EDIT screen.
 *
 * Dim mode (cursor == -1): cursor sits on the rows digit (dim_field 0) or
 *   cols digit (dim_field 1) within the title label "[X] RxC".
 *   Character positions: [=0, X=1, ]=2, ' '=3, R=4, x=5, C=6.
 *
 * Cell mode (cursor >= 0): cursor sits after the '=' in the active list row
 *   label "r,c=VALUE", advancing as the user types.
 */
static void matrix_edit_cursor_update(void)
{
    /* No-op, matrix functionality removed */
}

/*---------------------------------------------------------------------------
 * Token execution
 *---------------------------------------------------------------------------*/

/**
 * @brief Inserts a MATH menu item into the active destination and exits the
 *        MATH menu.  If the menu was opened from the Y= editor it inserts
 *        at yeq_cursor_pos and restores the Y= screen; otherwise it inserts
 *        into the main expression.  Called from the MODE_MATH_MENU handler.
 */
static void math_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (math_return_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        char *eq = graph_state.equations[yeq_selected];
        size_t ins_len = strlen(ins);
        size_t eq_len  = strlen(eq);
        if (eq_len + ins_len < 63) {
            memmove(&eq[yeq_cursor_pos + ins_len], &eq[yeq_cursor_pos],
                    eq_len - yeq_cursor_pos + 1);
            memcpy(&eq[yeq_cursor_pos], ins, ins_len);
            yeq_cursor_pos += (uint8_t)ins_len;
        }
        lvgl_lock();
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    math_return_mode = MODE_NORMAL;
}

/**
 * @brief Generic cross-module helper that takes a menu item and inserts it
 *        into either the Y= editor or the normal calculator context, then
 *        returns context via pointers.
 */
void menu_insert_text(const char *ins, CalcMode_t *ret_mode)
{
    if (*ret_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        char *eq = graph_state.equations[yeq_selected];
        size_t ins_len = strlen(ins);
        size_t eq_len  = strlen(eq);
        if (eq_len + ins_len < 63) {
            memmove(&eq[yeq_cursor_pos + ins_len], &eq[yeq_cursor_pos],
                    eq_len - yeq_cursor_pos + 1);
            memcpy(&eq[yeq_cursor_pos], ins, ins_len);
            yeq_cursor_pos += (uint8_t)ins_len;
        }
        lvgl_lock();
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    *ret_mode = MODE_NORMAL;
}

/**
 * @brief Inserts a TEST menu item into the active destination and exits the
 *        TEST menu.  Mirrors math_menu_insert — supports return to Y= or
 *        normal calculator input.
 */
static void test_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (test_return_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        char *eq = graph_state.equations[yeq_selected];
        size_t ins_len = strlen(ins);
        size_t eq_len  = strlen(eq);
        if (eq_len + ins_len < 63) {
            memmove(&eq[yeq_cursor_pos + ins_len], &eq[yeq_cursor_pos],
                    eq_len - yeq_cursor_pos + 1);
            memcpy(&eq[yeq_cursor_pos], ins, ins_len);
            yeq_cursor_pos += (uint8_t)ins_len;
        }
        lvgl_lock();
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    test_return_mode = MODE_NORMAL;
}

/*---------------------------------------------------------------------------
 * Navigation helper functions
 *---------------------------------------------------------------------------*/

/* Hides every graph editor, menu overlay, and the graph canvas.
 * Must be called inside lvgl_lock(). */
void hide_all_screens(void)
{
    lv_obj_add_flag(ui_graph_yeq_screen,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_graph_range_screen,        LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_graph_zoom_screen,         LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_mode_screen,               LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_math_screen,               LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_test_screen,               LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_matrix_screen,             LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_matrix_edit_screen,        LV_OBJ_FLAG_HIDDEN);
    hide_prgm_screens();
    Graph_SetVisible(false);
}

/* Navigates to a graph-related screen from any current mode.
 * Handles all hide/show/state-reset logic in one place.
 * Caller must do any FROM-state cleanup before calling
 * (e.g. range_commit_field, zoom_menu_reset, zbox_corner1_set=false).
 * Pass MODE_NORMAL to press GRAPH: renders the graph canvas. */
void nav_to(CalcMode_t target)
{
    lvgl_lock();
    hide_all_screens();
    current_mode = target;

    switch (target) {
    case MODE_GRAPH_YEQ:
        graph_state.active = false;
        yeq_cursor_pos = (uint8_t)strlen(graph_state.equations[yeq_selected]);
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
        yeq_update_highlight();
        yeq_reflow_rows();
        yeq_cursor_update();
        break;

    case MODE_GRAPH_RANGE:
        graph_state.active = false;
        range_field_reset();
        range_load_field();
        lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_range_display();
        range_update_highlight();
        range_cursor_update();
        break;

    case MODE_GRAPH_ZOOM:
        graph_state.active = false;
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
        trace_eq_idx = find_first_active_eq();
        trace_x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
        Graph_SetVisible(true);
        Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
        break;

    default:
        break;
    }
    lvgl_unlock();
}

/* Opens a menu (MATH, TEST, or MATRIX) from any screen.
 * return_to: the mode to restore when the menu is closed.
 * Hides all screens first so no overlay leaks through. */
static void menu_open(Token_t menu_token, CalcMode_t return_to)
{
    lvgl_lock();
    hide_all_screens();
    switch (menu_token) {
    case TOKEN_MATH:
        math_return_mode   = return_to;
        math_tab           = 0;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        current_mode       = MODE_MATH_MENU;
        lv_obj_clear_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_math_display();
        break;
    case TOKEN_TEST:
        test_return_mode  = return_to;
        test_item_cursor  = 0;
        current_mode      = MODE_TEST_MENU;
        lv_obj_clear_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_test_display();
        break;
    case TOKEN_MATRX:
        matrix_return_mode = return_to;
        matrix_tab         = 0;
        matrix_item_cursor = 0;
        current_mode       = MODE_MATRIX_MENU;
        lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_matrix_display();
        break;
    case TOKEN_PRGM:
        prgm_menu_open(return_to);
        break;
    default:
        break;
    }
    lvgl_unlock();
}

/* Closes a menu and restores the calling screen.
 * Returns the restored CalcMode_t (MODE_NORMAL or MODE_GRAPH_YEQ).
 * Does NOT fall through; callers decide whether to return or break. */
CalcMode_t menu_close(Token_t menu_token)
{
    CalcMode_t ret;
    switch (menu_token) {
    case TOKEN_MATH:
        ret                = math_return_mode;
        math_return_mode   = MODE_NORMAL;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        break;
    case TOKEN_TEST:
        ret              = test_return_mode;
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        break;
    case TOKEN_MATRX:
        ret                = matrix_return_mode;
        matrix_return_mode = MODE_NORMAL;
        matrix_tab         = 0;
        matrix_item_cursor = 0;
        break;
    case TOKEN_PRGM:
        ret = prgm_menu_close();
        break;
    default:
        ret = MODE_NORMAL;
        break;
    }
    current_mode = ret;
    lvgl_lock();
    lv_obj_add_flag(ui_math_screen,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_test_screen,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
    hide_prgm_screens();
    if (ret == MODE_GRAPH_YEQ)
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();
    return ret;
}

/* Moves the active tab in a multi-tab menu left or right.
 * Resets item cursor and scroll offset on tab change. */
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll,
                     uint8_t tab_count, bool left, void (*update)(void))
{
    if (left) {
        if (*tab > 0) { (*tab)--; *cursor = 0; if (scroll) *scroll = 0; }
    } else {
        if (*tab < tab_count - 1) { (*tab)++; *cursor = 0; if (scroll) *scroll = 0; }
    }
    lvgl_lock(); update(); lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Per-mode token handlers
 * Each returns true if the token was fully handled (Execute_Token should
 * return), false if execution should fall through to the next handler.
 *---------------------------------------------------------------------------*/

static bool handle_yeq_mode(Token_t t)
{
    char *eq  = graph_state.equations[yeq_selected];
    uint8_t eq_len = strlen(eq);
    const char *append = NULL;
    char num_buf[2] = {0, 0};

    /* Clamp cursor to current equation length */
    if (yeq_cursor_pos > eq_len) yeq_cursor_pos = eq_len;

    switch (t) {
    case TOKEN_GRAPH:
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_Y_EQUALS:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
        eq[0]          = '\0';
        yeq_cursor_pos = 0;
        lvgl_lock();
        lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        if (yeq_cursor_pos > 0) {
            if (matrix_token_size_before(eq, yeq_cursor_pos) == 3) {
                yeq_cursor_pos -= 3;
            } else {
                do { yeq_cursor_pos--; }
                while (yeq_cursor_pos > 0 &&
                       ((uint8_t)eq[yeq_cursor_pos] & 0xC0) == 0x80);
            }
        }
        lvgl_lock();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (yeq_cursor_pos < eq_len) {
            uint8_t mat = matrix_token_size_at(eq, yeq_cursor_pos, (uint8_t)eq_len);
            if (mat) {
                yeq_cursor_pos += mat;
            } else {
                uint8_t step = utf8_char_size(&eq[yeq_cursor_pos]);
                yeq_cursor_pos += step ? step : 1;
            }
            if (yeq_cursor_pos > (uint8_t)eq_len) yeq_cursor_pos = (uint8_t)eq_len;
        }
        lvgl_lock();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_INS:
        insert_mode = !insert_mode;
        lvgl_lock();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_RANGE:
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_TRACE:
        nav_to(MODE_GRAPH_TRACE);
        return true;
    case TOKEN_DEL:
        if (yeq_cursor_pos > 0) {
            uint8_t prev;
            uint8_t mat = matrix_token_size_before(eq, yeq_cursor_pos);
            if (mat) {
                prev = yeq_cursor_pos - mat;
            } else {
                prev = yeq_cursor_pos;
                do { prev--; }
                while (prev > 0 && ((uint8_t)eq[prev] & 0xC0) == 0x80);
            }
            memmove(&eq[prev], &eq[yeq_cursor_pos],
                    eq_len - yeq_cursor_pos + 1);
            yeq_cursor_pos = prev;
            lvgl_lock();
            lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
        }
        return true;
    case TOKEN_UP:
        if (yeq_selected > 0) yeq_selected--;
        yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
        lvgl_lock();
        yeq_update_highlight();
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
    case TOKEN_DOWN:
        if (yeq_selected < GRAPH_NUM_EQ - 1) yeq_selected++;
        yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
        lvgl_lock();
        yeq_update_highlight();
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
        return true;
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
    case TOKEN_ASIN:    append = "asin("; break;
    case TOKEN_ACOS:    append = "acos("; break;
    case TOKEN_ATAN:    append = "atan("; break;
    case TOKEN_LN:      append = "ln(";   break;
    case TOKEN_LOG:     append = "log(";  break;
    case TOKEN_SQRT:    append = "\xE2\x88\x9A("; break;
    case TOKEN_ABS:     append = "abs(";  break;
    case TOKEN_SQUARE:  append = "^2";    break;
    case TOKEN_PI:      append = "π";     break;
    case TOKEN_NEG:     append = "-";     break;
    case TOKEN_X_INV:   append = "^-1";   break;
    case TOKEN_ANS:     append = "ANS";   break;
    case TOKEN_MATH:
        menu_open(TOKEN_MATH, MODE_GRAPH_YEQ);
        return true;
    case TOKEN_TEST:
        menu_open(TOKEN_TEST, MODE_GRAPH_YEQ);
        return true;
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
    default:            break;
    }

    if (append != NULL) {
        size_t len = strlen(append);
        if (!insert_mode && len == 1 && yeq_cursor_pos < eq_len) {
            uint8_t cur_size = matrix_token_size_at(eq, yeq_cursor_pos, (uint8_t)eq_len);
            if (!cur_size) cur_size = utf8_char_size(&eq[yeq_cursor_pos]);
            if (cur_size <= 1) {
                eq[yeq_cursor_pos++] = append[0];
            } else {
                memmove(&eq[yeq_cursor_pos + 1],
                        &eq[yeq_cursor_pos + cur_size],
                        eq_len - yeq_cursor_pos - cur_size + 1);
                eq[yeq_cursor_pos++] = append[0];
            }
            lvgl_lock();
            lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
        } else if (eq_len + len < 63) {
            memmove(&eq[yeq_cursor_pos + len], &eq[yeq_cursor_pos],
                    eq_len - yeq_cursor_pos + 1);
            memcpy(&eq[yeq_cursor_pos], append, len);
            yeq_cursor_pos += (uint8_t)len;
            lvgl_lock();
            lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
        }
    }
    lvgl_lock();
    ui_update_status_bar();
    yeq_cursor_update();
    lvgl_unlock();
    return true;
}

static bool handle_range_mode(Token_t t)
{
    switch (t) {
    case TOKEN_0 ... TOKEN_9: {
        char ch = (char)((t - TOKEN_0) + '0');
        if (!insert_mode && range_field_cursor_pos < range_field_len) {
            range_field_buf[range_field_cursor_pos++] = ch;
        } else if (range_field_len < sizeof(range_field_buf) - 1) {
            memmove(&range_field_buf[range_field_cursor_pos + 1],
                    &range_field_buf[range_field_cursor_pos],
                    range_field_len - range_field_cursor_pos + 1);
            range_field_buf[range_field_cursor_pos++] = ch;
            range_field_len++;
        }
        lvgl_lock();
        ui_update_range_display();
        range_cursor_update();
        lvgl_unlock();
        return true;
    }

    case TOKEN_DECIMAL:
        if (strchr(range_field_buf, '.') == NULL) {
            char ch = '.';
            if (!insert_mode && range_field_cursor_pos < range_field_len) {
                range_field_buf[range_field_cursor_pos++] = ch;
            } else if (range_field_len < sizeof(range_field_buf) - 1) {
                memmove(&range_field_buf[range_field_cursor_pos + 1],
                        &range_field_buf[range_field_cursor_pos],
                        range_field_len - range_field_cursor_pos + 1);
                range_field_buf[range_field_cursor_pos++] = ch;
                range_field_len++;
            }
            lvgl_lock();
            ui_update_range_display();
            range_cursor_update();
            lvgl_unlock();
        }
        return true;

    case TOKEN_NEG:
        if (range_field_len > 0 && range_field_buf[0] == '-') {
            memmove(range_field_buf, range_field_buf + 1, range_field_len);
            range_field_len--;
        } else if (range_field_len < sizeof(range_field_buf) - 1) {
            memmove(range_field_buf + 1, range_field_buf, range_field_len + 1);
            range_field_buf[0] = '-';
            range_field_len++;
        }
        range_field_cursor_pos = range_field_len;
        lvgl_lock();
        ui_update_range_display();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_DEL:
        if (range_field_cursor_pos > 0) {
            memmove(&range_field_buf[range_field_cursor_pos - 1],
                    &range_field_buf[range_field_cursor_pos],
                    range_field_len - range_field_cursor_pos + 1);
            range_field_len--;
            range_field_cursor_pos--;
            lvgl_lock();
            ui_update_range_display();
            range_cursor_update();
            lvgl_unlock();
        }
        return true;

    case TOKEN_LEFT:
        if (range_field_cursor_pos > 0) range_field_cursor_pos--;
        lvgl_lock();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_RIGHT:
        if (range_field_cursor_pos < range_field_len) range_field_cursor_pos++;
        lvgl_lock();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_INS:
        insert_mode = !insert_mode;
        lvgl_lock();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER:
    case TOKEN_DOWN:
        range_commit_field();
        if (range_field_selected < 6) range_field_selected++;
        range_load_field();
        lvgl_lock();
        ui_update_range_display();
        range_update_highlight();
        range_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_UP:
        range_commit_field();
        if (range_field_selected > 0) range_field_selected--;
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
        if (range_field_len > 0) {
            range_field_len        = 0;
            range_field_buf[0]     = '\0';
            range_field_cursor_pos = 0;
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
        nav_to(MODE_GRAPH_YEQ);
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

static bool handle_zoom_mode(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (zoom_item_cursor > 0) {
            zoom_item_cursor--;
        } else if (zoom_scroll_offset > 0) {
            zoom_scroll_offset--;
        }
        lvgl_lock();
        ui_update_zoom_display();
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(zoom_scroll_offset + zoom_item_cursor) + 1 < ZOOM_ITEM_COUNT) {
            if (zoom_item_cursor < MENU_VISIBLE_ROWS - 1)
                zoom_item_cursor++;
            else if (zoom_scroll_offset + MENU_VISIBLE_ROWS < ZOOM_ITEM_COUNT)
                zoom_scroll_offset++;
        }
        lvgl_lock();
        ui_update_zoom_display();
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        zoom_execute_item((uint8_t)(zoom_scroll_offset + zoom_item_cursor + 1));
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
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_GRAPH:
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_TRACE:
        nav_to(MODE_GRAPH_TRACE);
        return true;
    case TOKEN_1 ... TOKEN_9: {
        uint8_t item = (uint8_t)(t - TOKEN_0);
        if (item <= ZOOM_ITEM_COUNT)
            zoom_execute_item(item);
        return true;
    }
    default:
        /* Any unrecognized key exits the ZOOM menu and is processed normally */
        zoom_menu_reset();
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}

static bool handle_zoom_factors_mode(Token_t t)
{
    switch (t) {
    case TOKEN_0 ... TOKEN_9: {
        char ch = (char)((t - TOKEN_0) + '0');
        if (!insert_mode && zoom_factors_cursor < zoom_factors_len) {
            zoom_factors_buf[zoom_factors_cursor++] = ch;
        } else if (zoom_factors_len < sizeof(zoom_factors_buf) - 1) {
            memmove(&zoom_factors_buf[zoom_factors_cursor + 1],
                    &zoom_factors_buf[zoom_factors_cursor],
                    zoom_factors_len - zoom_factors_cursor + 1);
            zoom_factors_buf[zoom_factors_cursor++] = ch;
            zoom_factors_len++;
        }
        lvgl_lock();
        ui_update_zoom_factors_display();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;
    }

    case TOKEN_DECIMAL:
        if (strchr(zoom_factors_buf, '.') == NULL) {
            char ch = '.';
            if (!insert_mode && zoom_factors_cursor < zoom_factors_len) {
                zoom_factors_buf[zoom_factors_cursor++] = ch;
            } else if (zoom_factors_len < sizeof(zoom_factors_buf) - 1) {
                memmove(&zoom_factors_buf[zoom_factors_cursor + 1],
                        &zoom_factors_buf[zoom_factors_cursor],
                        zoom_factors_len - zoom_factors_cursor + 1);
                zoom_factors_buf[zoom_factors_cursor++] = ch;
                zoom_factors_len++;
            }
            lvgl_lock();
            ui_update_zoom_factors_display();
            zoom_factors_cursor_update();
            lvgl_unlock();
        }
        return true;

    case TOKEN_DEL:
        if (zoom_factors_cursor > 0) {
            memmove(&zoom_factors_buf[zoom_factors_cursor - 1],
                    &zoom_factors_buf[zoom_factors_cursor],
                    zoom_factors_len - zoom_factors_cursor + 1);
            zoom_factors_len--;
            zoom_factors_cursor--;
            lvgl_lock();
            ui_update_zoom_factors_display();
            zoom_factors_cursor_update();
            lvgl_unlock();
        }
        return true;

    case TOKEN_LEFT:
        if (zoom_factors_cursor > 0) zoom_factors_cursor--;
        lvgl_lock();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_RIGHT:
        if (zoom_factors_cursor < zoom_factors_len) zoom_factors_cursor++;
        lvgl_lock();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_INS:
        insert_mode = !insert_mode;
        lvgl_lock();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_ENTER:
    case TOKEN_DOWN:
        zoom_factors_commit_field();
        if (zoom_factors_field < 1) zoom_factors_field++;
        zoom_factors_load_field();
        lvgl_lock();
        ui_update_zoom_factors_display();
        zoom_factors_update_highlight();
        zoom_factors_cursor_update();
        lvgl_unlock();
        return true;

    case TOKEN_UP:
        zoom_factors_commit_field();
        if (zoom_factors_field > 0) zoom_factors_field--;
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
        if (zoom_factors_len > 0) {
            zoom_factors_len        = 0;
            zoom_factors_buf[0]     = '\0';
            zoom_factors_cursor     = 0;
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

static bool handle_zbox_mode(Token_t t)
{
    switch (t) {
    case TOKEN_LEFT:
        if (zbox_px > 0) zbox_px--;
        lvgl_lock();
        Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (zbox_px < GRAPH_W - 1) zbox_px++;
        lvgl_lock();
        Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_UP:
        if (zbox_py > 0) zbox_py--;
        lvgl_lock();
        Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (zbox_py < GRAPH_H - 1) zbox_py++;
        lvgl_lock();
        Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        if (!zbox_corner1_set) {
            zbox_px1         = zbox_px;
            zbox_py1         = zbox_py;
            zbox_corner1_set = true;
            lvgl_lock();
            Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
            lvgl_unlock();
        } else {
            int32_t x_lo = zbox_px1 < zbox_px ? zbox_px1 : zbox_px;
            int32_t x_hi = zbox_px1 < zbox_px ? zbox_px  : zbox_px1;
            int32_t y_lo = zbox_py1 < zbox_py ? zbox_py1 : zbox_py;
            int32_t y_hi = zbox_py1 < zbox_py ? zbox_py  : zbox_py1;
            if (x_hi > x_lo && y_hi > y_lo) {
                float x_range    = graph_state.x_max - graph_state.x_min;
                float y_range    = graph_state.y_max - graph_state.y_min;
                float new_x_min  = graph_state.x_min + (float)x_lo / (float)(GRAPH_W - 1) * x_range;
                float new_x_max  = graph_state.x_min + (float)x_hi / (float)(GRAPH_W - 1) * x_range;
                float new_y_max  = graph_state.y_max - (float)y_lo / (float)(GRAPH_H - 1) * y_range;
                float new_y_min  = graph_state.y_max - (float)y_hi / (float)(GRAPH_H - 1) * y_range;
                graph_state.x_min = new_x_min;
                graph_state.x_max = new_x_max;
                graph_state.y_min = new_y_min;
                graph_state.y_max = new_y_max;
            }
            current_mode     = MODE_NORMAL;
            zbox_corner1_set = false;
            lvgl_lock();
            Graph_ClearTrace();
            Graph_Render(angle_degrees);
            lvgl_unlock();
        }
        return true;
    case TOKEN_CLEAR:
        zbox_corner1_set = false;
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return true;
    case TOKEN_ZOOM:
        zbox_corner1_set = false;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_GRAPH:
        zbox_corner1_set = false;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_Y_EQUALS:
        zbox_corner1_set = false;
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        zbox_corner1_set = false;
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_TRACE:
        zbox_corner1_set = false;
        nav_to(MODE_GRAPH_TRACE);
        return true;
    default:
        zbox_corner1_set = false;
        nav_to(MODE_NORMAL);
        return false; /* fall through to main switch */
    }
}

static bool handle_trace_mode(Token_t t)
{
    float step = (graph_state.x_max - graph_state.x_min) / (float)(GRAPH_W - 1);
    switch (t) {
    case TOKEN_LEFT:
        if (trace_x > graph_state.x_min) trace_x -= step;
        lvgl_lock();
        Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (trace_x < graph_state.x_max) trace_x += step;
        lvgl_lock();
        Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_UP:
        for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
            uint8_t idx = (trace_eq_idx + GRAPH_NUM_EQ - i) % GRAPH_NUM_EQ;
            if (strlen(graph_state.equations[idx]) > 0) {
                trace_eq_idx = idx;
                break;
            }
        }
        lvgl_lock();
        Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
        lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        for (uint8_t i = 1; i <= GRAPH_NUM_EQ; i++) {
            uint8_t idx = (trace_eq_idx + i) % GRAPH_NUM_EQ;
            if (strlen(graph_state.equations[idx]) > 0) {
                trace_eq_idx = idx;
                break;
            }
        }
        lvgl_lock();
        Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
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
        Graph_ClearTrace();
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        Graph_ClearTrace();
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        Graph_ClearTrace();
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_GRAPH:
        Graph_ClearTrace();
        nav_to(MODE_NORMAL);
        return true;
    default:
        /* Any other key exits trace immediately — hide canvas then fall through
         * to the main switch so the key is processed normally. */
        current_mode = MODE_NORMAL;
        lvgl_lock();
        hide_all_screens();
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}

static bool handle_mode_screen(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (mode_row_selected > 0) mode_row_selected--;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (mode_row_selected < MODE_ROW_COUNT - 1) mode_row_selected++;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        if (mode_cursor[mode_row_selected] > 0)
            mode_cursor[mode_row_selected]--;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (mode_cursor[mode_row_selected] < mode_option_count[mode_row_selected] - 1)
            mode_cursor[mode_row_selected]++;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        mode_committed[mode_row_selected] = mode_cursor[mode_row_selected];
        if (mode_row_selected == 1)
            Calc_SetDecimalMode(mode_committed[1]);
        if (mode_row_selected == 2)
            angle_degrees = (mode_committed[2] == 1);
        if (mode_row_selected == 6)
            graph_state.grid_on = (mode_committed[6] == 1);
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_CLEAR:
    case TOKEN_MODE:
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        return true;
    default:
        /* Any other key exits MODE screen and is processed normally */
        current_mode = MODE_NORMAL;
        lvgl_lock();
        lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        lvgl_unlock();
        return false; /* fall through to main switch */
    }
}

static bool handle_math_menu(Token_t t)
{
    int total = (int)math_tab_item_count[math_tab];
    switch (t) {
    case TOKEN_LEFT:
        tab_move(&math_tab, &math_item_cursor, &math_scroll_offset, MATH_TAB_COUNT, true, ui_update_math_display);
        return true;
    case TOKEN_RIGHT:
        tab_move(&math_tab, &math_item_cursor, &math_scroll_offset, MATH_TAB_COUNT, false, ui_update_math_display);
        return true;
    case TOKEN_UP:
        if (math_item_cursor > 0) {
            math_item_cursor--;
        } else if (math_scroll_offset > 0) {
            math_scroll_offset--;
        }
        lvgl_lock(); ui_update_math_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(math_scroll_offset + math_item_cursor) + 1 < total) {
            if (math_item_cursor < MENU_VISIBLE_ROWS - 1)
                math_item_cursor++;
            else if ((int)(math_scroll_offset + MENU_VISIBLE_ROWS) < total)
                math_scroll_offset++;
        }
        lvgl_lock(); ui_update_math_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)math_scroll_offset + (int)math_item_cursor;
        if (idx < total) {
            const char *ins = math_menu_items[math_tab][idx].insert;
            if (ins != NULL) { math_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_1 ... TOKEN_9: {
        int idx = (int)(t - TOKEN_0) - 1;
        if (idx < total) {
            const char *ins = math_menu_items[math_tab][idx].insert;
            if (ins != NULL) { math_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_CLEAR:
    case TOKEN_MATH:
        menu_close(TOKEN_MATH);
        return true;
    case TOKEN_Y_EQUALS:
        math_return_mode   = MODE_NORMAL;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        math_return_mode   = MODE_NORMAL;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        math_return_mode   = MODE_NORMAL;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_GRAPH:
        math_return_mode   = MODE_NORMAL;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_TRACE:
        math_return_mode   = MODE_NORMAL;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        nav_to(MODE_GRAPH_TRACE);
        return true;
    default: {
        CalcMode_t ret = menu_close(TOKEN_MATH);
        if (ret == MODE_GRAPH_YEQ)
            return true;
        return false; /* fall through to main switch */
    }
    }
    /* Execution reaches here only from ENTER/number when item not found */
    return true;
}

static bool handle_test_menu(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (test_item_cursor > 0) test_item_cursor--;
        lvgl_lock(); ui_update_test_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (test_item_cursor < TEST_ITEM_COUNT - 1) test_item_cursor++;
        lvgl_lock(); ui_update_test_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        const char *ins = test_menu_items[test_item_cursor].insert;
        if (ins != NULL) { test_menu_insert(ins); return true; }
        break;
    }
    case TOKEN_1 ... TOKEN_6: {
        int idx = (int)(t - TOKEN_0) - 1;
        if (idx >= 0 && idx < TEST_ITEM_COUNT) {
            const char *ins = test_menu_items[idx].insert;
            if (ins != NULL) { test_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_CLEAR:
    case TOKEN_TEST:
        menu_close(TOKEN_TEST);
        return true;
    case TOKEN_Y_EQUALS:
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        nav_to(MODE_GRAPH_YEQ);
        return true;
    case TOKEN_RANGE:
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        nav_to(MODE_GRAPH_RANGE);
        return true;
    case TOKEN_ZOOM:
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        return true;
    case TOKEN_GRAPH:
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        nav_to(MODE_NORMAL);
        return true;
    case TOKEN_TRACE:
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        nav_to(MODE_GRAPH_TRACE);
        return true;
    default: {
        CalcMode_t ret = menu_close(TOKEN_TEST);
        if (ret == MODE_GRAPH_YEQ)
            return true;
        return false; /* fall through to main switch */
    }
    }
    return true;
}

static bool handle_sto_pending(Token_t t)
{
    if (t >= TOKEN_A && t <= TOKEN_Z) {
        calc_variables[t - TOKEN_A] = ans;
        sto_pending = false;
        static const char var_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        char val_buf[16];
        Calc_FormatResult(ans, val_buf, sizeof(val_buf));
        uint8_t idx = history_count % HISTORY_LINE_COUNT;
        char var_str[3] = { var_names[t - TOKEN_A], '\0', '\0' };
        strncpy(history[idx].expression, var_str, MAX_EXPR_LEN - 1);
        history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
        strncpy(history[idx].result, val_buf, MAX_RESULT_LEN - 1);
        history[idx].result[MAX_RESULT_LEN - 1] = '\0';
        history_count++;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    } else if (t == TOKEN_CLEAR || t == TOKEN_2ND || t == TOKEN_ALPHA) {
        sto_pending = false;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return true;
    }
    /* Any other key cancels STO silently and falls through */
    sto_pending = false;
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
    return false;
}

void handle_normal_mode(Token_t t)
{
    switch (t) {

    case TOKEN_0 ... TOKEN_9:
        expr_insert_char((char)((t - TOKEN_0) + '0'));
        Update_Calculator_Display();
        break;

    case TOKEN_DECIMAL:
        expr_insert_char('.');
        Update_Calculator_Display();
        break;

    case TOKEN_ADD:
        expr_prepend_ans_if_empty();
        expr_insert_char('+');
        Update_Calculator_Display();
        break;

    case TOKEN_SUB:
        expr_prepend_ans_if_empty();
        expr_insert_char('-');
        Update_Calculator_Display();
        break;

    case TOKEN_MULT:
        expr_prepend_ans_if_empty();
        expr_insert_char('*');
        Update_Calculator_Display();
        break;

    case TOKEN_DIV:
        expr_prepend_ans_if_empty();
        expr_insert_char('/');
        Update_Calculator_Display();
        break;

    case TOKEN_LEFT:
        if (cursor_pos > 0) {
            if (matrix_token_size_before(expression, cursor_pos) == 3) {
                cursor_pos -= 3;
            } else {
                do { cursor_pos--; }
                while (cursor_pos > 0 &&
                       ((uint8_t)expression[cursor_pos] & 0xC0) == 0x80);
            }
            Update_Calculator_Display();
        }
        break;

    case TOKEN_RIGHT:
        if (cursor_pos < expr_len) {
            uint8_t mat = matrix_token_size_at(expression, cursor_pos, expr_len);
            if (mat) {
                cursor_pos += mat;
            } else {
                cursor_pos++;
                while (cursor_pos < expr_len &&
                       ((uint8_t)expression[cursor_pos] & 0xC0) == 0x80)
                    cursor_pos++;
            }
            Update_Calculator_Display();
        }
        break;

    case TOKEN_UP:
        if (expr_len == 0 || history_recall_offset > 0) {
            if (history_recall_offset < history_count) {
                history_recall_offset++;
                uint8_t idx = (history_count - history_recall_offset) % HISTORY_LINE_COUNT;
                strncpy(expression, history[idx].expression, MAX_EXPR_LEN - 1);
                expression[MAX_EXPR_LEN - 1] = '\0';
                expr_len   = (uint8_t)strlen(expression);
                cursor_pos = expr_len;
                Update_Calculator_Display();
            }
        }
        break;

    case TOKEN_DOWN:
        if (history_recall_offset > 0) {
            history_recall_offset--;
            if (history_recall_offset == 0) {
                expr_len      = 0;
                cursor_pos    = 0;
                expression[0] = '\0';
            } else {
                uint8_t idx = (history_count - history_recall_offset) % HISTORY_LINE_COUNT;
                strncpy(expression, history[idx].expression, MAX_EXPR_LEN - 1);
                expression[MAX_EXPR_LEN - 1] = '\0';
                expr_len   = (uint8_t)strlen(expression);
                cursor_pos = expr_len;
            }
            Update_Calculator_Display();
        }
        break;

    case TOKEN_ENTER:
        if (expr_len == 0 && history_count > 0) {
            uint8_t last_idx = (history_count - 1) % HISTORY_LINE_COUNT;
            CalcResult_t result = Calc_Evaluate(history[last_idx].expression,
                                                ans, ans_is_matrix, angle_degrees);
            char result_str[MAX_RESULT_LEN];
            format_calc_result(&result, result_str, MAX_RESULT_LEN, &ans);
            uint8_t idx = history_count % HISTORY_LINE_COUNT;
            strncpy(history[idx].expression, history[last_idx].expression, MAX_EXPR_LEN - 1);
            history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
            strncpy(history[idx].result, result_str, MAX_RESULT_LEN - 1);
            history[idx].result[MAX_RESULT_LEN - 1] = '\0';
            history_count++;
            history_recall_offset = 0;
            lvgl_lock();
            ui_update_history();
            lvgl_unlock();
            break;
        }
        if (expr_len > 0) {
            CalcResult_t result = Calc_Evaluate(expression, ans, ans_is_matrix,
                                                angle_degrees);
            char result_str[MAX_RESULT_LEN];
            format_calc_result(&result, result_str, MAX_RESULT_LEN, &ans);

            uint8_t idx = history_count % HISTORY_LINE_COUNT;
            strncpy(history[idx].expression, expression, MAX_EXPR_LEN - 1);
            history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
            strncpy(history[idx].result, result_str, MAX_RESULT_LEN - 1);
            history[idx].result[MAX_RESULT_LEN - 1] = '\0';
            history_count++;

            expr_len              = 0;
            cursor_pos            = 0;
            expression[0]         = '\0';
            history_recall_offset = 0;

            lvgl_lock();
            ui_update_history();
            lvgl_unlock();
        }
        break;

    case TOKEN_CLEAR:
        if (graph_state.active) {
            lvgl_lock();
            Graph_SetVisible(false);
            lvgl_unlock();
            break;
        }
        expr_len      = 0;
        cursor_pos    = 0;
        expression[0] = '\0';
        Update_Calculator_Display();
        break;

    case TOKEN_DEL:
        expr_delete_at_cursor();
        Update_Calculator_Display();
        break;

    case TOKEN_INS:
        insert_mode = !insert_mode;
        Update_Calculator_Display();
        break;

    case TOKEN_MODE:
        memcpy(mode_cursor, mode_committed, sizeof(mode_cursor));
        mode_row_selected = 0;
        current_mode = MODE_MODE_SCREEN;
        lvgl_lock();
        lv_obj_clear_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_mode_display();
        lvgl_unlock();
        break;

    case TOKEN_MATH:
        menu_open(TOKEN_MATH, MODE_NORMAL);
        break;

    case TOKEN_TEST:
        menu_open(TOKEN_TEST, MODE_NORMAL);
        break;

    case TOKEN_MATRX:
        menu_open(TOKEN_MATRX, MODE_NORMAL);
        break;

    case TOKEN_PRGM:
        menu_open(TOKEN_PRGM, MODE_NORMAL);
        break;

    case TOKEN_MTRX_A: expr_insert_str("[A]"); Update_Calculator_Display(); break;
    case TOKEN_MTRX_B: expr_insert_str("[B]"); Update_Calculator_Display(); break;
    case TOKEN_MTRX_C: expr_insert_str("[C]"); Update_Calculator_Display(); break;

    case TOKEN_SIN:     expr_insert_str("sin(");  Update_Calculator_Display(); break;
    case TOKEN_COS:     expr_insert_str("cos(");  Update_Calculator_Display(); break;
    case TOKEN_TAN:     expr_insert_str("tan(");  Update_Calculator_Display(); break;
    case TOKEN_ASIN:    expr_insert_str("asin("); Update_Calculator_Display(); break;
    case TOKEN_ACOS:    expr_insert_str("acos("); Update_Calculator_Display(); break;
    case TOKEN_ATAN:    expr_insert_str("atan("); Update_Calculator_Display(); break;
    case TOKEN_ABS:     expr_insert_str("abs(");  Update_Calculator_Display(); break;
    case TOKEN_LN:      expr_insert_str("ln(");   Update_Calculator_Display(); break;
    case TOKEN_LOG:     expr_insert_str("log(");  Update_Calculator_Display(); break;
    case TOKEN_SQRT:    expr_insert_str("\xE2\x88\x9A("); Update_Calculator_Display(); break;
    case TOKEN_EE:      expr_insert_str("*10^");  Update_Calculator_Display(); break;
    case TOKEN_E_X:     expr_insert_str("exp(");  Update_Calculator_Display(); break;
    case TOKEN_TEN_X:   expr_insert_str("10^(");  Update_Calculator_Display(); break;
    case TOKEN_PI:      expr_insert_str("π");     Update_Calculator_Display(); break;
    case TOKEN_ANS:     expr_insert_str("ANS");   Update_Calculator_Display(); break;
    case TOKEN_THETA:   expr_insert_str("θ");     Update_Calculator_Display(); break;
    case TOKEN_SPACE:   expr_insert_char(' ');    Update_Calculator_Display(); break;
    case TOKEN_COMMA:   expr_insert_char(',');    Update_Calculator_Display(); break;
    case TOKEN_QUOTES:  expr_insert_char('"');    Update_Calculator_Display(); break;
    case TOKEN_QSTN_M:  expr_insert_char('?');    Update_Calculator_Display(); break;

    case TOKEN_SQUARE:
        expr_prepend_ans_if_empty();
        expr_insert_str("^2");
        Update_Calculator_Display();
        break;

    case TOKEN_X_INV:
        expr_prepend_ans_if_empty();
        expr_insert_str("^-1");
        Update_Calculator_Display();
        break;

    case TOKEN_POWER:
        expr_prepend_ans_if_empty();
        expr_insert_char('^');
        Update_Calculator_Display();
        break;

    case TOKEN_L_PAR:   expr_insert_char('('); Update_Calculator_Display(); break;
    case TOKEN_R_PAR:   expr_insert_char(')'); Update_Calculator_Display(); break;
    case TOKEN_NEG:     expr_insert_char('-'); Update_Calculator_Display(); break;

    case TOKEN_ENTRY:
        if (history_count > 0) {
            history_recall_offset = 1;
            uint8_t idx = (history_count - 1) % HISTORY_LINE_COUNT;
            strncpy(expression, history[idx].expression, MAX_EXPR_LEN - 1);
            expression[MAX_EXPR_LEN - 1] = '\0';
            expr_len   = (uint8_t)strlen(expression);
            cursor_pos = expr_len;
            Update_Calculator_Display();
        }
        break;

    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z: {
        static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        char ch[2] = { alpha_chars[t - TOKEN_A], '\0' };
        expr_insert_str(ch);
        Update_Calculator_Display();
        break;
    }

    case TOKEN_STO:
        sto_pending = true;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        break;

    case TOKEN_Y_EQUALS:
        nav_to(MODE_GRAPH_YEQ);
        break;

    case TOKEN_RANGE:
        nav_to(MODE_GRAPH_RANGE);
        break;

    case TOKEN_ZOOM:
        zoom_menu_reset();
        nav_to(MODE_GRAPH_ZOOM);
        break;

    case TOKEN_GRAPH:
        nav_to(MODE_NORMAL);
        break;

    case TOKEN_TRACE:
        nav_to(MODE_GRAPH_TRACE);
        break;

    default:
        break;
    }
}



/**
 * @brief Processes a single calculator token from the keypad queue.
 * @param t  Token to execute.
 */
void Execute_Token(Token_t t)
{
    /*--- TOKEN_ON: save state (plain ON) or save + sleep (2nd+ON) ----------*/
    if (t == TOKEN_ON) {
        bool power_down = (current_mode == MODE_2ND);

        lvgl_lock();
        lv_obj_t *saving_lbl = lv_label_create(lv_scr_act());
        lv_label_set_text(saving_lbl, "Saving...");
        lv_obj_set_style_text_color(saving_lbl, lv_color_hex(0xFFAA00), 0);
        lv_obj_align(saving_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
        lvgl_unlock();
        osDelay(20);

        PersistBlock_t block;
        Calc_BuildPersistBlock(&block);
        Persist_Save(&block);
        Prgm_Save();

        current_mode       = MODE_NORMAL;
        return_mode        = MODE_NORMAL;
        sto_pending        = false;
        prgm_reset_execution_state();
        lvgl_lock();
        lv_obj_del(saving_lbl);
        hide_all_screens();
        ui_update_status_bar();
        lvgl_unlock();

        if (power_down) {
            Power_DisplayBlankAndMessage();
        }
        return;
    }

    /*--- TOKEN_MODE: always opens MODE screen from any mode ----------------*/
    if (t == TOKEN_MODE) {
        memcpy(mode_cursor, mode_committed, sizeof(mode_cursor));
        mode_row_selected = 0;
        current_mode = MODE_MODE_SCREEN;
        lvgl_lock();
        hide_all_screens();
        lv_obj_clear_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_mode_display();
        lvgl_unlock();
        return;
    }

    if (current_mode == MODE_PRGM_RUNNING)        { handle_prgm_running(t); return; }

    if (current_mode == MODE_GRAPH_YEQ)          { if (handle_yeq_mode(t))          return; }
    if (current_mode == MODE_GRAPH_RANGE)         { if (handle_range_mode(t))         return; }
    if (current_mode == MODE_GRAPH_ZOOM)          { if (handle_zoom_mode(t))          return; }
    if (current_mode == MODE_GRAPH_ZOOM_FACTORS)  { if (handle_zoom_factors_mode(t))  return; }
    if (current_mode == MODE_GRAPH_ZBOX)          { if (handle_zbox_mode(t))          return; }
    if (current_mode == MODE_GRAPH_TRACE)         { if (handle_trace_mode(t))         return; }
    if (current_mode == MODE_MODE_SCREEN)         { if (handle_mode_screen(t))        return; }
    if (current_mode == MODE_MATH_MENU)           { if (handle_math_menu(t))          return; }
    if (current_mode == MODE_TEST_MENU)           { if (handle_test_menu(t))          return; }
    if (current_mode == MODE_MATRIX_MENU)         { if (handle_matrix_menu(t))        return; }
    if (current_mode == MODE_MATRIX_EDIT)         { handle_matrix_edit(t); return; }
    if (current_mode == MODE_PRGM_MENU)           { if (handle_prgm_menu(t))          return; }
    if (current_mode == MODE_PRGM_NEW_NAME)       { if (handle_prgm_new_name(t))      return; }
    if (current_mode == MODE_PRGM_EDITOR)         { if (handle_prgm_editor(t))        return; }
    if (current_mode == MODE_PRGM_CTL_MENU)       { if (handle_prgm_ctl_menu(t))      return; }
    if (current_mode == MODE_PRGM_IO_MENU)        { if (handle_prgm_io_menu(t))       return; }

    if (sto_pending) { if (handle_sto_pending(t)) return; }

    handle_normal_mode(t);
}

/*---------------------------------------------------------------------------
 * Keypad event handler
 *---------------------------------------------------------------------------*/

/**
 * @brief Translates a hardware key ID into a token and posts it to the queue.
 * @param key_id  Raw hardware key identifier from Keypad_Scan().
 */
void Process_Hardware_Key(uint8_t key_id)
{
    if (key_id == 0)
        return;

    if (key_id >= TI81_LookupTable_Size)
        return;

    KeyDefinition_t key        = TI81_LookupTable[key_id];
    Token_t         token_to_send = TOKEN_NONE;

    if (current_mode == MODE_2ND) {
        token_to_send  = key.second;
        current_mode   = return_mode;   /* restore the mode that was active before 2nd */
        return_mode    = MODE_NORMAL;
        if (token_to_send == TOKEN_NONE) {
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
    } else if (current_mode == MODE_ALPHA) {
        token_to_send  = key.alpha;
        current_mode   = return_mode;   /* restore the mode that was active before ALPHA */
        return_mode    = MODE_NORMAL;
        if (token_to_send == TOKEN_NONE) {
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
    } else if (current_mode == MODE_ALPHA_LOCK) {
        if (key.normal == TOKEN_ALPHA) {
            /* ALPHA pressed while locked — exit alpha lock */
            current_mode = return_mode;
            return_mode  = MODE_NORMAL;
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
        token_to_send = key.alpha;  /* stay locked — do not restore mode */
    } else if (sto_pending) {
        /* STO implicitly uses the alpha layer for the destination key */
        token_to_send = key.alpha;
    } else {
        token_to_send  = key.normal;
    }

    /* Handle sticky modifier keys — pressing again cancels the mode */
    if (token_to_send == TOKEN_2ND) {
        if (current_mode == MODE_2ND) {
            current_mode = return_mode;
            return_mode  = MODE_NORMAL;
        } else {
            return_mode  = current_mode;
            current_mode = MODE_2ND;
        }
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_ALPHA) {
        if (current_mode == MODE_ALPHA) {
            current_mode = return_mode;
            return_mode  = MODE_NORMAL;
        } else {
            return_mode  = current_mode;
            current_mode = MODE_ALPHA;
        }
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_A_LOCK) {
        return_mode  = current_mode;
        current_mode = MODE_ALPHA_LOCK;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }

    if (token_to_send != TOKEN_NONE) {
        if (xQueueSend(keypadQueueHandle, &token_to_send, 0) != pdPASS) {
            /* Queue full — keypress dropped */
        }
    }
}

/*---------------------------------------------------------------------------
 * FreeRTOS task
 *---------------------------------------------------------------------------*/

/**
 * @brief Calculator core task.
 *        Waits for LVGL initialisation, creates the UI, then processes
 *        keypad tokens from the queue indefinitely.
 */
void StartCalcCoreTask(void const *argument)
{
    (void)argument;
    xSemaphoreTake(xLVGL_Ready, portMAX_DELAY);

    lvgl_lock();
    ui_init_styles();
    ui_init_screen();
    ui_init_graph_screens();
    ui_init_mode_screen();
    ui_init_math_screen();
    ui_init_test_screen();
    ui_init_matrix_screen();
    ui_init_prgm_screens();
    cursor_timer = lv_timer_create(cursor_timer_cb, CURSOR_BLINK_MS, NULL);
    ui_update_zoom_display();   /* populate ZOOM labels with initial scroll=0 */
    ui_update_mode_display();
    ui_update_math_display();
    ui_update_test_display();
    ui_update_matrix_display();
    ui_update_matrix_edit_display();
    /* Load programs from FLASH sector 11 before populating the PRGM menu */
    Prgm_Init();

    ui_refresh_display();

    /* Restore saved state from FLASH sector 10 if present and valid */
    {
        PersistBlock_t saved;
        if (Persist_Load(&saved)) {
            Calc_ApplyPersistBlock(&saved);
            ui_refresh_display();   /* show loaded ANS in expression display */
            /* Sync Y= labels with loaded equations */
            for (int i = 0; i < GRAPH_NUM_EQ; i++) {
                lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
            }
            /* Sync MODE screen cursor/highlight with loaded mode_committed */
            ui_update_mode_display();
            /* Sync RANGE field labels with loaded graph_state values */
            ui_update_range_display();
            /* Sync ZOOM FACTORS labels with loaded zoom_x_fact / zoom_y_fact */
            ui_update_zoom_factors_display();
        }
    }

    lvgl_unlock();

    if (keypadQueueHandle == NULL) {
        vTaskDelete(NULL);
        return;
    }

    Token_t received_token;
    for (;;) {
        if (xQueueReceive(keypadQueueHandle, &received_token,
                          portMAX_DELAY) == pdPASS) {
            Execute_Token(received_token);
        }
    }
}