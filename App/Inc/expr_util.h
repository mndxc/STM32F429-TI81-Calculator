/**
 * @file    expr_util.h
 * @brief   Pure expression-buffer utility functions.
 *
 * All functions take explicit state parameters — no dependency on
 * LVGL, FreeRTOS, or HAL.  Suitable for host-side unit testing.
 */

#ifndef EXPR_UTIL_H
#define EXPR_UTIL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Returns the byte length of the UTF-8 character starting at s[0].
 *        Returns 1 for ASCII or any continuation/invalid byte (never stalls).
 *        Returns 0 only at the null terminator.
 */
uint8_t ExprUtil_Utf8CharSize(const char *s);

/**
 * @brief Converts a byte offset to a glyph (character) index.
 *        Used to pass a glyph index to LVGL label APIs.
 */
uint32_t ExprUtil_Utf8ByteToGlyph(const char *s, uint32_t byte_idx);

/**
 * @brief Returns 3 if the 3 bytes immediately before pos in buf form a
 *        matrix token ("[A]", "[B]", or "[C]"); otherwise returns 0.
 */
uint8_t ExprUtil_MatrixTokenSizeBefore(const char *buf, uint8_t pos);

/**
 * @brief Returns 3 if the 3 bytes starting at pos in buf form a matrix
 *        token ("[A]", "[B]", or "[C]"); otherwise returns 0.
 */
uint8_t ExprUtil_MatrixTokenSizeAt(const char *buf, uint8_t pos, uint8_t len);

/**
 * @brief Moves the cursor one character left (UTF-8 and matrix-token aware).
 *        No-op if cursor is already at 0.
 */
void ExprUtil_MoveCursorLeft(const char *buf, uint8_t *cursor);

/**
 * @brief Moves the cursor one character right (UTF-8 and matrix-token aware).
 *        No-op if cursor is already at len.
 */
void ExprUtil_MoveCursorRight(const char *buf, uint8_t len, uint8_t *cursor);

/**
 * @brief Inserts or overwrites a single character at *cursor.
 *
 * In overwrite mode and not at end: replaces the current character (treating
 * matrix tokens and multi-byte UTF-8 sequences as atomic units).
 * In insert mode, or at end: shifts the tail right.
 * No-op if the buffer would overflow max_len.
 */
void ExprUtil_InsertChar(char *buf, uint8_t *len, uint8_t *cursor,
                         uint8_t max_len, bool insert_mode, char c);

/**
 * @brief Inserts a string at *cursor, advancing *cursor by the string length.
 *        No-op if the buffer would overflow max_len.
 */
void ExprUtil_InsertStr(char *buf, uint8_t *len, uint8_t *cursor,
                        uint8_t max_len, const char *s);

/**
 * @brief Deletes the character immediately before *cursor (backspace).
 *        Handles matrix tokens and UTF-8 multi-byte sequences atomically.
 *        No-op if cursor is at 0.
 */
void ExprUtil_DeleteAtCursor(char *buf, uint8_t *len, uint8_t *cursor);

/**
 * @brief If the buffer is empty, prepends "ANS" and advances *cursor to 3.
 *        No-op if the buffer is non-empty or max_len < 4.
 */
void ExprUtil_PrependAns(char *buf, uint8_t *len, uint8_t *cursor,
                         uint8_t max_len);

#endif /* EXPR_UTIL_H */
