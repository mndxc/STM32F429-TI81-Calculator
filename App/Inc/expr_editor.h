/**
 * @file    expr_editor.h
 * @brief   Main expression editor — owns the expression buffer, STO-pending flag,
 *          and main-screen cursor objects.
 *
 * Extracted from calculator_core.c.  All expression buffer mutations and the
 * STO synthesis rule ("pass MODE_STO when sto_pending is true") now live here
 * rather than being repeated at every call site.
 *
 * Dependency note: the expression display itself is rendered by
 * ui_refresh_display() in calculator_core.c, which calls ExprEditor_CursorUpdate()
 * and ExprEditor_CursorHide() at the right points in its layout pass.
 * Callers in ui_input.c / ui_sto.c use the buffer-mutation API and never need
 * to see the LVGL cursor objects directly.
 */

#ifndef EXPR_EDITOR_H
#define EXPR_EDITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "app_common.h"  /* CalcMode_t */
#include "expr_util.h"   /* ExprBuffer_t, MAX_EXPR_LEN */

#ifndef HOST_TEST
#  include "lvgl.h"
#else
/* In HOST_TEST builds lv_obj_t is provided by calculator_core_test_stubs.h,
 * which must be included before expr_editor.h in the HOST_TEST include chain.
 * Re-declaring the typedef here (without the struct body) is compatible. */
struct lv_obj_s;
typedef struct lv_obj_s lv_obj_t;
#endif

/*---------------------------------------------------------------------------
 * Lifecycle
 *---------------------------------------------------------------------------*/

/** Create the main-screen cursor box / inner label on @p parent. No-op in HOST_TEST. */
void ExprEditor_Init(lv_obj_t *parent);

/*---------------------------------------------------------------------------
 * Expression buffer — read access
 *---------------------------------------------------------------------------*/

/** Returns a read-only pointer to the null-terminated expression string. */
const char *ExprEditor_GetBuf(void);

/** Current byte length of the expression (does not count the null terminator). */
int ExprEditor_GetLen(void);

/** Current cursor insertion point (byte offset into the expression string, not glyph index).
 *  0 is a valid position (cursor before first character) — not a sentinel or error value.
 *  Always points to the start byte of a UTF-8 character sequence. */
int ExprEditor_GetCursor(void);

/** Set cursor to @p pos (clamped to [0, len] by the caller). */
void ExprEditor_SetCursor(int pos);

/*---------------------------------------------------------------------------
 * STO-pending flag
 *---------------------------------------------------------------------------*/

bool ExprEditor_GetStoPending(void);
/** Set or clear the STO-pending transient prefix flag.
 *  When true, the next ExprEditor_CursorUpdate() call renders the cursor in
 *  MODE_STO (green 'A') regardless of the mode argument — this is the STO
 *  synthesis rule.  Set to true when TOKEN_STO is pressed; cleared on the
 *  next keypress that consumes or cancels the STO action. */
void ExprEditor_SetStoPending(bool v);

/*---------------------------------------------------------------------------
 * Cursor blink state (shared: toggled once per blink tick, read by all cursors)
 *---------------------------------------------------------------------------*/

bool ExprEditor_GetCursorVisible(void);
void ExprEditor_SetCursorVisible(bool v);

/*---------------------------------------------------------------------------
 * Cursor rendering — called from ui_refresh_display() in calculator_core.c
 *
 * ExprEditor_CursorUpdate: renders cursor_box onto @p row_label at @p char_pos,
 *   applying the STO synthesis rule internally (passes MODE_STO when sto_pending).
 * ExprEditor_CursorHide: hides cursor_box (called when expr cursor scrolls off-screen).
 *---------------------------------------------------------------------------*/

void ExprEditor_CursorUpdate(lv_obj_t *row_label, uint32_t char_pos,
                              CalcMode_t mode, bool insert_mode);
void ExprEditor_CursorHide(void);

/*---------------------------------------------------------------------------
 * Expression buffer mutations — each updates the internal ExprBuffer_t only;
 * callers are responsible for calling Update_Calculator_Display() afterwards.
 *---------------------------------------------------------------------------*/

/** Insert @p s at the cursor, advancing the cursor by its length. */
void ExprEditor_Insert(const char *s);

/**
 * Insert or overwrite a single character at cursor.
 * @p insert_mode: true = insert (shift right); false = overwrite (replace at cursor).
 */
void ExprEditor_InsertChar(char c, bool insert_mode);

/** Delete the character immediately before the cursor (UTF-8-aware backspace). */
void ExprEditor_Delete(void);

/** Clear the expression buffer and reset cursor to 0. */
void ExprEditor_Clear(void);

/** Move cursor left one glyph (UTF-8-aware). */
void ExprEditor_Left(void);

/** Move cursor right one glyph (UTF-8-aware). */
void ExprEditor_Right(void);

/**
 * If the expression is empty, prepend "ANS" and advance cursor.
 * No-op when the expression already has content.
 */
void ExprEditor_PrependAns(void);

/** Clear expression and set sto_pending = false. Used by Calc_ResetInputState(). */
void ExprEditor_Reset(void);

/**
 * Load @p s verbatim into the expression buffer.
 * Sets len and cursor to strlen(s).  Used by history_load_offset().
 */
void ExprEditor_LoadStr(const char *s);

#endif /* EXPR_EDITOR_H */
