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
#include "calc_engine.h"
#include "graph.h"
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
#define MAX_EXPR_LEN        64
#define MAX_RESULT_LEN      32

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

extern SemaphoreHandle_t xLVGL_Mutex;
extern SemaphoreHandle_t xLVGL_Ready;
extern const uint32_t TI81_LookupTable_Size;

/*---------------------------------------------------------------------------
 * Private types
 *---------------------------------------------------------------------------*/

typedef struct {
    char expression[MAX_EXPR_LEN];
    char result[MAX_RESULT_LEN];
} HistoryEntry_t;

/*---------------------------------------------------------------------------
 * Private variables
 *---------------------------------------------------------------------------*/

/* LVGL objects — main display */
static lv_obj_t *disp_rows[DISP_ROW_COUNT]; /* Full-width text rows (Montserrat 24) */
static lv_obj_t *ui_lbl_angle_mode;         /* Small DEG/RAD corner label */
static lv_obj_t *ui_lbl_modifier;           /* Hidden — used only for Y= mirror */

/* Cursor blink state */
static bool        cursor_visible = true;
static lv_timer_t *cursor_timer   = NULL;
static lv_obj_t   *cursor_box     = NULL;  /* Filled-block cursor rectangle */
static lv_obj_t   *cursor_inner   = NULL;  /* Character label inside cursor_box */

/* Graph screens */
static lv_obj_t *ui_graph_yeq_screen   = NULL;
static lv_obj_t *ui_lbl_yeq_name[GRAPH_NUM_EQ]; /* "Y1=" ... "Y4=" row labels */
static lv_obj_t *ui_lbl_yeq_eq[GRAPH_NUM_EQ];   /* Equation content per row */
static lv_obj_t *ui_lbl_yeq_modifier  = NULL;   /* 2nd/ALPHA indicator on Y= screen */
static uint8_t   yeq_selected           = 0;    /* Which Y= row is being edited */
static lv_obj_t *ui_graph_zoom_screen  = NULL;
static lv_obj_t *ui_graph_range_screen = NULL;
static lv_obj_t *ui_lbl_range_xmin     = NULL;
static lv_obj_t *ui_lbl_range_xmax     = NULL;
static lv_obj_t *ui_lbl_range_ymin     = NULL;
static lv_obj_t *ui_lbl_range_ymax     = NULL;
static lv_obj_t *ui_lbl_range_xscl     = NULL;
static lv_obj_t *ui_lbl_range_yscl     = NULL;


/* Styles */
static lv_style_t style_bg;
static lv_style_t style_modifier_2nd;
static lv_style_t style_modifier_alpha;

/* Calculator state */
static char         expression[MAX_EXPR_LEN];
static uint8_t      expr_len       = 0;
static uint8_t      cursor_pos     = 0;  /* Insertion point, 0–expr_len */
static CalcMode_t   current_mode   = MODE_NORMAL;
static bool         angle_degrees  = true;
static float        ans            = 0.0f;
static bool         sto_pending    = false;  /* True after STO — next alpha stores ans */

static HistoryEntry_t history[HISTORY_LINE_COUNT];
static uint8_t        history_count = 0;

/* Graph state */
GraphState_t graph_state = {
    .equations = {{0}},
    .x_min    = -10.0f,
    .x_max    =  10.0f,
    .y_min    = -10.0f,
    .y_max    =  10.0f,
    .x_scl    =   1.0f,
    .y_scl    =   1.0f,
    .active   = false,
};

/* RANGE editor state */
static uint8_t  range_field_selected = 0;   /* 0=xmin 1=xmax 2=ymin 3=ymax 4=xscl 5=yscl */
static char     range_field_buf[16]  = {0};
static uint8_t  range_field_len      = 0;

/* TRACE cursor state */
static float   trace_x      = 0.0f;
static uint8_t trace_eq_idx = 0;

/* Mode to restore after a 2nd/ALPHA modifier is consumed */
static CalcMode_t return_mode = MODE_NORMAL;

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

    /* 2nd modifier indicator (used on Y= screen) */
    lv_style_init(&style_modifier_2nd);
    lv_style_set_text_font(&style_modifier_2nd, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_modifier_2nd, lv_color_hex(COLOR_2ND));

    /* Alpha modifier indicator (used on Y= screen) */
    lv_style_init(&style_modifier_alpha);
    lv_style_set_text_font(&style_modifier_alpha, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_modifier_alpha, lv_color_hex(COLOR_ALPHA));
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
        lv_obj_set_style_text_font(disp_rows[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(disp_rows[i],
                                    lv_color_hex(COLOR_HISTORY_EXPR), 0);
        lv_label_set_long_mode(disp_rows[i], LV_LABEL_LONG_CLIP);
        lv_label_set_text(disp_rows[i], "");
    }

    /* DEG/RAD indicator — small, dimly visible in top-right corner */
    ui_lbl_angle_mode = lv_label_create(scr);
    lv_obj_set_style_text_font(ui_lbl_angle_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_lbl_angle_mode,
                                lv_color_hex(0x444444), 0);
    lv_obj_align(ui_lbl_angle_mode, LV_ALIGN_TOP_RIGHT, -2, 3);
    lv_label_set_text(ui_lbl_angle_mode, "DEG");

    /* Hidden modifier label — kept alive only for Y= screen mirroring */
    ui_lbl_modifier = lv_label_create(scr);
    lv_obj_set_style_opa(ui_lbl_modifier, LV_OPA_TRANSP, 0);
    lv_label_set_text(ui_lbl_modifier, "");

    /* Cursor block — filled rectangle that overlays the insertion point.
     * Sized to match one Montserrat-24 character cell (16 x 26 px). */
    cursor_box = lv_obj_create(scr);
    lv_obj_set_size(cursor_box, 16, 26);
    lv_obj_set_style_bg_color(cursor_box, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_opa(cursor_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cursor_box, 0, 0);
    lv_obj_set_style_pad_all(cursor_box, 0, 0);
    lv_obj_set_style_radius(cursor_box, 0, 0);
    lv_obj_clear_flag(cursor_box, LV_OBJ_FLAG_SCROLLABLE);

    cursor_inner = lv_label_create(cursor_box);
    lv_obj_set_style_text_font(cursor_inner, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(cursor_inner, lv_color_hex(COLOR_BG), 0);
    lv_obj_center(cursor_inner);
    lv_label_set_text(cursor_inner, "");
}

/*---------------------------------------------------------------------------
 * Graph screen initialisation
 *---------------------------------------------------------------------------*/

/**
 * @brief Refreshes all six RANGE field labels from the current graph_state values.
 *        Called whenever graph_state is modified while the RANGE screen is visible.
 */
static void ui_update_range_display(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.4g", graph_state.x_min);
    lv_label_set_text(ui_lbl_range_xmin, buf);
    snprintf(buf, sizeof(buf), "%.4g", graph_state.x_max);
    lv_label_set_text(ui_lbl_range_xmax, buf);
    snprintf(buf, sizeof(buf), "%.4g", graph_state.y_min);
    lv_label_set_text(ui_lbl_range_ymin, buf);
    snprintf(buf, sizeof(buf), "%.4g", graph_state.y_max);
    lv_label_set_text(ui_lbl_range_ymax, buf);
    snprintf(buf, sizeof(buf), "%.4g", graph_state.x_scl);
    lv_label_set_text(ui_lbl_range_xscl, buf);
    snprintf(buf, sizeof(buf), "%.4g", graph_state.y_scl);
    lv_label_set_text(ui_lbl_range_yscl, buf);
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

    /* Four Y= equation rows */
    const char *eq_row_names[] = { "Y1=", "Y2=", "Y3=", "Y4=" };
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        int32_t row_y = 4 + i * 26;
        ui_lbl_yeq_name[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_name[i], 4, row_y);
        lv_obj_set_style_text_color(ui_lbl_yeq_name[i],
                                     lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(ui_lbl_yeq_name[i], eq_row_names[i]);

        ui_lbl_yeq_eq[i] = lv_label_create(ui_graph_yeq_screen);
        lv_obj_set_pos(ui_lbl_yeq_eq[i], 44, row_y);
        lv_obj_set_width(ui_lbl_yeq_eq[i], DISPLAY_W - 48);
        lv_obj_set_style_text_color(ui_lbl_yeq_eq[i],
                                     lv_color_hex(0xFFFFFF), 0);
        lv_label_set_long_mode(ui_lbl_yeq_eq[i], LV_LABEL_LONG_CLIP);
        lv_label_set_text(ui_lbl_yeq_eq[i], "");
    }

    /* Modifier indicator (2nd / ALPHA / A-LOCK) — top right */
    ui_lbl_yeq_modifier = lv_label_create(ui_graph_yeq_screen);
    lv_obj_set_style_text_font(ui_lbl_yeq_modifier, &lv_font_montserrat_14, 0);
    lv_obj_align(ui_lbl_yeq_modifier, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_label_set_text(ui_lbl_yeq_modifier, "");

    /* Hint */
    lv_obj_t *lbl_hint = lv_label_create(ui_graph_yeq_screen);
    lv_obj_set_pos(lbl_hint, 4, DISPLAY_H - 20);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x888888), 0);
    lv_label_set_text(lbl_hint, "GRAPH to plot  CLEAR to reset");

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

    /* Title */
    lv_obj_t *lbl_range_title = lv_label_create(ui_graph_range_screen);
    lv_obj_set_pos(lbl_range_title, 4, 4);
    lv_obj_set_style_text_color(lbl_range_title,
                                 lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_range_title, "RANGE");

    /* Six range fields */
    const char *range_labels[] = {
        "Xmin=", "Xmax=", "Ymin=", "Ymax=", "Xscl=", "Yscl="
    };
    lv_obj_t **range_value_labels[] = {
        &ui_lbl_range_xmin, &ui_lbl_range_xmax,
        &ui_lbl_range_ymin, &ui_lbl_range_ymax,
        &ui_lbl_range_xscl, &ui_lbl_range_yscl
    };
    for (int i = 0; i < 6; i++) {
        int32_t y = 30 + i * 30;
        lv_obj_t *lbl_name = lv_label_create(ui_graph_range_screen);
        lv_obj_set_pos(lbl_name, 4, y);
        lv_obj_set_style_text_color(lbl_name,
                                     lv_color_hex(0xAAAAAA), 0);
        lv_label_set_text(lbl_name, range_labels[i]);

        *range_value_labels[i] = lv_label_create(ui_graph_range_screen);
        lv_obj_set_pos(*range_value_labels[i], 80, y);
        lv_obj_set_style_text_color(*range_value_labels[i],
                                     lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(*range_value_labels[i], "0");
    }

    /* Hint */
    lv_obj_t *lbl_range_hint = lv_label_create(ui_graph_range_screen);
    lv_obj_set_pos(lbl_range_hint, 4, DISPLAY_H - 20);
    lv_obj_set_style_text_color(lbl_range_hint,
                                 lv_color_hex(0x888888), 0);
    lv_label_set_text(lbl_range_hint, "ZOOM for ZStandard");

    /* --- ZOOM menu screen --- */
    ui_graph_zoom_screen = lv_obj_create(scr);
    lv_obj_set_size(ui_graph_zoom_screen, DISPLAY_W, DISPLAY_H);
    lv_obj_set_pos(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_bg_color(ui_graph_zoom_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ui_graph_zoom_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_graph_zoom_screen, 0, 0);
    lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_zoom_title = lv_label_create(ui_graph_zoom_screen);
    lv_obj_set_pos(lbl_zoom_title, 4, 4);
    lv_obj_set_style_text_color(lbl_zoom_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_zoom_title, "ZOOM");

    const char *zoom_items[] = {
        "1: ZStandard   (+/-10)",
        "2: ZTrig       (+/-2pi)",
        "3: ZDecimal    (+/-4.7)",
        "4: ZSquare     (fix aspect)",
        "5: ZInteger    (1px=1unit)",
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *lbl = lv_label_create(ui_graph_zoom_screen);
        lv_obj_set_pos(lbl, 4, 28 + i * 26);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(lbl, zoom_items[i]);
    }

    lv_obj_t *lbl_zoom_hint = lv_label_create(ui_graph_zoom_screen);
    lv_obj_set_pos(lbl_zoom_hint, 4, DISPLAY_H - 20);
    lv_obj_set_style_text_color(lbl_zoom_hint, lv_color_hex(0x888888), 0);
    lv_label_set_text(lbl_zoom_hint, "Press 1-5 to select  CLEAR to exit");

    /* Init graph canvas */
    Graph_Init(scr);
}

/*---------------------------------------------------------------------------
 * UI update functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Positions and styles the block cursor for the given display row.
 *
 * Uses lv_label_get_letter_pos() to compute the pixel X offset of the
 * insertion point, then moves cursor_box to that position and sets its
 * color and inner character to reflect the current input mode:
 *   - Normal / A-LOCK: light-grey block, no inner character, blinks.
 *   - 2nd:             amber block, '^' inside, always on.
 *   - ALPHA / A-LOCK:  green block, 'A' inside, always on.
 *   - STO pending:     green block, 'A' inside, always on.
 *
 * @param row_label  The LVGL label whose text contains the current expression.
 * @param char_pos   Character index within row_label at which to place the cursor.
 */
static void cursor_update(lv_obj_t *row_label, uint32_t char_pos)
{
    if (cursor_box == NULL) return;

    bool show;
    lv_color_t box_color;
    const char *inner_text;

    if (sto_pending) {
        /* STO waiting for a variable letter — show alpha cursor */
        show       = true;
        box_color  = lv_color_hex(COLOR_ALPHA);
        inner_text = "A";
    } else switch (current_mode) {
        case MODE_2ND:
            show       = true;
            box_color  = lv_color_hex(COLOR_2ND);
            inner_text = "^";
            break;
        case MODE_ALPHA:
        case MODE_ALPHA_LOCK:
            show       = true;
            box_color  = lv_color_hex(COLOR_ALPHA);
            inner_text = "A";
            break;
        default:
            show       = cursor_visible;
            box_color  = lv_color_hex(0xCCCCCC);
            inner_text = "";
            break;
    }

    if (!show) {
        lv_obj_add_flag(cursor_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Find pixel position of character at char_pos within the row label */
    lv_point_t pos;
    lv_label_get_letter_pos(row_label, char_pos, &pos);

    int32_t lx = lv_obj_get_x(row_label);
    int32_t ly = lv_obj_get_y(row_label);
    lv_obj_set_pos(cursor_box, lx + pos.x, ly + pos.y);

    lv_obj_set_style_bg_color(cursor_box, box_color, 0);
    lv_label_set_text(cursor_inner, inner_text);
    lv_obj_clear_flag(cursor_box, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Redraws all DISP_ROW_COUNT display rows from the history buffer
 *        and the current input expression.
 *
 * Display line model (li = absolute line index):
 *   even li:               committed expression — left-aligned, grey
 *   odd  li:               result               — right-aligned, white
 *   li == history_count*2: current expression being typed + cursor
 */
static void ui_refresh_display(void)
{
    if (disp_rows[0] == NULL) return;

    int total = history_count * 2 + 1;   /* lines incl. current expr */
    int start = (total > DISP_ROW_COUNT) ? (total - DISP_ROW_COUNT) : 0;

    lv_label_set_text(ui_lbl_angle_mode, angle_degrees ? "DEG" : "RAD");

    for (int row = 0; row < DISP_ROW_COUNT; row++) {
        int li = start + row;

        if (li >= total) {
            /* Below current position — blank */
            lv_label_set_text(disp_rows[row], "");
            continue;
        }

        if (li == history_count * 2) {
            /* Current expression being typed */
            lv_obj_set_style_text_color(disp_rows[row],
                                        lv_color_hex(COLOR_EXPR), 0);
            lv_obj_set_style_text_align(disp_rows[row],
                                        LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(disp_rows[row], expression);
            cursor_update(disp_rows[row], cursor_pos);
        } else if (li % 2 == 0) {
            /* Committed expression — left-aligned, grey */
            int entry = (li / 2) % HISTORY_LINE_COUNT;
            lv_obj_set_style_text_color(disp_rows[row],
                                        lv_color_hex(COLOR_HISTORY_EXPR), 0);
            lv_obj_set_style_text_align(disp_rows[row],
                                        LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(disp_rows[row], history[entry].expression);
        } else {
            /* Result — right-aligned, white */
            int entry = ((li - 1) / 2) % HISTORY_LINE_COUNT;
            lv_obj_set_style_text_color(disp_rows[row],
                                        lv_color_hex(COLOR_HISTORY_RES), 0);
            lv_obj_set_style_text_align(disp_rows[row],
                                        LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(disp_rows[row], history[entry].result);
        }
    }

    /* If the current expression row scrolled off-screen, hide the cursor */
    if (history_count * 2 < start && cursor_box != NULL) {
        lv_obj_add_flag(cursor_box, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief LVGL timer callback — blinks the cursor every CURSOR_BLINK_MS.
 */
static void cursor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    /* 2nd and ALPHA show a fixed cursor (always on).
       Normal and ALPHA_LOCK blink the block. */
    if (sto_pending || current_mode == MODE_2ND || current_mode == MODE_ALPHA) {
        cursor_visible = true;
    } else {
        cursor_visible = !cursor_visible;
    }
    /* Called from lv_task_handler() — DefaultTask already holds the LVGL
       mutex. Do NOT call lvgl_lock() here or it will deadlock. */
    ui_refresh_display();
}

/**
 * @brief Updates the hidden modifier label (for Y= screen mirroring)
 *        and refreshes the main display so the cursor reflects current mode.
 */
static void ui_update_status_bar(void)
{
    /* Update the hidden modifier label text/color for Y= mirroring */
    switch (current_mode) {
    case MODE_2ND:
        lv_obj_add_style(ui_lbl_modifier, &style_modifier_2nd, 0);
        lv_label_set_text(ui_lbl_modifier, "2nd");
        break;
    case MODE_ALPHA:
        lv_obj_remove_style(ui_lbl_modifier, &style_modifier_2nd, 0);
        lv_obj_add_style(ui_lbl_modifier, &style_modifier_alpha, 0);
        lv_label_set_text(ui_lbl_modifier, "ALPHA");
        break;
    case MODE_ALPHA_LOCK:
        lv_obj_remove_style(ui_lbl_modifier, &style_modifier_2nd, 0);
        lv_obj_add_style(ui_lbl_modifier, &style_modifier_alpha, 0);
        lv_label_set_text(ui_lbl_modifier, "A-LOCK");
        break;
    default:
        if (sto_pending) {
            lv_obj_remove_style(ui_lbl_modifier, &style_modifier_alpha, 0);
            lv_obj_add_style(ui_lbl_modifier, &style_modifier_2nd, 0);
            lv_label_set_text(ui_lbl_modifier, "STO\xE2\x86\x92");
        } else {
            lv_label_set_text(ui_lbl_modifier, "");
        }
        break;
    }

    /* Mirror modifier onto Y= screen */
    if (ui_lbl_yeq_modifier != NULL) {
        lv_label_set_text(ui_lbl_yeq_modifier,
                          lv_label_get_text(ui_lbl_modifier));
        lv_obj_set_style_text_color(ui_lbl_yeq_modifier,
                                    lv_obj_get_style_text_color(ui_lbl_modifier, 0),
                                    0);
    }

    /* Cursor char changes with mode — force an immediate refresh */
    cursor_visible = true;
    ui_refresh_display();
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
 * @brief Inserts a single character at cursor_pos and advances the cursor.
 */
static void expr_insert_char(char c)
{
    if (expr_len >= MAX_EXPR_LEN - 1) return;
    memmove(&expression[cursor_pos + 1], &expression[cursor_pos],
            expr_len - cursor_pos + 1);
    expression[cursor_pos] = c;
    expr_len++;
    cursor_pos++;
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
    memmove(&expression[cursor_pos - 1], &expression[cursor_pos],
            expr_len - cursor_pos + 1);
    expr_len--;
    cursor_pos--;
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

/* Applies one of the five TI-81 ZOOM presets to graph_state.
 * preset 1=ZStandard, 2=ZTrig, 3=ZDecimal, 4=ZSquare, 5=ZInteger. */
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
    }
}

/*---------------------------------------------------------------------------
 * RANGE editor helpers
 *---------------------------------------------------------------------------*/

/* Returns the LVGL label pointer for RANGE field index idx (0=Xmin … 5=Yscl). */
static lv_obj_t *range_get_label(uint8_t idx)
{
    switch (idx) {
    case 0: return ui_lbl_range_xmin;
    case 1: return ui_lbl_range_xmax;
    case 2: return ui_lbl_range_ymin;
    case 3: return ui_lbl_range_ymax;
    case 4: return ui_lbl_range_xscl;
    case 5: return ui_lbl_range_yscl;
    default: return NULL;
    }
}

/* Returns the committed graph_state value for RANGE field index idx. */
static float range_get_field_value(uint8_t idx)
{
    switch (idx) {
    case 0: return graph_state.x_min;
    case 1: return graph_state.x_max;
    case 2: return graph_state.y_min;
    case 3: return graph_state.y_max;
    case 4: return graph_state.x_scl;
    case 5: return graph_state.y_scl;
    default: return 0.0f;
    }
}

/* Parses range_field_buf and writes it into the appropriate graph_state field.
 * Scale fields (xscl, yscl) reject zero or negative values. */
static void range_commit_field(void)
{
    if (range_field_len == 0)
        return;
    float val = strtof(range_field_buf, NULL);
    switch (range_field_selected) {
    case 0: graph_state.x_min = val; break;
    case 1: graph_state.x_max = val; break;
    case 2: graph_state.y_min = val; break;
    case 3: graph_state.y_max = val; break;
    case 4: if (val > 0.0f) graph_state.x_scl = val; break;  /* must be positive */
    case 5: if (val > 0.0f) graph_state.y_scl = val; break;  /* must be positive */
    }
}

/* Highlights the active RANGE field label in yellow; all others are white. */
static void range_update_highlight(void)
{
    for (uint8_t i = 0; i < 6; i++) {
        lv_obj_t *lbl = range_get_label(i);
        if (lbl == NULL)
            continue;
        lv_obj_set_style_text_color(lbl,
            (i == range_field_selected) ? lv_color_hex(0xFFFF00)
                                        : lv_color_hex(0xFFFFFF),
            0);
    }
}

/*---------------------------------------------------------------------------
 * Token execution
 *---------------------------------------------------------------------------*/

/**
 * @brief Processes a single calculator token from the keypad queue.
 * @param t  Token to execute.
 */
void Execute_Token(Token_t t)
{
    /*--- Y= equation editor mode handler -----------------------------------*/
    if (current_mode == MODE_GRAPH_YEQ) {
        char *eq  = graph_state.equations[yeq_selected];
        uint8_t eq_len = strlen(eq);
        const char *append = NULL;
        char num_buf[2] = {0, 0};

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
            eq[0] = '\0';
            lvgl_lock();
            lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], "");
            lvgl_unlock();
            return;
        case TOKEN_DEL:
            if (eq_len > 0) {
                eq[--eq_len] = '\0';
                lvgl_lock();
                lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
                lvgl_unlock();
            }
            return;
        case TOKEN_UP:
            if (yeq_selected > 0) yeq_selected--;
            lvgl_lock();
            yeq_update_highlight();
            lvgl_unlock();
            return;
        case TOKEN_DOWN:
            if (yeq_selected < GRAPH_NUM_EQ - 1) yeq_selected++;
            lvgl_lock();
            yeq_update_highlight();
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
        case TOKEN_SQRT:    append = "sqrt("; break;
        case TOKEN_ABS:     append = "abs(";  break;
        case TOKEN_SQUARE:  append = "^2";    break;
        case TOKEN_PI:      append = "pi";    break;
        case TOKEN_NEG:     append = "-";     break;
        case TOKEN_X_INV:   append = "^-1";   break;
        case TOKEN_ANS:     append = "ANS";   break;
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
        default:            return;
        }

        if (append != NULL) {
            size_t len = strlen(append);
            if (eq_len + len < 63) {
                strcat(eq, append);
                lvgl_lock();
                lv_label_set_text(ui_lbl_yeq_eq[yeq_selected], eq);
                lvgl_unlock();
            }
        }
        return;
    }

    /*--- RANGE field editor mode handler -----------------------------------*/
    if (current_mode == MODE_GRAPH_RANGE) {
        lv_obj_t *lbl;
        char stored_buf[16];

        switch (t) {
        case TOKEN_0 ... TOKEN_9:
            if (range_field_len < sizeof(range_field_buf) - 1) {
                range_field_buf[range_field_len++] = (char)((t - TOKEN_0) + '0');
                range_field_buf[range_field_len]   = '\0';
                lbl = range_get_label(range_field_selected);
                lvgl_lock();
                if (lbl) lv_label_set_text(lbl, range_field_buf);
                lvgl_unlock();
            }
            return;

        case TOKEN_DECIMAL:
            if (range_field_len < sizeof(range_field_buf) - 1 &&
                strchr(range_field_buf, '.') == NULL) {
                range_field_buf[range_field_len++] = '.';
                range_field_buf[range_field_len]   = '\0';
                lbl = range_get_label(range_field_selected);
                lvgl_lock();
                if (lbl) lv_label_set_text(lbl, range_field_buf);
                lvgl_unlock();
            }
            return;

        case TOKEN_NEG:
            if (range_field_len > 0 && range_field_buf[0] == '-') {
                memmove(range_field_buf, range_field_buf + 1, range_field_len);
                range_field_len--;
            } else if (range_field_len < sizeof(range_field_buf) - 1) {
                memmove(range_field_buf + 1, range_field_buf, range_field_len + 1);
                range_field_buf[0] = '-';
                range_field_len++;
            }
            lbl = range_get_label(range_field_selected);
            lvgl_lock();
            if (lbl) {
                if (range_field_len > 0) {
                    lv_label_set_text(lbl, range_field_buf);
                } else {
                    snprintf(stored_buf, sizeof(stored_buf), "%.4g",
                             range_get_field_value(range_field_selected));
                    lv_label_set_text(lbl, stored_buf);
                }
            }
            lvgl_unlock();
            return;

        case TOKEN_DEL:
            if (range_field_len > 0)
                range_field_buf[--range_field_len] = '\0';
            lbl = range_get_label(range_field_selected);
            lvgl_lock();
            if (lbl) {
                if (range_field_len > 0) {
                    lv_label_set_text(lbl, range_field_buf);
                } else {
                    snprintf(stored_buf, sizeof(stored_buf), "%.4g",
                             range_get_field_value(range_field_selected));
                    lv_label_set_text(lbl, stored_buf);
                }
            }
            lvgl_unlock();
            return;

        case TOKEN_ENTER:
        case TOKEN_DOWN:
            range_commit_field();
            range_field_len      = 0;
            range_field_buf[0]   = '\0';
            if (range_field_selected < 5) range_field_selected++;
            lvgl_lock();
            ui_update_range_display();
            range_update_highlight();
            lvgl_unlock();
            return;

        case TOKEN_UP:
            range_commit_field();
            range_field_len      = 0;
            range_field_buf[0]   = '\0';
            if (range_field_selected > 0) range_field_selected--;
            lvgl_lock();
            ui_update_range_display();
            range_update_highlight();
            lvgl_unlock();
            return;

        case TOKEN_ZOOM:
            graph_state.x_min = -10.0f;
            graph_state.x_max =  10.0f;
            graph_state.y_min = -10.0f;
            graph_state.y_max =  10.0f;
            graph_state.x_scl =   1.0f;
            graph_state.y_scl =   1.0f;
            range_field_len      = 0;
            range_field_buf[0]   = '\0';
            lvgl_lock();
            ui_update_range_display();
            range_update_highlight();
            lvgl_unlock();
            return;

        case TOKEN_GRAPH:
            range_commit_field();
            current_mode         = MODE_NORMAL;
            range_field_selected = 0;
            range_field_len      = 0;
            range_field_buf[0]   = '\0';
            lvgl_lock();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_Render(angle_degrees);
            lvgl_unlock();
            return;

        case TOKEN_RANGE:
            current_mode         = MODE_NORMAL;
            range_field_selected = 0;
            range_field_len      = 0;
            range_field_buf[0]   = '\0';
            lvgl_lock();
            ui_update_range_display();
            lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;

        default:
            return;
        }
    }

    /*--- ZOOM preset menu mode handler -------------------------------------*/
    if (current_mode == MODE_GRAPH_ZOOM) {
        uint8_t preset = 0;
        switch (t) {
        case TOKEN_0 ... TOKEN_9:
            preset = (uint8_t)(t - TOKEN_0);
            break;
        case TOKEN_CLEAR:
        case TOKEN_ZOOM:
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            lvgl_unlock();
            return;
        default:
            return;
        }
        if (preset >= 1 && preset <= 5) {
            apply_zoom_preset(preset);
            current_mode = MODE_NORMAL;
            lvgl_lock();
            lv_obj_add_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
            Graph_SetVisible(true);
            Graph_Render(angle_degrees);
            lvgl_unlock();
        }
        return;
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
            strncpy(history[idx].result, val_buf, MAX_RESULT_LEN - 1);
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
            cursor_pos--;
            Update_Calculator_Display();
        }
        break;

    case TOKEN_RIGHT:
        if (cursor_pos < expr_len) {
            cursor_pos++;
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
                strncpy(result_str, result.error_msg,
                        MAX_RESULT_LEN - 1);
            } else {
                Calc_FormatResult(result.value, result_str,
                                  MAX_RESULT_LEN);
                ans = result.value;
            }

            /* Store in history */
            uint8_t idx = history_count % HISTORY_LINE_COUNT;
            strncpy(history[idx].expression, expression,
                    MAX_EXPR_LEN - 1);
            strncpy(history[idx].result, result_str,
                    MAX_RESULT_LEN - 1);
            history_count++;

            expr_len      = 0;
            cursor_pos    = 0;
            expression[0] = '\0';

            lvgl_lock();
            ui_update_history();
            lvgl_unlock();
        }
        break;

    case TOKEN_CLEAR:
        expr_len      = 0;
        cursor_pos    = 0;
        expression[0] = '\0';
        Update_Calculator_Display();
        break;

    case TOKEN_DEL:
        expr_delete_at_cursor();
        Update_Calculator_Display();
        break;

    case TOKEN_MODE:
        angle_degrees = !angle_degrees;
        Update_Calculator_Display();
        break;

    case TOKEN_SIN:     expr_insert_str("sin(");  Update_Calculator_Display(); break;
    case TOKEN_COS:     expr_insert_str("cos(");  Update_Calculator_Display(); break;
    case TOKEN_TAN:     expr_insert_str("tan(");  Update_Calculator_Display(); break;
    case TOKEN_ASIN:    expr_insert_str("asin("); Update_Calculator_Display(); break;
    case TOKEN_ACOS:    expr_insert_str("acos("); Update_Calculator_Display(); break;
    case TOKEN_ATAN:    expr_insert_str("atan("); Update_Calculator_Display(); break;
    case TOKEN_ABS:     expr_insert_str("abs(");  Update_Calculator_Display(); break;
    case TOKEN_LN:      expr_insert_str("ln(");   Update_Calculator_Display(); break;
    case TOKEN_LOG:     expr_insert_str("log(");  Update_Calculator_Display(); break;
    case TOKEN_SQRT:    expr_insert_str("sqrt("); Update_Calculator_Display(); break;
    case TOKEN_EE:      expr_insert_str("*10^");  Update_Calculator_Display(); break;
    case TOKEN_E_X:     expr_insert_str("exp(");  Update_Calculator_Display(); break;
    case TOKEN_TEN_X:   expr_insert_str("10^(");  Update_Calculator_Display(); break;
    case TOKEN_PI:      expr_insert_str("pi");    Update_Calculator_Display(); break;
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
        current_mode = MODE_GRAPH_YEQ;
        graph_state.active = false;
        /* Switch to Y= editor — show equation entry screen */
        lvgl_lock();
        Graph_SetVisible(false);
        lv_obj_clear_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
        yeq_update_highlight();
        lvgl_unlock();
        break;

    case TOKEN_RANGE:
        /* Switch to RANGE editor */
        current_mode         = MODE_GRAPH_RANGE;
        range_field_selected = 0;
        range_field_len      = 0;
        range_field_buf[0]   = '\0';
        graph_state.active   = false;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(false);
        lv_obj_clear_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        ui_update_range_display();
        range_update_highlight();
        lvgl_unlock();
        break;

    case TOKEN_ZOOM:
        /* Show ZOOM preset menu */
        current_mode = MODE_GRAPH_ZOOM;
        graph_state.active = false;
        lvgl_lock();
        lv_obj_add_flag(ui_graph_yeq_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_graph_range_screen, LV_OBJ_FLAG_HIDDEN);
        Graph_SetVisible(false);
        lv_obj_clear_flag(ui_graph_zoom_screen, LV_OBJ_FLAG_HIDDEN);
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
        /* Find the first active (non-empty) Y= equation to start on */
        trace_eq_idx = 0;
        for (uint8_t i = 0; i < GRAPH_NUM_EQ; i++) {
            if (strlen(graph_state.equations[i]) > 0) {
                trace_eq_idx = i;
                break;
            }
        }
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
    xSemaphoreTake(xLVGL_Ready, portMAX_DELAY);

    lvgl_lock();
    ui_init_styles();
    ui_init_screen();
    ui_init_graph_screens();
    cursor_timer = lv_timer_create(cursor_timer_cb, CURSOR_BLINK_MS, NULL);
    ui_refresh_display();
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