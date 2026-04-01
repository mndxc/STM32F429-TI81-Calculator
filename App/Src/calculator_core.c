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
#include "prgm_exec.h"
#include "calc_internal.h"
#include "ui_matrix.h"
#include "ui_prgm.h"
#include "graph_ui.h"
#include "ui_palette.h"
#include "expr_util.h"
#include "cmsis_os.h"
#include "lvgl.h"
#include "main.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

/* Scrollable menu geometry */
#define MODE_ROW_COUNT      8           /* Rows in the MODE screen */
#define MODE_MAX_COLS       11          /* Max options per MODE row (row 1 has 11) */
#define MATH_TAB_COUNT      4           /* MATH menu tabs: MATH NUM HYP PRB */
#define TEST_ITEM_COUNT     6           /* TEST menu items: = ≠ > ≥ < ≤ */

#define MATRIX_MAX_DIM      CALC_MATRIX_MAX_DIM  /* alias — actual size in calc_engine.h */
#define MATRIX_LIST_VISIBLE 7                    /* visible cell rows in the list editor */
#define MATRIX_TAB_COUNT    2           /* MATRX and EDIT tabs */
#define MATRIX_MATRX_ITEMS  6           /* Items in the MATRX operations tab */
#define MATRIX_EDIT_ITEMS   3           /* Items in the EDIT tab */



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

/* Editor / cursor state structs — group logically related static variables so they
 * can be snapshot, serialized, and reasoned about as a unit. */

typedef struct {
    uint8_t    tab;             /* 0=MATH 1=NUM 2=HYP 3=PRB */
    uint8_t    item_cursor;     /* Visible-row index of highlight */
    uint8_t    scroll_offset;
    CalcMode_t return_mode;     /* Mode to restore after selection */
} MathMenuState_t;

typedef struct {
    uint8_t    item_cursor;
    CalcMode_t return_mode;
} TestMenuState_t;

typedef struct {
    uint8_t  row_selected;
    uint8_t  cursor[MODE_ROW_COUNT];    /* Arrow-key highlight per row */
    uint8_t  committed[MODE_ROW_COUNT]; /* Committed selection per row (persisted) */
} ModeScreenState_t;

typedef struct {
    uint8_t    tab;             /* 0=MATRX, 1=EDIT */
    uint8_t    item_cursor;
    CalcMode_t return_mode;
} MatrixMenuState_t;

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
static int8_t  matrix_scroll_focus  = -1;   /* history slot index with scroll focus; -1=none */
static uint8_t matrix_scroll_offset = 0;    /* horizontal character scroll offset */

/* Matrix history ring buffer — stores CalcMatrix_t for the last MATRIX_RING_COUNT results */
static CalcMatrix_t matrix_ring[MATRIX_RING_COUNT];
static uint8_t      matrix_ring_gen_table[MATRIX_RING_COUNT]; /* generation written to each slot */
static uint8_t      matrix_ring_write_count = 0;              /* total writes, wraps at 256 */

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

/* MODE screen state */
lv_obj_t *ui_mode_screen = NULL;
/* Row 2 starts at 1 = Degree to match angle_degrees=true default */
static ModeScreenState_t s_mode = {
    .row_selected = 0,
    .cursor    = {0, 0, 1, 0, 0, 0, 0, 0},
    .committed = {0, 0, 1, 0, 0, 0, 0, 0},
};

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
static lv_obj_t *ui_math_screen = NULL;
static MathMenuState_t s_math = {0};
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
        {"\xC2\xB3",                    "^3"},       /* ³  U+00B3  — display only; engine reads ^3 */
        {"\xC2\xB3\xE2\x88\x9A(",      "^(1/3)"},   /* ³√( U+00B3+U+221A — display only */
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
        {"sinh\xEE\x80\x81(",  "asinh("},   /* sinh⁻¹( — display; engine reads asinh( */
        {"cosh\xEE\x80\x81(",  "acosh("},   /* cosh⁻¹( — display; engine reads acosh( */
        {"tanh\xEE\x80\x81(",  "atanh("},   /* tanh⁻¹( — display; engine reads atanh( */
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
static lv_obj_t  *ui_test_screen = NULL;
static TestMenuState_t s_test = {0};
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
static MatrixMenuState_t s_matrix_menu = {0};

/* MATRIX EDIT sub-screen state */




/*---------------------------------------------------------------------------
 * Forward declarations for display helpers defined later in this file
 *---------------------------------------------------------------------------*/

static void ui_update_mode_display(void);
static void ui_update_math_display(void);
static void ui_update_test_display(void);

/* Per-mode token handler forward declarations */
static bool handle_mode_screen(Token_t t);
static bool handle_math_menu(Token_t t);
static bool handle_test_menu(Token_t t);
static bool menu_handle_nav_keys(Token_t t, CalcMode_t *ret_mode, uint8_t *cursor, uint8_t *scroll);
static bool handle_sto_pending(Token_t t);

/* handle_normal_mode sub-handlers */
static void handle_digit_key(Token_t t);
static void handle_arithmetic_op(Token_t t);
static void handle_history_nav(Token_t t);
static void handle_function_insert(Token_t t);
static void history_load_offset(uint8_t offset);
static void history_enter_evaluate(void);
static void handle_clear_key(void);
static void handle_mode_key(void);
static void handle_sto_key(void);
static void handle_normal_graph_nav(Token_t t);

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
    memcpy(out->mode_committed, s_mode.committed, sizeof(s_mode.committed));
    out->zoom_x_fact = graph_ui_get_zoom_x_fact();
    out->zoom_y_fact = graph_ui_get_zoom_y_fact();

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
    for (int i = 0; i < 4; i++) out->enabled[i] = graph_state.enabled[i] ? 1u : 0u;

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
 * Re-derives calc_decimal_mode and angle_degrees from s_mode.committed via
 * their existing setters so behaviour is consistent with ENTER on MODE.
 */
void Calc_ApplyPersistBlock(const PersistBlock_t *in)
{
    memcpy(calc_variables, in->calc_variables, sizeof(calc_variables));
    ans = in->ans;
    memcpy(s_mode.committed, in->mode_committed, sizeof(s_mode.committed));

    /* Re-derives state that is computed from s_mode.committed */
    angle_degrees = (in->mode_committed[2] == 1);
    Calc_SetDecimalMode(in->mode_committed[1]);

    graph_ui_set_zoom_facts(in->zoom_x_fact, in->zoom_y_fact);

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
    for (int i = 0; i < 4; i++) graph_state.enabled[i] = (in->enabled[i] != 0);

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
    lv_style_set_bg_color(&style_bg, lv_color_hex(COLOR_BLACK));
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
    lv_obj_set_style_bg_color(box, lv_color_hex(COLOR_GREY_LIGHT), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *inner = lv_label_create(box);
    lv_obj_set_style_text_font(inner, &jetbrains_mono_24, 0);
    lv_obj_set_style_text_color(inner, lv_color_hex(COLOR_BLACK), 0);
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
                                    lv_color_hex(COLOR_GREY_MED), 0);
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

/* Creates a full-screen opaque black LVGL panel, hidden by default.
 * Used as the base for all overlay screens (MODE, MATH, TEST, MATRIX). */
lv_obj_t *screen_create(lv_obj_t *parent)
{
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_set_size(scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);
    return scr;
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
            lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_GREY_INACTIVE), 0);
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
        lv_obj_set_style_text_color(math_tab_labels[i], lv_color_hex(COLOR_GREY_INACTIVE), 0);
        lv_label_set_text(math_tab_labels[i], math_tab_names[i]);
    }

    /* MENU_VISIBLE_ROWS dynamic item labels — text set by ui_update_math_display() */
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        math_item_labels[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(math_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(math_item_labels[i], "");
    }

    /* Scroll indicator overlays — amber arrow, opaque bg covers colon beneath */
    for (int i = 0; i < 2; i++) {
        int row = (i == 0) ? 0 : (MENU_VISIBLE_ROWS - 1);
        math_scroll_ind[i] = lv_label_create(ui_math_screen);
        lv_obj_set_pos(math_scroll_ind[i], 18, 30 + row * 30);
        lv_obj_set_style_text_font(math_scroll_ind[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(math_scroll_ind[i], lv_color_hex(COLOR_AMBER), 0);
        lv_obj_set_style_bg_color(math_scroll_ind[i], lv_color_hex(COLOR_BLACK), 0);
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
    lv_obj_set_style_text_color(test_title_label, lv_color_hex(COLOR_YELLOW), 0);
    lv_label_set_text(test_title_label, "TEST");

    /* Item labels — text set by ui_update_test_display() */
    for (int i = 0; i < TEST_ITEM_COUNT; i++) {
        test_item_labels[i] = lv_label_create(ui_test_screen);
        lv_obj_set_pos(test_item_labels[i], 4, 30 + i * 30);
        lv_obj_set_style_text_font(test_item_labels[i], &jetbrains_mono_24, 0);
        lv_obj_set_style_text_color(test_item_labels[i], lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(test_item_labels[i], "");
    }
}


/*---------------------------------------------------------------------------
 * UI update functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Generic block-cursor placement.
 *
 * All cursor appearance logic — visibility, color, inner character, and
 * insert/overwrite shape — is driven by explicit parameters rather than
 * module-level globals.
 *
 * Cursor inner-char key:
 *   MODE_STO        → green 'A'  (STO-pending synthetic mode)
 *   MODE_2ND        → amber '^'
 *   MODE_ALPHA/LOCK → green 'A'
 *   insert=true     → grey underline style  (overwrite = default: blank grey block)
 *
 * @param box          The cursor rectangle LVGL object to move/show/hide.
 * @param inner        The label child of box that shows the inner character.
 * @param parent_label The LVGL label whose text provides the reference position.
 * @param glyph_pos    Glyph index within parent_label at which to place the cursor.
 * @param visible      Whether the cursor is currently in the visible blink phase.
 * @param mode         Calculator mode driving cursor color/inner-char appearance.
 *                     Pass MODE_STO (synthesised from sto_pending) for STO-pending state.
 * @param insert       True when insert mode is active (renders underscore-style cursor).
 */
void cursor_render(lv_obj_t *box, lv_obj_t *inner,
                   lv_obj_t *parent_label, uint32_t glyph_pos,
                   bool visible, CalcMode_t mode, bool insert)
{
    if (box == NULL) return;

    lv_color_t box_color;
    const char *inner_text;

    switch (mode) {
        case MODE_STO:
            box_color  = lv_color_hex(COLOR_ALPHA);
            inner_text = "A";
            break;
        case MODE_2ND:
            box_color  = lv_color_hex(COLOR_2ND);
            inner_text = "^";
            break;
        case MODE_ALPHA:
        case MODE_ALPHA_LOCK:
            box_color  = lv_color_hex(COLOR_ALPHA);
            inner_text = "A";
            break;
        default:
            box_color  = lv_color_hex(COLOR_GREY_LIGHT);
            inner_text = "";  /* insert mode shown by underscore shape, not letter */
            break;
    }

    if (!visible) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_point_t pos;
    lv_label_get_letter_pos(parent_label, glyph_pos, &pos);

    int32_t lx = lv_obj_get_x(parent_label);
    int32_t ly = lv_obj_get_y(parent_label);

    /* Insert mode: underscore-style cursor (3 px at character baseline).
     * Overwrite mode: full-height block cursor (26 px).
     * In insert mode the box is 3 px tall; the inner label (^/A) overflows
     * above the underline via LV_OBJ_FLAG_OVERFLOW_VISIBLE, giving a combined
     * "underline + mode indicator" visual for insert+2ND or insert+ALPHA. */
    bool in_insert = insert && (mode != MODE_STO);
    lv_obj_set_height(box, in_insert ? 3 : 26);
    lv_obj_set_pos(box, lx + pos.x, ly + pos.y + (in_insert ? 23 : 0));

    lv_obj_set_style_bg_color(box, box_color, 0);
    lv_label_set_text(inner, inner_text);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
}


/**
 * @brief Positions the main calculator screen block cursor.
 *
 * @param row_label  The LVGL label containing the current expression.
 * @param char_pos   Character index at which to place the cursor.
 */
static void cursor_update(lv_obj_t *row_label, uint32_t char_pos)
{
    CalcMode_t display_mode = sto_pending ? MODE_STO : current_mode;
    cursor_render(cursor_box, cursor_inner, row_label, char_pos,
                  cursor_visible, display_mode, insert_mode);
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

/* Compute the maximum formatted-cell width for each column of matrix m. */
static void matrix_col_widths(const CalcMatrix_t *m,
                               uint8_t widths[CALC_MATRIX_MAX_DIM])
{
    for (int c = 0; c < CALC_MATRIX_MAX_DIM; c++) widths[c] = 0;
    for (int r = 0; r < (int)m->rows; r++) {
        for (int c = 0; c < (int)m->cols; c++) {
            char cell[12];
            Calc_FormatResult(m->data[r][c], cell, sizeof(cell));
            cell[8] = '\0';
            uint8_t len = (uint8_t)strlen(cell);
            if (len > widths[c]) widths[c] = len;
        }
    }
}

/* Total characters needed for one formatted row: [ col0 col1 … coln ] */
static int matrix_row_total_width(const CalcMatrix_t *m)
{
    uint8_t widths[CALC_MATRIX_MAX_DIM];
    matrix_col_widths(m, widths);
    int total = 2; /* [ and ] */
    for (int c = 0; c < (int)m->cols; c++) {
        if (c > 0) total++; /* space separator */
        total += widths[c];
    }
    return total;
}

/*
 * Build a display string for one row of matrix m with horizontal scroll applied.
 *
 * display_cols: number of character columns visible (typically expr_chars_per_row).
 * Shows '>' at the right edge when more content is to the right.
 * Shows '<' at the left edge when scrolled past content.
 * buf must be at least display_cols + 2 bytes.
 */
static void matrix_format_row(const CalcMatrix_t *m, int row_idx,
                               int scroll_offset, int display_cols,
                               char *buf, int buf_size)
{
    uint8_t widths[CALC_MATRIX_MAX_DIM];
    matrix_col_widths(m, widths);

    /* Build the full unclipped row string */
    char full[80];
    int pos = 0;
    full[pos++] = '[';
    for (int c = 0; c < (int)m->cols && pos < (int)sizeof(full) - 2; c++) {
        if (c > 0) full[pos++] = ' ';
        char cell[12];
        Calc_FormatResult(m->data[row_idx][c], cell, sizeof(cell));
        cell[8] = '\0';
        int cl = (int)strlen(cell);
        int cw = (int)widths[c];
        /* Right-pad cell to column width */
        for (int p = 0; p < cw && pos < (int)sizeof(full) - 1; p++)
            full[pos++] = (p < cl) ? cell[p] : ' ';
    }
    if (pos < (int)sizeof(full) - 1) full[pos++] = ']';
    full[pos] = '\0';
    int full_len = pos;

    bool clip_left  = (scroll_offset > 0);
    bool clip_right = (scroll_offset + display_cols < full_len);

    /* Source range: shrink by 1 char on each clipped side for the indicator */
    int src_start = scroll_offset + (clip_left  ? 1 : 0);
    int src_end   = scroll_offset + display_cols - (clip_right ? 1 : 0);
    if (src_end > full_len) src_end = full_len;

    int out = 0;
    if (clip_left  && out < buf_size - 1) buf[out++] = '<';
    for (int src = src_start; src < src_end && out < buf_size - 2; src++)
        buf[out++] = full[src];
    if (clip_right && out < buf_size - 1) buf[out++] = '>';
    buf[out] = '\0';
}

/** Returns the CalcMatrix_t for history entry @p e, or NULL if evicted from the ring. */
static const CalcMatrix_t *history_get_matrix(const HistoryEntry_t *e)
{
    if (!e->has_matrix) return NULL;
    uint8_t slot = e->matrix_ring_idx;
    if (matrix_ring_gen_table[slot] != e->matrix_ring_gen) return NULL;
    return &matrix_ring[slot];
}

/**
 * @brief Render one history result row onto @p label.
 *
 * Sets the label colour to COLOR_WHITE, then either formats a matrix row
 * (column-aligned, left-aligned, with horizontal scroll applied) or a scalar
 * result line (right-aligned), and sets the label text.
 */
static void render_result_row(lv_obj_t *label, const HistoryEntry_t *entry,
                               int result_line)
{
    char rbuf[MAX_RESULT_LEN];
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_WHITE), 0);
    if (entry->has_matrix) {
        const CalcMatrix_t *m = history_get_matrix(entry);
        if (m != NULL) {
            int off = (matrix_scroll_focus == (int8_t)(entry - history))
                      ? (int)matrix_scroll_offset : 0;
            matrix_format_row(m, result_line,
                              off, (int)expr_chars_per_row, rbuf, sizeof(rbuf));
        } else {
            /* Matrix evicted from ring — fall back to pre-formatted result string */
            get_result_line(entry->result, result_line, rbuf, sizeof(rbuf));
        }
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    } else {
        get_result_line(entry->result, result_line, rbuf, sizeof(rbuf));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_label_set_text(label, rbuf);
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
        int rlines = history[idx].has_matrix
                     ? (int)history[idx].matrix_rows_cache
                     : count_result_lines(history[idx].result);
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
                int rlines = history[idx].has_matrix
                             ? (int)history[idx].matrix_rows_cache
                             : count_result_lines(history[idx].result);

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
                                                lv_color_hex(COLOR_GREY_MED), 0);
                    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
                    lv_label_set_text(disp_rows[row], row_buf);
                    break;
                }
                line += erows;

                if (li < line + rlines) {
                    /* Result line for this history entry */
                    int result_line = li - line;
                    render_result_row(disp_rows[row], &history[idx], result_line);
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

            lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_GREY_LIGHT), 0);
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
void ui_update_status_bar(void)
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
    ExprUtil_PrependAns(expression, &expr_len, &cursor_pos, MAX_EXPR_LEN);
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
    ExprUtil_InsertChar(expression, &expr_len, &cursor_pos, MAX_EXPR_LEN, insert_mode, c);
}

/**
 * @brief Inserts a string at cursor_pos and advances the cursor by its length.
 */
static void expr_insert_str(const char *s)
{
    ExprUtil_InsertStr(expression, &expr_len, &cursor_pos, MAX_EXPR_LEN, s);
}

/**
 * @brief Deletes the character immediately before cursor_pos (backspace).
 */
void expr_delete_at_cursor(void)
{
    ExprUtil_DeleteAtCursor(expression, &expr_len, &cursor_pos);
}

/**
 * @brief Refreshes the display after history changes.
 */
void ui_update_history(void)
{
    ui_refresh_display();
}

/** Write @p text directly to display row @p row_1based (1–8) without
 *  touching the history ring buffer.  Must be called under lvgl_lock(). */
void ui_output_row(uint8_t row_1based, const char *text)
{
    if (row_1based < 1 || row_1based > DISP_ROW_COUNT) return;
    if (disp_rows[0] == NULL) return;
    uint8_t row = row_1based - 1;
    lv_obj_set_style_text_color(disp_rows[row], lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_align(disp_rows[row], LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(disp_rows[row], text);
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
            if (r == s_mode.row_selected && c == (int)s_mode.cursor[r])
                col = lv_color_hex(COLOR_YELLOW);
            else if (c == (int)s_mode.committed[r])
                col = lv_color_hex(COLOR_WHITE);   /* white — committed */
            else
                col = lv_color_hex(COLOR_GREY_INACTIVE);   /* dim — not selected */
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
            (i == (int)s_math.tab) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_GREY_INACTIVE), 0);
    }

    /* Hide both scroll indicators; re-shown below if needed */
    lv_obj_add_flag(math_scroll_ind[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(math_scroll_ind[1], LV_OBJ_FLAG_HIDDEN);

    int total = (int)math_tab_item_count[s_math.tab];
    for (int i = 0; i < MENU_VISIBLE_ROWS; i++) {
        int idx = (int)s_math.scroll_offset + i;
        if (idx >= total) {
            lv_label_set_text(math_item_labels[i], "");
            continue;
        }
        bool more_below = (s_math.scroll_offset + MENU_VISIBLE_ROWS < (uint8_t)total)
                          && (i == MENU_VISIBLE_ROWS - 1);
        bool more_above = (s_math.scroll_offset > 0) && (i == 0);
        char buf[40];
        const char *name = math_menu_items[s_math.tab][idx].display;
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
            (i == (int)s_math.item_cursor) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(math_item_labels[i], buf);
    }
}

/* Refreshes the TEST menu item labels based on s_test.item_cursor. */
static void ui_update_test_display(void)
{
    for (int i = 0; i < TEST_ITEM_COUNT; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%s", i + 1, test_menu_items[i].display);
        lv_obj_set_style_text_color(test_item_labels[i],
            (i == (int)s_test.item_cursor) ? lv_color_hex(COLOR_YELLOW) : lv_color_hex(COLOR_WHITE), 0);
        lv_label_set_text(test_item_labels[i], buf);
    }
}


/*---------------------------------------------------------------------------
 * Token execution
 *---------------------------------------------------------------------------*/

/**
 * @brief Inserts a MATH menu item into the active destination and exits the
 *        MATH menu.  If the menu was opened from the Y= editor it inserts
 *        at s_yeq.cursor_pos and restores the Y= screen; otherwise it inserts
 *        into the main expression.  Called from the MODE_MATH_MENU handler.
 */
static void math_menu_insert(const char *ins)
{
    lvgl_lock();
    lv_obj_add_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
    lvgl_unlock();

    if (s_math.return_mode == MODE_PRGM_EDITOR) {
        prgm_editor_menu_insert(ins);   /* F4: redirect to program editor */
    } else if (s_math.return_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        graph_ui_yeq_insert(ins);
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    s_math.return_mode = MODE_NORMAL;
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
        graph_ui_yeq_insert(ins);
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

    if (s_test.return_mode == MODE_PRGM_EDITOR) {
        prgm_editor_menu_insert(ins);   /* F4: redirect to program editor */
    } else if (s_test.return_mode == MODE_GRAPH_YEQ) {
        current_mode = MODE_GRAPH_YEQ;
        graph_ui_yeq_insert(ins);
    } else {
        current_mode = MODE_NORMAL;
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    s_test.return_mode = MODE_NORMAL;
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

/* Opens a menu (MATH, TEST, or MATRIX) from any screen.
 * return_to: the mode to restore when the menu is closed.
 * Hides all screens first so no overlay leaks through. */
void menu_open(Token_t menu_token, CalcMode_t return_to)
{
    lvgl_lock();
    hide_all_screens();
    switch (menu_token) {
    case TOKEN_MATH:
        s_math.return_mode   = return_to;
        s_math.tab           = 0;
        s_math.item_cursor   = 0;
        s_math.scroll_offset = 0;
        current_mode       = MODE_MATH_MENU;
        lv_obj_clear_flag(ui_math_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_math_display();
        break;
    case TOKEN_TEST:
        s_test.return_mode  = return_to;
        s_test.item_cursor  = 0;
        current_mode      = MODE_TEST_MENU;
        lv_obj_clear_flag(ui_test_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_test_display();
        break;
    case TOKEN_MATRX:
        s_matrix_menu.return_mode = return_to;
        s_matrix_menu.tab         = 0;
        s_matrix_menu.item_cursor = 0;
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
        ret                = s_math.return_mode;
        s_math.return_mode   = MODE_NORMAL;
        s_math.item_cursor   = 0;
        s_math.scroll_offset = 0;
        break;
    case TOKEN_TEST:
        ret              = s_test.return_mode;
        s_test.return_mode = MODE_NORMAL;
        s_test.item_cursor = 0;
        break;
    case TOKEN_MATRX:
        ret                = s_matrix_menu.return_mode;
        s_matrix_menu.return_mode = MODE_NORMAL;
        s_matrix_menu.tab         = 0;
        s_matrix_menu.item_cursor = 0;
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
 * Per-mode token handlers (non-graph)
 * Each returns true if the token was fully handled (Execute_Token should
 * return), false if execution should fall through to the next handler.
 *---------------------------------------------------------------------------*/

static bool handle_mode_screen(Token_t t)
{
    switch (t) {
    case TOKEN_UP:
        if (s_mode.row_selected > 0) s_mode.row_selected--;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s_mode.row_selected < MODE_ROW_COUNT - 1) s_mode.row_selected++;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_LEFT:
        if (s_mode.cursor[s_mode.row_selected] > 0)
            s_mode.cursor[s_mode.row_selected]--;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_RIGHT:
        if (s_mode.cursor[s_mode.row_selected] < mode_option_count[s_mode.row_selected] - 1)
            s_mode.cursor[s_mode.row_selected]++;
        lvgl_lock(); ui_update_mode_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER:
        s_mode.committed[s_mode.row_selected] = s_mode.cursor[s_mode.row_selected];
        if (s_mode.row_selected == 1)
            Calc_SetDecimalMode(s_mode.committed[1]);
        if (s_mode.row_selected == 2)
            angle_degrees = (s_mode.committed[2] == 1);
        if (s_mode.row_selected == 6)
            graph_state.grid_on = (s_mode.committed[6] == 1);
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

/* Shared nav-key handler for menus that can jump to graph screens.
 * Resets ret_mode and cursor (and scroll if non-NULL) then calls nav_to().
 * Returns true if the token was a nav key, false otherwise. */
static bool menu_handle_nav_keys(Token_t t, CalcMode_t *ret_mode, uint8_t *cursor, uint8_t *scroll)
{
    CalcMode_t target;
    switch (t) {
    case TOKEN_Y_EQUALS: target = MODE_GRAPH_YEQ;   break;
    case TOKEN_RANGE:    target = MODE_GRAPH_RANGE;  break;
    case TOKEN_ZOOM:     target = MODE_GRAPH_ZOOM;   break;
    case TOKEN_GRAPH:    target = MODE_NORMAL;        break;
    case TOKEN_TRACE:    target = MODE_GRAPH_TRACE;  break;
    default:             return false;
    }
    *ret_mode = MODE_NORMAL;
    *cursor   = 0;
    if (scroll) *scroll = 0;
    nav_to(target);
    return true;
}

static bool handle_math_menu(Token_t t)
{
    int total = (int)math_tab_item_count[s_math.tab];
    switch (t) {
    case TOKEN_LEFT:
        tab_move(&s_math.tab, &s_math.item_cursor, &s_math.scroll_offset, MATH_TAB_COUNT, true, ui_update_math_display);
        return true;
    case TOKEN_RIGHT:
        tab_move(&s_math.tab, &s_math.item_cursor, &s_math.scroll_offset, MATH_TAB_COUNT, false, ui_update_math_display);
        return true;
    case TOKEN_UP:
        if (s_math.item_cursor > 0) {
            s_math.item_cursor--;
        } else if (s_math.scroll_offset > 0) {
            s_math.scroll_offset--;
        }
        lvgl_lock(); ui_update_math_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if ((int)(s_math.scroll_offset + s_math.item_cursor) + 1 < total) {
            if (s_math.item_cursor < MENU_VISIBLE_ROWS - 1)
                s_math.item_cursor++;
            else if ((int)(s_math.scroll_offset + MENU_VISIBLE_ROWS) < total)
                s_math.scroll_offset++;
        }
        lvgl_lock(); ui_update_math_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        int idx = (int)s_math.scroll_offset + (int)s_math.item_cursor;
        if (idx < total) {
            const char *ins = math_menu_items[s_math.tab][idx].insert;
            if (ins != NULL) { math_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_1 ... TOKEN_9: {
        int idx = (int)(t - TOKEN_0) - 1;
        if (idx < total) {
            const char *ins = math_menu_items[s_math.tab][idx].insert;
            if (ins != NULL) { math_menu_insert(ins); return true; }
        }
        break;
    }
    case TOKEN_CLEAR:
    case TOKEN_MATH:
        menu_close(TOKEN_MATH);
        return true;
    default:
        if (menu_handle_nav_keys(t, &s_math.return_mode, &s_math.item_cursor, &s_math.scroll_offset))
            return true;
    {
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
        if (s_test.item_cursor > 0) s_test.item_cursor--;
        lvgl_lock(); ui_update_test_display(); lvgl_unlock();
        return true;
    case TOKEN_DOWN:
        if (s_test.item_cursor < TEST_ITEM_COUNT - 1) s_test.item_cursor++;
        lvgl_lock(); ui_update_test_display(); lvgl_unlock();
        return true;
    case TOKEN_ENTER: {
        const char *ins = test_menu_items[s_test.item_cursor].insert;
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
    default:
        if (menu_handle_nav_keys(t, &s_test.return_mode, &s_test.item_cursor, NULL))
            return true;
    {
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
        sto_pending = false;
        static const char var_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        uint8_t var_idx = t - TOKEN_A;

        CalcResult_t result = Calc_Evaluate(expression, ans, ans_is_matrix, angle_degrees);

        char result_str[MAX_RESULT_LEN];
        char expr_hist[MAX_EXPR_LEN + 4];  /* expression + "->A\0" */
        snprintf(expr_hist, sizeof(expr_hist), "%s->%c", expression, var_names[var_idx]);

        if (result.error != CALC_OK) {
            strncpy(result_str, result.error_msg, MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else if (result.has_matrix) {
            strncpy(result_str, "ERR:DATA TYPE", MAX_RESULT_LEN - 1);
            result_str[MAX_RESULT_LEN - 1] = '\0';
        } else {
            calc_variables[var_idx] = result.value;
            ans = result.value;
            ans_is_matrix = false;
            Calc_FormatResult(result.value, result_str, MAX_RESULT_LEN);
        }

        uint8_t idx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[idx].expression, expr_hist, MAX_EXPR_LEN - 1);
        history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
        strncpy(history[idx].result, result_str, MAX_RESULT_LEN - 1);
        history[idx].result[MAX_RESULT_LEN - 1] = '\0';
        history[idx].has_matrix = false;
        matrix_scroll_focus  = -1;
        matrix_scroll_offset = 0;
        history_count++;

        expr_len              = 0;
        cursor_pos            = 0;
        expression[0]         = '\0';
        history_recall_offset = 0;

        lvgl_lock();
        ui_update_history();
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

static void handle_digit_key(Token_t t)
{
    if (t == TOKEN_DECIMAL) {
        expr_insert_char('.');
    } else {
        expr_insert_char((char)((t - TOKEN_0) + '0'));
    }
    Update_Calculator_Display();
}

static void handle_arithmetic_op(Token_t t)
{
    switch (t) {
    case TOKEN_ADD:    expr_prepend_ans_if_empty(); expr_insert_char('+');      break;
    case TOKEN_SUB:    expr_prepend_ans_if_empty(); expr_insert_char('-');      break;
    case TOKEN_MULT:   expr_prepend_ans_if_empty(); expr_insert_char('*');      break;
    case TOKEN_DIV:    expr_prepend_ans_if_empty(); expr_insert_char('/');      break;
    case TOKEN_SQUARE: expr_prepend_ans_if_empty(); expr_insert_str("^2");      break;
    case TOKEN_X_INV:  expr_prepend_ans_if_empty(); expr_insert_str("^-1");     break;
    case TOKEN_POWER:  expr_prepend_ans_if_empty(); expr_insert_char('^');      break;
    case TOKEN_L_PAR:  expr_insert_char('(');                                   break;
    case TOKEN_R_PAR:  expr_insert_char(')');                                   break;
    case TOKEN_NEG:    expr_insert_char('-');                                   break;
    default: break;
    }
    Update_Calculator_Display();
}

/**
 * @brief Write a completed evaluation into the next history slot and refresh the display.
 *
 * Stores @p expr and @p result_str into history[history_count % HISTORY_LINE_COUNT],
 * copies matrix data when @p r->has_matrix is set, advances history_count, and
 * triggers a UI update under the LVGL mutex.
 *
 * The caller is responsible for any expression-buffer reset and history_recall_offset
 * update that should happen after the commit.
 */
static void commit_history_entry(const char *expr, const char *result_str,
                                 const CalcResult_t *r)
{
    uint8_t idx = history_count % HISTORY_LINE_COUNT;
    strncpy(history[idx].expression, expr, MAX_EXPR_LEN - 1);
    history[idx].expression[MAX_EXPR_LEN - 1] = '\0';
    strncpy(history[idx].result, result_str, MAX_RESULT_LEN - 1);
    history[idx].result[MAX_RESULT_LEN - 1] = '\0';
    history[idx].has_matrix = false;
    if (r->has_matrix) {
        uint8_t slot = (uint8_t)(matrix_ring_write_count % MATRIX_RING_COUNT);
        matrix_ring[slot]           = calc_matrices[r->matrix_idx];
        matrix_ring_gen_table[slot] = matrix_ring_write_count;
        history[idx].has_matrix       = true;
        history[idx].matrix_ring_idx  = slot;
        history[idx].matrix_ring_gen  = matrix_ring_write_count;
        history[idx].matrix_rows_cache = calc_matrices[r->matrix_idx].rows;
        matrix_ring_write_count++;
        matrix_scroll_focus  = (int8_t)idx;
        matrix_scroll_offset = 0;
    } else {
        matrix_scroll_focus  = -1;
        matrix_scroll_offset = 0;
    }
    history_count++;
    lvgl_lock();
    ui_update_history();
    lvgl_unlock();
}

/* Load a history entry at the given scroll offset into the expression buffer. */
static void history_load_offset(uint8_t offset)
{
    uint8_t idx = (history_count - offset) % HISTORY_LINE_COUNT;
    strncpy(expression, history[idx].expression, MAX_EXPR_LEN - 1);
    expression[MAX_EXPR_LEN - 1] = '\0';
    expr_len   = (uint8_t)strlen(expression);
    cursor_pos = expr_len;
    Update_Calculator_Display();
}

/* Evaluate (or run) the current expression on TOKEN_ENTER.
 * Called only when expr_len > 0. */
static void history_enter_evaluate(void)
{
    /* prgmNAME expression: insert into history and run the program */
    if (strncmp(expression, "prgm", 4) == 0) {
        int8_t slot = prgm_lookup_slot(expression + 4);
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, expression, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        history[hidx].result[0] = '\0';
        history_count++;
        expr_len              = 0;
        cursor_pos            = 0;
        expression[0]         = '\0';
        history_recall_offset = 0;
        Update_Calculator_Display();
        if (slot >= 0)
            prgm_run_start((uint8_t)slot);
        return;
    }
    CalcResult_t result = Calc_Evaluate(expression, ans, ans_is_matrix, angle_degrees);
    char result_str[MAX_RESULT_LEN];
    format_calc_result(&result, result_str, MAX_RESULT_LEN, &ans);
    commit_history_entry(expression, result_str, &result);
    expr_len              = 0;
    cursor_pos            = 0;
    expression[0]         = '\0';
    history_recall_offset = 0;
}

static void handle_history_nav(Token_t t)
{
    switch (t) {

    case TOKEN_LEFT:
        if (expr_len == 0 && matrix_scroll_focus >= 0 &&
            history_get_matrix(&history[matrix_scroll_focus]) != NULL) {
            if (matrix_scroll_offset > 0) {
                matrix_scroll_offset--;
                Update_Calculator_Display();
            }
        } else if (cursor_pos > 0) {
            ExprUtil_MoveCursorLeft(expression, &cursor_pos);
            Update_Calculator_Display();
        }
        break;

    case TOKEN_RIGHT:
        if (expr_len == 0 && matrix_scroll_focus >= 0) {
            const CalcMatrix_t *m = history_get_matrix(&history[matrix_scroll_focus]);
            if (m != NULL) {
                int total_w = matrix_row_total_width(m);
                int max_off = (total_w > (int)expr_chars_per_row)
                              ? (total_w - (int)expr_chars_per_row) : 0;
                if ((int)matrix_scroll_offset < max_off) {
                    matrix_scroll_offset++;
                    Update_Calculator_Display();
                }
            }
        } else if (cursor_pos < expr_len) {
            ExprUtil_MoveCursorRight(expression, expr_len, &cursor_pos);
            Update_Calculator_Display();
        }
        break;

    case TOKEN_UP:
        if ((expr_len == 0 || history_recall_offset > 0) &&
            history_recall_offset < history_count) {
            history_recall_offset++;
            history_load_offset(history_recall_offset);
        }
        break;

    case TOKEN_DOWN:
        if (history_recall_offset > 0) {
            history_recall_offset--;
            if (history_recall_offset == 0) {
                expr_len = 0; cursor_pos = 0; expression[0] = '\0';
                Update_Calculator_Display();
            } else {
                history_load_offset(history_recall_offset);
            }
        }
        break;

    case TOKEN_ENTER:
        if (expr_len == 0 && history_count > 0) {
            /* Re-evaluate the last history entry */
            uint8_t last_idx = (history_count - 1) % HISTORY_LINE_COUNT;
            CalcResult_t result = Calc_Evaluate(history[last_idx].expression,
                                                ans, ans_is_matrix, angle_degrees);
            char result_str[MAX_RESULT_LEN];
            format_calc_result(&result, result_str, MAX_RESULT_LEN, &ans);
            commit_history_entry(history[last_idx].expression, result_str, &result);
            history_recall_offset = 0;
        } else if (expr_len > 0) {
            history_enter_evaluate();
        }
        break;

    case TOKEN_ENTRY:
        if (history_count > 0) {
            history_recall_offset = 1;
            history_load_offset(1);
        }
        break;

    default: break;
    }
}

static void handle_function_insert(Token_t t)
{
    switch (t) {
    case TOKEN_MTRX_A: expr_insert_str("[A]"); break;
    case TOKEN_MTRX_B: expr_insert_str("[B]"); break;
    case TOKEN_MTRX_C: expr_insert_str("[C]"); break;

    case TOKEN_SIN:   expr_insert_str("sin(");  break;
    case TOKEN_COS:   expr_insert_str("cos(");  break;
    case TOKEN_TAN:   expr_insert_str("tan(");  break;
    case TOKEN_ASIN:  expr_insert_str("sin\xEE\x80\x81("); break;   /* sin⁻¹( */
    case TOKEN_ACOS:  expr_insert_str("cos\xEE\x80\x81("); break;   /* cos⁻¹( */
    case TOKEN_ATAN:  expr_insert_str("tan\xEE\x80\x81("); break;   /* tan⁻¹( */
    case TOKEN_ABS:   expr_insert_str("abs(");  break;
    case TOKEN_LN:    expr_insert_str("ln(");   break;
    case TOKEN_LOG:   expr_insert_str("log(");  break;
    case TOKEN_SQRT:  expr_insert_str("\xE2\x88\x9A("); break;
    case TOKEN_EE:    expr_insert_str("*10^");  break;
    case TOKEN_E_X:   expr_insert_str("exp(");  break;
    case TOKEN_TEN_X: expr_insert_str("10^(");  break;
    case TOKEN_PI:    expr_insert_str("π");     break;
    case TOKEN_ANS:   expr_insert_str("ANS");   break;
    case TOKEN_THETA: expr_insert_str("θ");     break;
    case TOKEN_SPACE: expr_insert_char(' ');    break;
    case TOKEN_COMMA: expr_insert_char(',');    break;
    case TOKEN_QUOTES: expr_insert_char('"');   break;
    case TOKEN_QSTN_M: expr_insert_char('?');   break;

    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z: {
        static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        char ch[2] = { alpha_chars[t - TOKEN_A], '\0' };
        expr_insert_str(ch);
        break;
    }

    default: break;
    }
    Update_Calculator_Display();
}

static void handle_clear_key(void)
{
    if (graph_state.active) {
        lvgl_lock();
        Graph_SetVisible(false);
        lvgl_unlock();
        return;
    }
    expr_len      = 0;
    cursor_pos    = 0;
    expression[0] = '\0';
    Update_Calculator_Display();
}

static void handle_mode_key(void)
{
    memcpy(s_mode.cursor, s_mode.committed, sizeof(s_mode.cursor));
    s_mode.row_selected = 0;
    current_mode = MODE_MODE_SCREEN;
    lvgl_lock();
    lv_obj_clear_flag(ui_mode_screen, LV_OBJ_FLAG_HIDDEN);
    ui_update_mode_display();
    lvgl_unlock();
}

static void handle_sto_key(void)
{
    if (expr_len == 0) {
        expr_prepend_ans_if_empty();
        Update_Calculator_Display();
    }
    sto_pending = true;
    lvgl_lock();
    ui_update_status_bar();
    lvgl_unlock();
}

static void handle_normal_graph_nav(Token_t t)
{
    switch (t) {
    case TOKEN_Y_EQUALS: nav_to(MODE_GRAPH_YEQ);   break;
    case TOKEN_RANGE:    nav_to(MODE_GRAPH_RANGE);  break;
    case TOKEN_ZOOM:     nav_to(MODE_GRAPH_ZOOM);   break;
    case TOKEN_GRAPH:    nav_to(MODE_NORMAL);        break;
    case TOKEN_TRACE:    nav_to(MODE_GRAPH_TRACE);  break;
    default:             break;
    }
}

void handle_normal_mode(Token_t t)
{
    switch (t) {
    case TOKEN_0 ... TOKEN_9:
    case TOKEN_DECIMAL:
        handle_digit_key(t);        break;
    case TOKEN_ADD: case TOKEN_SUB: case TOKEN_MULT: case TOKEN_DIV:
    case TOKEN_SQUARE: case TOKEN_X_INV: case TOKEN_POWER:
    case TOKEN_L_PAR: case TOKEN_R_PAR: case TOKEN_NEG:
        handle_arithmetic_op(t);    break;
    case TOKEN_LEFT: case TOKEN_RIGHT:
    case TOKEN_UP:   case TOKEN_DOWN:
    case TOKEN_ENTER: case TOKEN_ENTRY:
        handle_history_nav(t);      break;
    case TOKEN_CLEAR:               handle_clear_key();         break;
    case TOKEN_DEL:                 expr_delete_at_cursor();
                                    Update_Calculator_Display(); break;
    case TOKEN_INS:                 insert_mode = !insert_mode;
                                    Update_Calculator_Display(); break;
    case TOKEN_MODE:                handle_mode_key();           break;
    case TOKEN_MATH:                menu_open(TOKEN_MATH,  MODE_NORMAL); break;
    case TOKEN_TEST:                menu_open(TOKEN_TEST,  MODE_NORMAL); break;
    case TOKEN_MATRX:               menu_open(TOKEN_MATRX, MODE_NORMAL); break;
    case TOKEN_PRGM:                menu_open(TOKEN_PRGM,  MODE_NORMAL); break;
    case TOKEN_MTRX_A: case TOKEN_MTRX_B: case TOKEN_MTRX_C:
    case TOKEN_SIN: case TOKEN_COS: case TOKEN_TAN:
    case TOKEN_ASIN: case TOKEN_ACOS: case TOKEN_ATAN:
    case TOKEN_ABS: case TOKEN_LN: case TOKEN_LOG: case TOKEN_SQRT:
    case TOKEN_EE: case TOKEN_E_X: case TOKEN_TEN_X:
    case TOKEN_PI: case TOKEN_ANS: case TOKEN_THETA:
    case TOKEN_SPACE: case TOKEN_COMMA: case TOKEN_QUOTES: case TOKEN_QSTN_M:
    case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
    case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
    case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
    case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
    case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
    case TOKEN_Z:
        handle_function_insert(t);  break;
    case TOKEN_STO:                 handle_sto_key();            break;
    case TOKEN_Y_EQUALS: case TOKEN_RANGE: case TOKEN_ZOOM:
    case TOKEN_GRAPH:    case TOKEN_TRACE:
        handle_normal_graph_nav(t); break;
    default:                        break;
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
        lv_obj_set_style_text_color(saving_lbl, lv_color_hex(COLOR_AMBER), 0);
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

    /*--- TOKEN_QUIT: hard exit to main calculator screen ------------------*/
    if (t == TOKEN_QUIT) {
        current_mode = MODE_NORMAL;
        return_mode  = MODE_NORMAL;
        sto_pending  = false;
        prgm_reset_execution_state();
        lvgl_lock();
        hide_all_screens();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }

    /*--- TOKEN_MODE: always opens MODE screen from any mode ----------------*/
    if (t == TOKEN_MODE) {
        memcpy(s_mode.cursor, s_mode.committed, sizeof(s_mode.cursor));
        s_mode.row_selected = 0;
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
    /* F5b: ALPHA_LOCK in name-entry — same pattern as A6 editor fix below */
    if (current_mode == MODE_PRGM_NEW_NAME ||
        (current_mode == MODE_ALPHA_LOCK && return_mode == MODE_PRGM_NEW_NAME))
                                                  { if (handle_prgm_new_name(t))      return; }
    /* A6: ALPHA_LOCK in editor — current_mode stays MODE_ALPHA_LOCK; route by return_mode */
    if (current_mode == MODE_PRGM_EDITOR ||
        (current_mode == MODE_ALPHA_LOCK && return_mode == MODE_PRGM_EDITOR))
                                                  { if (handle_prgm_editor(t))        return; }
    if (current_mode == MODE_PRGM_CTL_MENU)       { if (handle_prgm_ctl_menu(t))      return; }
    if (current_mode == MODE_PRGM_IO_MENU)        { if (handle_prgm_io_menu(t))       return; }
    if (current_mode == MODE_PRGM_EXEC_MENU)      { if (handle_prgm_exec_menu(t))     return; }

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
            /* A2: fall back to normal function (e.g. ENTER/DEL/CLEAR in name-entry) */
            token_to_send = key.normal;
            if (token_to_send == TOKEN_NONE) {
                lvgl_lock();
                ui_update_status_bar();
                lvgl_unlock();
                return;
            }
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
        /* F1: on CLEAR, abort any running program immediately from keypadTask so
         * prgm_run_loop() (on CalcCoreTask) exits on its next iteration check. */
        if (token_to_send == TOKEN_CLEAR)
            prgm_request_abort();  /* no-op if not running */
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
    ui_update_zoom_display();   /* populate ZOOM labels with initial scroll=0 (defined in graph_ui.c) */
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
            graph_ui_sync_yeq_labels();
            /* Sync MODE screen cursor/highlight with loaded s_mode.committed */
            ui_update_mode_display();
            /* Sync RANGE field labels with loaded graph_state values */
            ui_update_range_display();
            /* Sync ZOOM FACTORS labels with loaded s_zf.x_fact / s_zf.y_fact */
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