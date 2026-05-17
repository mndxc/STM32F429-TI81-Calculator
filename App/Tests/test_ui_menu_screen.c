/**
 * @file    test_ui_menu_screen.c
 * @brief   Host-side unit tests for MenuScreen_t navigation (ui_menu_screen.c).
 *
 * Compiled with HOST_TEST=1: MenuScreen_Init/UpdateDisplay/SetTab are no-ops
 * for LVGL; HandleToken navigation logic runs unchanged.
 *
 * Build:
 *   cmake -S App/Tests -B build-tests && cmake --build build-tests
 *   ./build-tests/test_ui_menu_screen   # exits 0 on full pass
 */

#include "../Inc/ui_menu_screen.h"
#include <stdio.h>
#include <string.h>

/* lvgl_lock/unlock stubs required by HandleToken */
void lvgl_lock(void)   {}
void lvgl_unlock(void) {}

/* -------------------------------------------------------------------------- */
/* Minimal test framework                                                      */
/* -------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT_EQ(a, b, msg) do { \
    if ((int)(a) == (int)(b)) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s:%d] %s: got %d, expected %d\n", \
                            __FILE__, __LINE__, (msg), (int)(a), (int)(b)); } \
} while (0)

#define EXPECT_TRUE(expr, msg)  EXPECT_EQ(!!(expr), 1, msg)
#define EXPECT_FALSE(expr, msg) EXPECT_EQ(!!(expr), 0, msg)

/* -------------------------------------------------------------------------- */
/* Shared callback state                                                       */
/* -------------------------------------------------------------------------- */

static int        g_select_idx    = -1;
static int        g_select_count  = 0;
static bool       g_cancel_called = false;
static CalcMode_t g_tab_switch_mode = 0;
static bool       g_extra_consumed  = false;

static void reset_callbacks(void)
{
    g_select_idx      = -1;
    g_select_count    = 0;
    g_cancel_called   = false;
    g_tab_switch_mode = 0;
    g_extra_consumed  = false;
}

static void cb_select(int idx, lv_obj_t *screen)
{
    (void)screen;
    g_select_idx = idx;
    g_select_count++;
}

static void cb_cancel(lv_obj_t *screen)
{
    (void)screen;
    g_cancel_called = true;
}

static void cb_tab_switch(lv_obj_t *screen, CalcMode_t mode)
{
    (void)screen;
    g_tab_switch_mode = mode;
}

static bool cb_extra_consume(Token_t t, MenuScreen_t *ms)
{
    (void)t; (void)ms;
    g_extra_consumed = true;
    return true;
}

static bool cb_extra_passthrough(Token_t t, MenuScreen_t *ms)
{
    (void)t; (void)ms;
    return false;
}

/* -------------------------------------------------------------------------- */
/* Fixtures — single-tab menu (5 items, no tab bar)                           */
/* -------------------------------------------------------------------------- */

static const char * const s5_labels[5] = {
    "1:A", "2:B", "3:C", "4:D", "5:E"
};
static const MenuTabDesc_t s5_tabs[1] = {
    { 5, s5_labels, NULL, cb_select }
};
static const MenuScreenDesc_t s5_desc = {
    .tab_count   = 0,
    .tab_names   = NULL,
    .tab_x       = NULL,
    .default_tab = 0,
    .wrap_tabs   = false,
    .title       = "TEST",
    .tabs        = s5_tabs,
    .left_mode   = 0,
    .right_mode  = 0,
    .on_cancel   = cb_cancel,
    .on_tab_switch = NULL,
    .on_extra    = NULL,
};

/* -------------------------------------------------------------------------- */
/* Fixtures — 10-item single-tab menu (for scroll testing)                    */
/* -------------------------------------------------------------------------- */

static const char * const s10_labels[10] = {
    "1:A","2:B","3:C","4:D","5:E","6:F","7:G","8:H","9:I","0:J"
};
static const MenuTabDesc_t s10_tabs[1] = {
    { 10, s10_labels, NULL, cb_select }
};
static const MenuScreenDesc_t s10_desc = {
    .tab_count   = 0,
    .tab_names   = NULL,
    .tab_x       = NULL,
    .default_tab = 0,
    .wrap_tabs   = false,
    .title       = NULL,
    .tabs        = s10_tabs,
    .left_mode   = 0,
    .right_mode  = 0,
    .on_cancel   = cb_cancel,
    .on_tab_switch = NULL,
    .on_extra    = NULL,
};

/* -------------------------------------------------------------------------- */
/* Fixtures — 3-tab within-menu menu (wrap and no-wrap variants)              */
/* -------------------------------------------------------------------------- */

static const char * const tab0_labels[3] = {"1:X","2:Y","3:Z"};
static const char * const tab1_labels[2] = {"1:A","2:B"};
static const char * const tab2_labels[4] = {"1:P","2:Q","3:R","4:S"};
static const char * const mt_tab_names[3] = {"T0","T1","T2"};
static const int           mt_tab_x[3]    = {4, 60, 116};

static const MenuTabDesc_t mt_tabs[3] = {
    { 3, tab0_labels, NULL, cb_select },
    { 2, tab1_labels, NULL, cb_select },
    { 4, tab2_labels, NULL, cb_select },
};

static const MenuScreenDesc_t mt_desc_wrap = {
    .tab_count     = 3,
    .tab_names     = mt_tab_names,
    .tab_x         = mt_tab_x,
    .default_tab   = 0,
    .wrap_tabs     = true,
    .title         = NULL,
    .tabs          = mt_tabs,
    .left_mode     = 0,
    .right_mode    = 0,
    .on_cancel     = cb_cancel,
    .on_tab_switch = NULL,
    .on_extra      = NULL,
};

static const MenuScreenDesc_t mt_desc_nowrap = {
    .tab_count     = 3,
    .tab_names     = mt_tab_names,
    .tab_x         = mt_tab_x,
    .default_tab   = 0,
    .wrap_tabs     = false,
    .title         = NULL,
    .tabs          = mt_tabs,
    .left_mode     = 0,
    .right_mode    = 0,
    .on_cancel     = cb_cancel,
    .on_tab_switch = NULL,
    .on_extra      = NULL,
};

/* -------------------------------------------------------------------------- */
/* Fixtures — sibling-mode menu (CTL/I/O/EXEC pattern)                        */
/* -------------------------------------------------------------------------- */

static const char * const sib_tab_names[3] = {"CTL","I/O","EXEC"};
static const int           sib_tab_x[3]    = {4, 80, 156};

static const MenuTabDesc_t sib_tabs_ctl[3] = {
    { 5, s5_labels, NULL, cb_select },
    { 0, NULL, NULL, NULL },
    { 0, NULL, NULL, NULL },
};

static const MenuScreenDesc_t sib_desc = {
    .tab_count     = 3,
    .tab_names     = sib_tab_names,
    .tab_x         = sib_tab_x,
    .default_tab   = 0,
    .wrap_tabs     = false,
    .title         = NULL,
    .tabs          = sib_tabs_ctl,
    .left_mode     = (CalcMode_t)99,   /* sentinel: not MODE_NORMAL */
    .right_mode    = (CalcMode_t)100,
    .on_cancel     = cb_cancel,
    .on_tab_switch = cb_tab_switch,
    .on_extra      = NULL,
};

/* -------------------------------------------------------------------------- */
/* Helper: init ms for a descriptor                                            */
/* -------------------------------------------------------------------------- */

static void ms_init(MenuScreen_t *ms, const MenuScreenDesc_t *desc)
{
    MenuScreen_Init(ms, desc, NULL);
}

/* -------------------------------------------------------------------------- */
/* Group 1: digit shortcuts                                                    */
/* -------------------------------------------------------------------------- */

static void test_digit_shortcuts(void)
{
    printf("Group 1: digit shortcuts\n");
    MenuScreen_t ms;
    ms_init(&ms, &s5_desc);

    reset_callbacks();
    EXPECT_TRUE(MenuScreen_HandleToken(&ms, TOKEN_1), "TOKEN_1 returns true");
    EXPECT_EQ(g_select_idx, 0, "TOKEN_1 -> idx 0");
    EXPECT_EQ(g_select_count, 1, "TOKEN_1 -> called once");

    reset_callbacks();
    EXPECT_TRUE(MenuScreen_HandleToken(&ms, TOKEN_5), "TOKEN_5 returns true");
    EXPECT_EQ(g_select_idx, 4, "TOKEN_5 -> idx 4");

    /* TOKEN_0 maps to index 9, out of range for 5-item list — no call */
    reset_callbacks();
    EXPECT_TRUE(MenuScreen_HandleToken(&ms, TOKEN_0), "TOKEN_0 (OOR) returns true");
    EXPECT_EQ(g_select_count, 0, "TOKEN_0 OOR -> no select call");

    /* TOKEN_6 also out of range */
    reset_callbacks();
    EXPECT_TRUE(MenuScreen_HandleToken(&ms, TOKEN_6), "TOKEN_6 (OOR) returns true");
    EXPECT_EQ(g_select_count, 0, "TOKEN_6 OOR -> no select call");
}

/* -------------------------------------------------------------------------- */
/* Group 2: UP/DOWN navigation with scroll                                     */
/* -------------------------------------------------------------------------- */

static void test_up_down_scroll(void)
{
    printf("Group 2: UP/DOWN navigation with scroll\n");
    MenuScreen_t ms;
    ms_init(&ms, &s10_desc);  /* 10 items, MENU_VISIBLE_ROWS=7 */

    /* Initial state */
    EXPECT_EQ(ms.nav.cursor, 0, "Initial cursor 0");
    EXPECT_EQ(ms.nav.scroll, 0, "Initial scroll 0");

    /* Press DOWN 6 times — cursor should reach row 6 (end of window) */
    for (int i = 0; i < 6; i++)
        MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    EXPECT_EQ(ms.nav.cursor, 6, "After 6 DOWN: cursor=6");
    EXPECT_EQ(ms.nav.scroll, 0, "After 6 DOWN: scroll=0");

    /* One more DOWN — cursor stays at 6, scroll advances */
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    EXPECT_EQ(ms.nav.cursor, 6, "After 7th DOWN: cursor stays 6");
    EXPECT_EQ(ms.nav.scroll, 1, "After 7th DOWN: scroll=1");

    /* Two more DOWNs — at last item (abs=9): scroll and cursor stay */
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    EXPECT_EQ(ms.nav.scroll + ms.nav.cursor, 9, "At last item: abs=9");

    /* Boundary: another DOWN does nothing */
    int prev_abs = (int)ms.nav.scroll + (int)ms.nav.cursor;
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    EXPECT_EQ((int)ms.nav.scroll + (int)ms.nav.cursor, prev_abs, "DOWN at last: no change");

    /* UP at top of window scrolls the window */
    ms.nav.cursor = 0;
    ms.nav.scroll = 3;
    MenuScreen_HandleToken(&ms, TOKEN_UP);
    EXPECT_EQ(ms.nav.cursor, 0, "UP with scroll: cursor stays 0");
    EXPECT_EQ(ms.nav.scroll, 2, "UP with scroll: scroll--");

    /* UP when at top of everything (cursor=0, scroll=0): no change */
    ms.nav.cursor = 0;
    ms.nav.scroll = 0;
    MenuScreen_HandleToken(&ms, TOKEN_UP);
    EXPECT_EQ(ms.nav.cursor, 0, "UP at top: cursor stays 0");
    EXPECT_EQ(ms.nav.scroll, 0, "UP at top: scroll stays 0");
}

/* -------------------------------------------------------------------------- */
/* Group 3: LEFT/RIGHT with within-menu wrap                                   */
/* -------------------------------------------------------------------------- */

static void test_tab_wrap(void)
{
    printf("Group 3: LEFT/RIGHT with within-menu wrap\n");
    MenuScreen_t ms;
    ms_init(&ms, &mt_desc_wrap);

    /* Initial tab = 0 */
    EXPECT_EQ(ms.active_tab, 0, "Initial active_tab=0");

    /* RIGHT advances tab */
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ(ms.active_tab, 1, "RIGHT: tab 0->1");
    EXPECT_EQ(ms.nav.cursor, 0, "RIGHT: cursor reset");

    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ(ms.active_tab, 2, "RIGHT: tab 1->2");

    /* RIGHT at last tab wraps to 0 */
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ(ms.active_tab, 0, "RIGHT wrap: tab 2->0");

    /* LEFT at tab 0 wraps to last */
    MenuScreen_HandleToken(&ms, TOKEN_LEFT);
    EXPECT_EQ(ms.active_tab, 2, "LEFT wrap: tab 0->2");

    /* LEFT advances backward */
    MenuScreen_HandleToken(&ms, TOKEN_LEFT);
    EXPECT_EQ(ms.active_tab, 1, "LEFT: tab 2->1");
}

/* -------------------------------------------------------------------------- */
/* Group 4: LEFT/RIGHT without wrap — boundary behaviour                       */
/* -------------------------------------------------------------------------- */

static void test_tab_nowrap(void)
{
    printf("Group 4: LEFT/RIGHT without wrap\n");
    MenuScreen_t ms;
    ms_init(&ms, &mt_desc_nowrap);

    /* LEFT at tab 0 — stays at 0, but still returns true (consumes token) */
    bool ret = MenuScreen_HandleToken(&ms, TOKEN_LEFT);
    EXPECT_EQ(ms.active_tab, 0, "LEFT no-wrap at 0: tab stays 0");
    EXPECT_TRUE(ret, "LEFT no-wrap: returns true");

    /* RIGHT to last tab */
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ(ms.active_tab, 2, "After 2 RIGHTs: tab=2");

    /* RIGHT at last tab — stays at 2, returns true */
    ret = MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ(ms.active_tab, 2, "RIGHT no-wrap at last: tab stays 2");
    EXPECT_TRUE(ret, "RIGHT no-wrap at last: returns true");
}

/* -------------------------------------------------------------------------- */
/* Group 5: CLEAR calls on_cancel                                              */
/* -------------------------------------------------------------------------- */

static void test_clear(void)
{
    printf("Group 5: CLEAR calls on_cancel\n");
    MenuScreen_t ms;
    ms_init(&ms, &s5_desc);

    reset_callbacks();
    bool ret = MenuScreen_HandleToken(&ms, TOKEN_CLEAR);
    EXPECT_TRUE(ret, "CLEAR returns true");
    EXPECT_TRUE(g_cancel_called, "CLEAR calls on_cancel");
}

/* -------------------------------------------------------------------------- */
/* Group 6: unknown token returns false (no on_extra)                          */
/* -------------------------------------------------------------------------- */

static void test_unknown_token(void)
{
    printf("Group 6: unknown token returns false\n");
    MenuScreen_t ms;
    ms_init(&ms, &s5_desc);

    /* TOKEN_ALPHA is not handled — should return false */
    bool ret = MenuScreen_HandleToken(&ms, TOKEN_ALPHA);
    EXPECT_FALSE(ret, "TOKEN_ALPHA with no on_extra returns false");

    /* TOKEN_MATH also falls through */
    ret = MenuScreen_HandleToken(&ms, TOKEN_MATH);
    EXPECT_FALSE(ret, "TOKEN_MATH with no on_extra returns false");
}

/* -------------------------------------------------------------------------- */
/* Group 7: on_extra precedence                                                */
/* -------------------------------------------------------------------------- */

static void test_on_extra(void)
{
    printf("Group 7: on_extra precedence\n");

    /* on_extra that consumes token */
    MenuScreenDesc_t desc = s5_desc;
    desc.on_extra = cb_extra_consume;
    MenuScreen_t ms;
    ms_init(&ms, &desc);

    reset_callbacks();
    bool ret = MenuScreen_HandleToken(&ms, TOKEN_ALPHA);
    EXPECT_TRUE(ret, "on_extra consuming: returns true");
    EXPECT_TRUE(g_extra_consumed, "on_extra consuming: callback called");

    /* on_extra that passes through */
    desc.on_extra = cb_extra_passthrough;
    ms_init(&ms, &desc);
    reset_callbacks();
    ret = MenuScreen_HandleToken(&ms, TOKEN_ALPHA);
    EXPECT_FALSE(ret, "on_extra pass-through: returns false");

    /* on_extra not called for tokens handled by generic logic (UP/DOWN/etc.) */
    desc.on_extra = cb_extra_consume;
    ms_init(&ms, &desc);
    reset_callbacks();
    MenuScreen_HandleToken(&ms, TOKEN_UP);
    EXPECT_FALSE(g_extra_consumed, "on_extra NOT called for TOKEN_UP");
}

/* -------------------------------------------------------------------------- */
/* Group 8: sibling-mode LEFT/RIGHT calls on_tab_switch                        */
/* -------------------------------------------------------------------------- */

static void test_sibling_tab_switch(void)
{
    printf("Group 8: sibling-mode LEFT/RIGHT calls on_tab_switch\n");
    MenuScreen_t ms;
    ms_init(&ms, &sib_desc);

    reset_callbacks();
    MenuScreen_HandleToken(&ms, TOKEN_LEFT);
    EXPECT_EQ((int)g_tab_switch_mode, 99, "LEFT calls on_tab_switch with left_mode");

    reset_callbacks();
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ((int)g_tab_switch_mode, 100, "RIGHT calls on_tab_switch with right_mode");

    /* active_tab must not change in sibling mode (on_tab_switch handles it) */
    EXPECT_EQ(ms.active_tab, 0, "Sibling mode: active_tab unchanged by L/R");
}

/* -------------------------------------------------------------------------- */
/* Group 9: ENTER selects the cursor item                                      */
/* -------------------------------------------------------------------------- */

static void test_enter_select(void)
{
    printf("Group 9: ENTER selects cursor item\n");
    MenuScreen_t ms;
    ms_init(&ms, &s5_desc);

    /* Move down 2 — cursor at 2, abs=2 */
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);

    reset_callbacks();
    MenuScreen_HandleToken(&ms, TOKEN_ENTER);
    EXPECT_EQ(g_select_idx, 2, "ENTER with cursor=2: idx=2");

    /* After SetTab cursor resets */
    ms_init(&ms, &mt_desc_wrap);
    MenuScreen_HandleToken(&ms, TOKEN_DOWN);
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);   /* SetTab resets cursor */
    reset_callbacks();
    MenuScreen_HandleToken(&ms, TOKEN_ENTER);
    EXPECT_EQ(g_select_idx, 0, "ENTER after tab switch: idx=0 (cursor reset)");
}

/* -------------------------------------------------------------------------- */
/* Group 10: MenuScreen_IsMenuOpeningKey                                       */
/* -------------------------------------------------------------------------- */

static void test_is_menu_opening_key(void)
{
    printf("Group 10: MenuScreen_IsMenuOpeningKey\n");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_MATH),   "MATH is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_TEST),   "TEST is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_VARS),   "VARS is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_MATRX),  "MATRX is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_PRGM),   "PRGM is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_Y_VARS), "Y_VARS is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_STAT),   "STAT is menu key");
    EXPECT_TRUE(MenuScreen_IsMenuOpeningKey(TOKEN_DRAW),   "DRAW is menu key");
    EXPECT_FALSE(MenuScreen_IsMenuOpeningKey(TOKEN_ENTER), "ENTER is not menu key");
    EXPECT_FALSE(MenuScreen_IsMenuOpeningKey(TOKEN_ALPHA), "ALPHA is not menu key");
    EXPECT_FALSE(MenuScreen_IsMenuOpeningKey(TOKEN_GRAPH), "GRAPH is not menu key");
}

/* -------------------------------------------------------------------------- */
/* Group 11: MenuScreen_DefaultExtra — closes menu and returns false           */
/* -------------------------------------------------------------------------- */

static void test_default_extra(void)
{
    printf("Group 11: MenuScreen_DefaultExtra\n");
    MenuScreenDesc_t desc = s5_desc;
    desc.on_extra = MenuScreen_DefaultExtra;
    MenuScreen_t ms;
    ms_init(&ms, &desc);

    /* Graph nav key: closes menu (cancel called) and falls through */
    reset_callbacks();
    bool ret = MenuScreen_HandleToken(&ms, TOKEN_GRAPH);
    EXPECT_FALSE(ret, "DefaultExtra TOKEN_GRAPH: returns false");
    EXPECT_TRUE(g_cancel_called, "DefaultExtra TOKEN_GRAPH: on_cancel called");

    reset_callbacks();
    ret = MenuScreen_HandleToken(&ms, TOKEN_Y_EQUALS);
    EXPECT_FALSE(ret, "DefaultExtra Y_EQUALS: returns false");
    EXPECT_TRUE(g_cancel_called, "DefaultExtra Y_EQUALS: on_cancel called");

    /* Menu-opening key: closes menu and falls through */
    reset_callbacks();
    ret = MenuScreen_HandleToken(&ms, TOKEN_MATH);
    EXPECT_FALSE(ret, "DefaultExtra TOKEN_MATH: returns false");
    EXPECT_TRUE(g_cancel_called, "DefaultExtra TOKEN_MATH: on_cancel called");

    /* Unknown token: falls through without closing */
    reset_callbacks();
    ret = MenuScreen_HandleToken(&ms, TOKEN_ALPHA);
    EXPECT_FALSE(ret, "DefaultExtra TOKEN_ALPHA: returns false");
    EXPECT_FALSE(g_cancel_called, "DefaultExtra TOKEN_ALPHA: on_cancel NOT called");
}

/* -------------------------------------------------------------------------- */
/* Group 12: ResetAndShow restores default_tab                                 */
/* -------------------------------------------------------------------------- */

static void test_reset_and_show(void)
{
    printf("Group 12: ResetAndShow restores default_tab\n");
    MenuScreen_t ms;
    ms_init(&ms, &mt_desc_wrap);

    /* Navigate away from default */
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    MenuScreen_HandleToken(&ms, TOKEN_RIGHT);
    EXPECT_EQ(ms.active_tab, 2, "After 2 RIGHTs: tab=2");

    MenuScreen_ResetAndShow(&ms);
    EXPECT_EQ(ms.active_tab, 0, "ResetAndShow: active_tab back to default=0");
    EXPECT_EQ(ms.nav.cursor, 0, "ResetAndShow: cursor=0");
    EXPECT_EQ(ms.nav.scroll, 0, "ResetAndShow: scroll=0");
}

/* -------------------------------------------------------------------------- */
/* main                                                                        */
/* -------------------------------------------------------------------------- */

int main(void)
{
    test_digit_shortcuts();
    test_up_down_scroll();
    test_tab_wrap();
    test_tab_nowrap();
    test_clear();
    test_unknown_token();
    test_on_extra();
    test_sibling_tab_switch();
    test_enter_select();
    test_is_menu_opening_key();
    test_default_extra();
    test_reset_and_show();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
