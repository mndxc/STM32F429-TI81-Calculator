/**
 * @file    calculator_core_test_stubs.h
 * @brief   Host-test stubs replacing LVGL, RTOS, and cross-module dependencies
 *          when calculator_core.c is compiled with -DHOST_TEST=1.
 *
 * Provides:
 *   - LVGL type stubs (lv_obj_t, lv_timer_t, lv_style_t, lv_color_t, lv_font_t)
 *   - LVGL constant and function no-ops so calculator_core.c compiles unchanged
 *   - FreeRTOS / CMSIS-RTOS type and function stubs
 *   - External-function no-op stubs for graph_ui.c, ui_matrix.c, ui_prgm.c,
 *     persist.c, and app_init.c symbols referenced in calculator_core.c
 *   - calc_internal.h replacement: HistoryEntry_t, display constants, and
 *     extern declarations for all shared state (defined in calculator_core.c)
 *   - ui_matrix.h replacement: MatrixMenuState_t and inline no-op stubs
 *   - ui_prgm.h replacement: prgm helper inlines and no-op stubs
 *   - graph_ui.h replacement: nav_to and other graph UI stubs
 *
 * Include order in calculator_core.c HOST_TEST block:
 *   app_common.h, app_init.h, calc_engine.h, persist.h, prgm_exec.h,
 *   expr_util.h, ui_palette.h, then this header.
 */

#ifndef CALCULATOR_CORE_TEST_STUBS_H
#define CALCULATOR_CORE_TEST_STUBS_H

/* app_common.h provides CalcMode_t, Token_t, GraphState_t.
 * calc_engine.h provides CalcResult_t, CalcMatrix_t, etc.
 * persist.h provides PersistBlock_t (already has HOST_TEST guards for HAL).
 * All are safe to include in HOST_TEST builds. */
#include "app_common.h"
#include "calc_engine.h"
#include "persist.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * LVGL type stubs
 *---------------------------------------------------------------------------*/

typedef struct lv_obj_s    { int dummy; } lv_obj_t;
typedef struct lv_timer_s  { int dummy; } lv_timer_t;
typedef struct lv_style_s  { int dummy; } lv_style_t;
typedef struct { uint32_t full; }         lv_color_t;
typedef struct lv_font_s   { int dummy; } lv_font_t;
typedef struct { int16_t x, y; }          lv_point_t;

/* LVGL flag constants */
#define LV_OBJ_FLAG_HIDDEN           ((uint32_t)0x01U)
#define LV_OBJ_FLAG_SCROLLABLE       ((uint32_t)0x02U)
#define LV_OBJ_FLAG_OVERFLOW_VISIBLE ((uint32_t)0x04U)
#define LV_OPA_COVER                 0xFFU
#define LV_LABEL_LONG_CLIP           0
#define LV_TEXT_ALIGN_LEFT           0
#define LV_TEXT_ALIGN_RIGHT          1
#define LV_ALIGN_BOTTOM_MID          0
#define LV_HOR_RES                   320
#define LV_VER_RES                   240

/* LVGL colour helper */
static inline lv_color_t lv_color_hex(uint32_t c) { (void)c; lv_color_t r; r.full = 0; return r; }

/* LVGL object / label creators — return NULL (all ptrs checked before use) */
static inline lv_obj_t  *lv_obj_create(lv_obj_t *p)    { (void)p; return NULL; }
static inline lv_obj_t  *lv_label_create(lv_obj_t *p)  { (void)p; return NULL; }
static inline lv_obj_t  *lv_scr_act(void)              { return NULL; }
static inline lv_timer_t *lv_timer_create(void *cb, uint32_t ms, void *ud)
    { (void)cb; (void)ms; (void)ud; return NULL; }

/* LVGL style functions */
#define lv_style_init(s)                              ((void)(s))
#define lv_style_set_bg_color(s, c)                   ((void)(s))
#define lv_style_set_bg_opa(s, v)                     ((void)(s))
#define lv_style_set_border_width(s, v)               ((void)(s))
#define lv_style_set_pad_all(s, v)                    ((void)(s))
#define lv_obj_add_style(o, s, v)                     ((void)(o))

/* LVGL geometry / position */
#define lv_obj_set_size(o, w, h)                      ((void)(o))
#define lv_obj_set_height(o, h)                       ((void)(o))
#define lv_obj_set_width(o, w)                        ((void)(o))
#define lv_obj_set_pos(o, x, y)                       ((void)(o))
#define lv_obj_center(o)                              ((void)(o))
#define lv_obj_align(o, a, x, y)                      ((void)(o))
static inline int32_t lv_obj_get_x(lv_obj_t *o)      { (void)o; return 0; }
static inline int32_t lv_obj_get_y(lv_obj_t *o)      { (void)o; return 0; }

/* LVGL style setters */
#define lv_obj_set_style_bg_color(o, c, v)            ((void)(o))
#define lv_obj_set_style_bg_opa(o, v, s)              ((void)(o))
#define lv_obj_set_style_border_width(o, v, s)        ((void)(o))
#define lv_obj_set_style_pad_all(o, v, s)             ((void)(o))
#define lv_obj_set_style_radius(o, v, s)              ((void)(o))
#define lv_obj_set_style_text_font(o, f, v)           ((void)(o))
#define lv_obj_set_style_text_color(o, c, v)          ((void)(o))
#define lv_obj_set_style_text_align(o, a, v)          ((void)(o))

/* LVGL label helpers */
#define lv_label_set_long_mode(o, m)                  ((void)(o))
#define lv_label_set_text(o, t)                       ((void)(o), (void)(t))
#define lv_label_set_text_fmt(o, ...)                 ((void)(o))
#define lv_label_get_letter_pos(o, p, pt)             ((void)(o))

/* LVGL flag manipulation */
#define lv_obj_add_flag(o, f)                         ((void)(o))
#define lv_obj_clear_flag(o, f)                       ((void)(o))
static inline bool lv_obj_has_flag(lv_obj_t *o, uint32_t f) { (void)o; (void)f; return false; }
#define lv_obj_del(o)                                 ((void)(o))

/* LVGL font query — returns a plausible monospace character width */
static inline uint16_t lv_font_get_glyph_width(const lv_font_t *f, uint32_t c, uint32_t n)
    { (void)f; (void)c; (void)n; return 14u; }

/* LVGL font externs — defined as stub objects in test_normal_mode.c */
extern const lv_font_t jetbrains_mono_24;
extern const lv_font_t jetbrains_mono_20;

/*---------------------------------------------------------------------------
 * FreeRTOS / CMSIS-RTOS stubs
 *---------------------------------------------------------------------------*/

typedef void *SemaphoreHandle_t;
typedef void *osMessageQId;
#define portMAX_DELAY           ((uint32_t)0xFFFFFFFFU)
#define pdPASS                  ((int)1)
#define xSemaphoreTake(m, t)    ((void)(m), (void)(t), pdPASS)
#define xSemaphoreGive(m)       ((void)(m))
#define xQueueSend(q, v, t)     ((void)(q), (void)(v), (void)(t), pdPASS)
#define xQueueReceive(q, v, t)  ((void)(q), (void)(v), (void)(t), 0)
#define osDelay(ms)             ((void)(ms))
#define vTaskDelete(h)          ((void)(h))

/* RTOS handle externs — defined as NULL in test_normal_mode.c */
extern SemaphoreHandle_t xLVGL_Mutex;
extern SemaphoreHandle_t xLVGL_Ready;
extern osMessageQId      keypadQueueHandle;

/*---------------------------------------------------------------------------
 * calc_internal.h replacement
 * Constants and types must match the real calc_internal.h exactly.
 *---------------------------------------------------------------------------*/

#define DISPLAY_W           320
#define DISPLAY_H           240
#define DISP_ROW_COUNT      8
#define DISP_ROW_H          30
#define CURSOR_BLINK_MS     530
#define HISTORY_LINE_COUNT  1
#define MAX_EXPR_LEN        96
#define MAX_RESULT_LEN      96
#define MATRIX_RING_COUNT   1
#define MENU_VISIBLE_ROWS   7

typedef struct {
    char    expression[MAX_EXPR_LEN];
    char    result[MAX_RESULT_LEN];
    bool    has_matrix;
    uint8_t matrix_ring_idx;
    uint8_t matrix_ring_gen;
    uint8_t matrix_rows_cache;
} HistoryEntry_t;

/* Shared state — all defined in calculator_core.c */
extern CalcMode_t  current_mode;
extern CalcMode_t  return_mode;
extern bool        insert_mode;
extern bool        cursor_visible;
extern float       ans;
extern bool        ans_is_matrix;
extern bool        angle_degrees;
extern bool        sto_pending;          /* non-static in HOST_TEST mode */

extern HistoryEntry_t history[HISTORY_LINE_COUNT];
extern uint8_t        history_count;
extern int8_t         history_recall_offset;

extern char    expression[MAX_EXPR_LEN];
extern uint8_t expr_len;
extern uint8_t cursor_pos;

extern GraphState_t graph_state;

/* LVGL screen pointers — ui_mode_screen defined in calculator_core.c;
 * the rest are stub-defined in test_normal_mode.c */
extern lv_obj_t *ui_mode_screen;
extern lv_obj_t *ui_matrix_screen;
extern lv_obj_t *ui_matrix_edit_screen;
extern lv_obj_t *ui_graph_yeq_screen;
extern lv_obj_t *ui_param_yeq_screen;
extern lv_obj_t *ui_graph_range_screen;
extern lv_obj_t *ui_graph_zoom_screen;
extern lv_obj_t *ui_graph_zoom_factors_screen;
extern lv_obj_t *ui_prgm_editor_screen;
extern lv_obj_t *ui_prgm_new_screen;
extern lv_obj_t *ui_stat_screen;
extern lv_obj_t *ui_stat_edit_screen;
extern lv_obj_t *ui_stat_results_screen;
extern lv_obj_t *ui_draw_screen;
extern lv_obj_t *ui_vars_screen;

/* calc_internal.h function declarations (defined in calculator_core.c) */
void Update_Calculator_Display(void);
void ui_update_status_bar(void);
void ui_update_history(void);
void ui_refresh_display(void);
void ui_output_row(uint8_t row_1based, const char *text);
void expr_delete_at_cursor(void);
void format_calc_result(const CalcResult_t *r, char *buf, int buf_size, float *ans_ptr);
void handle_normal_mode(Token_t t);
void lvgl_lock(void);
void lvgl_unlock(void);
void hide_all_screens(void);
void menu_open(Token_t menu_token, CalcMode_t return_to);
CalcMode_t menu_close(Token_t menu_token);
void menu_insert_text(const char *ins, CalcMode_t *ret_mode);
/* cursor_render / cursor_box_create / screen_create / tab_move: LVGL-only,
 * declared in calc_internal.h but not needed by testable sub-handlers */
void cursor_render(lv_obj_t *box, lv_obj_t *inner, lv_obj_t *parent_label,
                   uint32_t glyph_pos, bool visible, CalcMode_t mode, bool insert);
void cursor_box_create(lv_obj_t *parent, bool start_hidden,
                       lv_obj_t **out_box, lv_obj_t **out_inner);
lv_obj_t *screen_create(lv_obj_t *parent);
void tab_move(uint8_t *tab, uint8_t *cursor, uint8_t *scroll,
              uint8_t tab_count, bool left, void (*update)(void));

/*---------------------------------------------------------------------------
 * ui_matrix.h replacement
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    tab;
    uint8_t    item_cursor;
    CalcMode_t return_mode;
} MatrixMenuState_t;

extern MatrixMenuState_t matrix_menu_state;

static inline void ui_init_matrix_screen(void)             {}
static inline void ui_update_matrix_display(void)         {}
static inline void ui_update_matrix_edit_display(void)    {}
static inline void matrix_edit_cursor_update(void)        {}
static inline bool handle_matrix_menu(Token_t t, MatrixMenuState_t *s)
    { (void)t; (void)s; return false; }
static inline void handle_matrix_edit(Token_t t) { (void)t; }

/*---------------------------------------------------------------------------
 * ui_stat.h replacement
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    tab;
    uint8_t    item_cursor;
    CalcMode_t return_mode;
} StatMenuState_t;

extern StatMenuState_t stat_menu_state;

/* stat_data and stat_results are extern in app_common.h; defined in test_normal_mode.c */

static inline void ui_init_stat_screen(void)           {}
static inline void ui_init_stat_edit_screen(void)      {}
static inline void ui_init_stat_results_screen(void)   {}
static inline void ui_update_stat_display(void)        {}
static inline void ui_update_stat_edit_display(void)   {}
static inline void ui_update_stat_results_display(void){}
static inline bool handle_stat_menu(Token_t t, StatMenuState_t *s)
    { (void)t; (void)s; return false; }
static inline bool handle_stat_edit(Token_t t)    { (void)t; return false; }
static inline bool handle_stat_results(Token_t t) { (void)t; return false; }

/*---------------------------------------------------------------------------
 * ui_vars.h replacement
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    tab;
    uint8_t    item_cursor;
    uint8_t    scroll_offset;
    CalcMode_t return_mode;
} VarsMenuState_t;

extern VarsMenuState_t vars_menu_state;

static inline void ui_init_vars_screen(void)        {}
static inline void ui_update_vars_display(void)     {}
static inline bool handle_vars_menu(Token_t t)
    { (void)t; return false; }

/*---------------------------------------------------------------------------
 * ui_draw.h replacement
 *---------------------------------------------------------------------------*/

typedef struct {
    uint8_t    item_cursor;
    CalcMode_t return_mode;
} DrawMenuState_t;

extern DrawMenuState_t draw_menu_state;

static inline void ui_init_draw_screen(void)        {}
static inline void ui_update_draw_display(void)     {}
static inline bool handle_draw_menu(Token_t t)
    { (void)t; return false; }

/*---------------------------------------------------------------------------
 * ui_prgm.h replacement
 *---------------------------------------------------------------------------*/

/* Working buffer — stub-defined in test_normal_mode.c */
extern char    prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];
extern uint8_t prgm_edit_num_lines;

/* prgm_parse_from_store / prgm_slot_id_str / prgm_slot_is_used:
 * identical to the versions in prgm_exec_test_stubs.h (static inline, no conflict) */
static inline void prgm_parse_from_store(uint8_t idx)
{
    const char *body = g_prgm_store.bodies[idx];
    uint8_t n = 0;
    const char *p = body;
    while (*p && n < PRGM_MAX_LINES) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len >= PRGM_MAX_LINE_LEN) len = PRGM_MAX_LINE_LEN - 1;
        memcpy(prgm_edit_lines[n], p, len);
        prgm_edit_lines[n][len] = '\0';
        n++;
        if (!eol) break;
        p = eol + 1;
    }
    prgm_edit_num_lines = n;
}

static inline void prgm_slot_id_str(uint8_t slot, char *out)
{
    if      (slot < 9)  { out[0] = (char)('1' + slot); out[1] = '\0'; }
    else if (slot == 9) { out[0] = '0';                out[1] = '\0'; }
    else if (slot < 36) { out[0] = (char)('A' + slot - 10); out[1] = '\0'; }
    else                { out[0] = 'T';                out[1] = '\0'; }
}

static inline bool prgm_slot_is_used(uint8_t slot)
    { return g_prgm_store.names[slot][0] != '\0'; }

static inline void ui_init_prgm_screens(void)        {}
static inline void hide_prgm_screens(void)          {}
static inline void prgm_menu_open(CalcMode_t m)     { (void)m; current_mode = MODE_PRGM_MENU; }
static inline CalcMode_t prgm_menu_close(void)      { return MODE_NORMAL; }
static inline void prgm_editor_cursor_update(void)  {}
static inline void prgm_new_cursor_update(void)     {}
static inline void prgm_reset_state(CalcMode_t m)   { (void)m; }
static inline bool handle_prgm_menu(Token_t t)      { (void)t; return false; }
static inline bool handle_prgm_new_name(Token_t t)  { (void)t; return false; }
static inline bool handle_prgm_editor(Token_t t)    { (void)t; return false; }
static inline bool handle_prgm_ctl_menu(Token_t t)  { (void)t; return false; }
static inline bool handle_prgm_io_menu(Token_t t)   { (void)t; return false; }
static inline bool handle_prgm_exec_menu(Token_t t) { (void)t; return false; }
static inline bool handle_prgm_running(Token_t t)   { (void)t; return false; }
static inline void prgm_editor_menu_insert(const char *s) { (void)s; }
static inline void ui_prgm_menu_show(const char *title,
    const char texts[][PRGM_MAX_LINE_LEN], uint8_t count,
    uint8_t cursor, uint8_t scroll)
    { (void)title; (void)texts; (void)count; (void)cursor; (void)scroll; }
static inline void ui_prgm_menu_hide(void) {}

/*---------------------------------------------------------------------------
 * graph_ui.h replacement
 *---------------------------------------------------------------------------*/

/* nav_to: sets current_mode so dispatch tests can check mode transitions */
static inline void nav_to(CalcMode_t target) { current_mode = target; }

static inline void yeq_cursor_update(void)              {}
static inline void range_cursor_update(void)            {}
static inline void zoom_factors_cursor_update(void)     {}
static inline void graph_ui_yeq_insert(const char *s)  { (void)s; }
static inline void ui_update_zoom_display(void)         {}
static inline void ui_update_zoom_factors_display(void) {}

/* graph_ui_get/set zoom factors — called from Calc_BuildPersistBlock /
 * Calc_ApplyPersistBlock which are compiled in HOST_TEST mode */
static inline float graph_ui_get_zoom_x_fact(void) { return 2.0f; }
static inline float graph_ui_get_zoom_y_fact(void) { return 2.0f; }
static inline void  graph_ui_set_zoom_facts(float x, float y) { (void)x; (void)y; }

/* init / sync stubs called from StartCalcCoreTask */
static inline void ui_init_graph_screens(void)      {}
static inline void ui_update_range_display(void)    {}
static inline void graph_ui_sync_yeq_labels(void)   {}

/* graph mode handlers — called from Execute_Token (compiled in HOST_TEST) */
static inline bool handle_yeq_mode(Token_t t)          { (void)t; return false; }
static inline bool handle_range_mode(Token_t t)        { (void)t; return false; }
static inline bool handle_zoom_mode(Token_t t)         { (void)t; return false; }
static inline bool handle_zoom_factors_mode(Token_t t) { (void)t; return false; }
static inline bool handle_zbox_mode(Token_t t)         { (void)t; return false; }
static inline bool handle_trace_mode(Token_t t)        { (void)t; return false; }

/*---------------------------------------------------------------------------
 * graph.h replacement
 *---------------------------------------------------------------------------*/

static inline void Graph_SetVisible(bool v)                  { (void)v; }
static inline void Graph_InvalidateCache(void)               {}
static inline void Graph_DrawScatter(const StatData_t *d)    { (void)d; }
static inline void Graph_DrawXYLine(const StatData_t *d)     { (void)d; }
static inline void Graph_DrawHistogram(const StatData_t *d)  { (void)d; }

/* graph_ui.h — parametric handler stub */
static inline bool handle_param_yeq_mode(Token_t t) { (void)t; return false; }

/*---------------------------------------------------------------------------
 * persist.h extras — Persist_Save / Persist_Load defined in persist.c
 * which uses HAL; provide no-op stubs here for the HOST_TEST binary.
 *---------------------------------------------------------------------------*/

/* persist.h guards these out under HOST_TEST — re-declare them here so
 * calculator_core.c can call them; define as real stubs in test_normal_mode.c. */
bool Persist_Save(const PersistBlock_t *in);
bool Persist_Load(PersistBlock_t *out);

/* prgm_exec.h declares Prgm_Save() unconditionally; defined in test_normal_mode.c. */

/* Power_DisplayBlankAndMessage is declared in app_init.h (unconditional) and
 * defined in app_init.c which is not linked in host builds.
 * Defined as a real stub in test_normal_mode.c. */

#endif /* CALCULATOR_CORE_TEST_STUBS_H */
