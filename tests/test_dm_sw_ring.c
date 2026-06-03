#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dm_sw_ring.h"
#include <stdint.h>
#include <string.h>

// ============================================================================
//                      Shared test state
// ============================================================================

static const dm_sw_ring_flags_t g_flags =
    dm_sw_ring_flags_drop_old_data | dm_sw_ring_flags_mutex_sync;

static dm_sw_ring_t g_ring   = NULL;
static uint8_t      g_io_buf[8];

// ============================================================================
//                      Lifecycle hooks
// ============================================================================

void dmod_test_setup(void)
{
    g_ring = dm_sw_ring_create(4, g_flags);
    memset(g_io_buf, 0, sizeof(g_io_buf));
}

void dmod_test_teardown(void)
{
    if (g_ring != NULL)
    {
        dm_sw_ring_destroy(g_ring);
        g_ring = NULL;
    }
}

// ============================================================================
//                      Test steps
// ============================================================================

DMOD_TEST_STEP(dm_sw_ring_create)
{
    /* The ring created by setup is not needed for this test */
    dm_sw_ring_destroy(g_ring);
    g_ring = NULL;

    dm_sw_ring_t ring = dm_sw_ring_create(4, g_flags);
    DMOD_TEST_EXPECT_NOT_NULL(ring);
    DMOD_TEST_EXPECT_NULL(dm_sw_ring_create(0, g_flags));

    if (ring != NULL)
    {
        dm_sw_ring_destroy(ring);
    }
}

DMOD_TEST_STEP(dm_sw_ring_destroy)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_ring);
    dm_sw_ring_destroy(g_ring);
    g_ring = NULL; /* prevent double-free in teardown */
}

DMOD_TEST_STEP(dm_sw_ring_write)
{
    const uint8_t data[] = {10, 11, 12, 13, 14};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, NULL, 1), 0u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 0u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 5), 5u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 4u);
}

DMOD_TEST_STEP(dm_sw_ring_read)
{
    const uint8_t data[] = {1, 2, 3};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 3), 3u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_read(g_ring, NULL, 1), 0u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 3u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_read(g_ring, g_io_buf, 2), 2u);
    DMOD_TEST_EXPECT_EQ(g_io_buf[0], 1u);
    DMOD_TEST_EXPECT_EQ(g_io_buf[1], 2u);
}

DMOD_TEST_STEP(dm_sw_ring_capacity)
{
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_capacity(g_ring), 4u);
}

DMOD_TEST_STEP(dm_sw_ring_size)
{
    const uint8_t data[] = {1, 2, 3};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 0u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 3), 3u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 3u);
    DMOD_TEST_EXPECT_EQ((uint32_t)dm_sw_ring_discard(g_ring, 1), 1u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 2u);
}

DMOD_TEST_STEP(dm_sw_ring_available_space)
{
    const uint8_t data[] = {1, 2, 3};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_available_space(g_ring), 4u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 3), 3u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_available_space(g_ring), 1u);
}

DMOD_TEST_STEP(dm_sw_ring_peek)
{
    const uint8_t data[] = {1, 2, 3, 4};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 4), 4u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_peek(g_ring, NULL, 1), -1);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_peek(g_ring, g_io_buf, 4), 4);
    DMOD_TEST_EXPECT_EQ(g_io_buf[0], 1u);
    DMOD_TEST_EXPECT_EQ(g_io_buf[3], 4u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 4u);
}

DMOD_TEST_STEP(dm_sw_ring_discard)
{
    const uint8_t data[] = {4, 5, 6};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 3), 3u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_discard(g_ring, 0), 0);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_discard(g_ring, 1), 1);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 2u);
}

DMOD_TEST_STEP(dm_sw_ring_clear)
{
    const uint8_t data[] = {1, 2, 3, 4};

    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 4), 4u);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_clear(g_ring), 0);
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_size(g_ring), 0u);
}

DMOD_TEST_STEP(dm_sw_ring_is_full)
{
    const uint8_t data[] = {1, 2, 3, 4};

    DMOD_TEST_EXPECT_FALSE(dm_sw_ring_is_full(g_ring));
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 4), 4u);
    DMOD_TEST_EXPECT_TRUE(dm_sw_ring_is_full(g_ring));
}

DMOD_TEST_STEP(dm_sw_ring_is_empty)
{
    const uint8_t data[] = {1};

    DMOD_TEST_EXPECT_TRUE(dm_sw_ring_is_empty(g_ring));
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_write(g_ring, data, 1), 1u);
    DMOD_TEST_EXPECT_FALSE(dm_sw_ring_is_empty(g_ring));
    DMOD_TEST_EXPECT_EQ(dm_sw_ring_read(g_ring, g_io_buf, 1), 1u);
    DMOD_TEST_EXPECT_TRUE(dm_sw_ring_is_empty(g_ring));
}

