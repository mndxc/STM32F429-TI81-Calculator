/**
 * @file    prgm.h
 * @brief   Program storage and persistence for the TI-81 PRGM system.
 *
 * Stores up to PRGM_MAX_PROGRAMS programs in a flat ProgramStore_t block.
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

#ifndef PRGM_H
#define PRGM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/*---------------------------------------------------------------------------
 * Limits and layout
 *---------------------------------------------------------------------------*/

#define PRGM_MAX_PROGRAMS   10          /**< Max concurrent programs         */
#define PRGM_NAME_LEN        8          /**< Max program name chars (no null) */
#define PRGM_BODY_LEN      512          /**< Max program body bytes incl null */

/*---------------------------------------------------------------------------
 * FLASH target
 *---------------------------------------------------------------------------*/

#define PRGM_MAGIC        0xCA1C512EU   /**< "calc p12e" — unique from persist */
#define PRGM_VERSION      1U
#define PRGM_FLASH_ADDR   0x080E0000U   /**< Sector 11, 128 KB */
#define PRGM_SECTOR       FLASH_SECTOR_11

/*---------------------------------------------------------------------------
 * Data structures
 *---------------------------------------------------------------------------*/

/**
 * @brief In-RAM program store.  Loaded on boot, modified during edit,
 *        flushed to FLASH sector 11 on 2nd+ON.
 *
 * Bodies are stored as raw text with '\n' line separators.
 * Control-flow keywords (If, Disp, etc.) are stored as plain ASCII strings
 * inserted by the CTL / I/O sub-menus in the editor.
 */
typedef struct {
    char    names[PRGM_MAX_PROGRAMS][PRGM_NAME_LEN + 1];   /**< Null-terminated names   */
    char    bodies[PRGM_MAX_PROGRAMS][PRGM_BODY_LEN];      /**< '\n'-delimited line text */
    uint8_t count;    /**< Number of programs currently stored (0–PRGM_MAX_PROGRAMS) */
    uint8_t _pad[3];  /**< Word alignment */
} ProgramStore_t;
/* Size: 10*(9+512) + 4 = 5214 B */

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
/* Size: ~5226 B — well within 128 KB sector */

_Static_assert(sizeof(ProgramFlashBlock_t) % 4 == 0,
               "ProgramFlashBlock_t must be a multiple of 4 bytes");

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/**
 * @brief  Global in-RAM program store.  Defined in prgm.c.
 *         Call Prgm_Init() before use.
 */
extern ProgramStore_t g_prgm_store;

/**
 * @brief  Zero g_prgm_store then attempt to load from FLASH sector 11.
 *         Safe to call before RTOS starts.  Silently keeps defaults on
 *         blank / corrupt sector.
 */
void Prgm_Init(void);

/**
 * @brief  Erase sector 11 and write the current g_prgm_store to FLASH.
 * @return true if the magic word was read back successfully.
 *
 * Must be called with LVGL and other tasks stable (same rule as Persist_Save).
 * All write routines run from RAM to avoid AHB stall.
 */
bool Prgm_Save(void);

/**
 * @brief  Load g_prgm_store from FLASH sector 11.
 * @return true on success, false if blank or corrupt.
 */
bool Prgm_Load(void);

#endif /* PRGM_H */
