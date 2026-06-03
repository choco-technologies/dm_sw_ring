#include "dmod.h"
#include "dm_sw_ring.h"

// ============================================================================
//                      Local Types and Definitions
// ============================================================================
#define DM_SW_RING_MAGIC        0x52494E47 // 'RING' in ASCII

/**
 * @brief Internal structure representing a software ring buffer instance
 */
struct dm_sw_ring
{
    uint32_t magic;                     // Magic number for validation
    dm_sw_ring_capacity_t capacity;     // Capacity of the ring buffer (number of elements)
    dm_sw_ring_capacity_t head;         // Index of the head (next element to read)
    dm_sw_ring_capacity_t tail;         // Index of the tail (next element to write)
    uint8_t* buffer;                    // Pointer to the buffer memory
    bool drop_old_data;                 // Whether to drop old data when the buffer is full
};

// ============================================================================
//                      Local prototypes
// ============================================================================

static bool validate_ring(dm_sw_ring_t ring);

// ============================================================================
//                      Module Interface Implementation
// ============================================================================

/**
 * @brief Module initialization (optional)
 */
void dmod_preinit(void)
{
    // Nothing to do
}

/**
 * @brief Module initialization
 */
int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    return 0;
}

/**
 * @brief Module deinitialization
 */
void dmod_deinit(void)
{
    // Nothing to do
}

// ============================================================================
//                      Interface Implementation
// ============================================================================

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_t, _create, (dm_sw_ring_capacity_t capacity, bool drop_old_data))
{
    dm_sw_ring_t ring = NULL;

    if (capacity == 0)
    {
        DMOD_LOG_ERROR("Ring buffer capacity must be greater than zero\n");
        return NULL;
    }

    ring = Dmod_Malloc(sizeof(struct dm_sw_ring));
    if (ring == NULL)
    {
        DMOD_LOG_ERROR("Failed to allocate memory for ring buffer instance\n");
        return NULL;
    }

    ring->buffer = Dmod_Malloc(capacity);
    if (ring->buffer == NULL)
    {
        DMOD_LOG_ERROR("Failed to allocate memory for ring buffer data\n");
        Dmod_Free(ring);
        return NULL;
    }

    ring->magic = DM_SW_RING_MAGIC;
    ring->capacity = capacity;
    ring->head = 0;
    ring->tail = 0;
    ring->drop_old_data = drop_old_data;

    return ring;
}

dmod_dm_sw_ring_api_declaration(1.0, void, _destroy, (dm_sw_ring_t ring))
{
    Dmod_EnterCritical();

    if (validate_ring(ring))
    {
        ring->magic = 0;

        Dmod_Free(ring->buffer);
        Dmod_Free(ring);
    }
    else
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
    }

    Dmod_ExitCritical();
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _write, (dm_sw_ring_t ring, const void* data, dm_sw_ring_capacity_t length))
{
    int32_t written = -1;
    const uint8_t* input = (const uint8_t*)data;

    if ((input == NULL) && (length > 0))
    {
        DMOD_LOG_ERROR("Invalid data buffer\n");
        return -1;
    }

    Dmod_EnterCritical();

    if (validate_ring(ring))
    {
        
    }
    else
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
    }

    Dmod_ExitCritical();
    return written;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _read, (dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length))
{
    int32_t read = -1;

    if ((buffer == NULL) && (length > 0))
    {
        DMOD_LOG_ERROR("Invalid output buffer\n");
        return -1;
    }

    Dmod_EnterCritical();

    if (validate_ring(ring))
    {
        
    }
    else
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
    }

    Dmod_ExitCritical();
    return read;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _capacity, (dm_sw_ring_t ring))
{
    if (!validate_ring(ring))
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
        return 0;
    }

    return ring->capacity;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _size, (dm_sw_ring_t ring))
{
    if (!validate_ring(ring))
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
        return 0;
    }

    return 0;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _available_space, (dm_sw_ring_t ring))
{
    if (!validate_ring(ring))
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
        return 0;
    }

    return 0;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _peek, (dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length))
{
    int32_t peeked = -1;
    dm_sw_ring_capacity_t index = 0;

    if ((buffer == NULL) && (length > 0))
    {
        DMOD_LOG_ERROR("Invalid output buffer\n");
        return -1;
    }

    Dmod_EnterCritical();

    if (validate_ring(ring))
    {
        
    }
    else
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
    }

    Dmod_ExitCritical();
    return peeked;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _discard, (dm_sw_ring_t ring, dm_sw_ring_capacity_t length))
{
    int32_t discarded = -1;

    Dmod_EnterCritical();

    if (validate_ring(ring))
    {
        discarded = (int32_t)discard_unsafe(ring, length);
    }
    else
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
    }

    Dmod_ExitCritical();
    return discarded;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _clear, (dm_sw_ring_t ring))
{
    if (!validate_ring(ring))
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
        return -1;
    }

    ring->head = 0;
    ring->tail = 0;

    return 0;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _is_full, (dm_sw_ring_t ring))
{
    if (!validate_ring(ring))
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
        return -1;
    }

    return 0;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _is_empty, (dm_sw_ring_t ring))
{
    if (!validate_ring(ring))
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
        return -1;
    }

    return 0;
}

// ============================================================================
//                      Local prototypes implementation
// ============================================================================

/**
 * @brief Validate a ring buffer instance
 * @param ring The handle to the ring buffer instance to validate
 * @return true if the ring buffer instance is valid, false otherwise
 */
static bool validate_ring(dm_sw_ring_t ring)
{
    return (ring != NULL) && (ring->magic == DM_SW_RING_MAGIC);
}