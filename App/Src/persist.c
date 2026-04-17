/**
 * @file    persist.c
 * @brief   FLASH erase/write and state load/save for persistent storage.
 *
 * All routines that touch FLASH (erase, write, Persist_Save) carry
 * __attribute__((section(".RamFunc"))) so they execute from RAM during
 * the AHB bus stall caused by single-bank FLASH programming on STM32F429.
 *
 * Persist_Load is a plain memory-mapped read and needs no special placement.
 */

#include "persist.h"
#include <string.h>
#ifndef HOST_TEST
#  include "calc_engine.h"      /* calc_variables, calc_matrices, Calc_SetDecimalMode */
#  include "calculator_core.h"  /* Calc_GetAns, Calc_SetAnsScalar, Calc_Get/SetAngleDegrees */
#  include "ui_mode.h"          /* s_mode, ModeScreenState_t */
#  include "graph.h"            /* Graph_GetState, Graph_Set*, Graph_GetEquationBuf, … */
#  include "graph_ui_range.h"   /* graph_ui_get_zoom_x_fact, graph_ui_set_zoom_facts */
#endif

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief  XOR checksum over all words preceding the checksum field.
 *
 * Does not need .RamFunc — pure arithmetic with no FLASH access.
 * Exposed as public API for host-side testing via Persist_Checksum().
 */
uint32_t Persist_Checksum(const PersistBlock_t *b)
{
    const uint32_t *w = (const uint32_t *)b;
    uint32_t n = sizeof(PersistBlock_t) / 4 - 1; /* all words except checksum */
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < n; i++) {
        checksum ^= w[i];
    }
    return checksum;
}

/**
 * @brief  Validate magic, version, and checksum of an in-memory block.
 *
 * Does not read from FLASH — works on any PersistBlock_t in RAM.
 * Suitable for host-side round-trip testing.
 */
bool Persist_Validate(const PersistBlock_t *b)
{
    if (b->magic   != PERSIST_MAGIC)   return false;
    if (b->version != PERSIST_VERSION) return false;
    if (Persist_Checksum(b) != b->checksum) return false;
    return true;
}

#ifndef HOST_TEST

/**
 * @brief  Erase FLASH sector 10.
 *
 * Must run from RAM — FLASH is inaccessible while a sector erase is in
 * progress on this single-bank device.
 */
__attribute__((section(".RamFunc")))
static void persist_erase_sector(void)
{
    FLASH_EraseInitTypeDef e = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = PERSIST_SECTOR,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,  /* 2.7–3.6 V, 32-bit writes */
    };
    uint32_t sector_error = 0;
    HAL_FLASHEx_Erase(&e, &sector_error);
    /* sector_error == 0xFFFFFFFF on success */
}

/**
 * @brief  Write a PersistBlock_t to FLASH word by word.
 *
 * Must run from RAM for the same reason as persist_erase_sector.
 */
__attribute__((section(".RamFunc")))
static void persist_write_block(const PersistBlock_t *block)
{
    const uint32_t *words = (const uint32_t *)block;
    uint32_t n = sizeof(PersistBlock_t) / 4;
    for (uint32_t i = 0; i < n; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          PERSIST_FLASH_ADDR + i * 4,
                          (uint64_t)words[i]);
    }
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/**
 * @brief  Erase sector 10 and write the supplied block to FLASH.
 *
 * Copies *in to a local stack buffer first so the source pointer remains
 * valid in RAM throughout the write (caller's data may be in .bss).
 * Checksum is computed and injected before writing.
 *
 * Must run from RAM — called from Execute_Token while FLASH erase/write is
 * in progress.
 */
__attribute__((section(".RamFunc")))
bool Persist_Save(const PersistBlock_t *in)
{
    /* Work from a RAM copy so the source is always accessible */
    PersistBlock_t block;
    memcpy(&block, in, sizeof(block));

    block.magic    = PERSIST_MAGIC;
    block.version  = PERSIST_VERSION;
    block.checksum = Persist_Checksum(&block);

    HAL_FLASH_Unlock();
    persist_erase_sector();
    persist_write_block(&block);
    HAL_FLASH_Lock();

    /* Optional read-back verify: check the magic word was written */
    const PersistBlock_t *stored = (const PersistBlock_t *)PERSIST_FLASH_ADDR;
    return (stored->magic == PERSIST_MAGIC);
}

/**
 * @brief  Read saved state from FLASH sector 10.
 *
 * Validates magic, version, and XOR checksum before trusting the data.
 * Returns false on blank (0xFFFFFFFF) or corrupt sector — caller should
 * keep default initialisation values.
 *
 * Pure memory-mapped read; no .RamFunc needed.
 */
bool Persist_Load(PersistBlock_t *out)
{
    const PersistBlock_t *stored = (const PersistBlock_t *)PERSIST_FLASH_ADDR;

    if (stored->magic   != PERSIST_MAGIC)   { return false; }
    if (stored->version != PERSIST_VERSION) { return false; }
    if (Persist_Checksum(stored) != stored->checksum) { return false; }

    memcpy(out, stored, sizeof(PersistBlock_t));
    return true;
}

/**
 * @brief  Snapshot all saveable calculator state into a new block.
 *
 * Reads all subsystem state through the public accessor APIs introduced by
 * COUPLING_REFACTOR T1-T3 (Graph_GetState, Calc_GetAns, etc.) so persist.c
 * has no raw extern dependencies on calculator_core.c internals.
 *
 * graph_state.active is intentionally excluded — always boot with graph hidden.
 */
PersistBlock_t Persist_BuildBlock(void)
{
    PersistBlock_t out;
    memset(&out, 0, sizeof(out));

    memcpy(out.calc_variables, calc_variables, sizeof(calc_variables));
    out.ans = Calc_GetAns();
    memcpy(out.mode_committed, s_mode.committed, sizeof(s_mode.committed));
    out.zoom_x_fact = graph_ui_get_zoom_x_fact();
    out.zoom_y_fact = graph_ui_get_zoom_y_fact();

    /* Graph state — copy fields individually (skip active) */
    const GraphState_t *gs = Graph_GetState();
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(out.equations[i], Graph_GetEquationBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
    }
    out.x_min   = gs->x_min;
    out.x_max   = gs->x_max;
    out.y_min   = gs->y_min;
    out.y_max   = gs->y_max;
    out.x_scl   = gs->x_scl;
    out.y_scl   = gs->y_scl;
    out.x_res   = gs->x_res;
    out.grid_on = gs->grid_on ? 1u : 0u;
    for (int i = 0; i < 4; i++) out.enabled[i] = gs->enabled[i] ? 1u : 0u;

    /* Matrices [A], [B], [C] — save dimensions and flatten 6×6 data arrays */
    for (int m = 0; m < 3; m++) {
        out.matrix_rows[m] = calc_matrices[m].rows;
        out.matrix_cols[m] = calc_matrices[m].cols;
        memcpy(out.matrix_data[m], calc_matrices[m].data,
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }

    /* Parametric equations and T range */
    for (int i = 0; i < GRAPH_NUM_PARAM; i++) {
        memcpy(out.param_x[i], Graph_GetParamEquationXBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        memcpy(out.param_y[i], Graph_GetParamEquationYBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        out.param_enabled[i] = gs->param_enabled[i] ? 1u : 0u;
    }
    out.param_mode = gs->param_mode ? 1u : 0u;
    out.t_min  = gs->t_min;
    out.t_max  = gs->t_max;
    out.t_step = gs->t_step;

    /* STAT data list */
    memcpy(out.stat_list_x, stat_data.list_x,
           STAT_MAX_POINTS * sizeof(float));
    memcpy(out.stat_list_y, stat_data.list_y,
           STAT_MAX_POINTS * sizeof(float));
    out.stat_list_len = stat_data.list_len;

    return out;
}

/**
 * @brief  Restore calculator state from a previously loaded block.
 *
 * Re-derives angle_degrees, calc_decimal_mode, and MODE screen cursor from
 * the restored s_mode.committed so behaviour is consistent with ENTER on MODE.
 */
void Persist_ApplyBlock(const PersistBlock_t *block)
{
    memcpy(calc_variables, block->calc_variables, sizeof(calc_variables));
    Calc_SetAnsScalar(block->ans);
    memcpy(s_mode.committed, block->mode_committed, sizeof(s_mode.committed));
    for (int i = 0; i < MODE_ROW_COUNT; i++) s_mode.cursor[i] = s_mode.committed[i];

    /* Re-derive state computed from s_mode.committed */
    Calc_SetAngleDegrees(block->mode_committed[2] == 1);
    Calc_SetDecimalMode(block->mode_committed[1]);

    graph_ui_set_zoom_facts(block->zoom_x_fact, block->zoom_y_fact);

    /* Restore graph state — leave active = false */
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(Graph_GetEquationBuf((uint8_t)i), block->equations[i], GRAPH_EQUATION_BUF_LEN);
    }
    Graph_SetWindow(block->x_min, block->x_max, block->y_min, block->y_max,
                    block->x_scl, block->y_scl, block->x_res);
    Graph_SetGridOn(block->grid_on != 0);
    for (int i = 0; i < 4; i++) Graph_SetEquationEnabled((uint8_t)i, (block->enabled[i] != 0));

    /* Restore matrices [A], [B], [C] — dimensions and 6×6 data */
    for (int m = 0; m < 3; m++) {
        uint8_t rows = block->matrix_rows[m];
        uint8_t cols = block->matrix_cols[m];
        /* Clamp to valid range in case of corrupt data */
        calc_matrices[m].rows = (rows >= 1 && rows <= CALC_MATRIX_MAX_DIM) ? rows : 3;
        calc_matrices[m].cols = (cols >= 1 && cols <= CALC_MATRIX_MAX_DIM) ? cols : 3;
        memcpy(calc_matrices[m].data, block->matrix_data[m],
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }

    /* Restore parametric equations and T range */
    for (int i = 0; i < GRAPH_NUM_PARAM; i++) {
        char *px = Graph_GetParamEquationXBuf((uint8_t)i);
        memcpy(px, block->param_x[i], GRAPH_EQUATION_BUF_LEN);
        px[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
        char *py = Graph_GetParamEquationYBuf((uint8_t)i);
        memcpy(py, block->param_y[i], GRAPH_EQUATION_BUF_LEN);
        py[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
        Graph_SetParamEnabled((uint8_t)i, (block->param_enabled[i] != 0));
    }
    Graph_SetParamMode(block->param_mode != 0);
    Graph_SetParamWindow(block->t_min, block->t_max,
                         (block->t_step > 0.0f) ? block->t_step : 0.1309f);
    /* Re-sync MODE screen cursor for row 4 (Function|Param) */
    s_mode.cursor[4]    = (block->param_mode != 0) ? 1u : 0u;
    s_mode.committed[4] = s_mode.cursor[4];

    /* Restore STAT data list */
    memcpy(stat_data.list_x, block->stat_list_x,
           STAT_MAX_POINTS * sizeof(float));
    memcpy(stat_data.list_y, block->stat_list_y,
           STAT_MAX_POINTS * sizeof(float));
    stat_data.list_len = (block->stat_list_len <= STAT_MAX_POINTS)
                         ? block->stat_list_len : 0u;
}

#endif /* HOST_TEST */
