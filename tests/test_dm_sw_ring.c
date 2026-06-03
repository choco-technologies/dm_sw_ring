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

#define EXPECT_EQ_U32(actual, expected)                                                              \
    do                                                                                               \
    {                                                                                                \
        uint32_t _actual = (uint32_t)(actual);                                                       \
        uint32_t _expected = (uint32_t)(expected);                                                   \
        if (_actual != _expected)                                                                    \
        {                                                                                            \
            DMOD_LOG_ERROR("Expectation failed: %s == %s (actual=%u expected=%u) at %s:%d\n",       \
                           #actual, #expected, _actual, _expected, __FILE__, __LINE__);             \
            g_failures++;                                                                            \
        }                                                                                            \
    } while (0)
#define EXPECT_EQ_I32(actual, expected)                                                              \
    do                                                                                               \
    {                                                                                                \
        int32_t _actual = (int32_t)(actual);                                                         \
        int32_t _expected = (int32_t)(expected);                                                     \
        if (_actual != _expected)                                                                    \
        {                                                                                            \
            DMOD_LOG_ERROR("Expectation failed: %s == %s (actual=%d expected=%d) at %s:%d\n",       \
                           #actual, #expected, _actual, _expected, __FILE__, __LINE__);             \
            g_failures++;                                                                            \
        }                                                                                            \
    } while (0)
#define EXPECT_EQ_BOOL(actual, expected)                                                             \
    do                                                                                               \
    {                                                                                                \
        bool _actual = ((actual) ? true : false);                                                    \
        bool _expected = ((expected) ? true : false);                                                \
        if (_actual != _expected)                                                                    \
        {                                                                                            \
            DMOD_LOG_ERROR("Expectation failed: %s == %s (actual=%d expected=%d) at %s:%d\n",       \
                           #actual, #expected, (int)_actual, (int)_expected, __FILE__, __LINE__);   \
            g_failures++;                                                                            \
        }                                                                                            \
    } while (0)
#define EXPECT_EQ_U8(actual, expected)                                                               \
    do                                                                                               \
    {                                                                                                \
        uint8_t _actual = (uint8_t)(actual);                                                         \
        uint8_t _expected = (uint8_t)(expected);                                                     \
        if (_actual != _expected)                                                                    \
        {                                                                                            \
            DMOD_LOG_ERROR("Expectation failed: %s == %s (actual=%u expected=%u) at %s:%d\n",       \
                           #actual, #expected, _actual, _expected, __FILE__, __LINE__);             \
            g_failures++;                                                                            \
        }                                                                                            \
    } while (0)

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Disable waiting flags to keep tests deterministic and non-blocking.
    dm_sw_ring_flags_t ring_flags = dm_sw_ring_flags_drop_old_data | dm_sw_ring_flags_mutex_sync;
    dm_sw_ring_t ring = dm_sw_ring_create(4, ring_flags);
    EXPECT_TRUE(ring != NULL);
    EXPECT_TRUE(dm_sw_ring_create(0, ring_flags) == NULL);

    EXPECT_EQ_U32(dm_sw_ring_capacity(ring), 4);
    EXPECT_EQ_U32(dm_sw_ring_size(ring), 0);
    EXPECT_EQ_U32(dm_sw_ring_available_space(ring), 4);
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(ring), true);
    EXPECT_EQ_BOOL(dm_sw_ring_is_full(ring), false);

    {
        const uint8_t test_data_basic[] = {1, 2, 3};
        uint8_t io_buffer[4] = {0};
        EXPECT_EQ_U32(dm_sw_ring_write(ring, test_data_basic, 3), 3);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 3);
        EXPECT_EQ_U32(dm_sw_ring_available_space(ring), 1);

        EXPECT_EQ_I32(dm_sw_ring_peek(ring, io_buffer, 2), 2);
        EXPECT_EQ_U8(io_buffer[0], 1);
        EXPECT_EQ_U8(io_buffer[1], 2);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 3);

        io_buffer[0] = 0;
        io_buffer[1] = 0;
        EXPECT_EQ_U32(dm_sw_ring_read(ring, io_buffer, 2), 2);
        EXPECT_EQ_U8(io_buffer[0], 1);
        EXPECT_EQ_U8(io_buffer[1], 2);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 1);
    }

    EXPECT_EQ_I32(dm_sw_ring_discard(ring, 0), 0);
    EXPECT_EQ_I32(dm_sw_ring_discard(ring, 1), 1);
    EXPECT_EQ_U32(dm_sw_ring_size(ring), 0);
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(ring), true);

    {
        const uint8_t test_data_full_capacity[] = {7, 8, 9, 10};
        EXPECT_EQ_U32(dm_sw_ring_write(ring, test_data_full_capacity, 4), 4);
        EXPECT_EQ_BOOL(dm_sw_ring_is_full(ring), true);
        EXPECT_EQ_I32(dm_sw_ring_clear(ring), 0);
        EXPECT_EQ_BOOL(dm_sw_ring_is_empty(ring), true);
        EXPECT_EQ_U32(dm_sw_ring_available_space(ring), 4);
    }

    {
        const uint8_t test_data_overflow[] = {11, 12, 13, 14, 15};
        uint8_t io_buffer[4] = {0};
        EXPECT_EQ_U32(dm_sw_ring_write(ring, test_data_overflow, 5), 5);
        EXPECT_EQ_U32(dm_sw_ring_size(ring), 4);
        EXPECT_EQ_BOOL(dm_sw_ring_is_full(ring), true);

        EXPECT_EQ_I32(dm_sw_ring_peek(ring, io_buffer, 4), 4);
        EXPECT_EQ_U8(io_buffer[0], 12);
        EXPECT_EQ_U8(io_buffer[1], 13);
        EXPECT_EQ_U8(io_buffer[2], 14);
        EXPECT_EQ_U8(io_buffer[3], 15);

        io_buffer[0] = 0;
        io_buffer[1] = 0;
        io_buffer[2] = 0;
        io_buffer[3] = 0;
        EXPECT_EQ_U32(dm_sw_ring_read(ring, io_buffer, 4), 4);
        EXPECT_EQ_U8(io_buffer[0], 12);
        EXPECT_EQ_U8(io_buffer[1], 13);
        EXPECT_EQ_U8(io_buffer[2], 14);
        EXPECT_EQ_U8(io_buffer[3], 15);
        EXPECT_EQ_U32(dm_sw_ring_read(ring, io_buffer, 1), 0);
        EXPECT_EQ_I32(dm_sw_ring_peek(ring, io_buffer, 0), 0);
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
