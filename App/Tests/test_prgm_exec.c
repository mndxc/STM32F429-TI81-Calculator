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

char    expression[MAX_EXPR_LEN];
uint8_t expr_len  = 0;
uint8_t cursor_pos = 0;

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
    memset(expression, 0, sizeof(expression));
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
 * Group 3: If/Then/Else/End (7 tests)
 * ---------------------------------------------------------------------- */
static void test_if_then_else(void)
{
    printf("Group 3: If/Then/Else/End\n");

    /* 1. Condition true: Then-body executes, Else skipped */
    reset_state();
    run_program("1->A\nIf A\nThen\n5->B\nElse\n9->B\nEnd\n10->C");
    CHECK(NEAR(calc_variables['B'-'A'], 5.0f), "if-then-else: true picks Then");
    CHECK(NEAR(calc_variables['C'-'A'], 10.0f), "if-then-else: continues after End");

    /* 2. Condition false: Else-body executes, Then skipped */
    reset_state();
    run_program("0->A\nIf A\nThen\n5->B\nElse\n9->B\nEnd");
    CHECK(NEAR(calc_variables['B'-'A'], 9.0f), "if-then-else: false picks Else");

    /* 3. Condition true, no Else */
    reset_state();
    run_program("1->A\nIf A\nThen\n5->B\nEnd\n10->C");
    CHECK(NEAR(calc_variables['B'-'A'], 5.0f), "if-then: true body runs");
    CHECK(NEAR(calc_variables['C'-'A'], 10.0f), "if-then: continues after End");

    /* 4. Condition false, no Else: body skipped */
    reset_state();
    run_program("0->A\nIf A\nThen\n5->B\nEnd\n10->C");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "if-then: false body skipped");
    CHECK(NEAR(calc_variables['C'-'A'], 10.0f), "if-then: continues after End");

    /* 5. Nested If true/true */
    reset_state();
    run_program("1->A\n1->B\nIf A\nThen\nIf B\nThen\n5->C\nEnd\nEnd");
    CHECK(NEAR(calc_variables['C'-'A'], 5.0f), "if-nested: both true");

    /* 6. Nested If true/false */
    reset_state();
    run_program("1->A\n0->B\nIf A\nThen\nIf B\nThen\n5->C\nEnd\n7->D\nEnd");
    CHECK(NEAR(calc_variables['C'-'A'], 0.0f), "if-nested: inner false skips");
    CHECK(NEAR(calc_variables['D'-'A'], 7.0f), "if-nested: outer continues");

    /* 7. Multiple statements in Then */
    reset_state();
    run_program("1->A\nIf A\nThen\n1->B\n2->C\n3->D\nEnd");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "if-then: multi-stmt 1");
    CHECK(NEAR(calc_variables['C'-'A'], 2.0f), "if-then: multi-stmt 2");
    CHECK(NEAR(calc_variables['D'-'A'], 3.0f), "if-then: multi-stmt 3");
}

/* -------------------------------------------------------------------------
 * Group 4: While (5 tests)
 * ---------------------------------------------------------------------- */
static void test_while(void)
{
    printf("Group 4: While\n");

    /* 1. While false on first check: body not executed */
    reset_state();
    run_program("0->A\nWhile A\n5->B\nEnd");
    CHECK(NEAR(calc_variables['B'-'A'], 0.0f), "while: false body not run");

    /* 2. While loop executes correct number of times */
    reset_state();
    run_program("0->A\nWhile A<3\nA+1->A\nEnd");
    CHECK(NEAR(calc_variables['A'-'A'], 3.0f), "while: loop 3 times");

    /* 3. While accumulates sum */
    reset_state();
    run_program("1->I\n0->S\nWhile I<6\nS+I->S\nI+1->I\nEnd");
    CHECK(NEAR(calc_variables['S'-'A'], 15.0f), "while: sum 1..5=15");

    /* 4. While updates variable and stops */
    reset_state();
    run_program("10->A\nWhile A>0\nA-3->A\nEnd\n9->B");
    /* 10 → 7 → 4 → 1 → -2 (stops: -2 > 0 is false) */
    CHECK(NEAR(calc_variables['A'-'A'], -2.0f), "while: stops at negative");
    CHECK(NEAR(calc_variables['B'-'A'], 9.0f), "while: continues after End");

    /* 5. While single iteration */
    reset_state();
    run_program("1->A\nWhile A\n0->A\nEnd\n5->B");
    CHECK(NEAR(calc_variables['A'-'A'], 0.0f), "while: single iter");
    CHECK(NEAR(calc_variables['B'-'A'], 5.0f), "while: after End");
}

/* -------------------------------------------------------------------------
 * Group 5: For (7 tests)
 * ---------------------------------------------------------------------- */
static void test_for(void)
{
    printf("Group 5: For\n");

    /* 1. Basic For 1 to 5 step 1 */
    reset_state();
    run_program("0->S\nFor(I,1,5,1)\nS+I->S\nEnd");
    CHECK(NEAR(calc_variables['S'-'A'], 15.0f), "for: sum 1..5=15");

    /* 2. For with step 2 */
    reset_state();
    run_program("0->S\nFor(I,1,9,2)\nS+1->S\nEnd");
    /* I = 1,3,5,7,9 → 5 iterations */
    CHECK(NEAR(calc_variables['S'-'A'], 5.0f), "for: step 2 count=5");

    /* 3. For with negative step */
    reset_state();
    run_program("0->S\nFor(I,5,1,-1)\nS+1->S\nEnd");
    CHECK(NEAR(calc_variables['S'-'A'], 5.0f), "for: neg step 5 iters");

    /* 4. For: loop variable has correct value after loop */
    reset_state();
    run_program("For(I,1,3,1)\nEnd");
    /* After For(1,3,1): loop runs I=1,2,3; End increments to 4 which > 3 → done */
    CHECK(calc_variables['I'-'A'] > 3.0f, "for: var past limit after loop");

    /* 5. For: begin == end executes once */
    reset_state();
    run_program("0->S\nFor(I,5,5,1)\nS+1->S\nEnd");
    CHECK(NEAR(calc_variables['S'-'A'], 1.0f), "for: begin==end one iteration");

    /* 6. For: begin > end with positive step skips body */
    reset_state();
    run_program("0->S\nFor(I,10,1,1)\nS+1->S\nEnd");
    /* I is set to begin_v=10 then body is skipped */
    CHECK(NEAR(calc_variables['S'-'A'], 0.0f), "for: begin>end pos step skips");
    CHECK(NEAR(calc_variables['I'-'A'], 10.0f), "for: I set to begin even when skipped");

    /* 7. For continues after End */
    reset_state();
    run_program("For(I,1,3,1)\nEnd\n99->R");
    CHECK(NEAR(calc_variables['R'-'A'], 99.0f), "for: execution continues after loop");
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

    /* 3. Return from empty call stack ends program */
    reset_state();
    run_program("5->A\nReturn\n9->A");
    CHECK(current_mode == MODE_NORMAL, "return: ends on empty call stack");
    CHECK(NEAR(calc_variables['A'-'A'], 5.0f), "return: A=5");

    /* 4. Stop after some computation */
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

    /* 1. Disp "string" adds history entry with empty expression */
    reset_state();
    run_program("Disp \"hello\"");
    CHECK(history_count == 1, "disp: history_count=1");
    CHECK(strcmp(history[0].result, "hello") == 0, "disp: result=hello");
    CHECK(history[0].expression[0] == '\0', "disp: expression empty");

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

    /* 5. Disp string with trailing quote stripped correctly */
    reset_state();
    run_program("Disp \"test\"");
    CHECK(strcmp(history[0].result, "test") == 0, "disp: quotes stripped");
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

    /* 3. Prompt suspends execution */
    reset_state();
    run_program("Prompt C");
    CHECK(current_mode == MODE_PRGM_RUNNING, "prompt: suspends (RUNNING)");

    /* 4. Prompt adds a prompt history entry */
    reset_state();
    run_program("Prompt D");
    CHECK(history_count == 1, "prompt: history entry added");
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

    /* 5. Subroutine Return exits back to caller */
    reset_state();
    strncpy(g_prgm_store.names[1], "S", PRGM_NAME_LEN);
    strncpy(g_prgm_store.bodies[1], "1->B\nReturn\n99->B", PRGM_BODY_LEN - 1);
    run_program("prgm2\n5->C");
    CHECK(NEAR(calc_variables['B'-'A'], 1.0f), "call: Return stops sub");
    CHECK(NEAR(calc_variables['C'-'A'], 5.0f), "call: caller resumes after Return");
}

/* -------------------------------------------------------------------------
 * Group 14: Complex programs (8 tests)
 * ---------------------------------------------------------------------- */
static void test_complex_programs(void)
{
    printf("Group 14: Complex programs\n");

    /* 1. Compute factorial 5! = 120 using For */
    reset_state();
    run_program("1->R\nFor(I,1,5,1)\nR*I->R\nEnd");
    CHECK(NEAR(calc_variables['R'-'A'], 120.0f), "complex: 5! = 120");

    /* 2. Fibonacci: F(7) = 13 */
    reset_state();
    run_program(
        "1->A\n1->B\nFor(I,3,7,1)\nA+B->C\nB->A\nC->B\nEnd"
    );
    CHECK(NEAR(calc_variables['B'-'A'], 13.0f), "complex: fib(7)=13");

    /* 3. Count down from 5 to 0 using While */
    reset_state();
    run_program("5->A\n0->S\nWhile A>0\nS+1->S\nA-1->A\nEnd");
    CHECK(NEAR(calc_variables['S'-'A'], 5.0f), "complex: countdown 5 iters");
    CHECK(NEAR(calc_variables['A'-'A'], 0.0f), "complex: A=0 at end");

    /* 4. Sum of squares 1..4 = 30 */
    reset_state();
    run_program("0->S\nFor(I,1,4,1)\nS+I*I->S\nEnd");
    CHECK(NEAR(calc_variables['S'-'A'], 30.0f), "complex: sum sq 1..4=30");

    /* 5. If inside While: collect even sum 2+4+6+8+10=30 */
    reset_state();
    run_program(
        "1->I\n0->S\nWhile I<11\n"
        "If I/2=int(I/2)\nThen\nS+I->S\nEnd\n"
        "I+1->I\nEnd"
    );
    CHECK(NEAR(calc_variables['S'-'A'], 30.0f), "complex: even sum=30");

    /* 6. Goto-based loop: compute 2^8=256 */
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

    /* 7. Nested For: multiplication table sum */
    reset_state();
    run_program(
        "0->S\n"
        "For(I,1,3,1)\n"
        "For(J,1,3,1)\n"
        "S+I*J->S\n"
        "End\n"
        "End"
    );
    /* sum i*j for i,j in 1..3: (1+2+3)*(1+2+3) = 36 */
    CHECK(NEAR(calc_variables['S'-'A'], 36.0f), "complex: nested for sum=36");

    /* 8. Program with multiple command types */
    reset_state();
    run_program(
        "3->A\n"
        "2->B\n"
        "If A>B\nThen\nA-B->C\nElse\nB-A->C\nEnd\n"
        "C*2->D\n"
        "Disp \"done\""
    );
    CHECK(NEAR(calc_variables['C'-'A'], 1.0f), "complex: multi |A-B|=1");
    CHECK(NEAR(calc_variables['D'-'A'], 2.0f), "complex: multi D=2");
    CHECK(history_count == 1, "complex: Disp adds history");
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void)
{
    printf("=== test_prgm_exec ===\n");

    test_goto_lbl();
    test_if_single();
    test_if_then_else();
    test_while();
    test_for();
    test_is_ds();
    test_stop_pause_return();
    test_sto();
    test_general_expr();
    test_disp();
    test_input_prompt();
    test_clrhome();
    test_subroutine();
    test_complex_programs();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
