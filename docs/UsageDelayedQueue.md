Ready for review
Select text to add comments on the plan
Plan: Persistent FLASH Storage (Priority Item #4)
Context
Nothing survives power-off. Variables A–Z, ANS, graph equations, RANGE, and MODE settings all reset on every boot. This makes the calculator useless for multi-session work and is a prerequisite for PRGM. The save gesture is 2nd+ON, matching real TI-81 behaviour. Wear is controlled by saving only once per session.

Architecture
Two new files + changes to four existing files. The FLASH mechanics (erase/write) live in persist.c; the state packaging lives in calculator_core.c via two bulk transfer functions.
persist.h    — PersistBlock_t struct, Persist_Load/Save, Calc_Build/ApplyPersistBlock
persist.c    — FLASH erase + word-write (all .RamFunc), checksum, load (plain read)
calculator_core.c — Calc_BuildPersistBlock, Calc_ApplyPersistBlock, TOKEN_ON handler, startup load
app_init.c   — srand() seeding
keypad_map.c — TOKEN_ON keymap entry
CMakeLists.txt — add persist.c
Key constraint: Single-bank FLASH freezes the AHB bus during erase/program. All FLASH-write code must carry __attribute__((section(".RamFunc"))) to execute from RAM. The .RamFunc section already exists in STM32F429XX_FLASH.ld lines 180–181 inside .data; no linker script changes needed.

PersistBlock_t
Defined in App/Inc/persist.h. Flat struct, entirely word-aligned, self-validating.
#define PERSIST_MAGIC       0xCA1CC0DEU
#define PERSIST_VERSION     1U
#define PERSIST_FLASH_ADDR  0x080C0000U   /* Sector 7, 128 KB, currently unused */
#define PERSIST_SECTOR      FLASH_SECTOR_7

typedef struct {
    uint32_t magic;               /*  4 B — must equal PERSIST_MAGIC          */
    uint32_t version;             /*  4 B                                      */
    float    calc_variables[26];  /* 104 B — A–Z (extern in calc_engine.h)    */
    float    ans;                 /*  4 B                                      */
    uint8_t  mode_committed[8];   /*  8 B — rows 0–7 of MODE screen           */
    float    zoom_x_fact;         /*  4 B                                      */
    float    zoom_y_fact;         /*  4 B                                      */
    char     equations[4][64];    /* 256 B — Y1–Y4                            */
    float    x_min, x_max;        /*  8 B                                      */
    float    y_min, y_max;        /*  8 B                                      */
    float    x_scl, y_scl, x_res; /* 12 B                                      */
    uint8_t  grid_on;             /*  1 B                                      */
    uint8_t  _pad[3];             /*  3 B — keeps size a multiple of 4        */
    uint32_t checksum;            /*  4 B — XOR of all preceding words, LAST  */
} PersistBlock_t;                 /* Total: 424 B                              */

_Static_assert(sizeof(PersistBlock_t) % 4 == 0, "PersistBlock_t must be word-aligned");
graph_state.active is not persisted — it is transient UI state; always boot with graph hidden. calc_decimal_mode is not a separate field — it is fully derived from mode_committed[1] and restored via the existing Calc_SetDecimalMode() setter.
Checksum: 32-bit XOR of all preceding uint32_t words. No HAL CRC peripheral needed.
static uint32_t persist_checksum(const PersistBlock_t *b)
{
    const uint32_t *w = (const uint32_t *)b;
    uint32_t n = sizeof(PersistBlock_t) / 4 - 1; /* all words except checksum */
    uint32_t crc = 0;
    for (uint32_t i = 0; i < n; i++) crc ^= w[i];
    return crc;
}

Step 1 — Create App/Inc/persist.h
* PersistBlock_t struct and _Static_assert (as above)
* #define constants: PERSIST_MAGIC, PERSIST_VERSION, PERSIST_FLASH_ADDR, PERSIST_SECTOR
* Function declarations:
    * bool Persist_Load(PersistBlock_t *out);
    * bool Persist_Save(const PersistBlock_t *in);
    * void Calc_BuildPersistBlock(PersistBlock_t *out); ← implemented in calculator_core.c
    * void Calc_ApplyPersistBlock(const PersistBlock_t *in); ← implemented in calculator_core.c
* Includes: <stdbool.h>, <stdint.h>, stm32f4xx_hal.h

Step 2 — Create App/Src/persist.c
Three internal functions + two public functions.
persist_checksum — no .RamFunc needed (just arithmetic)
See checksum algorithm above.
persist_erase_sector — must be .RamFunc
__attribute__((section(".RamFunc")))
static void persist_erase_sector(void) {
    FLASH_EraseInitTypeDef e = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = PERSIST_SECTOR,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,   /* 2.7–3.6V, 32-bit writes */
    };
    uint32_t sector_error = 0;
    HAL_FLASHEx_Erase(&e, &sector_error);
    /* sector_error == 0xFFFFFFFF on success */
}
persist_write_block — must be .RamFunc
__attribute__((section(".RamFunc")))
static void persist_write_block(const PersistBlock_t *block) {
    const uint32_t *words = (const uint32_t *)block;
    uint32_t n = sizeof(PersistBlock_t) / 4;
    for (uint32_t i = 0; i < n; i++)
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          PERSIST_FLASH_ADDR + i * 4,
                          (uint64_t)words[i]);
}
Persist_Save — must be .RamFunc
1. Copy *in to local block on stack
2. Set block.checksum = persist_checksum(&block)
3. HAL_FLASH_Unlock()
4. persist_erase_sector()
5. persist_write_block(&block)
6. HAL_FLASH_Lock()
7. Return true (optional: read back first word, compare to magic, return false on mismatch)
Persist_Load — no .RamFunc needed (memory-mapped FLASH read)
1. const PersistBlock_t *stored = (const PersistBlock_t *)PERSIST_FLASH_ADDR
2. if stored->magic != PERSIST_MAGIC → return false
3. if stored->version != PERSIST_VERSION → return false
4. if persist_checksum(stored) != stored->checksum → return false
5. memcpy(out, stored, sizeof(PersistBlock_t))
6. return true

Step 3 — Modify App/Src/calculator_core.c
3a. Add #include "persist.h" near the top include block.
3b. Add Calc_BuildPersistBlock and Calc_ApplyPersistBlock (near the static state variable declarations, ~line 120)
Calc_BuildPersistBlock — reads all static locals and graph_state into out:
* memcpy(out->calc_variables, calc_variables, sizeof(calc_variables)) — calc_variables is extern
* out->ans = ans
* memcpy(out->mode_committed, mode_committed, sizeof(mode_committed))
* out->zoom_x_fact = zoom_x_fact; out->zoom_y_fact = zoom_y_fact
* Copy all GraphState_t fields individually (not memcpy of the whole struct, because active is excluded)
Calc_ApplyPersistBlock — restores state from in:
* memcpy(calc_variables, in->calc_variables, sizeof(calc_variables))
* ans = in->ans
* memcpy(mode_committed, in->mode_committed, sizeof(mode_committed))
* angle_degrees = (in->mode_committed[2] == 1) — re-derive from mode_committed[2]
* Calc_SetDecimalMode(in->mode_committed[1]) — re-derive calc_decimal_mode via existing setter
* zoom_x_fact = in->zoom_x_fact; zoom_y_fact = in->zoom_y_fact
* Restore graph_state fields individually (copy equations, floats, grid_on; leave active = false)
3c. Add TOKEN_ON early-exit at the very top of Execute_Token, before any mode handler
void Execute_Token(Token_t t)
{
    if (t == TOKEN_ON) {
        PersistBlock_t block;
        Calc_BuildPersistBlock(&block);
        Persist_Save(&block);
        return;
    }
    /* ... rest of Execute_Token unchanged ... */
This ensures 2nd+ON saves from any mode (Y=, RANGE, MATH menu, etc.), consistent with how TOKEN_MODE should also be elevated (a separate Known Issue in CLAUDE.md).
3d. Call Persist_Load in StartCalcCoreTask after UI init, still inside lvgl_lock()
After ui_refresh_display() completes (end of the existing init block, ~line 3595), add:
    PersistBlock_t saved;
    if (Persist_Load(&saved)) {
        Calc_ApplyPersistBlock(&saved);
        ui_refresh_display();   /* show loaded ANS/expression */
        /* Sync Y= labels with loaded equations */
        for (int i = 0; i < GRAPH_NUM_EQ; i++)
            lv_label_set_text(ui_lbl_yeq_eq[i], graph_state.equations[i]);
        /* Sync MODE screen cursor with loaded mode_committed */
        ui_update_mode_display();
    }

Step 4 — Modify App/Src/app_init.c
Add #include <stdlib.h> if not present.
After xSemaphoreGive(xLVGL_Ready) (line 104), before the for(;;) render loop:
srand(HAL_GetTick());   /* seed RNG — tick varies with OS/USB init duration */
This fixes the PRB tab rand function producing the same sequence every boot.

Step 5 — Modify App/Drivers/Keypad/keypad_map.c
Note: TOKEN_ON is defined in keypad_map.h but has no entry in the keymap table. The physical ON key is not in the 7×8 matrix on the discovery board.
* For the discovery board: no physical ON key is wired. Add a temporary mapping to an unused matrix position (ID_B8_A4 is UNUSED) for testing. Document this as a placeholder until the custom PCB is built.
* For the custom PCB: wire the physical ON button to the correct matrix position.
Entry to add:
[ID_B8_A4] = { TOKEN_ON, TOKEN_ON, TOKEN_NONE },
Both normal and second slots are TOKEN_ON so the 2nd+ON gesture produces the token via the key.second path in Process_Hardware_Key.

Step 6 — Modify CMakeLists.txt
Add App/Src/persist.c to the target_sources block under the logic sources section, alongside the other App/Src/ files.

File Change Summary
File	Action	Key changes
App/Inc/persist.h	Create	PersistBlock_t, constants, all 4 function declarations
App/Src/persist.c	Create	Persist_Load, Persist_Save (.RamFunc), erase/write helpers (.RamFunc), checksum
App/Src/calculator_core.c	Modify	#include "persist.h", Calc_Build/ApplyPersistBlock, TOKEN_ON early-exit, startup Persist_Load call
App/Src/app_init.c	Modify	srand(HAL_GetTick()) after xSemaphoreGive
App/Drivers/Keypad/keypad_map.c	Modify	Add [ID_B8_A4] = {TOKEN_ON, TOKEN_ON, TOKEN_NONE}
CMakeLists.txt	Modify	Add App/Src/persist.c to target_sources
No changes to linker script, app_common.h, calc_engine.h, or keypad_map.h.

Verification
Build checks
1. Build succeeds with zero warnings on persist.c
2. In the .map file, confirm Persist_Save, persist_erase_sector, persist_write_block appear under .data output section (RAM), not .text
3. In the .map file, confirm no section VMA overlaps 0x080C0000
On-hardware test sequence
1. First boot (blank FLASH): Persist_Load returns false → defaults used → no crash
2. Save and reload: Set A=5, Y1=X^2, Xmin=-5, Fix 2 mode → 2nd+ON → reset board → confirm all values survive
3. Overwrite: Save twice (different values) → confirm second save wins after reset
4. Checksum guard: Use ST-Link to flip a byte at 0x080C0004 → reset → confirm defaults load (not corrupted values)
5. Inspect raw FLASH: mdw 0x080C0000 106 in OpenOCD → word 0 = 0xCA1CC0DE, word 1 = 0x00000001, last word = XOR of all preceding
