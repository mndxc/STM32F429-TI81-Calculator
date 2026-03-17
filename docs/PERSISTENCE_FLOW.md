# Persistence Save/Load Flow

This document explains exactly **what gets persisted**, **when it is written**, and **how it is restored** after power-off.

---

## Where data is stored

- Address: `0x080C0000`
- STM32F429 sector: `FLASH_SECTOR_7`
- Block type: `PersistBlock_t`
- Validation:
  1. `magic == 0xCA1CC0DE`
  2. `version == 1`
  3. `checksum == XOR(all words except checksum field)`

Definitions live in `App/Inc/persist.h`, implementation in `App/Src/persist.c`.

---

## What is persisted

The persisted block contains:

- `calc_variables[26]` (A..Z)
- `ans`
- `mode_committed[8]`
- `zoom_x_fact`, `zoom_y_fact`
- `equations[4][64]` (Y1..Y4)
- Graph window/range values: `x_min/x_max/y_min/y_max/x_scl/y_scl/x_res`
- `grid_on`

Not persisted on purpose:

- `graph_state.active` (transient UI state)
  - Always restored as `false` on boot.

Derived/restored behavior:

- `angle_degrees` is re-derived from `mode_committed[2]`.
- Decimal mode is restored through `Calc_SetDecimalMode(mode_committed[1])`.

---

## Save path (runtime)

Save is triggered by `TOKEN_ON` in `Execute_Token()`:

1. Build a RAM block from live calculator state: `Calc_BuildPersistBlock(&block)`
2. Save block to flash: `Persist_Save(&block)`

`Persist_Save()` steps:

1. Copy caller block to local stack block
2. Force header fields (`magic`, `version`)
3. Compute checksum
4. `HAL_FLASH_Unlock()`
5. Erase sector 7
6. Program all words to `0x080C0000`
7. `HAL_FLASH_Lock()`

Flash erase/program routines are placed in `.RamFunc` so code executes from RAM during flash operations.

---

## Load path (boot)

Load occurs during calculator task startup (`StartCalcCoreTask`):

1. UI is initialized
2. `Persist_Load(&saved)` is called
3. If valid block is found:
   1. `Calc_ApplyPersistBlock(&saved)` restores state
   2. display is refreshed (`ui_refresh_display()`)
   3. Y= labels are synced from restored equations
   4. MODE screen display is synced (`ui_update_mode_display()`)

`Persist_Load()` returns `false` if any validation fails (magic/version/checksum). In that case, default runtime initialization stays in effect.

---

## Sequence summary

### Save (2nd+ON -> TOKEN_ON)

`Execute_Token(TOKEN_ON)`
-> `Calc_BuildPersistBlock()`
-> `Persist_Save()`
-> `HAL_FLASH_Unlock()`
-> `erase sector 7`
-> `program words`
-> `HAL_FLASH_Lock()`

### Boot restore

`StartCalcCoreTask()`
-> `Persist_Load()`
-> validate magic/version/checksum
-> `Calc_ApplyPersistBlock()`
-> UI sync/refresh

---

## Quick debugging checklist

1. Read flash base words:
   - word 0 should be `0xCA1CC0DE`
   - word 1 should be `0x00000001`
2. If load is ignored, check:
   - magic mismatch
   - version mismatch
   - checksum mismatch
3. Confirm ON mapping exists for your hardware key matrix (Discovery placeholder maps `ID_B8_A4` to `TOKEN_ON`).
