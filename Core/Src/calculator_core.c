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

static HistoryEntry_t history[HISTORY_LINE_COUNT];
static uint8_t        history_count = 0;

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
    default:
        lv_label_set_text(ui_lbl_modifier, "");
        break;
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
 * Token execution
 *--------------------------------------------------------------------------*/

/**
 * @brief Processes a single calculator token from the keypad queue.
 * @param t  Token to execute.
 */
void Execute_Token(Token_t t)
{
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
        current_mode   = MODE_NORMAL;
    } else if (current_mode == MODE_ALPHA) {
        token_to_send  = key.alpha;
        current_mode   = MODE_NORMAL;
    } else {
        token_to_send  = key.normal;
    }

    /* Handle sticky modifier keys */
    if (token_to_send == TOKEN_2ND) {
        current_mode = MODE_2ND;
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_ALPHA) {
        current_mode = MODE_ALPHA;
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