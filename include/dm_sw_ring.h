#ifndef DM_SW_RING_H
#define DM_SW_RING_H

#include <stdint.h>
#include "dmod_types.h"
#include "dm_sw_ring_defs.h"

// ============================================================================
//                      Types
// ============================================================================
/**
 * @brief Opaque handle to a software ring buffer instance
 */
typedef struct dm_sw_ring* dm_sw_ring_t;

/**
 * @brief Type for ring buffer capacity (number of elements)
 */
typedef uint32_t dm_sw_ring_capacity_t;

// ============================================================================
//                      Module Interface Declarations
// ============================================================================

/**
 * @brief Create a software ring buffer instance
 * @param capacity The capacity of the ring buffer (number of elements)
 * @param drop_old_data Whether to drop old data when the buffer is full    
 * @return A handle to the created ring buffer instance, or NULL on failure
 */
dmod_dm_sw_ring_api(1.0, dm_sw_ring_t, _create, (dm_sw_ring_capacity_t capacity, bool drop_old_data));

/**
 * @brief Destroy a software ring buffer instance
 * @param ring The handle to the ring buffer instance to destroy
 */
dmod_dm_sw_ring_api(1.0, void, _destroy, (dm_sw_ring_t ring));

/**
 * @brief Write data to the ring buffer
 * @param ring The handle to the ring buffer instance
 * @param data Pointer to the data to write
 * @param length The number of elements to write
 * @return The number of elements actually written, or a negative error code on failure
 */
dmod_dm_sw_ring_api(1.0, int32_t, _write, (dm_sw_ring_t ring, const void* data, dm_sw_ring_capacity_t length));

/**
 * @brief Read data from the ring buffer
 * @param ring The handle to the ring buffer instance
 * @param buffer Pointer to the buffer to store the read data
 * @param length The maximum number of elements to read
 * @return The number of elements actually read, or a negative error code on failure
 */
dmod_dm_sw_ring_api(1.0, int32_t, _read, (dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length));

/**
 * @brief Get the capacity of the ring buffer
 * @param ring The handle to the ring buffer instance
 * @return The capacity of the ring buffer (number of elements)
 */
dmod_dm_sw_ring_api(1.0, dm_sw_ring_capacity_t, _capacity, (dm_sw_ring_t ring));

/**
 * @brief Get the number of elements currently stored in the ring buffer
 * @param ring The handle to the ring buffer instance
 * @return The number of elements currently stored in the ring buffer
 */
dmod_dm_sw_ring_api(1.0, dm_sw_ring_capacity_t, _size, (dm_sw_ring_t ring));

/**
 * @brief Get the number of free spaces available in the ring buffer
 * @param ring The handle to the ring buffer instance
 * @return The number of free spaces available in the ring buffer
 */
dmod_dm_sw_ring_api(1.0, dm_sw_ring_capacity_t, _available_space, (dm_sw_ring_t ring));

/**
 * @brief Peek at the data in the ring buffer without removing it
 * @param ring The handle to the ring buffer instance
 * @param buffer Pointer to the buffer to store the peeked data
 * @param length The maximum number of elements to peek
 * @return The number of elements actually peeked, or a negative error code on failure
 */
dmod_dm_sw_ring_api(1.0, int32_t, _peek, (dm_sw_ring_t ring, void* buffer, dm_sw_ring_capacity_t length));

/**
 * @brief Discard the oldest data from the ring buffer without reading it
 * @param ring The handle to the ring buffer instance
 * @param length The maximum number of elements to discard
 * @return The number of elements actually discarded, or a negative error code on failure
 */
dmod_dm_sw_ring_api(1.0, int32_t, _discard, (dm_sw_ring_t ring, dm_sw_ring_capacity_t length));

/**
 * @brief Clear the contents of the ring buffer
 * @param ring The handle to the ring buffer instance
 * @return 0 on success, or a negative error code on failure
 */
dmod_dm_sw_ring_api(1.0, int32_t, _clear, (dm_sw_ring_t ring));

/**
 * @brief Check if the ring buffer is full
 * @param ring The handle to the ring buffer instance
 * @return true if the ring buffer is full, false otherwise
 */
dmod_dm_sw_ring_api(1.0, int32_t, _is_full, (dm_sw_ring_t ring));

/**
 * @brief Check if the ring buffer is empty
 * @param ring The handle to the ring buffer instance
 * @return true if the ring buffer is empty, false otherwise
 */
dmod_dm_sw_ring_api(1.0, int32_t, _is_empty, (dm_sw_ring_t ring));

#endif // DM_SW_RING_H