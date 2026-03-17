/**
 * @file    persist.h
 * @brief   Persistent FLASH storage interface for calculator session state.
 */

#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define PERSIST_MAGIC       0xCA1CC0DEU
#define PERSIST_VERSION     1U
#define PERSIST_FLASH_ADDR  0x080C0000U   /* Sector 7, 128 KB */
#define PERSIST_SECTOR      FLASH_SECTOR_7

typedef struct {
    uint32_t magic;
    uint32_t version;
    float    calc_variables[26];
    float    ans;
    uint8_t  mode_committed[8];
    float    zoom_x_fact;
    float    zoom_y_fact;
    char     equations[4][64];
    float    x_min;
    float    x_max;
    float    y_min;
    float    y_max;
    float    x_scl;
    float    y_scl;
    float    x_res;
    uint8_t  grid_on;
    uint8_t  _pad[3];
    uint32_t checksum;
} PersistBlock_t;

_Static_assert(sizeof(PersistBlock_t) % 4 == 0,
               "PersistBlock_t must be word-aligned");

bool Persist_Load(PersistBlock_t *out);
bool Persist_Save(const PersistBlock_t *in);
void Calc_BuildPersistBlock(PersistBlock_t *out);
void Calc_ApplyPersistBlock(const PersistBlock_t *in);

#endif /* PERSIST_H */
