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

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief  XOR checksum over all words preceding the checksum field.
 *
 * Does not need .RamFunc — pure arithmetic with no FLASH access.
 */
static uint32_t persist_checksum(const PersistBlock_t *b)
{
    const uint32_t *w = (const uint32_t *)b;
    uint32_t n = sizeof(PersistBlock_t) / 4 - 1; /* all words except checksum */
    uint32_t crc = 0;
    for (uint32_t i = 0; i < n; i++) {
        crc ^= w[i];
    }
    return crc;
}

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
 * @brief  Erase sector 7 and write the supplied block to FLASH.
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
    block.checksum = persist_checksum(&block);

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
    if (persist_checksum(stored) != stored->checksum) { return false; }

    memcpy(out, stored, sizeof(PersistBlock_t));
    return true;
}
