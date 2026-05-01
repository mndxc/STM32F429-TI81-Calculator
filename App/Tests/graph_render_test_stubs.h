/**
 * @file    graph_render_test_stubs.h
 * @brief   HOST_TEST LVGL stubs for graph.c (included by graph.c under HOST_TEST).
 *
 * graph.h's HOST_TEST guard defines lv_obj_t before this header is reached.
 * This header provides lv_color_t, lv_font_t, all LVGL constants, and function
 * stubs.  lv_color_hex() preserves the colour value and lv_color_to_u16() does
 * the real RGB888→RGB565 conversion so pixel-value assertions in
 * test_graph_render.c work correctly.
 *
 * All other LVGL calls (canvas init, label set_text, obj_invalidate, …) are
 * no-ops; they are compiled but not reached during Graph_Render() tests.
 */

#ifndef GRAPH_RENDER_TEST_STUBS_H
#define GRAPH_RENDER_TEST_STUBS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>   /* memset — used in graph_render_setup HOST_TEST branch */

/* lv_obj_t is already defined by graph.h's HOST_TEST guard.  Provide the
 * remaining types needed by graph.c's implementation. */
#ifndef LV_OBJ_T_DEFINED
#  define LV_OBJ_T_DEFINED
   typedef struct lv_obj_s { int dummy; } lv_obj_t;
#endif

typedef struct { uint32_t full; } lv_color_t;
typedef struct lv_font_s { int dummy; } lv_font_t;

/*---------------------------------------------------------------------------
 * LVGL constants
 *--------------------------------------------------------------------------*/
#define LV_OPA_COVER             0xFFU
#define LV_OPA_70                0xB2U
#define LV_OBJ_FLAG_HIDDEN       ((uint32_t)0x01U)
#define LV_OBJ_FLAG_SCROLLABLE   ((uint32_t)0x02U)
#define LV_COLOR_FORMAT_RGB565   4

/*---------------------------------------------------------------------------
 * Colour helpers — preserve RGB value so pixel assertions work
 *--------------------------------------------------------------------------*/

static inline lv_color_t lv_color_hex(uint32_t c)
{
    lv_color_t r; r.full = c; return r;
}

/* RGB888 → RGB565 (matches LVGL's own conversion) */
static inline uint16_t lv_color_to_u16(lv_color_t c)
{
    uint32_t r8 = (c.full >> 16) & 0xFFu;
    uint32_t g8 = (c.full >>  8) & 0xFFu;
    uint32_t b8 =  c.full        & 0xFFu;
    return (uint16_t)(((r8 & 0xF8u) << 8) | ((g8 & 0xFCu) << 3) | (b8 >> 3));
}

/*---------------------------------------------------------------------------
 * LVGL object / canvas / label — no-op stubs
 *--------------------------------------------------------------------------*/

static inline lv_obj_t *lv_obj_create(lv_obj_t *p)         { (void)p; return NULL; }
static inline lv_obj_t *lv_canvas_create(lv_obj_t *p)      { (void)p; return NULL; }
static inline lv_obj_t *lv_label_create(lv_obj_t *p)       { (void)p; return NULL; }

#define lv_canvas_set_buffer(o, buf, w, h, fmt)  ((void)(o))
#define lv_canvas_fill_bg(o, c, opa)             ((void)(o))
#define lv_obj_invalidate(o)                     ((void)(o))
#define lv_obj_set_size(o, w, h)                 ((void)(o))
#define lv_obj_set_pos(o, x, y)                  ((void)(o))
#define lv_obj_add_flag(o, f)                    ((void)(o))
#define lv_obj_clear_flag(o, f)                  ((void)(o))

#define lv_obj_set_style_bg_color(o, c, v)       ((void)(o))
#define lv_obj_set_style_bg_opa(o, v, s)         ((void)(o))
#define lv_obj_set_style_border_width(o, v, s)   ((void)(o))
#define lv_obj_set_style_pad_all(o, v, s)        ((void)(o))
#define lv_obj_set_style_pad_hor(o, v, s)        ((void)(o))
#define lv_obj_set_style_text_font(o, f, v)      ((void)(o))
#define lv_obj_set_style_text_color(o, c, v)     ((void)(o))
#define lv_obj_set_style_text_align(o, a, v)     ((void)(o))
#define lv_obj_set_style_radius(o, v, s)         ((void)(o))
#define lv_obj_set_height(o, h)                  ((void)(o))
#define lv_obj_set_width(o, w)                   ((void)(o))

#define lv_label_set_text(o, t)                  ((void)(o), (void)(t))
#define lv_label_set_text_fmt(o, ...)            ((void)(o))

#endif /* GRAPH_RENDER_TEST_STUBS_H */
