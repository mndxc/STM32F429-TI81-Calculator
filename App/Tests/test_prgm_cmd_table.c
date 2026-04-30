/**
 * @file    test_prgm_cmd_table.c
 * @brief   Host-compiled test: validate cmd_table[] prefix ordering in prgm_exec.c.
 *
 * Build and run:
 *   cmake -S App/Tests -B App/Tests/build && cmake --build App/Tests/build
 *   ./App/Tests/build/test_prgm_cmd_table
 *
 * Returns 0 on all pass, 1 on any failure.
 *
 * What is tested:
 *   prgm_exec.c dispatches program lines via a linear scan of cmd_table[].
 *   Non-exact entries use strncmp prefix matching, so a shorter non-exact
 *   prefix appearing before a longer prefix it starts with would silently
 *   shadow the later entry (the longer command would never be reached).
 *   prgm_cmd_table_validate() checks every (i, j) pair where i < j and
 *   entry i is non-exact; it fails if cmd_table[i].prefix is a prefix of
 *   cmd_table[j].prefix.
 */

#include <stdio.h>
#include <string.h>
#include "prgm_exec.h"
#include "prgm_exec_test_stubs.h"

/* -------------------------------------------------------------------------
 * Global state definitions required by prgm_exec.c under HOST_TEST.
 * Must match the pattern in test_prgm_exec.c exactly.
 * ---------------------------------------------------------------------- */

CalcMode_t     current_mode           = MODE_NORMAL;
float          ans                    = 0.0f;
bool           ans_is_matrix          = false;
bool           angle_degrees          = false;

HistoryEntry_t history[HISTORY_LINE_COUNT];
uint8_t        history_count          = 0;
int8_t         history_recall_offset  = 0;

ExprBuffer_t   expr;

char    prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];
uint8_t prgm_edit_num_lines = 0;

ProgramStore_t g_prgm_store;

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
    printf("=== test_prgm_cmd_table ===\n");

    /* T1-A: no non-exact prefix in cmd_table[] may shadow a later entry */
    CHECK(prgm_cmd_table_validate(),
          "cmd_table[] — no non-exact prefix shadows a later entry");

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
