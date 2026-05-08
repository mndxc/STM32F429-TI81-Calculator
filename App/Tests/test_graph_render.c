/**
 * @file    test_graph_render.c
 * @brief   Host-compiled integration test: Graph_Render() → Calc_Parse/Eval pipeline.
 *
 * Build and run:
 *   cmake -S App/Tests -B App/Tests/build && cmake --build App/Tests/build
 *   ./App/Tests/build/test_graph_render
 *
 * Returns 0 on all pass, 1 on any failure.
 *
 * What is tested (F3 from docs/architecture_review_2026-04-30.md):
 *   Graph_Render() calls Calc_Parse() once per active equation and Calc_Eval()
 *   once per pixel column (320 columns).  Under HOST_TEST, the SDRAM pixel
 *   buffers in graph.c and graph_draw.c are redirected to in-memory static
 *   arrays.  Graph_GetTestBuf() exposes the pixel buffer so tests can assert
 *   exact RGB565 values at known canvas coordinates.
 *
 *   1. Smoke test   — no equations, no axes in window → all pixels are black.
 *   2. Constant eq  — Y1="5" → flat row of Y1-colour pixels at the expected row.
 *   3. Disabled eq  — enabled=false → no curve pixels drawn.
 *   4. Cache update — changing the equation string triggers re-parse and the
 *                     new curve appears at the correct row; old row is cleared.
 *   5. Bad expr     — malformed expression → Calc_Parse fails gracefully, no crash.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* LVGL stubs must come before graph.h so lv_color_t etc. are available. */
#include "graph_render_test_stubs.h"
#include "graph.h"
#include "graph_coord.h"     /* graph_coord_* inline transforms */
#include "ui_palette.h"      /* COLOR_CURVE_Y1 for expected pixel value */

/* -------------------------------------------------------------------------
 * Stubs for calculator_core.c symbols referenced by graph.c / graph_draw.c
 * ---------------------------------------------------------------------- */

bool Calc_GetAngleDegrees(void) { return false; }   /* radians mode */

/* -------------------------------------------------------------------------
 * Test accessor declared in graph.c's HOST_TEST section
 * ---------------------------------------------------------------------- */

const uint16_t *Graph_GetTestBuf(void);

/* -------------------------------------------------------------------------
 * Expected Y1 curve colour
 *
 * COLOR_CURVE_Y1 = 0xFFFFFF.  lv_color_hex(0xFFFFFF).full = 0xFFFFFF.
 * lv_color_to_u16: (0xFF & 0xF8)<<8 | (0xFF & 0xFC)<<3 | (0xFF>>3)
 *                = 0xF800 | 0x07E0 | 0x001F = 0xFFFF.
 * -------------------------------------------------------------------- */
#define EXPECTED_Y1_PX  ((uint16_t)0xFFFFu)

/* -------------------------------------------------------------------------
 * Test infrastructure
 * ---------------------------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name) do {                                       \
    if (cond) {                                                      \
        g_passed++;                                                  \
    } else {                                                         \
        g_failed++;                                                  \
        printf("  FAIL [line %d]: %s\n", __LINE__, (name));         \
    }                                                                \
} while (0)

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/* Reset all graph state to a deterministic baseline before each test.
 * Window [1,11]×[0,10]: y-axis (x=0) outside view; x-axis (y=0) at row 239. */
static void setup_graph(void)
{
    for (int i = 0; i < GRAPH_NUM_EQ; i++) {
        Graph_SetEquationEnabled(i, false);
        char *b = Graph_GetEquationBuf(i);
        if (b) b[0] = '\0';
    }
    Graph_InvalidateCache();
    Graph_SetConnectedMode(false);    /* dot mode — one pixel per column */
    Graph_SetSequentialMode(true);
    Graph_SetGridOn(false);
    Graph_SetParamMode(false);
    Graph_SetWindow(1.0f, 11.0f, 0.0f, 10.0f, 1.0f, 1.0f, 1.0f);
}

/* Expected canvas row for a Y=y_math value with the baseline window y=[0,10].
 * Matches math_y_to_px() in graph.c: (int32_t)((y_max-y)/range*(GRAPH_H-1)) */
static int32_t expected_row(float y_math)
{
    return (int32_t)((10.0f - y_math) / (10.0f - 0.0f) * (float)(GRAPH_H - 1));
}

/* -------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------- */

static void test_render_empty(void)
{
    printf("test_render_empty\n");
    setup_graph();
    /* Override window so neither axis falls inside view: both 0s are excluded */
    Graph_SetWindow(2.0f, 12.0f, 2.0f, 12.0f, 1.0f, 1.0f, 1.0f);

    Graph_Render();

    const uint16_t *buf = Graph_GetTestBuf();
    CHECK(buf[0] == 0,
          "empty: top-left pixel is background (0)");
    CHECK(buf[(GRAPH_H - 1) * GRAPH_W + (GRAPH_W - 1)] == 0,
          "empty: bottom-right pixel is background (0)");
    CHECK(buf[(GRAPH_H / 2) * GRAPH_W + (GRAPH_W / 2)] == 0,
          "empty: centre pixel is background (0)");
}

static void test_render_constant_y1(void)
{
    printf("test_render_constant_y1\n");
    setup_graph();
    char *eq = Graph_GetEquationBuf(0);
    strncpy(eq, "5", GRAPH_EQUATION_BUF_LEN - 1);
    eq[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
    Graph_SetEquationEnabled(0, true);

    Graph_Render();

    /* math_y_to_px(5) with y=[0,10]: (int32_t)(0.5 * 239) = 119 */
    int32_t row = expected_row(5.0f);
    const uint16_t *buf = Graph_GetTestBuf();

    CHECK(buf[row * GRAPH_W + 0]           == EXPECTED_Y1_PX,
          "Y1=5: first column (px=0) has Y1 colour");
    CHECK(buf[row * GRAPH_W + GRAPH_W / 2] == EXPECTED_Y1_PX,
          "Y1=5: centre column has Y1 colour");
    CHECK(buf[row * GRAPH_W + GRAPH_W - 1] == EXPECTED_Y1_PX,
          "Y1=5: last column (px=319) has Y1 colour");

    /* Rows away from the curve and both axes should be black */
    CHECK(buf[50 * GRAPH_W + GRAPH_W / 2] == 0,
          "Y1=5: row 50 is background");
    CHECK(buf[200 * GRAPH_W + GRAPH_W / 2] == 0,
          "Y1=5: row 200 is background");
}

static void test_render_disabled_equation(void)
{
    printf("test_render_disabled_equation\n");
    setup_graph();
    char *eq = Graph_GetEquationBuf(0);
    strncpy(eq, "5", GRAPH_EQUATION_BUF_LEN - 1);
    eq[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
    Graph_SetEquationEnabled(0, false);   /* explicitly disabled */

    Graph_Render();

    int32_t row = expected_row(5.0f);
    const uint16_t *buf = Graph_GetTestBuf();
    CHECK(buf[row * GRAPH_W + GRAPH_W / 2] == 0,
          "disabled Y1=5: no curve pixels at expected row");
}

static void test_render_cache_invalidation(void)
{
    printf("test_render_cache_invalidation\n");
    setup_graph();
    char *eq = Graph_GetEquationBuf(0);
    strncpy(eq, "5", GRAPH_EQUATION_BUF_LEN - 1);
    eq[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
    Graph_SetEquationEnabled(0, true);

    Graph_Render();

    int32_t row5 = expected_row(5.0f);   /* 119 */
    const uint16_t *buf = Graph_GetTestBuf();
    CHECK(buf[row5 * GRAPH_W + GRAPH_W / 2] == EXPECTED_Y1_PX,
          "cache: first render Y1=5 draws at row 119");

    /* Change equation string — Graph_Render() detects via strncmp and re-parses */
    strncpy(eq, "2", GRAPH_EQUATION_BUF_LEN - 1);
    eq[GRAPH_EQUATION_BUF_LEN - 1] = '\0';

    Graph_Render();

    /* math_y_to_px(2) with y=[0,10]: (int32_t)(0.8 * 239) = 191 */
    int32_t row2 = expected_row(2.0f);   /* 191 */
    buf = Graph_GetTestBuf();
    CHECK(buf[row2 * GRAPH_W + GRAPH_W / 2] == EXPECTED_Y1_PX,
          "cache: second render Y1=2 draws at row 191");
    /* memset in graph_render_setup clears the old row before drawing the new curve */
    CHECK(buf[row5 * GRAPH_W + GRAPH_W / 2] == 0,
          "cache: old curve row 119 cleared after re-render with Y1=2");
}

/* Verify graph_coord_* transforms against known values for the baseline window
 * x=[1,11], y=[0,10] (GRAPH_W=320, GRAPH_H=240). */
static void test_coord_transforms(void)
{
    printf("test_coord_transforms\n");

    GraphState_t s = {
        .x_min = 1.0f, .x_max = 11.0f,
        .y_min = 0.0f, .y_max = 10.0f,
    };

    /* math_x_to_px: x=1 → px 0, x=11 → px 319 (GRAPH_W-1) */
    CHECK(graph_coord_math_x_to_px(&s, 1.0f)  == 0,
          "coord: math_x_to_px(x=x_min) == 0");
    CHECK(graph_coord_math_x_to_px(&s, 11.0f) == GRAPH_W - 1,
          "coord: math_x_to_px(x=x_max) == GRAPH_W-1");

    /* math_y_to_px: y=10 → row 0 (top), y=0 → row 239 (GRAPH_H-1, bottom) */
    CHECK(graph_coord_math_y_to_px(&s, 10.0f) == 0,
          "coord: math_y_to_px(y=y_max) == 0");
    CHECK(graph_coord_math_y_to_px(&s, 0.0f)  == GRAPH_H - 1,
          "coord: math_y_to_px(y=y_min) == GRAPH_H-1");

    /* px_to_math_x: px=0 → x_min, px=GRAPH_W-1 → x_max */
    float mx0 = graph_coord_px_to_math_x(&s, 0);
    float mxN = graph_coord_px_to_math_x(&s, GRAPH_W - 1);
    CHECK(fabsf(mx0 - 1.0f)  < 1e-4f,
          "coord: px_to_math_x(0) == x_min");
    CHECK(fabsf(mxN - 11.0f) < 1e-4f,
          "coord: px_to_math_x(GRAPH_W-1) == x_max");

    /* px_to_math_y: row=0 → y_max, row=GRAPH_H-1 → y_min */
    float my0 = graph_coord_px_to_math_y(&s, 0);
    float myN = graph_coord_px_to_math_y(&s, GRAPH_H - 1);
    CHECK(fabsf(my0 - 10.0f) < 1e-4f,
          "coord: px_to_math_y(0) == y_max");
    CHECK(fabsf(myN - 0.0f)  < 1e-4f,
          "coord: px_to_math_y(GRAPH_H-1) == y_min");
}

static void test_render_invalid_expression(void)
{
    printf("test_render_invalid_expression\n");
    setup_graph();
    char *eq = Graph_GetEquationBuf(0);
    strncpy(eq, "!!bad!!", GRAPH_EQUATION_BUF_LEN - 1);
    eq[GRAPH_EQUATION_BUF_LEN - 1] = '\0';
    Graph_SetEquationEnabled(0, true);

    /* Calc_Parse must return !CALC_OK; Graph_Render must not crash */
    Graph_Render();

    int32_t row = expected_row(5.0f);
    const uint16_t *buf = Graph_GetTestBuf();
    CHECK(buf[row * GRAPH_W + GRAPH_W / 2] == 0,
          "bad expr: no curve pixels drawn for malformed equation");
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void)
{
    printf("=== test_graph_render ===\n");

    test_coord_transforms();
    test_render_empty();
    test_render_constant_y1();
    test_render_disabled_equation();
    test_render_cache_invalidation();
    test_render_invalid_expression();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
