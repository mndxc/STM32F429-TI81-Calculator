/**
 * @file    expr_buffer.c
 * @brief   ExprBuffer_t stateful wrapper — implements the five ExprBuffer_*
 *          API functions declared in expr_util.h.
 *
 * Zero LVGL / HAL dependencies.  Suitable for host-side unit testing.
 */

#include "expr_util.h"
#include <string.h>

void ExprBuffer_Insert(ExprBuffer_t *b, bool insert_mode, const char *s)
{
    size_t slen = strlen(s);
    if (slen == 1) {
        ExprUtil_InsertChar(b->buf, &b->len, &b->cursor,
                            MAX_EXPR_LEN, insert_mode, s[0]);
    } else if (slen > 1) {
        ExprUtil_InsertStr(b->buf, &b->len, &b->cursor, MAX_EXPR_LEN, s);
    }
}

void ExprBuffer_Delete(ExprBuffer_t *b)
{
    ExprUtil_DeleteAtCursor(b->buf, &b->len, &b->cursor);
}

void ExprBuffer_Left(ExprBuffer_t *b)
{
    ExprUtil_MoveCursorLeft(b->buf, &b->cursor);
}

void ExprBuffer_Right(ExprBuffer_t *b)
{
    ExprUtil_MoveCursorRight(b->buf, b->len, &b->cursor);
}

void ExprBuffer_Clear(ExprBuffer_t *b)
{
    b->buf[0] = '\0';
    b->len    = 0;
    b->cursor = 0;
}
