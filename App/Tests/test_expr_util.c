/**
 * @file    test_expr_util.c
 * @brief   Host-side unit tests for expr_util.c pure expression-buffer functions.
 *
 * Build:
 *   cmake -S App/Tests -B build/tests && cmake --build build/tests
 *   ./build/tests/test_expr_util   # exits 0 on full pass
 *
 * Soft-fail: every test group runs even if earlier groups fail.  Final
 * exit code is non-zero if any failure was recorded.
 */

#include "../Inc/expr_util.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Minimal test framework                                                       */
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

#define EXPECT_TRUE(expr, msg) EXPECT_EQ(!!(expr), 1, msg)
#define EXPECT_FALSE(expr, msg) EXPECT_EQ(!!(expr), 0, msg)

/* -------------------------------------------------------------------------- */
/* Group 1: ExprUtil_Utf8CharSize                                               */
/* -------------------------------------------------------------------------- */

static void test_utf8_char_size(void)
{
    printf("Group 1: ExprUtil_Utf8CharSize\n");

    /* Null terminator */
    EXPECT_EQ(ExprUtil_Utf8CharSize(""),       0, "null terminator");

    /* ASCII */
    EXPECT_EQ(ExprUtil_Utf8CharSize("A"),      1, "ASCII 'A'");
    EXPECT_EQ(ExprUtil_Utf8CharSize("1"),      1, "ASCII '1'");

    /* 2-byte: π U+03C0 = 0xCF 0x80 */
    EXPECT_EQ(ExprUtil_Utf8CharSize("\xCF\x80"), 2, "2-byte pi");

    /* 3-byte: √ U+221A = 0xE2 0x88 0x9A */
    EXPECT_EQ(ExprUtil_Utf8CharSize("\xE2\x88\x9A"), 3, "3-byte sqrt");

    /* 3-byte: ↑ U+2191 = 0xE2 0x86 0x91 */
    EXPECT_EQ(ExprUtil_Utf8CharSize("\xE2\x86\x91"), 3, "3-byte up-arrow");

    /* Continuation byte (0x80) treated as 1 to avoid stalling */
    EXPECT_EQ(ExprUtil_Utf8CharSize("\x80"),   1, "continuation byte");

    /* 4-byte sequence start (0xF0) */
    EXPECT_EQ(ExprUtil_Utf8CharSize("\xF0\x9F\x98\x80"), 4, "4-byte emoji");
}

/* -------------------------------------------------------------------------- */
/* Group 2: ExprUtil_Utf8ByteToGlyph                                            */
/* -------------------------------------------------------------------------- */

static void test_utf8_byte_to_glyph(void)
{
    printf("Group 2: ExprUtil_Utf8ByteToGlyph\n");

    /* ASCII-only string "ABC" */
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph("ABC", 0), 0, "glyph 0 of ASCII");
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph("ABC", 1), 1, "glyph 1 of ASCII");
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph("ABC", 3), 3, "glyph 3 (end) of ASCII");

    /* "A√B": A=1B, √=3B, B=1B → byte 0=glyph 0, byte 1=glyph 1, byte 4=glyph 2 */
    const char *s = "A\xE2\x88\x9A" "B";
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph(s, 0), 0, "byte 0 = glyph 0");
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph(s, 1), 1, "byte 1 = glyph 1 (start of sqrt)");
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph(s, 4), 2, "byte 4 = glyph 2 (B)");
    EXPECT_EQ(ExprUtil_Utf8ByteToGlyph(s, 5), 3, "byte 5 = glyph 3 (end)");
}

/* -------------------------------------------------------------------------- */
/* Group 3: ExprUtil_MatrixTokenSizeBefore                                     */
/* -------------------------------------------------------------------------- */

static void test_matrix_size_before(void)
{
    printf("Group 3: ExprUtil_MatrixTokenSizeBefore\n");

    /* "[A]" at pos=3 */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("[A]", 3), 3, "[A] before pos=3");

    /* "[B]" at pos=3 */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("[B]", 3), 3, "[B] before pos=3");

    /* "[C]" at pos=3 */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("[C]", 3), 3, "[C] before pos=3");

    /* "[D]" — not a valid matrix token */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("[D]", 3), 0, "[D] not a matrix token");

    /* pos < 3 — not enough bytes */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("[A]", 2), 0, "pos=2 too small");

    /* mid-string: "det([A]" — matrix token ends at pos=7 */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("det([A]", 7), 3, "[A] in mid-string");

    /* Not a bracket token */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeBefore("ABC", 3), 0, "ABC not matrix");
}

/* -------------------------------------------------------------------------- */
/* Group 4: ExprUtil_MatrixTokenSizeAt                                         */
/* -------------------------------------------------------------------------- */

static void test_matrix_size_at(void)
{
    printf("Group 4: ExprUtil_MatrixTokenSizeAt\n");

    /* "[A]" starting at pos=0, len=3 */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeAt("[A]", 0, 3), 3, "[A] at pos=0");

    /* "[B]" at pos=2 in "2+[B]", len=5 */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeAt("2+[B]", 2, 5), 3, "[B] at pos=2");

    /* "[C]" */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeAt("[C]", 0, 3), 3, "[C] at pos=0");

    /* "[D]" not valid */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeAt("[D]", 0, 3), 0, "[D] not valid");

    /* pos+3 > len — not enough room */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeAt("[A]", 1, 3), 0, "pos+3>len guard");

    /* Not a bracket */
    EXPECT_EQ(ExprUtil_MatrixTokenSizeAt("ABC", 0, 3), 0, "ABC not matrix");
}

/* -------------------------------------------------------------------------- */
/* Group 5: ExprUtil_MoveCursorLeft                                            */
/* -------------------------------------------------------------------------- */

static void test_cursor_left(void)
{
    printf("Group 5: ExprUtil_MoveCursorLeft\n");

    uint8_t cur;

    /* ASCII — moves back one byte */
    cur = 3;
    ExprUtil_MoveCursorLeft("ABC", &cur);
    EXPECT_EQ(cur, 2, "ASCII left from 3");

    /* Already at 0 — no-op */
    cur = 0;
    ExprUtil_MoveCursorLeft("ABC", &cur);
    EXPECT_EQ(cur, 0, "left from 0 is no-op");

    /* Multi-byte: "A√B" — cursor at 4 (after √=3 bytes) should jump to 1 */
    cur = 4;
    ExprUtil_MoveCursorLeft("A\xE2\x88\x9A" "B", &cur);
    EXPECT_EQ(cur, 1, "left across 3-byte sqrt");

    /* Matrix token: "2+[A]" — cursor at 5 should jump to 2 */
    cur = 5;
    ExprUtil_MoveCursorLeft("2+[A]", &cur);
    EXPECT_EQ(cur, 2, "left across [A] matrix token");

    /* Matrix token: cursor at 3 in "[A]" — jumps to 0 */
    cur = 3;
    ExprUtil_MoveCursorLeft("[A]", &cur);
    EXPECT_EQ(cur, 0, "left across [A] from pos=3");
}

/* -------------------------------------------------------------------------- */
/* Group 6: ExprUtil_MoveCursorRight                                           */
/* -------------------------------------------------------------------------- */

static void test_cursor_right(void)
{
    printf("Group 6: ExprUtil_MoveCursorRight\n");

    uint8_t cur;

    /* ASCII — moves forward one byte */
    cur = 0;
    ExprUtil_MoveCursorRight("ABC", 3, &cur);
    EXPECT_EQ(cur, 1, "ASCII right from 0");

    /* Already at end — no-op */
    cur = 3;
    ExprUtil_MoveCursorRight("ABC", 3, &cur);
    EXPECT_EQ(cur, 3, "right at end is no-op");

    /* Multi-byte: "A√B" — cursor at 1 (start of √) should jump to 4 */
    cur = 1;
    ExprUtil_MoveCursorRight("A\xE2\x88\x9A" "B", 5, &cur);
    EXPECT_EQ(cur, 4, "right across 3-byte sqrt");

    /* Matrix token: "[A]+2" — cursor at 0 should jump to 3 */
    cur = 0;
    ExprUtil_MoveCursorRight("[A]+2", 5, &cur);
    EXPECT_EQ(cur, 3, "right across [A] matrix token");
}

/* -------------------------------------------------------------------------- */
/* Group 7: ExprUtil_InsertChar — insert mode                                  */
/* -------------------------------------------------------------------------- */

static void test_insert_char_insert_mode(void)
{
    printf("Group 7: ExprUtil_InsertChar (insert mode)\n");

    char buf[16];
    uint8_t len, cur;

    /* Insert 'X' at cursor=0 into "AB" → "XAB" */
    strcpy(buf, "AB");
    len = 2; cur = 0;
    ExprUtil_InsertChar(buf, &len, &cur, 16, true, 'X');
    EXPECT_STREQ(buf, "XAB", "insert at start");
    EXPECT_EQ(len, 3, "len after insert at start");
    EXPECT_EQ(cur, 1, "cursor after insert at start");

    /* Insert 'X' at cursor=2 (end) into "AB" → "ABX" */
    strcpy(buf, "AB");
    len = 2; cur = 2;
    ExprUtil_InsertChar(buf, &len, &cur, 16, true, 'X');
    EXPECT_STREQ(buf, "ABX", "insert at end");
    EXPECT_EQ(len, 3, "len after insert at end");
    EXPECT_EQ(cur, 3, "cursor after insert at end");

    /* Buffer overflow — max_len=3, len=2 inserting one more: 2+1=3 > max_len=3?
     * Check: if (*len + 1 > max_len) return; → 2+1=3, 3>3 is false, so it inserts.
     * Actually max_len=2 test: len=1, max_len=2: 1+1=2 > 2? No, so inserts.
     * len=2, max_len=2: 2+1=3 > 2? Yes, so returns without inserting. */
    strcpy(buf, "AB");
    len = 2; cur = 1;
    ExprUtil_InsertChar(buf, &len, &cur, 2, true, 'X'); /* max_len=2, already 2 */
    EXPECT_STREQ(buf, "AB", "insert blocked at max_len");
    EXPECT_EQ(len, 2, "len unchanged at max_len");
    EXPECT_EQ(cur, 1, "cursor unchanged at max_len");
}

/* -------------------------------------------------------------------------- */
/* Group 8: ExprUtil_InsertChar — overwrite mode                               */
/* -------------------------------------------------------------------------- */

static void test_insert_char_overwrite_mode(void)
{
    printf("Group 8: ExprUtil_InsertChar (overwrite mode)\n");

    char buf[16];
    uint8_t len, cur;

    /* Overwrite 'A' with 'X' at cursor=0 in "ABC" → "XBC" */
    strcpy(buf, "ABC");
    len = 3; cur = 0;
    ExprUtil_InsertChar(buf, &len, &cur, 16, false, 'X');
    EXPECT_STREQ(buf, "XBC", "overwrite ASCII");
    EXPECT_EQ(len, 3, "len unchanged after overwrite");
    EXPECT_EQ(cur, 1, "cursor advances after overwrite");

    /* Overwrite 3-byte √ with 'X': "A√B" → "AXB"
     * buf = "A\xE2\x88\x9A" "B", len=5, cursor=1 (at start of √) */
    memcpy(buf, "A\xE2\x88\x9A" "B", 6); /* includes null terminator */
    len = 5; cur = 1;
    ExprUtil_InsertChar(buf, &len, &cur, 16, false, 'X');
    EXPECT_STREQ(buf, "AXB", "overwrite 3-byte sqrt");
    EXPECT_EQ(len, 3, "len decreases when overwriting multi-byte");
    EXPECT_EQ(cur, 2, "cursor advances past replacement");

    /* Overwrite matrix token [A] with 'X': "[A]+2" → "X+2"
     * cursor=0, len=5 */
    strcpy(buf, "[A]+2");
    len = 5; cur = 0;
    ExprUtil_InsertChar(buf, &len, &cur, 16, false, 'X');
    EXPECT_STREQ(buf, "X+2", "overwrite [A] matrix token");
    EXPECT_EQ(len, 3, "len after overwrite [A]");
    EXPECT_EQ(cur, 1, "cursor after overwrite [A]");

    /* Overwrite at end (cursor==len) behaves like insert */
    strcpy(buf, "AB");
    len = 2; cur = 2;
    ExprUtil_InsertChar(buf, &len, &cur, 16, false, 'X');
    EXPECT_STREQ(buf, "ABX", "overwrite at end acts as insert");
    EXPECT_EQ(len, 3, "len after overwrite-at-end insert");
}

/* -------------------------------------------------------------------------- */
/* Group 9: ExprUtil_InsertStr                                                 */
/* -------------------------------------------------------------------------- */

static void test_insert_str(void)
{
    printf("Group 9: ExprUtil_InsertStr\n");

    char buf[16];
    uint8_t len, cur;

    /* Insert "sin(" at cursor=0 in empty buffer */
    buf[0] = '\0';
    len = 0; cur = 0;
    ExprUtil_InsertStr(buf, &len, &cur, 16, "sin(");
    EXPECT_STREQ(buf, "sin(", "insert str into empty");
    EXPECT_EQ(len, 4, "len after insert str");
    EXPECT_EQ(cur, 4, "cursor after insert str");

    /* Insert "√(" in middle: "sin(x" → "sin(√(x" */
    strcpy(buf, "sin(x");
    len = 5; cur = 4;
    ExprUtil_InsertStr(buf, &len, &cur, 16, "\xE2\x88\x9A(");
    EXPECT_STREQ(buf, "sin(\xE2\x88\x9A(x", "insert multi-byte str in middle");
    EXPECT_EQ(len, 9, "len after multi-byte insert");
    EXPECT_EQ(cur, 8, "cursor after multi-byte insert");

    /* Overflow guard: max_len=6, buf has 5 chars, inserting 2 → 7 >= 6, blocked */
    strcpy(buf, "ABCDE");
    len = 5; cur = 2;
    ExprUtil_InsertStr(buf, &len, &cur, 6, "XY");
    EXPECT_STREQ(buf, "ABCDE", "insert str blocked at overflow");
    EXPECT_EQ(len, 5, "len unchanged at overflow");
}

/* -------------------------------------------------------------------------- */
/* Group 10: ExprUtil_DeleteAtCursor                                           */
/* -------------------------------------------------------------------------- */

static void test_delete_at_cursor(void)
{
    printf("Group 10: ExprUtil_DeleteAtCursor\n");

    char buf[16];
    uint8_t len, cur;

    /* Delete at cursor=0 — no-op */
    strcpy(buf, "ABC");
    len = 3; cur = 0;
    ExprUtil_DeleteAtCursor(buf, &len, &cur);
    EXPECT_STREQ(buf, "ABC", "delete at 0 is no-op");
    EXPECT_EQ(len, 3, "len unchanged at 0");

    /* Delete ASCII: "ABC", cursor=2 → "AC" */
    strcpy(buf, "ABC");
    len = 3; cur = 2;
    ExprUtil_DeleteAtCursor(buf, &len, &cur);
    EXPECT_STREQ(buf, "AC", "delete ASCII");
    EXPECT_EQ(len, 2, "len after ASCII delete");
    EXPECT_EQ(cur, 1, "cursor after ASCII delete");

    /* Delete 3-byte √: "A√B", cursor=4 (after √) → "AB" */
    memcpy(buf, "A\xE2\x88\x9A" "B", 6);
    len = 5; cur = 4;
    ExprUtil_DeleteAtCursor(buf, &len, &cur);
    EXPECT_STREQ(buf, "AB", "delete 3-byte sqrt");
    EXPECT_EQ(len, 2, "len after sqrt delete");
    EXPECT_EQ(cur, 1, "cursor after sqrt delete");

    /* Delete matrix token [A]: "2+[A]", cursor=5 → "2+" */
    strcpy(buf, "2+[A]");
    len = 5; cur = 5;
    ExprUtil_DeleteAtCursor(buf, &len, &cur);
    EXPECT_STREQ(buf, "2+", "delete [A] matrix token");
    EXPECT_EQ(len, 2, "len after [A] delete");
    EXPECT_EQ(cur, 2, "cursor after [A] delete");

    /* Delete [B] in the middle: "det([B],1,2)", cursor=7 (after [B]) → "det(,1,2)" */
    strcpy(buf, "det([B],1");
    len = 9; cur = 7;
    ExprUtil_DeleteAtCursor(buf, &len, &cur);
    EXPECT_STREQ(buf, "det(,1", "delete [B] in middle");
    EXPECT_EQ(len, 6, "len after [B] in middle delete");
    EXPECT_EQ(cur, 4, "cursor after [B] in middle delete");
}

/* -------------------------------------------------------------------------- */
/* Group 11: ExprUtil_PrependAns                                               */
/* -------------------------------------------------------------------------- */

static void test_prepend_ans(void)
{
    printf("Group 11: ExprUtil_PrependAns\n");

    char buf[16];
    uint8_t len, cur;

    /* Empty buffer — ANS is prepended */
    buf[0] = '\0';
    len = 0; cur = 0;
    ExprUtil_PrependAns(buf, &len, &cur, 16);
    EXPECT_STREQ(buf, "ANS", "prepend ANS to empty");
    EXPECT_EQ(len, 3, "len = 3 after prepend");
    EXPECT_EQ(cur, 3, "cursor = 3 after prepend");

    /* Non-empty buffer — no-op */
    strcpy(buf, "1+2");
    len = 3; cur = 0;
    ExprUtil_PrependAns(buf, &len, &cur, 16);
    EXPECT_STREQ(buf, "1+2", "prepend ANS to non-empty is no-op");
    EXPECT_EQ(len, 3, "len unchanged for non-empty");
    EXPECT_EQ(cur, 0, "cursor unchanged for non-empty");

    /* max_len too small (≤ 3) — no-op */
    buf[0] = '\0';
    len = 0; cur = 0;
    ExprUtil_PrependAns(buf, &len, &cur, 3);
    EXPECT_EQ(len, 0, "prepend blocked when max_len=3");
}

/* -------------------------------------------------------------------------- */
/* Group 12: Round-trip cursor navigation                                      */
/* -------------------------------------------------------------------------- */

static void test_cursor_roundtrip(void)
{
    printf("Group 12: Cursor round-trip\n");

    /* "A√[A]B" — byte layout: A(1) √(3) [A](3) B(1) = 8 bytes
     * Glyphs: A(0) √(1) [A](2) B(3)
     * Step right from 0 → 1 → 4 → 7 → 8 (end)
     * Step left  from 8 → 7 → 4 → 1 → 0 */
    const char *s = "A\xE2\x88\x9A[A]B";
    uint8_t len = 8;

    uint8_t cur = 0;
    ExprUtil_MoveCursorRight(s, len, &cur); EXPECT_EQ(cur, 1, "right: 0→1 (A)");
    ExprUtil_MoveCursorRight(s, len, &cur); EXPECT_EQ(cur, 4, "right: 1→4 (√)");
    ExprUtil_MoveCursorRight(s, len, &cur); EXPECT_EQ(cur, 7, "right: 4→7 ([A])");
    ExprUtil_MoveCursorRight(s, len, &cur); EXPECT_EQ(cur, 8, "right: 7→8 (B)");
    ExprUtil_MoveCursorRight(s, len, &cur); EXPECT_EQ(cur, 8, "right: 8 at end, no-op");

    ExprUtil_MoveCursorLeft(s, &cur); EXPECT_EQ(cur, 7, "left: 8→7 (B)");
    ExprUtil_MoveCursorLeft(s, &cur); EXPECT_EQ(cur, 4, "left: 7→4 ([A])");
    ExprUtil_MoveCursorLeft(s, &cur); EXPECT_EQ(cur, 1, "left: 4→1 (√)");
    ExprUtil_MoveCursorLeft(s, &cur); EXPECT_EQ(cur, 0, "left: 1→0 (A)");
    ExprUtil_MoveCursorLeft(s, &cur); EXPECT_EQ(cur, 0, "left: 0 at start, no-op");
}

/* -------------------------------------------------------------------------- */
/* main                                                                         */
/* -------------------------------------------------------------------------- */

int main(void)
{
    test_utf8_char_size();
    test_utf8_byte_to_glyph();
    test_matrix_size_before();
    test_matrix_size_at();
    test_cursor_left();
    test_cursor_right();
    test_insert_char_insert_mode();
    test_insert_char_overwrite_mode();
    test_insert_str();
    test_delete_at_cursor();
    test_prepend_ans();
    test_cursor_roundtrip();

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
