/**
 * @file    prgm_store_access.h
 * @brief   LVGL-free declarations for reading the program editor working buffer.
 *
 * Safe to include from Application Core modules (prgm_exec.c) without pulling
 * in lvgl.h.  ui_prgm.h includes this header so callers of ui_prgm.h continue
 * to resolve these symbols unchanged.
 *
 * Implementations live in prgm_editor.c (Prgm_GetLine, Prgm_GetNumLines) and
 * ui_prgm.c (prgm_parse_from_store).
 */
#ifndef PRGM_STORE_ACCESS_H
#define PRGM_STORE_ACCESS_H

#include <stdint.h>

/** Return the line at index @p ln from the current program editor buffer. */
const char *Prgm_GetLine(uint8_t ln);

/** Return the number of lines in the current program editor buffer. */
uint8_t Prgm_GetNumLines(void);

/** Load slot @p idx from g_prgm_store into the editor working buffer. */
void prgm_parse_from_store(uint8_t idx);

#endif /* PRGM_STORE_ACCESS_H */
