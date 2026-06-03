#include "dmod.h"
#include "dm_sw_ring.h"

// ============================================================================
//                      Local Types and Definitions
// ============================================================================
#define DM_SW_RING_MAGIC        0x52494E47 // 'RING' in ASCII
#define DM_SW_RING_MAX_SIZE     INT32_MAX

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
    dm_sw_ring_flags_t flags;           // Flags for ring buffer behavior
    void* mutex;                        // Mutex for synchronization (if enabled)
};

// ============================================================================
//                      Local prototypes
// ============================================================================

static bool lock_ring(dm_sw_ring_t ring);
static void unlock_ring(dm_sw_ring_t ring);
static bool validate_ring(dm_sw_ring_t ring);
static dm_sw_ring_capacity_t available_space(dm_sw_ring_t ring);
static bool is_full(dm_sw_ring_t ring);
static bool is_empty(dm_sw_ring_t ring);
static void put_byte(dm_sw_ring_t ring, uint8_t data);
static uint8_t get_byte(dm_sw_ring_t ring);
static void discard(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t prepare_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t write_data(dm_sw_ring_t ring, const uint8_t* data, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t read_data(dm_sw_ring_t ring, uint8_t* buffer, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t peek_data(dm_sw_ring_t ring, uint8_t* buffer, dm_sw_ring_capacity_t length);

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

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_t, _create, (dm_sw_ring_capacity_t capacity, dm_sw_ring_flags_t flags))
{
    dm_sw_ring_t ring = NULL;

    if (capacity == 0 || capacity > DM_SW_RING_MAX_SIZE)
    {
        DMOD_LOG_ERROR("Ring buffer capacity must be greater than zero and less than or equal to %d\n", DM_SW_RING_MAX_SIZE);
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

    if(flags & dm_sw_ring_flags_mutex_sync)
    {
        ring->mutex = Dmod_Mutex_New(true);
        if (ring->mutex == NULL)
        {
            DMOD_LOG_ERROR("Failed to create mutex for ring buffer synchronization\n");
            Dmod_Free(ring->buffer);
            Dmod_Free(ring);
            return NULL;
        }
    }
    else
    {
        ring->mutex = NULL;
    }

    ring->magic = DM_SW_RING_MAGIC;
    ring->capacity = capacity;
    ring->head = 0;
    ring->tail = 0;
    ring->flags = flags;

    return ring;
}

dmod_dm_sw_ring_api_declaration(1.0, void, _destroy, (dm_sw_ring_t ring))
{
    if (lock_ring(ring))
    {
        ring->magic = 0;

        if (ring->mutex != NULL)
        {
            Dmod_Mutex_Delete(ring->mutex);
        }

        Dmod_Free(ring->buffer);
        Dmod_Free(ring);

        // No need to unlock since the instance is being destroyed
    }
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

    if (lock_ring(ring))
    {
        length = prepare_space(ring, length);

        written = (int32_t)write_data(ring, input, length);

        unlock_ring(ring);
    }

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

    if (lock_ring(ring))
    {
        read = (int32_t)read_data(ring, (uint8_t*)buffer, length);

        unlock_ring(ring);
    }

    return read;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _capacity, (dm_sw_ring_t ring))
{
    dm_sw_ring_capacity_t capacity = 0;
    if(lock_ring(ring))
    {
        capacity = ring->capacity;
        unlock_ring(ring);
    }
    return capacity;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _size, (dm_sw_ring_t ring))
{
    dm_sw_ring_capacity_t size = 0;
    if(lock_ring(ring))
    {
        if (ring->tail >= ring->head)
        {
            size = ring->tail - ring->head;
        }
        else
        {
            size = ring->capacity - (ring->head - ring->tail);
        }
        unlock_ring(ring);
    }
    return size;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _available_space, (dm_sw_ring_t ring))
{
    dm_sw_ring_capacity_t space = 0;
    if(lock_ring(ring))
    {
        space = available_space(ring);
        unlock_ring(ring);
    }
    return space;
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

    if (lock_ring(ring))
    {
        peeked = (int32_t)peek_data(ring, (uint8_t*)buffer, length);
        unlock_ring(ring);
    }

    return peeked;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _discard, (dm_sw_ring_t ring, dm_sw_ring_capacity_t length))
{
    int32_t discarded = -1;
    if(length == 0)
    {
        return 0; // No-op if length is zero
    }
    if(lock_ring(ring))
    {
        length = length > ring->capacity ? ring->capacity : length;
        discard(ring, length);
        discarded = (int32_t)length;
        unlock_ring(ring);
    }
    
    return discarded;
}

dmod_dm_sw_ring_api_declaration(1.0, int32_t, _clear, (dm_sw_ring_t ring))
{
    int32_t result = -1;
    if(lock_ring(ring))
    {
        ring->head = 0;
        ring->tail = 0;
        result = 0; // Success
        unlock_ring(ring);
    }
    return result;
}

dmod_dm_sw_ring_api_declaration(1.0, bool, _is_full, (dm_sw_ring_t ring))
{
    bool full = false;
    if(lock_ring(ring))
    {
        full = is_full(ring);
        unlock_ring(ring);
    }
    return full;
}

dmod_dm_sw_ring_api_declaration(1.0, bool, _is_empty, (dm_sw_ring_t ring))
{
    bool empty = false;
    if(lock_ring(ring))
    {
        empty = is_empty(ring);
        unlock_ring(ring);
    }
    return empty;
}

// ============================================================================
//                      Local prototypes implementation
// ============================================================================

/**
 * @brief Lock the ring buffer for exclusive access (if mutex synchronization is enabled)
 * @param ring The handle to the ring buffer instance
 * @return true if the lock was acquired successfully, false otherwise
 */
static bool lock_ring(dm_sw_ring_t ring)
{
    bool success = false;
    Dmod_EnterCritical();
    void* mutex = NULL;
    if(validate_ring(ring))
    {
        mutex = ring->mutex;
        success = true;
    }
    else 
    {
        DMOD_LOG_ERROR("Invalid ring buffer instance\n");
    }
    Dmod_ExitCritical();
    if(mutex != NULL && Dmod_Mutex_Lock(ring->mutex) != 0)
    {
        DMOD_LOG_ERROR("Failed to acquire mutex lock for ring buffer\n");
        success = false;
    }
    return success;
}

/**
 * @brief Unlock the ring buffer after exclusive access (if mutex synchronization is enabled)
 * @param ring The handle to the ring buffer instance
 */
static void unlock_ring(dm_sw_ring_t ring)
{
    if(ring->mutex != NULL)
    {
        Dmod_Mutex_Unlock(ring->mutex);
    }
    else 
    {
        Dmod_ExitCritical();
    }
}

/**
 * @brief Validate a ring buffer instance
 * @param ring The handle to the ring buffer instance to validate
 * @return true if the ring buffer instance is valid, false otherwise
 */
static bool validate_ring(dm_sw_ring_t ring)
{
    return (ring != NULL) && (ring->magic == DM_SW_RING_MAGIC);
}

/**
 * @brief Get the number of free spaces available in the ring buffer
 * @param ring The handle to the ring buffer instance
 * @return The number of free spaces available in the ring buffer
 */
static dm_sw_ring_capacity_t available_space(dm_sw_ring_t ring)
{
    if (ring->tail >= ring->head)
    {
        return ring->capacity - (ring->tail - ring->head);
    }
    else
    {
        return ring->head - ring->tail;
    }
}

/**
 * @brief Get the number of elements currently stored in the ring buffer
 * @param ring The handle to the ring buffer instance
 * @return The number of elements currently stored in the ring buffer
 */
static bool is_full(dm_sw_ring_t ring)
{
    return available_space(ring) == 0;
}

/**
 * @brief Check if the ring buffer is empty
 * @param ring The handle to the ring buffer instance
 * @return true if the ring buffer is empty, false otherwise
 */
static bool is_empty(dm_sw_ring_t ring)
{
    return ring->head == ring->tail;
}

/**
 * @brief Write a single byte to the ring buffer (unsafe, caller must ensure space is available)
 * @param ring The handle to the ring buffer instance
 * @param data The byte to write
 */
static void put_byte(dm_sw_ring_t ring, uint8_t data)
{
    ring->buffer[ring->tail] = data;
    ring->tail = (ring->tail + 1) % ring->capacity;
}

/**
 * @brief Read a single byte from the ring buffer (unsafe, caller must ensure data is available)
 * @param ring The handle to the ring buffer instance
 * @return The byte read from the ring buffer
 */
static uint8_t get_byte(dm_sw_ring_t ring)
{
    uint8_t data = ring->buffer[ring->head];
    ring->head = (ring->head + 1) % ring->capacity;
    return data;
}

/**
 * @brief Discard the oldest data from the ring buffer without reading it (unsafe, caller must ensure data is available)
 * @param ring The handle to the ring buffer instance
 * @param length The number of elements to discard
 */
static void discard(dm_sw_ring_t ring, dm_sw_ring_capacity_t length)
{
    ring->head = (ring->head + length) % ring->capacity;
}

/**
 * @brief Prepare space in the ring buffer for writing new data, discarding old data if necessary (unsafe, caller must ensure ring is valid)
 * @param ring The handle to the ring buffer instance
 * @param length The number of elements to prepare space for
 * @return The number of elements that can be written after preparing space, which may be less than the requested length if dropping old data is disabled
 */
static dm_sw_ring_capacity_t prepare_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length)
{
    length = length > ring->capacity ? ring->capacity : length;
    
    dm_sw_ring_capacity_t available = available_space(ring);
    dm_sw_ring_capacity_t missing   = length - available;

    if(ring->flags & dm_sw_ring_flags_drop_old_data)
    {
        discard(ring, missing);
        available += missing;
    }

    return available;
}

/**
 * @brief Write data to the ring buffer (unsafe, caller must ensure space is available)
 * @param ring The handle to the ring buffer instance
 * @param data Pointer to the data to write
 * @param length The number of elements to write
 * @return The number of elements actually written, which may be less than the requested length if dropping old data is disabled and there is not enough space
 */
static dm_sw_ring_capacity_t write_data(dm_sw_ring_t ring, const uint8_t* data, dm_sw_ring_capacity_t length)
{
    dm_sw_ring_capacity_t written = 0;

    for (written = 0; written < length; written++)
    {
        if (is_full(ring))
        {
            break;
        }
        put_byte(ring, data[written]);
    }

    return written;
}

/**
 * @brief Read data from the ring buffer (unsafe, caller must ensure data is available)
 * @param ring The handle to the ring buffer instance
 * @param buffer Pointer to the buffer to store the read data
 * @param length The maximum number of elements to read
 * @return The number of elements actually read, which may be less than the requested length if
 * there is not enough data available in the ring buffer
 */
static dm_sw_ring_capacity_t read_data(dm_sw_ring_t ring, uint8_t* buffer, dm_sw_ring_capacity_t length)
{    
    dm_sw_ring_capacity_t read = 0;

    for (read = 0; read < length; read++)
    {
        if (is_empty(ring))
        {
            break; // Buffer is empty
        }
        buffer[read] = get_byte(ring);
    }

    return read;
}

/**
 * @brief Peek at the data in the ring buffer without removing it (unsafe, caller must ensure data is available)
 * @param ring The handle to the ring buffer instance
 * @param buffer Pointer to the buffer to store the peeked data
 * @param length The maximum number of elements to peek
 * @return The number of elements actually peeked, which may be less than the requested length if there is not enough data available in the ring buffer
 */
static dm_sw_ring_capacity_t peek_data(dm_sw_ring_t ring, uint8_t* buffer, dm_sw_ring_capacity_t length)
{
    dm_sw_ring_capacity_t peeked = 0;
    dm_sw_ring_capacity_t index = ring->head;

    for (peeked = 0; peeked < length; peeked++)
    {
        if (index == ring->tail)
        {
            break; // Buffer is empty
        }
        buffer[peeked] = ring->buffer[index];
        index = (index + 1) % ring->capacity;
    }

    return peeked;
}