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
#include <string.h>

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
