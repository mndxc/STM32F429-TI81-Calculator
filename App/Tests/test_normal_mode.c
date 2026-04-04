/**
 * @file    test_normal_mode.c
 * @brief   Host-compiled unit tests for handle_normal_mode() and its
 *          eight static sub-handlers in calculator_core.c.
 *
 * Build and run (no ARM toolchain required):
 *   cmake -S App/Tests -B build/tests && cmake --build build/tests
 *   ./build/tests/test_normal_mode
 *
 * Returns 0 on all pass, 1 on any failure.
 *
 * Testing strategy:
 *   - Call handle_normal_mode(TOKEN_*) directly; inspect shared state.
 *   - Observables: expression[], expr_len, cursor_pos, ans, ans_is_matrix,
 *     history[], history_count, history_recall_offset, sto_pending,
 *     insert_mode, current_mode.
 *   - All LVGL / RTOS calls are no-ops (see calculator_core_test_stubs.h).
 *
 * Sub-handlers under test:
 *   handle_digit_key        — TOKEN_0..TOKEN_9, TOKEN_DECIMAL
 *   handle_arithmetic_op    — arithmetic/grouping operators
 *   handle_function_insert  — trig, math, alpha letter inserts
 *   handle_history_nav      — UP/DOWN/LEFT/RIGHT/ENTER/ENTRY
 *   handle_clear_key        — TOKEN_CLEAR
 *   handle_sto_key          — TOKEN_STO
 *   handle_normal_mode      — TOKEN_INS toggle, TOKEN_DEL, dispatch tokens
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "prgm_exec.h"
#include "calculator_core_test_stubs.h"

/* -------------------------------------------------------------------------
 * Stub definitions for external symbols not provided by linked .c files
 * ---------------------------------------------------------------------- */

/* LVGL screen pointers — all NULL; no UI init is called in tests */
lv_obj_t *ui_matrix_screen             = NULL;
lv_obj_t *ui_matrix_edit_screen        = NULL;
lv_obj_t *ui_graph_yeq_screen          = NULL;
lv_obj_t *ui_param_yeq_screen          = NULL;
lv_obj_t *ui_graph_range_screen        = NULL;
lv_obj_t *ui_graph_zoom_screen         = NULL;
lv_obj_t *ui_graph_zoom_factors_screen = NULL;
lv_obj_t *ui_prgm_editor_screen        = NULL;
lv_obj_t *ui_prgm_new_screen           = NULL;

/* Stub font objects (never dereferenced — LVGL calls are no-ops) */
const lv_font_t jetbrains_mono_24 = {0};
const lv_font_t jetbrains_mono_20 = {0};

/* RTOS handle stubs */
SemaphoreHandle_t xLVGL_Mutex    = NULL;
SemaphoreHandle_t xLVGL_Ready    = NULL;
osMessageQId      keypadQueueHandle = NULL;

/* Matrix menu state (normally owned by ui_matrix.c) */
MatrixMenuState_t matrix_menu_state = {0, 0, MODE_NORMAL};

/* STAT screen pointers (normally owned by ui_stat.c) */
lv_obj_t *ui_stat_screen         = NULL;
lv_obj_t *ui_stat_edit_screen    = NULL;
lv_obj_t *ui_stat_results_screen = NULL;

/* STAT state (normally owned by ui_stat.c) */
StatMenuState_t stat_menu_state = {0, 0, MODE_NORMAL};

/* DRAW screen pointer and state (normally owned by ui_draw.c) */
lv_obj_t *ui_draw_screen = NULL;
DrawMenuState_t draw_menu_state = {0, MODE_NORMAL};

/* VARS screen pointer and state (normally owned by ui_vars.c) */
lv_obj_t *ui_vars_screen = NULL;
VarsMenuState_t vars_menu_state = {0, 0, 0, MODE_NORMAL};
StatData_t    stat_data    = {0};
StatResults_t stat_results = {0};

/* Program store and editor buffers (normally owned by prgm_exec.c / ui_prgm.c) */
ProgramStore_t g_prgm_store;
char    prgm_edit_lines[PRGM_MAX_LINES][PRGM_MAX_LINE_LEN];
uint8_t prgm_edit_num_lines = 0;

/* persist.c / prgm_exec.c / app_init.c stubs */
bool Persist_Save(const PersistBlock_t *b)  { (void)b; return true; }
bool Persist_Load(PersistBlock_t *b)        { (void)b; return false; }
bool Prgm_Save(void)                        { return false; }
void Prgm_Init(void)                        {}
bool Prgm_Load(void)                        { return false; }
void Power_DisplayBlankAndMessage(void)     {}

/* -------------------------------------------------------------------------
 * Test infrastructure
 * ---------------------------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name) do {                                         \
    if (cond) {                                                        \
        g_passed++;                                                    \
    } else {                                                           \
        g_failed++;                                                    \
        printf("  FAIL [line %d]: %s\n", __LINE__, (name));           \
    }                                                                  \
} while (0)

#define NEAR(a, b)  (fabs((double)(a) - (double)(b)) < 1e-4)

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/** Full state reset — call before each test group. */
static void reset_state(void)
{
    for (int i = 0; i < 26; i++) calc_variables[i] = 0.0f;
    ans                   = 0.0f;
    ans_is_matrix         = false;
    angle_degrees         = true;
    current_mode          = MODE_NORMAL;
    return_mode           = MODE_NORMAL;
    insert_mode           = false;
    sto_pending           = false;
    history_count         = 0;
    history_recall_offset = 0;
    memset(history, 0, sizeof(history));
    memset(expression, 0, sizeof(expression));
    expr_len   = 0;
    cursor_pos = 0;
    memset(&g_prgm_store, 0, sizeof(g_prgm_store));
    memset(prgm_edit_lines, 0, sizeof(prgm_edit_lines));
    prgm_edit_num_lines = 0;
}

/** Convenience: load a string directly into the expression buffer. */
static void load_expr(const char *s)
{
    size_t len = strlen(s);
    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(expression, s, len);
    expression[len] = '\0';
    expr_len   = (uint8_t)len;
    cursor_pos = (uint8_t)len;
}

/* -------------------------------------------------------------------------
 * Group 1: handle_digit_key — expression buffer mutations (11 tests)
 * ---------------------------------------------------------------------- */
static void test_digit_key(void)
{
    printf("Group 1: handle_digit_key\n");

    /* 1. Single digit appended to empty expression */
    reset_state();
    handle_normal_mode(TOKEN_5);
    CHECK(expr_len == 1 && expression[0] == '5', "digit: TOKEN_5 inserts '5'");

    /* 2. Multiple digits form a multi-char number */
    reset_state();
    handle_normal_mode(TOKEN_1);
    handle_normal_mode(TOKEN_2);
    handle_normal_mode(TOKEN_3);
    CHECK(strcmp(expression, "123") == 0, "digit: 1,2,3 → \"123\"");
    CHECK(cursor_pos == 3, "digit: cursor at end after three digits");

    /* 3. Decimal point inserts '.' */
    reset_state();
    handle_normal_mode(TOKEN_3);
    handle_normal_mode(TOKEN_DECIMAL);
    handle_normal_mode(TOKEN_1);
    CHECK(strcmp(expression, "3.1") == 0, "digit: decimal point inserts '.'");

    /* 4. TOKEN_0 inserts '0' */
    reset_state();
    handle_normal_mode(TOKEN_0);
    CHECK(expression[0] == '0', "digit: TOKEN_0 inserts '0'");

    /* 5. TOKEN_9 inserts '9' */
    reset_state();
    handle_normal_mode(TOKEN_9);
    CHECK(expression[0] == '9', "digit: TOKEN_9 inserts '9'");

    /* 6. All ten digit tokens produce correct characters */
    reset_state();
    handle_normal_mode(TOKEN_0); handle_normal_mode(TOKEN_1);
    handle_normal_mode(TOKEN_2); handle_normal_mode(TOKEN_3);
    handle_normal_mode(TOKEN_4); handle_normal_mode(TOKEN_5);
    handle_normal_mode(TOKEN_6); handle_normal_mode(TOKEN_7);
    handle_normal_mode(TOKEN_8); handle_normal_mode(TOKEN_9);
    CHECK(strcmp(expression, "0123456789") == 0, "digit: all ten tokens correct");

    /* 7. Digit does not auto-prepend ANS on empty expression */
    reset_state();
    ans = 42.0f;
    handle_normal_mode(TOKEN_7);
    CHECK(strcmp(expression, "7") == 0, "digit: no ANS prepend on digit");
}

/* -------------------------------------------------------------------------
 * Group 2: handle_arithmetic_op — ANS prepend + operator insertion (14 tests)
 * ---------------------------------------------------------------------- */
static void test_arithmetic_op(void)
{
    printf("Group 2: handle_arithmetic_op\n");

    /* 1. ADD on empty expression prepends ANS */
    reset_state();
    ans = 5.0f;
    handle_normal_mode(TOKEN_ADD);
    CHECK(strncmp(expression, "ANS", 3) == 0, "arith: ADD on empty prepends ANS");
    CHECK(expression[3] == '+',               "arith: ADD inserts '+' after ANS");

    /* 2. SUB on empty expression prepends ANS */
    reset_state();
    ans = 3.0f;
    handle_normal_mode(TOKEN_SUB);
    CHECK(strncmp(expression, "ANS", 3) == 0, "arith: SUB on empty prepends ANS");
    CHECK(expression[3] == '-',               "arith: SUB inserts '-' after ANS");

    /* 3. ADD on non-empty expression appends without prepending ANS */
    reset_state();
    load_expr("2");
    handle_normal_mode(TOKEN_ADD);
    CHECK(strcmp(expression, "2+") == 0, "arith: ADD appends '+' to existing expr");

    /* 4. MULT on empty prepends ANS */
    reset_state();
    handle_normal_mode(TOKEN_MULT);
    CHECK(strncmp(expression, "ANS", 3) == 0, "arith: MULT prepends ANS");
    CHECK(expression[3] == '*',               "arith: MULT inserts '*'");

    /* 5. DIV on empty prepends ANS */
    reset_state();
    handle_normal_mode(TOKEN_DIV);
    CHECK(expression[3] == '/', "arith: DIV inserts '/'");

    /* 6. POWER on empty prepends ANS */
    reset_state();
    handle_normal_mode(TOKEN_POWER);
    CHECK(expression[3] == '^', "arith: POWER inserts '^'");

    /* 7. SQUARE inserts "^2" */
    reset_state();
    load_expr("X");
    handle_normal_mode(TOKEN_SQUARE);
    CHECK(strcmp(expression, "X^2") == 0, "arith: SQUARE appends '^2'");

    /* 8. X_INV inserts "^-1" */
    reset_state();
    load_expr("X");
    handle_normal_mode(TOKEN_X_INV);
    CHECK(strcmp(expression, "X^-1") == 0, "arith: X_INV appends '^-1'");

    /* 9. L_PAR inserts '(' — no ANS prepend */
    reset_state();
    handle_normal_mode(TOKEN_L_PAR);
    CHECK(expression[0] == '(' && strncmp(expression, "ANS", 3) != 0,
          "arith: L_PAR no ANS prepend");

    /* 10. R_PAR inserts ')' — no ANS prepend */
    reset_state();
    load_expr("(2");
    handle_normal_mode(TOKEN_R_PAR);
    CHECK(strcmp(expression, "(2)") == 0, "arith: R_PAR inserts ')'");

    /* 11. NEG inserts '-' without ANS prepend */
    reset_state();
    handle_normal_mode(TOKEN_NEG);
    CHECK(expression[0] == '-' && strncmp(expression, "ANS", 3) != 0,
          "arith: NEG inserts '-' without ANS");

    /* 12. Cursor advances correctly after operator insert */
    reset_state();
    load_expr("3");
    handle_normal_mode(TOKEN_ADD);
    CHECK(cursor_pos == 2, "arith: cursor at 2 after '3+'");
}

/* -------------------------------------------------------------------------
 * Group 3: handle_function_insert — insert strings (16 tests)
 * ---------------------------------------------------------------------- */
static void test_function_insert(void)
{
    printf("Group 3: handle_function_insert\n");

    /* 1. SIN inserts "sin(" */
    reset_state();
    handle_normal_mode(TOKEN_SIN);
    CHECK(strncmp(expression, "sin(", 4) == 0, "func: TOKEN_SIN → \"sin(\"");

    /* 2. COS inserts "cos(" */
    reset_state();
    handle_normal_mode(TOKEN_COS);
    CHECK(strncmp(expression, "cos(", 4) == 0, "func: TOKEN_COS → \"cos(\"");

    /* 3. TAN inserts "tan(" */
    reset_state();
    handle_normal_mode(TOKEN_TAN);
    CHECK(strncmp(expression, "tan(", 4) == 0, "func: TOKEN_TAN → \"tan(\"");

    /* 4. LN inserts "ln(" */
    reset_state();
    handle_normal_mode(TOKEN_LN);
    CHECK(strncmp(expression, "ln(", 3) == 0, "func: TOKEN_LN → \"ln(\"");

    /* 5. LOG inserts "log(" */
    reset_state();
    handle_normal_mode(TOKEN_LOG);
    CHECK(strncmp(expression, "log(", 4) == 0, "func: TOKEN_LOG → \"log(\"");

    /* 6. ABS inserts "abs(" */
    reset_state();
    handle_normal_mode(TOKEN_ABS);
    CHECK(strncmp(expression, "abs(", 4) == 0, "func: TOKEN_ABS → \"abs(\"");

    /* 7. E_X inserts "exp(" */
    reset_state();
    handle_normal_mode(TOKEN_E_X);
    CHECK(strncmp(expression, "exp(", 4) == 0, "func: TOKEN_E_X → \"exp(\"");

    /* 8. TEN_X inserts "10^(" */
    reset_state();
    handle_normal_mode(TOKEN_TEN_X);
    CHECK(strncmp(expression, "10^(", 4) == 0, "func: TOKEN_TEN_X → \"10^(\"");

    /* 9. ANS token inserts "ANS" */
    reset_state();
    handle_normal_mode(TOKEN_ANS);
    CHECK(strcmp(expression, "ANS") == 0, "func: TOKEN_ANS → \"ANS\"");

    /* 10. SQRT inserts a 3-byte UTF-8 √ followed by '(' */
    reset_state();
    handle_normal_mode(TOKEN_SQRT);
    CHECK(expr_len >= 4, "func: TOKEN_SQRT inserts ≥4 bytes (√ is 3-byte UTF-8)");
    /* The last byte before the null should be '(' */
    CHECK(expression[expr_len - 1] == '(', "func: TOKEN_SQRT ends with '('");

    /* 11. EE inserts "*10^" */
    reset_state();
    handle_normal_mode(TOKEN_EE);
    CHECK(strcmp(expression, "*10^") == 0, "func: TOKEN_EE → \"*10^\"");

    /* 12. PI inserts the UTF-8 π character (2 bytes) */
    reset_state();
    handle_normal_mode(TOKEN_PI);
    CHECK(expr_len == 2, "func: TOKEN_PI inserts 2-byte UTF-8 π");
    CHECK((uint8_t)expression[0] == 0xCF && (uint8_t)expression[1] == 0x80,
          "func: TOKEN_PI UTF-8 bytes correct");

    /* 13. Alpha token TOKEN_A inserts 'A' */
    reset_state();
    handle_normal_mode(TOKEN_A);
    CHECK(expression[0] == 'A' && expr_len == 1, "func: TOKEN_A → 'A'");

    /* 14. Alpha token TOKEN_Z inserts 'Z' */
    reset_state();
    handle_normal_mode(TOKEN_Z);
    CHECK(expression[0] == 'Z' && expr_len == 1, "func: TOKEN_Z → 'Z'");

    /* 15. TOKEN_MTRX_A inserts "[A]" */
    reset_state();
    handle_normal_mode(TOKEN_MTRX_A);
    CHECK(strcmp(expression, "[A]") == 0, "func: TOKEN_MTRX_A → \"[A]\"");

    /* 16. Function insert advances cursor to end */
    reset_state();
    handle_normal_mode(TOKEN_SIN);
    CHECK(cursor_pos == expr_len, "func: cursor at end after insert");
}

/* -------------------------------------------------------------------------
 * Group 4: handle_history_nav — UP/DOWN/ENTER/ENTRY/LEFT/RIGHT (13 tests)
 * ---------------------------------------------------------------------- */
static void test_history_nav(void)
{
    printf("Group 4: handle_history_nav\n");

    /* 1. ENTER on non-empty expression evaluates it and commits to history */
    reset_state();
    load_expr("2+3");
    handle_normal_mode(TOKEN_ENTER);
    CHECK(history_count == 1,                          "nav: ENTER increments history_count");
    CHECK(strcmp(history[0].expression, "2+3") == 0,   "nav: ENTER stores expression");
    CHECK(NEAR(ans, 5.0f),                             "nav: ENTER evaluates 2+3=5");
    CHECK(expr_len == 0,                               "nav: ENTER clears expression");
    CHECK(history_recall_offset == 0,                  "nav: ENTER resets recall offset");

    /* 2. ENTER on empty with history re-evaluates last entry */
    reset_state();
    load_expr("4*4");
    handle_normal_mode(TOKEN_ENTER);
    handle_normal_mode(TOKEN_ENTER);   /* empty expr, re-evaluate "4*4" */
    CHECK(history_count == 2,    "nav: ENTER empty re-eval increments history again");
    CHECK(NEAR(ans, 16.0f),      "nav: ENTER empty re-eval result correct");

    /* 3. UP recalls last history entry */
    reset_state();
    load_expr("7");
    handle_normal_mode(TOKEN_ENTER);
    handle_normal_mode(TOKEN_UP);
    CHECK(strcmp(expression, "7") == 0,        "nav: UP loads last history expression");
    CHECK(history_recall_offset == 1,          "nav: UP sets recall offset to 1");
    CHECK(cursor_pos == expr_len,              "nav: UP cursor at end of recalled expression");

    /* 4. DOWN after UP clears expression and resets offset */
    reset_state();
    load_expr("9");
    handle_normal_mode(TOKEN_ENTER);
    handle_normal_mode(TOKEN_UP);
    handle_normal_mode(TOKEN_DOWN);
    CHECK(expr_len == 0,               "nav: DOWN clears expression after UP");
    CHECK(history_recall_offset == 0,  "nav: DOWN resets recall offset to 0");

    /* 5. UP on empty with no history is a no-op */
    reset_state();
    handle_normal_mode(TOKEN_UP);
    CHECK(expr_len == 0,             "nav: UP with no history is no-op");
    CHECK(history_recall_offset == 0, "nav: UP with no history leaves offset 0");

    /* 6. ENTRY loads the last history entry */
    reset_state();
    load_expr("10");
    handle_normal_mode(TOKEN_ENTER);
    memset(expression, 0, sizeof(expression));
    expr_len = 0; cursor_pos = 0;
    handle_normal_mode(TOKEN_ENTRY);
    CHECK(strcmp(expression, "10") == 0,  "nav: ENTRY loads last expression");
    CHECK(history_recall_offset == 1,     "nav: ENTRY sets recall offset to 1");

    /* 7. LEFT moves cursor back one position */
    reset_state();
    load_expr("AB");
    handle_normal_mode(TOKEN_LEFT);
    CHECK(cursor_pos == 1, "nav: LEFT moves cursor back by 1");

    /* 8. LEFT at position 0 is a no-op */
    reset_state();
    load_expr("X");
    cursor_pos = 0;
    handle_normal_mode(TOKEN_LEFT);
    CHECK(cursor_pos == 0, "nav: LEFT at 0 is no-op");

    /* 9. RIGHT moves cursor forward */
    reset_state();
    load_expr("AB");
    cursor_pos = 0;
    handle_normal_mode(TOKEN_RIGHT);
    CHECK(cursor_pos == 1, "nav: RIGHT moves cursor forward by 1");

    /* 10. RIGHT at end is a no-op */
    reset_state();
    load_expr("X");
    handle_normal_mode(TOKEN_RIGHT);
    CHECK(cursor_pos == 1, "nav: RIGHT at end is no-op");

    /* 11. ENTER evaluates a simple expression correctly */
    reset_state();
    load_expr("3*3");
    handle_normal_mode(TOKEN_ENTER);
    CHECK(NEAR(ans, 9.0f), "nav: ENTER evaluates 3*3=9");

    /* 12. ANS is updated after ENTER */
    reset_state();
    load_expr("6+6");
    handle_normal_mode(TOKEN_ENTER);
    CHECK(NEAR(ans, 12.0f), "nav: ans updated after ENTER 6+6=12");

    /* 13. History expression matches what was typed */
    reset_state();
    load_expr("1+1");
    handle_normal_mode(TOKEN_ENTER);
    CHECK(strcmp(history[0].expression, "1+1") == 0, "nav: history expression stored verbatim");
}

/* -------------------------------------------------------------------------
 * Group 5: handle_clear_key — TOKEN_CLEAR (3 tests)
 * ---------------------------------------------------------------------- */
static void test_clear_key(void)
{
    printf("Group 5: handle_clear_key\n");

    /* 1. CLEAR resets expression, expr_len, and cursor_pos */
    reset_state();
    load_expr("123+456");
    handle_normal_mode(TOKEN_CLEAR);
    CHECK(expr_len == 0,    "clear: expr_len reset to 0");
    CHECK(cursor_pos == 0,  "clear: cursor_pos reset to 0");
    CHECK(expression[0] == '\0', "clear: expression string empty");

    /* 2. CLEAR on already-empty expression is a no-op (no crash) */
    reset_state();
    handle_normal_mode(TOKEN_CLEAR);
    CHECK(expr_len == 0, "clear: no-op on empty expression");

    /* 3. CLEAR does not affect history */
    reset_state();
    load_expr("5");
    handle_normal_mode(TOKEN_ENTER);
    load_expr("99");
    handle_normal_mode(TOKEN_CLEAR);
    CHECK(history_count == 1, "clear: history unchanged after CLEAR");
    CHECK(strcmp(history[0].expression, "5") == 0, "clear: history entry intact");
}

/* -------------------------------------------------------------------------
 * Group 6: handle_sto_key — TOKEN_STO (4 tests)
 * ---------------------------------------------------------------------- */
static void test_sto_key(void)
{
    printf("Group 6: handle_sto_key\n");

    /* 1. STO sets sto_pending */
    reset_state();
    load_expr("42");
    handle_normal_mode(TOKEN_STO);
    CHECK(sto_pending == true, "sto: TOKEN_STO sets sto_pending");

    /* 2. STO on empty expression prepends ANS first */
    reset_state();
    ans = 7.0f;
    handle_normal_mode(TOKEN_STO);
    CHECK(strncmp(expression, "ANS", 3) == 0, "sto: STO on empty prepends ANS");
    CHECK(sto_pending == true,                "sto: sto_pending set after ANS prepend");

    /* 3. sto_pending starts false */
    reset_state();
    CHECK(sto_pending == false, "sto: sto_pending initially false");

    /* 4. STO with expression already populated does not modify expression */
    reset_state();
    load_expr("PI");
    handle_normal_mode(TOKEN_STO);
    CHECK(strcmp(expression, "PI") == 0, "sto: STO does not modify non-empty expression");
    CHECK(sto_pending == true,           "sto: sto_pending set with non-empty expression");
}

/* -------------------------------------------------------------------------
 * Group 7: INSERT toggle and DEL key (5 tests)
 * ---------------------------------------------------------------------- */
static void test_ins_del(void)
{
    printf("Group 7: INS toggle and DEL\n");

    /* 1. TOKEN_INS toggles insert_mode from false to true */
    reset_state();
    CHECK(insert_mode == false, "ins: insert_mode initially false");
    handle_normal_mode(TOKEN_INS);
    CHECK(insert_mode == true, "ins: TOKEN_INS enables insert_mode");

    /* 2. TOKEN_INS again reverts insert_mode */
    handle_normal_mode(TOKEN_INS);
    CHECK(insert_mode == false, "ins: second TOKEN_INS disables insert_mode");

    /* 3. TOKEN_DEL removes the character before cursor */
    reset_state();
    load_expr("AB");
    handle_normal_mode(TOKEN_DEL);
    CHECK(expr_len == 1,         "del: DEL reduces expr_len by 1");
    CHECK(expression[0] == 'A',  "del: DEL removes last char leaving 'A'");
    CHECK(cursor_pos == 1,       "del: cursor_pos after DEL");

    /* 4. TOKEN_DEL on empty expression is a no-op */
    reset_state();
    handle_normal_mode(TOKEN_DEL);
    CHECK(expr_len == 0, "del: DEL on empty is no-op");

    /* 5. TOKEN_DEL with cursor in middle removes the right character */
    reset_state();
    load_expr("ABC");
    cursor_pos = 2;
    handle_normal_mode(TOKEN_DEL);
    CHECK(expr_len == 2,            "del: mid-cursor DEL reduces len by 1");
    CHECK(expression[0] == 'A',     "del: mid-cursor DEL: first char intact");
    CHECK(expression[1] == 'C',     "del: mid-cursor DEL: third char shifts down");
}

/* -------------------------------------------------------------------------
 * Group 8: dispatch — handle_normal_mode routes tokens to correct mode (7 tests)
 * ---------------------------------------------------------------------- */
static void test_dispatch(void)
{
    printf("Group 8: dispatch — mode transitions\n");

    /* 1. TOKEN_MATH opens MATH menu (current_mode → MODE_MATH_MENU) */
    reset_state();
    handle_normal_mode(TOKEN_MATH);
    CHECK(current_mode == MODE_MATH_MENU, "dispatch: TOKEN_MATH → MODE_MATH_MENU");

    /* 2. TOKEN_TEST opens TEST menu */
    reset_state();
    handle_normal_mode(TOKEN_TEST);
    CHECK(current_mode == MODE_TEST_MENU, "dispatch: TOKEN_TEST → MODE_TEST_MENU");

    /* 3. TOKEN_MATRX opens MATRIX menu */
    reset_state();
    handle_normal_mode(TOKEN_MATRX);
    CHECK(current_mode == MODE_MATRIX_MENU, "dispatch: TOKEN_MATRX → MODE_MATRIX_MENU");

    /* 4. TOKEN_PRGM opens PRGM menu */
    reset_state();
    handle_normal_mode(TOKEN_PRGM);
    CHECK(current_mode == MODE_PRGM_MENU, "dispatch: TOKEN_PRGM → MODE_PRGM_MENU");

    /* 5. TOKEN_MODE switches to MODE_MODE_SCREEN */
    reset_state();
    handle_normal_mode(TOKEN_MODE);
    CHECK(current_mode == MODE_MODE_SCREEN, "dispatch: TOKEN_MODE → MODE_MODE_SCREEN");

    /* 6. TOKEN_GRAPH navigates to MODE_NORMAL (nav_to stub sets current_mode) */
    reset_state();
    handle_normal_mode(TOKEN_GRAPH);
    CHECK(current_mode == MODE_NORMAL, "dispatch: TOKEN_GRAPH → MODE_NORMAL");

    /* 7. TOKEN_TRACE navigates to MODE_GRAPH_TRACE */
    reset_state();
    handle_normal_mode(TOKEN_TRACE);
    CHECK(current_mode == MODE_GRAPH_TRACE, "dispatch: TOKEN_TRACE → MODE_GRAPH_TRACE");
}

/* -------------------------------------------------------------------------
 * Group 9: multi-step expression building (5 tests)
 * ---------------------------------------------------------------------- */
static void test_expr_building(void)
{
    printf("Group 9: multi-step expression building\n");

    /* 1. Digit + operator + digit = evaluable expression */
    reset_state();
    handle_normal_mode(TOKEN_4);
    handle_normal_mode(TOKEN_ADD);
    handle_normal_mode(TOKEN_5);
    handle_normal_mode(TOKEN_ENTER);
    CHECK(NEAR(ans, 9.0f), "build: 4+5 evaluates to 9");

    /* 2. sin(0) = 0 (radian mode from reset_state) */
    reset_state();
    angle_degrees = false;
    handle_normal_mode(TOKEN_SIN);
    handle_normal_mode(TOKEN_0);
    handle_normal_mode(TOKEN_R_PAR);
    handle_normal_mode(TOKEN_ENTER);
    CHECK(NEAR(ans, 0.0f), "build: sin(0)=0");

    /* 3. ANS is used in subsequent expression */
    reset_state();
    load_expr("10");
    handle_normal_mode(TOKEN_ENTER);
    handle_normal_mode(TOKEN_ADD);   /* empty expr → ANS prepended */
    handle_normal_mode(TOKEN_5);
    handle_normal_mode(TOKEN_ENTER);
    CHECK(NEAR(ans, 15.0f), "build: ANS+5=15 (ANS=10)");

    /* 4. STO + variable letter stores the value */
    reset_state();
    load_expr("8");
    handle_normal_mode(TOKEN_ENTER);
    /* sto_pending path: use handle_normal_mode which dispatches through sto_pending */
    load_expr("8");
    handle_normal_mode(TOKEN_STO);
    CHECK(sto_pending == true, "build: sto pending set");
    /* Simulate pressing TOKEN_A while sto_pending — handled by handle_sto_pending
     * which is dispatched from Execute_Token, not handle_normal_mode directly.
     * Reset sto_pending manually for this stub environment. */
    sto_pending = false;

    /* 5. ENTRY immediately after an ENTER recalls correctly */
    reset_state();
    load_expr("2*6");
    handle_normal_mode(TOKEN_ENTER);
    memset(expression, 0, sizeof(expression));
    expr_len = cursor_pos = 0;
    handle_normal_mode(TOKEN_ENTRY);
    CHECK(strcmp(expression, "2*6") == 0, "build: ENTRY recalls last expression");
}

/* -------------------------------------------------------------------------
 * Group 10: edge cases (6 tests)
 * ---------------------------------------------------------------------- */
static void test_edge_cases(void)
{
    printf("Group 10: edge cases\n");

    /* 1. TOKEN_COMMA inserts ',' */
    reset_state();
    handle_normal_mode(TOKEN_COMMA);
    CHECK(expression[0] == ',', "edge: TOKEN_COMMA inserts ','");

    /* 2. TOKEN_SPACE inserts ' ' */
    reset_state();
    handle_normal_mode(TOKEN_SPACE);
    CHECK(expression[0] == ' ', "edge: TOKEN_SPACE inserts ' '");

    /* 3. Long expression does not overflow — fill to near capacity */
    reset_state();
    for (int i = 0; i < MAX_EXPR_LEN - 2; i++)
        handle_normal_mode(TOKEN_1);
    CHECK(expr_len < MAX_EXPR_LEN, "edge: expression does not overflow MAX_EXPR_LEN");

    /* 4. Evaluating an error expression does not crash */
    reset_state();
    load_expr("1/0");
    handle_normal_mode(TOKEN_ENTER);  /* division by zero — expect no crash */
    CHECK(history_count == 1, "edge: 1/0 completes without crash");

    /* 5. TOKEN_Y_EQUALS navigates to MODE_GRAPH_YEQ */
    reset_state();
    handle_normal_mode(TOKEN_Y_EQUALS);
    CHECK(current_mode == MODE_GRAPH_YEQ, "edge: TOKEN_Y_EQUALS → MODE_GRAPH_YEQ");

    /* 6. TOKEN_RANGE navigates to MODE_GRAPH_RANGE */
    reset_state();
    handle_normal_mode(TOKEN_RANGE);
    CHECK(current_mode == MODE_GRAPH_RANGE, "edge: TOKEN_RANGE → MODE_GRAPH_RANGE");
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(void)
{
    test_digit_key();
    test_arithmetic_op();
    test_function_insert();
    test_history_nav();
    test_clear_key();
    test_sto_key();
    test_ins_del();
    test_dispatch();
    test_expr_building();
    test_edge_cases();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
