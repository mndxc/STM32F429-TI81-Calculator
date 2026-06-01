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
#  include "calc_engine.h"      /* calc_variables, calc_matrices, Calc_Set/GetNotationMode, Calc_Set/GetDecimalMode, Calc_GetAns, Calc_SetAnsScalar, Calc_Get/SetAngleDegrees */
#  include "ui_mode.h"          /* UiMode_GetCommittedArray, UiMode_RestoreCommittedArray, MODE_ROW_COUNT */
#  include "graph.h"            /* Graph_GetState, Graph_Set*, Graph_GetEquationBuf, … */
#  include "graph_ui_range.h"   /* graph_ui_get_zoom_x_fact, graph_ui_set_zoom_facts */
#  include "ui_stat.h"          /* Stat_GetData, Stat_GetResults, Stat_SetData */
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
 * Build helpers — snapshot each subsystem into an out block
 *---------------------------------------------------------------------------*/

static void build_graph_state(PersistBlock_t *out)
{
    const GraphState_t *gs = Graph_GetState();
    out->graph.zoom_x_fact = graph_ui_get_zoom_x_fact();
    out->graph.zoom_y_fact = graph_ui_get_zoom_y_fact();
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(out->graph.equations[i], Graph_GetEquationBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        out->graph.enabled[i] = gs->enabled[i] ? 1u : 0u;
    }
    out->graph.x_min = gs->x_min;
    out->graph.x_max = gs->x_max;
    out->graph.y_min = gs->y_min;
    out->graph.y_max = gs->y_max;
    out->graph.x_scl = gs->x_scl;
    out->graph.y_scl = gs->y_scl;
    out->graph.x_res = gs->x_res;
    out->mode.grid_on = gs->grid_on ? 1u : 0u;
    for (int i = 0; i < GRAPH_NUM_PARAM; i++) {
        memcpy(out->graph.param_x[i], Graph_GetParamEquationXBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        memcpy(out->graph.param_y[i], Graph_GetParamEquationYBuf((uint8_t)i), GRAPH_EQUATION_BUF_LEN);
        out->graph.param_enabled[i] = gs->param_enabled[i] ? 1u : 0u;
    }
    out->graph.param_mode = gs->param_mode ? 1u : 0u;
    out->graph.t_min  = gs->t_min;
    out->graph.t_max  = gs->t_max;
    out->graph.t_step = gs->t_step;
}

static void build_matrix_state(PersistBlock_t *out)
{
    for (int m = 0; m < 3; m++) {
        out->matrix.rows[m] = calc_matrices[m].rows;
        out->matrix.cols[m] = calc_matrices[m].cols;
        memcpy(out->matrix.data[m], calc_matrices[m].data,
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }
}

static void build_stat_state(PersistBlock_t *out)
{
    const StatData_t *sd = Stat_GetData();
    memcpy(out->stat.list_x, sd->list_x, STAT_MAX_POINTS * sizeof(float));
    memcpy(out->stat.list_y, sd->list_y, STAT_MAX_POINTS * sizeof(float));
    out->stat.list_len = sd->list_len;
}

/*---------------------------------------------------------------------------
 * Apply helpers — restore each subsystem from a loaded block
 *---------------------------------------------------------------------------*/

static void apply_graph_state(const PersistBlock_t *block)
{
    graph_ui_set_zoom_facts(block->graph.zoom_x_fact, block->graph.zoom_y_fact);
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        memcpy(Graph_GetEquationBuf((uint8_t)i), block->graph.equations[i], GRAPH_EQUATION_BUF_LEN);
        Graph_SetEquationEnabled((uint8_t)i, (block->graph.enabled[i] != 0));
    }
    Graph_SetWindow(block->graph.x_min, block->graph.x_max,
                    block->graph.y_min, block->graph.y_max,
                    block->graph.x_scl, block->graph.y_scl, block->graph.x_res);
    Graph_SetGridOn(block->mode.grid_on != 0);
    Graph_SetConnectedMode(block->mode.committed[4] == 0);
    Graph_SetSequentialMode(block->mode.committed[5] == 0);
    Graph_SetPolarDisplay(block->mode.committed[7] == 1);
    for (int i = 0; i < GRAPH_NUM_PARAM; i++) {
        char *px = Graph_GetParamEquationXBuf((uint8_t)i);
        memcpy(px, block->graph.param_x[i], GRAPH_EQUATION_BUF_LEN);
        px[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
        char *py = Graph_GetParamEquationYBuf((uint8_t)i);
        memcpy(py, block->graph.param_y[i], GRAPH_EQUATION_BUF_LEN);
        py[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
        Graph_SetParamEnabled((uint8_t)i, (block->graph.param_enabled[i] != 0));
    }
    Graph_SetParamMode(block->graph.param_mode != 0);
    Graph_SetParamWindow(block->graph.t_min, block->graph.t_max,
                         (block->graph.t_step > 0.0f) ? block->graph.t_step : 0.1309f);
    UiMode_SetRow(3, (block->graph.param_mode != 0) ? 1u : 0u);
}

static void apply_matrix_state(const PersistBlock_t *block)
{
    for (int m = 0; m < 3; m++) {
        uint8_t rows = block->matrix.rows[m];
        uint8_t cols = block->matrix.cols[m];
        calc_matrices[m].rows = (rows >= 1 && rows <= CALC_MATRIX_MAX_DIM) ? rows : 3;
        calc_matrices[m].cols = (cols >= 1 && cols <= CALC_MATRIX_MAX_DIM) ? cols : 3;
        memcpy(calc_matrices[m].data, block->matrix.data[m],
               CALC_MATRIX_MAX_DIM * CALC_MATRIX_MAX_DIM * sizeof(float));
    }
}

static void apply_stat_state(const PersistBlock_t *block)
{
    StatData_t sd;
    memcpy(sd.list_x, block->stat.list_x, STAT_MAX_POINTS * sizeof(float));
    memcpy(sd.list_y, block->stat.list_y, STAT_MAX_POINTS * sizeof(float));
    sd.list_len = (block->stat.list_len <= STAT_MAX_POINTS)
                  ? block->stat.list_len : 0u;
    Stat_SetData(&sd);
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

    block.magic      = PERSIST_MAGIC;
    block.version    = PERSIST_VERSION;
    block.graph_ver  = GRAPH_PERSIST_VERSION;
    block.stat_ver   = STAT_PERSIST_VERSION;
    block.matrix_ver = MATRIX_PERSIST_VERSION;
    block.prgm_ver   = PRGM_PERSIST_VERSION;
    block.mode_ver   = MODE_PERSIST_VERSION;
    block.checksum   = Persist_Checksum(&block);

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

    out.graph_ver  = GRAPH_PERSIST_VERSION;
    out.stat_ver   = STAT_PERSIST_VERSION;
    out.matrix_ver = MATRIX_PERSIST_VERSION;
    out.prgm_ver   = PRGM_PERSIST_VERSION;
    out.mode_ver   = MODE_PERSIST_VERSION;

    UiMode_GetCommittedArray(out.mode.committed, MODE_ROW_COUNT);
    build_graph_state(&out);
    build_matrix_state(&out);
    build_stat_state(&out);

    return out;
}

/**
 * @brief  Restore calculator state from a previously loaded block.
 *
 * Re-derives angle_degrees, calc_decimal_mode, and MODE screen cursor from
 * the restored mode.committed block so behaviour is consistent with ENTER on MODE.
 */
void Persist_ApplyBlock(const PersistBlock_t *block)
{
    memcpy(calc_variables, block->calc_variables, sizeof(calc_variables));
    Calc_SetAnsScalar(block->ans);
    UiMode_RestoreCommittedArray(block->mode.committed, MODE_ROW_COUNT);

    /* Re-derive state computed from mode.committed */
    Calc_SetNotationMode(block->mode.committed[0]);
    Calc_SetDecimalMode(block->mode.committed[1]);
    Calc_SetAngleDegrees(block->mode.committed[2] == 1);

    apply_graph_state(block);
    apply_matrix_state(block);
    apply_stat_state(block);
}

/**
 * @brief  Factory-reset all calculator state (guidebook p. 1-28).
 *
 * Builds a default-valued block, applies it in-memory, then saves to FLASH.
 * The caller must separately call Prgm_Init()+Prgm_Save() and srand(0).
 */
void Persist_Reset(void)
{
    PersistBlock_t block;
    memset(&block, 0, sizeof(block));

    /* RANGE: standard defaults (guidebook p. 1-28, p. 3-2) */
    block.graph.x_min  = -10.0f;  block.graph.x_max  =  10.0f;
    block.graph.y_min  = -10.0f;  block.graph.y_max  =  10.0f;
    block.graph.x_scl  =   1.0f;  block.graph.y_scl  =   1.0f;
    block.graph.x_res  =   1.0f;
    block.graph.t_min  =   0.0f;  block.graph.t_max  =   6.2832f;
    block.graph.t_step =   0.10472f; /* π/30, guidebook p. 4-5 */

    /* Zoom factors: 4 (guidebook p. 1-28) */
    block.graph.zoom_x_fact = 4.0f;
    block.graph.zoom_y_fact = 4.0f;

    /* Matrix dimensions: 6×6 (guidebook p. 1-28) */
    for (int m = 0; m < 3; m++) {
        block.matrix.rows[m] = CALC_MATRIX_MAX_DIM;
        block.matrix.cols[m] = CALC_MATRIX_MAX_DIM;
    }

    /* All other fields (variables, equations, stat, mode) remain zeroed:
     * – calc_variables[27] = 0  → A–Z, θ all zero
     * – ans = 0
     * – mode_committed[8] = 0  → all row-0: Normal/Float/Radian/Function/Connected/Sequential/GridOff/Rect
     * – matrix_data = 0        → all matrix elements zero
     * – equations/param_x/y = "" (null-terminated by memset)
     * – stat_list_len = 0      → stat data cleared */

    Persist_ApplyBlock(&block);
    Persist_Save(&block);
}

#endif /* HOST_TEST */
