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
 *--------------------------------------------------------------------------*/

#define DISPLAY_W           320
#define DISPLAY_H           240

#define STATUS_BAR_H        20
#define RESULT_AREA_H       40
#define EXPR_AREA_H         30
#define HISTORY_AREA_H      (DISPLAY_H - STATUS_BAR_H - EXPR_AREA_H - RESULT_AREA_H)

#define HISTORY_LINE_COUNT  4
#define MAX_EXPR_LEN        64
#define MAX_RESULT_LEN      32

/* Color scheme */
#define COLOR_BG            0x1A1A1A    /* Near black background */
#define COLOR_STATUS_BG     0x2A2A2A    /* Slightly lighter status bar */
#define COLOR_HISTORY       0x888888    /* Grey history text */
#define COLOR_EXPR          0xCCCCCC    /* Light grey expression text */
#define COLOR_RESULT        0xFFFFFF    /* White result text */
#define COLOR_ACCENT        0x4A90D9    /* Blue accent */
#define COLOR_2ND           0xF5A623    /* Amber for 2nd mode indicator */
#define COLOR_ALPHA         0x7ED321    /* Green for alpha mode indicator */

/*---------------------------------------------------------------------------
 * External references
 *--------------------------------------------------------------------------*/

extern SemaphoreHandle_t xLVGL_Mutex;
extern SemaphoreHandle_t xLVGL_Ready;
extern const uint32_t TI81_LookupTable_Size;

/*---------------------------------------------------------------------------
 * Private types
 *--------------------------------------------------------------------------*/

typedef struct {
    char expression[MAX_EXPR_LEN];
    char result[MAX_RESULT_LEN];
} HistoryEntry_t;

/*---------------------------------------------------------------------------
 * Private variables
 *--------------------------------------------------------------------------*/

/* LVGL objects */
static lv_obj_t *ui_status_bar;
static lv_obj_t *ui_lbl_angle_mode;
static lv_obj_t *ui_lbl_modifier;
static lv_obj_t *ui_lbl_ans_indicator;
static lv_obj_t *ui_history_labels[HISTORY_LINE_COUNT];
static lv_obj_t *ui_history_results[HISTORY_LINE_COUNT];
static lv_obj_t *ui_lbl_expression;
static lv_obj_t *ui_lbl_result;

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
static lv_style_t style_status_bar;
static lv_style_t style_history;
static lv_style_t style_expression;
static lv_style_t style_result;
static lv_style_t style_modifier_2nd;
static lv_style_t style_modifier_alpha;

/* Calculator state */
static char         expression[MAX_EXPR_LEN];
static uint8_t      expr_len       = 0;
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
 *--------------------------------------------------------------------------*/

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
 *--------------------------------------------------------------------------*/

static void ui_init_styles(void)
{
    /* Background */
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(COLOR_BG));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_pad_all(&style_bg, 0);

    /* Status bar */
    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, lv_color_hex(COLOR_STATUS_BG));
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_border_width(&style_status_bar, 0);
    lv_style_set_pad_all(&style_status_bar, 4);

    /* History text */
    lv_style_init(&style_history);
    lv_style_set_text_font(&style_history, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_history, lv_color_hex(COLOR_HISTORY));

    /* Expression text */
    lv_style_init(&style_expression);
    lv_style_set_text_font(&style_expression, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_expression, lv_color_hex(COLOR_EXPR));

    /* Result text */
    lv_style_init(&style_result);
    lv_style_set_text_font(&style_result, &lv_font_montserrat_28);
    lv_style_set_text_color(&style_result, lv_color_hex(COLOR_RESULT));

    /* 2nd modifier indicator */
    lv_style_init(&style_modifier_2nd);
    lv_style_set_text_font(&style_modifier_2nd, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_modifier_2nd, lv_color_hex(COLOR_2ND));

    /* Alpha modifier indicator */
    lv_style_init(&style_modifier_alpha);
    lv_style_set_text_font(&style_modifier_alpha, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_modifier_alpha, lv_color_hex(COLOR_ALPHA));
}

static void ui_init_screen(void)
{
    /* Root screen */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &style_bg, 0);
    lv_obj_set_size(scr, DISPLAY_W, DISPLAY_H);

    /* --- Status bar --- */
    ui_status_bar = lv_obj_create(scr);
    lv_obj_set_size(ui_status_bar, DISPLAY_W, STATUS_BAR_H);
    lv_obj_set_pos(ui_status_bar, 0, 0);
    lv_obj_add_style(ui_status_bar, &style_status_bar, 0);
    lv_obj_clear_flag(ui_status_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Angle mode label — left side of status bar */
    ui_lbl_angle_mode = lv_label_create(ui_status_bar);
    lv_obj_add_style(ui_lbl_angle_mode, &style_history, 0);
    lv_obj_align(ui_lbl_angle_mode, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(ui_lbl_angle_mode, "DEG");

    /* Modifier indicator — centre of status bar */
    ui_lbl_modifier = lv_label_create(ui_status_bar);
    lv_obj_add_style(ui_lbl_modifier, &style_modifier_2nd, 0);
    lv_obj_align(ui_lbl_modifier, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(ui_lbl_modifier, "");

    /* ANS indicator — right side of status bar */
    ui_lbl_ans_indicator = lv_label_create(ui_status_bar);
    lv_obj_add_style(ui_lbl_ans_indicator, &style_history, 0);
    lv_obj_align(ui_lbl_ans_indicator, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_text(ui_lbl_ans_indicator, "ANS");

    /* --- History area --- */
    int32_t history_y = STATUS_BAR_H;
    int32_t line_h    = HISTORY_AREA_H / HISTORY_LINE_COUNT;

    for (int i = 0; i < HISTORY_LINE_COUNT; i++) {
        /* Expression side — left aligned */
        ui_history_labels[i] = lv_label_create(scr);
        lv_obj_add_style(ui_history_labels[i], &style_history, 0);
        lv_obj_set_pos(ui_history_labels[i], 4,
                       history_y + (i * line_h) + (line_h / 2) - 7);
        lv_label_set_text(ui_history_labels[i], "");
        lv_label_set_long_mode(ui_history_labels[i],
                               LV_LABEL_LONG_CLIP);
        lv_obj_set_width(ui_history_labels[i], 200);

        /* Result side — right aligned */
        ui_history_results[i] = lv_label_create(scr);
        lv_obj_add_style(ui_history_results[i], &style_history, 0);
        lv_obj_set_pos(ui_history_results[i], 204,
                       history_y + (i * line_h) + (line_h / 2) - 7);
        lv_label_set_text(ui_history_results[i], "");
        lv_label_set_long_mode(ui_history_results[i],
                               LV_LABEL_LONG_CLIP);
        lv_obj_set_width(ui_history_results[i], 112);
    }

    /* --- Expression area --- */
    ui_lbl_expression = lv_label_create(scr);
    lv_obj_add_style(ui_lbl_expression, &style_expression, 0);
    lv_obj_set_pos(ui_lbl_expression, 4,
                   STATUS_BAR_H + HISTORY_AREA_H + 6);
    lv_label_set_long_mode(ui_lbl_expression, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui_lbl_expression, DISPLAY_W - 8);
    lv_label_set_text(ui_lbl_expression, "");

    /* --- Result area --- */
    ui_lbl_result = lv_label_create(scr);
    lv_obj_add_style(ui_lbl_result, &style_result, 0);
    lv_obj_set_pos(ui_lbl_result, 0,
                   STATUS_BAR_H + HISTORY_AREA_H + EXPR_AREA_H);
    lv_obj_set_width(ui_lbl_result, DISPLAY_W - 8);
    lv_obj_set_style_text_align(ui_lbl_result, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(ui_lbl_result, "0");
}

/*---------------------------------------------------------------------------
 * Graph screen initialisation
 *--------------------------------------------------------------------------*/
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
 *--------------------------------------------------------------------------*/

/**
 * @brief Updates the modifier indicator in the status bar.
 */
static void ui_update_status_bar(void)
{
    /* Angle mode */
    lv_label_set_text(ui_lbl_angle_mode, angle_degrees ? "DEG" : "RAD");

    /* Modifier mode */
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
            lv_label_set_text(ui_lbl_modifier, "STO\xE2\x86\x92");  /* STO→ */
        } else {
            lv_label_set_text(ui_lbl_modifier, "");
        }
        break;
    }

    /* Mirror modifier onto Y= screen if it exists */
    if (ui_lbl_yeq_modifier != NULL) {
        lv_label_set_text(ui_lbl_yeq_modifier,
                          lv_label_get_text(ui_lbl_modifier));
        lv_obj_set_style_text_color(ui_lbl_yeq_modifier,
                                    lv_obj_get_style_text_color(ui_lbl_modifier, 0),
                                    0);
    }
}

/**
 * @brief Refreshes all history lines from the history buffer.
 */
static void ui_update_history(void)
{
    for (int i = 0; i < HISTORY_LINE_COUNT; i++) {
        int entry = history_count - HISTORY_LINE_COUNT + i;
        if (entry >= 0 && entry < history_count) {
            lv_label_set_text(ui_history_labels[i],
                              history[entry % HISTORY_LINE_COUNT].expression);
            lv_label_set_text(ui_history_results[i],
                              history[entry % HISTORY_LINE_COUNT].result);
        } else {
            lv_label_set_text(ui_history_labels[i], "");
            lv_label_set_text(ui_history_results[i], "");
        }
    }
}

/**
 * @brief Updates the expression and result display labels.
 */
void Update_Calculator_Display(void)
{
    if (ui_lbl_expression == NULL || ui_lbl_result == NULL)
        return;

    lvgl_lock();
    lv_label_set_text(ui_lbl_expression,
                      (expr_len == 0) ? "" : expression);
    lv_label_set_text(ui_lbl_result,
                      (expr_len == 0) ? "0" : expression);
    ui_update_status_bar();
    lvgl_unlock();
}

/*---------------------------------------------------------------------------
 * Y= editor helpers
 *--------------------------------------------------------------------------*/

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
 *--------------------------------------------------------------------------*/

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
 *--------------------------------------------------------------------------*/

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
 *--------------------------------------------------------------------------*/

/**
 * @brief Processes a single calculator token from the keypad queue.
 * @param t  Token to execute.
 */
void Execute_Token(Token_t t)
{
    /* Handle Y= equation editing mode */
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

    /* Handle RANGE field editing mode */
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

    /* Handle ZOOM menu mode */
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

    /* Handle TRACE cursor mode */
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

    /* STO pending — next alpha key stores ans to that variable */
    if (sto_pending) {
        if (t >= TOKEN_A && t <= TOKEN_Z) {
            calc_variables[t - TOKEN_A] = ans;
            sto_pending = false;
            /* Show "X=value" in result label briefly via expression area */
            char stored_buf[32];
            char val_buf[16];
            Calc_FormatResult(ans, val_buf, sizeof(val_buf));
            static const char var_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            snprintf(stored_buf, sizeof(stored_buf), "%c=%s",
                     var_names[t - TOKEN_A], val_buf);
            lvgl_lock();
            lv_label_set_text(ui_lbl_result, stored_buf);
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
        if (expr_len < MAX_EXPR_LEN - 1) {
            char c = (char)((t - TOKEN_0) + '0');
            expression[expr_len++] = c;
            expression[expr_len]   = '\0';
            Update_Calculator_Display();
        }
        break;

    case TOKEN_DECIMAL:
        if (expr_len < MAX_EXPR_LEN - 1) {
            expression[expr_len++] = '.';
            expression[expr_len]   = '\0';
            Update_Calculator_Display();
        }
        break;

    case TOKEN_ADD:
        if (expr_len < MAX_EXPR_LEN - 1) {
            expression[expr_len++] = '+';
            expression[expr_len]   = '\0';
            Update_Calculator_Display();
        }
        break;

    case TOKEN_SUB:
        if (expr_len < MAX_EXPR_LEN - 1) {
            expression[expr_len++] = '-';
            expression[expr_len]   = '\0';
            Update_Calculator_Display();
        }
        break;

    case TOKEN_MULT:
        if (expr_len < MAX_EXPR_LEN - 1) {
            expression[expr_len++] = '*';
            expression[expr_len]   = '\0';
            Update_Calculator_Display();
        }
        break;

    case TOKEN_DIV:
        if (expr_len < MAX_EXPR_LEN - 1) {
            expression[expr_len++] = '/';
            expression[expr_len]   = '\0';
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
        expression[0] = '\0';

        lvgl_lock();
        lv_label_set_text(ui_lbl_result, result_str);
        lv_label_set_text(ui_lbl_expression, "");
        ui_update_history();
        lvgl_unlock();
    }
    break;

    case TOKEN_CLEAR:
        expr_len      = 0;
        expression[0] = '\0';
        Update_Calculator_Display();
        break;

    case TOKEN_DEL:
        if (expr_len > 0) {
            expression[--expr_len] = '\0';
            Update_Calculator_Display();
        }
        break;

    case TOKEN_MODE:
        angle_degrees = !angle_degrees;
        Update_Calculator_Display();
        break;

        case TOKEN_SIN:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "sin(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_COS:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "cos(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_TAN:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "tan(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_ASIN:
    if (expr_len < MAX_EXPR_LEN - 5) {
        strcat(expression, "asin(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_ACOS:
    if (expr_len < MAX_EXPR_LEN - 5) {
        strcat(expression, "acos(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_ATAN:
    if (expr_len < MAX_EXPR_LEN - 5) {
        strcat(expression, "atan(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_ABS:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "abs(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_LN:
    if (expr_len < MAX_EXPR_LEN - 3) {
        strcat(expression, "ln(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_LOG:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "log(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_SQRT:
    if (expr_len < MAX_EXPR_LEN - 5) {
        strcat(expression, "sqrt(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_SQUARE:
    if (expr_len < MAX_EXPR_LEN - 2) {
        strcat(expression, "^2");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_X_INV:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "^-1");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_POWER:
    if (expr_len < MAX_EXPR_LEN - 1) {
        expression[expr_len++] = '^';
        expression[expr_len]   = '\0';
        Update_Calculator_Display();
    }
    break;

case TOKEN_L_PAR:
    if (expr_len < MAX_EXPR_LEN - 1) {
        expression[expr_len++] = '(';
        expression[expr_len]   = '\0';
        Update_Calculator_Display();
    }
    break;

case TOKEN_R_PAR:
    if (expr_len < MAX_EXPR_LEN - 1) {
        expression[expr_len++] = ')';
        expression[expr_len]   = '\0';
        Update_Calculator_Display();
    }
    break;

case TOKEN_NEG:
    if (expr_len < MAX_EXPR_LEN - 1) {
        expression[expr_len++] = '-';
        expression[expr_len]   = '\0';
        Update_Calculator_Display();
    }
    break;

case TOKEN_EE:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "*10^");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_E_X:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "exp(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_TEN_X:
    if (expr_len < MAX_EXPR_LEN - 4) {
        strcat(expression, "10^(");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_PI:
    if (expr_len < MAX_EXPR_LEN - 2) {
        strcat(expression, "pi");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_ANS:
    if (expr_len < MAX_EXPR_LEN - 3) {
        strcat(expression, "ANS");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

case TOKEN_ENTRY:
    if (history_count > 0) {
        uint8_t idx = (history_count - 1) % HISTORY_LINE_COUNT;
        strncpy(expression, history[idx].expression, MAX_EXPR_LEN - 1);
        expression[MAX_EXPR_LEN - 1] = '\0';
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

/* Alpha characters A–Z and special alpha tokens */
case TOKEN_A: case TOKEN_B: case TOKEN_C: case TOKEN_D: case TOKEN_E:
case TOKEN_F: case TOKEN_G: case TOKEN_H: case TOKEN_I: case TOKEN_J:
case TOKEN_K: case TOKEN_L: case TOKEN_M: case TOKEN_N: case TOKEN_O:
case TOKEN_P: case TOKEN_Q: case TOKEN_R: case TOKEN_S: case TOKEN_T:
case TOKEN_U: case TOKEN_V: case TOKEN_W: case TOKEN_X: case TOKEN_Y:
case TOKEN_Z: {
    static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char ch[2] = { alpha_chars[t - TOKEN_A], '\0' };
    if (expr_len < MAX_EXPR_LEN - 1) {
        strcat(expression, ch);
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;
}
case TOKEN_THETA:
    if (expr_len < MAX_EXPR_LEN - 1) {
        strcat(expression, "θ");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;
case TOKEN_SPACE:
    if (expr_len < MAX_EXPR_LEN - 1) {
        strcat(expression, " ");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;
case TOKEN_COMMA:
    if (expr_len < MAX_EXPR_LEN - 1) {
        strcat(expression, ",");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;
case TOKEN_QUOTES:
    if (expr_len < MAX_EXPR_LEN - 1) {
        strcat(expression, "\"");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;
case TOKEN_QSTN_M:
    if (expr_len < MAX_EXPR_LEN - 1) {
        strcat(expression, "?");
        expr_len = strlen(expression);
        Update_Calculator_Display();
    }
    break;

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
 *--------------------------------------------------------------------------*/

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
 *--------------------------------------------------------------------------*/

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