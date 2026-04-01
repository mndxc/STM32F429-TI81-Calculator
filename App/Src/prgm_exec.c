/**
 * @file    prgm_exec.c
 * @brief   Program storage (FLASH sector 11 erase/write/load) and execution engine
 *          (prgm_run_start, prgm_run_loop, prgm_execute_line).
 *
 * Mirrors persist.c in structure.  All routines that touch FLASH carry
 * __attribute__((section(".RamFunc"))) so they execute from RAM during
 * the AHB bus stall on this single-bank STM32F429.
 *
 * g_prgm_store lives in normal .bss (main RAM).  Prgm_Init() zeros it
 * then loads from FLASH, so no startup-copy dependency exists.
 */

#include "prgm_exec.h"
#ifdef HOST_TEST
#  include "prgm_exec_test_stubs.h"
#else
/*
 * Embedded-only UI dependencies.  In host builds these are replaced by
 * prgm_exec_test_stubs.h so the execution engine remains testable without
 * hardware or LVGL.
 *
 * The calc_internal.h inclusion is a cross-layer dependency: it pulls in the
 * UI super-module's shared state so that handle_normal_mode() can be called
 * after evaluating a program expression line.  Long-term fix: extract a
 * calc_evaluate_and_commit() helper from handle_normal_mode and call that
 * instead — tracked as part of the P22 refactor in CLAUDE.md.
 */
#  include "ui_prgm.h"
#  include "calc_internal.h"
#  include "graph.h"
#endif
#include "calc_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef HOST_TEST
/*---------------------------------------------------------------------------
 * Global store — placed in CCMRAM (64 KB, CPU-only, 0% used elsewhere).
 * At 19280 B the store fits easily; CCMRAM cannot be used for DMA but
 * g_prgm_store is only ever touched by the CPU (CalcCoreTask / Prgm_Save).
 *---------------------------------------------------------------------------*/

ProgramStore_t g_prgm_store __attribute__((section(".ccmram")));

/*---------------------------------------------------------------------------
 * Static write buffer — also in CCMRAM so it does not burden main RAM.
 * The .RamFunc write routine copies from CCMRAM into FLASH word-by-word;
 * CCMRAM is accessible to the CPU even while code runs from .RamFunc SRAM.
 *---------------------------------------------------------------------------*/

static ProgramFlashBlock_t s_prgm_flash_buf __attribute__((section(".ccmram")));

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

static uint32_t prgm_checksum(const ProgramFlashBlock_t *b)
{
    const uint32_t *w = (const uint32_t *)b;
    uint32_t n = sizeof(ProgramFlashBlock_t) / 4 - 1;
    uint32_t cs = 0;
    for (uint32_t i = 0; i < n; i++)
        cs ^= w[i];
    return cs;
}

__attribute__((section(".RamFunc")))
static void prgm_erase_sector(void)
{
    FLASH_EraseInitTypeDef e = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = PRGM_SECTOR,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t sector_error = 0;
    HAL_FLASHEx_Erase(&e, &sector_error);
}

__attribute__((section(".RamFunc")))
static void prgm_write_block(const ProgramFlashBlock_t *block)
{
    const uint32_t *words = (const uint32_t *)block;
    uint32_t n = sizeof(ProgramFlashBlock_t) / 4;
    for (uint32_t i = 0; i < n; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          PRGM_FLASH_ADDR + i * 4,
                          (uint64_t)words[i]);
    }
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

void Prgm_Init(void)
{
    memset(&g_prgm_store, 0, sizeof(g_prgm_store));
    Prgm_Load();
}

__attribute__((section(".RamFunc")))
bool Prgm_Save(void)
{
    /* Build block into the static RAM buffer */
    memcpy(&s_prgm_flash_buf.store, &g_prgm_store, sizeof(g_prgm_store));
    s_prgm_flash_buf.magic    = PRGM_MAGIC;
    s_prgm_flash_buf.version  = PRGM_VERSION;
    s_prgm_flash_buf.checksum = prgm_checksum(&s_prgm_flash_buf);

    HAL_FLASH_Unlock();
    prgm_erase_sector();
    prgm_write_block(&s_prgm_flash_buf);
    HAL_FLASH_Lock();

    const ProgramFlashBlock_t *stored =
        (const ProgramFlashBlock_t *)PRGM_FLASH_ADDR;
    return (stored->magic == PRGM_MAGIC);
}

bool Prgm_Load(void)
{
    const ProgramFlashBlock_t *f =
        (const ProgramFlashBlock_t *)PRGM_FLASH_ADDR;

    if (f->magic   != PRGM_MAGIC)   return false;
    if (f->version != PRGM_VERSION) return false;
    if (prgm_checksum(f) != f->checksum) return false;

    memcpy(&g_prgm_store, &f->store, sizeof(g_prgm_store));
    return true;
}
#endif /* HOST_TEST */

/*---------------------------------------------------------------------------
 * PRGM executor — moved from ui_prgm.c
 *---------------------------------------------------------------------------*/

/* Execution-exclusive state variables */
static CallFrame_t prgm_call_stack[PRGM_CALL_DEPTH];
static uint8_t     prgm_call_top      = 0;
static uint8_t     prgm_run_idx       = 0;   /* program index being executed */
static uint16_t    prgm_run_pc        = 0;   /* current line 0-based */
static uint8_t     prgm_run_num_lines = 0;   /* total lines in running program */
static bool        prgm_run_active    = false;
static bool        prgm_waiting_input = false; /* true when paused at Pause/Input/Prompt */
static char        prgm_input_var     = 0;    /* 'A'–'Z' for Input/Prompt, 0 for Pause */




/*---------------------------------------------------------------------------
 * Slot lookup helper — used by cmd_prgm_call and TOKEN_ENTER in calculator_core.c
 *---------------------------------------------------------------------------*/

/**
 * @brief Find a program slot by slot-ID string (e.g. "1","A") or user name.
 * @return 0-based slot index, or -1 if not found.
 */
int8_t prgm_lookup_slot(const char *id)
{
    if (!id || id[0] == '\0') return -1;
    for (uint8_t i = 0; i < PRGM_MAX_PROGRAMS; i++) {
        /* Match by user name first */
        if (g_prgm_store.names[i][0] != '\0' &&
            strcmp(g_prgm_store.names[i], id) == 0)
            return (int8_t)i;
        /* Match by canonical slot ID string */
        char slot_id[3];
        prgm_slot_id_str(i, slot_id);
        if (strcmp(slot_id, id) == 0)
            return (int8_t)i;
    }
    return -1;
}

/*---------------------------------------------------------------------------
 * Command handler type and dispatch table
 *---------------------------------------------------------------------------*/

typedef void (*CmdHandler_t)(const char *line, uint16_t ln);

typedef struct {
    const char   *prefix;  /* match string */
    uint8_t       len;     /* strlen(prefix) */
    bool          exact;   /* true: strcmp; false: strncmp */
    CmdHandler_t  handler;
} CmdEntry_t;

/*---------------------------------------------------------------------------
 * Individual command handlers — one per PRGM command type.
 * Each receives the full line and the 0-based line index ln.
 * prgm_run_pc is already ln+1 when the handler is called.
 *---------------------------------------------------------------------------*/

static void cmd_lbl(const char *line, uint16_t ln)
{
    (void)line; (void)ln; /* no-op during sequential execution */
}

static void cmd_goto(const char *line, uint16_t ln)
{
    (void)ln;
    char lbl = line[5]; /* single-character label only */
    if (lbl == '\0') { prgm_run_active = false; return; }
    for (uint16_t i = 0; i < (uint16_t)prgm_run_num_lines; i++) {
        if (strncmp(prgm_edit_lines[i], "Lbl ", 4) == 0 &&
            prgm_edit_lines[i][4] == lbl) {
            prgm_run_pc = i + 1;
            return;
        }
    }
    prgm_run_active = false; /* label not found */
}

static void cmd_if(const char *line, uint16_t ln)
{
    (void)ln;
    CalcResult_t r = Calc_Evaluate(line + 3, ans, ans_is_matrix, angle_degrees);
    bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
    if (!cond)
        prgm_run_pc++; /* single-line If: skip one statement */
}




/** Shared arg parser for IS>( and DS<(: extracts variable and threshold string. */
static bool parse_incdec_args(const char *line, int prefix_len,
                               char *var_out, char *val_buf)
{
    const char *args = line + prefix_len;
    if (args[0] < 'A' || args[0] > 'Z' || args[1] != ',') return false;
    *var_out = args[0];
    const char *rest = args + 2;
    int depth = 0, j = 0;
    for (const char *p = rest; *p && j < MAX_EXPR_LEN - 1; p++) {
        if (*p == '(' || *p == '[')      depth++;
        else if (*p == ']') { if (depth > 0) depth--; }
        else if (*p == ')') { if (depth == 0) break; depth--; }
        val_buf[j++] = *p;
    }
    val_buf[j] = '\0';
    return j > 0;
}

static void cmd_is_gt(const char *line, uint16_t ln)
{
    char var, val_buf[MAX_EXPR_LEN];
    if (!parse_incdec_args(line, 4, &var, val_buf)) return;
    CalcResult_t r = Calc_Evaluate(val_buf, ans, ans_is_matrix, angle_degrees);
    if (r.error != CALC_OK || r.has_matrix) return;
    calc_variables[var - 'A'] += 1.0f;
    if (calc_variables[var - 'A'] > r.value)
        prgm_run_pc = ln + 2; /* skip next line */
}

static void cmd_ds_lt(const char *line, uint16_t ln)
{
    char var, val_buf[MAX_EXPR_LEN];
    if (!parse_incdec_args(line, 4, &var, val_buf)) return;
    CalcResult_t r = Calc_Evaluate(val_buf, ans, ans_is_matrix, angle_degrees);
    if (r.error != CALC_OK || r.has_matrix) return;
    calc_variables[var - 'A'] -= 1.0f;
    if (calc_variables[var - 'A'] < r.value)
        prgm_run_pc = ln + 2; /* skip next line */
}


static void cmd_end(const char *line, uint16_t ln)
{
    (void)line; (void)ln; /* no-op: no block structures in simplified CTL spec */
}

static void cmd_pause(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    prgm_waiting_input = true;
    prgm_input_var     = 0;
}

static void cmd_stop(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    prgm_run_active = false;
}


static void cmd_prgm_call(const char *line, uint16_t ln)
{
    (void)ln;
    int8_t idx = prgm_lookup_slot(line + 4);
    if (idx < 0) return; /* program not found — continue */
    if (prgm_call_top < PRGM_CALL_DEPTH) {
        prgm_call_stack[prgm_call_top].idx       = prgm_run_idx;
        prgm_call_stack[prgm_call_top].pc        = prgm_run_pc;
        prgm_call_stack[prgm_call_top].num_lines = prgm_run_num_lines;
        prgm_call_top++;
        prgm_run_idx       = (uint8_t)idx;
        prgm_parse_from_store((uint8_t)idx);
        prgm_run_num_lines = prgm_edit_num_lines;
        prgm_run_pc        = 0;
    }
}

static void cmd_clrhome(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    history_count         = 0;
    history_recall_offset = 0;
#ifndef HOST_TEST
    lvgl_lock(); ui_update_history(); lvgl_unlock();
#endif
}

static void cmd_disphome(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
#endif
}

static void cmd_dispgraph(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    lvgl_lock();
    hide_all_screens();
    Graph_SetVisible(true);
    lvgl_unlock();
    osDelay(20);  /* let DefaultTask flush the show-graph state before rendering */
    Graph_Render(angle_degrees);
#endif
}


static void cmd_disp(const char *line, uint16_t ln)
{
    (void)ln;
    const char *arg = line + 5;
    uint8_t hidx = history_count % HISTORY_LINE_COUNT;
    if (*arg == '"') {
        /* String literal: left-aligned in expression row */
        const char *s   = arg + 1;
        const char *end = strchr(s, '"');
        size_t len = end ? (size_t)(end - s) : strlen(s);
        if (len >= (size_t)(MAX_EXPR_LEN - 1)) len = (size_t)(MAX_EXPR_LEN - 2);
        strncpy(history[hidx].expression, s, len);
        history[hidx].expression[len] = '\0';
        history[hidx].result[0] = '\0';
    } else {
        /* Variable or expression: right-aligned in result row */
        char disp_buf[MAX_RESULT_LEN];
        CalcResult_t r = Calc_Evaluate(arg, ans, ans_is_matrix, angle_degrees);
        format_calc_result(&r, disp_buf, MAX_RESULT_LEN, &ans);
        if (r.error == CALC_OK && !r.has_matrix) {
            ans = r.value; ans_is_matrix = false;
        } else if (r.error == CALC_OK && r.has_matrix) {
            ans = (float)r.matrix_idx; ans_is_matrix = true;
        }
        history[hidx].expression[0] = '\0';
        strncpy(history[hidx].result, disp_buf, MAX_RESULT_LEN - 1);
        history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
    }
    history_count++;
#ifndef HOST_TEST
    lvgl_lock(); ui_update_history(); lvgl_unlock();
#endif
}

static void cmd_input(const char *line, uint16_t ln)
{
    (void)ln;
    const char *arg = line + 6;
    char var = (*arg >= 'A' && *arg <= 'Z') ? *arg : 0;
    prgm_input_var = var;
    /* Original TI-81: always show just "?" — variable name not displayed */
    char prompt[4];
    snprintf(prompt, sizeof(prompt), "?");
    uint8_t hidx = history_count % HISTORY_LINE_COUNT;
    strncpy(history[hidx].expression, prompt, MAX_EXPR_LEN - 1);
    history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
    history[hidx].result[0] = '\0';
    history_count++;
    expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
    prgm_waiting_input = true;
#ifndef HOST_TEST
    lvgl_lock(); ui_update_history(); lvgl_unlock();
    Update_Calculator_Display();
#endif
}


/*---------------------------------------------------------------------------
 * Dispatch table — ordered by frequency for a minor linear-scan benefit.
 * STO (strstr pattern) and general expression (fallback) are handled after
 * the table loop in prgm_execute_line.
 *---------------------------------------------------------------------------*/

static const CmdEntry_t cmd_table[] = {
    { "If ",       3, false, cmd_if        },
    { "Disp ",     5, false, cmd_disp      },
    { "End",       3, true,  cmd_end       },
    { "Goto ",     5, false, cmd_goto      },
    { "Lbl ",      4, false, cmd_lbl       },
    { "IS>(",      4, false, cmd_is_gt     },
    { "DS<(",      4, false, cmd_ds_lt     },
    { "Pause",     5, true,  cmd_pause     },
    { "Stop",      4, true,  cmd_stop      },
    { "prgm",      4, false, cmd_prgm_call },
    { "ClrHome",   7, true,  cmd_clrhome   },
    { "DispHome",  8, true,  cmd_disphome  },
    { "DispGraph", 9, true,  cmd_dispgraph },
    { "Input ",    6, false, cmd_input     },
};

/** Execute the program line at index @p ln. prgm_run_pc is already ln+1. */
static void prgm_execute_line(uint16_t ln)
{
    const char *line = prgm_edit_lines[ln];

    for (size_t i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
        const CmdEntry_t *e = &cmd_table[i];
        bool match = e->exact ? (strcmp(line, e->prefix) == 0)
                              : (strncmp(line, e->prefix, e->len) == 0);
        if (match) {
            e->handler(line, ln);
            return;
        }
    }

    /* STO (expr->VAR) — matched by pattern anywhere in line, not a prefix */
    const char *sto_arrow = strstr(line, "->");
    if (sto_arrow) {
        size_t llen = (size_t)(sto_arrow - line);
        if (llen > 0 && llen < (size_t)(MAX_EXPR_LEN - 1)) {
            char left[MAX_EXPR_LEN];
            strncpy(left, line, llen);
            left[llen] = '\0';
            const char *varname = sto_arrow + 2;
            if (*varname >= 'A' && *varname <= 'Z') {
                CalcResult_t r = Calc_Evaluate(left, ans, ans_is_matrix, angle_degrees);
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[*varname - 'A'] = r.value;
                    ans           = r.value;
                    ans_is_matrix = false;
                }
            }
        }
        return;
    }

    /* General expression line — evaluate and update ANS */
    {
        CalcResult_t r = Calc_Evaluate(line, ans, ans_is_matrix, angle_degrees);
        if (r.error == CALC_OK && !r.has_matrix) {
            ans = r.value;
            ans_is_matrix = false;
        } else if (r.error == CALC_OK && r.has_matrix) {
            ans = (float)r.matrix_idx;
            ans_is_matrix = true;
        }
    }
}

/** Main synchronous execution loop.  Runs lines from prgm_run_pc until a
 *  pause point or end of program.  Re-entered via handle_prgm_running on
 *  ENTER after Pause/Input/Prompt. */
void prgm_request_abort(void)
{
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_call_top      = 0;
}

void prgm_run_loop(void)
{
    prgm_run_active = true;

restart:
    while (prgm_run_pc < (uint16_t)prgm_run_num_lines
           && prgm_run_active && !prgm_waiting_input) {
        uint16_t ln = prgm_run_pc++;
        prgm_execute_line(ln);
#ifndef HOST_TEST
        /* Yield so keypadTask can call prgm_request_abort() on CLEAR, and so
         * DefaultTask can render Disp output.  Without this yield an infinite
         * program loop starves other tasks → black screen + slow heartbeat. */
        osDelay(0);
#endif
    }

    if (prgm_waiting_input)
        return;  /* Pause/Input/Prompt: stay in MODE_PRGM_RUNNING, wait for user */

    if (!prgm_run_active) {
        /* Stop/Return/Goto-abort: program ended before last line */
        current_mode = MODE_NORMAL;
#ifndef HOST_TEST
        {
            uint8_t hidx = history_count % HISTORY_LINE_COUNT;
            history[hidx].expression[0] = '\0';
            strncpy(history[hidx].result, "Done", MAX_RESULT_LEN - 1);
            history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
            history_count++;
        }
        lvgl_lock(); ui_update_history(); lvgl_unlock();
#endif
        return;
    }

    /* End of lines — implicit return from subroutine if call stack not empty */
    if (prgm_call_top > 0) {
        prgm_call_top--;
        prgm_run_idx       = prgm_call_stack[prgm_call_top].idx;
        prgm_run_pc        = prgm_call_stack[prgm_call_top].pc;
        prgm_run_num_lines = prgm_call_stack[prgm_call_top].num_lines;
        prgm_parse_from_store(prgm_run_idx);
        goto restart;
    }

    /* Program done */
    prgm_run_active = false;
    current_mode    = MODE_NORMAL;
#ifndef HOST_TEST
    {
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        history[hidx].expression[0] = '\0';
        strncpy(history[hidx].result, "Done", MAX_RESULT_LEN - 1);
        history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
        history_count++;
    }
    lvgl_lock(); ui_update_history(); lvgl_unlock();
#endif
}

/** Initialise executor state and start running program @p idx. */
void prgm_run_start(uint8_t idx)
{
    prgm_run_idx       = idx;
    prgm_run_pc        = 0;
    prgm_call_top      = 0;
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_input_var     = 0;
    expression[0]      = '\0';
    expr_len           = 0;
    cursor_pos         = 0;
    prgm_parse_from_store(idx);
    prgm_run_num_lines = prgm_edit_num_lines;
    current_mode       = MODE_PRGM_RUNNING;
#ifndef HOST_TEST
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
#endif
    prgm_run_loop();
}

#ifndef HOST_TEST
/**
 * @brief Token handler for MODE_PRGM_RUNNING.
 *
 * Intercepts tokens while a program is executing (waiting at Pause/Input/
 * Prompt).  CLEAR aborts; ENTER resumes; other tokens feed the expression
 * input buffer while waiting for Input/Prompt.
 */
bool handle_prgm_running(Token_t t)
{
    if (prgm_waiting_input) {
        if (t == TOKEN_ENTER) {
            if (prgm_input_var != 0) {
                /* Evaluate and store to the target variable */
                CalcResult_t r = Calc_Evaluate(expression, ans, ans_is_matrix,
                                               angle_degrees);
                char res_buf[MAX_RESULT_LEN];
                format_calc_result(&r, res_buf, MAX_RESULT_LEN, &ans);
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[prgm_input_var - 'A'] = r.value;
                    ans           = r.value;
                    ans_is_matrix = false;
                }
                /* Append expression + result to history */
                uint8_t hidx = history_count % HISTORY_LINE_COUNT;
                strncpy(history[hidx].expression, expression, MAX_EXPR_LEN - 1);
                history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
                strncpy(history[hidx].result, res_buf, MAX_RESULT_LEN - 1);
                history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
                history_count++;
            }
            expression[0]     = '\0';
            expr_len          = 0;
            cursor_pos        = 0;
            prgm_waiting_input = false;
            prgm_input_var    = 0;
            lvgl_lock(); ui_update_history(); lvgl_unlock();
            prgm_run_loop();  /* resume execution */
            return true;
        }
        if (t == TOKEN_DEL) {
            expr_delete_at_cursor();
            Update_Calculator_Display();
            return true;
        }
        if (t == TOKEN_CLEAR) {
            if (expr_len > 0) {
                expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
                Update_Calculator_Display();
            } else {
                /* Abort on CLEAR with empty expression */
                prgm_run_active    = false;
                prgm_waiting_input = false;
                prgm_call_top      = 0;
                current_mode       = MODE_NORMAL;
                lvgl_lock(); ui_refresh_display(); lvgl_unlock();
            }
            return true;
        }
        /* Block keys that would navigate away or change mode */
        switch (t) {
        case TOKEN_GRAPH: case TOKEN_Y_EQUALS: case TOKEN_RANGE:
        case TOKEN_ZOOM:  case TOKEN_TRACE:
        case TOKEN_MATH:  case TOKEN_TEST:    case TOKEN_MATRX:
        case TOKEN_PRGM:  case TOKEN_STO:     case TOKEN_INS:
        case TOKEN_LEFT:  case TOKEN_RIGHT:
        case TOKEN_UP:    case TOKEN_DOWN:
        case TOKEN_2ND:   case TOKEN_ALPHA:   case TOKEN_A_LOCK:
        case TOKEN_QUIT:
            return true; /* consume silently */
        default: {
            /* Route expression tokens through the normal-mode handler.
             * Safe subset: expression-building keys never change current_mode. */
            CalcMode_t saved = current_mode;
            current_mode = MODE_NORMAL;
            handle_normal_mode(t);
            current_mode = saved;
            return true;
        }
        }
    }

    /* Not waiting for input — abort on CLEAR, consume everything else */
    if (t == TOKEN_CLEAR) {
        prgm_run_active    = false;
        prgm_waiting_input = false;
        prgm_call_top      = 0;
        expression[0]      = '\0';
        expr_len           = 0;
        cursor_pos         = 0;
        current_mode       = MODE_NORMAL;
        lvgl_lock(); ui_refresh_display(); lvgl_unlock();
        return true;
    }
    return true; /* consume all tokens while running */
}
#endif /* HOST_TEST */

void prgm_reset_execution_state(void)
{
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_call_top      = 0;
}
