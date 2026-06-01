/**
 * @file  mode_dispatcher.c
 * @brief Token dispatch table and FreeRTOS calculator core task.
 *
 * Execute_Token() is the single entry point for all calculator token processing.
 * It walks a static routing table (k_route_table[]) and invokes the first
 * matching handler that returns true.
 *
 * Navigation helpers (hide_all_screens, menu_open/close, menu_insert_text,
 * tab_move) live here because they are called only from mode handler modules
 * that already depend on this layer.
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "calc_history.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "expr_util.h"
#  include "ui_palette.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "calculator_core_test_stubs.h"
#  include "expr_editor.h"
#  include "calculator_core.h"
#  include "calc_mode_topology.h"
#else
#  include "app_common.h"
#  include "app_init.h"
#  include "calc_engine.h"
#  include "graph.h"
#  include "graph_draw.h"
#  include "persist.h"
#  include "prgm_exec.h"
#  include "ui_shared.h"
#  include "calc_history.h"
#  include "calculator_core.h"
#  include "ui_mode.h"
#  include "ui_input.h"
#  include "ui_math_menu.h"
#  include "ui_matrix.h"
#  include "ui_prgm.h"
#  include "prgm_editor.h"
#  include "ui_prgm_ctl.h"
#  include "ui_prgm_io.h"
#  include "ui_prgm_exec.h"
#  include "ui_prgm_mode.h"
#  include "ui_stat.h"
#  include "ui_draw.h"
#  include "ui_vars.h"
#  include "ui_yvars.h"
#  include "ui_reset.h"
#  include "ui_error.h"
#  include "graph_ui.h"
#  include "graph_ui_range.h"
#  include "ui_graph_zoom.h"
#  include "ui_palette.h"
#  include "expr_util.h"
#  include "expr_editor.h"
#  include "cmsis_os.h"
#  include "lvgl.h"
#  include "main.h"
#  include "calc_mode_topology.h"
#endif
#include "mode_dispatcher.h"
#include "ui_main_display.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * External references
 *---------------------------------------------------------------------------*/

extern const uint32_t TI81_LookupTable_Size;

#ifdef HOST_TEST
/* Direct access to current_mode / return_mode for calc_mode_topology_validate().
 * These are non-static externs in HOST_TEST builds (defined in calculator_core.c). */
extern CalcMode_t current_mode;
extern CalcMode_t return_mode;
#endif

/*---------------------------------------------------------------------------
 * Navigation helpers
 *---------------------------------------------------------------------------*/

/* Hides every graph editor, menu overlay, and the graph canvas.
 * Must be called inside lvgl_lock(). */
void hide_all_screens(void)
{
    Graph_HideYeqScreen();
    ParamYeq_HideScreen();
    Graph_HideRangeScreen();
    Zoom_HideScreen();
    Graph_HideZoomFactorsScreen();
    Mode_HideScreen();
    Math_HideScreen();
    Test_HideScreen();
    Matrix_HideMenuScreen();
    Matrix_HideEditScreen();
    Stat_HideMenuScreen();
    Stat_HideEditScreen();
    Stat_HideResultsScreen();
    Draw_HideScreen();
    Vars_HideScreen();
    Yvars_HideScreen();
    Reset_HideScreen();
    Error_HideScreen();
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
        math_menu_open(return_to);
        break;
    case TOKEN_TEST:
        test_menu_open(return_to);
        break;
    case TOKEN_MATRX:
        Matrix_MenuOpen(return_to);
        break;
    case TOKEN_PRGM:
        prgm_menu_open(return_to);
        break;
    case TOKEN_STAT:
        Stat_MenuOpen(return_to);
        break;
    case TOKEN_DRAW:
        Draw_MenuOpen(return_to);
        break;
    case TOKEN_VARS:
        Vars_MenuOpen(return_to);
        break;
    case TOKEN_Y_VARS:
        Yvars_MenuOpen(return_to);
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
        ret = math_menu_close();
        break;
    case TOKEN_TEST:
        ret = test_menu_close();
        break;
    case TOKEN_MATRX:
        ret = Matrix_MenuClose();
        break;
    case TOKEN_PRGM:
        ret = prgm_menu_close();
        break;
    case TOKEN_STAT:
        ret = Stat_MenuClose();
        break;
    case TOKEN_DRAW:
        ret = Draw_MenuClose();
        break;
    case TOKEN_VARS:
        ret = Vars_MenuClose();
        break;
    case TOKEN_Y_VARS:
        ret = Yvars_MenuClose();
        break;
    default:
        ret = MODE_NORMAL;
        break;
    }
    Calc_SetMode(ret);
    lvgl_lock();
    Math_HideScreen();
    Test_HideScreen();
    Matrix_HideMenuScreen();
    Stat_HideMenuScreen();
    Stat_HideEditScreen();
    Stat_HideResultsScreen();
    Draw_HideScreen();
    Vars_HideScreen();
    Yvars_HideScreen();
    hide_prgm_screens();
    if (ret == MODE_GRAPH_YEQ)
        Graph_ShowYeqScreen();
    lvgl_unlock();
    return ret;
}

/**
 * @brief Generic cross-module helper that takes a menu item and inserts it
 *        into either the Y= editor or the normal calculator context, then
 *        returns context via pointers.
 */
void menu_insert_text(const char *ins, CalcMode_t *ret_mode)
{
    if (*ret_mode == MODE_PRGM_EDITOR) {
        PrgmEditor_MenuInsert(ins);
    } else if (*ret_mode == MODE_GRAPH_YEQ) {
        Calc_SetMode(MODE_GRAPH_YEQ);
        graph_ui_yeq_insert(ins);
    } else {
        Calc_SetMode(MODE_NORMAL);
        expr_insert_str(ins);
        Update_Calculator_Display();
    }
    *ret_mode = MODE_NORMAL;
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
 * Execute_Token dispatch infrastructure
 *---------------------------------------------------------------------------*/

typedef bool (*ModeHandler_fn)(Token_t);
typedef bool (*ModePredicate_fn)(Token_t);

/**
 * Registers one routing path in Execute_Token's dispatch table.
 * If pred is NULL, the entry fires when current_mode == mode.
 * If pred is non-NULL, pred(t) is called instead of the mode comparison.
 * The final table entry uses pred_always (always returns true) as the fallback.
 */
typedef struct {
    CalcMode_t       mode;    /* primary mode this entry handles */
    ModePredicate_fn pred;    /* if non-NULL, overrides mode comparison */
    ModeHandler_fn   handler;
} ModeRegistration_t;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Thin wrappers for handlers whose signatures differ from ModeHandler_fn. */
static bool dispatch_matrix_menu(Token_t t) { return handle_matrix_menu(t); }
static bool dispatch_matrix_edit(Token_t t) { handle_matrix_edit(t); return true; }
static bool dispatch_stat_menu(Token_t t)   { return handle_stat_menu(t); }

/*---------------------------------------------------------------------------
 * Route handlers — extracted from the former inline blocks in Execute_Token
 *---------------------------------------------------------------------------*/

static bool route_token_on(Token_t t)
{
    (void)t;
    bool power_down = (Calc_GetMode() == MODE_2ND);

    lvgl_lock();
    lv_obj_t *saving_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(saving_lbl, "Saving...");
    lv_obj_set_style_text_color(saving_lbl, lv_color_hex(COLOR_AMBER), 0);
    lv_obj_align(saving_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
    lvgl_unlock();
    osDelay(20);

    PersistBlock_t block = Persist_BuildBlock();
    Persist_Save(&block);
    Prgm_Save();

    Calc_SetMode(MODE_NORMAL);
    Calc_SetReturnMode(MODE_NORMAL);
    ExprEditor_SetStoPending(false);
    Prgm_ResetExecutionState();
    lvgl_lock();
    lv_obj_del(saving_lbl);
    hide_all_screens();
    ui_update_status_bar();
    lvgl_unlock();

    if (power_down) {
        Power_DisplayBlankAndMessage();
    }
    return true;
}

static bool route_token_quit(Token_t t)
{
    (void)t;
    Calc_SetMode(MODE_NORMAL);
    Calc_SetReturnMode(MODE_NORMAL);
    ExprEditor_SetStoPending(false);
    Prgm_ResetExecutionState();
    lvgl_lock();
    hide_all_screens();
    ui_update_status_bar();
    lvgl_unlock();
    return true;
}

static bool route_token_mode(Token_t t)  { (void)t; ui_mode_open(); return true; }

static bool route_token_reset(Token_t t)
{
    (void)t;
    lvgl_lock();
    hide_all_screens();
    lvgl_unlock();
    Reset_MenuOpen(Calc_GetMode());
    return true;
}

/* MODE_PRGM_RUNNING always consumes the token regardless of handler return. */
static bool route_prgm_running(Token_t t) { handle_prgm_running(t); return true; }

/* Normal-mode fallback: always fires last, always consumes the token. */
static bool route_normal_mode(Token_t t)  { handle_normal_mode(t);  return true; }

/*---------------------------------------------------------------------------
 * Route predicates
 *---------------------------------------------------------------------------*/

static bool pred_token_on   (Token_t t) { return t == TOKEN_ON;    }
static bool pred_token_quit (Token_t t) { return t == TOKEN_QUIT;  }
static bool pred_token_mode (Token_t t) { return t == TOKEN_MODE;  }
static bool pred_token_reset(Token_t t) { return t == TOKEN_RESET; }

static bool pred_prgm_new_name(Token_t t) {
    (void)t;
    CalcMode_t cm = Calc_GetMode();
    CalcMode_t rm = Calc_GetReturnMode();
    return cm == MODE_PRGM_NEW_NAME ||
           (cm == MODE_ALPHA_LOCK && rm == MODE_PRGM_NEW_NAME);
}
static bool pred_prgm_editor(Token_t t) {
    (void)t;
    CalcMode_t cm = Calc_GetMode();
    CalcMode_t rm = Calc_GetReturnMode();
    return cm == MODE_PRGM_EDITOR ||
           (cm == MODE_ALPHA_LOCK && rm == MODE_PRGM_EDITOR);
}

static bool pred_graph_mode(Token_t t) {
    (void)t;
    CalcMode_t cm = Calc_GetMode();
    return cm == MODE_GRAPH_YEQ          ||
           cm == MODE_GRAPH_RANGE         ||
           cm == MODE_GRAPH_ZOOM          ||
           cm == MODE_GRAPH_ZOOM_FACTORS  ||
           cm == MODE_GRAPH_ZBOX          ||
           cm == MODE_GRAPH_ZOOM_CURSOR   ||
           cm == MODE_GRAPH_DRAW_CURSOR   ||
           cm == MODE_GRAPH_TRACE         ||
           cm == MODE_GRAPH_FREE_CURSOR   ||
           cm == MODE_GRAPH_PARAM_YEQ;
}

static bool pred_sto_pending(Token_t t) { (void)t; return ExprEditor_GetStoPending(); }
static bool pred_always     (Token_t t) { (void)t; return true; }

/*---------------------------------------------------------------------------
 * Unified routing table — every Execute_Token path in dispatch order.
 * If pred is NULL, the entry fires when current_mode == mode.
 * If pred is non-NULL, pred(t) determines whether the entry fires.
 * First matching entry whose handler returns true stops dispatch.
 * To add a new mode: insert one { .mode = MODE_XXX, .pred = NULL, .handler = yyy } row.
 *---------------------------------------------------------------------------*/
static const ModeRegistration_t k_route_table[] = {
    /* Global token overrides — highest priority regardless of mode ----------*/
    { MODE_NORMAL,             pred_token_on,    route_token_on          },
    { MODE_NORMAL,             pred_token_quit,  route_token_quit        },
    { MODE_NORMAL,             pred_token_mode,  route_token_mode        },
    { MODE_NORMAL,             pred_token_reset, route_token_reset       },
    /* Program execution intercept (before per-mode table) ------------------*/
    { MODE_PRGM_RUNNING,       NULL,             route_prgm_running      },
    /* Graph sub-modes — single entry; Graph_HandleKey in graph.c dispatches. */
    { MODE_NORMAL,             pred_graph_mode,  Graph_HandleKey         },
    { MODE_MODE_SCREEN,        NULL,             handle_mode_screen      },
    { MODE_MATH_MENU,          NULL,             handle_math_menu        },
    { MODE_TEST_MENU,          NULL,             handle_test_menu        },
    { MODE_MATRIX_MENU,        NULL,             dispatch_matrix_menu    },
    { MODE_MATRIX_EDIT,        NULL,             dispatch_matrix_edit    },
    { MODE_STAT_MENU,          NULL,             dispatch_stat_menu      },
    { MODE_STAT_EDIT,          NULL,             handle_stat_edit        },
    { MODE_STAT_RESULTS,       NULL,             handle_stat_results     },
    { MODE_DRAW_MENU,          NULL,             handle_draw_menu        },
    { MODE_VARS_MENU,          NULL,             handle_vars_menu        },
    { MODE_YVARS_MENU,         NULL,             handle_yvars_menu       },
    { MODE_PRGM_MENU,          NULL,             handle_prgm_menu        },
    { MODE_PRGM_CTL_MENU,      NULL,             handle_prgm_ctl_menu    },
    { MODE_PRGM_IO_MENU,       NULL,             handle_prgm_io_menu     },
    { MODE_PRGM_EXEC_MENU,     NULL,             handle_prgm_exec_menu   },
    { MODE_PRGM_MODE_NUMBER,   NULL,             handle_prgm_mode_number },
    { MODE_PRGM_MODE_GRAPH,    NULL,             handle_prgm_mode_graph  },
    { MODE_RESET_CONFIRM,      NULL,             handle_reset_confirm    },
    { MODE_ERROR_SCREEN,       NULL,             handle_error_screen     },
    /* ALPHA_LOCK compound conditions (also fire when ALPHA_LOCK+return_mode) */
    { MODE_PRGM_NEW_NAME,      pred_prgm_new_name, handle_prgm_new_name },
    { MODE_PRGM_EDITOR,        pred_prgm_editor,   handle_prgm_editor   },
    /* STO intercept and normal-mode fallback --------------------------------*/
    { MODE_NORMAL,             pred_sto_pending, handle_sto_pending      },
    { MODE_NORMAL,             pred_always,      route_normal_mode       },
};

/**
 * @brief Processes a single calculator token from the keypad queue.
 * @param t  Token to execute.
 */
void Execute_Token(Token_t t)
{
    for (size_t i = 0; i < ARRAY_SIZE(k_route_table); i++) {
        const ModeRegistration_t *e = &k_route_table[i];
        bool fires = e->pred ? e->pred(t) : (Calc_GetMode() == e->mode);
        if (fires && e->handler(t)) return;
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

    if (Calc_GetMode() == MODE_2ND) {
        token_to_send  = key.second;
        Calc_SetMode(Calc_GetReturnMode());
        Calc_SetReturnMode(MODE_NORMAL);
        if (token_to_send == TOKEN_NONE) {
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
    } else if (Calc_GetMode() == MODE_ALPHA) {
        token_to_send  = key.alpha;
        Calc_SetMode(Calc_GetReturnMode());
        Calc_SetReturnMode(MODE_NORMAL);
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
    } else if (Calc_GetMode() == MODE_ALPHA_LOCK) {
        if (key.normal == TOKEN_ALPHA) {
            Calc_SetMode(Calc_GetReturnMode());
            Calc_SetReturnMode(MODE_NORMAL);
            lvgl_lock();
            ui_update_status_bar();
            lvgl_unlock();
            return;
        }
        token_to_send = key.alpha;
    } else if (ExprEditor_GetStoPending()) {
        token_to_send = key.alpha;
    } else {
        token_to_send  = key.normal;
    }

    if (token_to_send == TOKEN_2ND) {
        if (Calc_GetMode() == MODE_2ND) {
            Calc_SetMode(Calc_GetReturnMode());
            Calc_SetReturnMode(MODE_NORMAL);
        } else {
            Calc_SetReturnMode(Calc_GetMode());
            Calc_SetMode(MODE_2ND);
        }
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_ALPHA) {
        if (Calc_GetMode() == MODE_ALPHA) {
            Calc_SetMode(Calc_GetReturnMode());
            Calc_SetReturnMode(MODE_NORMAL);
        } else {
            Calc_SetReturnMode(Calc_GetMode());
            Calc_SetMode(MODE_ALPHA);
        }
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }
    if (token_to_send == TOKEN_A_LOCK) {
        Calc_SetReturnMode(Calc_GetMode());
        Calc_SetMode(MODE_ALPHA_LOCK);
        lvgl_lock();
        ui_update_status_bar();
        lvgl_unlock();
        return;
    }

    if (token_to_send != TOKEN_NONE) {
        /* F1: on CLEAR, abort any running program immediately from keypadTask so
         * Prgm_RunLoop() (on CalcCoreTask) exits on its next iteration check. */
        if (token_to_send == TOKEN_CLEAR)
            Prgm_RequestAbort();
        if (xQueueSend(keypadQueueHandle, &token_to_send, 0) != pdPASS) {
            /* Queue full — keypress dropped */
        }
    }
}

/*---------------------------------------------------------------------------
 * Stat list accessors — forwarded to calc_engine via Calc_RegisterStatAccessors().
 * Guarded: Stat_GetData() is unavailable in HOST_TEST builds.
 *---------------------------------------------------------------------------*/
#ifndef HOST_TEST
static float calc_stat_get_x(int n)   { return Stat_GetData()->list_x[n - 1]; }
static float calc_stat_get_y(int n)   { return Stat_GetData()->list_y[n - 1]; }
static int   calc_stat_get_len(void)  { return Stat_GetData()->list_len; }
#endif

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
    UiMainDisplay_Init();
    ui_init_graph_screens();
    ui_mode_init();
    ui_init_math_screen();
    ui_init_test_screen();
    ui_init_matrix_screen();
    ui_init_stat_screen();
    ui_init_stat_edit_screen();
    ui_init_stat_results_screen();
    ui_init_draw_screen();
    ui_init_vars_screen();
    ui_init_yvars_screen();
    ui_init_reset_screen();
    ui_init_error_screen();
    ui_init_prgm_screens();
    ui_update_zoom_display();
    ui_update_mode_display();
    ui_update_math_display();
    ui_update_matrix_display();
    ui_update_matrix_edit_display();
    ui_update_stat_display();
    ui_update_vars_display();
    ui_update_yvars_display();
    Calc_RegisterYEquations(
        Graph_GetState()->equations,
        GRAPH_NUM_EQ);
#ifndef HOST_TEST
    Calc_RegisterStatAccessors(calc_stat_get_x, calc_stat_get_y, calc_stat_get_len);
#endif
    Prgm_Init();

    ui_refresh_display();

    {
        PersistBlock_t saved;
        if (Persist_Load(&saved)) {
            Persist_ApplyBlock(&saved);
            ui_refresh_display();
            graph_ui_sync_yeq_labels();
            ui_update_mode_display();
            ui_update_range_display();
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

#ifdef HOST_TEST
/*---------------------------------------------------------------------------
 * Routing topology validator — HOST_TEST only
 *---------------------------------------------------------------------------*/

/**
 * @brief Validate that every CalcMode_t value is covered by the routing table.
 *
 * Each mode in [0, MODE_COUNT) must appear in exactly one of:
 *   (a) k_route_table[] — a non-fallback pred fires when current_mode == mode
 *       (uses TOKEN_ENTER as a neutral token; sto_pending and return_mode at
 *        their default values so only mode-based predicates can fire), or
 *   (b) known_special_cases[] — modes intentionally handled by the fallback.
 *
 * The last table entry (pred_always → route_normal_mode) is the fallback and is
 * excluded from the "dedicated entry" check.
 *
 * Adding a 32nd mode without updating either the table or known_special_cases
 * causes this function to return false and print a diagnostic.
 */
bool calc_mode_topology_validate(void)
{
    static const CalcMode_t known_special_cases[] = {
        MODE_NORMAL,      /* base mode: no dedicated entry; falls to handle_normal_mode */
        MODE_2ND,         /* overlay: no dedicated entry; falls to handle_normal_mode */
        MODE_ALPHA,       /* overlay: no dedicated entry; falls to handle_normal_mode */
        MODE_ALPHA_LOCK,  /* compound preds (pred_prgm_new_name/pred_prgm_editor) cover
                           * the ALPHA_LOCK+return_mode sub-cases; base case falls through */
        MODE_STO,         /* synthetic: never stored in current_mode */
    };
    static const size_t n_special =
        sizeof(known_special_cases) / sizeof(known_special_cases[0]);

    const Token_t neutral = TOKEN_ENTER;

    CalcMode_t saved_mode = current_mode;
    CalcMode_t saved_ret  = return_mode;
    bool       saved_sto  = ExprEditor_GetStoPending();
    return_mode = MODE_NORMAL;
    ExprEditor_SetStoPending(false);

    bool ok = true;

    for (int m = 0; m < MODE_COUNT; m++) {
        CalcMode_t mode = (CalcMode_t)m;
        current_mode = mode;  /* direct assignment: iterates through synthetic/sentinel values; bypasses Calc_SetMode() intentionally */

        bool has_dedicated = false;
        for (size_t i = 0; i < ARRAY_SIZE(k_route_table) - 1; i++) {
            const ModeRegistration_t *e = &k_route_table[i];
            bool fires = e->pred ? e->pred(neutral) : (current_mode == e->mode);
            if (fires) {
                has_dedicated = true;
                break;
            }
        }

        bool is_special = false;
        for (size_t j = 0; j < n_special; j++) {
            if (known_special_cases[j] == mode) {
                is_special = true;
                break;
            }
        }

        if (has_dedicated && is_special) {
            printf("  FAIL mode %d: in both route table and known_special_cases\n", m);
            ok = false;
        } else if (!has_dedicated && !is_special) {
            printf("  FAIL mode %d: no routing entry and not in known_special_cases\n", m);
            ok = false;
        }
    }

    for (size_t j = 0; j < n_special; j++) {
        if ((int)known_special_cases[j] >= MODE_COUNT) {
            printf("  FAIL known_special_cases[%zu] = %d is out of range [0, MODE_COUNT)\n",
                   j, (int)known_special_cases[j]);
            ok = false;
        }
    }

    current_mode = saved_mode;
    return_mode  = saved_ret;
    ExprEditor_SetStoPending(saved_sto);
    return ok;
}
#endif /* HOST_TEST */
