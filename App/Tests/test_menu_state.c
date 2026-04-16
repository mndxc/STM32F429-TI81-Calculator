/**
 * @file    test_menu_state.c
 * @brief   Host-side unit tests for MenuState_t navigation helpers (menu_state.c).
 *
 * Build:
 *   cmake -S App/Tests -B build-tests && cmake --build build-tests
 *   ./build-tests/test_menu_state   # exits 0 on full pass
 */

#include "../Inc/menu_state.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Minimal test framework                                                      */
/* -------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT_EQ(a, b, msg) do { \
    if ((a) == (b)) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s:%d] %s: got %d, expected %d\n", \
                            __FILE__, __LINE__, (msg), (int)(a), (int)(b)); } \
} while (0)

/* -------------------------------------------------------------------------- */
/* Group 1: MenuState_MoveUp boundaries                                        */
/* -------------------------------------------------------------------------- */

static void test_move_up(void)
{
    printf("Group 1: MenuState_MoveUp\n");

    MenuState_t s;

    /* At top (cursor=0, scroll=0): nothing changes */
    s.cursor = 0; s.scroll = 0; s.tab = 0; s.return_mode = MODE_NORMAL;
    MenuState_MoveUp(&s, 5, 3);
    EXPECT_EQ(s.cursor, 0, "MoveUp at top: cursor stays 0");
    EXPECT_EQ(s.scroll, 0, "MoveUp at top: scroll stays 0");

    /* cursor > 0: cursor decrements, scroll unchanged */
    s.cursor = 2; s.scroll = 0;
    MenuState_MoveUp(&s, 5, 3);
    EXPECT_EQ(s.cursor, 1, "MoveUp cursor>0: cursor--");
    EXPECT_EQ(s.scroll, 0, "MoveUp cursor>0: scroll unchanged");

    /* cursor = 0, scroll > 0: scroll decrements */
    s.cursor = 0; s.scroll = 3;
    MenuState_MoveUp(&s, 10, 3);
    EXPECT_EQ(s.cursor, 0, "MoveUp scroll>0: cursor stays 0");
    EXPECT_EQ(s.scroll, 2, "MoveUp scroll>0: scroll--");

    /* cursor = 0, scroll = 1 → scroll hits 0 */
    s.cursor = 0; s.scroll = 1;
    MenuState_MoveUp(&s, 10, 3);
    EXPECT_EQ(s.scroll, 0, "MoveUp scroll=1: scroll becomes 0");

    /* Single-item list at top: no movement */
    s.cursor = 0; s.scroll = 0;
    MenuState_MoveUp(&s, 1, 3);
    EXPECT_EQ(s.cursor, 0, "MoveUp single-item: cursor stays 0");
    EXPECT_EQ(s.scroll, 0, "MoveUp single-item: scroll stays 0");
}

/* -------------------------------------------------------------------------- */
/* Group 2: MenuState_MoveDown boundaries                                      */
/* -------------------------------------------------------------------------- */

static void test_move_down(void)
{
    printf("Group 2: MenuState_MoveDown\n");

    MenuState_t s;

    /* At last item: nothing changes */
    s.cursor = 2; s.scroll = 2; s.tab = 0; s.return_mode = MODE_NORMAL;
    MenuState_MoveDown(&s, 5, 3); /* absolute = 4 = last of 5 */
    EXPECT_EQ(s.cursor, 2, "MoveDown at last: cursor stays");
    EXPECT_EQ(s.scroll, 2, "MoveDown at last: scroll stays");

    /* cursor < visible-1: cursor increments */
    s.cursor = 0; s.scroll = 0;
    MenuState_MoveDown(&s, 5, 3);
    EXPECT_EQ(s.cursor, 1, "MoveDown cursor<visible-1: cursor++");
    EXPECT_EQ(s.scroll, 0, "MoveDown cursor<visible-1: scroll unchanged");

    /* cursor = visible-1 (window full): scroll increments */
    s.cursor = 2; s.scroll = 0;
    MenuState_MoveDown(&s, 5, 3); /* absolute=2, next=3 exists */
    EXPECT_EQ(s.cursor, 2, "MoveDown cursor=visible-1: cursor stays");
    EXPECT_EQ(s.scroll, 1, "MoveDown cursor=visible-1: scroll++");

    /* Single-item list: no movement */
    s.cursor = 0; s.scroll = 0;
    MenuState_MoveDown(&s, 1, 3);
    EXPECT_EQ(s.cursor, 0, "MoveDown single-item: cursor stays 0");
    EXPECT_EQ(s.scroll, 0, "MoveDown single-item: scroll stays 0");

    /* Exactly 2 items, visible=3: cursor goes 0→1, then stops */
    s.cursor = 0; s.scroll = 0;
    MenuState_MoveDown(&s, 2, 3);
    EXPECT_EQ(s.cursor, 1, "MoveDown 2-item: cursor becomes 1");
    MenuState_MoveDown(&s, 2, 3);
    EXPECT_EQ(s.cursor, 1, "MoveDown 2-item at last: cursor stays 1");
    EXPECT_EQ(s.scroll, 0, "MoveDown 2-item at last: scroll stays 0");
}

/* -------------------------------------------------------------------------- */
/* Group 3: MenuState_PrevTab / MenuState_NextTab                              */
/* -------------------------------------------------------------------------- */

static void test_tab_move(void)
{
    printf("Group 3: MenuState_PrevTab / MenuState_NextTab\n");

    MenuState_t s;
    s.tab = 2; s.cursor = 3; s.scroll = 1; s.return_mode = MODE_NORMAL;

    /* PrevTab: tab decrements, cursor/scroll reset */
    MenuState_PrevTab(&s, 5);
    EXPECT_EQ(s.tab,    1, "PrevTab: tab--");
    EXPECT_EQ(s.cursor, 0, "PrevTab: cursor reset");
    EXPECT_EQ(s.scroll, 0, "PrevTab: scroll reset");

    /* PrevTab at tab 0: stops at 0 */
    s.tab = 0; s.cursor = 2; s.scroll = 1;
    MenuState_PrevTab(&s, 5);
    EXPECT_EQ(s.tab,    0, "PrevTab at 0: tab stays 0");
    EXPECT_EQ(s.cursor, 0, "PrevTab at 0: cursor reset");

    /* NextTab: tab increments, cursor/scroll reset */
    s.tab = 1; s.cursor = 3; s.scroll = 2;
    MenuState_NextTab(&s, 5);
    EXPECT_EQ(s.tab,    2, "NextTab: tab++");
    EXPECT_EQ(s.cursor, 0, "NextTab: cursor reset");
    EXPECT_EQ(s.scroll, 0, "NextTab: scroll reset");

    /* NextTab at last tab: stops */
    s.tab = 4; s.cursor = 1; s.scroll = 0;
    MenuState_NextTab(&s, 5);
    EXPECT_EQ(s.tab,    4, "NextTab at last: tab stays");
    EXPECT_EQ(s.cursor, 0, "NextTab at last: cursor reset");
}

/* -------------------------------------------------------------------------- */
/* Group 4: MenuState_DigitToIndex                                             */
/* -------------------------------------------------------------------------- */

static void test_digit_to_index(void)
{
    printf("Group 4: MenuState_DigitToIndex\n");

    /* TOKEN_1..TOKEN_9 map to 0..8 */
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_1, 10), 0, "TOKEN_1 -> 0");
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_5, 10), 4, "TOKEN_5 -> 4");
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_9, 10), 8, "TOKEN_9 -> 8");

    /* TOKEN_0 maps to index 9 */
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_0, 10), 9, "TOKEN_0 -> 9");

    /* Out-of-range: index >= total */
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_5, 4), -1, "TOKEN_5 out of range");
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_0, 9), -1, "TOKEN_0 out of 9-item list");

    /* Non-digit tokens return -1 */
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_ENTER, 10), -1, "TOKEN_ENTER -> -1");
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_UP,    10), -1, "TOKEN_UP -> -1");

    /* Edge: exactly at boundary */
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_3, 3), 2,  "TOKEN_3 with total=3 -> 2");
    EXPECT_EQ(MenuState_DigitToIndex(TOKEN_4, 3), -1, "TOKEN_4 with total=3 -> -1");
}

/* -------------------------------------------------------------------------- */
/* Group 5: MenuState_AbsoluteIndex                                            */
/* -------------------------------------------------------------------------- */

static void test_absolute_index(void)
{
    printf("Group 5: MenuState_AbsoluteIndex\n");

    MenuState_t s;

    s.scroll = 0; s.cursor = 0;
    EXPECT_EQ(MenuState_AbsoluteIndex(&s), 0, "scroll=0 cursor=0 -> 0");

    s.scroll = 3; s.cursor = 2;
    EXPECT_EQ(MenuState_AbsoluteIndex(&s), 5, "scroll=3 cursor=2 -> 5");

    s.scroll = 7; s.cursor = 0;
    EXPECT_EQ(MenuState_AbsoluteIndex(&s), 7, "scroll=7 cursor=0 -> 7");
}

/* -------------------------------------------------------------------------- */
/* main                                                                        */
/* -------------------------------------------------------------------------- */

int main(void)
{
    test_move_up();
    test_move_down();
    test_tab_move();
    test_digit_to_index();
    test_absolute_index();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
