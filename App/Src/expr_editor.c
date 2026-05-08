/**
 * @file    expr_editor.c
 * @brief   Main expression editor — owns the expression buffer, STO-pending flag,
 *          and main-screen cursor objects (extracted from calculator_core.c).
 *
 * State owned here:
 *   expr          — the live expression buffer (text + length + cursor position)
 *   sto_pending   — true after STO; next alpha key commits ans to a variable
 *   cursor_visible — shared blink state toggled by the cursor timer
 *   cursor_box / cursor_inner — LVGL objects for the main-screen block cursor
 *
 * In HOST_TEST builds the LVGL objects are absent (cursor_render is a no-op stub)
 * and expr / sto_pending / cursor_visible are non-static so test code can read
 * and write them directly via the extern declarations in calculator_core_test_stubs.h.
 */

#ifdef HOST_TEST
#  include "app_common.h"
#  include "expr_util.h"
#  include "expr_editor.h"
#else
#  include "ui_shared.h"   /* cursor_render, cursor_box_create, lvgl_lock/unlock */
#  include "expr_util.h"
#  include "expr_editor.h"
#endif
#include <string.h>

/*---------------------------------------------------------------------------
 * State
 *---------------------------------------------------------------------------*/

/* In HOST_TEST builds these must be non-static so test_normal_mode.c can
 * read and write them directly through the extern declarations in
 * calculator_core_test_stubs.h — the same pattern used for current_mode,
 * insert_mode, ans, etc. in calculator_core.c. */
#ifdef HOST_TEST
ExprBuffer_t expr         = {{0}, 0, 0};
bool         sto_pending  = false;
bool         cursor_visible = true;
#else
static ExprBuffer_t expr         = {{0}, 0, 0};
static bool         sto_pending  = false;
static bool         cursor_visible = true;
static lv_obj_t    *cursor_box   = NULL;
static lv_obj_t    *cursor_inner = NULL;
#endif

/*---------------------------------------------------------------------------
 * Lifecycle
 *---------------------------------------------------------------------------*/

void ExprEditor_Init(lv_obj_t *parent)
{
#ifndef HOST_TEST
    cursor_box_create(parent, false, &cursor_box, &cursor_inner);
#else
    (void)parent;
#endif
}

/*---------------------------------------------------------------------------
 * Buffer read access
 *---------------------------------------------------------------------------*/

const char *ExprEditor_GetBuf(void)    { return expr.buf; }
int         ExprEditor_GetLen(void)    { return (int)expr.len; }
int         ExprEditor_GetCursor(void) { return (int)expr.cursor; }
void        ExprEditor_SetCursor(int pos) { expr.cursor = (uint8_t)pos; }

/*---------------------------------------------------------------------------
 * STO-pending flag
 *---------------------------------------------------------------------------*/

bool ExprEditor_GetStoPending(void)    { return sto_pending; }
void ExprEditor_SetStoPending(bool v)  { sto_pending = v; }

/*---------------------------------------------------------------------------
 * Cursor blink state
 *---------------------------------------------------------------------------*/

bool ExprEditor_GetCursorVisible(void)    { return cursor_visible; }
void ExprEditor_SetCursorVisible(bool v)  { cursor_visible = v; }

/*---------------------------------------------------------------------------
 * Cursor rendering
 *---------------------------------------------------------------------------*/

/**
 * Render cursor_box onto row_label at char_pos, synthesising MODE_STO from
 * sto_pending.  This is the single site where the STO cursor synthesis rule lives.
 */
void ExprEditor_CursorUpdate(lv_obj_t *row_label, uint32_t char_pos,
                              CalcMode_t mode, bool insert_mode)
{
#ifndef HOST_TEST
    CalcMode_t display_mode = sto_pending ? MODE_STO : mode;
    cursor_render(cursor_box, cursor_inner, row_label, char_pos,
                  cursor_visible, display_mode, insert_mode);
#else
    (void)row_label; (void)char_pos; (void)mode; (void)insert_mode;
#endif
}

void ExprEditor_CursorHide(void)
{
#ifndef HOST_TEST
    if (cursor_box != NULL)
        lv_obj_add_flag(cursor_box, LV_OBJ_FLAG_HIDDEN);
#endif
}

/*---------------------------------------------------------------------------
 * Expression buffer mutations
 *---------------------------------------------------------------------------*/

void ExprEditor_Insert(const char *s)
{
    ExprUtil_InsertStr(expr.buf, &expr.len, &expr.cursor, MAX_EXPR_LEN, s);
}

void ExprEditor_InsertChar(char c, bool insert_mode)
{
    ExprUtil_InsertChar(expr.buf, &expr.len, &expr.cursor, MAX_EXPR_LEN, insert_mode, c);
}

void ExprEditor_Delete(void)
{
    ExprBuffer_Delete(&expr);
}

void ExprEditor_Clear(void)
{
    ExprBuffer_Clear(&expr);
}

void ExprEditor_Left(void)
{
    ExprBuffer_Left(&expr);
}

void ExprEditor_Right(void)
{
    ExprBuffer_Right(&expr);
}

void ExprEditor_PrependAns(void)
{
    ExprUtil_PrependAns(expr.buf, &expr.len, &expr.cursor, MAX_EXPR_LEN);
}

void ExprEditor_Reset(void)
{
    sto_pending = false;
    ExprBuffer_Clear(&expr);
}

void ExprEditor_LoadStr(const char *s)
{
    size_t len = strlen(s);
    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(expr.buf, s, len);
    expr.buf[len] = '\0';
    expr.len    = (uint8_t)len;
    expr.cursor = (uint8_t)len;
}
