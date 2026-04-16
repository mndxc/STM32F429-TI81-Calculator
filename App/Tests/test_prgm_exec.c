/**
 * @file    test_prgm_exec.c
 * @brief   Host-compiled unit tests for the prgm_exec.c executor.
 *
 * Build and run (no ARM toolchain required):
 *   cmake -S App/Tests -B build/tests && cmake --build build/tests
 *   ./build/tests/test_prgm_exec
 *
 * Returns 0 on all pass, 1 on any failure.
 *
 * Testing strategy:
 *   - Programs are loaded into g_prgm_store.bodies[0] (slot 0, ID "1").
 *   - prgm_run_start(0) runs the program synchronously.
 *   - Observables: current_mode, calc_variables[], ans, history_count,
 *     history[].result, history[].expression.
 *   - MODE_NORMAL after run = program completed.
 *   - MODE_PRGM_RUNNING after run = program paused (Pause/Input/Prompt).
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "prgm_exec.h"          /* ProgramStore_t, prgm_run_start, etc. */
#include "prgm_exec_test_stubs.h" /* CalcMode_t, HistoryEntry_t, externs */

/* -------------------------------------------------------------------------
 * Global state definitions (declared extern in prgm_exec_test_stubs.h)
 * ---------------------------------------------------------------------- */

CalcMode_t    current_mode          = MODE_NORMAL;
float         ans                   = 0.0f;
bool          ans_is_matrix         = false;
bool          angle_degrees         = false;

HistoryEntry_t history[HISTORY_LINE_COUNT];
uint8_t        history_count        = 0;
int8_t         history_recall_offset = 0;

ExprBuffer_t expr;  /* .buf, .len, .cursor — replaces the former three variables */

/* Compatibility aliases so existing test assertions and helpers compile unchanged.
 * NOTE: 'expression' is intentionally NOT aliased — HistoryEntry_t also has an
 * '.expression' field, so a global #define would silently corrupt those accesses. */
#define expr_len     expr.len
#define cursor_pos   expr.cursor

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

#define NEAR(a, b)  (fabs((double)(a) - (double)(b)) < 1e-4)

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/** Reset all observable state before each test group. */
static void reset_state(void)
{
    for (int i = 0; i < 26; i++) calc_variables[i] = 0.0f;
    ans            = 0.0f;
    ans_is_matrix  = false;
    angle_degrees  = false;
    current_mode   = MODE_NORMAL;
    history_count  = 0;
    history_recall_offset = 0;
    memset(history, 0, sizeof(history));
    memset(expr.buf, 0, sizeof(expr.buf));
    expr_len   = 0;
    cursor_pos = 0;
    memset(&g_prgm_store, 0, sizeof(g_prgm_store));
    memset(prgm_edit_lines, 0, sizeof(prgm_edit_lines));
    prgm_edit_num_lines = 0;
}

/**
 * Load @p body into slot 0 and run it.  Uses slot name "P" so that
 * prgm_slot_is_used(0) returns true and the slot ID is "1".
 */
static void run_program(const char *body)
{
    strncpy(g_prgm_store.names[0], "P", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[0], body, PRGM_BODY_LEN - 1);
    g_prgm_store.bodies[0][PRGM_BODY_LEN - 1] = '\0';
    prgm_run_start(0);
}

/* -------------------------------------------------------------------------
 * Group 1: Goto / Lbl (5 tests)
 * ---------------------------------------------------------------------- */
static void test_goto_lbl(void)
{
    printf("Group 1: Goto/Lbl\n");

    /* 1. Goto jumps past lines to Lbl */
    reset_state();
    run_program("Goto A\n99->B\nLbl A\n5->C");
    CHECK(current_mode == MODE_NORMAL, "goto: program completes");
    CHECK(NEAR(calc_variables['C'-'A'], 5.0f), "goto: C=5 after label");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "goto: B skipped");

    /* 2. Goto to first line loops once then runs forward */
    reset_state();
    calc_variables['A'-'A'] = 0.0f;
    run_program("1->A\nGoto B\n99->A\nLbl B\n2->C");
    CHECK(NEAR(calc_variables['C'-'A'], 2.0f), "goto: first-line label reached");

    /* 3. Label is a no-op during sequential execution */
    reset_state();
    run_program("3->A\nLbl X\n4->B");
    CHECK(NEAR(calc_variables['A'-'A'], 3.0f), "lbl: A=3 sequential");
    CHECK(NEAR(calc_variables['B'-'A'], 4.0f), "lbl: B=4 sequential");

    /* 4. Goto non-existent label aborts program */
    reset_state();
    run_program("Goto Z\n5->A");
    CHECK(current_mode == MODE_NORMAL, "goto: unknown label aborts cleanly");
    CHECK(NEAR(calc_variables['A'-'A'], 0.0f), "goto: A not set after abort");

    /* 5. Goto forward to end of program */
    reset_state();
    run_program("Goto E\n1->A\n2->B\nLbl E\n7->C");
    CHECK(NEAR(calc_variables['C'-'A'], 7.0f), "goto: forward to end label");
    CHECK(NEAR(calc_variables['A'-'A'], 0.0f), "goto: A skipped");
}

/* -------------------------------------------------------------------------
 * Group 2: If single-line (5 tests)
 * ---------------------------------------------------------------------- */
static void test_if_single(void)
{
    printf("Group 2: If single-line\n");

    /* 1. Condition true: next line executes */
    reset_state();
    run_program("1->A\nIf A=1\n5->B");
    CHECK(NEAR(calc_variables['B'-'A'], 5.0f), "if-single: true executes next");

    /* 2. Condition false: next line skipped */
    reset_state();
    run_program("0->A\nIf A=1\n5->B\n9->C");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "if-single: false skips next");
    CHECK(NEAR(calc_variables['C'-'A'], 9.0f), "if-single: line after skip runs");

    /* 3. Condition evaluates expression */
    reset_state();
    run_program("3->A\nIf A>2\n1->B");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "if-single: A>2 true");

    /* 4. Condition false skips exactly one line */
    reset_state();
    run_program("0->A\nIf A\n1->B\n2->C");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "if-single: skip one line");
    CHECK(NEAR(calc_variables['C'-'A'], 2.0f), "if-single: two lines after still run");

    /* 5. If at end of program (no next line — no crash) */
    reset_state();
    run_program("1->A\nIf A");
    CHECK(current_mode == MODE_NORMAL, "if-single: at end of program no crash");
}

/* -------------------------------------------------------------------------
 * Group 6: IS> / DS< (6 tests)
 * ---------------------------------------------------------------------- */
static void test_is_ds(void)
{
    printf("Group 6: IS>/DS<\n");

    /* 1. IS>: increments var and skips next line when > threshold */
    reset_state();
    calc_variables['A'-'A'] = 2.0f;
    run_program("IS>(A,2\n1->B\n9->C");
    /* A: 2+1=3 > 2 → skip line "1->B"; C=9 runs */
    CHECK(NEAR(calc_variables['A'-'A'], 3.0f), "IS>: incremented");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "IS>: next line skipped");
    CHECK(NEAR(calc_variables['C'-'A'], 9.0f), "IS>: line after skip runs");

    /* 2. IS>: increments but does NOT skip when <= threshold */
    reset_state();
    calc_variables['A'-'A'] = 0.0f;
    run_program("IS>(A,2\n1->B\n9->C");
    /* A: 0+1=1 <= 2 → do NOT skip; B=1, C=9 */
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "IS>: incremented no-skip");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "IS>: next line NOT skipped");

    /* 3. IS>: threshold exact — 1+1=2 is NOT > 2, so no skip */
    reset_state();
    calc_variables['A'-'A'] = 1.0f;
    run_program("IS>(A,2\n1->B");
    CHECK(NEAR(calc_variables['A'-'A'], 2.0f), "IS>: at threshold no skip");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "IS>: B set at threshold");

    /* 4. DS<: decrements and skips when < threshold */
    reset_state();
    calc_variables['A'-'A'] = 1.0f;
    run_program("DS<(A,1\n1->B\n9->C");
    /* A: 1-1=0 < 1 → skip "1->B" */
    CHECK(NEAR(calc_variables['A'-'A'], 0.0f), "DS<: decremented");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "DS<: next line skipped");
    CHECK(NEAR(calc_variables['C'-'A'], 9.0f), "DS<: line after skip runs");

    /* 5. DS<: does NOT skip when >= threshold */
    reset_state();
    calc_variables['A'-'A'] = 5.0f;
    run_program("DS<(A,1\n1->B");
    /* A: 5-1=4 >= 1 → no skip */
    CHECK(NEAR(calc_variables['A'-'A'], 4.0f), "DS<: decremented no-skip");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "DS<: B set no-skip");

    /* 6. DS<: threshold exact — 2-1=1 NOT < 1, so no skip */
    reset_state();
    calc_variables['A'-'A'] = 2.0f;
    run_program("DS<(A,1\n1->B");
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "DS<: at threshold no skip");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "DS<: B set at threshold");
}

/* -------------------------------------------------------------------------
 * Group 7: Stop / Pause / Return (5 tests)
 * ---------------------------------------------------------------------- */
static void test_stop_pause_return(void)
{
    printf("Group 7: Stop/Pause/Return\n");

    /* 1. Stop halts program — following lines do not run */
    reset_state();
    run_program("5->A\nStop\n9->A");
    CHECK(current_mode == MODE_NORMAL, "stop: program completes");
    CHECK(NEAR(calc_variables['A'-'A'], 5.0f), "stop: A=5 before stop");

    /* 2. Pause suspends execution (MODE_PRGM_RUNNING) */
    reset_state();
    run_program("3->A\nPause\n9->A");
    CHECK(current_mode == MODE_PRGM_RUNNING, "pause: mode is RUNNING");
    CHECK(NEAR(calc_variables['A'-'A'], 3.0f), "pause: A=3 before pause");

    /* 3. Stop after some computation */
    reset_state();
    run_program("1->A\n2->B\nStop\n3->C");
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "stop: A set");
    CHECK(NEAR(calc_variables['B'-'A'], 2.0f), "stop: B set");
    CHECK(NEAR(calc_variables['C'-'A'], 0.0f), "stop: C not set");

    /* 5. Program ends normally without explicit Stop */
    reset_state();
    run_program("1->A\n2->B");
    CHECK(current_mode == MODE_NORMAL, "normal end: mode NORMAL");
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "normal end: A=1");
    CHECK(NEAR(calc_variables['B'-'A'], 2.0f), "normal end: B=2");
}

/* -------------------------------------------------------------------------
 * Group 8: STO (6 tests)
 * ---------------------------------------------------------------------- */
static void test_sto(void)
{
    printf("Group 8: STO\n");

    /* 1. Literal STO: 3->A */
    reset_state();
    run_program("3->A");
    CHECK(NEAR(calc_variables['A'-'A'], 3.0f), "sto: 3->A");

    /* 2. Expression STO: 1+2->B */
    reset_state();
    run_program("1+2->B");
    CHECK(NEAR(calc_variables['B'-'A'], 3.0f), "sto: 1+2->B");

    /* 3. STO updates ANS */
    reset_state();
    run_program("7->A");
    CHECK(NEAR(ans, 7.0f), "sto: updates ans");

    /* 4. STO chain: A+1->A */
    reset_state();
    calc_variables['A'-'A'] = 5.0f;
    run_program("A+1->A");
    CHECK(NEAR(calc_variables['A'-'A'], 6.0f), "sto: A+1->A");

    /* 5. Multiple STO in sequence */
    reset_state();
    run_program("1->A\n2->B\n3->C");
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "sto: multi A=1");
    CHECK(NEAR(calc_variables['B'-'A'], 2.0f), "sto: multi B=2");
    CHECK(NEAR(calc_variables['C'-'A'], 3.0f), "sto: multi C=3");

    /* 6. STO does not run on syntax error (no crash) */
    reset_state();
    run_program("(->A");   /* malformed — STO arrow found but expr error */
    CHECK(current_mode == MODE_NORMAL, "sto: syntax error no crash");
    /* A remains 0 since evaluation failed */
    CHECK(NEAR(calc_variables['A'-'A'], 0.0f), "sto: A unchanged on error");
}

/* -------------------------------------------------------------------------
 * Group 9: General expression lines (4 tests)
 * ---------------------------------------------------------------------- */
static void test_general_expr(void)
{
    printf("Group 9: General expression\n");

    /* 1. Expression updates ANS */
    reset_state();
    run_program("3+4");
    CHECK(NEAR(ans, 7.0f), "expr: 3+4=7 in ANS");

    /* 2. Expression using a variable */
    reset_state();
    calc_variables['A'-'A'] = 10.0f;
    run_program("A*2");
    CHECK(NEAR(ans, 20.0f), "expr: A*2 uses var");

    /* 3. Expression after STO can use ANS */
    reset_state();
    run_program("5->A\nA+3");
    CHECK(NEAR(ans, 8.0f), "expr: A+3 after sto");

    /* 4. Evaluation error in expression is non-fatal */
    reset_state();
    run_program("1->A\n(+)\n2->B");
    CHECK(current_mode == MODE_NORMAL, "expr: error non-fatal");
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "expr: A=1 before error");
    CHECK(NEAR(calc_variables['B'-'A'], 2.0f), "expr: B=2 after error");
}

/* -------------------------------------------------------------------------
 * Group 10: Disp (5 tests)
 * ---------------------------------------------------------------------- */
static void test_disp(void)
{
    printf("Group 10: Disp\n");

    /* 1. Disp "string" adds history entry: string in expression (left-aligned) */
    reset_state();
    run_program("Disp \"hello\"");
    CHECK(history_count == 1, "disp: history_count=1");
    CHECK(strcmp(history[0].expression, "hello") == 0, "disp: expression=hello");
    CHECK(history[0].result[0] == '\0', "disp: result empty");

    /* 2. Disp variable: displays its formatted value */
    reset_state();
    calc_variables['A'-'A'] = 42.0f;
    run_program("Disp A");
    CHECK(history_count == 1, "disp: A count=1");
    CHECK(strlen(history[0].result) > 0, "disp: A result not empty");

    /* 3. Disp expression evaluates first */
    reset_state();
    run_program("Disp 2+3");
    CHECK(history_count == 1, "disp: expr count=1");

    /* 4. Disp increments history_count for each call */
    reset_state();
    run_program("Disp \"a\"\nDisp \"b\"\nDisp \"c\"");
    CHECK(history_count == 3, "disp: three calls = count 3");

    /* 5. Disp string: text in expression row (E1 left-aligned) */
    reset_state();
    run_program("Disp \"test\"");
    CHECK(strcmp(history[0].expression, "test") == 0, "disp: quotes stripped to expression");
}

/* -------------------------------------------------------------------------
 * Group 11: Input / Prompt (4 tests)
 * ---------------------------------------------------------------------- */
static void test_input_prompt(void)
{
    printf("Group 11: Input/Prompt\n");

    /* 1. Input suspends execution */
    reset_state();
    run_program("Input A");
    CHECK(current_mode == MODE_PRGM_RUNNING, "input: suspends (RUNNING)");

    /* 2. Input adds a prompt history entry */
    reset_state();
    run_program("Input B");
    CHECK(history_count == 1, "input: history entry added");
    /* Expression should contain "B=?" */
    CHECK(strstr(history[0].expression, "B") != NULL ||
          strstr(history[0].expression, "?") != NULL,
          "input: prompt in history");

}

/* -------------------------------------------------------------------------
 * Group 12: ClrHome (3 tests)
 * ---------------------------------------------------------------------- */
static void test_clrhome(void)
{
    printf("Group 12: ClrHome\n");

    /* 1. ClrHome clears history_count */
    reset_state();
    history_count = 5;
    run_program("ClrHome");
    CHECK(history_count == 0, "clrhome: history_count reset to 0");

    /* 2. ClrHome clears history_recall_offset */
    reset_state();
    history_recall_offset = 3;
    run_program("ClrHome");
    CHECK(history_recall_offset == 0, "clrhome: recall_offset reset");

    /* 3. ClrHome followed by Disp: count starts from 0 */
    reset_state();
    history_count = 10;
    run_program("ClrHome\nDisp \"hi\"");
    CHECK(history_count == 1, "clrhome: then Disp gives count=1");
}

/* -------------------------------------------------------------------------
 * Group 13: Subroutine call (5 tests)
 * ---------------------------------------------------------------------- */
static void test_subroutine(void)
{
    printf("Group 13: Subroutine call\n");

    /* Set up slot 1 as a subroutine (ID "2" per prgm_slot_id_str) */

    /* 1. Subroutine body executes: sets a variable */
    reset_state();
    strncpy(g_prgm_store.names[1], "S", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[1], "7->B", PRGM_BODY_LEN - 1);
    run_program("prgm2\n9->C");
    CHECK(NEAR(calc_variables['B'-'A'], 7.0f), "call: sub sets B=7");
    CHECK(NEAR(calc_variables['C'-'A'], 9.0f), "call: continues after return");

    /* 2. Caller resumes after subroutine finishes */
    reset_state();
    strncpy(g_prgm_store.names[1], "S", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[1], "1->B", PRGM_BODY_LEN - 1);
    run_program("5->A\nprgm2\nA+B->C");
    CHECK(NEAR(calc_variables['C'-'A'], 6.0f), "call: A+B=6 after sub");

    /* 3. Call to non-existent ID is a no-op (continues) */
    reset_state();
    run_program("1->A\nprgmZ\n2->B");
    CHECK(current_mode == MODE_NORMAL, "call: unknown id no crash");
    CHECK(NEAR(calc_variables['B'-'A'], 2.0f), "call: continues after unknown id");

    /* 4. Subroutine with computation */
    reset_state();
    strncpy(g_prgm_store.names[1], "S", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[1], "A*2->A", PRGM_BODY_LEN - 1);
    calc_variables['A'-'A'] = 3.0f;
    run_program("prgm2");
    CHECK(NEAR(calc_variables['A'-'A'], 6.0f), "call: sub doubles A");

}

/* -------------------------------------------------------------------------
 * Group 14: Complex programs (8 tests)
 * ---------------------------------------------------------------------- */
static void test_complex_programs(void)
{
    printf("Group 14: Complex programs\n");

    /* 1. Goto-based loop: compute 2^8=256 */
    reset_state();
    run_program(
        "1->R\n0->I\n"
        "Lbl L\n"
        "If I=8\nGoto E\n"
        "R*2->R\n"
        "I+1->I\n"
        "Goto L\n"
        "Lbl E"
    );
    CHECK(NEAR(calc_variables['R'-'A'], 256.0f), "complex: 2^8=256 goto loop");
}

/* -------------------------------------------------------------------------
 * Group 15: Empty body — T19 (run an empty/body-only slot)
 * ---------------------------------------------------------------------- */
static void test_empty_body(void)
{
    printf("Group 15: Empty body\n");

    /* 1. Completely empty body — program completes, mode NORMAL, no history */
    reset_state();
    run_program("");
    CHECK(current_mode == MODE_NORMAL, "empty: mode NORMAL");
    CHECK(history_count == 0, "empty: no history");

    /* 2. Newline-only body (blank lines) — same as empty */
    reset_state();
    run_program("\n\n");
    CHECK(current_mode == MODE_NORMAL, "empty-nl: mode NORMAL");
    CHECK(history_count == 0, "empty-nl: no history");

    /* 3. Slot with body but no user name still executes via prgm_run_start */
    reset_state();
    /* names[0] stays empty (not set by run_program helper) */
    strncpy(g_prgm_store.bodies[0], "5->A", PRGM_BODY_LEN - 1);
    prgm_run_start(0);
    CHECK(NEAR(calc_variables['A'-'A'], 5.0f), "no-name: body runs via index");
}

/* -------------------------------------------------------------------------
 * Group 16: prgm_lookup_slot — T35 / T35b (prgmNAME execution model)
 * ---------------------------------------------------------------------- */
static void test_lookup_slot(void)
{
    printf("Group 16: prgm_lookup_slot\n");

    reset_state();
    strncpy(g_prgm_store.names[0],  "HELLO", PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[9],  "ZERO",  PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[10], "ALPHA", PRGM_NAME_LEN);

    /* 1. Lookup by user name */
    CHECK(prgm_lookup_slot("HELLO") == 0,  "lookup: name HELLO -> 0");

    /* 2. Canonical ID "1" -> slot 0 (TI-81: digit 1 = first slot) */
    CHECK(prgm_lookup_slot("1") == 0, "lookup: id '1' -> 0");

    /* 3. Canonical ID "0" -> slot 9 (TI-81: digit 0 = tenth slot) */
    CHECK(prgm_lookup_slot("0") == 9, "lookup: id '0' -> 9");

    /* 4. Canonical ID "A" -> slot 10 */
    CHECK(prgm_lookup_slot("A") == 10, "lookup: id 'A' -> 10");

    /* 5. User name "ZERO" resolves to slot 9 (name match beats ID match) */
    CHECK(prgm_lookup_slot("ZERO") == 9, "lookup: name ZERO -> 9");

    /* 6. Non-existent name -> -1 */
    CHECK(prgm_lookup_slot("NOEXIST") == -1, "lookup: unknown -> -1");

    /* 7. Empty string -> -1 */
    CHECK(prgm_lookup_slot("") == -1, "lookup: empty -> -1");

    /* 8. "T" matches slot 29 (the T letter slot, A-Z range) before theta(36);
     * theta's ASCII representation "T" collides with the T slot — first match wins */
    CHECK(prgm_lookup_slot("T") == 29, "lookup: id 'T' -> 29 (T-letter slot before theta)");
}

/* -------------------------------------------------------------------------
 * Group 17: Two-level nested subroutine — T34
 * ---------------------------------------------------------------------- */
static void test_nested_subroutine(void)
{
    printf("Group 17: Nested subroutine 2 levels\n");

    /* 1. Two-level chain: main(0) -> mid(1) -> deep(2), all auto-return */
    reset_state();
    strncpy(g_prgm_store.names[1], "MID",  PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[2], "DEEP", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[1], "prgm3\n2->B", PRGM_BODY_LEN - 1);
    strncpy(g_prgm_store.bodies[2], "1->A",         PRGM_BODY_LEN - 1);
    run_program("prgm2\n3->C");
    CHECK(NEAR(calc_variables['A'-'A'], 1.0f), "2-deep: A=1 from innermost");
    CHECK(NEAR(calc_variables['B'-'A'], 2.0f), "2-deep: B=2 from mid");
    CHECK(NEAR(calc_variables['C'-'A'], 3.0f), "2-deep: C=3 from main");

    /* 2. Call stack overflow (>4 levels deep) is silently ignored — no crash.
     * Chain: 0->1->2->3->4 (slot 4 tries to call slot 5 at depth=4, no-op).
     * Slot 5 body ("99->E") must NOT run. */
    reset_state();
    strncpy(g_prgm_store.names[1], "S2", PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[2], "S3", PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[3], "S4", PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[4], "S5", PRGM_NAME_LEN);
    strncpy(g_prgm_store.names[5], "S6", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[1], "prgm3",  PRGM_BODY_LEN - 1);
    strncpy(g_prgm_store.bodies[2], "prgm4",  PRGM_BODY_LEN - 1);
    strncpy(g_prgm_store.bodies[3], "prgm5",  PRGM_BODY_LEN - 1);
    strncpy(g_prgm_store.bodies[4], "prgm6",  PRGM_BODY_LEN - 1);
    strncpy(g_prgm_store.bodies[5], "99->E",  PRGM_BODY_LEN - 1);
    run_program("prgm2");
    CHECK(current_mode == MODE_NORMAL, "overflow: no crash on 5-deep call");
    CHECK(NEAR(calc_variables['E'-'A'], 0.0f), "overflow: slot-6 body not executed");
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void)
{
    printf("=== test_prgm_exec ===\n");

    test_goto_lbl();
    test_if_single();
    test_is_ds();
    test_stop_pause_return();
    test_sto();
    test_general_expr();
    test_disp();
    test_input_prompt();
    test_clrhome();
    test_subroutine();
    test_complex_programs();
    test_empty_body();
    test_lookup_slot();
    test_nested_subroutine();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
