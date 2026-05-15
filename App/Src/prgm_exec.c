/**
 * @file    prgm_exec.c
 * @brief   Program storage (FLASH sector 11 erase/write/load) and execution engine
 *          (Prgm_RunStart, Prgm_RunLoop, prgm_execute_line).
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
#  include "calc_history.h"
#  include "prgm_store_access.h"
#  include "graph.h"
#endif
#include "calc_engine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Output callback table — set once by Prgm_Init (embedded) or
 * Prgm_SetOutput (host tests).  All executor I/O goes through s_out.
 *---------------------------------------------------------------------------*/
static const PrgmOutput_t *s_out = NULL;

void Prgm_SetOutput(const PrgmOutput_t *out) { s_out = out; }

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
 * Store accessor API
 *
 * g_prgm_store is defined in the #ifndef HOST_TEST block above (embedded
 * builds) or in the test translation unit (HOST_TEST builds).  The forward
 * declaration below makes it visible to these accessor functions in both
 * compilation modes without re-exporting it via the public header.
 *---------------------------------------------------------------------------*/

#ifdef HOST_TEST
/* In host-test builds g_prgm_store is defined in test_prgm_exec.c. */
extern ProgramStore_t g_prgm_store;
#endif

const char *Prgm_GetName(uint8_t slot)
{
    return g_prgm_store.names[slot];
}

const char *Prgm_GetBody(uint8_t slot)
{
    return g_prgm_store.bodies[slot];
}

bool Prgm_IsSlotOccupied(uint8_t slot)
{
    return g_prgm_store.names[slot][0] != '\0';
}

void Prgm_SetName(uint8_t slot, const char *name)
{
    strncpy(g_prgm_store.names[slot], name, PRGM_NAME_LEN);
    g_prgm_store.names[slot][PRGM_NAME_LEN] = '\0';
}

void Prgm_AppendLine(uint8_t slot, const char *line)
{
    char   *body     = g_prgm_store.bodies[slot];
    size_t  used     = strlen(body);
    size_t  line_len = strlen(line);
    /* need room for optional newline + line + NUL */
    size_t  need = (used > 0 ? 1u : 0u) + line_len + 1u;
    if (used + need > (size_t)PRGM_BODY_LEN) return;
    if (used > 0) body[used++] = '\n';
    memcpy(body + used, line, line_len);
    body[used + line_len] = '\0';
}

void Prgm_SetBody(uint8_t slot, const char *body)
{
    strncpy(g_prgm_store.bodies[slot], body, PRGM_BODY_LEN - 1);
    g_prgm_store.bodies[slot][PRGM_BODY_LEN - 1] = '\0';
}

void Prgm_ClearSlot(uint8_t slot)
{
    memset(g_prgm_store.names[slot],  0, PRGM_NAME_LEN + 1);
    memset(g_prgm_store.bodies[slot], 0, PRGM_BODY_LEN);
}

/*---------------------------------------------------------------------------
 * PRGM executor — moved from ui_prgm.c
 *---------------------------------------------------------------------------*/

/* Executor-private limits and types */
#define PRGM_CALL_DEPTH  4

typedef struct {
    uint8_t  idx;       /**< caller program index in g_prgm_store */
    uint16_t pc;        /**< return address (line after the prgm call) */
    uint8_t  num_lines; /**< caller's total line count */
} CallFrame_t;

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
int8_t Prgm_LookupSlot(const char *id)
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
 *
 * Structured comment format used by scripts/check_sync.sh:
 *   CMD: <token>  — must match the prefix string in cmd_table[] exactly
 *   Syntax:       — as entered by the user
 *   Effect:       — what the command does at runtime
 *---------------------------------------------------------------------------*/

/* CMD: Lbl
 * Syntax: Lbl <A-Z|0-9>
 * Effect: No-op during sequential execution; marks a jump target for Goto. */
static void cmd_lbl(const char *line, uint16_t ln)
{
    (void)line; (void)ln; /* no-op during sequential execution */
}

/* CMD: Goto
 * Syntax: Goto <A-Z|0-9>
 * Effect: Scans program lines for a matching Lbl; sets PC to that line.
 *         Halts execution if label is not found. */
static void cmd_goto(const char *line, uint16_t ln)
{
    (void)ln;
    char lbl = line[5]; /* single-character label only */
    if (lbl == '\0') { prgm_run_active = false; return; }
    for (uint16_t i = 0; i < (uint16_t)prgm_run_num_lines; i++) {
        if (strncmp(Prgm_GetLine(i), "Lbl ", 4) == 0 &&
            Prgm_GetLine(i)[4] == lbl) {
            prgm_run_pc = i + 1;
            return;
        }
    }
    prgm_run_active = false; /* label not found */
}

/* CMD: If
 * Syntax: If <expr>
 * Effect: Evaluates <expr>; if zero or error, skips the immediately following
 *         line (single-line If — no Then/Else/End block). */
static void cmd_if(const char *line, uint16_t ln)
{
    (void)ln;
    CalcResult_t r = Calc_Evaluate(line + 3, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());
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

/* CMD: IS>(
 * Syntax: IS>(<var>,<expr>)
 * Effect: Increments <var> by 1; if the new value > <expr>, skips the next line. */
static void cmd_is_gt(const char *line, uint16_t ln)
{
    char var, val_buf[MAX_EXPR_LEN];
    if (!parse_incdec_args(line, 4, &var, val_buf)) return;
    CalcResult_t r = Calc_Evaluate(val_buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());
    if (r.error != CALC_OK || r.has_matrix) return;
    calc_variables[var - 'A'] += 1.0f;
    if (calc_variables[var - 'A'] > r.value)
        prgm_run_pc = ln + 2; /* skip next line */
}

/* CMD: DS<(
 * Syntax: DS<(<var>,<expr>)
 * Effect: Decrements <var> by 1; if the new value < <expr>, skips the next line. */
static void cmd_ds_lt(const char *line, uint16_t ln)
{
    char var, val_buf[MAX_EXPR_LEN];
    if (!parse_incdec_args(line, 4, &var, val_buf)) return;
    CalcResult_t r = Calc_Evaluate(val_buf, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                   Calc_GetAngleDegrees());
    if (r.error != CALC_OK || r.has_matrix) return;
    calc_variables[var - 'A'] -= 1.0f;
    if (calc_variables[var - 'A'] < r.value)
        prgm_run_pc = ln + 2; /* skip next line */
}


/* CMD: End
 * Syntax: End
 * Effect: In a subroutine, returns control to the calling program (pops the
 *         call frame).  At the top level, terminates the program (like Stop).
 *         Guidebook p. 8-11. */
static void cmd_end(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    /* Exhaust current program's line counter; Prgm_RunLoop() will then either
     * pop the call stack (subroutine return) or finalize execution (top level). */
    prgm_run_pc = prgm_run_num_lines;
}

/* CMD: Pause
 * Syntax: Pause
 * Effect: Suspends execution; waits for the user to press ENTER before
 *         continuing. Sets prgm_waiting_input with no target variable. */
static void cmd_pause(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    prgm_waiting_input = true;
    prgm_input_var     = 0;
}

/* CMD: Stop
 * Syntax: Stop
 * Effect: Terminates program execution immediately; returns to normal mode. */
static void cmd_stop(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    prgm_run_active = false;
}


/* CMD: prgm
 * Syntax: prgm<NAME>
 * Effect: Pushes current PC/index onto call stack (depth 4) and begins
 *         executing the named program. Returns here on Stop or end-of-program.
 *         Silently continues (no error) if the named program is not found. */
static void cmd_prgm_call(const char *line, uint16_t ln)
{
    (void)ln;
    int8_t idx = Prgm_LookupSlot(line + 4);
    if (idx < 0) return; /* program not found — continue */
    if (prgm_call_top < PRGM_CALL_DEPTH) {
        prgm_call_stack[prgm_call_top].idx       = prgm_run_idx;
        prgm_call_stack[prgm_call_top].pc        = prgm_run_pc;
        prgm_call_stack[prgm_call_top].num_lines = prgm_run_num_lines;
        prgm_call_top++;
        prgm_run_idx       = (uint8_t)idx;
        prgm_parse_from_store((uint8_t)idx);
        prgm_run_num_lines = Prgm_GetNumLines();
        prgm_run_pc        = 0;
    }
}

/* CMD: ClrHome
 * Syntax: ClrHome
 * Effect: Clears the history display; resets history_count and recall offset. */
static void cmd_clrhome(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    CalcHistory_Clear();
    if (s_out) s_out->clr_home();
}

/* CMD: DispHome
 * Syntax: DispHome
 * Effect: Switches the display to the home (calculator) screen. */
static void cmd_disphome(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    if (s_out) s_out->show_home();
}

/* CMD: DispGraph
 * Syntax: DispGraph
 * Effect: Renders the current graph and switches to the graph screen.
 *         Waits 20 ms for DefaultTask to flush before rendering. */
static void cmd_dispgraph(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    if (s_out) s_out->disp_graph();
}


/* CMD: Disp
 * Syntax: Disp "<string>"  |  Disp <expr>  |  Disp <var>
 * Effect: String literal → left-aligned in expression row.
 *         Expression/variable → evaluated and right-aligned in result row;
 *         also updates ANS. Appends one history entry and refreshes display. */
static void cmd_disp(const char *line, uint16_t ln)
{
    (void)ln;
    const char *arg = line + 5;
    if (*arg == '"') {
        /* String literal: left-aligned in expression row */
        const char *s   = arg + 1;
        const char *end = strchr(s, '"');
        size_t len = end ? (size_t)(end - s) : strlen(s);
        if (len >= (size_t)(MAX_EXPR_LEN - 1)) len = (size_t)(MAX_EXPR_LEN - 2);
        char disp_expr[MAX_EXPR_LEN];
        memcpy(disp_expr, s, len);
        disp_expr[len] = '\0';
        if (s_out) s_out->disp_text(disp_expr, "");
    } else {
        /* Variable or expression: right-aligned in result row */
        char disp_buf[MAX_RESULT_LEN];
        CalcResult_t r = Calc_Evaluate(arg, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                       Calc_GetAngleDegrees());
        Calc_FormatResultStr(&r, disp_buf, MAX_RESULT_LEN);
        if (s_out) s_out->disp_text("", disp_buf);
    }
}

/* CMD: Input
 * Syntax: Input         (no argument) or Input <A-Z>
 * Effect (no arg):  Activates the graph free-moving cursor; calc_variables X
 *                   and Y are updated as the cursor moves; execution resumes
 *                   when ENTER is pressed (guidebook p. 8-13).
 * Effect (with var): Displays "?" prompt, clears the expression buffer, and
 *                   suspends execution waiting for a value followed by ENTER.
 *                   The entered value is stored in the specified variable. */
static void cmd_input(const char *line, uint16_t ln)
{
    (void)ln;
    const char *arg = line + 6;
    char var = (*arg >= 'A' && *arg <= 'Z') ? *arg : 0;
    prgm_input_var = var;
    if (var == 0) {
        /* No-argument form: graph-exploration mode */
        prgm_waiting_input = true;
        if (s_out && s_out->input_graph) s_out->input_graph();
        return;
    }
    /* Variable-argument form: text input */
    char prompt[4];
    snprintf(prompt, sizeof(prompt), "?");
    if (s_out) s_out->disp_text(prompt, "");
    Calc_ClearExpr();
    prgm_waiting_input = true;
    if (s_out) s_out->input_ready();
}


/*---------------------------------------------------------------------------
 * PRGM MODE — NUMBER tab handlers
 * Each sets a display/angle mode by calling the same API used by ui_mode.c.
 *---------------------------------------------------------------------------*/

/* CMD: Norm
 * Syntax: Norm
 * Effect: Sets Normal (auto) display notation. */
static void cmd_norm(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    Calc_SetNotationMode(0);
}

/* CMD: Sci
 * Syntax: Sci
 * Effect: Sets Scientific display notation. */
static void cmd_sci(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    Calc_SetNotationMode(1);
}

/* CMD: Eng
 * Syntax: Eng
 * Effect: Sets Engineering display notation. */
static void cmd_eng(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    Calc_SetNotationMode(2);
}

/* CMD: Fix
 * Syntax: Fix <0-9>
 * Effect: Sets Fixed decimal display with the given number of decimal places.
 *         Decimal mode 1 = Fix 0, 2 = Fix 1, …, 10 = Fix 9. */
static void cmd_fix(const char *line, uint16_t ln)
{
    (void)ln;
    const char *arg = line + 4; /* "Fix " is 4 chars */
    if (*arg >= '0' && *arg <= '9')
        Calc_SetDecimalMode((uint8_t)(*arg - '0') + 1u);
}

/* CMD: Float
 * Syntax: Float
 * Effect: Sets Floating (auto) decimal display. */
static void cmd_float(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    Calc_SetDecimalMode(0);
}

/* CMD: Rad
 * Syntax: Rad
 * Effect: Sets Radian angle mode. */
static void cmd_rad(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    Calc_SetAngleDegrees(false);
}

/* CMD: Deg
 * Syntax: Deg
 * Effect: Sets Degree angle mode. */
static void cmd_deg(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
    Calc_SetAngleDegrees(true);
}

/*---------------------------------------------------------------------------
 * PRGM MODE — GRAPH tab handlers
 * Each calls the same Graph_Set* accessor used by ui_mode.c.
 * All Graph_Set* calls are guarded by #ifndef HOST_TEST because graph.h
 * is not available in host-test builds.
 *---------------------------------------------------------------------------*/

/* CMD: Function
 * Syntax: Function
 * Effect: Switches to Function (Y=) graphing mode. */
static void cmd_function(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetParamMode(false);
#endif
}

/* CMD: Param
 * Syntax: Param
 * Effect: Switches to Parametric graphing mode. */
static void cmd_param(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetParamMode(true);
#endif
}

/* CMD: Connected
 * Syntax: Connected
 * Effect: Sets Connected (line segment) graph style. */
static void cmd_connected(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetConnectedMode(true);
#endif
}

/* CMD: Dot
 * Syntax: Dot
 * Effect: Sets Dot (pixel only) graph style. */
static void cmd_dot(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetConnectedMode(false);
#endif
}

/* CMD: Sequence
 * Syntax: Sequence
 * Effect: Sets Sequential plotting (one equation at a time). */
static void cmd_sequence(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetSequentialMode(true);
#endif
}

/* CMD: Simul
 * Syntax: Simul
 * Effect: Sets Simultaneous plotting (all equations advance together). */
static void cmd_simul(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetSequentialMode(false);
#endif
}

/* CMD: Grid Off
 * Syntax: Grid Off
 * Effect: Disables graph grid dots. */
static void cmd_grid_off(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetGridOn(false);
#endif
}

/* CMD: Grid On
 * Syntax: Grid On
 * Effect: Enables graph grid dots. */
static void cmd_grid_on(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetGridOn(true);
#endif
}

/* CMD: Rect
 * Syntax: Rect
 * Effect: Sets Rectangular (X/Y) coordinate display at graph cursor. */
static void cmd_rect(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetPolarDisplay(false);
#endif
}

/* CMD: Polar
 * Syntax: Polar
 * Effect: Sets Polar (R/θ) coordinate display at graph cursor. */
static void cmd_polar(const char *line, uint16_t ln)
{
    (void)line; (void)ln;
#ifndef HOST_TEST
    Graph_SetPolarDisplay(true);
#endif
}

/*---------------------------------------------------------------------------
 * Dispatch table — ordered by frequency for a minor linear-scan benefit.
 * STO (strstr pattern) and general expression (fallback) are handled after
 * the table loop in prgm_execute_line.
 *---------------------------------------------------------------------------*/

static const CmdEntry_t cmd_table[] = {
    { "If ",        3, false, cmd_if        },
    { "Disp ",      5, false, cmd_disp      },
    { "End",        3, true,  cmd_end       },
    { "Goto ",      5, false, cmd_goto      },
    { "Lbl ",       4, false, cmd_lbl       },
    { "IS>(",       4, false, cmd_is_gt     },
    { "DS<(",       4, false, cmd_ds_lt     },
    { "Pause",      5, true,  cmd_pause     },
    { "Stop",       4, true,  cmd_stop      },
    { "prgm",       4, false, cmd_prgm_call },
    { "ClrHome",    7, true,  cmd_clrhome   },
    { "DispHome",   8, true,  cmd_disphome  },
    { "DispGraph",  9, true,  cmd_dispgraph },
    { "Input ",     6, false, cmd_input     },
    /* PRGM MODE — NUMBER tab */
    { "Norm",       4, true,  cmd_norm      },
    { "Sci",        3, true,  cmd_sci       },
    { "Eng",        3, true,  cmd_eng       },
    { "Fix ",       4, false, cmd_fix       },
    { "Float",      5, true,  cmd_float     },
    { "Rad",        3, true,  cmd_rad       },
    { "Deg",        3, true,  cmd_deg       },
    /* PRGM MODE — GRAPH tab */
    { "Function",   8, true,  cmd_function  },
    { "Param",      5, true,  cmd_param     },
    { "Connected",  9, true,  cmd_connected },
    { "Dot",        3, true,  cmd_dot       },
    { "Sequence",   8, true,  cmd_sequence  },
    { "Simul",      5, true,  cmd_simul     },
    { "Grid Off",   8, true,  cmd_grid_off  },
    { "Grid On",    7, true,  cmd_grid_on   },
    { "Rect",       4, true,  cmd_rect      },
    { "Polar",      5, true,  cmd_polar     },
};

/** Execute the program line at index @p ln. prgm_run_pc is already ln+1. */
static void prgm_execute_line(uint16_t ln)
{
    const char *line = Prgm_GetLine(ln);

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
                CalcResult_t r = Calc_Evaluate(left, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                               Calc_GetAngleDegrees());
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[*varname - 'A'] = r.value;
                    Calc_SetAnsScalar(r.value);
                }
            } else if ((unsigned char)varname[0] == 0xCEu &&
                       (unsigned char)varname[1] == 0xB8u) {
                /* STO → θ (U+03B8, UTF-8: CE B8) */
                CalcResult_t r = Calc_Evaluate(left, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                               Calc_GetAngleDegrees());
                if (r.error == CALC_OK && !r.has_matrix) {
                    calc_variables[26] = r.value;
                    Calc_SetAnsScalar(r.value);
                }
            }
        }
        return;
    }

    /* General expression line — evaluate and update ANS */
    {
        CalcResult_t r = Calc_Evaluate(line, Calc_GetAns(), Calc_GetAnsIsMatrix(),
                                       Calc_GetAngleDegrees());
        if (r.error == CALC_OK) {
            if (!r.has_matrix)
                Calc_SetAnsScalar(r.value);
            else
                Calc_SetAnsMatrix((float)r.matrix_idx);
        }
    }
}

/** Main synchronous execution loop.  Runs lines from prgm_run_pc until a
 *  pause point or end of program.  Re-entered via handle_prgm_running on
 *  ENTER after Pause/Input/Prompt. */
void Prgm_RequestAbort(void)
{
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_call_top      = 0;
}

void Prgm_RunLoop(void)
{
    prgm_run_active = true;

restart:
    while (prgm_run_pc < (uint16_t)prgm_run_num_lines
           && prgm_run_active && !prgm_waiting_input) {
        uint16_t ln = prgm_run_pc++;
        prgm_execute_line(ln);
#ifndef HOST_TEST
        /* Yield so keypadTask can call Prgm_RequestAbort() on CLEAR, and so
         * DefaultTask can render Disp output.  Without this yield an infinite
         * program loop starves other tasks → black screen + slow heartbeat. */
        osDelay(0);
#endif
    }

    if (prgm_waiting_input)
        return;  /* Pause/Input/Prompt: stay in MODE_PRGM_RUNNING, wait for user */

    if (!prgm_run_active) {
        /* Stop/Return/Goto-abort: program ended before last line */
        Calc_SetMode(MODE_NORMAL);
        if (s_out) s_out->prog_done();
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
    Calc_SetMode(MODE_NORMAL);
    if (s_out) s_out->prog_done();
}

/** Initialise executor state and start running program @p idx. */
void Prgm_RunStart(uint8_t idx)
{
    prgm_run_idx       = idx;
    prgm_run_pc        = 0;
    prgm_call_top      = 0;
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_input_var     = 0;
    Calc_ClearExpr();
    prgm_parse_from_store(idx);
    prgm_run_num_lines = Prgm_GetNumLines();
    Calc_SetMode(MODE_PRGM_RUNNING);
    if (s_out) s_out->show_home();
    Prgm_RunLoop();
}

/* handle_prgm_running() was moved to ui_prgm.c — declared in ui_prgm.h. */

void Prgm_ResetExecutionState(void)
{
    prgm_run_active    = false;
    prgm_waiting_input = false;
    prgm_input_var     = 0;
    prgm_call_top      = 0;
}

bool Prgm_IsWaitingInput(void) { return prgm_waiting_input; }
char Prgm_GetInputVar(void)    { return prgm_input_var; }
void Prgm_ClearInputWait(void) { prgm_waiting_input = false; prgm_input_var = 0; }

#ifdef HOST_TEST
/**
 * Walk every pair (i, j) where i < j. If entry i is non-exact and its prefix
 * is a prefix of entry j's prefix, entry i would silently shadow j at runtime.
 * Returns true when the table is clean; prints a diagnostic and returns false
 * on the first violation found.
 */
bool Prgm_CmdTableValidate(void)
{
    bool ok = true;
    size_t n = sizeof(cmd_table) / sizeof(cmd_table[0]);
    for (size_t i = 0; i < n; i++) {
        if (cmd_table[i].exact) continue;
        for (size_t j = i + 1; j < n; j++) {
            if (strncmp(cmd_table[j].prefix, cmd_table[i].prefix,
                        cmd_table[i].len) == 0) {
                fprintf(stderr,
                        "FAIL: cmd_table[%zu] \"%s\" (non-exact) shadows "
                        "cmd_table[%zu] \"%s\"\n",
                        i, cmd_table[i].prefix, j, cmd_table[j].prefix);
                ok = false;
            }
        }
    }
    return ok;
}
#endif /* HOST_TEST */
