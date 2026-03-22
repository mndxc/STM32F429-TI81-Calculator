/**
 * @file    prgm_exec.c
 * @brief   Program storage — FLASH sector 11 erase/write and load/save.
 *
 * Mirrors persist.c in structure.  All routines that touch FLASH carry
 * __attribute__((section(".RamFunc"))) so they execute from RAM during
 * the AHB bus stall on this single-bank STM32F429.
 *
 * g_prgm_store lives in normal .bss (main RAM).  Prgm_Init() zeros it
 * then loads from FLASH, so no startup-copy dependency exists.
 */

#include "prgm_exec.h"
#include "ui_prgm.h"
#include "calc_internal.h"
#include "calc_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

/*---------------------------------------------------------------------------
 * PRGM executor — moved from ui_prgm.c
 *---------------------------------------------------------------------------*/

/* Execution-exclusive state variables */
static CtrlFrame_t prgm_ctrl_stack[PRGM_CTRL_DEPTH];
static uint8_t     prgm_ctrl_top      = 0;
static CallFrame_t prgm_call_stack[PRGM_CALL_DEPTH];
static uint8_t     prgm_call_top      = 0;
static uint8_t     prgm_run_idx       = 0;   /* program index being executed */
static uint16_t    prgm_run_pc        = 0;   /* current line 0-based */
static uint8_t     prgm_run_num_lines = 0;   /* total lines in running program */
static bool        prgm_run_active    = false;
static bool        prgm_waiting_input = false; /* true when paused at Pause/Input/Prompt */
static char        prgm_input_var     = 0;    /* 'A'–'Z' for Input/Prompt, 0 for Pause */

/**
 * @brief Skip forward to the line after the matching Else (if want_else) or End.
 *
 * Call with prgm_run_pc pointing to the first line to scan (not already past
 * it).  On return prgm_run_pc is the first line to execute after the target.
 * Counts nested block-openers so that nested If/While/For structures are
 * handled correctly.
 */
static void prgm_skip_to_target(bool want_else)
{
    int depth = 0;
    while (prgm_run_pc < (uint16_t)prgm_run_num_lines) {
        const char *ln = prgm_edit_lines[prgm_run_pc];
        if (strncmp(ln, "If ", 3) == 0) {
            /* Only a block-If (next line is Then) increments depth */
            uint16_t nxt = prgm_run_pc + 1;
            if (nxt < (uint16_t)prgm_run_num_lines &&
                strncmp(prgm_edit_lines[nxt], "Then", 4) == 0)
                depth++;
        } else if (strncmp(ln, "While ", 6) == 0 || strncmp(ln, "For(", 4) == 0) {
            depth++;
        } else if (strcmp(ln, "Else") == 0 && depth == 0 && want_else) {
            prgm_run_pc++; /* execute from line after Else */
            return;
        } else if (strcmp(ln, "End") == 0) {
            if (depth == 0) {
                prgm_run_pc++; /* continue from line after End */
                return;
            }
            depth--;
        }
        prgm_run_pc++;
    }
    /* No matching End found — fall off end of program */
}

/** Execute the program line at index @p ln. prgm_run_pc is already ln+1. */
static void prgm_execute_line(uint16_t ln)
{
    const char *line = prgm_edit_lines[ln];

    /* --- Lbl (no-op during sequential execution) ------------------------- */
    if (strncmp(line, "Lbl ", 4) == 0) return;

    /* --- Goto ------------------------------------------------------------- */
    if (strncmp(line, "Goto ", 5) == 0) {
        const char *lbl = line + 5;
        for (uint16_t i = 0; i < (uint16_t)prgm_run_num_lines; i++) {
            if (strncmp(prgm_edit_lines[i], "Lbl ", 4) == 0 &&
                strcmp(prgm_edit_lines[i] + 4, lbl) == 0) {
                prgm_run_pc = i + 1; /* execute line after Lbl */
                return;
            }
        }
        prgm_run_active = false; /* label not found */
        return;
    }

    /* --- If --------------------------------------------------------------- */
    if (strncmp(line, "If ", 3) == 0) {
        CalcResult_t r = Calc_Evaluate(line + 3, ans, ans_is_matrix, angle_degrees);
        bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
        if (!cond) {
            if (prgm_run_pc < (uint16_t)prgm_run_num_lines &&
                strncmp(prgm_edit_lines[prgm_run_pc], "Then", 4) == 0) {
                /* Block If: skip past Then, find Else or End */
                prgm_run_pc++;            /* skip Then */
                prgm_skip_to_target(true);
            } else {
                prgm_run_pc++;            /* single-line If: skip one statement */
            }
        }
        return;
    }

    /* --- Then ------------------------------------------------------------- */
    if (strcmp(line, "Then") == 0) {
        /* Push CF_IF so End knows it belongs to an If block */
        if (prgm_ctrl_top < PRGM_CTRL_DEPTH) {
            prgm_ctrl_stack[prgm_ctrl_top].type        = CF_IF;
            prgm_ctrl_stack[prgm_ctrl_top].origin_line = ln;
            prgm_ctrl_top++;
        }
        return;
    }

    /* --- Else ------------------------------------------------------------- */
    if (strcmp(line, "Else") == 0) {
        /* We are in the true block — pop CF_IF and skip to matching End */
        if (prgm_ctrl_top > 0 && prgm_ctrl_stack[prgm_ctrl_top - 1].type == CF_IF)
            prgm_ctrl_top--;
        /* prgm_run_pc == ln+1: scan from next line to End (no Else wanted) */
        prgm_skip_to_target(false);
        return;
    }

    /* --- While ------------------------------------------------------------ */
    if (strncmp(line, "While ", 6) == 0) {
        CalcResult_t r = Calc_Evaluate(line + 6, ans, ans_is_matrix, angle_degrees);
        bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
        if (cond) {
            if (prgm_ctrl_top < PRGM_CTRL_DEPTH) {
                prgm_ctrl_stack[prgm_ctrl_top].type        = CF_WHILE;
                prgm_ctrl_stack[prgm_ctrl_top].origin_line = ln;
                prgm_ctrl_top++;
            }
        } else {
            prgm_skip_to_target(false);
        }
        return;
    }

    /* --- For( ------------------------------------------------------------- */
    if (strncmp(line, "For(", 4) == 0) {
        const char *args = line + 4;
        /* Variable must be a single letter followed by comma */
        if (args[0] < 'A' || args[0] > 'Z' || args[1] != ',') return;
        char var = args[0];
        const char *rest = args + 2;

        /* Split rest at depth-0 commas → up to 3 args (begin, end, step) */
        char parts[3][MAX_EXPR_LEN];
        int n = 0, depth = 0, j = 0;
        for (const char *p = rest; *p && n < 3; p++) {
            if (*p == '(' || *p == '[')       depth++;
            else if (*p == ')') { if (depth == 0) break; depth--; }
            else if (*p == ']') { if (depth > 0)  depth--; }
            else if (*p == ',' && depth == 0) {
                parts[n][j] = '\0'; n++; j = 0; continue;
            }
            if (j < MAX_EXPR_LEN - 1) parts[n][j++] = *p;
        }
        if (j > 0) { parts[n][j] = '\0'; n++; }
        if (n < 2) return;

        CalcResult_t rb = Calc_Evaluate(parts[0], ans, ans_is_matrix, angle_degrees);
        CalcResult_t re = Calc_Evaluate(parts[1], ans, ans_is_matrix, angle_degrees);
        if (rb.error != CALC_OK || rb.has_matrix) return;
        if (re.error != CALC_OK || re.has_matrix) return;
        float begin_v = rb.value, end_v = re.value, step_v = 1.0f;
        if (n >= 3) {
            CalcResult_t rs = Calc_Evaluate(parts[2], ans, ans_is_matrix, angle_degrees);
            if (rs.error == CALC_OK && !rs.has_matrix) step_v = rs.value;
        }
        if (step_v == 0.0f) return; /* infinite loop guard */

        calc_variables[var - 'A'] = begin_v;
        bool run = (step_v > 0.0f) ? (begin_v <= end_v) : (begin_v >= end_v);
        if (run) {
            if (prgm_ctrl_top < PRGM_CTRL_DEPTH) {
                prgm_ctrl_stack[prgm_ctrl_top].type        = CF_FOR;
                prgm_ctrl_stack[prgm_ctrl_top].origin_line = ln;
                prgm_ctrl_stack[prgm_ctrl_top].for_limit   = end_v;
                prgm_ctrl_stack[prgm_ctrl_top].for_step    = step_v;
                prgm_ctrl_stack[prgm_ctrl_top].for_var     = var;
                prgm_ctrl_top++;
            }
        } else {
            prgm_skip_to_target(false);
        }
        return;
    }

    /* --- End -------------------------------------------------------------- */
    if (strcmp(line, "End") == 0) {
        if (prgm_ctrl_top == 0) return; /* unmatched End */
        CtrlFrame_t *frame = &prgm_ctrl_stack[prgm_ctrl_top - 1];
        if (frame->type == CF_IF) {
            prgm_ctrl_top--;
        } else if (frame->type == CF_WHILE) {
            /* Re-evaluate While condition */
            const char *wcond = prgm_edit_lines[frame->origin_line] + 6;
            CalcResult_t r = Calc_Evaluate(wcond, ans, ans_is_matrix, angle_degrees);
            bool cond = (r.error == CALC_OK && !r.has_matrix && r.value != 0.0f);
            if (cond) {
                prgm_run_pc = frame->origin_line + 1; /* re-execute body */
            } else {
                prgm_ctrl_top--;
            }
        } else if (frame->type == CF_FOR) {
            char fvar = frame->for_var;
            calc_variables[fvar - 'A'] += frame->for_step;
            float val = calc_variables[fvar - 'A'];
            bool still = (frame->for_step > 0.0f) ? (val <= frame->for_limit)
                                                   : (val >= frame->for_limit);
            if (still) {
                prgm_run_pc = frame->origin_line + 1; /* first body line */
            } else {
                prgm_ctrl_top--;
            }
        }
        return;
    }

    /* --- Pause ------------------------------------------------------------ */
    if (strcmp(line, "Pause") == 0) {
        prgm_waiting_input = true;
        prgm_input_var     = 0;
        return;
    }

    /* --- Stop ------------------------------------------------------------- */
    if (strcmp(line, "Stop") == 0) {
        prgm_run_active = false;
        return;
    }

    /* --- Return ----------------------------------------------------------- */
    if (strcmp(line, "Return") == 0) {
        if (prgm_call_top > 0) {
            prgm_call_top--;
            prgm_run_idx       = prgm_call_stack[prgm_call_top].idx;
            prgm_run_pc        = prgm_call_stack[prgm_call_top].pc;
            prgm_run_num_lines = prgm_call_stack[prgm_call_top].num_lines;
            prgm_parse_from_store(prgm_run_idx);
        } else {
            prgm_run_active = false;
        }
        return;
    }

    /* --- prgm (subroutine call) ------------------------------------------ */
    if (strncmp(line, "prgm", 4) == 0) {
        /* Look up by slot identifier (e.g. "prgmA" → slot 10) */
        const char *id_str = line + 4;
        for (uint8_t i = 0; i < PRGM_MAX_PROGRAMS; i++) {
            if (!prgm_slot_is_used(i)) continue;
            char slot_id[3];
            prgm_slot_id_str(i, slot_id);
            if (strcmp(slot_id, id_str) == 0) {
                if (prgm_call_top < PRGM_CALL_DEPTH) {
                    prgm_call_stack[prgm_call_top].idx       = prgm_run_idx;
                    prgm_call_stack[prgm_call_top].pc        = prgm_run_pc;
                    prgm_call_stack[prgm_call_top].num_lines = prgm_run_num_lines;
                    prgm_call_top++;
                    prgm_run_idx       = i;
                    prgm_parse_from_store(i);
                    prgm_run_num_lines = prgm_edit_num_lines;
                    prgm_run_pc        = 0;
                }
                return;
            }
        }
        return; /* program not found — continue */
    }

    /* --- ClrHome ---------------------------------------------------------- */
    if (strcmp(line, "ClrHome") == 0) {
        history_count         = 0;
        history_recall_offset = 0;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        return;
    }

    /* --- Disp ------------------------------------------------------------- */
    if (strncmp(line, "Disp ", 5) == 0) {
        const char *arg = line + 5;
        char disp_buf[MAX_RESULT_LEN];
        if (*arg == '"') {
            /* String literal */
            const char *s   = arg + 1;
            const char *end = strchr(s, '"');
            size_t len = end ? (size_t)(end - s) : strlen(s);
            if (len >= (size_t)(MAX_RESULT_LEN - 1)) len = (size_t)(MAX_RESULT_LEN - 2);
            strncpy(disp_buf, s, len);
            disp_buf[len] = '\0';
        } else {
            CalcResult_t r = Calc_Evaluate(arg, ans, ans_is_matrix, angle_degrees);
            format_calc_result(&r, disp_buf, MAX_RESULT_LEN, &ans);
            if (r.error == CALC_OK && !r.has_matrix) {
                ans = r.value; ans_is_matrix = false;
            } else if (r.error == CALC_OK && r.has_matrix) {
                ans = (float)r.matrix_idx; ans_is_matrix = true;
            }
        }
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        history[hidx].expression[0] = '\0';
        strncpy(history[hidx].result, disp_buf, MAX_RESULT_LEN - 1);
        history[hidx].result[MAX_RESULT_LEN - 1] = '\0';
        history_count++;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        return;
    }

    /* --- Input ------------------------------------------------------------ */
    if (strncmp(line, "Input ", 6) == 0) {
        const char *arg = line + 6;
        char var = (*arg >= 'A' && *arg <= 'Z') ? *arg : 0;
        prgm_input_var = var;
        char prompt[12];
        snprintf(prompt, sizeof(prompt), var ? "%c=?" : "?", var);
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, prompt, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        history[hidx].result[0] = '\0';
        history_count++;
        expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
        prgm_waiting_input = true;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        Update_Calculator_Display();
        return;
    }

    /* --- Prompt ----------------------------------------------------------- */
    if (strncmp(line, "Prompt ", 7) == 0) {
        const char *arg = line + 7;
        char var = (*arg >= 'A' && *arg <= 'Z') ? *arg : 0;
        prgm_input_var = var;
        char prompt[12];
        snprintf(prompt, sizeof(prompt), var ? "%c=?" : "?", var);
        uint8_t hidx = history_count % HISTORY_LINE_COUNT;
        strncpy(history[hidx].expression, prompt, MAX_EXPR_LEN - 1);
        history[hidx].expression[MAX_EXPR_LEN - 1] = '\0';
        history[hidx].result[0] = '\0';
        history_count++;
        expression[0] = '\0'; expr_len = 0; cursor_pos = 0;
        prgm_waiting_input = true;
        lvgl_lock(); ui_update_history(); lvgl_unlock();
        Update_Calculator_Display();
        return;
    }

    /* --- STO (expr->VAR) -------------------------------------------------- */
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
                    ans = r.value;
                    ans_is_matrix = false;
                }
            }
        }
        return;
    }

    /* --- General expression line ----------------------------------------- */
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
void prgm_run_loop(void)
{
    prgm_run_active = true;

restart:
    while (prgm_run_pc < (uint16_t)prgm_run_num_lines
           && prgm_run_active && !prgm_waiting_input) {
        uint16_t ln = prgm_run_pc++;
        prgm_execute_line(ln);
    }

    if (!prgm_run_active || prgm_waiting_input)
        return;

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
    lvgl_lock();
    ui_refresh_display();
    lvgl_unlock();
}

/** Initialise executor state and start running program @p idx. */
void prgm_run_start(uint8_t idx)
{
    prgm_run_idx       = idx;
    prgm_run_pc        = 0;
    prgm_ctrl_top      = 0;
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
    lvgl_lock();
    hide_all_screens();
    ui_refresh_display();
    lvgl_unlock();
    prgm_run_loop();
}

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
                prgm_ctrl_top      = 0;
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
        prgm_ctrl_top      = 0;
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

void prgm_reset_execution_state(void)
{
    prgm_run_active = false;
    prgm_waiting_input = false;
    prgm_ctrl_top = 0;
    prgm_call_top = 0;
}
