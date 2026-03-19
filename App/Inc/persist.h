/**
 * @file    persist.h
 * @brief   Persistent FLASH storage for calculator state.
 *
 * Saves/loads variables A-Z, ANS, MODE settings, graph equations, RANGE
 * parameters, and ZOOM factors to STM32F429 FLASH sector 10 (0x080C0000,
 * 128 KB). State survives power-off. Save is triggered by 2nd+ON.
 *
 * FLASH mechanics live in persist.c; state packaging lives in
 * calculator_core.c via Calc_BuildPersistBlock / Calc_ApplyPersistBlock.
 */

#ifndef PERSIST_H
#define PERSIST_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/*---------------------------------------------------------------------------
 * Flash target
 *---------------------------------------------------------------------------*/

#define PERSIST_MAGIC       0xCA1CC0DEU   /* Marker — "calc code" */
#define PERSIST_VERSION     1U
#define PERSIST_FLASH_ADDR  0x080C0000U   /* Sector 10, 128 KB, unused by firmware */
#define PERSIST_SECTOR      FLASH_SECTOR_10
/* STM32F429 sector map (12 sectors per bank):
 *   Sectors 0-3:  16 KB each  (0x08000000 - 0x0800FFFF)
 *   Sector  4:    64 KB       (0x08010000 - 0x0801FFFF)
 *   Sectors 5-11: 128 KB each (0x08020000 - 0x080FFFFF)
 * Firmware (~684 KB) ends at ~0x080AAFFF; first fully-free sector is Sector 10. */

/*---------------------------------------------------------------------------
 * Saved state block
 *---------------------------------------------------------------------------*/

/**
 * @brief Flat, word-aligned struct written verbatim to FLASH sector 10.
 *
 * Fields:
 *   magic / version  — validated on load; stale or blank sector returns false
 *   calc_variables   — user variables A–Z (extern in calc_engine.h)
 *   ans              — last result
 *   mode_committed   — 8-byte MODE screen committed selections
 *   zoom_x_fact      — ZOOM FACTORS XFact
 *   zoom_y_fact      — ZOOM FACTORS YFact
 *   equations        — Y1–Y4 equation strings
 *   x_min … x_res    — RANGE parameters
 *   grid_on          — grid display toggle (MODE row 7)
 *   _pad             — alignment padding (do not use)
 *   checksum         — 32-bit XOR of all preceding words; computed last
 *
 * graph_state.active is NOT saved — always boot with graph hidden.
 * calc_decimal_mode is derived from mode_committed[1] via Calc_SetDecimalMode().
 */
typedef struct {
    uint32_t magic;               /*   4 B */
    uint32_t version;             /*   4 B */
    float    calc_variables[26];  /* 104 B */
    float    ans;                 /*   4 B */
    uint8_t  mode_committed[8];   /*   8 B */
    float    zoom_x_fact;         /*   4 B */
    float    zoom_y_fact;         /*   4 B */
    char     equations[4][64];    /* 256 B */
    float    x_min;               /*   4 B */
    float    x_max;               /*   4 B */
    float    y_min;               /*   4 B */
    float    y_max;               /*   4 B */
    float    x_scl;               /*   4 B */
    float    y_scl;               /*   4 B */
    float    x_res;               /*   4 B */
    uint8_t  grid_on;             /*   1 B */
    uint8_t  _pad[3];             /*   3 B — word alignment */
    uint32_t checksum;            /*   4 B — XOR of all preceding words */
} PersistBlock_t;                 /* Total: 424 B */

_Static_assert(sizeof(PersistBlock_t) % 4 == 0,
               "PersistBlock_t must be a multiple of 4 bytes");

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/**
 * @brief  Read saved state from FLASH sector 10.
 * @param  out  Destination buffer; filled on success.
 * @return true  if magic, version, and checksum all pass.
 * @return false if sector is blank or data is corrupt (use defaults).
 *
 * Pure memory-mapped read — safe to call from any context.
 */
bool Persist_Load(PersistBlock_t *out);

/**
 * @brief  Erase sector 10 and write the supplied block to FLASH.
 * @param  in  State to persist; checksum is computed internally.
 * @return true always (read-back verify optional in implementation).
 *
 * Runs erase and write routines from RAM (.RamFunc) to avoid AHB stall
 * while FLASH is busy on this single-bank device.
 */
bool Persist_Save(const PersistBlock_t *in);

/**
 * @brief  Snapshot all saveable calculator state into @p out.
 *         Implemented in calculator_core.c.
 */
void Calc_BuildPersistBlock(PersistBlock_t *out);

/**
 * @brief  Restore calculator state from a previously loaded block.
 *         Implemented in calculator_core.c.
 */
void Calc_ApplyPersistBlock(const PersistBlock_t *in);

#endif /* PERSIST_H */
