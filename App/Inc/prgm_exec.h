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
 * The store is saved to / loaded from FLASH sector 11 (0x080E0000, 128 KB)
 * independently of the calculator variable/graph persist block in sector 10.
 * All FLASH write routines carry .RamFunc to execute from RAM during AHB stall.
 *
 * FLASH sector map (STM32F429ZIT6):
 *   Sector 10: 0x080C0000 — calculator variables / graph / matrices
 *   Sector 11: 0x080E0000 — program storage  (this module)
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

/* Executor limits */
#define PRGM_CTRL_DEPTH     8           /**< Max nested If/While/For depth */
#define PRGM_CALL_DEPTH     4           /**< Max subroutine call depth */

/* Editor / execution shared working buffer limits */
#define PRGM_MAX_LINES     64           /**< Max lines in one program */
#define PRGM_MAX_LINE_LEN  48           /**< Max chars per line (incl null) */

/*---------------------------------------------------------------------------
 * Execution control-flow types
 *---------------------------------------------------------------------------*/

typedef enum { CF_IF = 0, CF_WHILE, CF_FOR } CtrlType_t;

typedef struct {
    CtrlType_t  type;
    uint16_t    origin_line;   /**< While: condition line; For: For( line */
    float       for_limit;
    float       for_step;
    char        for_var;
} CtrlFrame_t;

typedef struct {
    uint8_t  idx;        /**< caller program index in g_prgm_store */
    uint16_t pc;         /**< return address (line after the prgm call) */
    uint8_t  num_lines;  /**< caller's total line count */
} CallFrame_t;

/*---------------------------------------------------------------------------
 * FLASH target
 *---------------------------------------------------------------------------*/

#define PRGM_MAGIC        0xCA1C512EU   /**< "calc p12e" — unique from persist */
#define PRGM_VERSION      2U            /**< v2: 37 fixed slots, no count field */
#define PRGM_FLASH_ADDR   0x080E0000U   /**< Sector 11, 128 KB */
#ifndef HOST_TEST
#  define PRGM_SECTOR       FLASH_SECTOR_11
#endif

/*---------------------------------------------------------------------------
 * Data structures
 *---------------------------------------------------------------------------*/

/**
 * @brief In-RAM program store.  Loaded on boot, modified during edit,
 *        flushed to FLASH sector 11 on 2nd+ON.
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
void prgm_run_start(uint8_t idx);

/**
 * @brief  Main synchronous execution loop.
 *         Runs lines from prgm_run_pc until a pause point or end of program.
 *         Re-entered via handle_prgm_running on ENTER after Pause/Input/Prompt.
 */
void prgm_run_loop(void);

/**
 * @brief  Reset all executor state variables to their initial (idle) values.
 *         Called on TOKEN_ON (save/power) and hard QUIT to prevent stale state.
 */
void prgm_reset_execution_state(void);

/**
 * @brief  Find a program slot by slot-ID string (e.g. "1","A") or user name.
 * @return 0-based slot index, or -1 if not found.
 */
int8_t prgm_lookup_slot(const char *id);

/**
 * @brief  Abort a running program immediately.
 *         Safe to call from any task (e.g. keypadTask) while prgm_run_loop()
 *         is executing on CalcCoreTask.  Sets prgm_run_active = false so the
 *         run loop exits on the next iteration.  No-op if not running.
 */
void prgm_request_abort(void);

/**
 * @brief  Returns true when the executor is paused waiting for user input
 *         (Input / Pause command).  Used by handle_prgm_running() in ui_prgm.c.
 */
bool prgm_is_waiting_input(void);

/**
 * @brief  Returns the target variable letter ('A'–'Z') for the pending Input
 *         command, or '\0' for Pause (no target variable).
 */
char prgm_get_input_var(void);

/**
 * @brief  Clears the waiting-for-input flag and target variable after the user
 *         has provided input (pressed ENTER in MODE_PRGM_RUNNING).
 */
void prgm_clear_input_wait(void);

#endif /* PRGM_EXEC_H */
