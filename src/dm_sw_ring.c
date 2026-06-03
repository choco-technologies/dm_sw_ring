#define DMOD_ENABLE_REGISTRATION ON
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
    void* data_semaphore;               // Semaphore for waiting on data availability (if enabled)
    void* space_semaphore;              // Semaphore for waiting on space availability (if enabled)
};

// ============================================================================
//                      Local prototypes
// ============================================================================

static void ring_cleanup(dm_sw_ring_t ring);
static bool lock_ring(dm_sw_ring_t ring);
static void unlock_ring(dm_sw_ring_t ring);
static bool validate_ring(dm_sw_ring_t ring);
static dm_sw_ring_capacity_t available_space(dm_sw_ring_t ring);
static dm_sw_ring_capacity_t available_data(dm_sw_ring_t ring);
static bool is_full(dm_sw_ring_t ring);
static bool is_empty(dm_sw_ring_t ring);
static void put_byte(dm_sw_ring_t ring, uint8_t data);
static uint8_t get_byte(dm_sw_ring_t ring);
static void discard(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t prepare_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t write_data(dm_sw_ring_t ring, const uint8_t* data, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t read_data(dm_sw_ring_t ring, uint8_t* buffer, dm_sw_ring_capacity_t length);
static dm_sw_ring_capacity_t peek_data(dm_sw_ring_t ring, uint8_t* buffer, dm_sw_ring_capacity_t length);
static bool wait_for_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static bool wait_for_data(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static void signal_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);
static void signal_data(dm_sw_ring_t ring, dm_sw_ring_capacity_t length);

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


int dmod_init(const Dmod_Config_t *Config)
{
    DMOD_LOG_INFO("DM Software Ring module initialized\n");
    return 0;
}

int dmod_deinit(void)
{
    DMOD_LOG_INFO("DM Software Ring module deinitialized\n");
    return 0;
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
        ring_cleanup(ring);
        return NULL;
    }

    ring->magic = DM_SW_RING_MAGIC;
    ring->capacity = capacity;
    ring->head = 0;
    ring->tail = 0;
    ring->flags = flags;
    ring->mutex = NULL;
    ring->space_semaphore = NULL;
    ring->data_semaphore = NULL;

    if(flags & dm_sw_ring_flags_mutex_sync)
    {
        ring->mutex = Dmod_Mutex_New(true);
        if (ring->mutex == NULL)
        {
            DMOD_LOG_ERROR("Failed to create mutex for ring buffer synchronization\n");
            ring_cleanup(ring);
            return NULL;
        }
    }

    if(flags & dm_sw_ring_flags_wait_for_data)
    {
        dm_sw_ring_capacity_t max_count = flags & dm_sw_ring_flags_wait_for_all_data ? capacity : 1;
        ring->data_semaphore = Dmod_Semaphore_New(0, max_count);
        if (ring->data_semaphore == NULL)
        {
            DMOD_LOG_ERROR("Failed to create semaphore for data availability\n");
            ring_cleanup(ring);
            return NULL;
        }
    }

    if(flags & dm_sw_ring_flags_wait_for_space)
    {
        ring->space_semaphore = Dmod_Semaphore_New(ring->capacity, ring->capacity);
        if (ring->space_semaphore == NULL)
        {
            DMOD_LOG_ERROR("Failed to create semaphore for space availability\n");
            ring_cleanup(ring);
            return NULL;
        }
    }

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

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _write, (dm_sw_ring_t ring, const void* data, dm_sw_ring_capacity_t length))
{
    dm_sw_ring_capacity_t written = 0;
    const uint8_t* input = (const uint8_t*)data;

    if ((input == NULL) && (length > 0))
    {
        DMOD_LOG_ERROR("The given data buffer is NULL or length is zero\n");
        return 0;
    }

    if (lock_ring(ring))
    {
        while(written < length)
        {
            dm_sw_ring_capacity_t to_send = length - written;
            dm_sw_ring_capacity_t available = prepare_space(ring, to_send);

            if (available == 0)
            {
                break; // No more space available, exit the loop
            }

            written += write_data(ring, input + written, available);
        }

        unlock_ring(ring);
    }

    return written;
}

dmod_dm_sw_ring_api_declaration(1.0, dm_sw_ring_capacity_t, _read, (dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length))
{
    dm_sw_ring_capacity_t all_read = 0;

    if ((buffer == NULL) && (length > 0))
    {
        DMOD_LOG_ERROR("Invalid output buffer\n");
        return 0;
    }

    if (lock_ring(ring))
    {
        while(all_read < length)
        {
            dm_sw_ring_capacity_t to_read = length - all_read;
            dm_sw_ring_capacity_t read = read_data(ring, (uint8_t*)buffer + all_read, to_read);
            all_read += read;

            if (read == 0 && !(ring->flags & dm_sw_ring_flags_wait_for_all_data))
            {
                break; // No more data available, exit the loop
            }
        }
        unlock_ring(ring);
    }

    return all_read;
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
 * @brief Clean up resources associated with a ring buffer instance
 * @param ring The handle to the ring buffer instance to clean up
 */
static void ring_cleanup(dm_sw_ring_t ring)
{
    if (ring->mutex != NULL)
    {
        Dmod_Mutex_Delete(ring->mutex);
        ring->mutex = NULL;
    }
    if (ring->data_semaphore != NULL)
    {
        Dmod_Semaphore_Delete(ring->data_semaphore);
        ring->data_semaphore = NULL;
    }
    if (ring->space_semaphore != NULL)
    {
        Dmod_Semaphore_Delete(ring->space_semaphore);
        ring->space_semaphore = NULL;
    }
    if (ring->buffer != NULL)
    {
        Dmod_Free(ring->buffer);
        ring->buffer = NULL;
    }
}

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
static dm_sw_ring_capacity_t available_data(dm_sw_ring_t ring)
{
    if (ring->tail >= ring->head)
    {
        return ring->tail - ring->head;
    }
    else
    {
        return ring->capacity - (ring->head - ring->tail);
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
    if (length == 0)
    {
        return;
    }
    dm_sw_ring_capacity_t available = available_data(ring);
    length = length > available ? available : length; 
    
    wait_for_data(ring, length); 

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
    dm_sw_ring_capacity_t missing   = (length > available) ? (length - available) : 0;

    if(missing == 0)
    {
        available = length; // Enough space available, can write full length
    }
    else if(ring->flags & dm_sw_ring_flags_drop_old_data)
    {
        discard(ring, missing);
        available += missing;
    }
    else if(ring->flags & dm_sw_ring_flags_wait_for_space)
    {
        if(!wait_for_space(ring, missing))
        {
            DMOD_LOG_ERROR("Failed to wait for space in ring buffer\n");
            available = 0; // No space available
        }
        else
        {
            available = available_space(ring);
        }
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
        if (!wait_for_space(ring, 1))
        {
            break;
        }
        put_byte(ring, data[written]);
    }

    signal_data(ring, written);

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
    bool should_wait = (ring->flags & dm_sw_ring_flags_wait_for_data) != 0;

    for (read = 0; read < length; read++)
    {
        dm_sw_ring_capacity_t available = available_data(ring);
        if (available == 0 && !should_wait)
        {
            break; // No more data available, exit the loop
        }

        if (!wait_for_data(ring, 1))
        {
            break; // Buffer is empty
        }
        buffer[read] = get_byte(ring);
        should_wait = (ring->flags & dm_sw_ring_flags_wait_for_all_data) != 0; // If waiting for all data, continue waiting even if some data was read
    }

    signal_space(ring, read); 

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

/**
 * @brief Wait for space to become available in the ring buffer for writing (if wait_for_space flag is enabled)
 * @param ring The handle to the ring buffer instance
 * @param length The number of elements to wait for space for
 * @return true if space is available or was successfully waited for, false if an error occurred while waiting
 */
static bool wait_for_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length)
{
    bool success = available_space(ring) >= length;
    if (ring->space_semaphore != NULL && !success)
    {
        unlock_ring(ring); // Unlock before waiting to allow other threads to make progress
        success = Dmod_Semaphore_Wait(ring->space_semaphore, length) == 0 
               && lock_ring(ring);
    }

    return success;
}

/**
 * @brief Wait for data to become available in the ring buffer for reading (if wait_for_data or wait_for_some_data flag is enabled)
 * @param ring The handle to the ring buffer instance
 * @param length The number of elements to wait for data for
 * @return true if data is available or was successfully waited for, false if an error occurred while waiting
 */
static bool wait_for_data(dm_sw_ring_t ring, dm_sw_ring_capacity_t length)
{
    bool success = available_data(ring) >= length;
    if (ring->data_semaphore != NULL && !success)
    {
        unlock_ring(ring); // Unlock before waiting to allow other threads to make progress
        success = Dmod_Semaphore_Wait(ring->data_semaphore, length) == 0 
               && lock_ring(ring);
    }

    return success;
}

/**
 * @brief Signal that space has become available in the ring buffer (if wait_for_space flag is enabled)
 * @param ring The handle to the ring buffer instance
 * @param length The number of elements of space that have become available
 */
static void signal_space(dm_sw_ring_t ring, dm_sw_ring_capacity_t length)
{
    if (ring->space_semaphore != NULL)
    {
        Dmod_Semaphore_Post(ring->space_semaphore, length);
    }
}

/**
 * @brief Signal that data has become available in the ring buffer (if wait_for_data or wait_for_some_data flag is enabled)
 * @param ring The handle to the ring buffer instance
 * @param length The number of elements of data that have become available
 */
static void signal_data(dm_sw_ring_t ring, dm_sw_ring_capacity_t length)
{
    if (ring->data_semaphore != NULL)
    {
        Dmod_Semaphore_Post(ring->data_semaphore, length);
    }
}