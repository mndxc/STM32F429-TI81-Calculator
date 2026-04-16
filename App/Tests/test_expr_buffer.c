/**
 * @file    test_expr_buffer.c
 * @brief   Host-side unit tests for the ExprBuffer_t wrapper (expr_buffer.c).
 *
 * Build:
 *   cmake -S App/Tests -B build/tests && cmake --build build/tests
 *   ./build/tests/test_expr_buffer   # exits 0 on full pass
 */

#include "../Inc/expr_util.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Minimal test framework (same macros as test_expr_util.c)                   */
/* -------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT_EQ(a, b, msg) do { \
    if ((a) == (b)) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s:%d] %s: got %d, expected %d\n", \
                            __FILE__, __LINE__, (msg), (int)(a), (int)(b)); } \
} while (0)

#define EXPECT_STREQ(a, b, msg) do { \
    if (strcmp((a), (b)) == 0) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s:%d] %s: got \"%s\", expected \"%s\"\n", \
                            __FILE__, __LINE__, (msg), (a), (b)); } \
} while (0)

#define EXPECT_TRUE(e, msg)  EXPECT_EQ(!!(e), 1, msg)
#define EXPECT_FALSE(e, msg) EXPECT_EQ(!!(e), 0, msg)

/* -------------------------------------------------------------------------- */
/* Group 1: ExprBuffer_Clear                                                   */
/* -------------------------------------------------------------------------- */

static void test_clear(void)
{
    printf("Group 1: ExprBuffer_Clear\n");

    ExprBuffer_t b;
    /* Dirty state */
    strcpy(b.buf, "hello");
    b.len    = 5;
    b.cursor = 3;

    ExprBuffer_Clear(&b);
    EXPECT_EQ(b.len,    0,    "clear: len == 0");
    EXPECT_EQ(b.cursor, 0,    "clear: cursor == 0");
    EXPECT_EQ(b.buf[0], '\0', "clear: buf[0] == nul");

    /* Clear on already-empty is a no-op */
    ExprBuffer_Clear(&b);
    EXPECT_EQ(b.len, 0, "clear: double-clear is no-op");
}

/* -------------------------------------------------------------------------- */
/* Group 2: ExprBuffer_Insert — single character (insert mode)                */
/* -------------------------------------------------------------------------- */

static void test_insert_single_insert_mode(void)
{
    printf("Group 2: ExprBuffer_Insert single char (insert mode)\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);

    /* Append 'A' to empty buffer */
    char s1[] = "A";
    ExprBuffer_Insert(&b, true, s1);
    EXPECT_STREQ(b.buf, "A", "insert: 'A' into empty");
    EXPECT_EQ(b.len,    1,   "insert: len == 1");
    EXPECT_EQ(b.cursor, 1,   "insert: cursor == 1");

    /* Append '1' */
    char s2[] = "1";
    ExprBuffer_Insert(&b, true, s2);
    EXPECT_STREQ(b.buf, "A1", "insert: 'A1'");
    EXPECT_EQ(b.len,    2,    "insert: len == 2");

    /* Insert 'X' at cursor=0 (prepend) */
    b.cursor = 0;
    char s3[] = "X";
    ExprBuffer_Insert(&b, true, s3);
    EXPECT_STREQ(b.buf, "XA1", "insert: prepend 'X'");
    EXPECT_EQ(b.cursor, 1,     "insert: cursor after prepend == 1");
}

/* -------------------------------------------------------------------------- */
/* Group 3: ExprBuffer_Insert — single character (overwrite mode)             */
/* -------------------------------------------------------------------------- */

static void test_insert_single_overwrite_mode(void)
{
    printf("Group 3: ExprBuffer_Insert single char (overwrite mode)\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);
    strcpy(b.buf, "ABC");
    b.len    = 3;
    b.cursor = 0;

    /* Overwrite 'A' with 'X' → "XBC" */
    char s[] = "X";
    ExprBuffer_Insert(&b, false, s);
    EXPECT_STREQ(b.buf, "XBC", "overwrite: 'A'→'X'");
    EXPECT_EQ(b.len,    3,     "overwrite: len unchanged");
    EXPECT_EQ(b.cursor, 1,     "overwrite: cursor advances");
}

/* -------------------------------------------------------------------------- */
/* Group 4: ExprBuffer_Insert — multi-character string                        */
/* -------------------------------------------------------------------------- */

static void test_insert_multi(void)
{
    printf("Group 4: ExprBuffer_Insert multi-char string\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);

    ExprBuffer_Insert(&b, true, "sin(");
    EXPECT_STREQ(b.buf, "sin(", "multi: 'sin(' into empty");
    EXPECT_EQ(b.len,    4,      "multi: len == 4");
    EXPECT_EQ(b.cursor, 4,      "multi: cursor == 4");

    /* Multi-char always inserts regardless of insert_mode=false */
    b.cursor = 0;
    ExprBuffer_Insert(&b, false, "cos(");
    EXPECT_STREQ(b.buf, "cos(sin(", "multi: 'cos(' prepended in overwrite mode");
    EXPECT_EQ(b.len,    8,           "multi: len == 8");
}

/* -------------------------------------------------------------------------- */
/* Group 5: ExprBuffer_Delete (backspace)                                     */
/* -------------------------------------------------------------------------- */

static void test_delete(void)
{
    printf("Group 5: ExprBuffer_Delete\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);
    strcpy(b.buf, "ABC");
    b.len    = 3;
    b.cursor = 3;

    /* Delete 'C' */
    ExprBuffer_Delete(&b);
    EXPECT_STREQ(b.buf, "AB", "delete: 'C' removed");
    EXPECT_EQ(b.len,    2,    "delete: len == 2");
    EXPECT_EQ(b.cursor, 2,    "delete: cursor == 2");

    /* Delete 'B' */
    ExprBuffer_Delete(&b);
    EXPECT_STREQ(b.buf, "A",  "delete: 'B' removed");
    EXPECT_EQ(b.len,    1,    "delete: len == 1");

    /* Delete at cursor=0 is a no-op */
    b.cursor = 0;
    ExprBuffer_Delete(&b);
    EXPECT_STREQ(b.buf, "A",  "delete: no-op at cursor=0");
    EXPECT_EQ(b.len,    1,    "delete: len unchanged at cursor=0");
}

/* -------------------------------------------------------------------------- */
/* Group 6: ExprBuffer_Left / ExprBuffer_Right                                */
/* -------------------------------------------------------------------------- */

static void test_left_right(void)
{
    printf("Group 6: ExprBuffer_Left / ExprBuffer_Right\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);
    strcpy(b.buf, "ABC");
    b.len    = 3;
    b.cursor = 3;

    /* Move left three times */
    ExprBuffer_Left(&b);
    EXPECT_EQ(b.cursor, 2, "left: 3→2");
    ExprBuffer_Left(&b);
    EXPECT_EQ(b.cursor, 1, "left: 2→1");
    ExprBuffer_Left(&b);
    EXPECT_EQ(b.cursor, 0, "left: 1→0");
    ExprBuffer_Left(&b);
    EXPECT_EQ(b.cursor, 0, "left: 0 is no-op");

    /* Move right three times */
    ExprBuffer_Right(&b);
    EXPECT_EQ(b.cursor, 1, "right: 0→1");
    ExprBuffer_Right(&b);
    EXPECT_EQ(b.cursor, 2, "right: 1→2");
    ExprBuffer_Right(&b);
    EXPECT_EQ(b.cursor, 3, "right: 2→3");
    ExprBuffer_Right(&b);
    EXPECT_EQ(b.cursor, 3, "right: 3 is no-op");
}

/* -------------------------------------------------------------------------- */
/* Group 7: Invariant — cursor never exceeds len after sequence               */
/* -------------------------------------------------------------------------- */

static void test_invariants(void)
{
    printf("Group 7: Invariant: cursor <= len after operations\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);

    /* Build "sin(" one char at a time */
    const char *chars[] = { "s", "i", "n", "(" };
    for (int i = 0; i < 4; i++) {
        ExprBuffer_Insert(&b, true, chars[i]);
        EXPECT_TRUE(b.cursor <= b.len, "invariant: cursor <= len after insert");
    }

    /* Delete everything */
    while (b.len > 0) {
        ExprBuffer_Delete(&b);
        EXPECT_TRUE(b.cursor <= b.len, "invariant: cursor <= len after delete");
    }
    EXPECT_EQ(b.len,    0,    "invariant: empty after all deletes");
    EXPECT_EQ(b.cursor, 0,    "invariant: cursor=0 when empty");
    EXPECT_EQ(b.buf[0], '\0', "invariant: buf null-terminated when empty");
}

/* -------------------------------------------------------------------------- */
/* Group 8: Overflow guard                                                     */
/* -------------------------------------------------------------------------- */

static void test_overflow_guard(void)
{
    printf("Group 8: Overflow guard\n");

    ExprBuffer_t b;
    ExprBuffer_Clear(&b);

    /* Fill buffer to MAX_EXPR_LEN - 1 bytes */
    char fill[2] = "A";
    while (b.len < MAX_EXPR_LEN - 1) {
        ExprBuffer_Insert(&b, true, fill);
    }
    EXPECT_EQ(b.len, MAX_EXPR_LEN - 1, "overflow: filled to MAX-1");

    /* One more insert must be blocked */
    uint8_t len_before = b.len;
    ExprBuffer_Insert(&b, true, fill);
    EXPECT_EQ(b.len, len_before, "overflow: insert blocked at capacity");
    EXPECT_TRUE(b.cursor <= b.len, "overflow: cursor still valid");
}

/* -------------------------------------------------------------------------- */
/* main                                                                        */
/* -------------------------------------------------------------------------- */

int main(void)
{
    test_clear();
    test_insert_single_insert_mode();
    test_insert_single_overwrite_mode();
    test_insert_multi();
    test_delete();
    test_left_right();
    test_invariants();
    test_overflow_guard();

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
