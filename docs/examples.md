# DM_SW_RING Usage Examples

## Example 1: Simple FIFO Queue

Create a 16-byte ring buffer, write some data, and read it back in FIFO order.

```c
#include "dm_sw_ring.h"
#include <string.h>
#include <assert.h>

void simple_fifo_example(void)
{
    // Create a 16-byte buffer with mutex sync but no blocking/overwrite
    dm_sw_ring_t ring = dm_sw_ring_create(16, dm_sw_ring_flags_mutex_sync);

    uint8_t tx[] = {1, 2, 3, 4, 5};
    dm_sw_ring_write(ring, tx, sizeof(tx));

    uint8_t rx[5];
    dm_sw_ring_capacity_t n = dm_sw_ring_read(ring, rx, sizeof(rx));
    // n == 5, rx == {1, 2, 3, 4, 5}

    dm_sw_ring_destroy(ring);
}
```

---

## Example 2: Overflow — Drop Oldest Data

When `dm_sw_ring_flags_drop_old_data` is set, writing beyond capacity silently discards the oldest bytes, preserving the most recent data.

```c
#include "dm_sw_ring.h"

void drop_old_example(void)
{
    // 4-byte buffer, drop oldest bytes on overflow
    dm_sw_ring_t ring = dm_sw_ring_create(4, dm_sw_ring_flags_drop_old_data
                                             | dm_sw_ring_flags_mutex_sync);

    uint8_t tx[] = {10, 11, 12, 13, 14}; // 5 bytes → overflows by 1
    dm_sw_ring_write(ring, tx, sizeof(tx));
    // Buffer now contains: {11, 12, 13, 14}  (byte 10 was dropped)

    uint8_t rx[4];
    dm_sw_ring_read(ring, rx, sizeof(rx));
    // rx == {11, 12, 13, 14}

    dm_sw_ring_destroy(ring);
}
```

---

## Example 3: Blocking Producer / Consumer (RTOS)

In an RTOS environment, use the default flags to have writers block when the buffer is full and readers block when the buffer is empty.

```c
#include "dm_sw_ring.h"

static dm_sw_ring_t g_ring;

// Producer task — runs in its own RTOS thread
void producer_task(void)
{
    uint8_t byte = 0;
    while (1)
    {
        // Blocks until space is available (dm_sw_ring_flags_wait_for_space)
        dm_sw_ring_write(g_ring, &byte, 1);
        byte++;
    }
}

// Consumer task — runs in its own RTOS thread
void consumer_task(void)
{
    uint8_t byte;
    while (1)
    {
        // Blocks until at least one byte is available (dm_sw_ring_flags_wait_for_some_data)
        dm_sw_ring_read(g_ring, &byte, 1);
        // process byte...
    }
}

void rtos_example_init(void)
{
    // Default flags: drop_old_data | mutex_sync | wait_for_space | wait_for_some_data
    g_ring = dm_sw_ring_create(64, dm_sw_ring_flags_default);
}
```

---

## Example 4: Non-Destructive Peek

Use `dm_sw_ring_peek` to inspect data without consuming it — useful for framing protocols where you need to check a header before deciding how many bytes to read.

```c
#include "dm_sw_ring.h"

void peek_example(dm_sw_ring_t ring)
{
    uint8_t header[2];

    // Peek at the first 2 bytes without consuming them
    int32_t peeked = dm_sw_ring_peek(ring, header, sizeof(header));
    if (peeked < 2)
    {
        return; // Not enough data yet
    }

    uint16_t payload_length = (uint16_t)((header[0] << 8) | header[1]);

    if (dm_sw_ring_size(ring) < 2u + payload_length)
    {
        return; // Full frame not yet available — come back later
    }

    // Discard the header and read the payload
    dm_sw_ring_discard(ring, 2);

    uint8_t payload[256];
    dm_sw_ring_read(ring, payload, payload_length);
    // process payload...
}
```

---

## Example 5: Discard and Clear

Remove data without reading it.

```c
#include "dm_sw_ring.h"

void discard_clear_example(void)
{
    dm_sw_ring_t ring = dm_sw_ring_create(8, dm_sw_ring_flags_mutex_sync);

    uint8_t data[] = {1, 2, 3, 4, 5, 6};
    dm_sw_ring_write(ring, data, sizeof(data));

    // Drop the first 3 bytes
    dm_sw_ring_discard(ring, 3);
    // Buffer now contains: {4, 5, 6}

    // Wipe everything
    dm_sw_ring_clear(ring);
    // Buffer is empty

    dm_sw_ring_destroy(ring);
}
```

---

## Example 6: State Inspection

```c
#include "dm_sw_ring.h"
#include <stdio.h>

void inspect_example(dm_sw_ring_t ring)
{
    printf("capacity       : %u\n", dm_sw_ring_capacity(ring));
    printf("bytes used     : %u\n", dm_sw_ring_size(ring));
    printf("bytes free     : %u\n", dm_sw_ring_available_space(ring));
    printf("is_full        : %s\n", dm_sw_ring_is_full(ring)  ? "yes" : "no");
    printf("is_empty       : %s\n", dm_sw_ring_is_empty(ring) ? "yes" : "no");
}
```
