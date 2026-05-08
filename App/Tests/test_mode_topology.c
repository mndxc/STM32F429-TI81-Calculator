/**
 * @file    test_mode_topology.c
 * @brief   Host-compiled test: every CalcMode_t value maps to exactly one
 *          routing entry in k_route_table[] or one known_special_cases[] slot.
 *
 * Build and run:
 *   cmake -S App/Tests -B build/tests && cmake --build build/tests
 *   ./build/tests/test_mode_topology
 *
 * Returns 0 on all pass, 1 on any failure.
 *
 * What is tested:
 *   calc_mode_topology_validate() walks every CalcMode_t value [0, MODE_COUNT)
 *   and asserts each appears in exactly one of:
 *   (a) k_route_table[] — a non-fallback predicate fires when current_mode == mode, or
 *   (b) known_special_cases[] — modes intentionally handled outside per-mode dispatch.
 *
 *   Adding a new CalcMode_t value without updating either the routing table
 *   (a new pred + handler row) or known_special_cases (explicit declaration of
 *   intent) causes this test to fail — forcing the developer to declare intent.
 */

#include <stdio.h>
#include <string.h>
#include "prgm_exec.h"
#include "calculator_core_test_stubs.h"
#include "calculator_core.h"
#include "calc_mode_topology.h"

/* -------------------------------------------------------------------------
 * External symbol definitions required by calculator_core.c under HOST_TEST.
 * Mirror test_normal_mode.c exactly so the same set of linked .c files works.
 * ---------------------------------------------------------------------- */

lv_obj_t *ui_matrix_screen             = NULL;
lv_obj_t *ui_matrix_edit_screen        = NULL;
lv_obj_t *ui_graph_yeq_screen          = NULL;
lv_obj_t *ui_param_yeq_screen          = NULL;
lv_obj_t *ui_graph_range_screen        = NULL;
lv_obj_t *ui_graph_zoom_screen         = NULL;
lv_obj_t *ui_graph_zoom_factors_screen = NULL;

const lv_font_t jetbrains_mono_24 = {0};
const lv_font_t jetbrains_mono_20 = {0};

SemaphoreHandle_t xLVGL_Mutex    = NULL;
SemaphoreHandle_t xLVGL_Ready    = NULL;
osMessageQId      keypadQueueHandle = NULL;

MenuState_t matrix_menu_state = {0};

lv_obj_t *ui_stat_screen         = NULL;
lv_obj_t *ui_stat_edit_screen    = NULL;
lv_obj_t *ui_stat_results_screen = NULL;
MenuState_t stat_menu_state = {0};

lv_obj_t *ui_math_screen = NULL;
lv_obj_t *ui_test_screen = NULL;

lv_obj_t *ui_draw_screen = NULL;
MenuState_t draw_menu_state = {0};

lv_obj_t *ui_vars_screen = NULL;
MenuState_t vars_menu_state = {0, 0, 0, MODE_NORMAL};

lv_obj_t *ui_yvars_screen = NULL;
MenuState_t yvars_menu_state = {0};

ProgramStore_t g_prgm_store;
char    prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];
uint8_t prgm_edit_num_lines = 0;

HistoryEntry_t history[HISTORY_LINE_COUNT];
uint8_t        history_count        = 0;
int8_t         history_recall_offset = 0;

bool Persist_Save(const PersistBlock_t *b)       { (void)b; return true; }
bool Persist_Load(PersistBlock_t *b)             { (void)b; return false; }
PersistBlock_t Persist_BuildBlock(void)          { PersistBlock_t b; memset(&b, 0, sizeof(b)); return b; }
void Persist_ApplyBlock(const PersistBlock_t *b) { (void)b; }
bool Prgm_Save(void)                             { return false; }
void Prgm_Init(void)                             {}
bool Prgm_Load(void)                             { return false; }
void Power_DisplayBlankAndMessage(void)          {}

/* -------------------------------------------------------------------------
 * Test infrastructure
 * ---------------------------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name) do {                                       \
    if (cond) {                                                      \
        g_passed++;                                                  \
    } else {                                                         \
        g_failed++;                                                  \
        printf("  FAIL [line %d]: %s\n", __LINE__, (name));         \
    }                                                                \
} while (0)

int main(void)
{
    printf("=== test_mode_topology ===\n");

    CHECK(calc_mode_topology_validate(),
          "CalcMode_t — every mode in route table XOR known_special_cases, no stale entries");

    /* CalcMode_IsValidTransition: MODE_STO and MODE_COUNT are not settable */
    CHECK(!CalcMode_IsValidTransition(MODE_NORMAL, MODE_STO),
          "MODE_STO is not a valid transition target");
    CHECK(!CalcMode_IsValidTransition(MODE_NORMAL, MODE_COUNT),
          "MODE_COUNT sentinel is not a valid transition target");
    CHECK(!CalcMode_IsValidTransition(MODE_NORMAL, (CalcMode_t)(MODE_COUNT + 1)),
          "out-of-range value is not a valid transition target");

    /* All real modes (excluding synthetic/sentinel) are valid targets */
    bool all_real_modes_valid = true;
    for (int m = 0; m < (int)MODE_STO; m++) {
        if (!CalcMode_IsValidTransition(MODE_NORMAL, (CalcMode_t)m)) {
            printf("  FAIL: mode %d should be a valid transition target\n", m);
            all_real_modes_valid = false;
        }
    }
    CHECK(all_real_modes_valid, "all non-synthetic modes are valid transition targets");

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
