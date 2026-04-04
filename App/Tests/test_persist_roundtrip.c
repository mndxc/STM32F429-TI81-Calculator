/**
 * @file    test_persist_roundtrip.c
 * @brief   Host-side tests for PersistBlock_t serialization and validation.
 *
 * Verifies that:
 *  - Persist_Checksum produces a stable, deterministic result.
 *  - A well-formed block (correct magic, version, checksum) passes
 *    Persist_Validate.
 *  - Corrupting any single field causes Persist_Validate to return false.
 *  - Field values survive the round-trip (set → read back).
 *
 * Build:
 *   cmake -S App/Tests -B build/tests && cmake --build build/tests
 *   ./build/tests/test_persist_roundtrip   # exits 0 on full pass
 *
 * HOST_TEST must be defined by the CMake target to exclude HAL dependencies.
 */

#define HOST_TEST  /* pulled in by CMake -DHOST_TEST=1; repeated here for IDEs */

#include "../Inc/persist.h"
#include "../Inc/calc_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Minimal test framework                                                       */
/* -------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT_EQ(a, b, msg) do { \
    if ((a) == (b)) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s:%d] %s: got %d, expected %d\n", \
                            __FILE__, __LINE__, (msg), (int)(a), (int)(b)); } \
} while (0)

#define EXPECT_TRUE(expr, msg)  EXPECT_EQ(!!(expr), 1, msg)
#define EXPECT_FALSE(expr, msg) EXPECT_EQ(!!(expr), 0, msg)

/* -------------------------------------------------------------------------- */
/* Helper: fill a PersistBlock_t with known non-zero values.                   */
/* -------------------------------------------------------------------------- */

static void fill_block(PersistBlock_t *b)
{
    memset(b, 0, sizeof(*b));
    b->magic   = PERSIST_MAGIC;
    b->version = PERSIST_VERSION;

    /* Variables A–Z: A=1.0, B=2.0, … Z=26.0 */
    for (int i = 0; i < 26; i++)
        b->calc_variables[i] = (float)(i + 1);

    b->ans = 42.5f;

    /* MODE selections: Float(0), Degree(1), etc. */
    b->mode_committed[0] = 0;  /* Normal */
    b->mode_committed[1] = 0;  /* Float */
    b->mode_committed[2] = 1;  /* Degree */
    b->mode_committed[6] = 1;  /* Grid on */

    b->zoom_x_fact = 4.0f;
    b->zoom_y_fact = 4.0f;

    strncpy(b->equations[0], "sin(X)", 63);
    strncpy(b->equations[1], "cos(X)", 63);

    b->x_min = -10.0f;
    b->x_max =  10.0f;
    b->y_min =  -6.5f;
    b->y_max =   6.5f;
    b->x_scl =   1.0f;
    b->y_scl =   1.0f;
    b->x_res =   1.0f;
    b->grid_on = 1;

    /* Matrices [A] and [B]: 2×2 identity */
    b->matrix_rows[0] = 2;  b->matrix_cols[0] = 2;
    b->matrix_data[0][0] = 1.0f; b->matrix_data[0][1] = 0.0f;
    b->matrix_data[0][2] = 0.0f; b->matrix_data[0][3] = 1.0f;

    b->matrix_rows[1] = 3;  b->matrix_cols[1] = 3;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            b->matrix_data[1][r * CALC_MATRIX_MAX_DIM + c] = (float)(r * 3 + c + 1);

    b->matrix_rows[2] = 1;  b->matrix_cols[2] = 1;
    b->matrix_data[2][0] = 7.0f;

    /* Stamp checksum last */
    b->checksum = Persist_Checksum(b);
}

/* -------------------------------------------------------------------------- */
/* Group 1: Checksum stability                                                  */
/* -------------------------------------------------------------------------- */

static void test_checksum_stability(void)
{
    printf("Group 1: Checksum stability\n");

    PersistBlock_t b;
    fill_block(&b);
    uint32_t c1 = Persist_Checksum(&b);
    uint32_t c2 = Persist_Checksum(&b);
    EXPECT_EQ(c1, c2, "checksum is deterministic");

    /* Changing one field changes the checksum */
    float old = b.ans;
    b.ans = old + 1.0f;
    uint32_t c3 = Persist_Checksum(&b);
    EXPECT_FALSE(c3 == c1, "checksum changes when ans changes");
    b.ans = old;  /* restore */
}

/* -------------------------------------------------------------------------- */
/* Group 2: Persist_Validate — valid block                                     */
/* -------------------------------------------------------------------------- */

static void test_validate_valid(void)
{
    printf("Group 2: Persist_Validate (valid block)\n");

    PersistBlock_t b;
    fill_block(&b);
    EXPECT_TRUE(Persist_Validate(&b), "valid block passes");

    /* Re-validate after reading back via a memcpy (simulates load) */
    PersistBlock_t copy;
    memcpy(&copy, &b, sizeof(b));
    EXPECT_TRUE(Persist_Validate(&copy), "memcpy'd block passes");
}

/* -------------------------------------------------------------------------- */
/* Group 3: Persist_Validate — invalid blocks                                  */
/* -------------------------------------------------------------------------- */

static void test_validate_invalid(void)
{
    printf("Group 3: Persist_Validate (invalid blocks)\n");

    PersistBlock_t b;

    /* Wrong magic */
    fill_block(&b);
    b.magic = 0xDEADBEEFU;
    EXPECT_FALSE(Persist_Validate(&b), "wrong magic fails");

    /* Wrong version */
    fill_block(&b);
    b.version = PERSIST_VERSION + 1;
    EXPECT_FALSE(Persist_Validate(&b), "wrong version fails");

    /* Corrupt one variable byte (flip low bit of calc_variables[0]) */
    fill_block(&b);
    ((uint8_t *)&b.calc_variables[0])[0] ^= 0x01;
    EXPECT_FALSE(Persist_Validate(&b), "corrupted variable fails checksum");

    /* Corrupt equation string */
    fill_block(&b);
    b.equations[0][0] ^= 0x01;
    EXPECT_FALSE(Persist_Validate(&b), "corrupted equation fails checksum");

    /* All-zeros block (blank FLASH simulation) */
    memset(&b, 0, sizeof(b));
    EXPECT_FALSE(Persist_Validate(&b), "blank block fails");

    /* All-0xFF block (erased FLASH) */
    memset(&b, 0xFF, sizeof(b));
    EXPECT_FALSE(Persist_Validate(&b), "erased FLASH block fails");
}

/* -------------------------------------------------------------------------- */
/* Group 4: Field round-trip — read back what was written                      */
/* -------------------------------------------------------------------------- */

static void test_field_roundtrip(void)
{
    printf("Group 4: Field round-trip\n");

    PersistBlock_t b;
    fill_block(&b);
    EXPECT_TRUE(Persist_Validate(&b), "block validates before field checks");

    /* Variables */
    for (int i = 0; i < 26; i++) {
        /* Compare as bit patterns to avoid float == warning */
        uint32_t expected, got;
        float exp_f = (float)(i + 1);
        memcpy(&expected, &exp_f, 4);
        memcpy(&got, &b.calc_variables[i], 4);
        EXPECT_EQ(got, expected, "variable round-trip");
    }

    /* ANS */
    {
        float expected = 42.5f;
        uint32_t e, g;
        memcpy(&e, &expected, 4);
        memcpy(&g, &b.ans, 4);
        EXPECT_EQ(g, e, "ans round-trip");
    }

    /* MODE */
    EXPECT_EQ(b.mode_committed[2], 1, "mode_committed[2] (Degree)");
    EXPECT_EQ(b.mode_committed[6], 1, "mode_committed[6] (Grid on)");

    /* Graph state */
    EXPECT_EQ(b.grid_on, 1, "grid_on");
    EXPECT_EQ(strcmp(b.equations[0], "sin(X)"), 0, "equation[0]");
    EXPECT_EQ(strcmp(b.equations[1], "cos(X)"), 0, "equation[1]");

    /* Matrix dimensions */
    EXPECT_EQ(b.matrix_rows[0], 2, "matrix[0] rows");
    EXPECT_EQ(b.matrix_cols[0], 2, "matrix[0] cols");
    EXPECT_EQ(b.matrix_rows[1], 3, "matrix[1] rows");
    EXPECT_EQ(b.matrix_rows[2], 1, "matrix[2] rows");

    /* Matrix [A] identity values */
    {
        float v;
        memcpy(&v, &b.matrix_data[0][0], 4);
        EXPECT_EQ((int)v, 1, "matrix[0][0][0] = 1 (identity)");
        memcpy(&v, &b.matrix_data[0][3], 4);
        EXPECT_EQ((int)v, 1, "matrix[0][1][1] = 1 (identity)");
        memcpy(&v, &b.matrix_data[0][1], 4);
        EXPECT_EQ((int)v, 0, "matrix[0][0][1] = 0 (identity)");
    }
}

/* -------------------------------------------------------------------------- */
/* Group 5: Size and alignment assertions                                       */
/* -------------------------------------------------------------------------- */

static void test_block_properties(void)
{
    printf("Group 5: Block size and alignment\n");

    /* Block must be a multiple of 4 bytes (for word-aligned FLASH writes) */
    EXPECT_EQ(sizeof(PersistBlock_t) % 4, 0, "PersistBlock_t size multiple of 4");

    /* Expected total size: 2060 bytes.
     * Grew from 864 → 1264 in PERSIST_VERSION 5 when parametric fields were
     * added: param_x[3][64]=192, param_y[3][64]=192, param_enabled[3]=3,
     * param_mode=1, t_min+t_max+t_step=12, plus alignment padding.
     * Grew from 1264 → 2060 in PERSIST_VERSION 6 when STAT fields were
     * added: stat_list_x[99]=396, stat_list_y[99]=396, stat_list_len=1,
     * _stat_pad[3]=3. */
    EXPECT_EQ((int)sizeof(PersistBlock_t), 2060, "PersistBlock_t is 2060 bytes");
}

/* -------------------------------------------------------------------------- */
/* main                                                                         */
/* -------------------------------------------------------------------------- */

int main(void)
{
    test_checksum_stability();
    test_validate_valid();
    test_validate_invalid();
    test_field_roundtrip();
    test_block_properties();

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
