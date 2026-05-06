/**
 * @file    persist.h
 * @brief   Persistent FLASH storage for calculator state.
 *
 * Saves/loads variables A-Z, ANS, MODE settings, graph equations, RANGE
 * parameters, ZOOM factors, matrices, parametric equations, and STAT data
 * to STM32F429 FLASH sector 11 (0x080E0000, 128 KB).  State survives
 * power-off.  Save is triggered by 2nd+ON.
 *
 * FLASH mechanics and state packaging both live in persist.c.
 * The public entry points are Persist_BuildBlock / Persist_ApplyBlock.
 *
 * Layout: PersistBlock_t uses five typed sub-structs (GraphPersist_t,
 * StatPersist_t, MatrixPersist_t, PrgmPersist_t, ModePersist_t) plus
 * per-section version fields.  Future feature bumps increment only the
 * relevant sub-version; the migration path in Persist_Load() handles each
 * section independently.
 */

#ifndef PERSIST_H
#define PERSIST_H

#include <stdint.h>
#include <stdbool.h>
#ifndef HOST_TEST
#  include "stm32f4xx_hal.h"
#endif
#include "app_common.h"    /* GRAPH_NUM_PARAM */
#include "calc_engine.h"   /* CalcMatrix_t, CALC_MATRIX_MAX_DIM */

/*---------------------------------------------------------------------------
 * Flash target
 *---------------------------------------------------------------------------*/

#define PERSIST_MAGIC       0xCA1CC0DEU   /* Marker — "calc code" */
#define PERSIST_VERSION     9U            /* Bumped when PersistBlock_t adopted sub-struct layout */

/* Per-section versions — increment when the corresponding sub-struct layout changes.
 * Future Persist_Load() migration paths key on these independently of PERSIST_VERSION. */
#define GRAPH_PERSIST_VERSION    1U
#define STAT_PERSIST_VERSION     1U
#define MATRIX_PERSIST_VERSION   1U
#define PRGM_PERSIST_VERSION     1U
#define MODE_PERSIST_VERSION     1U
#define PERSIST_FLASH_ADDR  0x080E0000U   /* Sector 11, 128 KB, unused by firmware */
#ifndef HOST_TEST
#  define PERSIST_SECTOR    FLASH_SECTOR_11
#endif
/* STM32F429 sector map (12 sectors per bank):
 *   Sectors 0-3:  16 KB each  (0x08000000 - 0x0800FFFF)
 *   Sector  4:    64 KB       (0x08010000 - 0x0801FFFF)
 *   Sectors 5-11: 128 KB each (0x08020000 - 0x080FFFFF)
 * Firmware (~820 KB as of 2026-04-28) ends at ~0x080C84xx, inside Sector 10.
 * Linker FLASH region is capped at 896 KB (sectors 0-10) so overflow is caught
 * at link time; Sector 11 (0x080E0000) is reserved for persist. */

/*---------------------------------------------------------------------------
 * Sub-structs — one per subsystem; independently versioned
 *---------------------------------------------------------------------------*/

/**
 * @brief Graph subsystem: Y= equations, RANGE, ZOOM factors, parametric
 *        equations, T range, and Y= enable flags.
 *        graph_state.active is NOT saved — always boot with graph hidden.
 */
typedef struct {
    float    zoom_x_fact;                           /*   4 B */
    float    zoom_y_fact;                           /*   4 B */
    char     equations[4][64];                      /* 256 B — Y1–Y4 strings */
    uint8_t  enabled[4];                            /*   4 B — Y= enable flags */
    float    x_min;                                 /*   4 B */
    float    x_max;                                 /*   4 B */
    float    y_min;                                 /*   4 B */
    float    y_max;                                 /*   4 B */
    float    x_scl;                                 /*   4 B */
    float    y_scl;                                 /*   4 B */
    float    x_res;                                 /*   4 B */
    char     param_x[GRAPH_NUM_PARAM][64];          /* 192 B — X(t) strings */
    char     param_y[GRAPH_NUM_PARAM][64];          /* 192 B — Y(t) strings */
    uint8_t  param_enabled[GRAPH_NUM_PARAM];        /*   3 B — pair enable flags */
    uint8_t  param_mode;                            /*   1 B — 0=function, 1=parametric */
    float    t_min;                                 /*   4 B */
    float    t_max;                                 /*   4 B */
    float    t_step;                                /*   4 B */
} GraphPersist_t;                                   /* Total: 696 B */

/**
 * @brief Statistics data list — up to STAT_MAX_POINTS (x, y) pairs.
 */
typedef struct {
    float    list_x[STAT_MAX_POINTS];               /* 600 B */
    float    list_y[STAT_MAX_POINTS];               /* 600 B */
    uint8_t  list_len;                              /*   1 B */
    uint8_t  _pad[3];                               /*   3 B — word alignment */
} StatPersist_t;                                    /* Total: 1204 B */

/**
 * @brief Matrix dimensions and cell data for [A], [B], [C].
 *        ANS matrix (index 3) is transient and not saved.
 */
typedef struct {
    uint8_t  rows[3];                               /*   3 B — row counts for [A][B][C] */
    uint8_t  cols[3];                               /*   3 B — col counts for [A][B][C] */
    uint8_t  _pad[2];                               /*   2 B — word alignment */
    float    data[3][CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM]; /* 432 B */
} MatrixPersist_t;                                  /* Total: 440 B */

/**
 * @brief Program subsystem — reserved; programs live in a separate FLASH
 *        sector managed by prgm_exec.c and are not persisted here.
 */
typedef struct {
    uint32_t _reserved;                             /*   4 B */
} PrgmPersist_t;                                    /* Total: 4 B */

/**
 * @brief MODE screen committed selections and grid toggle.
 *        calc_decimal_mode and angle_degrees are re-derived from committed[]
 *        on load via Calc_SetDecimalMode / Calc_SetAngleDegrees.
 */
typedef struct {
    uint8_t  committed[8];                          /*   8 B — MODE screen rows 0–7 */
    uint8_t  grid_on;                               /*   1 B */
    uint8_t  _pad[3];                               /*   3 B — word alignment */
} ModePersist_t;                                    /* Total: 12 B */

/*---------------------------------------------------------------------------
 * Saved state block
 *---------------------------------------------------------------------------*/

/**
 * @brief Hierarchical, word-aligned struct written verbatim to FLASH sector 11.
 *
 * Header (16 B):
 *   magic / version   — validated on load; stale or blank sector returns false
 *   graph_ver … mode_ver — per-section versions for independent migration paths
 *
 * Sub-sections (2356 B):
 *   graph   — GraphPersist_t (696 B)
 *   stat    — StatPersist_t  (1204 B)
 *   matrix  — MatrixPersist_t (440 B)
 *   prgm    — PrgmPersist_t   (4 B, reserved)
 *   mode    — ModePersist_t   (12 B)
 *
 * Top-level fields (116 B):
 *   calc_variables — A–Z plus θ at index 26
 *   ans            — last scalar result
 *   checksum       — 32-bit XOR of all preceding words; computed by Persist_Save
 *
 * Adoption history:
 *   v1–v8  flat layout (2060 B at v6, 2472 B at v8)
 *   v9     sub-struct layout adopted (2488 B); v8 blocks are discarded on load
 */
typedef struct {
    uint32_t        magic;          /*   4 B */
    uint16_t        version;        /*   2 B */
    uint16_t        graph_ver;      /*   2 B */
    uint16_t        stat_ver;       /*   2 B */
    uint16_t        matrix_ver;     /*   2 B */
    uint16_t        prgm_ver;       /*   2 B */
    uint16_t        mode_ver;       /*   2 B — header total: 16 B */
    GraphPersist_t  graph;          /* 696 B */
    StatPersist_t   stat;           /* 1204 B */
    MatrixPersist_t matrix;         /* 440 B */
    PrgmPersist_t   prgm;           /*   4 B */
    ModePersist_t   mode;           /*  12 B — subsections total: 2356 B */
    float           calc_variables[27]; /* 108 B — [0–25]=A–Z, [26]=θ */
    float           ans;            /*   4 B */
    uint32_t        checksum;       /*   4 B — XOR of all preceding words */
} PersistBlock_t;                   /* Total: 2488 B */

_Static_assert(sizeof(PersistBlock_t) % 4 == 0,
               "PersistBlock_t must be a multiple of 4 bytes");

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/**
 * @brief  Compute the XOR checksum over all words preceding the checksum
 *         field.  Available on host for testing; not tied to FLASH.
 */
uint32_t Persist_Checksum(const PersistBlock_t *b);

/**
 * @brief  Validate magic, version, and XOR checksum of an in-memory block.
 * @return true  if all three fields match expected values.
 * @return false if the block is blank, stale, or corrupt.
 *
 * Host-compatible — does not read from FLASH.
 */
bool Persist_Validate(const PersistBlock_t *b);

#ifndef HOST_TEST
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
#endif /* HOST_TEST */

/**
 * @brief  Snapshot all saveable calculator state into a new block.
 *         Implemented in persist.c.  Not available in HOST_TEST builds.
 * @return Filled PersistBlock_t; pass to Persist_Save().
 */
PersistBlock_t Persist_BuildBlock(void);

/**
 * @brief  Restore calculator state from a previously loaded block.
 *         Implemented in persist.c.  Not available in HOST_TEST builds.
 */
void Persist_ApplyBlock(const PersistBlock_t *block);

/**
 * @brief  Factory-reset all calculator state.
 *
 * Applies guidebook p. 1-28 "Results of Resetting":
 *   - All variables A–Z, θ, ANS set to 0
 *   - All matrix values zeroed; all matrix dimensions set to 6×6
 *   - All Y= equations and parametric equations erased
 *   - RANGE set to standard defaults (x=[-10,10], y=[-10,10], scl=1, res=1)
 *   - MODE set to factory defaults (Normal, Float, Radian, Function, …)
 *   - Zoom factors set to 4
 *   - STAT data erased
 *   Caller is responsible for calling Prgm_Init()+Prgm_Save() to erase programs,
 *   and srand(0) to reset the rand seed.
 *   Must be called from RAM (calls Persist_Save internally).
 */
void Persist_Reset(void);

#endif /* PERSIST_H */
