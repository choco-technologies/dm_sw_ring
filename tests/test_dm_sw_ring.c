#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dm_sw_ring.h"
#include <stdint.h>
#include <stdbool.h>

static int g_failures = 0;

#define EXPECT_TRUE(expr)                                                                            \
    do                                                                                               \
    {                                                                                                \
        if (!(expr))                                                                                 \
        {                                                                                            \
            DMOD_LOG_ERROR("Expectation failed: %s at %s:%d\n", #expr, __FILE__, __LINE__);        \
            g_failures++;                                                                            \
        }                                                                                            \
    } while (0)

#define EXPECT_EQ_U32(actual, expected) EXPECT_TRUE((uint32_t)(actual) == (uint32_t)(expected))
#define EXPECT_EQ_I32(actual, expected) EXPECT_TRUE((int32_t)(actual) == (int32_t)(expected))
#define EXPECT_EQ_BOOL(actual, expected) EXPECT_TRUE(((actual) ? true : false) == ((expected) ? true : false))
#define EXPECT_EQ_U8(actual, expected) EXPECT_TRUE((uint8_t)(actual) == (uint8_t)(expected))

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    dm_sw_ring_flags_t flags = dm_sw_ring_flags_drop_old_data | dm_sw_ring_flags_mutex_sync;
    dm_sw_ring_t ring = dm_sw_ring_create(4, flags);
    EXPECT_TRUE(ring != NULL);
    EXPECT_TRUE(dm_sw_ring_create(0, flags) == NULL);

    EXPECT_EQ_U32(dm_sw_ring_capacity(ring), 4);
    EXPECT_EQ_U32(dm_sw_ring_size(ring), 0);
    EXPECT_EQ_U32(dm_sw_ring_available_space(ring), 4);
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(ring), true);
    EXPECT_EQ_BOOL(dm_sw_ring_is_full(ring), false);

    {
        const uint8_t input1[] = {1, 2, 3};
        uint8_t buffer[4] = {0};
        EXPECT_EQ_U32(dm_sw_ring_write(ring, input1, 3), 3);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 3);
        EXPECT_EQ_U32(dm_sw_ring_available_space(ring), 1);

        EXPECT_EQ_I32(dm_sw_ring_peek(ring, buffer, 2), 2);
        EXPECT_EQ_U8(buffer[0], 1);
        EXPECT_EQ_U8(buffer[1], 2);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 3);

        buffer[0] = 0;
        buffer[1] = 0;
        EXPECT_EQ_U32(dm_sw_ring_read(ring, buffer, 2), 2);
        EXPECT_EQ_U8(buffer[0], 1);
        EXPECT_EQ_U8(buffer[1], 2);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 1);
    }

    EXPECT_EQ_I32(dm_sw_ring_discard(ring, 0), 0);
    EXPECT_EQ_I32(dm_sw_ring_discard(ring, 1), 1);
    EXPECT_EQ_U32(dm_sw_ring_size(ring), 0);
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(ring), true);

    {
        const uint8_t input2[] = {7, 8, 9, 10};
        EXPECT_EQ_U32(dm_sw_ring_write(ring, input2, 4), 4);
        EXPECT_EQ_BOOL(dm_sw_ring_is_full(ring), true);
        EXPECT_EQ_I32(dm_sw_ring_clear(ring), 0);
        EXPECT_EQ_BOOL(dm_sw_ring_is_empty(ring), true);
        EXPECT_EQ_U32(dm_sw_ring_available_space(ring), 4);
    }

    {
        const uint8_t input3[] = {11, 12, 13, 14, 15};
        uint8_t buffer[4] = {0};
        EXPECT_EQ_U32(dm_sw_ring_write(ring, input3, 5), 5);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 4);
        EXPECT_EQ_BOOL(dm_sw_ring_is_full(ring), true);

        EXPECT_EQ_I32(dm_sw_ring_peek(ring, buffer, 4), 4);
        EXPECT_EQ_U8(buffer[0], 12);
        EXPECT_EQ_U8(buffer[1], 13);
        EXPECT_EQ_U8(buffer[2], 14);
        EXPECT_EQ_U8(buffer[3], 15);

        buffer[0] = 0;
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = 0;
        EXPECT_EQ_U32(dm_sw_ring_read(ring, buffer, 4), 4);
        EXPECT_EQ_U8(buffer[0], 12);
        EXPECT_EQ_U8(buffer[1], 13);
        EXPECT_EQ_U8(buffer[2], 14);
        EXPECT_EQ_U8(buffer[3], 15);
        EXPECT_EQ_U32(dm_sw_ring_read(ring, buffer, 1), 0);
        EXPECT_EQ_I32(dm_sw_ring_peek(ring, buffer, 0), 0);
    }

    dm_sw_ring_destroy(ring);

    if (g_failures == 0)
    {
        DMOD_LOG_INFO("All dm_sw_ring API checks passed\n");
        return 0;
    }

    DMOD_LOG_ERROR("dm_sw_ring API checks failed: %d\n", g_failures);
    return 1;
}
