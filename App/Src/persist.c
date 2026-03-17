/**
 * @file    persist.c
 * @brief   Persistent FLASH storage implementation for calculator state.
 */

#include "persist.h"
#include <string.h>

static uint32_t persist_checksum(const PersistBlock_t *b)
{
    const uint32_t *w = (const uint32_t *)b;
    uint32_t n = sizeof(PersistBlock_t) / 4U - 1U;
    uint32_t crc = 0U;

    for (uint32_t i = 0; i < n; i++) {
        crc ^= w[i];
    }

    return crc;
}

__attribute__((section(".RamFunc")))
static void persist_erase_sector(void)
{
    FLASH_EraseInitTypeDef e = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = PERSIST_SECTOR,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t sector_error = 0;

    (void)HAL_FLASHEx_Erase(&e, &sector_error);
}

__attribute__((section(".RamFunc")))
static void persist_write_block(const PersistBlock_t *block)
{
    const uint32_t *words = (const uint32_t *)block;
    uint32_t n = sizeof(PersistBlock_t) / 4U;

    for (uint32_t i = 0; i < n; i++) {
        (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                PERSIST_FLASH_ADDR + (i * 4U),
                                (uint64_t)words[i]);
    }
}

__attribute__((section(".RamFunc")))
bool Persist_Save(const PersistBlock_t *in)
{
    PersistBlock_t block;

    memcpy(&block, in, sizeof(block));
    block.magic    = PERSIST_MAGIC;
    block.version  = PERSIST_VERSION;
    block.checksum = persist_checksum(&block);

    HAL_FLASH_Unlock();
    persist_erase_sector();
    persist_write_block(&block);
    HAL_FLASH_Lock();

    return true;
}

bool Persist_Load(PersistBlock_t *out)
{
    const PersistBlock_t *stored = (const PersistBlock_t *)PERSIST_FLASH_ADDR;

    if (stored->magic != PERSIST_MAGIC) {
        return false;
    }

    if (stored->version != PERSIST_VERSION) {
        return false;
    }

    if (persist_checksum(stored) != stored->checksum) {
        return false;
    }

    memcpy(out, stored, sizeof(*out));
    return true;
}
