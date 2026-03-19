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
#include "cmsis_os.h"
#include "lvgl.h"
#include "main.h"
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
#define MAX_RESULT_LEN      32

/* Scrollable menu geometry */
#define ZOOM_ITEM_COUNT     8           /* Total ZOOM items */
#define MENU_VISIBLE_ROWS   7           /* Item rows visible below tab bar (8 rows - 1 tab) */
#define MODE_ROW_COUNT      8           /* Rows in the MODE screen */
#define MODE_MAX_COLS       11          /* Max options per MODE row (row 1 has 11) */
#define MATH_TAB_COUNT      4           /* MATH menu tabs: MATH NUM HYP PRB */
#define TEST_ITEM_COUNT     6           /* TEST menu items: = ≠ > ≥ < ≤ */

#define MATRIX_COUNT        3           /* Number of matrices: [A], [B], [C] */
#define MATRIX_MAX_DIM      3           /* Fixed 3x3 for now */
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
    char expression[MAX_EXPR_LEN];
    char result[MAX_RESULT_LEN];
} HistoryEntry_t;

typedef struct {
    float   data[MATRIX_MAX_DIM][MATRIX_MAX_DIM];
    uint8_t rows;
    uint8_t cols;
} Matrix_t;

/*---------------------------------------------------------------------------
 * Private variables
 *---------------------------------------------------------------------------*/

/* LVGL objects — main display */
static lv_obj_t *disp_rows[DISP_ROW_COUNT]; /* Full-width text rows (Montserrat 24) */

/* Cursor blink state */
static bool        cursor_visible = true;
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
static char         expression[MAX_EXPR_LEN];
static uint8_t      expr_len       = 0;
static uint8_t      cursor_pos     = 0;  /* Insertion point, 0–expr_len */
static uint8_t      expr_chars_per_row = 22; /* Chars that fit on one display row; set at init */
static bool         insert_mode    = false; /* false=overwrite (default), true=insert */
static CalcMode_t   current_mode   = MODE_NORMAL;
static bool         angle_degrees  = true;
static float        ans            = 0.0f;
static bool         sto_pending    = false;  /* True after STO — next alpha stores ans */

static HistoryEntry_t history[HISTORY_LINE_COUNT];
static uint8_t        history_count = 0;
static int8_t         history_recall_offset = 0; /* 0=not recalling; N=Nth-most-recent entry */

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
static CalcMode_t return_mode = MODE_NORMAL;

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
static lv_obj_t *ui_mode_screen                          = NULL;
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
/* Display name shown in menu (number prefix added dynamically) */
static const char * const math_display_names[MATH_TAB_COUNT][8] = {
    {"R>P(",   "P>R(",   "^3",       "^(1/3)",
     "!",      "deg",    "rad",      "nDeriv("},
    {"Round(", "IPart(", "FPart(",   "Int(",    NULL, NULL, NULL, NULL},
    {"sinh(",  "cosh(",  "tanh(",    "asinh(",
     "acosh(", "atanh(", NULL, NULL},
    {"Rand",   "nPr",    "nCr",      NULL, NULL, NULL, NULL, NULL},
};
/* String inserted into expression when item is selected */
static const char * const math_insert_strings[MATH_TAB_COUNT][8] = {
    {"R>P(",   "P>R(",   "^3",       "^(1/3)",
     "!",      "\xC2\xB0", "r",     "nDeriv("},
    {"round(", "iPart(", "fPart(",   "int(",    NULL, NULL, NULL, NULL},
    {"sinh(",  "cosh(",  "tanh(",    "asinh(",  "acosh(", "atanh(", NULL, NULL},
    {"rand",   " nPr ",  " nCr ",    NULL, NULL, NULL, NULL, NULL},
};

/* TEST menu state */
static lv_obj_t  *ui_test_screen                   = NULL;
static uint8_t    test_item_cursor                  = 0;
static CalcMode_t test_return_mode                  = MODE_NORMAL;
static lv_obj_t  *test_title_label                  = NULL;
static lv_obj_t  *test_item_labels[TEST_ITEM_COUNT];

/* TEST menu data */
static const char * const test_display_names[TEST_ITEM_COUNT] = {
    "=",
    "\xE2\x89\xA0",   /* U+2260 ≠ */
    ">",
    "\xE2\x89\xA5",   /* U+2265 ≥ */
    "<",
    "\xE2\x89\xA4",   /* U+2264 ≤ */
};
static const char * const test_insert_strings[TEST_ITEM_COUNT] = {
    "=",
    "\xE2\x89\xA0",
    ">",
    "\xE2\x89\xA5",
    "<",
    "\xE2\x89\xA4",
};

/* Matrix data — rows/cols initialised to MATRIX_MAX_DIM, values zero */
static Matrix_t matrices[MATRIX_COUNT] = {
    { .rows = MATRIX_MAX_DIM, .cols = MATRIX_MAX_DIM },
    { .rows = MATRIX_MAX_DIM, .cols = MATRIX_MAX_DIM },
    { .rows = MATRIX_MAX_DIM, .cols = MATRIX_MAX_DIM },
};

/* MATRIX menu state */
static lv_obj_t    *ui_matrix_screen       = NULL;
static uint8_t      matrix_tab             = 0;   /* 0=MATRX, 1=EDIT */
static uint8_t      matrix_item_cursor     = 0;
static CalcMode_t   matrix_return_mode     = MODE_NORMAL;
static lv_obj_t    *matrix_tab_labels[MATRIX_TAB_COUNT];
static lv_obj_t    *matrix_item_labels[MENU_VISIBLE_ROWS];

/* MATRIX EDIT sub-screen state */
static lv_obj_t    *ui_matrix_edit_screen  = NULL;
static uint8_t      matrix_edit_idx        = 0;   /* 0=[A], 1=[B], 2=[C] */
static uint8_t      matrix_edit_row        = 0;
static uint8_t      matrix_edit_col        = 0;
static char         matrix_edit_buf[16]    = {0};
static uint8_t      matrix_edit_len        = 0;
static lv_obj_t    *matrix_edit_title_lbl  = NULL;
static lv_obj_t    *matrix_cell_labels[MATRIX_MAX_DIM][MATRIX_MAX_DIM];

/* MATRIX menu data */
static const char * const matrix_tab_names[MATRIX_TAB_COUNT]     = {"MATRX", "EDIT"};
static const uint8_t matrix_tab_item_count[MATRIX_TAB_COUNT]     = {MATRIX_MATRX_ITEMS, MATRIX_EDIT_ITEMS};
static const char * const matrix_op_names[MATRIX_MATRX_ITEMS]   = {
    "RowSwap(", "Row+(", "*Row(", "*Row+(", "det(", "T"
};
static const char * const matrix_op_insert[MATRIX_MATRX_ITEMS]  = {
    "rowSwap(", "row+(", "*row(", "*row+(", "det(", "T"
};
static const char * const matrix_edit_item_names[MATRIX_EDIT_ITEMS] = {"[A]", "[B]", "[C]"};

/*---------------------------------------------------------------------------
 * Forward declarations for display helpers defined later in this file
 *---------------------------------------------------------------------------*/

static void ui_update_zoom_display(void);
static void ui_update_mode_display(void);
static void ui_update_math_display(void);
static void ui_update_test_display(void);
static void ui_update_matrix_display(void);
static void ui_update_matrix_edit_display(void);
static void zoom_factors_reset(void);
static void ui_update_zoom_factors_display(void);
static void zoom_factors_update_highlight(void);
static void zoom_factors_cursor_update(void);

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

    /* Re-derive state that is computed from mode_committed */
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
static void cursor_box_create(lv_obj_t *parent, bool start_hidden,
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
    /* x=18 = 4(left margin) + 14(one char advance at 24px monospaced) */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        zoom_scroll_ind[i] = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(zoom_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(zoom_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(zoom_scroll_ind[i], lv_color_hex(0xFFAA00), 0);
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
    ui_mode_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_mode_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_mode_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_mode_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_mode_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_mode_screen, 0, 0);
    lv_obj_clear_flag(ui_mode_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);

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
    ui_math_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_math_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_math_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_math_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_math_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_math_screen, 0, 0);
    lv_obj_clear_flag(ui_math_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);

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

    /* Scroll indicator overlays — amber arrow shown over the digit's colon slot */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        math_scroll_ind[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(math_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_scroll_ind[i], lv_color_hex(0xFFAA00), 0);
        lv_label_set_text(math_scroll_ind[i], "");
        lv_obj_add_flag(math_scroll_ind[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* Creates the TEST menu screen (hidden at startup). */
static void ui_init_test_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    ui_test_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_test_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_test_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_test_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_test_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_test_screen, 0, 0);
    lv_obj_clear_flag(ui_test_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);

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

/* Creates the MATRIX menu and MATRIX EDIT sub-screen (both hidden at startup). */
static void ui_init_matrix_screen(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* --- MATRIX menu screen --- */
    ui_matrix_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_matrix_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_matrix_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_matrix_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_matrix_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_matrix_screen, 0, 0);
    lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);

    /* Tab bar: MATRX  EDIT */
    static const int16_t matrix_tab_x[MATRIX_TAB_COUNT] = {4, 100};
    for (int i = 0; i < MATRIX_TAB_COUNT; i++) {
        matrix_tab_labels[i] = lv_label_create(ui_matrix_screen);
        lv_obj_set_pos(matrix_tab_labels[i], matrix_tab_x[i], 4);
        lv_obj_set_style_text_font(matrix_tab_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(matrix_tab_labels[i], lv_color_hex(0x666666), 0);
        lv_label_set_text(matrix_tab_labels[i], matrix_tab_names[i]);
    }

    /* Item labels — text set by ui_update_matrix_display() */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        matrix_item_labels[i] = lv_label_create(ui_matrix_screen);
        lv_obj_set_pos(matrix_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(matrix_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(matrix_item_labels[i], lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(matrix_item_labels[i], "");
    }

    /* --- MATRIX EDIT sub-screen --- */
    ui_matrix_edit_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_matrix_edit_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_matrix_edit_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_matrix_edit_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_matrix_edit_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_matrix_edit_screen, 0, 0);
    lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);

    /* Title label: "[A] 3x3" — text updated when editor opens */
    matrix_edit_title_lbl = lv_label_create(ui_matrix_edit_screen);
    lv_obj_set_pos(matrix_edit_title_lbl, 4, 4);
    lv_obj_set_style_text_font(matrix_edit_title_lbl, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(matrix_edit_title_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(matrix_edit_title_lbl, "[A] 3x3");

    /* Cell labels — 3x3 grid.
     * 3 equal columns across 320px: col 0 at x=4, col 1 at x=110, col 2 at x=216.
     * Each row 30px tall starting at y=34 (below title row). */
    static const int16_t cell_col_x[MATRIX_MAX_DIM] = {4, 110, 216};
    for (int r = 0; r < MATRIX_MAX_DIM; r++) {
        for (int c = 0; c < MATRIX_MAX_DIM; c++) {
            matrix_cell_labels[r][c] = lv_label_create(ui_matrix_edit_screen);
            lv_obj_set_pos(matrix_cell_labels[r][c], cell_col_x[c], 34 + r * 30);
            lv_obj_set_width(matrix_cell_labels[r][c], 100);
            lv_obj_set_style_text_font(matrix_cell_labels[r][c], &jetbrains_mono_24, 0);
            lv_obj_set_style_text_color(matrix_cell_labels[r][c], lv_color_hex(0xFFFFFF), 0);
            lv_label_set_long_mode(matrix_cell_labels[r][c], LV_LABEL_LONG_CLIP);
            lv_label_set_text(matrix_cell_labels[r][c], "0");
        }
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
static void cursor_place(lv_obj_t *cbox, lv_obj_t *cinner,
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
static void ui_refresh_display(void)
{
    if (disp_rows[0] == NULL) return;

    int cpr = (int)expr_chars_per_row;
    int expr_rows = (expr_len == 0) ? 1 : (expr_len + cpr - 1) / cpr;

    /* Number of history entries visible (circular buffer cap) */
    int num_entries = ((int)history_count < HISTORY_LINE_COUNT)
                      ? (int)history_count : HISTORY_LINE_COUNT;

    /* Total display lines consumed by history (variable per entry) */
    int total_history_lines = 0;
    for (int d = 0; d < num_entries; d++) {
        int idx = (int)((history_count - num_entries + d) % HISTORY_LINE_COUNT);
        int elen = (int)strlen(history[idx].expression);
        int erows = (elen == 0) ? 1 : (elen + cpr - 1) / cpr;
        total_history_lines += erows + 1; /* expression sub-rows + result row */
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
                int elen = (int)strlen(history[idx].expression);
                int erows = (elen == 0) ? 1 : (elen + cpr - 1) / cpr;

                if (li < line + erows) {
                    /* Expression sub-row er of this history entry */
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

                if (li == line) {
                    /* Result row for this history entry */
                    lv_obj_set_style_text_color(disp_rows[row],
                                                lv_color_hex(COLOR_HISTORY_RES), 0);
                    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_RIGHT, 0);
                    lv_label_set_text(disp_rows[row], history[idx].result);
                    break;
                }
                line += 1;
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
        /* Overwrite: remove all bytes of the current UTF-8 char, then write c.
         * Without this, overwriting a 3-byte sequence (e.g. ≥) with one ASCII
         * byte leaves two orphaned continuation bytes that corrupt the string. */
        uint8_t cur_size = utf8_char_size(&expression[cursor_pos]);
        memmove(&expression[cursor_pos + 1],
                &expression[cursor_pos + cur_size],
                expr_len - cursor_pos - cur_size + 1);
        expression[cursor_pos] = c;
        expr_len = expr_len - cur_size + 1;
        cursor_pos++;
    } else {
        /* Insert: shift tail right, then write */
        if (expr_len >= MAX_EXPR_LEN - 1) return;
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
static void expr_delete_at_cursor(void)
{
    if (cursor_pos == 0) return;
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
static void ui_update_history(void)
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
        const char *name = math_display_names[math_tab][idx];
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
        snprintf(buf, sizeof(buf), "%d:%s", i + 1, test_display_names[i]);
        lv_obj_set_style_text_color(test_item_labels[i],
            (i == (int)test_item_cursor) ? lv_color_hex(0xFFFF00) : lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(test_item_labels[i], buf);
    }
}

/* Redraws the MATRIX menu: highlights active tab, populates item rows. */
static void ui_update_matrix_display(void)
{
    for (int i = 0; i < MATRIX_TAB_COUNT; i++) {
        lv_obj_set_style_text_color(matrix_tab_labels[i],
            (i == (int)matrix_tab) ? lv_color_hex(0xFFFF00) : lv_color_hex(0x666666), 0);
    }

    uint8_t item_count = matrix_tab_item_count[matrix_tab];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        if (i < (int)item_count) {
            char buf[32];
            if (matrix_tab == 0) {
                snprintf(buf, sizeof(buf), "%d:%s", i + 1, matrix_op_names[i]);
            } else {
                snprintf(buf, sizeof(buf), "%d:%s %dx%d",
                         i + 1, matrix_edit_item_names[i],
                         matrices[i].rows, matrices[i].cols);
            }
            lv_obj_set_style_text_color(matrix_item_labels[i],
                (i == (int)matrix_item_cursor) ? lv_color_hex(0xFFFF00) : lv_color_hex(0xFFFFFF), 0);
            lv_label_set_text(matrix_item_labels[i], buf);
        } else {
            lv_label_set_text(matrix_item_labels[i], "");
        }
    }
}

/* Redraws the MATRIX EDIT sub-screen: title, all cells, cursor highlight. */
static void ui_update_matrix_edit_display(void)
{
    Matrix_t *m = &matrices[matrix_edit_idx];
    char title_buf[20];
    snprintf(title_buf, sizeof(title_buf), "%s %dx%d",
             matrix_edit_item_names[matrix_edit_idx], m->rows, m->cols);
    lv_label_set_text(matrix_edit_title_lbl, title_buf);

    char cell_buf[16];
    for (int r = 0; r < MATRIX_MAX_DIM; r++) {
        for (int c = 0; c < MATRIX_MAX_DIM; c++) {
            bool is_cursor = (r == (int)matrix_edit_row && c == (int)matrix_edit_col);
            if (is_cursor && matrix_edit_len > 0) {
                snprintf(cell_buf, sizeof(cell_buf), "%s", matrix_edit_buf);
            } else {
                Calc_FormatResult(m->data[r][c], cell_buf, sizeof(cell_buf));
            }
            lv_obj_set_style_text_color(matrix_cell_labels[r][c],
                is_cursor ? lv_color_hex(0xFFFF00) : lv_color_hex(0xFFFFFF), 0);
            lv_label_set_text(matrix_cell_labels[r][c], cell_buf);
        }
    }
}

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

/**
 * @brief Inserts a MATRIX MATRX-tab operation string into the active destination
 *        and returns to the prior mode (normal or Y= editor).
 */
static void matrix_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (matrix_return_mode == MODE_GRAPH_YEQ) {
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
    matrix_return_mode = MODE_NORMAL;
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

        /* Show "Saving..." overlay before the flash erase begins.
         * Release the mutex immediately so DefaultTask can render one frame
         * (its 5 ms loop runs; lv_timer_handler flushes the label to the SDRAM
         * framebuffer).  Then delay 20 ms to ensure at least one full render
         * pass completes before we call Persist_Save.
         * LTDC reads directly from SDRAM so the label stays visible for the
         * entire ~1-2 s FLASH stall even though all CPU execution is frozen. */
        lvgl_lock();
        lv_obj_t *saving_lbl = lv_label_create(lv_scr_act());
        lv_label_set_text(saving_lbl, "Saving...");
        lv_obj_set_style_text_color(saving_lbl, lv_color_hex(0xFFAA00), 0);
        lv_obj_align(saving_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
        lvgl_unlock();
        osDelay(20);  /* let DefaultTask flush the label to the framebuffer */

        PersistBlock_t block;
        Calc_BuildPersistBlock(&block);
        Persist_Save(&block);   /* ← FLASH stalls here for ~1-2 s */

        /* Remove indicator and reset modifier state */
        current_mode = MODE_NORMAL;
        return_mode  = MODE_NORMAL;
        sto_pending  = false;
        lvgl_lock();
        lv_obj_del(saving_lbl);
        ui_update_status_bar();
        lvgl_unlock();

        if (power_down) {
            /* 2nd+ON — enter Stop mode; returns after ON button wakes the CPU */
            Power_EnterStop();
            /* Force a full LVGL redraw on wake (SDRAM content survives
             * self-refresh but LVGL dirty-region state may need a nudge) */
            lvgl_lock();
            lv_obj_invalidate(lv_scr_act());
            lvgl_unlock();
        }
        return;
    }

    /*--- Y= equation editor mode handler -----------------------------------*/
    if (current_mode == MODE_GRAPH_YEQ) {
        char *eq  = graph_state.equations[yeq_selected];
        uint8_t eq_len = strlen(eq);
        const char *append = NULL;
        char num_buf[2] = {0, 0};

        /* Clamp cursor to current equation length */
        if (yeq_cursor_pos > eq_len) yeq_cursor_pos = eq_len;

        switch (t) {
        case TOKEN_GRAPH:
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_Render(angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_Y_EQUALS:
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        case TOKEN_CLEAR:
            /* Clear the current equation */
            eq[0]          = '\0';
            yeq_cursor_pos = 0;
            lvgl_lock();
            lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_LEFT:
            if (yeq_cursor_pos > 0) {
                /* Step back past all UTF-8 continuation bytes (10xxxxxx) to
                 * land on the start byte of the previous character. */
                do { yeq_cursor_pos--; }
                while (yeq_cursor_pos > 0 &&
                       ((uint8_t)eq[yeq_cursor_pos] & 0xC0) == 0x80);
            }
            lvgl_lock();
            yeq_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            if (yeq_cursor_pos < eq_len) {
                uint8_t step = utf8_char_size(&eq[yeq_cursor_pos]);
                yeq_cursor_pos += step ? step : 1;
                if (yeq_cursor_pos > (uint8_t)eq_len) yeq_cursor_pos = (uint8_t)eq_len;
            }
            lvgl_lock();
            yeq_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_INS:
            insert_mode = !insert_mode;
            lvgl_lock();
            yeq_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_RANGE:
            current_mode = MODE_GRAPH_RANGE;
            range_field_reset();
            graph_state.active = false;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_range_display();
            range_update_highlight();
            range_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_ZOOM:
            current_mode = MODE_GRAPH_ZOOM;
            zoom_menu_reset();
            graph_state.active = false;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        case TOKEN_TRACE:
            trace_eq_idx = find_first_active_eq();
            trace_x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
            current_mode = MODE_GRAPH_TRACE;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_DEL:
            if (yeq_cursor_pos > 0) {
                /* Find the start byte of the previous UTF-8 character by
                 * stepping back past any continuation bytes (10xxxxxx). */
                uint8_t prev = yeq_cursor_pos;
                do { prev--; }
                while (prev > 0 && ((uint8_t)eq[prev] & 0xC0) == 0x80);
                memmove(&eq[prev], &eq[yeq_cursor_pos],
                        eq_len - yeq_cursor_pos + 1);
                yeq_cursor_pos = prev;
                lvgl_lock();
                lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
                yeq_reflow_rows();
                yeq_cursor_update();
                lvgl_unlock();
            }
            return;
        case TOKEN_UP:
            if (yeq_selected > 0) yeq_selected--;
            yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
            lvgl_lock();
            yeq_update_highlight();
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (yeq_selected < GRAPH_NUM_EQ - 1) yeq_selected++;
            yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
            lvgl_lock();
            yeq_update_highlight();
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
            return;
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
            math_return_mode   = MODE_GRAPH_YEQ;
            math_tab           = 0;
            math_item_cursor   = 0;
            math_scroll_offset = 0;
            current_mode       = MODE_MATH_MENU;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_math_display();
            lvgl_unlock();
            return;
        case TOKEN_TEST:
            test_return_mode  = MODE_GRAPH_YEQ;
            test_item_cursor  = 0;
            current_mode      = MODE_TEST_MENU;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_test_display();
            lvgl_unlock();
            return;
        case TOKEN_MATRX:
            matrix_return_mode = MODE_GRAPH_YEQ;
            matrix_tab         = 0;
            matrix_item_cursor = 0;
            current_mode       = MODE_MATRIX_MENU;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_matrix_display();
            lvgl_unlock();
            return;
        case TOKEN_MTRX_A: append = "[A]"; break;
        case TOKEN_MTRX_B: append = "[B]"; break;
        case TOKEN_MTRX_C: append = "[C]"; break;
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
                /* Overwrite the character at cursor position.  If it is a
                 * multi-byte UTF-8 sequence (e.g. √, π) remove all its bytes
                 * before placing the single incoming ASCII character. */
                uint8_t cur_size = utf8_char_size(&eq[yeq_cursor_pos]);
                if (cur_size <= 1) {
                    eq[yeq_cursor_pos++] = append[0];
                } else {
                    /* Shrink: replace cur_size bytes with 1 ASCII byte */
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
                /* Insert: shift tail right, then copy */
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
        /* Clear any stale 2nd/ALPHA indicator (modifier was consumed by
         * Process_Hardware_Key before this token was queued) */
        lvgl_lock();
        ui_update_status_bar();
        yeq_cursor_update();
        lvgl_unlock();
        return;
    }

    /*--- RANGE field editor mode handler -----------------------------------*/
    if (current_mode == MODE_GRAPH_RANGE) {
        switch (t) {
        case TOKEN_0 ... TOKEN_9: {
            char ch = (char)((t - TOKEN_0) + '0');
            if (!insert_mode && range_field_cursor_pos < range_field_len) {
                /* Overwrite the character at cursor position */
                range_field_buf[range_field_cursor_pos++] = ch;
            } else if (range_field_len < sizeof(range_field_buf) - 1) {
                /* Insert: shift tail right */
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
            return;
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
            return;

        case TOKEN_NEG:
            /* Toggle leading '-' sign; move cursor to end of modified buffer */
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
            return;

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
            return;

        case TOKEN_LEFT:
            if (range_field_cursor_pos > 0) range_field_cursor_pos--;
            lvgl_lock();
            range_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_RIGHT:
            if (range_field_cursor_pos < range_field_len) range_field_cursor_pos++;
            lvgl_lock();
            range_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_INS:
            insert_mode = !insert_mode;
            lvgl_lock();
            range_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_ENTER:
        case TOKEN_DOWN:
            range_commit_field();
            range_field_len        = 0;
            range_field_buf[0]     = '\0';
            range_field_cursor_pos = 0;
            if (range_field_selected < 6) range_field_selected++;
            lvgl_lock();
            ui_update_range_display();
            range_update_highlight();
            range_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_UP:
            range_commit_field();
            range_field_len        = 0;
            range_field_buf[0]     = '\0';
            range_field_cursor_pos = 0;
            if (range_field_selected > 0) range_field_selected--;
            lvgl_lock();
            ui_update_range_display();
            range_update_highlight();
            range_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_ZOOM:
            range_commit_field();
            range_field_reset();
            current_mode       = MODE_GRAPH_ZOOM;
            zoom_menu_reset();
            graph_state.active = false;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;

        case TOKEN_GRAPH:
            range_commit_field();
            current_mode = MODE_NORMAL;
            range_field_reset();
            lvgl_lock();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_Render(angle_degrees);
            lvgl_unlock();
            return;

        case TOKEN_RANGE:
            current_mode = MODE_NORMAL;
            range_field_reset();
            lvgl_lock();
            ui_update_range_display();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;

        case TOKEN_CLEAR:
            if (range_field_len > 0) {
                /* Clear in-progress edit, show stored value */
                range_field_len        = 0;
                range_field_buf[0]     = '\0';
                range_field_cursor_pos = 0;
                lvgl_lock();
                ui_update_range_display();
                range_cursor_update();
                lvgl_unlock();
            } else {
                /* Field already empty — exit RANGE screen to calculator */
                current_mode = MODE_NORMAL;
                range_field_reset();
                lvgl_lock();
                ui_update_range_display();
                lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
                lvgl_unlock();
            }
            return;

        case TOKEN_Y_EQUALS:
            range_commit_field();
            range_field_reset();
            current_mode = MODE_GRAPH_YEQ;
            graph_state.active     = false;
            yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
            lvgl_lock();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < GRAPH_NUM_EQ; i++)
                lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
            yeq_update_highlight();
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_TRACE:
            range_commit_field();
            range_field_reset();
            trace_eq_idx = find_first_active_eq();
            trace_x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
            current_mode = MODE_GRAPH_TRACE;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
            lvgl_unlock();
            return;

        default:
            lvgl_lock();
            ui_update_status_bar();
            range_cursor_update();
            lvgl_unlock();
            return;
        }
    }

    /*--- ZOOM menu mode handler --------------------------------------------*/
    if (current_mode == MODE_GRAPH_ZOOM) {
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
            return;
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
            return;
        case TOKEN_ENTER:
            zoom_execute_item((uint8_t)(zoom_scroll_offset + zoom_item_cursor + 1));
            return;
        case TOKEN_CLEAR:
        case TOKEN_ZOOM:
            current_mode = MODE_NORMAL;
            zoom_menu_reset();
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        case TOKEN_Y_EQUALS:
            current_mode = MODE_GRAPH_YEQ;
            graph_state.active = false;
            zoom_menu_reset();
            yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < GRAPH_NUM_EQ; i++)
                lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
            yeq_update_highlight();
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_RANGE:
            current_mode = MODE_GRAPH_RANGE;
            range_field_reset();
            zoom_menu_reset();
            graph_state.active = false;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_range_display();
            range_update_highlight();
            range_cursor_update();
            lvgl_unlock();
            return;
        case TOKEN_GRAPH:
            zoom_menu_reset();
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_Render(angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_TRACE:
            trace_eq_idx = find_first_active_eq();
            trace_x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
            current_mode = MODE_GRAPH_TRACE;
            zoom_menu_reset();
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_1 ... TOKEN_9: {
            uint8_t item = (uint8_t)(t - TOKEN_0); /* 1–9 */
            if (item <= ZOOM_ITEM_COUNT)
                zoom_execute_item(item);
            return;
        }
        default:
            /* Any unrecognized key exits the ZOOM menu */
            current_mode = MODE_NORMAL;
            zoom_menu_reset();
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        }
    }

    /*--- ZOOM FACTORS sub-screen handler -----------------------------------*/
    if (current_mode == MODE_GRAPH_ZOOM_FACTORS) {
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
            return;
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
            return;

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
            return;

        case TOKEN_LEFT:
            if (zoom_factors_cursor > 0) zoom_factors_cursor--;
            lvgl_lock();
            zoom_factors_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_RIGHT:
            if (zoom_factors_cursor < zoom_factors_len) zoom_factors_cursor++;
            lvgl_lock();
            zoom_factors_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_INS:
            insert_mode = !insert_mode;
            lvgl_lock();
            zoom_factors_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_ENTER:
        case TOKEN_DOWN:
            zoom_factors_commit_field();
            zoom_factors_len        = 0;
            zoom_factors_buf[0]     = '\0';
            zoom_factors_cursor     = 0;
            if (zoom_factors_field < 1) zoom_factors_field++;
            lvgl_lock();
            ui_update_zoom_factors_display();
            zoom_factors_update_highlight();
            zoom_factors_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_UP:
            zoom_factors_commit_field();
            zoom_factors_len        = 0;
            zoom_factors_buf[0]     = '\0';
            zoom_factors_cursor     = 0;
            if (zoom_factors_field > 0) zoom_factors_field--;
            lvgl_lock();
            ui_update_zoom_factors_display();
            zoom_factors_update_highlight();
            zoom_factors_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_ZOOM:
            /* Commit and return to ZOOM menu */
            zoom_factors_commit_field();
            zoom_factors_reset();
            current_mode = MODE_GRAPH_ZOOM;
            zoom_menu_reset();
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_zoom_display();
            lvgl_unlock();
            return;

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
                /* Field already empty — return to ZOOM menu */
                zoom_factors_reset();
                current_mode = MODE_GRAPH_ZOOM;
                zoom_menu_reset();
                lvgl_lock();
                lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
                ui_update_zoom_display();
                lvgl_unlock();
            }
            return;

        case TOKEN_Y_EQUALS:
            zoom_factors_commit_field();
            zoom_factors_reset();
            current_mode   = MODE_GRAPH_YEQ;
            graph_state.active = false;
            yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < GRAPH_NUM_EQ; i++)
                lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
            yeq_update_highlight();
            yeq_reflow_rows();
            yeq_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_RANGE:
            zoom_factors_commit_field();
            zoom_factors_reset();
            current_mode = MODE_GRAPH_RANGE;
            range_field_reset();
            graph_state.active = false;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(false);
            lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_range_display();
            range_update_highlight();
            range_cursor_update();
            lvgl_unlock();
            return;

        case TOKEN_GRAPH:
            zoom_factors_commit_field();
            zoom_factors_reset();
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_Render(angle_degrees);
            lvgl_unlock();
            return;

        case TOKEN_TRACE:
            zoom_factors_commit_field();
            zoom_factors_reset();
            trace_eq_idx = find_first_active_eq();
            trace_x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
            current_mode = MODE_GRAPH_TRACE;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_factors_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
            lvgl_unlock();
            return;

        default:
            lvgl_lock();
            zoom_factors_cursor_update();
            lvgl_unlock();
            return;
        }
    }

    /*--- ZBOX rubber-band zoom handler -------------------------------------*/
    if (current_mode == MODE_GRAPH_ZBOX) {
        switch (t) {
        case TOKEN_LEFT:
            if (zbox_px > 0) zbox_px--;
            lvgl_lock();
            Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            if (zbox_px < GRAPH_W - 1) zbox_px++;
            lvgl_lock();
            Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_UP:
            if (zbox_py > 0) zbox_py--;
            lvgl_lock();
            Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (zbox_py < GRAPH_H - 1) zbox_py++;
            lvgl_lock();
            Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_ENTER:
            if (!zbox_corner1_set) {
                /* Lock the first corner and start drawing the rectangle */
                zbox_px1         = zbox_px;
                zbox_py1         = zbox_py;
                zbox_corner1_set = true;
                lvgl_lock();
                Graph_DrawZBox(zbox_px, zbox_py, zbox_px1, zbox_py1, zbox_corner1_set, angle_degrees);
                lvgl_unlock();
            } else {
                /* Commit: map pixel box corners to math window coordinates */
                int32_t x_lo = zbox_px1 < zbox_px ? zbox_px1 : zbox_px;
                int32_t x_hi = zbox_px1 < zbox_px ? zbox_px  : zbox_px1;
                int32_t y_lo = zbox_py1 < zbox_py ? zbox_py1 : zbox_py;  /* smaller py = larger y_math */
                int32_t y_hi = zbox_py1 < zbox_py ? zbox_py  : zbox_py1;
                /* Only zoom if the box has non-zero area */
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
            return;
        case TOKEN_CLEAR:
            /* Exit to calculator */
            current_mode     = MODE_NORMAL;
            zbox_corner1_set = false;
            lvgl_lock();
            Graph_SetVisible(false);
            lvgl_unlock();
            return;
        case TOKEN_ZOOM:
            /* Cancel ZBox rubber-band and stay on graph */
            current_mode     = MODE_NORMAL;
            zbox_corner1_set = false;
            lvgl_lock();
            Graph_ClearTrace();
            Graph_Render(angle_degrees);
            lvgl_unlock();
            return;
        default:
            /* Any other key cancels ZBox and falls through to normal handling */
            current_mode     = MODE_NORMAL;
            zbox_corner1_set = false;
            lvgl_lock();
            Graph_ClearTrace();
            Graph_Render(angle_degrees);
            lvgl_unlock();
            break;
        }
    }

    /*--- TRACE cursor mode handler -----------------------------------------*/
    if (current_mode == MODE_GRAPH_TRACE) {
        float step = (graph_state.x_max - graph_state.x_min) / (float)(GRAPH_W - 1);
        switch (t) {
        case TOKEN_LEFT:
            if (trace_x > graph_state.x_min) trace_x -= step;
            lvgl_lock();
            Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            if (trace_x < graph_state.x_max) trace_x += step;
            lvgl_lock();
            Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
            lvgl_unlock();
            return;
        case TOKEN_UP:
            /* Cycle to the previous active equation */
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
            return;
        case TOKEN_DOWN:
            /* Cycle to the next active equation */
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
            return;
        case TOKEN_TRACE:
            /* Pressing TRACE again is a no-op while already tracing */
            return;
        default:
            /* Any other key exits trace — clear cursor, re-render clean graph,
             * then fall through to the main switch to process the key normally */
            current_mode = MODE_NORMAL;
            lvgl_lock();
            Graph_ClearTrace();
            Graph_Render(angle_degrees);
            lvgl_unlock();
            break;
        }
    }

    /*--- MODE settings screen handler --------------------------------------*/
    if (current_mode == MODE_MODE_SCREEN) {
        switch (t) {
        case TOKEN_UP:
            if (mode_row_selected > 0) mode_row_selected--;
            lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (mode_row_selected < MODE_ROW_COUNT - 1) mode_row_selected++;
            lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
            return;
        case TOKEN_LEFT:
            if (mode_cursor[mode_row_selected] > 0)
                mode_cursor[mode_row_selected]--;
            lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            if (mode_cursor[mode_row_selected] < mode_option_count[mode_row_selected] - 1)
                mode_cursor[mode_row_selected]++;
            lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
            return;
        case TOKEN_ENTER:
            mode_committed[mode_row_selected] = mode_cursor[mode_row_selected];
            /* Apply settings that are currently wired */
            if (mode_row_selected == 1)
                Calc_SetDecimalMode(mode_committed[1]); /* 0=Float, 1-10=Fix0-9 */
            if (mode_row_selected == 2)
                angle_degrees = (mode_committed[2] == 1); /* 0=Radian, 1=Degree */
            if (mode_row_selected == 6)
                graph_state.grid_on = (mode_committed[6] == 1); /* 0=Grid off, 1=Grid on */
            lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
            return;
        case TOKEN_CLEAR:
        case TOKEN_MODE:
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        default:
            /* Any other key exits the MODE screen and is processed normally */
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            break;
        }
        /* Execution reaches here only from default — fall through to main switch */
    }

    /*--- MATH/NUM/HYP/PRB menu handler -------------------------------------*/
    if (current_mode == MODE_MATH_MENU) {
        int total = (int)math_tab_item_count[math_tab];
        switch (t) {
        case TOKEN_LEFT:
            if (math_tab > 0) { math_tab--; math_item_cursor = 0; math_scroll_offset = 0; }
            lvgl_lock(); ui_update_math_display(); lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            if (math_tab < MATH_TAB_COUNT - 1) { math_tab++; math_item_cursor = 0; math_scroll_offset = 0; }
            lvgl_lock(); ui_update_math_display(); lvgl_unlock();
            return;
        case TOKEN_UP:
            if (math_item_cursor > 0) {
                math_item_cursor--;
            } else if (math_scroll_offset > 0) {
                math_scroll_offset--;
            }
            lvgl_lock(); ui_update_math_display(); lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if ((int)(math_scroll_offset + math_item_cursor) + 1 < total) {
                if (math_item_cursor < MENU_VISIBLE_ROWS - 1)
                    math_item_cursor++;
                else if ((int)(math_scroll_offset + MENU_VISIBLE_ROWS) < total)
                    math_scroll_offset++;
            }
            lvgl_lock(); ui_update_math_display(); lvgl_unlock();
            return;
        case TOKEN_ENTER: {
            int idx = (int)math_scroll_offset + (int)math_item_cursor;
            if (idx < total) {
                const char *ins = math_insert_strings[math_tab][idx];
                if (ins != NULL) { math_menu_insert(ins); return; }
            }
            break;
        }
        case TOKEN_1 ... TOKEN_9: {
            int idx = (int)(t - TOKEN_0) - 1; /* 0-indexed */
            if (idx < total) {
                const char *ins = math_insert_strings[math_tab][idx];
                if (ins != NULL) { math_menu_insert(ins); return; }
            }
            break;
        }
        case TOKEN_CLEAR:
        case TOKEN_MATH:
            current_mode        = math_return_mode;
            math_return_mode    = MODE_NORMAL;
            math_item_cursor    = 0;
            math_scroll_offset  = 0;
            lvgl_lock();
            lv_obj_add_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
            if (current_mode == MODE_GRAPH_YEQ)
                lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        default:
            /* Any other key exits the MATH menu and is processed normally */
            current_mode        = math_return_mode;
            math_return_mode    = MODE_NORMAL;
            math_item_cursor    = 0;
            math_scroll_offset  = 0;
            lvgl_lock();
            lv_obj_add_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
            if (current_mode == MODE_GRAPH_YEQ)
                lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            /* If returning to Y=, don't fall through to the main switch */
            if (current_mode == MODE_GRAPH_YEQ)
                return;
            break;
        }
        /* Execution reaches here only from default — fall through to main switch */
    }

    /*--- TEST menu handler -------------------------------------------------*/
    if (current_mode == MODE_TEST_MENU) {
        switch (t) {
        case TOKEN_UP:
            if (test_item_cursor > 0) test_item_cursor--;
            lvgl_lock(); ui_update_test_display(); lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (test_item_cursor < TEST_ITEM_COUNT - 1) test_item_cursor++;
            lvgl_lock(); ui_update_test_display(); lvgl_unlock();
            return;
        case TOKEN_ENTER: {
            const char *ins = test_insert_strings[test_item_cursor];
            if (ins != NULL) { test_menu_insert(ins); return; }
            break;
        }
        case TOKEN_1 ... TOKEN_6: {
            int idx = (int)(t - TOKEN_0) - 1;
            if (idx >= 0 && idx < TEST_ITEM_COUNT) {
                const char *ins = test_insert_strings[idx];
                if (ins != NULL) { test_menu_insert(ins); return; }
            }
            break;
        }
        case TOKEN_CLEAR:
        case TOKEN_TEST:
            current_mode      = test_return_mode;
            test_return_mode  = MODE_NORMAL;
            test_item_cursor  = 0;
            lvgl_lock();
            lv_obj_add_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
            if (current_mode == MODE_GRAPH_YEQ)
                lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        default:
            current_mode      = test_return_mode;
            test_return_mode  = MODE_NORMAL;
            test_item_cursor  = 0;
            lvgl_lock();
            lv_obj_add_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
            if (current_mode == MODE_GRAPH_YEQ)
                lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            if (current_mode == MODE_GRAPH_YEQ)
                return;
            break;
        }
        /* Fall through to main switch */
    }

    /*--- MATRIX menu handler -----------------------------------------------*/
    if (current_mode == MODE_MATRIX_MENU) {
        switch (t) {
        case TOKEN_LEFT:
            if (matrix_tab > 0) { matrix_tab--; matrix_item_cursor = 0; }
            lvgl_lock(); ui_update_matrix_display(); lvgl_unlock();
            return;
        case TOKEN_RIGHT:
            if (matrix_tab < MATRIX_TAB_COUNT - 1) { matrix_tab++; matrix_item_cursor = 0; }
            lvgl_lock(); ui_update_matrix_display(); lvgl_unlock();
            return;
        case TOKEN_UP:
            if (matrix_item_cursor > 0) matrix_item_cursor--;
            lvgl_lock(); ui_update_matrix_display(); lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (matrix_item_cursor < matrix_tab_item_count[matrix_tab] - 1) matrix_item_cursor++;
            lvgl_lock(); ui_update_matrix_display(); lvgl_unlock();
            return;
        case TOKEN_ENTER: {
            if (matrix_tab == 0) {
                const char *ins = matrix_op_insert[matrix_item_cursor];
                if (ins != NULL) { matrix_menu_insert(ins); }
            } else {
                matrix_edit_idx = matrix_item_cursor;
                matrix_edit_row = 0;
                matrix_edit_col = 0;
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
                current_mode = MODE_MATRIX_EDIT;
                lvgl_lock();
                lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
                ui_update_matrix_edit_display();
                lvgl_unlock();
            }
            return;
        }
        case TOKEN_1 ... TOKEN_6: {
            int idx = (int)(t - TOKEN_0) - 1;
            if (idx >= 0 && idx < (int)matrix_tab_item_count[matrix_tab]) {
                matrix_item_cursor = (uint8_t)idx;
                if (matrix_tab == 0) {
                    const char *ins = matrix_op_insert[idx];
                    if (ins != NULL) { matrix_menu_insert(ins); }
                } else {
                    matrix_edit_idx = (uint8_t)idx;
                    matrix_edit_row = 0;
                    matrix_edit_col = 0;
                    matrix_edit_len = 0;
                    matrix_edit_buf[0] = '\0';
                    current_mode = MODE_MATRIX_EDIT;
                    lvgl_lock();
                    lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
                    ui_update_matrix_edit_display();
                    lvgl_unlock();
                }
            }
            return;
        }
        case TOKEN_CLEAR:
        case TOKEN_MATRX: {
            CalcMode_t ret = matrix_return_mode;
            matrix_return_mode = MODE_NORMAL;
            current_mode = ret;
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
            if (ret == MODE_GRAPH_YEQ)
                lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        }
        default:
            matrix_return_mode = MODE_NORMAL;
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            break;
        }
        /* Fall through to main switch for unhandled keys */
    }

    /*--- MATRIX EDIT handler -----------------------------------------------*/
    if (current_mode == MODE_MATRIX_EDIT) {
        Matrix_t *m = &matrices[matrix_edit_idx];
        switch (t) {
        case TOKEN_0 ... TOKEN_9:
            if (matrix_edit_len < (uint8_t)(sizeof(matrix_edit_buf) - 1)) {
                matrix_edit_buf[matrix_edit_len++] = (char)((t - TOKEN_0) + '0');
                matrix_edit_buf[matrix_edit_len]   = '\0';
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_DECIMAL:
            if (matrix_edit_len < (uint8_t)(sizeof(matrix_edit_buf) - 1) &&
                strchr(matrix_edit_buf, '.') == NULL) {
                matrix_edit_buf[matrix_edit_len++] = '.';
                matrix_edit_buf[matrix_edit_len]   = '\0';
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_NEG:
            if (matrix_edit_len > 0 && matrix_edit_buf[0] == '-') {
                memmove(matrix_edit_buf, matrix_edit_buf + 1, matrix_edit_len);
                matrix_edit_len--;
            } else if (matrix_edit_len < (uint8_t)(sizeof(matrix_edit_buf) - 1)) {
                memmove(matrix_edit_buf + 1, matrix_edit_buf, matrix_edit_len + 1);
                matrix_edit_buf[0] = '-';
                matrix_edit_len++;
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_DEL:
            if (matrix_edit_len > 0) {
                matrix_edit_buf[--matrix_edit_len] = '\0';
                lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            }
            return;
        case TOKEN_ENTER:
        case TOKEN_RIGHT: {
            /* Commit cell and advance right, wrapping to next row */
            if (matrix_edit_len > 0) {
                m->data[matrix_edit_row][matrix_edit_col] = strtof(matrix_edit_buf, NULL);
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
            }
            matrix_edit_col++;
            if (matrix_edit_col >= m->cols) {
                matrix_edit_col = 0;
                if (matrix_edit_row < m->rows - 1) matrix_edit_row++;
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        }
        case TOKEN_LEFT: {
            if (matrix_edit_len > 0) {
                m->data[matrix_edit_row][matrix_edit_col] = strtof(matrix_edit_buf, NULL);
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
            }
            if (matrix_edit_col > 0) {
                matrix_edit_col--;
            } else if (matrix_edit_row > 0) {
                matrix_edit_row--;
                matrix_edit_col = m->cols - 1;
            }
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        }
        case TOKEN_UP:
            if (matrix_edit_len > 0) {
                m->data[matrix_edit_row][matrix_edit_col] = strtof(matrix_edit_buf, NULL);
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
            }
            if (matrix_edit_row > 0) matrix_edit_row--;
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (matrix_edit_len > 0) {
                m->data[matrix_edit_row][matrix_edit_col] = strtof(matrix_edit_buf, NULL);
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
            }
            if (matrix_edit_row < m->rows - 1) matrix_edit_row++;
            lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            return;
        case TOKEN_CLEAR:
            if (matrix_edit_len > 0) {
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
                lvgl_lock(); ui_update_matrix_edit_display(); lvgl_unlock();
            } else {
                /* Back to MATRIX menu */
                current_mode = MODE_MATRIX_MENU;
                lvgl_lock();
                lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
                ui_update_matrix_display();
                lvgl_unlock();
            }
            return;
        case TOKEN_MATRX:
            /* Commit current cell and return to MATRIX menu */
            if (matrix_edit_len > 0) {
                m->data[matrix_edit_row][matrix_edit_col] = strtof(matrix_edit_buf, NULL);
                matrix_edit_len = 0;
                matrix_edit_buf[0] = '\0';
            }
            current_mode = MODE_MATRIX_MENU;
            lvgl_lock();
            lv_obj_add_flag(ui_matrix_edit_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
            ui_update_matrix_display();
            lvgl_unlock();
            return;
        default:
            return; /* Ignore other keys in matrix editor */
        }
    }

    /*--- STO pending handler -----------------------------------------------*/
    /* When sto_pending is set, the next alpha key stores ans to that variable */
    if (sto_pending) {
        if (t >= TOKEN_A && t <= TOKEN_Z) {
            calc_variables[t - TOKEN_A] = ans;
            sto_pending = false;
            /* Show assignment as a history entry */
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
            return;
        } else if (t == TOKEN_CLEAR || t == TOKEN_2ND || t == TOKEN_ALPHA) {
            /* Cancel STO with CLEAR, 2nd, or ALPHA */
            sto_pending = false;
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
        /* Any other key cancels STO silently and falls through */
        sto_pending = false;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
    }

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
            /* Step back past all UTF-8 continuation bytes (10xxxxxx) to land
             * on the start byte of the previous character. */
            do { cursor_pos--; }
            while (cursor_pos > 0 &&
                   ((uint8_t)expression[cursor_pos] & 0xC0) == 0x80);
            Update_Calculator_Display();
        }
        break;

    case TOKEN_RIGHT:
        if (cursor_pos < expr_len) {
            /* Step forward past all continuation bytes to land after the
             * current character's full UTF-8 sequence. */
            cursor_pos++;
            while (cursor_pos < expr_len &&
                   ((uint8_t)expression[cursor_pos] & 0xC0) == 0x80)
                cursor_pos++;
            Update_Calculator_Display();
        }
        break;

    case TOKEN_UP:
        /* History recall — UP scrolls back through previous expressions */
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
        /* History recall — DOWN scrolls forward; clears expression at newest */
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
        if (expr_len > 0) {
            CalcResult_t result = Calc_Evaluate(expression, ans,
                                                angle_degrees);

            char result_str[MAX_RESULT_LEN];
            memset(result_str, 0, MAX_RESULT_LEN);

            if (result.error != CALC_OK) {
                strncpy(result_str, result.error_msg, MAX_RESULT_LEN - 1);
                result_str[MAX_RESULT_LEN - 1] = '\0';
            } else {
                Calc_FormatResult(result.value, result_str,
                                  MAX_RESULT_LEN);
                ans = result.value;
            }

            /* Store in history */
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
        /* Sync cursor to current committed value before opening */
        memcpy(mode_cursor, mode_committed, sizeof(mode_cursor));
        mode_row_selected = 0;
        current_mode = MODE_MODE_SCREEN;
        lvgl_lock();
        lv_obj_clear_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_mode_display();
        lvgl_unlock();
        break;

    case TOKEN_MATH:
        math_return_mode   = MODE_NORMAL;
        math_tab           = 0;
        math_item_cursor   = 0;
        math_scroll_offset = 0;
        current_mode = MODE_MATH_MENU;
        lvgl_lock();
        lv_obj_clear_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_math_display();
        lvgl_unlock();
        break;

    case TOKEN_TEST:
        test_return_mode = MODE_NORMAL;
        test_item_cursor = 0;
        current_mode = MODE_TEST_MENU;
        lvgl_lock();
        lv_obj_clear_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_test_display();
        lvgl_unlock();
        break;

    case TOKEN_MATRX:
        matrix_return_mode = MODE_NORMAL;
        matrix_tab         = 0;
        matrix_item_cursor = 0;
        current_mode = MODE_MATRIX_MENU;
        lvgl_lock();
        lv_obj_clear_flag(ui_matrix_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_matrix_display();
        lvgl_unlock();
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
            uint8_t idx = (history_count - 1) % HISTORY_LINE_COUNT;
            strncpy(expression, history[idx].expression, MAX_EXPR_LEN - 1);
            expression[MAX_EXPR_LEN - 1] = '\0';
            expr_len   = (uint8_t)strlen(expression);
            cursor_pos = expr_len;
            Update_Calculator_Display();
        }
        break;

    /* Alpha characters A–Z */
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
        current_mode   = MODE_GRAPH_YEQ;
        graph_state.active = false;
        yeq_cursor_pos = strlen(graph_state.equations[yeq_selected]);
        /* Switch to Y= editor — show equation entry screen */
        lvgl_lock();
        Graph_SetVisible(false);
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
        yeq_update_highlight();
        yeq_reflow_rows();
        yeq_cursor_update();
        lvgl_unlock();
        break;

    case TOKEN_RANGE:
        /* Switch to RANGE editor */
        current_mode = MODE_GRAPH_RANGE;
        range_field_reset();
        graph_state.active = false;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(false);
        lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_range_display();
        range_update_highlight();
        range_cursor_update();
        lvgl_unlock();
        break;

    case TOKEN_ZOOM:
        /* Show ZOOM preset menu */
        current_mode = MODE_GRAPH_ZOOM;
        zoom_menu_reset();
        graph_state.active = false;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(false);
        lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_zoom_display();
        lvgl_unlock();
        break;

    case TOKEN_GRAPH:
        lvgl_lock();
        lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_Render(angle_degrees);
        lvgl_unlock();
        break;

    case TOKEN_TRACE:
        trace_eq_idx = find_first_active_eq();
        trace_x      = (graph_state.x_min + graph_state.x_max) * 0.5f;
        current_mode = MODE_GRAPH_TRACE;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_yeq_screen,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_graph_range_screen,  LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(true);
        Graph_DrawTrace(trace_x, trace_eq_idx, angle_degrees);
        lvgl_unlock();
        break;


    default:
        break;
    }
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
    cursor_timer = lv_timer_create(cursor_timer_cb, CURSOR_BLINK_MS, NULL);
    ui_update_zoom_display();   /* populate ZOOM labels with initial scroll=0 */
    ui_update_mode_display();
    ui_update_math_display();
    ui_update_test_display();
    ui_update_matrix_display();
    ui_update_matrix_edit_display();
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