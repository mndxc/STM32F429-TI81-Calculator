/**
 * @file    expr_util.c
 * @brief   Pure expression-buffer utility functions (no LVGL/HAL/RTOS).
 *
 * All state is passed explicitly so these functions can be tested on host
 * without any embedded dependencies.
 */

#include "expr_util.h"
#include <string.h>

uint8_t ExprUtil_Utf8CharSize(const char *s)
{
    uint8_t c = (uint8_t)s[0];
    if (c == 0)    return 0;
    if (c < 0x80)  return 1;   /* ASCII */
    if (c < 0xC0)  return 1;   /* Continuation byte — skip safely */
    if (c < 0xE0)  return 2;   /* 2-byte sequence (e.g. U+00C0–U+07FF) */
    if (c < 0xF0)  return 3;   /* 3-byte sequence (e.g. √ U+221A, ↑ U+2191) */
    return 4;                   /* 4-byte sequence */
}

uint32_t ExprUtil_Utf8ByteToGlyph(const char *s, uint32_t byte_idx)
{
    uint32_t glyph = 0;
    uint32_t i = 0;
    while (i < byte_idx && s[i] != '\0') {
        uint8_t sz = ExprUtil_Utf8CharSize(&s[i]);
        i += sz;
        glyph++;
    }
    return glyph;
}

uint8_t ExprUtil_MatrixTokenSizeBefore(const char *buf, uint8_t pos)
{
    if (pos < 3) return 0;
    if (buf[pos - 3] == '[' && buf[pos - 1] == ']' &&
        (buf[pos - 2] == 'A' || buf[pos - 2] == 'B' || buf[pos - 2] == 'C'))
        return 3;
    return 0;
}

uint8_t ExprUtil_MatrixTokenSizeAt(const char *buf, uint8_t pos, uint8_t len)
{
    if (pos + 3 > len) return 0;
    if (buf[pos] == '[' && buf[pos + 2] == ']' &&
        (buf[pos + 1] == 'A' || buf[pos + 1] == 'B' || buf[pos + 1] == 'C'))
        return 3;
    return 0;
}

void ExprUtil_MoveCursorLeft(const char *buf, uint8_t *cursor)
{
    if (*cursor == 0) return;
    if (ExprUtil_MatrixTokenSizeBefore(buf, *cursor) == 3) {
        *cursor -= 3;
    } else {
        do { (*cursor)--; }
        while (*cursor > 0 && ((uint8_t)buf[*cursor] & 0xC0) == 0x80);
    }
}

void ExprUtil_MoveCursorRight(const char *buf, uint8_t len, uint8_t *cursor)
{
    if (*cursor >= len) return;
    uint8_t mat = ExprUtil_MatrixTokenSizeAt(buf, *cursor, len);
    if (mat) {
        *cursor += mat;
    } else {
        (*cursor)++;
        while (*cursor < len && ((uint8_t)buf[*cursor] & 0xC0) == 0x80)
            (*cursor)++;
    }
}

void ExprUtil_InsertChar(char *buf, uint8_t *len, uint8_t *cursor,
                         uint8_t max_len, bool insert_mode, char c)
{
    if (!insert_mode && *cursor < *len) {
        /* Overwrite: remove all bytes of the current char, then write c.
         * Treat [A]/[B]/[C] as atomic (3 bytes); handle multi-byte UTF-8
         * (e.g. ≥) to avoid orphaned continuation bytes. */
        uint8_t cur_size = ExprUtil_MatrixTokenSizeAt(buf, *cursor, *len);
        if (!cur_size) cur_size = ExprUtil_Utf8CharSize(&buf[*cursor]);
        memmove(&buf[*cursor + 1], &buf[*cursor + cur_size],
                *len - *cursor - cur_size + 1);
        buf[*cursor] = c;
        *len = *len - cur_size + 1;
        (*cursor)++;
    } else {
        /* Insert: shift tail right, then write */
        if (*len + 1 > max_len) return;
        memmove(&buf[*cursor + 1], &buf[*cursor], *len - *cursor + 1);
        buf[*cursor] = c;
        (*len)++;
        (*cursor)++;
    }
}

void ExprUtil_InsertStr(char *buf, uint8_t *len, uint8_t *cursor,
                        uint8_t max_len, const char *s)
{
    uint8_t slen = (uint8_t)strlen(s);
    if (*len + slen >= max_len) return;
    memmove(&buf[*cursor + slen], &buf[*cursor], *len - *cursor + 1);
    memcpy(&buf[*cursor], s, slen);
    *len    += slen;
    *cursor += slen;
}

void ExprUtil_DeleteAtCursor(char *buf, uint8_t *len, uint8_t *cursor)
{
    if (*cursor == 0) return;
    /* Treat [A]/[B]/[C] as atomic — delete all 3 bytes at once. */
    uint8_t mat = ExprUtil_MatrixTokenSizeBefore(buf, *cursor);
    if (mat) {
        uint8_t start = *cursor - mat;
        memmove(&buf[start], &buf[*cursor], *len - *cursor + 1);
        *len   -= mat;
        *cursor = start;
        return;
    }
    /* Find the start byte of the UTF-8 character that ends just before cursor.
     * Without this, deleting a 3-byte sequence (e.g. ≥) would only remove the
     * last byte, leaving two orphaned continuation bytes. */
    uint8_t start = *cursor - 1;
    while (start > 0 && ((uint8_t)buf[start] & 0xC0) == 0x80)
        start--;
    uint8_t char_bytes = *cursor - start;
    memmove(&buf[start], &buf[*cursor], *len - *cursor + 1);
    *len   -= char_bytes;
    *cursor = start;
}

void ExprUtil_PrependAns(char *buf, uint8_t *len, uint8_t *cursor,
                         uint8_t max_len)
{
    if (*len == 0 && max_len > 3) {
        memcpy(buf, "ANS", 4);
        *len    = 3;
        *cursor = 3;
    }
}
