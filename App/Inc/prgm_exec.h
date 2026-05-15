/**
 * @file    prgm_exec.h
 * @brief   Program storage, persistence, and execution for the TI-81 PRGM system.
 *
 * Provides 37 fixed program slots matching original TI-81 capacity.
 * Slot identifiers: 1–9, 0, A–Z, θ (indices 0–36).
 * A slot is occupied when names[slot][0] != '\0'.
 * Programs are named up to PRGM_NAME_LEN characters (A–Z only) and stored
 * as null-terminated, newline-delimited text bodies in PRGM_BODY_LEN bytes.
 *
 * The store is saved to / loaded from FLASH sector 12 (0x08100000, 128 KB,
 * Bank 2), independently of the calculator variable/graph persist block.
 * All FLASH write routines carry .RamFunc to execute from RAM during AHB stall.
 *
 * FLASH sector map (STM32F429ZIT6):
 *   Sector 10: 0x080C0000 — occupied by firmware (as of 2026-04-28, ~820 KB)
 *   Sector 11: 0x080E0000 — persist block (persist.h / persist.c)
 *   Sector 12: 0x08100000 — program storage (this module, Bank 2)
 */

#ifndef PRGM_EXEC_H
#define PRGM_EXEC_H

#include <stdint.h>
#include <stdbool.h>
#ifndef HOST_TEST
#  include "stm32f4xx_hal.h"
#endif

/*---------------------------------------------------------------------------
 * Limits and layout
 *---------------------------------------------------------------------------*/

#define PRGM_MAX_PROGRAMS   37          /**< Fixed slots: 1–9,0,A–Z,θ (TI-81) */
#define PRGM_NAME_LEN        8          /**< Max program name chars (no null) */
#define PRGM_BODY_LEN      512          /**< Max program body bytes incl null */

/* Editor / execution shared working buffer limits */
#define PRGM_MAX_LINES     64           /**< Max lines in one program */
#define PRGM_MAX_LINE_LEN  48           /**< Max chars per line (incl null) */

/*---------------------------------------------------------------------------
 * FLASH target
 *---------------------------------------------------------------------------*/

#define PRGM_MAGIC        0xCA1C512EU   /**< "calc p12e" — unique from persist */
#define PRGM_VERSION      2U            /**< v2: 37 fixed slots, no count field */
#define PRGM_FLASH_ADDR   0x08100000U   /**< Sector 12 (Bank 2), 128 KB */
#ifndef HOST_TEST
#  define PRGM_SECTOR       FLASH_SECTOR_12
#endif

/*---------------------------------------------------------------------------
 * Data structures
 *---------------------------------------------------------------------------*/

/**
 * @brief In-RAM program store.  Loaded on boot, modified during edit,
 *        flushed to FLASH sector 12 on 2nd+ON.
 *
 * Fixed 37 slots (1–9, 0, A–Z, θ).  A slot is occupied when names[slot][0]
 * is non-zero.  Bodies are stored as raw text with '\n' line separators.
 * Control-flow keywords (If, Disp, etc.) are stored as plain ASCII strings
 * inserted by the CTL / I/O sub-menus in the editor.
 */
typedef struct {
    char    names[PRGM_MAX_PROGRAMS][PRGM_NAME_LEN + 1];   /**< Null-terminated names   */
    char    bodies[PRGM_MAX_PROGRAMS][PRGM_BODY_LEN];      /**< '\n'-delimited line text */
    uint8_t _pad[3];  /**< Word-align: 37*521=19277 → +3 = 19280 B */
} ProgramStore_t;
/* Size: 37*(9+512)+3 = 19280 B */

/**
 * @brief Flat block written verbatim to FLASH sector 11.
 *
 * Layout mirrors PersistBlock_t: magic / version / payload / XOR checksum.
 */
typedef struct {
    uint32_t       magic;
    uint32_t       version;
    ProgramStore_t store;
    uint32_t       checksum;  /**< XOR of all preceding words */
} ProgramFlashBlock_t;
/* Size: 8 + 19280 + 4 = 19292 B — well within 128 KB sector */

_Static_assert(sizeof(ProgramStore_t) % 4 == 0,
               "ProgramStore_t must be a multiple of 4 bytes");

/*---------------------------------------------------------------------------
 * Output callback interface
 *
 * All I/O that the executor produces goes through a PrgmOutput_t instance.
 * Hardware builds register k_hw_output (defined in prgm_exec.c) at Prgm_Init
 * time.  Host-test builds call Prgm_SetOutput() with a buffer-backed struct
 * so tests can assert on actual output without LVGL or RTOS stubs.
 *---------------------------------------------------------------------------*/

/**
 * @brief  Output callbacks injected into the executor.
 *
 * Every function pointer must be non-NULL when registered via Prgm_SetOutput().
 *
 *  disp_text   — Disp "<str>", Disp <expr>, Input prompt: commits one
 *                expression/result pair and refreshes the history display.
 *  prog_done   — End of program (normal or Stop): shows "Done" and refreshes.
 *  clr_home    — ClrHome display refresh (CalcHistory_Clear() is still called
 *                unconditionally by cmd_clrhome; this fires afterward).
 *  disp_graph  — DispGraph: switches to the graph screen and renders.
 *  show_home   — DispHome and program start: switches to the home screen.
 *  input_ready — Input <var>: refreshes the expression-buffer display after
 *                the "?" prompt has been committed.
 *  input_graph — Input (no arg): activates the graph free-moving cursor;
 *                suspends execution until ENTER stores X/Y and resumes
 *                (guidebook p. 8-13).  May be NULL in host-test builds.
 */
typedef struct {
    void (*disp_text)(const char *expr, const char *result);
    void (*prog_done)(void);
    void (*clr_home)(void);
    void (*disp_graph)(void);
    void (*show_home)(void);
    void (*input_ready)(void);
    void (*input_graph)(void);
} PrgmOutput_t;

/**
 * @brief  Register the active output callbacks.
 *
 * Must be called before Prgm_RunStart().  In embedded builds, Prgm_Init()
 * calls this internally with the hardware callback table; host-test builds
 * call it directly with a test-owned struct.
 */
void Prgm_SetOutput(const PrgmOutput_t *out);

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Store accessors — use these instead of touching g_prgm_store directly.
 * g_prgm_store is no longer exported; prgm_exec.c owns it exclusively.
 *---------------------------------------------------------------------------*/

/** @brief Return the null-terminated name for @p slot (may be empty string). */
const char *Prgm_GetName(uint8_t slot);

/** @brief Return the null-terminated newline-delimited body for @p slot. */
const char *Prgm_GetBody(uint8_t slot);

/** @brief Return true when @p slot has a non-empty name (i.e. is occupied). */
bool Prgm_IsSlotOccupied(uint8_t slot);

/**
 * @brief  Copy @p name into @p slot's name field, clamped to PRGM_NAME_LEN
 *         characters and null-terminated.
 */
void Prgm_SetName(uint8_t slot, const char *name);

/**
 * @brief  Append @p line (without a newline) to @p slot's body.
 *         A newline separator is inserted before @p line if the body is
 *         non-empty.  No-op if there is insufficient space in PRGM_BODY_LEN.
 */
void Prgm_AppendLine(uint8_t slot, const char *line);

/**
 * @brief  Replace @p slot's body with @p body, truncated to PRGM_BODY_LEN-1
 *         and always null-terminated.
 */
void Prgm_SetBody(uint8_t slot, const char *body);

/** @brief Zero both the name and body of @p slot. */
void Prgm_ClearSlot(uint8_t slot);

/**
 * @brief  Zero g_prgm_store then attempt to load from FLASH sector 11.
 *         Safe to call before RTOS starts.  Silently keeps defaults on
 *         blank / corrupt sector.
 *
 * HOST_TEST note: guarded out of prgm_exec.c under HOST_TEST; define as a
 * real (non-static) stub in any test .c file that links prgm_exec.c.
 */
void Prgm_Init(void);

/**
 * @brief  Erase sector 11 and write the current g_prgm_store to FLASH.
 * @return true if the magic word was read back successfully.
 *
 * Must be called with LVGL and other tasks stable (same rule as Persist_Save).
 * All write routines run from RAM to avoid AHB stall.
 *
 * HOST_TEST note: guarded out of prgm_exec.c under HOST_TEST; define as a
 * real (non-static) stub in any test .c file that links prgm_exec.c.
 */
bool Prgm_Save(void);

/**
 * @brief  Load g_prgm_store from FLASH sector 11.
 * @return true on success, false if blank or corrupt.
 *
 * HOST_TEST note: guarded out of prgm_exec.c under HOST_TEST; define as a
 * real (non-static) stub in any test .c file that links prgm_exec.c.
 */
bool Prgm_Load(void);

/*---------------------------------------------------------------------------
 * Execution API (defined in prgm_exec.c, called from calculator_core.c and
 * ui_prgm.c)
 *---------------------------------------------------------------------------*/

/**
 * @brief  Initialise executor state and start running program @p idx.
 *         Sets current_mode to MODE_PRGM_RUNNING and enters the run loop.
 */
void Prgm_RunStart(uint8_t idx);

/**
 * @brief  Main synchronous execution loop.
 *         Runs lines from prgm_run_pc until a pause point or end of program.
 *         Re-entered via handle_prgm_running on ENTER after Pause/Input/Prompt.
 */
void Prgm_RunLoop(void);

/**
 * @brief  Reset all executor state variables to their initial (idle) values.
 *         Called on TOKEN_ON (save/power) and hard QUIT to prevent stale state.
 */
void Prgm_ResetExecutionState(void);

/**
 * @brief  Find a program slot by slot-ID string (e.g. "1","A") or user name.
 * @return 0-based slot index, or -1 if not found.
 */
int8_t Prgm_LookupSlot(const char *id);

/**
 * @brief  Abort a running program immediately.
 *         Safe to call from any task (e.g. keypadTask) while Prgm_RunLoop()
 *         is executing on CalcCoreTask.  Sets prgm_run_active = false so the
 *         run loop exits on the next iteration.  No-op if not running.
 */
void Prgm_RequestAbort(void);

/**
 * @brief  Returns true when the executor is paused waiting for user input
 *         (Input / Pause command).  Used by handle_prgm_running() in ui_prgm.c.
 */
bool Prgm_IsWaitingInput(void);

/**
 * @brief  Returns the target variable letter ('A'–'Z') for the pending Input
 *         command, or '\0' for Pause (no target variable).
 */
char Prgm_GetInputVar(void);

/**
 * @brief  Clears the waiting-for-input flag and target variable after the user
 *         has provided input (pressed ENTER in MODE_PRGM_RUNNING).
 */
void Prgm_ClearInputWait(void);

#ifdef HOST_TEST
/**
 * @brief  Validate cmd_table[] prefix ordering.
 *
 * Walks every pair (i, j) where i < j. If entry i is non-exact and its
 * prefix is a prefix of entry j's prefix, i would silently shadow j.
 * Prints a diagnostic to stderr and returns false on any violation.
 * Available only in HOST_TEST builds; call from test_prgm_cmd_table.c.
 */
bool Prgm_CmdTableValidate(void);
#endif /* HOST_TEST */

#endif /* PRGM_EXEC_H */
