#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dm_sw_ring.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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

typedef struct test_context
{
    dm_sw_ring_t ring;
    dm_sw_ring_flags_t ring_flags;
    uint8_t io_buffer[8];
} test_context_t;

typedef void (*test_setup_fn)(test_context_t* context);
typedef void (*test_step_fn)(test_context_t* context);
typedef void (*test_teardown_fn)(test_context_t* context);

static void setup_ring_capacity_4(test_context_t* context)
{
    context->ring = dm_sw_ring_create(4, context->ring_flags);
    EXPECT_TRUE(context->ring != NULL);
}

static void teardown_ring(test_context_t* context)
{
    if (context->ring != NULL)
    {
        dm_sw_ring_destroy(context->ring);
        context->ring = NULL;
    }
}

static void run_test_step(const char* name, test_context_t* context, test_setup_fn setup, test_step_fn step, test_teardown_fn teardown)
{
    int failures_before = g_failures;
    DMOD_LOG_STEP_BEGIN("%s\n", name);

    if (setup != NULL)
    {
        setup(context);
    }
    if (step != NULL && context->ring != NULL)
    {
        step(context);
    }
    if (teardown != NULL)
    {
        teardown(context);
    }

    DMOD_LOG_STEP((g_failures > failures_before) ? 1 : 0, "%s\n", name);
}

static void test_create_step(test_context_t* context)
{
    dm_sw_ring_t ring = dm_sw_ring_create(4, context->ring_flags);
    EXPECT_TRUE(ring != NULL);
    EXPECT_TRUE(dm_sw_ring_create(0, context->ring_flags) == NULL);
    if (ring != NULL)
    {
        dm_sw_ring_destroy(ring);
    }
}

static void test_destroy_step(test_context_t* context)
{
    EXPECT_TRUE(context->ring != NULL);
    dm_sw_ring_destroy(context->ring);
    context->ring = NULL;
}

static void test_write_step(test_context_t* context)
{
    const uint8_t data[] = {10, 11, 12, 13, 14};
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, NULL, 1), 0);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 0);
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 5), 5);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 4);
}

static void test_read_step(test_context_t* context)
{
    const uint8_t data[] = {1, 2, 3};
    memset(context->io_buffer, 0, sizeof(context->io_buffer));
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 3), 3);
    EXPECT_EQ_U32(dm_sw_ring_read(context->ring, NULL, 1), 0);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 3);
    EXPECT_EQ_U32(dm_sw_ring_read(context->ring, context->io_buffer, 2), 2);
    EXPECT_EQ_U8(context->io_buffer[0], 1);
    EXPECT_EQ_U8(context->io_buffer[1], 2);
}

static void test_capacity_step(test_context_t* context)
{
    EXPECT_EQ_U32(dm_sw_ring_capacity(context->ring), 4);
}

static void test_size_step(test_context_t* context)
{
    const uint8_t data[] = {1, 2, 3};
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 0);
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 3), 3);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 3);
    EXPECT_EQ_U32(dm_sw_ring_discard(context->ring, 1), 1);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 2);
}

static void test_available_space_step(test_context_t* context)
{
    const uint8_t data[] = {1, 2, 3};
    EXPECT_EQ_U32(dm_sw_ring_available_space(context->ring), 4);
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 3), 3);
    EXPECT_EQ_U32(dm_sw_ring_available_space(context->ring), 1);
}

static void test_peek_step(test_context_t* context)
{
    const uint8_t data[] = {1, 2, 3, 4};
    memset(context->io_buffer, 0, sizeof(context->io_buffer));
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 4), 4);
    EXPECT_EQ_I32(dm_sw_ring_peek(context->ring, NULL, 1), -1);
    EXPECT_EQ_I32(dm_sw_ring_peek(context->ring, context->io_buffer, 4), 4);
    EXPECT_EQ_U8(context->io_buffer[0], 1);
    EXPECT_EQ_U8(context->io_buffer[3], 4);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 4);
}

static void test_discard_step(test_context_t* context)
{
    const uint8_t data[] = {4, 5, 6};
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 3), 3);
    EXPECT_EQ_I32(dm_sw_ring_discard(context->ring, 0), 0);
    EXPECT_EQ_I32(dm_sw_ring_discard(context->ring, 1), 1);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 2);
}

static void test_clear_step(test_context_t* context)
{
    const uint8_t data[] = {1, 2, 3, 4};
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 4), 4);
    EXPECT_EQ_I32(dm_sw_ring_clear(context->ring), 0);
    EXPECT_EQ_U32(dm_sw_ring_size(context->ring), 0);
}

static void test_is_full_step(test_context_t* context)
{
    const uint8_t data[] = {1, 2, 3, 4};
    EXPECT_EQ_BOOL(dm_sw_ring_is_full(context->ring), false);
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 4), 4);
    EXPECT_EQ_BOOL(dm_sw_ring_is_full(context->ring), true);
}

static void test_is_empty_step(test_context_t* context)
{
    const uint8_t data[] = {1};
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(context->ring), true);
    EXPECT_EQ_U32(dm_sw_ring_write(context->ring, data, 1), 1);
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(context->ring), false);
    EXPECT_EQ_U32(dm_sw_ring_read(context->ring, context->io_buffer, 1), 1);
    EXPECT_EQ_BOOL(dm_sw_ring_is_empty(context->ring), true);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    test_context_t context = {
        .ring = NULL,
        .ring_flags = dm_sw_ring_flags_drop_old_data | dm_sw_ring_flags_mutex_sync,
        .io_buffer = {0}
    };

    run_test_step("dm_sw_ring_create", &context, NULL, test_create_step, NULL);
    run_test_step("dm_sw_ring_destroy", &context, setup_ring_capacity_4, test_destroy_step, teardown_ring);
    run_test_step("dm_sw_ring_write", &context, setup_ring_capacity_4, test_write_step, teardown_ring);
    run_test_step("dm_sw_ring_read", &context, setup_ring_capacity_4, test_read_step, teardown_ring);
    run_test_step("dm_sw_ring_capacity", &context, setup_ring_capacity_4, test_capacity_step, teardown_ring);
    run_test_step("dm_sw_ring_size", &context, setup_ring_capacity_4, test_size_step, teardown_ring);
    run_test_step("dm_sw_ring_available_space", &context, setup_ring_capacity_4, test_available_space_step, teardown_ring);
    run_test_step("dm_sw_ring_peek", &context, setup_ring_capacity_4, test_peek_step, teardown_ring);
    run_test_step("dm_sw_ring_discard", &context, setup_ring_capacity_4, test_discard_step, teardown_ring);
    run_test_step("dm_sw_ring_clear", &context, setup_ring_capacity_4, test_clear_step, teardown_ring);
    run_test_step("dm_sw_ring_is_full", &context, setup_ring_capacity_4, test_is_full_step, teardown_ring);
    run_test_step("dm_sw_ring_is_empty", &context, setup_ring_capacity_4, test_is_empty_step, teardown_ring);

    if (g_failures == 0)
    {
        DMOD_LOG_INFO("All dm_sw_ring API checks passed\n");
        return 0;
    }

    DMOD_LOG_ERROR("dm_sw_ring API checks failed: %d\n", g_failures);
    return 1;
}
